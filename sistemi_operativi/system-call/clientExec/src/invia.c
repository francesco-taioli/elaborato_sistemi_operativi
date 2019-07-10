#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/msg.h>
#include <fcntl.h>
#include "../inc/errExit.h"
#include <errno.h>
struct message {
    long mtype;
    char commandLineParam[256]; /* array of chars as message body */
    long prova;
};

int createQueue(int key) {
    int msqid = msgget(key, IPC_CREAT | S_IRUSR | S_IWUSR);
    if (msqid == -1)
        errExit("msgget failed");
    return msqid;
}


void readQueue(int msqid) {
    struct message message;
    // read a message from the message queue
    while(1){
		size_t mSize = sizeof(struct message) - sizeof(long);
		if (msgrcv(msqid, &message, mSize, 1, IPC_NOWAIT) == -1)
		    if(errno == ENOMSG)
		    	break;

		printf(" sto leggendo dalla coda dei messagi ->%s -- %d\n", message.commandLineParam, message.prova);
    }
}


int main(int argc, char *argv[]) {
    printf("Hi, I'm Invia program! Let's send...\n");

    int key = atoi(argv[3]);
    if(key <= 0){
        printf("Errore - la chaive deve essere positiva\n");
        exit(0);
    }
    int read = createQueue(key);
    int msqid = msgget(key, S_IRUSR | S_IWUSR);
    if (msqid == -1)
        errExit("msgget failed");

    struct message m;
    // message has type 1
    m.mtype = 1;
    m.prova = 4124241;
    // message contains the following string
	

    char buffer[256] = {0};

    for(int i=4;i<argc;i++){
        strcat(buffer, argv[i]);
        if(argc - i != 1)
            strcat(buffer, " ");
    }
    

    memcpy(m.commandLineParam, buffer, strlen(buffer) + 1);
    // size of m is only the size of its commandLineParam attribute!
    size_t mSize = sizeof(struct message) - sizeof(long);
    // sending the message in the queue
    if (msgsnd(msqid, &m, mSize, 0) == -1)
        errExit("failed to send a message");
    if (msgsnd(msqid, &m, mSize, 0) == -1)
        errExit("failed to send a message");
    if (msgsnd(msqid, &m, mSize, 0) == -1)
        errExit("failed to send a message");

	readQueue(read);

    return 0;
}
