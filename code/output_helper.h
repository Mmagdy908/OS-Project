#include <stdio.h>      
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "DS/pcb.h"
#include "DS/WTA_linked_list.h"


typedef struct {
    int totalProcesses;
    int totalWaitingTime;
    float totalWeightedTurnaroundTime;
    int totalExecutionTime;
    WTALinkedList* wtaList;
} SchedulerStats;

FILE* open_file(const char* filename, int is_log_file)
{
    FILE* file = fopen(filename, "a");
    
    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    if (is_log_file) {
        fprintf(file, "#At\ttime\tx\tprocess\ty\tstate\tarr\tw\ttotal\tz\tremain\ty\twait\tk\n");
    }

    return file;
}

void add_log( FILE* file, PCB* pcb, const char* state, int current_time)
{
    if (!file || !pcb) return;
 
    fprintf(file, "At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d\t",
            current_time,
            pcb->id,
            state,
            pcb->arrivaltime,
            pcb->executiontime,
            pcb->remainingtime,
            pcb->waitingtime);

    if (strcmp(state, "finished") == 0){
        fprintf(file, "TA\t%d\tWTA\t%.2f",
            pcb->finishtime - pcb->arrivaltime,
            (float)(pcb->finishtime - pcb->arrivaltime)/pcb->executiontime);
    }

    fprintf(file,"\n");
}

void add_performance(FILE* file, SchedulerStats* stats, int total_time){
    if (!file) return;
    
    // calculate standard deviation of WTA
    float WTA_mean=stats->totalWeightedTurnaroundTime/stats->totalProcesses;

    float wta_sum_sq=0.0;

    while(stats->wtaList->size){
        float wta=stats->wtaList->Head->wta;
        wta_sum_sq+=pow(wta - WTA_mean, 2);
        wta_list_remove_front(stats->wtaList);
    }

    wta_list_clear(stats->wtaList);
    float std_wta=sqrt(wta_sum_sq/stats->totalProcesses);

    fprintf(file, "CPU utilization = %d%%\nAvg WTA = %.2f\nAvg Waiting = %.2f\nStd WTA = %.2f\n",
        (stats->totalExecutionTime * 100)/total_time,
        stats->totalWeightedTurnaroundTime/stats->totalProcesses,
        (float)stats->totalWaitingTime/stats->totalProcesses,
        std_wta);
}

void close_file(FILE* file)
{
    if (file) {
        fclose(file);
    }
}

