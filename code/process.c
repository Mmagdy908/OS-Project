#include "headers.h"
#define MQKEY 500

/* Modify this file as needed*/


int main(int argc, char * argv[])
{
    initClk();

    //TODO it needs to get the remaining time from somewhere
    int id=atoi(argv[1]);
    int remainingtime=atoi(argv[2]);
    
    printf("Process forked with id: %d and remaining time: %d\n", id, remainingtime);

    int lastActiveTime=getClk();
    while (remainingtime > 0)
    {
        while(getClk()==lastActiveTime);

        int diff=getClk()-lastActiveTime;
        if(diff==1){
            remainingtime--;
        }

        lastActiveTime=getClk();
    }

    // notifying scheduler that process has finished
    int msgq_id = msgget(MQKEY, 0666 | IPC_CREAT);
    if (msgq_id == -1)
    {
        perror("Error creating message queue");
        exit(EXIT_FAILURE);
    }
    msgbuff message={.mtype=TERMINATE_PROCESS,.process={.id=id}};

    int res = msgsnd(msgq_id, &message, sizeof(message.process), !IPC_NOWAIT);
    if (res == -1)
        perror("Errror in sending process to scheduler");
    

    destroyClk(false);

    return 0;
}
