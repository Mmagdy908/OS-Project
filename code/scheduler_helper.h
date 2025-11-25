#include "headers.h"
#include "DS/linked_list.h"
#include "DS/queue.h"
#include "DS/pri_queue.h"
#include "output_helper.h"




void fork_process(PCB* pcb) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process - exec the process program
        char id_str[10];
        char runtime_str[10];
        sprintf(id_str, "%d", pcb->id);
        sprintf(runtime_str, "%d", pcb->remainingtime);
        
        execl("./process.out", "process.out", id_str, runtime_str, NULL);
        perror("ERROR forking new process\n");
        exit(1);
    } else if (pid > 0) {
        pcb->pid = pid;
    } else {
        perror("Fork failed");
    }
}

PCB* add_new_process(LinkedList* processList, SchedulerStats* stats, msgbuff message){
    // Add process to processList
    PCB* pcb = pcb_create(message.process.id, message.process.arrivaltime, message.process.runningtime, message.process.priority, message.process.dependencyId);
    list_add_front(processList, pcb);

    // check dependencies
    if(pcb->dependencyId != -1){
        
        PCB* dependencyPCB = list_find(processList, pcb->dependencyId);
        if(dependencyPCB){
            // add to dependency's dependents queue
            pcb->state=BLOCKED;
            queue_enqueue(dependencyPCB->dependents, pcb);
        }
    }

    stats->totalProcesses++;

    return pcb;
}

void start_continue_process(PCB* currentProcess){
    currentProcess->state = RUNNING;
    currentProcess->resumedAt=getClk();

    if(currentProcess->starttime == -1){
        currentProcess->starttime = getClk();
        currentProcess->waitingtime = currentProcess->starttime-currentProcess->arrivaltime;
        fork_process(currentProcess);
    }else{
        currentProcess->waitingtime += getClk()-currentProcess->lastActive;
        kill(currentProcess->pid, SIGCONT);
    }
}

void preempt_process(PCB* currentProcess, int currentTime){
    kill(currentProcess->pid, SIGSTOP);
    currentProcess->state = READY;
    currentProcess->remainingtime -= (currentTime - currentProcess->resumedAt);
    currentProcess->remainingtime = currentProcess->remainingtime>0 ? currentProcess->remainingtime : 0;
    currentProcess->lastActive = currentTime;
}

Queue* end_process(LinkedList* processList, SchedulerStats* stats, PCB* finishedPCB, PCB** currentProcess, FILE* log_file){
    if (!finishedPCB)
        return queue_create();
    
    Queue* dependents = queue_copy(finishedPCB->dependents);
    finishedPCB->dependents=NULL;


    finishedPCB->remainingtime = 0;
    finishedPCB->finishtime = getClk();
    finishedPCB->turnaround = finishedPCB->finishtime - finishedPCB->arrivaltime;
    finishedPCB->wturnaround = (float)finishedPCB->turnaround / finishedPCB->executiontime;
    stats->totalWaitingTime += finishedPCB->waitingtime;
    stats->totalWeightedTurnaroundTime += finishedPCB->wturnaround;
    stats->totalExecutionTime += finishedPCB->executiontime;
    wta_list_add_front(stats->wtaList, finishedPCB->wturnaround);

    // output scheduler.log (finished)
    add_log(log_file, finishedPCB, "finished", getClk());


    return dependents;
}


void handle_priority_inversion(PCB* blockedPCB, LinkedList* processList, PriQueue* readyQueue) {
    while (blockedPCB->state == BLOCKED){
        PCB* dependency=list_find(processList, blockedPCB->dependencyId);
        
        if (!dependency)  
            break;

        if (blockedPCB->priority < dependency->priority){
            // inherit priority
            dependency->priority = blockedPCB->priority;
            if(dependency->state == READY){
                pri_queue_remove(readyQueue, dependency);
                pri_queue_enqueue(readyQueue, dependency, dependency->priority);
            }
            
            blockedPCB = dependency;
        }
        else {
            break;
        }

    }
}


void apply_aging(PriQueue* readyQueue, int currentTime, int agingInterval) {
    PriNode* curr = readyQueue->Head;
    while (curr) {
        PCB* pcb = curr->pcb;
        int waitingTime = currentTime - pcb->readyFrom;
        if (waitingTime > 0 && waitingTime % agingInterval == 0) {
            int oldPriority = pcb->priority;
            pcb->priority = (pcb->priority > 0) ? pcb->priority - 1 : 0; // increase priority

            curr=curr->next; // move curr forward before modifying the queue

            if (pcb->priority != oldPriority) {
                // Reinsert into priority queue
                pri_queue_remove(readyQueue, pcb);
                pri_queue_enqueue(readyQueue, pcb, pcb->priority);
            }
        }
        else
            curr = curr->next;
    }
}