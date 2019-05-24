#include <stdlib.h>
#include <stdio.h>

int main (int argc, char *argv[]) {
    printf("\n%s \n", argv[0]);

    for(int i = 1; i < argc; i++)
        printf("%s \n",argv[i]);

    return 0;
}
