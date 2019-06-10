#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/msg.h>
#include <fcntl.h>
#include "../inc/errExit.h"

struct message {
    long mtype;
    char commandLineParam[256]; /* array of chars as message body */
};


int main(int argc, char *argv[]) {
    printf("Hi, I'm Invia program! Let's send...\n");

    int key = atoi(argv[3]);
    int msqid = msgget(key, S_IRUSR | S_IWUSR);
    if (msqid == -1)
        errExit("msgget failed");

    struct message m;
    // message has type 1
    m.mtype = 1;
    // message contains the following string


    char buffer[256];

    for(int i=1;i<argc;i++){
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


    return 0;
}