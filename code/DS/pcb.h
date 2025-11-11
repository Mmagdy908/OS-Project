#pragma once
#include <sys/types.h>

// Forward declaration
struct Queue;
typedef struct Queue Queue;

#include "./queue.h"


typedef enum {
    READY,
    RUNNING,
    BLOCKED,
} ProcessState;

typedef struct PCB{
    int id;
    int dependencyId;         // ID of process it depends on (-1 if none)
    int arrivaltime;
    int executiontime;      // original runtime
    int remainingtime;
    int waitingtime;
    int priority;           // 0-10, lower is higher priority
    ProcessState state;             
    int starttime;          // first time process ran (-1 if never started)
    int lastActive;        // last time process was active
    int finishtime;
    float turnaround;       // finishtime - arrivaltime
    float wturnaround;      // turnaround / executiontime
    pid_t pid;              // actual process ID after fork
    Queue* dependents;    // queue of PCB* that depend on this process
} PCB;

PCB* pcb_create(int id, int arrival, int runtime, int priority, int dependencyId) {
    PCB* pcb = (PCB*)malloc(sizeof(PCB));
    pcb->id = id;
    pcb->dependencyId = dependencyId;
    pcb->arrivaltime = arrival;
    pcb->executiontime = runtime;
    pcb->remainingtime = runtime;
    pcb->waitingtime = 0;
    pcb->priority = priority;
    pcb->state = READY;  // starts in ready/waiting state
    pcb->starttime = -1;
    pcb->lastActive = -1; 
    pcb->finishtime = -1;
    pcb->turnaround = 0;
    pcb->wturnaround = 0;
    pcb->pid = -1;
    pcb->dependents = queue_create();
    return pcb;
}

void pcb_clear(PCB* pcb) {
    queue_clear(pcb->dependents);
    free(pcb->dependents);
    free(pcb);
}