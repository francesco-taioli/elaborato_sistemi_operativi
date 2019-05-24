#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "../inc/errExit.h"

int main (int argc, char *argv[]) {
    /* Open new or existing file for reading/writing, truncating
    to zero bytes; file permissions read+write only for owner*/
    int fd = open("vr422009.lineArgument",
                  O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1)
        errExit("error creating file");

    for(int i = 0; i < argc; i++){
        //todo
        ssize_t numWrite = write(fd, argv[i], strlen(argv[i]));
        if (numWrite != strlen(argv[i]))
            errExit("failed to write to file");
    }


    return 0;
}
