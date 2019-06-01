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

pid_t keyManager;

void signalHandlerServer(int signal);
void closeAndRemoveIPC();
void signalHandlerKeyManager(int signal);
char * hash(struct Request *request, char * result);
void sendResponse(struct Request *request, char hash[10]);
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

                printf("U->%s t->%ld key->%s\n", tmp.userIdentifier, tmp.timeStamp, tmp.key);

                if(tmp.timeStamp == 0)continue;
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

                printf(" SERVICE %s  USER%s \n" , request.serviceName,  request.userIdentifier);
                fflush(stdout);

                //if(offset == MAX_REQUEST_INTO_MEMORY)
                    //signalHandlerServer();

                semOp(semid, MUTEX, -1);
                // search for a free area or a area that can be rewritten because it's invalid
                struct SHMKeyData shmKeyData;
                struct SHMKeyData *tmpOffset = shmPointer;
                for (int i = 0; i < MAX_REQUEST_INTO_MEMORY; i++) {
                    memcpy(&shmKeyData, tmpOffset + i, sizeof(struct SHMKeyData));    //increase pointer to access the next struct


                    if(shmKeyData.timeStamp == 0){
                        //area can be written
                        printf("sto inserendo i dati..\n");
                        fflush(stdout);
                        strcpy( shmKeyData.userIdentifier , request.userIdentifier);

                        //create hash
                        char hashArray[25]  = "";
                        hash(&request, hashArray);
                        strcpy(shmKeyData.key , hashArray);

                        shmKeyData.timeStamp = time(0);
                        memcpy(tmpOffset + i, &shmKeyData, sizeof(struct SHMKeyData));
                        sendResponse(&request, hashArray);
                        break;
                    };

                }
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

char * hash(struct Request *request, char * result){

    if(strcmp(request->serviceName, "stampa") == 0)
        strcpy(result, "stp");
    else if(strcmp(request->serviceName, "salva") == 0)
        strcpy(result, "slv");
    else if(strcmp(request->serviceName, "invia") == 0)
        strcpy(result, "inv");
    else strcpy(result, "udf");

    int hash = 31 * request->clientPid;
    for (int i = 0; request->userIdentifier[i] != '\0' || i < 10 ; i++)
        hash += request->userIdentifier[i];

    char tmp[25];
    snprintf(tmp,25, "%d",hash); //todo look
    strcat(result, tmp);

    
    strcpy(result, "provaKey");
    return result;
};

void sendResponse(struct Request *request, char hash[25]) {

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
};

