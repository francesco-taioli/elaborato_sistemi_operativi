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
        char buffer[50];
        sprintf(buffer, "%s%s", argv[i], " ");
        ssize_t numWrite = write(fd, buffer, strlen(buffer));
        if (numWrite != strlen(buffer))
            errExit("failed to write to file");
    }


    return 0;
}
