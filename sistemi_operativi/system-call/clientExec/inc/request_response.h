#ifndef ELABORATO_REQUEST_RESPONDE_HH
#define ELABORATO_REQUEST_RESPONDE_HH

#include <sys/types.h>
struct Request {   /* Request (client --> server) */
    pid_t clientPid;
    char serviceName[7];      /* Stampa, Salva o Invia*/
    char userIdentifier[25];
};

struct Response {  /* Response (server --> client) */
    int key;
};

// the data that will fit into shared memory
struct SHMKeyData {
    char userIdentifier[25];
    int key;
    time_t timeStamp;
};

#endif
