#include "headers.h"
#include "DS/linkedList.h"
#include "DS/queue.h"
#include "DS/priQueue.h"

#define MQKEY 500

typedef struct processData
{
    int arrivaltime;
    int priority;
    int runningtime;
    int id;
    int dependencyId;
} processData;

typedef struct {
    int totalProcesses;
    int totalWaitingTime;
    int totalTurnaroundTime;
    int totalExecutionTime;
} SchedulerStats;

typedef struct msgbuff
{
    long mtype;
    processData process;   //TODO re-check data type received from generator
} msgbuff;

enum MQTypes
{
    NEW_PROCESS = 1,
    TERMINATE_PROCESS = 2
};

int noArrivingProcesses = 0;

int main(int argc, char * argv[])
{
    initClk();
    
    //TODO implement the scheduler :)
    // Initialize message queue
    int  msgq_id;
    msgq_id = msgget(MQKEY, 0666 | IPC_CREAT); //create message queue and return id
    if (msgq_id == -1)
    {
        perror("Error in creating message queue in scheduler");
        exit(-1);
    }

    //upon termination release the clock resources.
    destroyClk(true);
}

void fork_process(PCB* pcb) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process - exec the process program
        char runtime_str[10];
        sprintf(runtime_str, "%d", pcb->remainingtime);
        execl("./process.out", "process.out", runtime_str, NULL);
        exit(0);
    } else if (pid > 0) {
        pcb->pid = pid;
        if (pcb->starttime == -1) {
            pcb->starttime = getClk();
        }
    } else {
        perror("Fork failed");
    }
}

void RoundRobin(int quantum, int msgq_id)
{
    msgbuff message;
    LinkedList* processList = list_create();
    Queue* readyQueue = queue_create();
    SchedulerStats stats = {0, 0, 0, 0};
    PCB* currentProcess = NULL;
    int targetTime = 0; // time when current process should yield

    while(1){
        // 1- Get all processes that have arrived by current time
        while(msgrcv(msgq_id, &message, sizeof(message.process), 0, IPC_NOWAIT)>0){
            if(message.mtype==NEW_PROCESS){

                // Add process to processList
                PCB* pcb = pcb_create(message.process.id, message.process.arrivaltime, message.process.runningtime, message.process.priority, message.process.dependencyId);
                list_add_front(processList, pcb);
    
                // check dependencies
                int isDependent = 0;
                if(pcb->dependencyId != -1){
                    PCB* dependencyPCB = list_find(processList, pcb->dependencyId);
                    if(dependencyPCB){
                        // add to dependency's dependents queue
                        pcb->state=BLOCKED;
                        queue_enqueue(dependencyPCB->dependents, pcb);
                        isDependent=1;
                    }
                }
    
                // If no dependencies, add to ready queue
                if(!isDependent){
                    pcb->state = READY;
                    queue_enqueue(readyQueue, pcb);
                }

                stats.totalProcesses++;
            }
            else if(message.mtype==TERMINATE_PROCESS){
                // 2- finished processes and unblock dependents
                PCB* finishedPCB = list_find(processList, message.process.id);
                if(finishedPCB){
                    // unblock dependents
                    Queue* dependents = finishedPCB->dependents;
                    while(dependents->size){
                        PCB* dependentPCB=queue_front(finishedPCB->dependents)->pcb;
                        dependentPCB->state = READY;
                        queue_enqueue(readyQueue, dependentPCB);
                        queue_dequeue(finishedPCB->dependents);
                    }
                    // remove finished process from processList
                    list_remove(processList, finishedPCB);

                    finishedPCB->finishtime = getClk();
                    finishedPCB->turnaround = finishedPCB->finishtime - finishedPCB->arrivaltime;
                    finishedPCB->wturnaround = (float)finishedPCB->turnaround / finishedPCB->executiontime;
                    stats.totalWaitingTime += finishedPCB->waitingtime;
                    stats.totalTurnaroundTime += finishedPCB->turnaround;
                    stats.totalExecutionTime += finishedPCB->executiontime;

                    if(currentProcess == finishedPCB)
                        currentProcess = NULL;
                }
            }
        }

        // 3- Schedule processes in RR manner
        // check if current process's quantum expired
        if(currentProcess && getClk() == targetTime){
            kill(currentProcess->pid, SIGSTOP);
            currentProcess->state = READY;
            currentProcess->remainingtime -= (targetTime - currentProcess->lastActive);
            currentProcess->lastActive = targetTime;
            queue_enqueue(readyQueue, currentProcess);
            currentProcess = NULL;
        }

        // if no current process, get next from ready queue
        if(!currentProcess && readyQueue->size){
            currentProcess = queue_front(readyQueue)->pcb;
            queue_dequeue(readyQueue);
            currentProcess->state = RUNNING;
            if(currentProcess->starttime == -1){
                currentProcess->starttime = getClk();
                currentProcess->waitingtime = currentProcess->starttime-currentProcess->arrivaltime;
                fork_process(currentProcess);
            }else{
                currentProcess->waitingtime = getClk()-currentProcess->lastActive;
                kill(currentProcess->pid, SIGCONT);
            }
            targetTime = getClk() + quantum;
        }

        if(processList->size == 0 && noArrivingProcesses)
            break;

    }

}

