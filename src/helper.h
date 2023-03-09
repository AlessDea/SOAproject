//
// Created by alessandrodea on 02/03/23.
//

#ifndef SOAPROJECT_HELPER_H
#define SOAPROJECT_HELPER_H


#define MODNAME "MSG MANAGER"

/* default number of blocks in the block device.
 * Actually it is the number of messages that can be stored
 * */
#ifndef NBLOCKS
#define NBLOCKS 10
#endif

#define BLOCK_SSIZE 4096 /* size in bytes of each block */
#define MSG_MAX_SIZE 2048 /* (X) block reserved space for message */
#define METADATA_SIZE (BLOCK_SSIZE - MSG_MAX_SIZE) /* metadata size (4096 - X) */

#define DEVICE_SIZE (NBLOCKS * BLOCK_SSIZE)
#define MY_SECTOR_SIZE 512
#define NR_SECTORS (DEVICE_SIZE/SECTOR_SIZE)
#define BLK_INDX(off) off/BLOCK_SSIZE /* from an offset return the index of the bloc. Needed if the offset is expressed not as multiple of block size */

/* IOCTL commands */
#define IOCTL_PUT_DATA 0
#define IOCTL_GET_DATA 1
#define IOCTL_INVALIDATE_DATA 2


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
}map[NBLOCKS];


/* description of my device */
static struct my_block_dev {

    spinlock_t lock;                /* For mutual exclusion */
    struct request_queue *queue;    /* The device request queue */
    struct gendisk *gd;             /* The gendisk structure */
    struct blk_mq_tag_set tag_set;  /* Queue properties, including the number of hardware queues, their capacity and request handling function */
    u8 *data;
    struct block blk;
    size_t size;

} dev;


struct _ioctl_put_data_args{
    char *src;
    size_t size;
};


struct _ioctl_get_data_args{
    int offset;
    char *dst;
    size_t size;
};


struct _ioctl_invalidate_args{
    int offset;
};



#endif //SOAPROJECT_HELPER_H
