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

#include "../inc/errExit.h"
#include "../inc/request_response.h"
#include "../inc/shared_memory.h"
#include "../inc/semaphore.h"


#define MAX_REQUEST_INTO_MEMORY 100
#define MUTEX 0

char *pathKeyFtok = "/tmp/vr422009.tmp";

int main (int argc, char *argv[]) {
    // check command line input arguments
    if (argc < 3) {
        printf("Usage: %s  username key param\n", argv[0]);
        exit(1);
    }

    char *userIdentifier = argv[1];

    // read the message username
    int key = atoi(argv[2]);
    if (key <= 0) {
        printf("Key is invalid!\n");
        exit(1);
    }

    int keyIsValid = 0; //1 = true

    // access to shared memory
    key_t shmKey = ftok(pathKeyFtok, 'z');
    if (shmKey == -1)
        errExit("ftok for shmKey failed");

    // allocate a shared memory segment
    int shmidServer = alloc_shared_memory(shmKey, sizeof(struct SHMKeyData) * MAX_REQUEST_INTO_MEMORY);

    // attach the shared memory segment
    struct SHMKeyData *shmPointer = (struct SHMKeyData*)get_shared_memory(shmidServer, 0);

    // create a semaphore set
    key_t semKey = ftok(pathKeyFtok, 'g');
    if (semKey == -1)
        errExit("ftok for semKey failed");

    // get the semaphore set
    int semid = semget(semKey, 1, S_IRUSR | S_IWUSR);
    if(semid < 0)
        errExit("semget failed");

    printf("Controllo la chiave..\n");
    fflush(stdout);

    //retrieve data
    semOp(semid, MUTEX, -1);
    for (int i = 0; i < MAX_REQUEST_INTO_MEMORY; i++) {
        //printf(" U:%s K:%d T:%ld \n", shmPointer[i].userIdentifier, shmPointer[i].key, shmPointer[i].timeStamp);
        if( strcmp(shmPointer[i].userIdentifier, userIdentifier) == 0)
            if(shmPointer[i].key == key && shmPointer[i].key != -1)
            {
                //execute the program
                keyIsValid = 1;
                //set the key to a negative value , so it become invalid
                shmPointer[i].key = -1;
            }
    }
    semOp(semid, MUTEX, 1);

    //try to execute program
    if(!keyIsValid){
        printf("La chiave non era valida o è scaduta\n");
        exit(1);
    }

    //key is valid, try to execute program
    // take the two last value of key -
    key = key % 100;
    char programName[] = "stampa";
    if(key == 11)
        strcpy(programName, "invia");
    else if(key == 22)
        strcpy(programName, "salva");

    execv(programName,argv);
    perror("failed to execute program");


}
