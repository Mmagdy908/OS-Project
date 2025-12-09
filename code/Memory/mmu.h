#ifndef MMU_H
#define MMU_H

typedef struct MemoryFrame{
    int process_id;
    int reference;
    int dirty;  
//int Page number (virtual number for reverse mapping) 
} MemoryFrame;

void init_MMU();
int setup_page_table(int process_id);
void clear_MMU_resources();

#endif
