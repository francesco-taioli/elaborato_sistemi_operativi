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


#define MAX_REQUEST_INTO_MEMORY 15
#define MUTEX 0

char *pathKeyFtok = "/tmp/vr422009.tmp";

int main (int argc, char *argv[]) {
    char *userName = "FRANCESCO";
    char *key = "provaKey";
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

    if (semid >= 0) { //todo check >=
        printf("Controllo la chiave..\n");
        fflush(stdout);

        //retrieve data
        semOp(semid, MUTEX, -1);
        struct SHMKeyData tmp;
        for (int i = 0; i < MAX_REQUEST_INTO_MEMORY; i++) {
            memcpy(&tmp, shmPointer + i, sizeof(struct SHMKeyData));    //increase pointer to access the next struct

            printf("U->%s t->%ld key->%s\n", tmp.userIdentifier, tmp.timeStamp, tmp.key);

            if( strcmp(tmp.userIdentifier, userName) == 0)
               if(strcmp(tmp.key, key) == 0)
               {
                   //execute the program and set the key to negative value
                   keyIsValid = 1;

                   //set the key to a negative value , so it become invalid
                   strcpy(tmp.key, "0");
                   tmp.timeStamp = 0;
                   memcpy(shmPointer + i, &tmp, sizeof(struct SHMKeyData));

               }
        }
        semOp(semid, MUTEX, 1);

    }
    else {
        printf("semget failed\n");//todo handle
    }

    //try to execute program
    if(!keyIsValid){
        printf("La chiave non era valida o è scaduta\n");
        exit(-1); //todo check
    }


    //key is validd, try to execute program

    //todo execute program

//    char buffer[100];
//    printf("Inserisci [nome programma][parametri]:");
//    scanf("%99[^\n]", buffer); //Any number of characters none of them specified as characters between the brackets. = negated scanset
//
//    int i = 0;
//    char *p = strtok (buffer, " ");
//    char *array[100];
//    while (p != NULL)
//    {
//        array[i++] = p;
//        p = strtok (NULL, " ");
//    }
//    array[i] = NULL;
//
//
//    printf("Eseguo %s ...\n", array[0] );
//    execv(array[0] ,array);

    char *argvToPass[]={"stampa","Foois", "prova","myname.",NULL};
    execv("salva",argvToPass);
    perror("Execl");

}
