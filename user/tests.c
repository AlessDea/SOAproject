//
// Created by alessandrodea on 27/03/23.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct block{
    short metadata;
    char data[2048];
};

int main(void){

    char *msg = malloc(sizeof (char)*21);
    char *tmp = msg;

    char *str = "aaaabbbbccccddddeeee";

    for(int i = 0; i < 5; i++){
        //printf("%p\n", tmp);
        memcpy(tmp, str, 4);
        tmp += 4*sizeof(char);
        str += 4*sizeof(char);
    }
    printf("full msg: %s\n", msg);
    tmp[21] = '\0';
    printf("full msg: %s\n", msg);

    // for(int i = 0; i < 21; i++){
    //     printf("%d\n", msg[i]);
    // }

}