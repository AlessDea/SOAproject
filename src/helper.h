//
// Created by alessandrodea on 02/03/23.
//


#ifndef SOAPROJECT_HELPER_H
#define SOAPROJECT_HELPER_H


#include <linux/fs.h>
#include <linux/types.h>
#include <linux/srcu.h>


/* User Messages Driver */
//----------------------------------------------------------------
#ifndef NBLOCKS
#define NBLOCKS 10
#endif

#define BLOCK_SSIZE 4096 /* size in bytes of each block */
#define METADATA_SIZE (sizeof(short) + sizeof(long)) /* metadata size (4096 - X) */
#define MSG_MAX_SIZE BLOCK_SSIZE - METADATA_SIZE /* (X) block reserved space for message */

#define DEVICE_SIZE (NBLOCKS * BLOCK_SSIZE)
#define BLK_INDX(off) off/MSG_MAX_SIZE /* from an offset return the index of the block. Needed if the offset is expressed not as multiple of block size */
#define IN_BLOCK_OFF(off) off%MSG_MAX_SIZE /* starting off inside the message */

#define GET_BLK_DATA(b) (b + METADATA_SIZE)

#define CLEAN_META 0x0000
#define VALID_MASK 0x8000
#define CHCECK_V_MASK 0xffff

#define MSG_LEN(md) (md & ~VALID_MASK)

#define INVALIDATE(md) (md & CLEAN_META)

#define IS_VALID(md) (md | ~VALID_MASK)


/* the struct of each block */
struct block{
    short metadata;
    long next; //next block according to the order of the delivery of data
    char data[MSG_MAX_SIZE];

    //char padding[(4 * 1024) - sizeof(long) - sizeof(short) - (sizeof(char)*MSG_MAX_SIZE)];
};



/* RCU */
//----------------------------------------------------------------
#define  AUDIT if(1) //this is a general audit flag


//this defines the RCU house keeping period
#ifndef PERIOD
#define PERIOD 120
#endif


#define EPOCHS (2) //we have the current and the past epoch only


#define MASK 0x8000000000000000

typedef struct insert_ret{
    long curr;
    long prev;
} insert_ret;

typedef struct invalidate_ret{
    long next;
    long prev;
} invalidate_ret;

typedef struct rcu_lst_elem{
    struct rcu_lst_elem * next;
    long key; //offset
    short validity;
} element;

// #######################################################
typedef struct device_map{
    long keys[NBLOCKS]; //used to mantain block validity (1)
    long num_of_valid_blocks;
    long first;
    long last; //last valid written block
} __attribute__((packed)) device_map;

typedef device_map map __attribute__((aligned(64)));

extern map dev_map; /* map of the device */
// #######################################################





/* ------------------------------------------------------------- */


#include <linux/types.h>
#include <linux/fs.h>


#define MOD_NAME "SINGLE FILE FS"

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

    //padding to fit into a single block
    char padding[(4 * 1024) - (3 * sizeof(uint64_t)) - (2 * sizeof(long))];
};



struct bdev_status {
    unsigned int usage;
    struct block_device *bdev;
    struct srcu_struct rcu;
};


extern struct mutex f_mutex;

// file.c
extern const struct inode_operations onefilefs_inode_ops;
extern const struct file_operations onefilefs_file_operations;

// dir.c
extern const struct file_operations onefilefs_dir_operations;

extern struct super_block *my_bdev_sb; // superblock ref to be used in the systemcalls

extern struct bdev_status dev_status;
{
    /* data */
};


#endif //SOAPROJECT_HELPER_H
