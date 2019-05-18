#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

#include "../inc/errExit.h"
#include "../inc/request_response.h"

char *pathToServerFIFO = "/tmp/fifo_server";
char *basePathToClientFIFO = "/tmp/fifo_client."; // to handle multiple process
int serverFIFO, serverFIFO_extra;

void closeFIFOandTerminate(){
    // Close the FIFO
    if (serverFIFO != 0 && close(serverFIFO) == -1)
        errExit("close server fifo -> failed");

    if (serverFIFO_extra != 0 && close(serverFIFO_extra) == -1) //todo why?
        errExit("close failed");

    // Remove the FIFO
    if (unlink(pathToServerFIFO) != 0)
        errExit("unlink server fifo -> failed");

    // terminate the process
    _exit(0);
};

int hash(struct Request *request){
    return 1234 * request->clientPid;
};

void sendResponse(struct Request *request) {

    // get the extended path for the fifo ( base path + pid )
    char pathToClientFIFO [25];
    sprintf(pathToClientFIFO, "%s%d", basePathToClientFIFO, request->clientPid);

    // Open the client's FIFO in write-only mode
    int clientFIFO = open(pathToClientFIFO, O_WRONLY);
    if (clientFIFO == -1) {
        printf("server open the client fifo -> failed");
        return; //todo check
    }

    // Prepare the response for the client
    struct Response response;
    response.key = hash(request);


    // write the response into the client fifo
    if (write(clientFIFO, &response,sizeof(struct Response))
            != sizeof(struct Response)) {
        printf("<Server> write failed");
        return;
    }

    // Close the FIFO
    if (close(clientFIFO) != 0)
        printf("<Server> close failed");


};



int main (int argc, char *argv[]) {
    sigset_t mySet, prevSet;
    // initialize mySet to contain all signals
    sigfillset(&mySet);
    // remove SIGTERM from mySet
    sigdelset(&mySet, SIGTERM);
    // blocking all signals but SIGTERM
    sigprocmask(SIG_SETMASK, &mySet, &prevSet);

    if (signal(SIGTERM, closeFIFOandTerminate) == SIG_ERR)
        errExit("change signal handler failed");

    printf("%d", getpid());
    fflush(stdout);



    //todo giusto che crashi quando  il file è gia statp creato

    // make the server fifo -> rw- -w- ---
    if (mkfifo(pathToServerFIFO, S_IRUSR | S_IWUSR | S_IWGRP) == -1)
        errExit("creation of server fifo -> failed");

    // wait a client req
    serverFIFO = open(pathToServerFIFO, O_RDONLY);
    if (serverFIFO == -1)
        errExit("open server fifo  read-only -> failed");


    // Open an extra descriptor, so that the server does not see end-of-file    todo why
    // even if all clients closed the write end of the FIFO
    serverFIFO_extra = open(pathToServerFIFO, O_WRONLY);
    if (serverFIFO_extra == -1)
       errExit("open write-only failed");                                     //todo why?

    struct Request request;
    int bR = -1;                                                                //todo buffer -1 why?

    do{

        bR = read(serverFIFO, &request, sizeof(struct Request));

        if (bR == -1) {
            printf("<Server> it looks like the FIFO is broken\n");
        } else if (bR != sizeof(struct Request) || bR == 0)
            printf("<Server> it looks like I did not receive a valid request\n");
        else
            sendResponse(&request);


    } while (bR != -1);

    //sigprocmask(SIG_SETMASK, &prevSet, NULL); todo serve?


    //register closeFIFOs() as pre-exit function
    atexit(closeFIFOandTerminate);
}
