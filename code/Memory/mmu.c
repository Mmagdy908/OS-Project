#include "mmu.h"
#include <stdio.h>
#include <stdlib.h>
#include "../DS/PT.h"
#include <stdio.h>
// #include "headers.h"


#define PAGE_TABLE_SIZE 64
#define MEMORY_SIZE 32

static FILE *memory_log = NULL;

PTLinkedList *page_tables_list = NULL;
MemoryFrame memory[MEMORY_SIZE];
int free_frames_stack[MEMORY_SIZE];
int stack_top = -1;

int clock_hand = 0;

void init_MMU()
{

    stack_top = -1;
    page_tables_list = PT_list_create();
    for (int i = MEMORY_SIZE - 1; i >= 0; i--)
    {
        memory[i].process_id = -1;
        memory[i].reference = 0;
        memory[i].dirty = 0;
        free_frames_stack[++stack_top] = i;
    }

    memory_log = fopen("memory.log", "w");
    if (!memory_log)
    {
        perror("Error opening memory.log");
        exit(1);
    }

}

void close_memory_log()
{
    if (memory_log)
    {
        fclose(memory_log);
        memory_log = NULL;
    }
}


int get_free_frame()
{
    if (stack_top == -1)
    {
        return -1; // No free frames
    }
    return free_frames_stack[stack_top--];
}

void free_frame(int frame_number)
{
    if (frame_number < 0 || frame_number >= MEMORY_SIZE)
    {
        printf("free_frame: Invalid frame number %d\n", frame_number);
        return;
    }

    if (stack_top == MEMORY_SIZE - 1)
    {
        printf("free_frame_stack overflow, cannot free frame %d\n", frame_number);
        return;
    }

    // invalidate the frame from page table
    PTE *page_table = PT_list_find(page_tables_list, memory[frame_number].process_id);
    if (page_table)
    {
        int virtual_page_number = memory[frame_number].virtual_page_number;

        // check for valid virtual_page_number(in case of page table frame)
        if (virtual_page_number >= 0 && virtual_page_number < PAGE_TABLE_SIZE)
            page_table[virtual_page_number].valid = 0;
    }

    memory[frame_number].process_id = -1;
    memory[frame_number].reference = 0;
    memory[frame_number].dirty = 0;
    free_frames_stack[++stack_top] = frame_number;
}

int allocate_frame(int process_id, int virtual_page_number, int *dirty_swap)
{

    int frame_number = get_free_frame();

    if (frame_number != -1)
    {

        memory[frame_number].process_id = process_id;
        memory[frame_number].reference = 1;
        memory[frame_number].virtual_page_number = virtual_page_number;
        memory[frame_number].dirty = 0;

        fprintf(memory_log, "Free Physical page  %d  allocated  \n", frame_number);

        return frame_number;
    }

    while (1)
    {
        if (memory[clock_hand].virtual_page_number == -1)
        {
            clock_hand = (clock_hand + 1) % MEMORY_SIZE;
            continue;
        }

        if (memory[clock_hand].reference == 1)
        {

            memory[clock_hand].reference = 0;
            clock_hand = (clock_hand + 1) % MEMORY_SIZE;
        }
        else
        {
            int victim_frame = clock_hand;
            fprintf(memory_log, "Swapping out page  %d  to disk  \n", victim_frame);
            if (memory[victim_frame].dirty && dirty_swap)
            {
                *dirty_swap = 1;
            }
            int victim_pid = memory[victim_frame].process_id;
            int victim_vpn = memory[victim_frame].virtual_page_number;
            PTE *victim_pt = PT_list_find(page_tables_list, victim_pid);
            if (victim_pt)
            {
                if (victim_vpn >= 0 && victim_vpn < PAGE_TABLE_SIZE)
                {
                    victim_pt[victim_vpn].valid = 0;
                    victim_pt[victim_vpn].frame_number = -1;
                }
            }
            memory[victim_frame].process_id = process_id;
            memory[victim_frame].virtual_page_number = virtual_page_number;
            memory[victim_frame].reference = 1;
            memory[victim_frame].dirty = 0;
            clock_hand = (clock_hand + 1) % MEMORY_SIZE;
            return victim_frame;
        }
    }
}

void release_process_frames(int process_id)
{
    for (int i = 0; i < MEMORY_SIZE; i++)
    {
        if (memory[i].process_id == process_id)
        {
            free_frame(i);
        }
    }
}

int MMU_request(int process_id, int virtual_page_number, int write)
{
    PTE *page_table = PT_list_find(page_tables_list, process_id);
    if (!page_table)
    {
        printf("MMU_request: Page table not found for process_id %d\n", process_id);
        return -1; // Page table not found
    }

    if (page_table[virtual_page_number].valid)
    {
        int frame_number = page_table[virtual_page_number].frame_number;
        memory[frame_number].reference = 1;
        if (write)
        {
            memory[frame_number].dirty = 1;
        }
        return 0; // No blocking needed
    }
    else
    {
        // Page fault 
        fprintf(memory_log, "PageFault upon VA %d  from process  %d \n",virtual_page_number, process_id);

        int dirty_swap = 0;
        int frame_number = allocate_frame(process_id, virtual_page_number, &dirty_swap);

        page_table[virtual_page_number].frame_number = frame_number;
        page_table[virtual_page_number].valid = 1;
        memory[frame_number].dirty = write ? 1 : 0;

        
        fprintf(memory_log, "At time %d page %d for process %d is loaded into memory page %d.  \n",
        getClk(), virtual_page_number, process_id, frame_number);

        fflush(memory_log);

        return dirty_swap ? 20 : 10; // Return blocking time
    }
}

int setup_page_table(int process_id)
{
    PTE *page_table = (PTE *)malloc(sizeof(PTE) * PAGE_TABLE_SIZE);
    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        page_table[i].frame_number = -1;
        page_table[i].valid = 0;
    }

    int page_table_frame_number = allocate_frame(process_id, -1, NULL);

    PT_list_add(page_tables_list, process_id, page_table);

    // load first page into memory
    int first_frame_number = allocate_frame(process_id, 0, NULL);
    page_table[0].frame_number = first_frame_number;
    page_table[0].valid = 1;
    memory[first_frame_number].dirty = 0;
    return page_table_frame_number; // return frame number for page table
}

void clear_MMU_resources()
{
    PT_list_clear(page_tables_list);
    close_memory_log();
}

// ==========================================
// TEST SUITE
// ==========================================

void print_test_header(const char *name)
{
    printf("\n========================================\n");
    printf("TEST: %s\n", name);
    printf("========================================\n");
}

void assert_equals(int expected, int actual, const char *msg)
{
    if (expected != actual)
    {
        printf("FAILED: %s. Expected %d, got %d\n", msg, expected, actual);
        exit(1);
    }
    else
    {
        printf("PASS: %s\n", msg);
    }
}

// // NOTE: Comment out this MAIN function before linking with scheduler!
// int main() {
//     printf("Starting MMU Tests...\n");

//     // ---------------------------------------------------------
//     // Scenario 1: Initialization
//     // ---------------------------------------------------------
//     print_test_header("Initialization");
//     init_MMU();

//     assert_equals(MEMORY_SIZE-1, stack_top, "Stack top should be 31 after init");
//     assert_equals(0, free_frames_stack[stack_top], "Top of stack should be 0 (LIFO)");

//     // ---------------------------------------------------------
//     // Scenario 2: Process Startup (Allocation)
//     // ---------------------------------------------------------
//     print_test_header("Process Startup (PID 100)");

//     int pt_frame = setup_page_table(100);

//     assert_equals(MEMORY_SIZE-3, stack_top, "Stack top should decrease by 2");

//     assert_equals(100, memory[pt_frame].process_id, "PT Frame owner should be 100");
//     assert_equals(-1, memory[pt_frame].virtual_page_number, "PT Frame VPN should be -1");

//     PTE* pt_100 = PT_list_find(page_tables_list, 100);
//     int p0_frame = pt_100[0].frame_number;

//     assert_equals(100, memory[p0_frame].process_id, "Page 0 Frame owner should be 100");
//     assert_equals(0, memory[p0_frame].virtual_page_number, "Page 0 Frame VPN should be 0");
//     assert_equals(1, pt_100[0].valid, "Page 0 should be marked valid in PT");

//     // ---------------------------------------------------------
//     // Scenario 3: Memory Request - HIT (Read)
//     // ---------------------------------------------------------
//     print_test_header("Memory Request - HIT (Read Page 0)");

//     int cycles = MMU_request(100, 0, 0);
//     assert_equals(0, cycles, "Hit should cost 0 cycles");
//     assert_equals(1, memory[p0_frame].reference, "Reference bit should be 1");
//     assert_equals(0, memory[p0_frame].dirty, "Dirty bit should remain 0 on Read");

//     // ---------------------------------------------------------
//     // Scenario 4: Memory Request - MISS (Write)
//     // ---------------------------------------------------------
//     print_test_header("Memory Request - MISS (Write Page 5)");

//     cycles = MMU_request(100, 5, 1);
//     assert_equals(10, cycles, "Miss (Clean Alloc) should cost 10 cycles");

//     int p5_frame = pt_100[5].frame_number;
//     assert_equals(100, memory[p5_frame].process_id, "Page 5 Frame owner should be 100");
//     assert_equals(1, memory[p5_frame].dirty, "Dirty bit should be 1 on Write");
//     assert_equals(1, pt_100[5].valid, "Page 5 should now be valid");

//     // ---------------------------------------------------------
//     // Scenario 5: Memory Request - HIT (Write Update)
//     // ---------------------------------------------------------
//     print_test_header("Memory Request - HIT (Write to Page 0)");

//     cycles = MMU_request(100, 0, 1);
//     assert_equals(0, cycles, "Hit should cost 0 cycles");
//     assert_equals(1, memory[p0_frame].dirty, "Dirty bit should update to 1");

//     // ---------------------------------------------------------
//     // Scenario 6: Cleanup (Termination)
//     // ---------------------------------------------------------
//     print_test_header("Process Termination");

//     int stack_before_release = stack_top;
//     release_process_frames(100);

//     assert_equals(stack_before_release + 3, stack_top, "Stack top should recover 3 frames");
//     assert_equals(0, pt_100[5].valid, "Page 5 should be invalid after release");
//     assert_equals(-1, memory[p5_frame].process_id, "Released frame PID should be -1");

//     return 0;
// }