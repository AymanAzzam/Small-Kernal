#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

// struct used in receiving messages from kernel
struct msgbuff1
{
   long mtype;
   char msg[65];
};

//struct used in sending messages to kernel
struct msgbuff2
{
   long mtype;
   int cnt;
};

void handler1(int signum);  // handler for first signal sent by kernal
void handler2(int signum);  // handler for second signal sent by kernal

long clk = 0, emptyslots = 10, timer = 0, reci = -1;;
key_t upstream, downstream;


int main(int argc, char *argv[])
{
    char disk[10][64];  /* the disk slots */
    bool full[10];    /* a boolean states the state of each slot */
    memset(full, 0, sizeof(full));
    upstream = atoi(argv[1]);       // msgqid for sending to kernel
    downstream = atoi(argv[2]);     // msgqid for receiving from kernel

    signal (SIGUSR1, handler1);
    signal (SIGUSR2, handler2);
    int rec_val;
    struct msgbuff1 message;
    


    while(1)
    {   
        rec_val = msgrcv(downstream, &message, sizeof(message)-sizeof(long), 0, IPC_NOWAIT);
        if(rec_val != -1){
	        printf("\nDisk Got message(%s) at time %ld \n",message.msg,clk);
            reci = 0;
            if(message.msg[0] == 'A')
            {
                // if add find an empty slot and add the message
                emptyslots--;
                timer = 3;
                for(int i = 0; i < 10; ++i)
                {
                    if(!full[i])
                    {
                        full[i] = 1;
                        strcpy(disk[i], message.msg+1);
                        break;
                    }
                }
		        message.msg[0] = 'G';
            }
            else if(message.msg[0] == 'D')
            {
                // on delete increment the number of empty slots and delete the required slot.
                int i = message.msg[3]-'0'; 
                timer = 1;
                emptyslots += full[i];
                full[i] = 0; 
                memset(disk[i],0,sizeof(disk[i]));
		        message.msg[0] = 'G';
            }
        } 
        if(timer == 0 && ~reci)
        {
            printf("\nDisk content after operation\n");
            for(int i = 0; i < 10; ++i)
            {
                if(full[i]) {
                    printf("#%d: %s\n" , i, disk[i]);
                }
                else {
                    printf("#%d: empty\n", i);
                }
            }
            reci = -1;
        }
    }

}

void handler2(int signum)
{
    // increment the clock
    clk++; 
    timer -= (timer > 0);
}

void handler1(int signum)
{   
    // send the number of empty slots in disk
    int send_val;
    struct msgbuff2 message;
    message.cnt = emptyslots;
    message.mtype = getpid();
  
  
    send_val = msgsnd(upstream, &message, sizeof(message.cnt), IPC_NOWAIT);
  	
}
