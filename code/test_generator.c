#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define null 0

struct processData
{
    int arrivaltime;
    int priority;
    int runningtime;
    int id;
    int dependencyId;
    int base;
    int limit;
};

void initializeProcessData(struct processData *processes, int*lastArrival, int* lastBase, int i) {
        processes[i].id = i + 1;
        processes[i].arrivaltime = *lastArrival + rand() % 11; // increasing arrival
        *lastArrival = processes[i].arrivaltime;
        processes[i].runningtime = 1 + rand() % 30;
        processes[i].priority = rand() % 11;
        processes[i].dependencyId = -1;
        processes[i].base = *lastBase;
        processes[i].limit = *lastBase + (1 + rand() % 2) * 1000;
        *lastBase += processes[i].limit;
}

void assignProcessDependency(struct processData *processes, int i,int numberOfProcesses){
    int depend = rand() % 2; 

    if (depend && i > 0) {
        int* possible = malloc(numberOfProcesses * sizeof(struct processData));
        int count = 0;

        for (int j = 0; j < i; j++) {
            int start = processes[j].arrivaltime;
            int end = processes[j].arrivaltime + processes[j].runningtime;

            if (processes[i].arrivaltime >= start && processes[i].arrivaltime <= end) {
                possible[count++] = processes[j].id;
            }
        }

        if (count > 0) {
            int randomIndex = rand() % count;
            processes[i].dependencyId = possible[randomIndex];
        } else {
            processes[i].dependencyId = -1; 
        }
    } else {
        processes[i].dependencyId = -1;
    }
}

void generateRequestAddress(int n, char* buffer) {
    for(int i = 0; i < 10; i++) {
        buffer[i] = '0';
    }

    buffer[10] = '\0'; // Null terminator

    int index = 9;
    while (n > 0 && index >= 0) {
        buffer[index] = (n % 2) ? '1' : '0';
        n /= 2;
        index--;
    }
}

void generateProcessRequestsData(int startTime, int endTime, int* generatedTime, char* addressBuffer, char* mode)
{
    int remainingTime = endTime - startTime;

    
    *generatedTime = startTime + (rand() % remainingTime);

    int addrInt = rand() % 1024;
    generateRequestAddress(addrInt, addressBuffer);

    int isWrite = rand() % 2;
    *mode = isWrite ? 'w' : 'r';
}



void writeProcessRequestsInFile(FILE *pFile, int runningTime) {
    int startingTime = 0;
    
    while (startingTime < runningTime) {
        int requestTime;
        char address[11];
        char mode;

        generateProcessRequestsData(startingTime, runningTime, &requestTime, address, &mode);

    
        fprintf(pFile, "%-10d %-20s %-10c\n",
                requestTime,
                address,
                mode
            );

        startingTime = requestTime + 1;
    }
}

void writeProcessInFile(FILE *pFile, struct processData *processes, int i) {
    fprintf(pFile, "%-5d %-10d %-10d %-10d %-15d %-10d %-10d\n",
            processes[i].id,
            processes[i].arrivaltime,
            processes[i].runningtime,
            processes[i].priority,
            processes[i].dependencyId,
            processes[i].base,
            processes[i].limit
        );
}

int main(int argc, char * argv[])
{
    FILE *pFile;
    pFile = fopen("processes.txt", "w");
    if (!pFile) {
        printf("Error opening file.\n");
        return 1;
    }

    int no;
    printf("Please enter the number of processes you want to generate: ");
    scanf("%d", &no);

    srand(time(null));

    fprintf(pFile, "%-5s %-10s %-10s %-10s %-15s %-10s %-10s\n",
        "#id", "arrival", "runtime", "priority", "dependencyId", "base", "Limit");

    struct processData *processes = malloc(no * sizeof(struct processData));
    if (!processes) {
        printf("Memory allocation failed.\n");
        fclose(pFile);
        return 1;
    }

    int lastArrival = 1;
    int lastBase = 0;
    for (int i = 0; i < no; i++) {
        initializeProcessData(processes, &lastArrival, &lastBase, i);

        assignProcessDependency(processes, i,no);
        
        writeProcessInFile(pFile, processes, i);

        char filePath[50];
        
        sprintf(filePath, "process_%d_requests.txt", i + 1);
        
        FILE* rFile=fopen(filePath, "w");
        fprintf(rFile, "%-10s %-20s %-10s\n",
        "#time", "#addressInBinary", "#r/w");

        writeProcessRequestsInFile(rFile, processes[i].runningtime);
        fclose(rFile);
        
      
        
    }
    fclose(pFile);
}
