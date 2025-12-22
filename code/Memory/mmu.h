#ifndef MMU_H
#define MMU_H

typedef struct MemoryFrame
{
    int process_id;
    int reference;
    int dirty;
    int virtual_page_number; // (virtual number for reverse mapping)
} MemoryFrame;

void init_MMU();
int setup_page_table(int process_id);
int MMU_request(int process_id, int virtual_page_number, int write, int currentTime);
void clear_MMU_resources();
void release_process_frames(int process_id);
void close_memory_log();

#endif
