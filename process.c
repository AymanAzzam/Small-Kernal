#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

struct msgbuff
{
	long mtype;
	char mtext[65];
};

void do_process(key_t msgqid, char *filename);
void SIGUSR2Handler(int signum);
long clk = 0;

int main(int argc, char* argv[])
{
	signal(SIGUSR2, SIGUSR2Handler);
	//this code should be done from kernel

	//The message queue that the processes will use to send to kernel
	key_t upstream = atoi(argv[1]); //need to convert
	
	do_process(upstream, argv[2]);
}

void SIGUSR2Handler(int signum)
{
	clk++;
	//printf("My Pid is [%d], my clk is %d, and i have got signal #%d\n",getpid(), clk, signum);
}

void do_process(key_t msgqid, char *filename)
{
	FILE *fp;
	fp = fopen(filename, "r");
	while (!feof(fp))
	{
		int time;
		fscanf(fp, "%d", &time);
		while (time > clk)
		{
		} //busy wait untill the time equals the clock
		char operation[5];
		fscanf(fp, "%s", operation);
		char str[65];
		if (strcmp(operation, "ADD") == 0)
			str[0] = 'A', fgets(str + 1, 64, fp);
		else if (strcmp(operation, "DEL") == 0)
			str[0] = 'D', str[1] = ' ', fgets(str + 2, 63, fp);

		//remove the '\n' char (if found)
		char* c = strchr(str, '\n');
		if(c)
			*c = '\0';
		
		struct msgbuff message;

		message.mtype = getpid();
		strcpy(message.mtext, str);

		//just send the message and don't wait		
		int send_val = msgsnd(msgqid, &message, sizeof(message.mtext), IPC_NOWAIT);
		
		if(send_val == -1)
			perror("Error in send");
		else
			printf("\nPid %d sent message (%s) at time %ld \n",getpid(),message.mtext,clk);
	}
	fclose(fp);
}
