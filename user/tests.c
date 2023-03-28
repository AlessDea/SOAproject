//
// Created by alessandrodea on 27/03/23.
//

#include <stdio.h>

struct block{
    short metadata;
    char data[2048];
};

int main(void){

    printf("size o the struct: %ld\n", sizeof(struct block));

}