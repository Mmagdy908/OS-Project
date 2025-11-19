#pragma once
#include <stdlib.h>


typedef struct WTANode{
    float wta;
    struct WTANode* next;
} WTANode;

typedef struct{
    WTANode* Head;
    int size;
} WTALinkedList;

WTALinkedList* wta_list_create() {
    WTALinkedList* ll = (WTALinkedList*)malloc(sizeof(WTALinkedList));
    ll->Head = NULL;
    ll->size = 0;
    return ll;
}

void wta_list_add_front(WTALinkedList* ll, float wta) {
    WTANode* newNode = (WTANode*)malloc(sizeof(WTANode));
    newNode->wta = wta;
    newNode->next = NULL;
    if (!ll->Head) {
        ll->Head = newNode;
    } else {
        newNode->next = ll->Head;
        ll->Head = newNode;
    }
    ll->size++;
}



void wta_list_remove_front(WTALinkedList* ll) {
    if (!ll->Head) return;
    
    WTANode* temp = ll->Head;
    ll->Head = ll->Head->next;
    free(temp);
    ll->size--;
}

void wta_list_clear(WTALinkedList* ll) {
    while (ll->size) {
        wta_list_remove_front(ll);
    }
    free(ll);
}