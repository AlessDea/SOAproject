//
// Created by alessandrodea on 02/03/23.
//


#ifndef SOAPROJECT_HELPER_H
#define SOAPROJECT_HELPER_H


#include <linux/fs.h>
#include <linux/types.h>
#include <linux/srcu.h>
#include <linux/buffer_head.h>



/* User Messages Driver */
#define MAX_NBLOCKS 1000

#ifndef NBLOCKS
#define NBLOCKS 10
#endif

#ifndef SYNCHRONOUS_W
#define SYNCHRONOUS_W
#endif



#define BLOCK_SSIZE 4096 /* size in bytes of each block */

#define DEVICE_SIZE (NBLOCKS * BLOCK_SSIZE)
#define BLK_INDX(off) (off/MSG_MAX_SIZE) 
#define IN_BLOCK_OFF(off) off%MSG_MAX_SIZE

#define GET_BLK_DATA(b) (b + METADATA_SIZE)

#define CLEAN_META 0x0000
#define VALID_MASK 0x8000
#define CHCECK_V_MASK 0xffff

#define MSG_LEN(md) (md & ~VALID_MASK)

#define INVALIDATE(md) (md & CLEAN_META)

#define IS_VALID(md) (md | ~VALID_MASK)


/* the struct of each block */
struct block{
    short metadata; //(valid + len)
    long next; //next block according to the order of the delivery of data
    char data[BLOCK_SSIZE - (sizeof(short) + sizeof(long))];

} __attribute__((packed, aligned(64)));




#define  AUDIT if(1) //this is a general audit flag


typedef struct device_map{
    long keys[NBLOCKS]; //used to mantain block validity (1)
    long num_of_valid_blocks;
    long first;
    long last; //last valid written block
    long size;
} __attribute__((packed, aligned(64))) map;

extern map dev_map; /* map of the device */


/* ------------------------------------------------------------- */

#define MOD_NAME "USER_MSG"

#define MAGIC 0x42424242
#define DEFAULT_BLOCK_SIZE 4096
#define SB_BLOCK_NUMBER 0
#define DEFAULT_FILE_INODE_BLOCK 1

#define FILENAME_MAXLEN 255

#define SINGLEFILEFS_ROOT_INODE_NUMBER 10
#define SINGLEFILEFS_FILE_INODE_NUMBER 1

#define SINGLEFILEFS_INODES_BLOCK_NUMBER 1

#define UNIQUE_FILE_NAME "user-msgs"


//inode definition
struct onefilefs_inode {
    mode_t mode;//not exploited
    uint64_t inode_no;
    uint64_t data_block_number;//not exploited

    union {
        uint64_t file_size;
        uint64_t dir_children_count;
    };
};

//dir definition (how the dir datablock is organized)
struct onefilefs_dir_record {
    char filename[FILENAME_MAXLEN];
    uint64_t inode_no;
};


//superblock definition
struct onefilefs_sb_info {
    uint64_t version;
    uint64_t magic;
    uint64_t block_size;
    long first_key;
    long last_key;
    long f_size;

    //padding to fit into a single block
    char padding[(4 * 1024) - (3 * sizeof(uint64_t)) - (3 * sizeof(long))];
};



struct bdev_status {
    unsigned int usage;
    struct block_device *bdev;
    struct srcu_struct rcu;
};

extern struct mutex f_mutex; // for writers synchronization



// file.c
extern const struct inode_operations onefilefs_inode_ops;
extern const struct file_operations onefilefs_file_operations;

// dir.c
extern const struct file_operations onefilefs_dir_operations;


extern struct super_block *my_bdev_sb; // superblock ref to be used in the systemcalls

extern struct bdev_status dev_status;


int reload_device_map(map *m);
long get_next_free_block(map *m);
int device_is_empty(map *m);
long is_block_valid(map *m, long idx);
long get_first_valid_block(map *m);
long get_next_valid_block(map *m, long idx);
long set_invalid_block(map *m, long idx);
long get_higher_valid_blk_indx(map *m);


#endif //SOAPROJECT_HELPER_H
