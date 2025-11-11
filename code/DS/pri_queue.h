#pragma once
#include <stdlib.h>
#include "./pcb.h"


typedef struct PriNode{
    PCB* pcb;
    struct PriNode* next;
    int priority;
} PriNode;

typedef struct {
    PriNode* Head;
    int size;
} PriQueue;

PriQueue* pri_queue_create() {
    PriQueue* q = (PriQueue*)malloc(sizeof(PriQueue));
    q->Head = NULL;
    q->size = 0;
    return q;
}

void pri_queue_enqueue(PriQueue* q, PCB* pcb, int priority) {
    PriNode* newNode = (PriNode*)malloc(sizeof(PriNode));
    newNode->pcb = pcb;
    newNode->next = NULL;
    newNode->priority = priority;
    if (!q->Head) {
        q->Head = newNode;
    } else if(newNode->priority < q->Head->priority){
        newNode->next = q->Head;
        q->Head = newNode;
    } else {
        PriNode* curr = q->Head;
        while (curr->next && curr->next->priority <= newNode->priority) {
            curr = curr->next;
        }
        newNode->next = curr->next;
        curr->next = newNode;
    }
    q->size++;
}

PriNode* pri_queue_front(PriQueue* q) {
    return q->Head;
}

void pri_queue_dequeue(PriQueue* q) {
    if (!q->Head) return;
    PriNode* temp = q->Head;
    q->Head = q->Head->next;
    free(temp);
    q->size--;
}

void pri_queue_clear(PriQueue* q) {
    while (q->size) {
        pri_queue_dequeue(q);
    }
    free(q);
}