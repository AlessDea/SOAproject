//
// Created by alessandrodea on 04/03/23.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>



/* IOCTL commands */
#define IOCTL_PUT_DATA 0
#define IOCTL_GET_DATA 1
#define IOCTL_INVALIDATE_DATA 2


struct _ioctl_put_data_args{
    char *src;
    size_t size;
}put_args;


#define DEV_NAME "/dev/msgsbdev"

int main(int argc, char argv[]){

    int fd, back_errno, i;
    char w = 'a';

    fd = open(DEV_NAME, O_RDWR);
    if (fd < 0) {
        back_errno = errno;
        perror("open");
        fprintf(stderr, "errno is %d\n", back_errno);
        exit(EXIT_FAILURE);
    }
    /*
    sleep(5);

    for(int i = 0; i < sizeof(msg); i++){
        msg[i] = 'a';
    }
    write(fd, msg, sizeof(msg));

    sleep(5);

    read(fd, tmp_msg, sizeof(msg));
    if(memcmp(msg, tmp_msg, sizeof(msg)) == 0)
        printf("ok\n");

    sleep(5);
*/

    printf("preparing the message\n");

    put_args.src = (char *)malloc(sizeof(char)*1024);

    if(argc > 1) {
        w = 'b';
    }
    for (i = 0; i < 1023; i++) {
        put_args.src[i] = w;
    }
    put_args.src[1023]  '\0';
    put_args.size = 1024;

    /*
    memcpy(put_args.src, "porcodio", 8);
    put_args.size = 512;
*/
    printf("%s\n", put_args.src);
    printf("calling ioctl\n");

    ioctl(fd, IOCTL_PUT_DATA, (void *) &put_args);

    //lseek(fd, sector * SECTOR_SIZE, SEEK_SET);





}