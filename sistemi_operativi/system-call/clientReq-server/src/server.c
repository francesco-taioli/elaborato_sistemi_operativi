#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <string.h>
#include <time.h>

#include "../inc/errExit.h"
#include "../inc/request_response.h"
#include "../inc/shared_memory.h"
#include "../inc/semaphore.h"

#define MAX_REQUEST_INTO_MEMORY 5
#define MUTEX 0

char *pathToServerFIFO = "/tmp/fifo_server";
char *basePathToClientFIFO = "/tmp/fifo_client."; // to handle multiple process
char *pathKeyFtok = "/tmp/vr422009.tmp";

int serverFIFO, serverFIFO_extra, semid, shmidServer;
struct SHMKeyData *shmPointer;

void closeFIFOandTerminate(){
    printf("------------------\n");
    fflush(stdout);
    // Close the FIFO
    if (serverFIFO != 0 && close(serverFIFO) == -1)
        errExit("close server fifo -> failed");

    if (serverFIFO_extra != 0 && close(serverFIFO_extra) == -1) //todo why? unlink server fifo
        errExit("close failed");

    // Remove the FIFO
    printf("<Server> removing server  FIFO...\n");
    if (unlink(pathToServerFIFO) != 0)
        errExit("unlink server fifo -> failed");

    //remove the ftok file
    // Remove the FIFO
    if (unlink(pathKeyFtok) != 0)
        errExit("unlink pathKeyFtok  -> failed");

    // remove the created semaphore set
    printf("<Server> removing the semaphore set...\n");
    if (semctl(semid, 0 /*ignored*/, IPC_RMID, NULL) == -1)
        errExit("semctl IPC_RMID failed");

    // detach the shared memory segment
    printf("\n<Server> detaching the shared memory segment...\n");
    free_shared_memory(shmPointer);

    // remove the shared memory segment
    printf("<Server> removing the shared memory segment...\n");
    remove_shared_memory(shmidServer);

    // terminate the process
    _exit(0);
};

char * hash(struct Request *request, char * result){
    //todo implement
    //return 1234 * request->clientPid;
    strcpy(result, "hashfasul");
    return result;
};

void sendResponse(struct Request *request, char hash[10]) {

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
    strcpy(response.key , hash);//todo check


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


int createSemaphoreSet(key_t semkey) {
    // Create a semaphore set with 2 semaphores
    int semid = semget(semkey, 1, IPC_CREAT | S_IRUSR | S_IWUSR);
    if (semid == -1)
        errExit("semget failed");

    // Initialize the semaphore set
    union semun arg;
    arg.val = 1;

    if (semctl(semid, 0, SETVAL, arg) == -1)
        errExit("semctl SETALL failed");

    return semid;
};

void createFileForKeyManagement(){
    int fd = open(pathKeyFtok, O_WRONLY | O_CREAT , S_IRWXU |S_IRWXG | S_IRWXO  );
    if (fd == -1)
        errExit("open file ftok -> failed");

    close(fd);
}



int main (int argc, char *argv[]) {
    int offset = 0; // index for access to the shm
    sigset_t mySet, prevSet;
    // initialize mySet to contain all signals
    sigfillset(&mySet);
    // remove SIGTERM from mySet
    sigdelset(&mySet, SIGTERM);
    // blocking all signals but SIGTERM
    sigprocmask(SIG_SETMASK, &mySet, &prevSet);

    if (signal(SIGTERM, closeFIFOandTerminate) == SIG_ERR)
       errExit("change signal handler failed");

    printf("%d\n", getpid());
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
        errExit("open write-only failed");

    // create file for ftok
    createFileForKeyManagement();

    // generate key with ftok
    key_t shmKey = ftok(pathKeyFtok, 'z');
    if (shmKey == -1)
        errExit("ftok for shmKey failed");

    // allocate a shared memory segment
    shmidServer = alloc_shared_memory(shmKey, sizeof(struct SHMKeyData) * MAX_REQUEST_INTO_MEMORY);

    // attach the shared memory segment
    shmPointer = (struct SHMKeyData*)get_shared_memory(shmidServer, 0);

    // create a semaphore set
    key_t semKey = ftok(pathKeyFtok, 'g');
    if (semKey == -1)
        errExit("ftok for semKey failed");

    semid = createSemaphoreSet(semKey);

    //create keyMananger
    pid_t pid = fork();
    if(pid == 0)
    {
        // keyamanger code
        while(1){
            sleep(30);

            semOp(semid, MUTEX, -1);
            struct SHMKeyData tmp;
            struct SHMKeyData *tmpOffset = shmPointer;
            for (int i = 0; i < MAX_REQUEST_INTO_MEMORY; i++) {
                memcpy(&tmp, tmpOffset + i, sizeof(struct SHMKeyData));    //increase pointer to access the next struct

                printf("U->%s t->%ld key->%s\n", tmp.userIdentifier, tmp.timeStamp, tmp.key);

                if(tmp.timeStamp == 0)break;
                if(time(0) - tmp.timeStamp > 60 * 1)
                {
                    // make the key invalid
                    strcpy(tmp.key, "-1");
                    memcpy(tmpOffset + i, &tmp, sizeof(struct SHMKeyData));
                    //printf("\nremove key : -> User %s Key%s\n", tmp.userIdentifier, tmp.key);
                }
            }

            semOp(semid, MUTEX, 1);
        }

    }
    else if(pid > 0)
    {
        //parent code
        struct Request request;
        int bR = -1;                                                                //todo buffer -1 why?
        do{

            bR = read(serverFIFO, &request, sizeof(struct Request));

            if (bR == -1) {
                printf("<Server> it looks like the FIFO is broken\n");
            }
            else if (bR != sizeof(struct Request) || bR == 0)
                printf("<Server> it looks like I did not receive a valid request\n");
            else {

                printf("sto inserendo i dati..\n");
                fflush(stdout);
                semOp(semid, MUTEX, -1);

                struct SHMKeyData shmKeyData;
                strcpy( shmKeyData.userIdentifier , request.userIdentifier);

                //create hash
                char hashArray[10]  = "";
                hash(&request, hashArray);
                strcpy(shmKeyData.key , hashArray);

                shmKeyData.timeStamp = time(0);
                memcpy(shmPointer + offset++ , &shmKeyData, sizeof(struct SHMKeyData));


                sendResponse(&request, hashArray);
                semOp(semid, MUTEX, 1);

                }

        } while (bR != -1);

        //sigprocmask(SIG_SETMASK, &prevSet, NULL); todo serve?
        //register closeFIFOs() as pre-exit function
        atexit(closeFIFOandTerminate);
    }
    else
        {
        //error
        printf("  %d  error fork", pid);
        fflush(stdout);
        errExit("\n somentigh goes wrong -> fork failed");
    }

    printf("sono qui11ddas\n");
    fflush(stdout);
}
