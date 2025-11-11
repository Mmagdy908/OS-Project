#pragma once
#include <stdlib.h>
#include "./pcb.h"


typedef struct ListNode{
    PCB* pcb;
    struct ListNode* next;
} ListNode;

typedef struct{
    ListNode* Head;
    int size;
} LinkedList;

LinkedList* list_create() {
    LinkedList* ll = (LinkedList*)malloc(sizeof(LinkedList));
    ll->Head = NULL;
    ll->size = 0;
    return ll;
}

void list_add_front(LinkedList* ll, PCB* pcb) {
    ListNode* newNode = (ListNode*)malloc(sizeof(ListNode));
    newNode->pcb = pcb;
    newNode->next = NULL;
    if (!ll->Head) {
        ll->Head = newNode;
    } else {
        newNode->next = ll->Head;
        ll->Head = newNode;
    }
    ll->size++;
}

PCB* list_find(LinkedList* ll, int id) {
    ListNode* curr = ll->Head;
    while (curr) {
        if (curr->pcb->id == id) {
            return curr->pcb;
        }
        curr = curr->next;
    }
    return NULL;
}

void list_remove(LinkedList* ll, PCB* target) {
    if (!ll->Head) return;
    
    if (ll->Head->pcb == target) {
        ListNode* temp = ll->Head;
        ll->Head = ll->Head->next;
        pcb_clear(temp->pcb);
        free(temp);
        ll->size--;
        return;
    }
    
    ListNode* curr = ll->Head;
    while (curr->next && curr->next->pcb != target) {
        curr = curr->next;
    }
    
    if (curr->next && curr->next->pcb == target) {
        ListNode* temp = curr->next;
        curr->next = curr->next->next;
        pcb_clear(temp->pcb);
        free(temp);
        ll->size--;
    }
}

void list_clear(LinkedList* ll) {
    while (ll->size) {
        list_remove(ll, ll->Head->pcb);
    }
    free(ll);
}