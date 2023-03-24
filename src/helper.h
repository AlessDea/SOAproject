//
// Created by alessandrodea on 02/03/23.
//


#ifndef SOAPROJECT_HELPER_H
#define SOAPROJECT_HELPER_H


#include <linux/fs.h>
#include <linux/types.h>


/* User Messages Driver */
//----------------------------------------------------------------
#ifndef NBLOCKS
#define NBLOCKS 10
#endif

#define BLOCK_SSIZE 4096 /* size in bytes of each block */
#define METADATA_SIZE sizeof(short) /* metadata size (4096 - X) */
#define MSG_MAX_SIZE BLOCK_SSIZE - METADATA_SIZE /* (X) block reserved space for message */

#define DEVICE_SIZE (NBLOCKS * BLOCK_SSIZE)
#define BLK_INDX(off) off/BLOCK_SSIZE /* from an offset return the index of the bloc. Needed if the offset is expressed not as multiple of block size */

#define GET_BLK_DATA(b) (b + METADATA_SIZE)

#define CLEAN_META 0x0000
#define VALID_MASK 0x8000

#define MSG_LEN(md) (md & ~VALID_MASK)

#define INVALIDATE(md) (md & CLEAN_META)


/* the struct of each block */
struct block{
    short metadata;
    char data[MSG_MAX_SIZE];
};



/* RCU */
//----------------------------------------------------------------
#define  AUDIT if(1) //this is a general audit flag


//this defines the RCU house keeping period
#ifndef PERIOD
#define PERIOD 1
#endif


#define EPOCHS (2) //we have the current and the past epoch only


#define MASK 0x8000000000000000


typedef struct rcu_lst_elem{
    struct rcu_lst_elem * next;
    long key; //offset
    short validity;
} element;

typedef struct rcu_list{
    unsigned long standing[EPOCHS];	//you can further optimize putting these values
    //on different cache lines
    unsigned long epoch; //a different cache line for this can also help
    int next_epoch_index;
    rwlock_t write_lock;
    int keys[NBLOCKS];
    element * head;
} __attribute__((packed)) rcu_list;

typedef rcu_list list __attribute__((aligned(64)));

extern list dev_map; /* map of the device */


#define list_insert rcu_list_insert
#define list_is_valid rcu_list_is_valid
#define list_remove rcu_list_remove
#define list_init rcu_list_init
#define list_first_free rcu_list_first_free
#define list_next_valid rcu_list_next_valid

void rcu_list_init(rcu_list * l);

int rcu_list_is_valid(rcu_list *l, long key);

int rcu_list_insert(rcu_list *l, long key);

int rcu_list_remove(rcu_list *l, long key);

int rcu_list_next_valid(rcu_list *l, long start_key);

int rcu_list_first_free(rcu_list *l);


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
    uint64_t inodes_count;//not exploited
    uint64_t free_blocks;//not exploited

    //padding to fit into a single block
    char padding[ (4 * 1024) - (5 * sizeof(uint64_t))];
};

// file.c
extern const struct inode_operations onefilefs_inode_ops;
extern const struct file_operations onefilefs_file_operations;

// dir.c
extern const struct file_operations onefilefs_dir_operations;

extern struct super_block *my_bdev_sb; // superblock ref to be used in the systemcalls


#endif //SOAPROJECT_HELPER_H
