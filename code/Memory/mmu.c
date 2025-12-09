#include "mmu.h"
#include <stdio.h>
#include <stdlib.h>
#include "../DS/PT.h"

#define PAGE_TABLE_SIZE 64
#define MEMORY_SIZE 32

PTLinkedList* page_tables_list = NULL;
MemoryFrame memory[MEMORY_SIZE];
int free_frames_stack[MEMORY_SIZE];
int stack_top = - 1;

void init_MMU(){
    page_tables_list = PT_list_create();
    for(int i = MEMORY_SIZE-1; i >=0; i--){
        memory[i].process_id = -1;
        memory[i].reference = 0;
        memory[i].dirty = 0;
        free_frames_stack[++stack_top] = i;
    }
}

int get_free_frame(){
    if(stack_top == - 1){
        return -1; // No free frames
    }
    return free_frames_stack[stack_top--];
}

void free_frame(int frame_number){
    if(frame_number < 0 || frame_number >= MEMORY_SIZE){
        printf("free_frame: Invalid frame number %d\n", frame_number);
        return;
    }

    if(stack_top == MEMORY_SIZE -1){
        printf("free_frame_stack overflow, cannot free frame %d\n", frame_number);
        return;
    }

    // invalidate the frame from page table
    PTE* page_table = PT_list_find(page_tables_list, memory[frame_number].process_id);
    if(page_table){
        int virtual_page_number = memory[frame_number].virtual_page_number;

        // check for valid virtual_page_number(in case of page table frame)
        if(virtual_page_number >= 0)
            page_table[virtual_page_number].valid = 0;
    }

    memory[frame_number].process_id = -1;
    memory[frame_number].reference = 0;
    memory[frame_number].dirty = 0;
    free_frames_stack[++stack_top] = frame_number;
}

int allocate_frame(int process_id, int virtual_page_number, int *dirty_swap){
    int frame_number = get_free_frame();
    if(frame_number == -1){
        // TODO Implement second chance algorithm here 
        // TODO set dirty_swap to 1 if a dirty page is swapped out
        // TODO ignore dirty_swap if it's NULL
        // TODO don't swap out the page table frames
        printf("allocate_frame: No free frames available, swapping not implemented yet\n");
        return 0; // temp for now
    }
    memory[frame_number].process_id = process_id;
    memory[frame_number].reference = 1;
    memory[frame_number].virtual_page_number = virtual_page_number;
    return frame_number;
}

void release_process_frames(int process_id){
    for(int i = 0; i < MEMORY_SIZE; i++){
        if(memory[i].process_id == process_id){
            free_frame(i);
        }
    }
}
/*** 
 * returns number of clock cycles which process should be blocked for
 * 0 if no blocking is needed
 * 10 if clean swap occurs
 * 20 if dirty swap is needed
 * -1 for errors
 * ***/
int MMU_request(int process_id, int virtual_page_number, int write){
    PTE* page_table = PT_list_find(page_tables_list, process_id);
    if(!page_table){
        printf("MMU_request: Page table not found for process_id %d\n", process_id);
        return -1; // Page table not found
    }

    if(page_table[virtual_page_number].valid){
        int frame_number = page_table[virtual_page_number].frame_number;
        memory[frame_number].reference = 1;
        if(write){
            memory[frame_number].dirty = 1;
        }
        return 0; // No blocking needed
    }else{
        int dirty_swap = 0;
        int frame_number = allocate_frame(process_id, virtual_page_number, &dirty_swap);
        
        page_table[virtual_page_number].frame_number = frame_number;
        page_table[virtual_page_number].valid = 1;
        memory[frame_number].dirty = write ? 1 : 0;
        return dirty_swap? 20 : 10; // Return blocking time
    }

}

int setup_page_table(int process_id)
{
    PTE* page_table = (PTE*)malloc(sizeof(PTE) * PAGE_TABLE_SIZE);
    for(int i = 0; i < PAGE_TABLE_SIZE; i++){
        page_table[i].frame_number = -1;
        page_table[i].valid = 0;
    }

    int page_table_frame_number = allocate_frame(process_id,-1, NULL);

    PT_list_add(page_tables_list, process_id, page_table);

    // load first page into memory
    int first_frame_number = allocate_frame(process_id, 0, NULL);
    page_table[0].frame_number = first_frame_number;
    page_table[0].valid = 1;
    memory[first_frame_number].dirty = 0;
    return page_table_frame_number; // return frame number for page table
}


void clear_MMU_resources(){
    PT_list_clear(page_tables_list);
}