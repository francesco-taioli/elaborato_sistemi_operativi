#include <stdlib.h>
#include <stdio.h>
#include <zconf.h>
#include <fcntl.h>

int main (int argc, char *argv[]) {
    printf("Hi, I'm Stampa program! Let's print...\n");
    
    close(1);
    int fd = open("stampaSuFIle.txt", O_WRONLY | O_CREAT);
    
    for(int i = 3; i < argc; i++)
        printf("%s ",argv[i]);

    printf("\n");
    return 0;
}
