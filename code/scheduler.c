#include "scheduler_algorithms.h"
#define MQKEY 500   // key for message queue

enum Schedulers
{
    HPF=1,
    SRTN,
    RR
};

int noArrivingProcesses = 0;
int msgq_id;

void setNoArrivingProcessesFlag(int signum){
    noArrivingProcesses=1;
}

void clearResources(int signum){
    destroyClk(true);
    msgctl(msgq_id, IPC_RMID, (struct msqid_ds *)0);
    exit(0);
}

int main(int argc, char * argv[])
{
    signal(SIGINT,clearResources);
    // signal(SIGTERM,clearResources);

    signal(SIGUSR1,setNoArrivingProcessesFlag);

    // reading arguments
    int schedulerType = atoi(argv[1]);
    int quantum=0;
    if(schedulerType==RR){
        quantum = atoi(argv[2]);
    }

    initClk();
    
    //TODO implement the scheduler :)
    // Initialize message queue
    // int  msgq_id;
    msgq_id = msgget(MQKEY, 0666 | IPC_CREAT); //create message queue and return id
    if (msgq_id == -1)
    {
        perror("Error in creating message queue in scheduler");
        exit(-1);
    }

    SchedulerStats stats;

    switch(schedulerType){
        case RR:
            stats=RoundRobin(quantum, msgq_id, &noArrivingProcesses);
            break;
        case HPF:
            // TODO get agingInterval from user
            stats=HighestPriorityFirst(10, msgq_id, &noArrivingProcesses);
            break;
        case SRTN:
            stats=ShortestRemainingTimeNext(msgq_id,&noArrivingProcesses);
            break;
    }

    // output scheduler.perf
    FILE* perf_file = open_file("scheduler.perf", 0);
    add_performance(perf_file, &stats, getClk());
    close_file(perf_file);

    // release message queue
    // msgctl(msgq_id, IPC_RMID, (struct msqid_ds *)0);

    //upon termination release the clock resources.
    destroyClk(false);
    
    return 0;
}