#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "../inc/errExit.h"
#include <errno.h>
#include <sys/stat.h>
int main (int argc, char *argv[]) {
    printf("Hi, I'm Salva program! Let's save...\n");
    
   
   	//cambio directori di lavoro
   	char oldDir[255] = {0};
   	getcwd(oldDir, sizeof(oldDir));
   	
   	printf("%s\n", oldDir);
   	char *newDir = "/home/francesco/uni/so/elaborato/sistemi_operativi/system-call/clientReq-server";
   	chdir(newDir);  
   	
   	/* Open new or existing file for reading/writing, truncating
    to zero bytes; file permissions read+write only for owner*/
   
   //check if a file is in directory
   struct stat path_stat;
   stat(argv[3], &path_stat);
   printf("%d", S_ISREG(path_stat.st_mode));
   
   
   
    int fd = open(argv[3], O_RDWR | O_CREAT  | O_TRUNC, S_IRUSR | S_IWUSR);  
    if (fd == -1)
        errExit("error creating file");

    for(int i = 4; i < argc; i++){
        char buffer[50] = {0};
        sprintf(buffer, "%s ", argv[i]);
        ssize_t numWrite = write(fd, buffer, strlen(buffer));
        if (numWrite != strlen(buffer))
            errExit("failed to write to file");
    }


    return 0;
}
