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
#include <sys/wait.h>
#include <sys/shm.h>


#include "../inc/errExit.h"
#include "../inc/request_response.h"
#include "../inc/shared_memory.h"
#include "../inc/semaphore.h"

#define MAX_REQUEST_INTO_MEMORY 100

#define MINUTES 5
#define MUTEX 0

char *pathToServerFIFO = "/tmp/vr422009.fifo_server";
char *basePathToClientFIFO = "/tmp/vr422009.fifo_client."; // to handle multiple process
char *pathKeyFtok = "/tmp/vr422009.tmp";

int serverFIFO, serverFIFO_extra, semid, shmidServer;
struct SHMKeyData *shmPointer;

pid_t keyManager;
int childCreated = 0;

void signalHandlerServer(int signal);
void closeAndRemoveIPC();
void signalHandlerKeyManager(int signal);
int  hash(struct Request *request);
void sendResponse(struct Request *request);
int createSemaphoreSet(key_t semkey);
void createFileForKeyManagement();
void checkMemoryForDeletion();
void child();



int main (int argc, char *argv[]) {
    //register closeAndRemoveIPC as pre-exit function
    atexit(closeAndRemoveIPC);

    srand(time(0));
    sigset_t mySet, prevSet;
    // initialize mySet to contain all signals
    sigfillset(&mySet);
    // remove SIGTERM from mySet
    sigdelset(&mySet, SIGTERM);
    sigdelset(&mySet, SIGALRM);
    // blocking all signals but SIGTERM
    sigprocmask(SIG_SETMASK, &mySet, &prevSet);

    if (signal(SIGTERM, signalHandlerServer) == SIG_ERR)
       errExit("change signal handler failed");

    printf("<Server> Running with pid %d\n", getpid());

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

    // make the server fifo -> rw- -w- ---
    if (mkfifo(pathToServerFIFO, S_IRUSR | S_IWUSR | S_IWGRP) == -1)
        errExit("creation of server fifo -> failed");


    // wait a client request
    serverFIFO = open(pathToServerFIFO, O_RDONLY);
    if (serverFIFO == -1)
        errExit("open server fifo  read-only -> failed");

    // Open an extra descriptor, so that the server does not see end-of-file
    // even if all clients closed the write end of the FIFO
    serverFIFO_extra = open(pathToServerFIFO, O_WRONLY);
    if (serverFIFO_extra == -1)
        errExit("open write-only failed");


    //create keyMananger
    keyManager = fork();
    childCreated = 1;
    if(keyManager == 0) child();
    else if(keyManager > 0)
    {
        ////////// parent  //////////
        struct shmid_ds buf;
        shmctl(shmidServer, IPC_STAT, &buf);
        printf("<Server> Natch %ld\n", buf.shm_nattch);

        //////// fine check shmid_ds

        struct Request request;
        ssize_t bR = -1;
        do{
            bR = read(serverFIFO, &request, sizeof(struct Request));
            if (bR == -1) {
                printf("<Server> it looks like the FIFO is broken\n");
            }
            else if (bR != sizeof(struct Request) || bR == 0)
                printf("<Server> it looks like I did not receive a valid request\n");
            else  sendResponse(&request);

        } while (bR != -1);

    }
    else{
        //error
        printf("  %d  error fork", keyManager);
        exit(1);
    }
};

void checkMemoryForDeletion(){
	printf("<Keyman> sono in alarm\n");
	
        semOp(semid, MUTEX, -1);
        printf("<KeyManager> cerco chiavi scadute ...\n");

        for (int i = 0; i < MAX_REQUEST_INTO_MEMORY; i++) {
            //printf(" U:%s K:%d T:%ld\n", shmPointer[i].userIdentifier, shmPointer[i].key, shmPointer[i].timeStamp);
            if(shmPointer[i].timeStamp == 0)continue;
            if(time(NULL) - shmPointer[i].timeStamp > 60 * MINUTES )
            {
                // make the key invalid
                if(shmPointer[i].key != -1){
                    printf("<KeyManager>remove key.. User: %s Key: %d\n", shmPointer[i].userIdentifier, shmPointer[i].key);
                    shmPointer[i].key = -1;
                }
            }
        }
        semOp(semid, MUTEX, 1);
    alarm(2);
    
    
};
void child(){
    if (signal(SIGTERM, signalHandlerKeyManager) == SIG_ERR)
        errExit("keyManager unable to catch signal sigterm");

    if(signal(SIGALRM, checkMemoryForDeletion) == SIG_ERR)
        errExit("keyman unable to handler sigalarm");

    alarm(2); 
    for(;;)
   	pause();
};


void signalHandlerServer(int signal){
    printf("<server> receive %s ----- \n", signal == 15? "SIGTERM" : "signal");
    exit(0);
};

void signalHandlerKeyManager(int signal){
    printf("<keyManager> receive %s -----\n", signal == 15? "SIGTERM" : "signal");
    exit(0);
};

void closeAndRemoveIPC(){
    // eseguo questo se sono il padre o in caso di crash del programma
    if(keyManager != 0 || ( keyManager == 0 && childCreated == 0)){

        printf("<server> removing files ...\n");
        // Close the FIFO
        if (serverFIFO != 0 && close(serverFIFO) == -1)
            errExit("close server fifo -> failed");

        if (serverFIFO_extra != 0 && close(serverFIFO_extra) == -1)
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
        printf("<Server> detaching the shared memory segment...\n");
        free_shared_memory(shmPointer);

        // remove the shared memory segment
        printf("<Server> removing the shared memory segment...\n");
        remove_shared_memory(shmidServer);
        if(childCreated){
            kill(keyManager, SIGTERM);
            int status;
            wait(&status);
            printf("Child terminated with %i\n", WEXITSTATUS(status));
        }



    }
}


int concatenate(int x, int y) {
    int pow = 10;
    while(y >= pow)
        pow *= 10;
    return x * pow + y;
}

int hash(struct Request *request){
    int hash = request->clientPid + ((rand() % 100) * 31);
    int serviceRecognizer = 0;

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

    return result;
};



void sendResponse(struct Request *request) {
    int key = -1;

    printf("<Server> richiesta ricevuta ... service: %s  user :%s \n" , request->serviceName,  request->userIdentifier);

    semOp(semid, MUTEX, -1);
    // search for a free area or a area that can be rewritten because it's invalid
    int memoryIsSaturated = 1; //1 is true
    int index;
    for ( index = 0; index < MAX_REQUEST_INTO_MEMORY; index++) {
        if(shmPointer[index].key == 0 || shmPointer[index].key == -1){ //if the area is free or the key is invalid
            //area can be written
            memoryIsSaturated = 0;
            printf("<Server>creo una chiave ..\n");

            //create hash
            key = hash(request);
            shmPointer[index].key = key;
            break;
        };
    }

    if(memoryIsSaturated) {
        printf("MEMORIA SATURA\n");
        fflush(stdout);
    }

    shmPointer[index].timeStamp = time(NULL);
    strcpy( shmPointer[index].userIdentifier , request->userIdentifier);
    semOp(semid, MUTEX, 1);
\
    // get the extended path for the fifo ( base path + keyManager )
    char pathToClientFIFO [35];
    sprintf(pathToClientFIFO, "%s%d", basePathToClientFIFO, request->clientPid);

    // Open the client's FIFO in write-only mode
    int clientFIFO = open(pathToClientFIFO, O_WRONLY);
    if (clientFIFO == -1) {
        printf("server open the client fifo -> failed");
        return;
    }

    // Prepare the response for the client
    struct Response response;
    response.key = key;

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

