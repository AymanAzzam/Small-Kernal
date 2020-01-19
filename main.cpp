#include <stdio.h>
#include <ctime>
#include <unistd.h>
#include <sys/msg.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string>
#include <string.h>
#include <stdlib.h>

#include <iostream>
using namespace std;

int n;
int timer = 0, clk = 0;

struct request      // process ~ kernel
{
    long mtype;     // pid
    char msg[65];     // request / response: 0 - successful ADD | 1 - successful DEL | 2 - unable to ADD | 3 - unable to DEL
};

struct req_disk
{
    long mtype;     // pid
    int msg;
};

void child_handler(int signum)
{
    int pid, status_loc;
    while(true)
    {
        int pid = waitpid(-1, &status_loc, WNOHANG);
        if (pid <= 0)
        {
            break;
        }
        if (WIFEXITED(status_loc))
        {
            --n;
            printf("A child with process %i has terminated with status %i", pid, WEXITSTATUS(status_loc));
        }
        else if (WIFSIGNALED(status_loc))
        {
            // --n;
            printf("A child (disk) with process %i has terminated with status %i\n", pid, WEXITSTATUS(status_loc));
        }
    }
}

void usr2_handler(int signum)
{
    if(timer)
        timer-=1;
    clk++;
}

void excecute_kernel(key_t up_p, key_t up_d, key_t down_d,int disk_id);

int main(int argc, char* argv[])
{
    pid_t pid;
    int disk_id;
    key_t up_p, down_p, up_d, down_d;
    char process_Path[] = "./process";
    char disk_Path[] = "./disk";
    int execl_ret;

    up_p = msgget(IPC_PRIVATE, IPC_CREAT | 0770);   // 7-> for owner, 7-> for group, -> 4 for others 
    up_d = msgget(IPC_PRIVATE, IPC_CREAT | 0770);   // 4 is octal -> mean 100  
    down_d = msgget(IPC_PRIVATE, IPC_CREAT | 0770); // first for read, second for write third for excecute 
    
    if(up_p == -1 || up_d == -1 || down_d == -1)   perror("Error in Creating message queues");

    string s[3];

    s[0] = to_string(up_p);
    const char *arg_up_p = s[0].data();

    s[1] = to_string(up_d);
    const char *arg_up_d = s[1].data();

    s[2] = to_string(down_d);
    const char *arg_down_d = s[2].data();
    
    n = argc-1;
    
    pid = fork();    // disk
    disk_id = pid;
    
    if (pid == 0)
    {
        execl_ret = execl(disk_Path, disk_Path, arg_up_d, arg_down_d, NULL);
        if(execl_ret==-1)   perror("Error in execute disk");
        return 0;
    }
    else
    {
        for (int i = 0; i < n; ++i) // n is number of children
        {
            pid = fork();
            if (pid == 0)
            {
                char file[100] = "files/";
                strcat(file,argv[i+1]);
                execl_ret = execl(process_Path, process_Path, arg_up_p, file, NULL);
                if(execl_ret==-1)   perror("Error in execute process");
                return 0;
            }
        }
    }
    signal(SIGUSR2, usr2_handler);
    signal(SIGCHLD, child_handler);
    excecute_kernel(up_p, up_d, down_d,disk_id);

    
    sleep(1);
    kill(disk_id,SIGKILL);
}

void excecute_kernel(key_t up_p, key_t up_d, key_t down_d, int disk_id)
{
    int rec_val, sendval = -1;
    msqid_ds msq_ds;
    msgqnum_t nmsg = 0;
    request process_req;
    req_disk disk_res;
    time_t old_time,new_time;
    
    old_time = time(NULL);
    while(n || nmsg || timer)
    {
        msgctl(up_p, IPC_STAT, &msq_ds);
        nmsg = msq_ds.msg_qnum;
        // Send clock to al group (processes, disk, kernal)
        new_time = time(NULL);
        if(new_time-old_time>0)
            old_time = new_time,    killpg(0,SIGUSR2);
        
        if(~sendval && !timer)
        {
            printf("\ndisk finished operation at time %d\n", clk);
            sendval = -1;
        }
        while(nmsg && timer == 0)
        {   
            new_time = time(NULL);
            if(new_time-old_time>0)
                old_time = new_time,    killpg(0,SIGUSR2);
            rec_val = msgrcv(up_p, &process_req, sizeof(process_req)-sizeof(long), 0, IPC_NOWAIT);
            if(rec_val>=0)
            {
                printf("\nKernal received message(%s)\n", process_req.msg);

                char condition = process_req.msg[0];
                if(condition == 'D')
                {  
                    condition='G';
                    // send delete request to disk
                    sendval= rec_val = msgsnd(down_d, &process_req, sizeof(process_req)-sizeof(long), IPC_NOWAIT);
                    if(rec_val == -1)   perror("Error in send message to down stream disk");
                    else
                    {
                        timer = 1;
                        printf("\nKernal sent message(%s) to disk\n", process_req.msg);
                    }
                    
                }
                else if(condition=='A')
                {
                    // get number of empty slots
                    kill(disk_id, SIGUSR1);
                    rec_val = msgrcv(up_d, &disk_res, sizeof(disk_res)-sizeof(long), 0, !IPC_NOWAIT);
                    if(rec_val == -1)   perror("Error in receive message for up stream disk");
                    else if(rec_val>0)
                    {
                        condition='G';
                        printf("\nKernal got Number of empty slots = %d\n", disk_res.msg);
                        if(disk_res.msg>0)
                        {
                            // send add request to disk
                            rec_val = sendval = msgsnd(down_d, &process_req, sizeof(process_req)-sizeof(long), IPC_NOWAIT);
                            if(rec_val == -1)   perror("Error in send message to down stream disk");
                            else
                            {
                                timer = 3;
                                printf("\nKernal sent message(%s) to disk\n", process_req.msg);
                            }
                        }
                    }
                }
            }
            
        }
        msgctl(up_p, IPC_STAT, &msq_ds);
        nmsg = msq_ds.msg_qnum;
    }
    msgctl(up_p, IPC_RMID, (struct msqid_ds *) 0);
    msgctl(up_d, IPC_RMID, (struct msqid_ds *) 0);
    msgctl(down_d, IPC_RMID, (struct msqid_ds *) 0);
}
