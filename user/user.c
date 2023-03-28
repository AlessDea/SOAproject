#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PUT_DATA        134
#define GET_DATA	   	174
#define INVALIDATE_DATA 177

int main(void){
    int cmd, ret;
    char *msg;


    ssize_t size;
    int offset;


    printf("0: put_data\n1: get_data\n2: invalidate_data\n3: exit\n");

    while(1) {
        printf("Insert a command: ");
        scanf("%d", &cmd);

        switch (cmd) {
            case 0:
                msg = (char *) malloc(sizeof(char) * 4096);
                if (msg == NULL) {
                    printf("malloc error\n");
                    return -1;
                }


                getc(stdin); //consume newline

                printf("Insert the message: ");
                fgets(msg, 4096, stdin);
                if ((strlen(msg) > 0) && (msg[strlen(msg) - 1] == '\n'))
                    msg[strlen(msg) - 1] = '\0';

                size = strlen(msg);
                printf("got msg %s (%ld)\n", msg, size);

                printf("calling syscall PUT\n");
                ret = syscall(PUT_DATA, msg, size);
                if (ret < 0) {
                    printf("syscall error\n");
                    return -1;
                }

                printf("message has been stored at offset %d\n", ret);
                break;

            case 1:
                msg = (char *) malloc(sizeof(char) * 4096);
                if (msg == NULL) {
                    printf("malloc error\n");
                    return -1;
                }

                size = 2048;

                printf("Insert the offset: ");
                scanf("%d", &offset);

                ret = syscall(GET_DATA, offset, msg, size);
                if (ret < 0) {
                    printf("syscall error\n");
                    return -1;
                }

                printf("msg: %s\n", msg);
                break;

            case 2:
                scanf("%d", &offset);
                ret = syscall(INVALIDATE_DATA, offset);
                if (ret < 0) {
                    printf("syscall error\n");
                    return -1;
                }
                break;

            case 3:
                return 0;
        }
    }
}



