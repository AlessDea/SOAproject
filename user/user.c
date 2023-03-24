#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PUT_DATA        134
#define GET_DATA	   	156
#define INVALIDATE_DATA 174

int main(void){
    int cmd, ret;
    char *msg;


    ssize_t size;
    int offset;


    printf("0: put_data\n1: get_data\n2: invalidate_data\n");
    printf("Insert a command: ");
    scanf("%d", &cmd);

    switch(cmd){
        case 0:
            msg = (char *)malloc(sizeof(char)*4096);
            if(msg == NULL){
                printf("malloc error\n");
                return -1;
            }


            getc(stdin); //consume newline

            fgets(msg, 4096, stdin);
            if ((strlen(msg) > 0) && (msg[strlen (msg) - 1] == '\n'))
                msg[strlen (msg) - 1] = '\0';

            printf("got msg %s\n", msg);
            size = strlen(msg);

            printf("calling syscall PUT\n");
            ret = syscall(PUT_DATA, msg, size);
            if(ret < 0) {
                printf("syscall error\n");
                return -1;
            }

            printf("message has been stored at offset %d\n", ret);
            break;

        case 1:
            break;
        case 2:
            break;
    }
}



