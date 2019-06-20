#include <stdlib.h>
#include <stdio.h>

int main (int argc, char *argv[]) {
    printf("Hi, I'm Stampa program! Let's print...\n");
    for(int i = 3; i < argc; i++)
        printf("%s ",argv[i]);

    printf("\n");
    return 0;
}
