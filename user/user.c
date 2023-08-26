#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define PUT_DATA        134
#define GET_DATA	   	174
#define INVALIDATE_DATA 177

int main(void){
    int cmd, ret, fd;
    char *msg;


    ssize_t size;
    int offset;


    printf("0: put_data\n1: get_data\n2: invalidate_data\n3: read\n4: exit\n");

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
                    break;
                }

                printf("message has been stored at offset %d\n", ret);
                break;

            case 1:
                msg = (char *) malloc(sizeof(char) * 4096);
                if (msg == NULL) {
                    printf("malloc error\n");
                    return -1;
                }

                bzero(msg, sizeof(char) * 4096);

                size = 2048;

                printf("Insert the offset: ");
                scanf("%d", &offset);

                ret = syscall(GET_DATA, offset, msg, size);
                if (ret < 0) {
                    printf("syscall error\n");
                    break;
                }

                printf("msg: %s\n", msg);
                break;

            case 2:
                printf("Insert the offset: ");
                scanf("%d", &offset);

                ret = syscall(INVALIDATE_DATA, offset);
                if (ret < 0) {
                    printf("syscall error\n");
                    break;
                }
                break;
            case 3:

                printf("Insert the len of message to read: ");
                scanf("%ld", &size);

                fd = open("../src/mount/user-msgs", O_RDONLY);
                if(fd == -1)
                {
                    printf("open error\n");
                    break;
                }

                msg = (char *) malloc(sizeof(char) * (size + 1));
                if (msg == NULL) {
                    printf("malloc error\n");
                    return -1;
                }

                ret = read(fd, msg, size);
                printf("read %d bytes, the message is: %s\n", ret, msg);

                close(fd);

                break;
            case 4:
                return 0;
        }
    }
}



