//
// Created by alessandrodea on 02/03/23.
//

#ifndef SOAPROJECT_HELPER_H
#define SOAPROJECT_HELPER_H


#define MODNAME "MSG MANAGER"


#ifndef NBLOCKS
#define NBLOCKS 10
#endif

#define BLOCK_SSIZE 4096 /* size in bytes of each block */
#define MSG_MAX_SIZE 2048 /* (X) block reserved space for message */
#define METADATA_SIZE (BLOCK_SSIZE - MSG_MAX_SIZE) /* metadata size (4096 - X) */

#define DEVICE_SIZE (NBLOCKS * BLOCK_SSIZE)
#define BLK_INDX(off) off/BLOCK_SSIZE /* from an offset return the index of the bloc. Needed if the offset is expressed not as multiple of block size */

#define GET_BLK_DATA(b) (b + METADATA_SIZE)


#define MY_BLOCK_MAJOR           0 /* with 0 the Major is chosen by the kernel */
#define MY_BLKDEV_NAME          "msgsbdev"

#define MY_BLOCK_MINORS       1


static int major;


/* the struct of each block */
struct block{
    u8 metadata[METADATA_SIZE];
    u8 data[MSG_MAX_SIZE];
};


/* a map that represent the status of each block of the device */
static struct device_map{
    int validity; //1: holds valid data; 0: invalid data, reuse it!
    size_t dirty_len;
    int prev_off;
}map[NBLOCKS];


struct my_device_data{
    struct cdev cdev;
    int valid_blks;
    char data[DEVICE_SIZE];
    size_t size;
};

#endif //SOAPROJECT_HELPER_H
