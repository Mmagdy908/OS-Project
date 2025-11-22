#include "headers.h"
#define PROCESS_FILE "processes.txt"
#define MQKEY 500

void clearResources(int);

// function to read input
msgbuff* readProcesses(int *count)
{
    FILE *f = fopen(PROCESS_FILE, "r");
    if (!f)
    {
        perror("Error opening processes.txt");
        exit(EXIT_FAILURE);
    }

    char line[256];
    int lines = 0;

    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == '#' || strlen(line) <= 1)        // mesh comment we mesh fady
            continue;
        lines++;
    }

    // Allocate array of msgbuff
    msgbuff *arr = malloc(lines * sizeof(msgbuff));
    if (!arr)
    {
        perror("malloc failed");
        fclose(f);
        exit(EXIT_FAILURE);
    }

    rewind(f);

    int i = 0;
    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == '#' || strlen(line) <= 1)
            continue;

        msgbuff msg;
        msg.mtype = NEW_PROCESS;  // always 1 for arrivals

        int id, arrivaltime, runningtime, priority, dependencyId;
        int read = sscanf(line, "%d %d %d %d %d",
                          &id, &arrivaltime, &runningtime, &priority, &dependencyId);

        if (read < 4)
        {
            fprintf(stderr, "Malformed line skipped: %s", line);
            continue;
        }

        if (read == 4)
            dependencyId = -1;

        msg.process.id          = id;
        msg.process.arrivaltime = arrivaltime;
        msg.process.runningtime = runningtime;
        msg.process.priority    = priority;
        msg.process.dependencyId = dependencyId;

        arr[i++] = msg;
    }

    fclose(f);
    *count = i;

    if (*count == 0)
        printf("Warning: No valid processes found.\n");

    return arr;
}

int main(int argc, char * argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
    // 1. Read the input files.
    int processCount = 0;
    msgbuff *processes = readProcesses(&processCount);

    // printf("Loaded %d processes from file.\n", processCount); deh 34an ne debug ba3din

// sort processes by arrival time
    for (int i = 0; i < processCount - 1; i++)
    {
        for (int j = 0; j < processCount - i - 1; j++)
        {
            int t1 = processes[j].process.arrivaltime;
            int t2 = processes[j + 1].process.arrivaltime;

            // swap if out of order
            if (t1 > t2)
            {
                msgbuff temp = processes[j];
                processes[j] = processes[j + 1];
                processes[j + 1] = temp;
            }
        }
    }
    // printf("Processes sorted by arrival time.\n"); 34an law magdy 3ayz ye debug ba3din

    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    int choice, quantum = 0;
    printf("Choose Scheduling Algorithm:\n");
    printf("1- HPF\n2- SRTN\n3- RR\n");         // 3ayz ab2a ata2kd
    scanf("%d", &choice);    
    if (choice == 3)    // law el user la qadar Allah e5tar rr
    {
        printf("Enter RR quantum: ");
        scanf("%d", &quantum);
    }
    //printf("Scheduling Algorithm chosen: %d\n", choice); later 34an ne debug

    //Create message queue
    int msgq_id = msgget(MQKEY, 0666 | IPC_CREAT);
    if (msgq_id == -1)
    {
        perror("Error creating message queue");
        exit(EXIT_FAILURE);
    }

    printf("Message Queue created with ID = %d\n", msgq_id);

    // 3. Initiate and create the scheduler and clock processes.
    //fork clock process
     pid_t clk_pid = fork();
    if (clk_pid == -1)
    {
        perror("Error while forking clock");
        exit(EXIT_FAILURE);
    }
    else if (clk_pid == 0)
    {
        // Child → Clock Process
        execl("./clk.out", "clk.out", NULL);
        // law kaml yeb2a ma4ta8l4
        perror("Error executing clk.out");
        exit(EXIT_FAILURE);
    }

    // printf("Clock process created with PID = %d\n", clk_pid); debug later

    //fork scheduler process

    pid_t scheduler_pid = fork();

    if (scheduler_pid == -1)
    {
        perror("Error while forking scheduler");
        exit(EXIT_FAILURE);
    }
    else if (scheduler_pid == 0)
    {
        // Child → Scheduler
        char algoStr[10];
        char quantumStr[10];

        sprintf(algoStr, "%d", choice);   // from earlier user input
        sprintf(quantumStr, "%d", quantum);   // 0 if not RR

        execl("./scheduler.out", "scheduler.out", algoStr, quantumStr, NULL);

        perror("Error executing scheduler.out");
        exit(EXIT_FAILURE);
    }

    printf("Scheduler process created with PID = %d\n", scheduler_pid);

    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this
    int x = getClk();
    printf("current time is %d\n", x);
    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters.
    // 6. Send the information to the scheduler at the appropriate time.
    // 7. Clear clock resources
    destroyClk(true);
}

void clearResources(int signum)
{
    //TODO Clears all resources in case of interruption
}
