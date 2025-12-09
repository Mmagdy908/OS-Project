#pragma once
#include <stdlib.h>

typedef struct PTE{
    int frame_number;
    int valid;
} PTE;


typedef struct PTListNode{
    int process_id;
    PTE* PT;
    struct PTListNode* next;
} PTListNode;


typedef struct{
    PTListNode* Head;
    int size;
} PTLinkedList;

PTLinkedList* PT_list_create() {
    PTLinkedList* ll = (PTLinkedList*)malloc(sizeof(PTLinkedList));
    ll->Head = NULL;
    ll->size = 0;
    return ll;
}

void PT_list_add(PTLinkedList* ll, int process_id, PTE* PT) {
    PTListNode* newNode = (PTListNode*)malloc(sizeof(PTListNode));
    newNode->process_id = process_id;
    newNode->PT = PT;
    newNode->next = NULL;
    if (!ll->Head) {
        ll->Head = newNode;
    } else {
        PTListNode* curr = ll->Head;
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = newNode;
    }
    ll->size++;
}

PTE* PT_list_find(PTLinkedList* ll, int process_id) {
    PTListNode* curr = ll->Head;
    while (curr) {
        if (curr->process_id == process_id) {
            return curr->PT;
        }
        curr = curr->next;
    }
    return NULL;
}

void PT_list_remove(PTLinkedList* ll, int process_id) {
    if (!ll->Head) return;
    
    if (ll->Head->process_id == process_id) {
        PTListNode* temp = ll->Head;
        ll->Head = ll->Head->next;
        free(temp->PT);
        free(temp);
        ll->size--;
        return;
    }
    
    PTListNode* curr = ll->Head;
    while (curr->next && curr->next->process_id != process_id) {
        curr = curr->next;
    }
    
    if (curr->next) {
        PTListNode* temp = curr->next;
        curr->next = curr->next->next;
        free(temp->PT);
        free(temp);
        ll->size--;
    }
}

void PT_list_clear(PTLinkedList* ll) {
    if(!ll) return;

    while(ll->size){
        PT_list_remove(ll, ll->Head->process_id);
    }
    
    free(ll);
}