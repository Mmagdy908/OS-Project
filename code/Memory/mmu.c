#include "mmu.h"
#include <stdio.h>
#include "../DS/PT.h"

#define PAGE_TABLE_SIZE 64
#define MEMORY_SIZE 32

PTLinkedList* page_tables_list = NULL;
MemoryFrame memory[MEMORY_SIZE];
int free_frames_stack[MEMORY_SIZE];
int stack_top = MEMORY_SIZE - 1;

void init_MMU(){
    page_tables_list = PT_list_create();
    for(int i = MEMORY_SIZE-1; i >=0; i--){
        memory[i].process_id = -1;
        memory[i].reference = 0;
        memory[i].dirty = 0;
        free_frames_stack[i] = i;
        free_frames_stack[stack_top--] = i;
    }
}

int setup_page_table(int process_id)
{
    // Temporary stub — does nothing
    printf("[MMU] setup_page_table called for PID %d (stub)\n", process_id);
    return -1; // temp value 
}


void clear_MMU_resources(){
    PT_list_clear(page_tables_list);
}