#include "headers.h"
#include "DS/linked_list.h"
#include "DS/queue.h"
#include "DS/pri_queue.h"
#include "output_helper.h"
#include "Memory/mmu.h"

// Load memory requests from per-process file
void load_memory_requests(PCB *pcb)
{
    char filename[256];
    sprintf(filename, "process_%d_requests.txt", pcb->id);

    FILE *file = fopen(filename, "r");
    if (!file)
    {
        pcb->memory_requests = NULL;
        pcb->num_requests = 0;
        return;
    }

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), file))
    {
        if (line[0] != '#' && strlen(line) > 1)
        {
            count++;
        }
    }

    if (count == 0)
    {
        fclose(file);
        pcb->memory_requests = NULL;
        pcb->num_requests = 0;
        return;
    }

    pcb->memory_requests = (MemoryRequest *)malloc(sizeof(MemoryRequest) * count);
    pcb->num_requests = count;

    rewind(file);
    int index = 0;
    while (fgets(line, sizeof(line), file))
    {
        if (line[0] == '#' || strlen(line) <= 1)
            continue;

        int time, address;
        char rw;
        if (sscanf(line, "%d %d %c", &time, &address, &rw) == 3)
        {
            pcb->memory_requests[index].time = time;
            pcb->memory_requests[index].address = address;
            pcb->memory_requests[index].write = (rw == 'w' || rw == 'W') ? 1 : 0;
            index++;
        }
    }

    fclose(file);
}

// Process memory requests - returns 1 if page fault occurred
int process_memory_requests(PCB *pcb, int current_time)
{
    if (!pcb->memory_requests || pcb->current_request_index >= pcb->num_requests)
        return 0;

    if (pcb->state != RUNNING)
        return 0;

    int relative_time = current_time - pcb->starttime;

    while (pcb->current_request_index < pcb->num_requests &&
           pcb->memory_requests[pcb->current_request_index].time <= relative_time)
    {

        MemoryRequest *req = &pcb->memory_requests[pcb->current_request_index];

        int block_cycles = MMU_request(pcb->id, req->address, req->write, getClk());

        if (block_cycles > 0)
        {
            pcb->blocked_until = current_time + block_cycles;
            pcb->state = BLOCKED_PAGE_FAULT;
            return 1;
        }

        pcb->current_request_index++;
    }

    return 0;
}

// Check if process can be unblocked from page fault
int check_unblock_page_fault(PCB *pcb, int current_time)
{
    if (pcb->state == BLOCKED_PAGE_FAULT && current_time >= pcb->blocked_until)
    {
        pcb->state = READY;
        return 1;
    }
    return 0;
}

void fork_process(PCB *pcb)
{
    pid_t pid = fork();

    if (pid == 0)
    {
        char id_str[10];
        char runtime_str[10];
        sprintf(id_str, "%d", pcb->id);
        sprintf(runtime_str, "%d", pcb->remainingtime);

        execl("./process.out", "process.out", id_str, runtime_str, NULL);
        perror("ERROR forking new process\n");
        exit(1);
    }
    else if (pid > 0)
    {
        pcb->pid = pid;
    }
    else
    {
        perror("Fork failed");
    }
}

PCB *add_new_process(LinkedList *processList, SchedulerStats *stats, msgbuff message)
{
    PCB *pcb = pcb_create(message.process.id, message.process.arrivaltime,
                          message.process.runningtime, message.process.priority,
                          message.process.dependencyId, message.process.base,
                          message.process.limit);
    list_add_front(processList, pcb);

    pcb->page_table_frame = setup_page_table(pcb->id); //
    MMU_request(pcb->id, 0, 0, getClk());

    load_memory_requests(pcb);

    // check dependencies
    if (pcb->dependencyId != -1)
    {
        PCB *dependencyPCB = list_find(processList, pcb->dependencyId);
        if (dependencyPCB)
        {
            pcb->state = BLOCKED_DEPENDENCY;
            queue_enqueue(dependencyPCB->dependents, pcb);
        }
    }

    stats->totalProcesses++;
    return pcb;
}

void start_continue_process(PCB *currentProcess)
{
    currentProcess->state = RUNNING;
    currentProcess->resumedAt = getClk();

    if (currentProcess->starttime == -1)
    {
        currentProcess->starttime = getClk();
        currentProcess->waitingtime = currentProcess->starttime - currentProcess->arrivaltime;
        fork_process(currentProcess);
    }
    else
    {
        currentProcess->waitingtime += getClk() - currentProcess->lastActive;
        kill(currentProcess->pid, SIGCONT);
    }
}

void preempt_process(PCB *currentProcess, int currentTime)
{
    kill(currentProcess->pid, SIGSTOP);
    currentProcess->state = READY;
    currentProcess->remainingtime -= (currentTime - currentProcess->resumedAt);
    currentProcess->remainingtime = currentProcess->remainingtime > 0 ? currentProcess->remainingtime : 0;
    currentProcess->lastActive = currentTime;
}

Queue *end_process(LinkedList *processList, SchedulerStats *stats, PCB *finishedPCB, PCB **currentProcess, FILE *log_file)
{
    if (!finishedPCB)
        return queue_create();

    Queue *dependents = queue_copy(finishedPCB->dependents);
    finishedPCB->dependents = NULL;

    finishedPCB->remainingtime = 0;
    finishedPCB->finishtime = getClk();
    finishedPCB->turnaround = finishedPCB->finishtime - finishedPCB->arrivaltime;
    finishedPCB->wturnaround = (float)finishedPCB->turnaround / finishedPCB->executiontime;
    stats->totalWaitingTime += finishedPCB->waitingtime;
    stats->totalWeightedTurnaroundTime += finishedPCB->wturnaround;
    stats->totalExecutionTime += finishedPCB->executiontime;
    wta_list_add_front(stats->wtaList, finishedPCB->wturnaround);

    add_log(log_file, finishedPCB, "finished", getClk());

    release_process_frames(finishedPCB->id);

    if (finishedPCB->memory_requests)
    {
        free(finishedPCB->memory_requests);
        finishedPCB->memory_requests = NULL;
    }

    return dependents;
}

void handle_priority_inversion(PCB *blockedPCB, LinkedList *processList, PriQueue *readyQueue)
{
    while (blockedPCB->state == BLOCKED_DEPENDENCY)
    {
        PCB *dependency = list_find(processList, blockedPCB->dependencyId);

        if (!dependency)
            break;

        if (blockedPCB->priority < dependency->priority)
        {
            dependency->priority = blockedPCB->priority;
            if (dependency->state == READY)
            {
                pri_queue_remove(readyQueue, dependency);
                pri_queue_enqueue(readyQueue, dependency, dependency->priority);
            }

            blockedPCB = dependency;
        }
        else
        {
            break;
        }
    }
}

void apply_aging(PriQueue *readyQueue, int currentTime, int agingInterval)
{
    PriNode *curr = readyQueue->Head;
    while (curr)
    {
        PCB *pcb = curr->pcb;
        int waitingTime = currentTime - pcb->readyFrom;
        if (waitingTime > 0 && waitingTime % agingInterval == 0)
        {
            int oldPriority = pcb->priority;
            pcb->priority = (pcb->priority > 0) ? pcb->priority - 1 : 0;

            curr = curr->next;

            if (pcb->priority != oldPriority)
            {
                pri_queue_remove(readyQueue, pcb);
                pri_queue_enqueue(readyQueue, pcb, pcb->priority);
            }
        }
        else
        {
            curr = curr->next;
        }
    }
}