#include "scheduler_helper.h"

SchedulerStats RoundRobin(int quantum, int msgq_id, int *noArrivingProcesses)
{
    msgbuff message;
    LinkedList *processList = list_create();
    Queue *readyQueue = queue_create();
    Queue *blockedQueue = queue_create(); // For page fault blocking
    SchedulerStats stats = {0, 0, 0, 0};
    stats.wtaList = wta_list_create();

    PCB *currentProcess = NULL;
    int targetTime = 0;

    FILE *log_file;
    log_file = open_file("scheduler.log", 1);

    while (1)
    {
        int current_time = getClk();

        // ===== 1. Check blocked queue for processes that can be unblocked =====
        QueueNode *curr = blockedQueue->Head;
        QueueNode *prev = NULL;
        while (curr)
        {
            PCB *blocked_pcb = curr->pcb;
            QueueNode *next = curr->next;

            // Check if I/O completed (page fault finished)
            if (check_unblock_page_fault(blocked_pcb, current_time))
            {
                // Move back to ready queue
                queue_enqueue(readyQueue, blocked_pcb);

                // Remove from blocked queue
                if (prev)
                {
                    prev->next = curr->next;
                }
                else
                {
                    blockedQueue->Head = curr->next;
                }
                free(curr);
                blockedQueue->size--;

                curr = next;
            }
            else
            {
                prev = curr;
                curr = next;
            }
        }

        // ===== 2. Receive new processes and handle terminations =====
        while (msgrcv(msgq_id, &message, sizeof(message.process), 0, IPC_NOWAIT) > 0)
        {
            if (message.mtype == NEW_PROCESS)
            {
                PCB *new_pcb = add_new_process(processList, &stats, message);

                // Add to ready queue if not blocked on dependency
                if (new_pcb->state != BLOCKED_DEPENDENCY)
                {
                    new_pcb->state = READY;
                    queue_enqueue(readyQueue, new_pcb);
                }
            }
            else if (message.mtype == TERMINATE_PROCESS)
            {
                PCB *finishedPCB = list_find(processList, message.process.id);
                Queue *dependents = end_process(processList, &stats, finishedPCB, &currentProcess, log_file);

                if (currentProcess && currentProcess->id == finishedPCB->id)
                    currentProcess = NULL;
                else
                {
                    queue_remove(readyQueue, finishedPCB);
                }

                list_remove(processList, finishedPCB);

                // Unblock dependents
                while (dependents->size)
                {
                    PCB *dependentPCB = queue_front(dependents)->pcb;
                    dependentPCB->state = READY;
                    queue_enqueue(readyQueue, dependentPCB);
                    queue_dequeue(dependents);
                }

                queue_clear(dependents);
            }
        }

        // ===== 3. Process memory requests for running process =====
        if (currentProcess && currentProcess->state == RUNNING)
        {
            // Check if any memory requests are due
            if (process_memory_requests(currentProcess, current_time))
            {
                // Page fault occurred - block the process

                // Stop the process
                kill(currentProcess->pid, SIGSTOP);
                currentProcess->remainingtime -= (current_time - currentProcess->resumedAt);
                currentProcess->remainingtime = currentProcess->remainingtime > 0 ? currentProcess->remainingtime : 0;
                currentProcess->lastActive = current_time;

                // Move to blocked queue
                queue_enqueue(blockedQueue, currentProcess);

                // Log the blocking
                add_log(log_file, currentProcess, "stopped", current_time);

                currentProcess = NULL; // Yield CPU immediately
            }
        }

        // ===== 4. Check if quantum expired =====
        if (currentProcess && current_time == targetTime)
        {
            preempt_process(currentProcess, targetTime);
            queue_enqueue(readyQueue, currentProcess);

            add_log(log_file, currentProcess, "stopped", current_time);

            currentProcess = NULL;
        }

        // ===== 5. Schedule next process from ready queue =====
        if (!currentProcess && readyQueue->size)
        {
            currentProcess = queue_front(readyQueue)->pcb;
            queue_dequeue(readyQueue);
            start_continue_process(currentProcess);
            targetTime = current_time + quantum;

            // Log start/resume
            if (currentProcess->executiontime == currentProcess->remainingtime)
                add_log(log_file, currentProcess, "started", current_time);
            else
                add_log(log_file, currentProcess, "resumed", current_time);
        }

        // ===== 6. Check termination condition =====
        if (processList->size == 0 && *noArrivingProcesses)
            break;
    }

    // Release resources
    list_clear(processList);
    queue_clear(readyQueue);
    queue_clear(blockedQueue);

    close_file(log_file);

    return stats;
}

SchedulerStats HighestPriorityFirst(int agingInterval, int msgq_id, int *noArrivingProcesses)
{
    msgbuff message;
    LinkedList *processList = list_create();
    PriQueue *readyQueue = pri_queue_create();
    SchedulerStats stats = {0, 0, 0, 0};
    stats.wtaList = wta_list_create();

    PCB *currentProcess = NULL;

    FILE *log_file;
    log_file = open_file("scheduler.log", 1);

    int lastClockTime = getClk();

    while (1)
    {
        // 1- Get all processes that have arrived by current time
        while (msgrcv(msgq_id, &message, sizeof(message.process), 0, IPC_NOWAIT) > 0)
        {
            if (message.mtype == NEW_PROCESS)
            {
                PCB *new_pcb = add_new_process(processList, &stats, message);
                // If no dependencies, add to ready queue
                if (new_pcb->state != BLOCKED_DEPENDENCY)
                {
                    new_pcb->state = READY;
                    new_pcb->readyFrom = getClk();
                    pri_queue_enqueue(readyQueue, new_pcb, new_pcb->priority);
                }

                // check for priority inversion
                handle_priority_inversion(new_pcb, processList, readyQueue);
            }
            else if (message.mtype == TERMINATE_PROCESS)
            {
                // 2- finished processes and unblock dependents
                PCB *finishedPCB = list_find(processList, message.process.id);
                Queue *dependents = end_process(processList, &stats, finishedPCB, &currentProcess, log_file);
                if (currentProcess && (currentProcess)->id == finishedPCB->id)
                    currentProcess = NULL;
                else
                {
                    // remove from ready queue
                    pri_queue_remove(readyQueue, finishedPCB);
                }

                // remove finished process from processList
                list_remove(processList, finishedPCB);

                // unblock dependents
                while (dependents->size)
                {
                    PCB *dependentPCB = queue_front(dependents)->pcb;
                    dependentPCB->state = READY;
                    dependentPCB->readyFrom = getClk();
                    pri_queue_enqueue(readyQueue, dependentPCB, dependentPCB->priority);

                    queue_dequeue(dependents);
                }
                queue_clear(dependents);

                // output scheduler.log (finished)
                // add_log(log_file, currentProcess, "finished", getClk());
                // if(currentProcess->id == message.process.id)
                //     currentProcess = NULL;
            }
        }

        // 3- Adaptive priority (aging)
        if (getClk() != lastClockTime)
        {
            apply_aging(readyQueue, getClk(), agingInterval);
            lastClockTime = getClk();
        }

        // 4- Schedule processes in HPF manner

        // check if current process has lower priority
        if (currentProcess && readyQueue->size &&
            pri_queue_front(readyQueue)->priority < currentProcess->priority)
        {
            preempt_process(currentProcess, getClk());
            pri_queue_enqueue(readyQueue, currentProcess, currentProcess->priority);

            // output scheduler.log (stopped)
            add_log(log_file, currentProcess, "stopped", getClk());

            currentProcess = NULL;
        }

        // if no current process, get next from ready queue
        if (!currentProcess && readyQueue->size)
        {
            currentProcess = pri_queue_front(readyQueue)->pcb;
            pri_queue_dequeue(readyQueue);
            start_continue_process(currentProcess);

            // output scheduler.log (started/resumed)
            if (currentProcess->executiontime == currentProcess->remainingtime)
                add_log(log_file, currentProcess, "started", getClk());
            else
                add_log(log_file, currentProcess, "resumed", getClk());
        }

        // 5- Check termination condition
        if (processList->size == 0 && *noArrivingProcesses)
            break;
    }

    // release resources
    list_clear(processList);
    pri_queue_clear(readyQueue);

    close_file(log_file);

    return stats;
}

SchedulerStats ShortestRemainingTimeNext(int msgq_id, int *noArrivingProcesses)
{
    msgbuff message;
    LinkedList *processList = list_create();
    PriQueue *readyQueue = pri_queue_create();
    SchedulerStats stats = {0, 0, 0, 0};
    stats.wtaList = wta_list_create();

    PCB *currentProcess = NULL;

    FILE *log_file;
    log_file = open_file("scheduler.log", 1);

    while (1)
    {
        // 1- Get all processes that have arrived by current time
        while (msgrcv(msgq_id, &message, sizeof(message.process), 0, IPC_NOWAIT) > 0)
        {
            if (message.mtype == NEW_PROCESS)
            {
                PCB *new_pcb = add_new_process(processList, &stats, message);
                // If no dependencies, add to ready queue
                if (new_pcb->state != BLOCKED_DEPENDENCY)
                {
                    new_pcb->state = READY;
                    pri_queue_enqueue(readyQueue, new_pcb, new_pcb->remainingtime);
                }
            }
            else if (message.mtype == TERMINATE_PROCESS)
            {
                // 2- finished processes and unblock dependents
                PCB *finishedPCB = list_find(processList, message.process.id);
                Queue *dependents = end_process(processList, &stats, finishedPCB, &currentProcess, log_file);
                if (currentProcess && (currentProcess)->id == finishedPCB->id)
                    currentProcess = NULL;
                else
                {
                    // remove from ready queue
                    pri_queue_remove(readyQueue, finishedPCB);
                }

                // remove finished process from processList
                list_remove(processList, finishedPCB);

                // unblock dependents
                while (dependents->size)
                {
                    PCB *dependentPCB = queue_front(dependents)->pcb;
                    dependentPCB->state = READY;
                    pri_queue_enqueue(readyQueue, dependentPCB, dependentPCB->remainingtime);
                    queue_dequeue(dependents);
                }
                queue_clear(dependents);

                // output scheduler.log (finished)
                // add_log(log_file, currentProcess, "finished", getClk());
                // if(currentProcess->id == message.process.id)
                //     currentProcess = NULL;
            }
            else
            {
                printf("ERROR mtype: %ld\n", message.mtype);
            }
        }

        // 3- Schedule processes in SRTN manner
        // check if current process has longer remaining time
        int currentProcessRemainingTime = 0;
        if (currentProcess)
            currentProcessRemainingTime = currentProcess->remainingtime - (getClk() - currentProcess->resumedAt);

        if (currentProcess && readyQueue->size &&
            pri_queue_front(readyQueue)->priority < currentProcessRemainingTime)
        {
            preempt_process(currentProcess, getClk());
            pri_queue_enqueue(readyQueue, currentProcess, currentProcess->remainingtime);

            // output scheduler.log (stopped)
            add_log(log_file, currentProcess, "stopped", getClk());

            currentProcess = NULL;
        }

        // if no current process, get next from ready queue
        if (!currentProcess && readyQueue->size)
        {
            currentProcess = pri_queue_front(readyQueue)->pcb;
            pri_queue_dequeue(readyQueue);
            start_continue_process(currentProcess);

            // output scheduler.log (started/resumed)
            if (currentProcess->executiontime == currentProcess->remainingtime)
                add_log(log_file, currentProcess, "started", getClk());
            else
                add_log(log_file, currentProcess, "resumed", getClk());
        }

        // 4- Check termination condition
        if (processList->size == 0 && *noArrivingProcesses)
            break;
    }

    // release resources
    list_clear(processList);
    pri_queue_clear(readyQueue);

    close_file(log_file);

    return stats;
}