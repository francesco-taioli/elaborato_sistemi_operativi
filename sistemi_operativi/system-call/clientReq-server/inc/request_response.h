#ifndef ELABORATO_REQUEST_RESPONDE_H
#define ELABORATO_REQUEST_RESPONDE_H

#include <sys/types.h>
struct Request {   /* Request (client --> server) */
    pid_t clientPid;
    char *serviceName;      /* Stampa, Salva o Invia*/
};

struct Response {  /* Response (server --> client) */
    int key;
};

#endif
