#include "output_helper.h"

enum MQTypes
{
    NEW_PROCESS = 1,
    TERMINATE_PROCESS = 2
};

SchedulerStats RoundRobin(int quantum, int msgq_id, int* noArrivingProcessesFlag)
{
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
                Queue* dependents = end_process(processList, &stats, message);
                while(dependents->size){
                    PCB* dependentPCB=queue_front(dependents)->pcb;
                    dependentPCB->state = READY;
                    queue_enqueue(readyQueue, dependentPCB);
                    queue_dequeue(dependents);
                }
                queue_clear(dependents);

                // output scheduler.log (finished)
                add_log(log_file, currentProcess, "finished", getClk());
                if(currentProcess->id == message.process.id)
                    currentProcess = NULL;
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
            if (currentProcess->starttime == getClk())
                add_log(log_file, currentProcess, "started", getClk());
            else
                add_log(log_file, currentProcess, "resumed", getClk());
            
            

        }

        // 4- Check termination condition
        //TODO change noArrivingProcesses flag depending on signal comming from process generator
        if(processList->size == 0 && *noArrivingProcessesFlag)
            break;

    }

    // release resources
    list_clear(processList);
    queue_clear(readyQueue);

    close_file(log_file);

    return stats;
}