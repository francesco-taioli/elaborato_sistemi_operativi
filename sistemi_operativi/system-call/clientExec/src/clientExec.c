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


#define MAX_REQUEST_INTO_MEMORY 5
#define MUTEX 0

int main (int argc, char *argv[]) {
    char *userName = "aasfa";
    int key = 12349;
    int keyIsValid = 0; //1 = true

    // access to shared memory
    key_t shmKey = 6327;// todo implement fotk

    // allocate a shared memory segment
    int shmidServer = alloc_shared_memory(shmKey, sizeof(struct SHMKeyData) * MAX_REQUEST_INTO_MEMORY);

    // attach the shared memory segment
    struct SHMKeyData *shmPointer = (struct SHMKeyData*)get_shared_memory(shmidServer, 0);


    // get the semaphore set
    key_t semKey = 6347; //todo implement in some way
    int semid = semget(semKey, 1, S_IRUSR | S_IWUSR);

    if (semid > 0) {

        //retrieve data
        semOp(semid, MUTEX, -1);
        struct SHMKeyData tmp;
        for (int i = 0; i < MAX_REQUEST_INTO_MEMORY; i++) {


            memcpy(&tmp, shmPointer + i, sizeof(struct SHMKeyData));    //increase pointer to access the next struct

            printf("U->%s t->%d key->%d\n", tmp.userIdentifier, tmp.timeStamp, tmp.key);


            if( strcmp(tmp.userIdentifier, userName) == 0)
               if(tmp.key == key)
               {
                   //execute the program and set the key to negative value
                   keyIsValid = 1;

                   //set the key to negativo, so it become invalid
                   tmp.key = -1;
                   memcpy(shmPointer + i, &tmp, sizeof(struct SHMKeyData));

               }
        }
        semOp(semid, MUTEX, 1);

    }
    else {
        printf("semget failed");
    }

    //try to execute program
    if(!keyIsValid){
        printf("La chiave non era valida o è scaduta\n");
        exit(-1); //todo check
    }

    printf("Chiave valida\n");
    //todo execute program

}
