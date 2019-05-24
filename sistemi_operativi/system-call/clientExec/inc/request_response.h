#ifndef ELABORATO_REQUEST_RESPONDE_HH
#define ELABORATO_REQUEST_RESPONDE_HH

#include <sys/types.h>
struct Request {   /* Request (client --> server) */
    pid_t clientPid;
    char serviceName[10];      /* Stampa, Salva o Invia*/
    char userIdentifier[25];
};

struct Response {  /* Response (server --> client) */
    char key[25];
};

// the data that will fit into shared memory
struct SHMKeyData {
    char userIdentifier[25];
    char key[25];
    time_t timeStamp;
};

#endif
