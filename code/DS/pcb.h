#pragma once
#include <sys/types.h>
#include <string.h>

// Forward declaration
struct Queue;
typedef struct Queue Queue;

#include "./queue.h"

// Updated Process State with separate blocking states
typedef enum
{
    READY,
    RUNNING,
    BLOCKED_DEPENDENCY, // Waiting on dependency
    BLOCKED_PAGE_FAULT, // Waiting on I/O for page fault
} ProcessState;

typedef struct MemoryRequest
{
    int time;    // Relative time when request occurs
    int address; // Virtual address to access
    int write;   // 0 for read, 1 for write
} MemoryRequest;

typedef struct PCB
{
    int id;
    int dependencyId;
    int arrivaltime;
    int executiontime;
    int remainingtime;
    int waitingtime;
    int priority;
    ProcessState state;
    int starttime;
    int resumedAt;
    int lastActive;
    int readyFrom;
    int finishtime;
    int disk_base;        // Base page on disk
    int disk_limit;       // Number of pages needed
    int page_table_frame; // Physical frame holding page table

    // Memory management fields
    MemoryRequest *memory_requests; // Array of memory requests
    int num_requests;               // Number of memory requests
    int current_request_index;      // Next request to process
    int blocked_until;              // Time when process can resume after page fault

    float turnaround;
    float wturnaround;
    pid_t pid;
    Queue *dependents;
} PCB;

PCB *pcb_create(int id, int arrival, int runtime, int priority, int dependencyId, int disk_base, int disk_limit)
{
    PCB *pcb = (PCB *)malloc(sizeof(PCB));
    pcb->id = id;
    pcb->dependencyId = dependencyId;
    pcb->arrivaltime = arrival;
    pcb->executiontime = runtime;
    pcb->remainingtime = runtime;
    pcb->waitingtime = 0;
    pcb->priority = priority;
    pcb->state = READY;
    pcb->starttime = -1;
    pcb->lastActive = -1;
    pcb->finishtime = -1;
    pcb->turnaround = 0;
    pcb->wturnaround = 0;
    pcb->pid = -1;
    pcb->disk_base = disk_base;
    pcb->disk_limit = disk_limit;
    pcb->page_table_frame = -1;

    // Initialize memory management fields
    pcb->memory_requests = NULL;
    pcb->num_requests = 0;
    pcb->current_request_index = 0;
    pcb->blocked_until = 0;

    pcb->dependents = queue_create();
    return pcb;
}

void pcb_clear(PCB *pcb)
{
    if (pcb->dependents)
        queue_clear(pcb->dependents);

    if (pcb->memory_requests)
    {
        free(pcb->memory_requests);
        pcb->memory_requests = NULL;
    }

    free(pcb);
}