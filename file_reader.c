#include <stdio.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[])
{
    FILE *fp;
    char buffer[BUF_SIZE];
    char *r;

    fp = fopen("template.html", "r");
    if (fp == NULL)
    {
        printf("Unable to open file.\n");
        return -1;
    }

    for(;;)
    {
        r = fgets(buffer, BUF_SIZE, fp);
        if (r != NULL)
            printf("%s", buffer);
        else
            break;
    }

    return 0;
}

