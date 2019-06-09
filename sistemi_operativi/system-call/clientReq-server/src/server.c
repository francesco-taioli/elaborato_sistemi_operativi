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
#include <math.h>

#include "../inc/errExit.h"
#include "../inc/request_response.h"
#include "../inc/shared_memory.h"
#include "../inc/semaphore.h"

#define MAX_REQUEST_INTO_MEMORY 15
#define MUTEX 0

char *pathToServerFIFO = "/tmp/fifo_server";
char *basePathToClientFIFO = "/tmp/fifo_client."; // to handle multiple process
char *pathKeyFtok = "/tmp/vr422009.tmp";

int serverFIFO, serverFIFO_extra, semid, shmidServer;
struct SHMKeyData *shmPointer;

pid_t keyManager;

void signalHandlerServer(int signal);
void closeAndRemoveIPC();
void signalHandlerKeyManager(int signal);
int  hash(struct Request *request);
void sendResponse(struct Request *request,int hash);
int createSemaphoreSet(key_t semkey);
void createFileForKeyManagement();



int main (int argc, char *argv[]) {
    sigset_t mySet, prevSet;
    // initialize mySet to contain all signals
    sigfillset(&mySet);
    // remove SIGTERM from mySet
    sigdelset(&mySet, SIGTERM);
    // blocking all signals but SIGTERM
    sigprocmask(SIG_SETMASK, &mySet, &prevSet);

    if (signal(SIGTERM, signalHandlerServer) == SIG_ERR)
       errExit("change signal handler failed");

    printf("%d\n", getpid());
    fflush(stdout);

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


    //create keyMananger
    keyManager = fork();
    if(keyManager == 0)
    {
                                                                ////////// keyamanger //////////

        // set signalHandler for keymanager -> sigterm
        if (signal(SIGTERM, signalHandlerKeyManager) == SIG_ERR)
            errExit("keyManager unable to catch signal sigterm");


        while(1){
            sleep(30);

            semOp(semid, MUTEX, -1);
            struct SHMKeyData tmp;
            struct SHMKeyData *tmpOffset = shmPointer;
            for (int i = 0; i < MAX_REQUEST_INTO_MEMORY; i++) {
                memcpy(&tmp, tmpOffset + i, sizeof(struct SHMKeyData));    //increase pointer to access the next struct

                //printf("U->%s t->%ld key->%d\n", tmp.userIdentifier, tmp.timeStamp, tmp.key);

                if(tmp.timeStamp == 0)continue;
                if(time(NULL) - tmp.timeStamp > 60 * 1)
                {
                    // make the key invalid
                    tmp.key = -1;
                    memcpy(tmpOffset + i, &tmp, sizeof(struct SHMKeyData));
                    printf("\nremove key : -> User %s Key%d\n", tmp.userIdentifier, tmp.key);
                    fflush(stdout);
                }
            }

            // todo nel caso ho saturato la memoria?
            semOp(semid, MUTEX, 1);
        }

    }
    else if(keyManager > 0)
    {
        ////////// parent  //////////
        struct Request request;
        ssize_t bR = -1;                                                                //todo buffer -1 why?
        do{

            bR = read(serverFIFO, &request, sizeof(struct Request));

            if (bR == -1) {
                printf("<Server> it looks like the FIFO is broken\n");
            }
            else if (bR != sizeof(struct Request) || bR == 0)
                printf("<Server> it looks like I did not receive a valid request\n");
            else {

                printf(" SERVICE %s  USER %s \n" , request.serviceName,  request.userIdentifier);
                fflush(stdout);


                semOp(semid, MUTEX, -1);
                // search for a free area or a area that can be rewritten because it's invalid
                struct SHMKeyData shmKeyData, shmToInsert;
                strcpy( shmKeyData.userIdentifier , "fsdfsdfs");

                struct SHMKeyData *tmpOffset = shmPointer;
                int index;
                for (index = 0; index < MAX_REQUEST_INTO_MEMORY; index++) {
                    memcpy(&shmKeyData, tmpOffset + index, sizeof(struct SHMKeyData));    //increase pointer to access the next struct

                    if(shmKeyData.key == 0 || shmKeyData.key == -1){ //if the area is free or the key is invalid
                        //area can be written
                        printf("creo una chiave valida..\n");
                        fflush(stdout);

                        //create hash
                        shmToInsert.key = hash(&request);
                        break;
                    };

                }
                if(index == MAX_REQUEST_INTO_MEMORY) {
                    printf("MEMORIA SATURA\n");
                    fflush(stdout);
                    shmToInsert.key = -1;
                }

                shmToInsert.timeStamp = time(NULL);
                strcpy( shmToInsert.userIdentifier , request.userIdentifier);
                memcpy(tmpOffset + index, &shmToInsert, sizeof(struct SHMKeyData));
                sendResponse(&request, shmToInsert.key);
                semOp(semid, MUTEX, 1);

                }

        } while (bR != -1);

        //sigprocmask(SIG_SETMASK, &prevSet, NULL); todo serve?
        //register closeFIFOs() as pre-exit function
        //atexit(signalHandlerServer);
    }
    else{
        //error
        printf("  %d  error fork", keyManager);
        closeAndRemoveIPC();
    }
};


void signalHandlerServer(int signal){
    kill(keyManager, SIGTERM);
    closeAndRemoveIPC();
};

void closeAndRemoveIPC(){
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
}

void signalHandlerKeyManager(int signal){
    if(signal == 15)
        exit(0);
};
int concatenate(int x, int y) {
    int pow = 10;
    while(y >= pow)
        pow *= 10;
    return x * pow + y;
}

int hash(struct Request *request){
    int hash = request->clientPid + ((rand() % 100) * 31); //rand from 0
    int serviceRecognizer = 0;
    //todo case insensitive
    if(strcmp(request->serviceName, "invia") == 0)
        serviceRecognizer = 11;
    else if(strcmp(request->serviceName, "salva") == 0 )
        serviceRecognizer = 22;
    else if(strcmp(request->serviceName, "stampa") == 0 )
        serviceRecognizer = 33;
    else
        return -1; //invalid value

    for (int i = 0; request->userIdentifier[i] != '\0' ; i++)
        hash += request->userIdentifier[i];
    int result = concatenate(hash, serviceRecognizer);

    //return result;
    return 5556;
};



void sendResponse(struct Request *request, int hash) {

    // get the extended path for the fifo ( base path + keyManager )
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
    response.key = hash;


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
};

