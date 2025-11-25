#pragma once
#include <stdlib.h>
struct PCB;
typedef struct PCB PCB;

#include "./pcb.h"


typedef struct QueueNode{
    PCB* pcb;
    struct QueueNode* next;
} QueueNode;

typedef struct Queue{
    QueueNode* Head;
    int size;
} Queue;


Queue* queue_create() {
    Queue* q = (Queue*)malloc(sizeof(Queue));
    q->Head = NULL;
    q->size = 0;
    return q;
}

void queue_enqueue(Queue* q, PCB* pcb) {
    QueueNode* newQueueNode = (QueueNode*)malloc(sizeof(QueueNode));
    newQueueNode->pcb = pcb;
    newQueueNode->next = NULL;
    if (!q->Head) {
        q->Head = newQueueNode;
    } else {
        QueueNode* curr = q->Head;
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = newQueueNode;
    }
    q->size++;
}

QueueNode* queue_front(Queue* q) {
    return q->Head;
}

void queue_dequeue(Queue* q) {
    if (!q->Head) return;
    QueueNode* temp = q->Head;
    q->Head = q->Head->next;
    free(temp);
    q->size--;
}

void queue_remove(Queue* q, PCB* pcb) {
    if (!q->Head) return;
    QueueNode* curr = q->Head;
    QueueNode* prev = NULL;
    while (curr) {
        if (curr->pcb == pcb) {
            if (prev) {
                prev->next = curr->next;
            } else {
                q->Head = curr->next;
            }
            free(curr);
            q->size--;
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

Queue* queue_copy(Queue* original) {
    Queue* newQueue = queue_create();
    QueueNode* curr = original->Head;
    while (curr) {
        queue_enqueue(newQueue, curr->pcb);
        curr = curr->next;
    }
    return newQueue;
}

void queue_clear(Queue* q) {
    while (q->size) {
        queue_dequeue(q);
    }
    free(q);
}