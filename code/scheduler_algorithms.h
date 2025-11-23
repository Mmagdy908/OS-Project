#include "scheduler_helper.h"

SchedulerStats RoundRobin(int quantum, int msgq_id, int* noArrivingProcesses)
{
    printf("starting round robin\n");

    msgbuff message;
    LinkedList* processList = list_create();
    Queue* readyQueue = queue_create();
    SchedulerStats stats = {0, 0, 0, 0};
    stats.wtaList = wta_list_create();

    PCB* currentProcess = NULL;
    int targetTime = 0; // time when current process should yield

    FILE* log_file;
    log_file = open_file("scheduler.log", 1);

    while(1){
        // 1- Get all processes that have arrived by current time
        while(msgrcv(msgq_id, &message, sizeof(message.process), 0, IPC_NOWAIT)>0){
            if(message.mtype==NEW_PROCESS){
                PCB* new_pcb = add_new_process(processList, &stats, message);
                
                // If no dependencies, add to ready queue
                if(new_pcb->state != BLOCKED){
                    new_pcb->state = READY;
                    queue_enqueue(readyQueue, new_pcb);
                }
                
            }
            else if(message.mtype==TERMINATE_PROCESS){
                // 2- finished processes and unblock dependents
                //unblock dependents
                Queue* dependents = end_process(processList, &stats, message, &currentProcess, log_file);
                while(dependents->size){
                    PCB* dependentPCB=queue_front(dependents)->pcb;
                    dependentPCB->state = READY;
                    queue_enqueue(readyQueue, dependentPCB);
                    queue_dequeue(dependents);
                }
                
                queue_clear(dependents);

                
    
                // if(currentProcess && currentProcess->id == message.process.id)
                //     currentProcess = NULL;
            }   
        }

        // 3- Schedule processes in RR manner
        // check if current process's quantum expired
        if(currentProcess && getClk() == targetTime){
            preempt_process(currentProcess, targetTime);
            queue_enqueue(readyQueue, currentProcess);
            
            // output scheduler.log (stopped)
            add_log(log_file, currentProcess, "stopped", getClk());

            currentProcess = NULL;
        }

        // if no current process, get next from ready queue
        if(!currentProcess && readyQueue->size){
            currentProcess = queue_front(readyQueue)->pcb;
            queue_dequeue(readyQueue);
            start_continue_process(currentProcess);
            targetTime = getClk() + quantum;

            // output scheduler.log (started/resumed)
            if (currentProcess->executiontime == currentProcess->remainingtime)
                add_log(log_file, currentProcess, "started", getClk());
            else
                add_log(log_file, currentProcess, "resumed", getClk());
        }

        // 4- Check termination condition
        //TODO change noArrivingProcesses flag depending on signal comming from process generator
        if(processList->size == 0 && *noArrivingProcesses)
            break;

    }

    // release resources
    list_clear(processList);
    queue_clear(readyQueue);

    close_file(log_file);

    return stats;
}


SchedulerStats HighestPriorityFirst(int agingInterval, int msgq_id, int* noArrivingProcesses)
{
    msgbuff message;
    LinkedList* processList = list_create();
    PriQueue* readyQueue = pri_queue_create();
    SchedulerStats stats = {0, 0, 0, 0};
    stats.wtaList = wta_list_create();

    PCB* currentProcess = NULL;

    FILE* log_file;
    log_file = open_file("scheduler.log", 1);

    int lastClockTime=getClk();

    while(1){
        // 1- Get all processes that have arrived by current time
        while(msgrcv(msgq_id, &message, sizeof(message.process), 0, IPC_NOWAIT)>0){
            if(message.mtype==NEW_PROCESS){
                PCB* new_pcb = add_new_process(processList, &stats, message);
                // If no dependencies, add to ready queue
                if(new_pcb->state != BLOCKED){
                    new_pcb->state = READY;
                    new_pcb->readyFrom= getClk();
                    pri_queue_enqueue(readyQueue, new_pcb, new_pcb->priority);
                }

                // check for priority inversion
                handle_priority_inversion(new_pcb, processList, readyQueue);
            }
            else if(message.mtype==TERMINATE_PROCESS){
                // 2- finished processes and unblock dependents
                Queue* dependents = end_process(processList, &stats, message, &currentProcess, log_file);
                while(dependents->size){
                    PCB* dependentPCB=queue_front(dependents)->pcb;
                    dependentPCB->state = READY;
                    dependentPCB->readyFrom= getClk();
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
        if(getClk()!=lastClockTime){
            apply_aging(readyQueue, getClk(), agingInterval);
            lastClockTime=getClk();
        }

        // 4- Schedule processes in HPF manner

        // check if current process has lower priority
        if(currentProcess && readyQueue->size &&
            pri_queue_front(readyQueue)->priority < currentProcess->priority){
            preempt_process(currentProcess, getClk());
            pri_queue_enqueue(readyQueue, currentProcess, currentProcess->priority);
            
            // output scheduler.log (stopped)
            add_log(log_file, currentProcess, "stopped", getClk());

            currentProcess = NULL;
        }

        // if no current process, get next from ready queue
        if(!currentProcess && readyQueue->size){
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
        //TODO change noArrivingProcesses flag depending on signal comming from process generator
        if(processList->size == 0 && *noArrivingProcesses)
            break;

    }

    // release resources
    list_clear(processList);
    pri_queue_clear(readyQueue);

    close_file(log_file);

    return stats;
}


SchedulerStats ShortestRemainingTimeNext(int msgq_id, int* noArrivingProcesses)
{
    msgbuff message;
    LinkedList* processList = list_create();
    PriQueue* readyQueue = pri_queue_create();
    SchedulerStats stats = {0, 0, 0, 0};
    stats.wtaList = wta_list_create();

    PCB* currentProcess = NULL;

    FILE* log_file;
    log_file = open_file("scheduler.log", 1);


    while(1){
        // 1- Get all processes that have arrived by current time
        while(msgrcv(msgq_id, &message, sizeof(message.process), 0, IPC_NOWAIT)>0){
            if(message.mtype==NEW_PROCESS){
                PCB* new_pcb = add_new_process(processList, &stats, message);
                // If no dependencies, add to ready queue
                if(new_pcb->state != BLOCKED){
                    new_pcb->state = READY;
                    pri_queue_enqueue(readyQueue, new_pcb, new_pcb->remainingtime);
                }
            }
            else if(message.mtype==TERMINATE_PROCESS){
                // 2- finished processes and unblock dependents
                Queue* dependents = end_process(processList, &stats, message, &currentProcess, log_file);
                while(dependents->size){
                    PCB* dependentPCB=queue_front(dependents)->pcb;
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
            else{
                printf("ERROR mtype: %ld\n", message.mtype);
            }
        }

        // 3- Schedule processes in SRTN manner
        // check if current process has longer remaining time
        int currentProcessRemainingTime = 0;
        if (currentProcess)
            currentProcessRemainingTime = currentProcess->remainingtime-(getClk()-currentProcess->resumedAt);

        if(currentProcess && readyQueue->size &&
            pri_queue_front(readyQueue)->priority < currentProcessRemainingTime){
            preempt_process(currentProcess, getClk());
            pri_queue_enqueue(readyQueue, currentProcess, currentProcess->remainingtime);
            
            // output scheduler.log (stopped)
            add_log(log_file, currentProcess, "stopped", getClk());

            currentProcess = NULL;
        }

        // if no current process, get next from ready queue
        if(!currentProcess && readyQueue->size){
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
        //TODO change noArrivingProcesses flag depending on signal comming from process generator
        if(processList->size == 0 && *noArrivingProcesses)
            break;

    }
    printf("terminating SRTN\n");

    // release resources
    list_clear(processList);
    pri_queue_clear(readyQueue);

    close_file(log_file);

    return stats;
}