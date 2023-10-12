/*
* 
* This is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
* 
* This module is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* @file tasklet.c 
* @brief This is the main source for the Linux Kernel Module which implements
*       a system call that can be used to ask the SoftIRQ daemon to execute a tasklet 
*
* @author Francesco Quaglia
*
* @date November 13, 2021
*/

#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/apic.h>
#include <asm/io.h>
#include <linux/syscalls.h>
#include <linux/buffer_head.h>

#include "include/scth.h"
#include "../src/helper.h"

int update_file_size(int size){
    struct inode *the_inode = NULL;
    struct super_block *sb = my_bdev_sb;
    struct buffer_head *bh = NULL;
    struct onefilefs_inode *FS_specific_inode;



    the_inode = iget_locked(sb, 1);
    if (!the_inode)
        return -ENOMEM;

    //already cached inode - modify its size
    if(!(the_inode->i_state & I_NEW)){
        the_inode->i_size += size;
    }

    // now modify the size also on the specific inode stored in the second block of the device (for consistency)

    bh = (struct buffer_head *)sb_bread(sb, SINGLEFILEFS_INODES_BLOCK_NUMBER );
    if(!bh){
        iput(the_inode);
        return -EIO;
    }
    FS_specific_inode = (struct onefilefs_inode*)bh->b_data;
    FS_specific_inode->file_size += size;

    mark_buffer_dirty(bh);
    brelse(bh);

    return 0;
}

#define AUDIT if(1)


/* here declare the three new syscall */
#define SYS_CALL_INSTALL

#ifdef SYS_CALL_INSTALL
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
    __SYSCALL_DEFINEx(2, _put_data, char *, source, size_t, size){
#else
    asmlinkage int sys_put_data(char * source, size_t size){
#endif

    struct block *blk, *blk1;
    struct buffer_head *bh;
    char* addr;
    int ret, ret1;
    long key;

    printk(KERN_INFO "%s: thread %d requests a put_data sys_call\n",MOD_NAME,current->pid);

    /* the operation must be done all or nothing so if there is no enough space returns ENOMEM.
     * Two things must be checked:
     * - the user message fits in the block (size <= MSG_MAX_SIZE)
     * - there is a free block
     * */


    // increment usage count
    atomic_fetch_add(1, &(dev_status.usage)); 

    // check if device is mounted
    if(dev_status.bdev == NULL){
        atomic_fetch_sub(1, &(dev_status.usage));
        return -ENODEV;
    }


    /* check if size is less-equal of the block size */
    // if((size + 1) > MSG_MAX_SIZE)  //+1 for the null terminator
    if((size) >= MSG_MAX_SIZE){  // = doesn't permit to store the '/0' at the end
        atomic_fetch_sub(1, &(dev_status.usage));
        return -ENOMEM; 
    }

    if(size <= 0){
        atomic_fetch_sub(1, &(dev_status.usage));
        return -ENOMEM; 
    }


    /* check if there is a free block available and if there is then 'book' it. */
    // off = list_first_free(&dev_map);
    // if(off < 0){
    //     printk(KERN_INFO "%s: thread %d request for put_data sys_call: no free blocks available", MOD_NAME, current->pid);
    //     return -ENOMEM;
    // }


    ret = mutex_lock(&f_mutex);
    if (ret != 0) {
        printk(KERN_CRIT "%s: mutex_lock ERROR\n", MODNAME);
        atomic_fetch_sub(1, &(dev_status.usage));
        return -EBUSY;
    }

    key = get_next_free_block(&dev_map);
    if(key < 0){
        printk(KERN_INFO "%s: thread %d request for put_data sys_call error\n",MOD_NAME,current->pid);
        atomic_fetch_sub(1, &(dev_status.usage));
        mutex_unlock(&f_mutex);
        return -ENOMEM;
    }


    if (dev_map.last != -1){

        // update the previouse block's next field with this one
        bh = (struct buffer_head *)sb_bread(my_bdev_sb, dev_map.last + 2); // +2 for the superblock and inode blocks
        if(!bh){
            atomic_fetch_sub(1, &(dev_status.usage));
            mutex_unlock(&f_mutex);
            return -EIO;
        }

        blk = (struct block*)bh->b_data;
        blk->next = key;

        //printk(KERN_INFO "%s: prev's (%ld) next %ld\n",MOD_NAME,ret.prev,blk->next);

        mark_buffer_dirty(bh);
        brelse(bh);

        //blk = NULL;

    }

    // update last key and first key in the device map
    dev_map.last = key;
    if(dev_map.first == -1)
        dev_map.first = key;


    addr = kzalloc(sizeof(char)*MSG_MAX_SIZE, GFP_KERNEL);
     if(!addr){
        printk(KERN_INFO "%s: kzalloc error\n",MOD_NAME);
        atomic_fetch_sub(1, &(dev_status.usage));
        mutex_unlock(&f_mutex);
        return -EIO;
    }

    /* perform the write in bh */
    blk1 = kzalloc(sizeof(struct block *), GFP_KERNEL);
    if(!blk1){
        printk(KERN_INFO "%s: kzalloc error\n",MOD_NAME);
        atomic_fetch_sub(1, &(dev_status.usage));
        mutex_unlock(&f_mutex);
        return -EIO;
    }

    ret1 = copy_from_user((char*)addr,(char*)source, size); // returns the number of bytes NOT copied
    addr[size] = '\0';

    blk1->metadata = VALID_MASK ^ (size);
    blk1->next = -1; // the next of the last inserted block is always null
    memcpy(blk1->data, addr, size+1);


    synchronize_srcu(&(dev_status.rcu));


    // get the buffer_head
    bh = (struct buffer_head *)sb_bread(my_bdev_sb, key + 2); // +2 for the superblock and inode blocks
    if(!bh){
        atomic_fetch_sub(1, &(dev_status.usage));
        mutex_unlock(&f_mutex);
        return -EIO;
    }

    memcpy(bh->b_data, (char *)blk1, sizeof(struct block));

    mark_buffer_dirty(bh);
    brelse(bh);

    
    //+1 for the null terminator which become a \n in the read operation
    //when a read is performed, with cat for example, the buffer has len as the file, so we need
    //space for the \n for each message
    update_file_size(size);
    printk(KERN_INFO "%s: thread %d request for put_data sys_call success\n",MOD_NAME,current->pid);

    // kfree(blk);
    // kfree(blk1);
    kfree(addr);
    atomic_fetch_sub(1, &(dev_status.usage));
    mutex_unlock(&f_mutex);
    return key;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
    __SYSCALL_DEFINEx(3, _get_data, long, offset, char *, destination, size_t, size){
#else
    asmlinkage int sys_get_data(long offset, char * destination, size_t size){
#endif

    short len;
    struct buffer_head *bh;
    struct block *blk;
    char *addr;
    int ret;
    short to_cpy;
    int srcu_idx;


    //printk(KERN_INFO "%s: thread %d requests a get_data sys_call\n",MOD_NAME,current->pid);

    /* check if offset exists */
    //if(BLK_INDX(offset) > NBLOCKS-1)
    //    return -ENODATA;

    //off = BLK_INDX(offset);

    // increment usage count
    atomic_fetch_add(1, &(dev_status.usage)); 

    // check if device is mounted
    if(dev_status.bdev == NULL){
        atomic_fetch_sub(1, &(dev_status.usage));
        return -ENODEV;
    }

    rcu_index = srcu_read_lock(&(dev_status.rcu));

    if(is_block_valid(&dev_map, offset) == 0){
        printk(KERN_INFO "%s: thread %d request for get_data sys_call error: the block is not available\n", MOD_NAME, current->pid);
        atomic_fetch_sub(1, &(dev_status.usage)); 
        srcu_read_unlock(&(dev_status.rcu), rcu_index);
        return -ENODATA;
    }

    // get the buffer_head
    bh = (struct buffer_head *)sb_bread(my_bdev_sb, offset+2);
    if(!bh){
        atomic_fetch_sub(1, &(dev_status.usage)); 
        srcu_read_unlock(&(dev_status.rcu), rcu_index);
        return -EIO;
    }

    blk = (struct block*)bh->b_data;

    len = MSG_LEN(blk->metadata);

    if(size >= MSG_MAX_SIZE) //too much 
        to_cpy = len;
    else if(size >= len)
        to_cpy = len;
    else
        to_cpy = size;

    //addr = (void*)get_zeroed_page(GFP_KERNEL);

    addr = kzalloc(sizeof(char)*MSG_MAX_SIZE, GFP_KERNEL);

    memcpy((char*)addr, (char*)blk->data, to_cpy+1);
    addr[to_cpy] = 0x00;

    srcu_read_unlock(&(dev_status.rcu), rcu_index);

    ret = copy_to_user((char*)destination,(char*)addr,to_cpy);//returns the number of bytes NOT copied

    brelse(bh);
    kfree(addr);
    atomic_fetch_sub(1, &(dev_status.usage));

    printk(KERN_INFO "%s: thread %d request for get_data sys_call success\n",MOD_NAME,current->pid);

    return to_cpy;

}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
    __SYSCALL_DEFINEx(1, _invalidate_data, int, offset){
#else
    asmlinkage int sys_invalidate_data(int, offset){
#endif

    struct buffer_head *bh;
    struct block *blk;
    struct invalidate_ret ret;

    // increment usage count
    atomic_fetch_add(1, &(dev_status.usage)); 

    // check if device is mounted
    if(dev_status.bdev == NULL){
        atomic_fetch_sub(1, &(dev_status.usage));
        return -ENODEV;
    }
    
    printk("%s: thread %d requests a invalidate_data sys_call\n",MOD_NAME,current->pid);

    // check if the block is already invalid
    // ret = list_is_valid(&dev_map, offset);
    // if(ret == 0){
    //     printk(KERN_INFO "%s: thread %d request for invalidate_data sys_call error: block is already invalid\n",MOD_NAME,current->pid);
    //     return 0; //already invalid
    // } controllare se è valido non serve, lo fa già remove il controllo

    ret = mutex_lock(&f_mutex);
    if (ret != 0) {
        printk(KERN_CRIT "%s: mutex_lock ERROR\n", MODNAME);
        atomic_fetch_sub(1, &(dev_status.usage));
        return -EBUSY;
    }

    // check if the block is valid
    if(is_block_valid(&dev_map, offset) == 0){
        printk(KERN_INFO "%s: thread %d request for get_data sys_call error: the block is not available\n", MOD_NAME, current->pid);
        atomic_fetch_sub(1, &(dev_status.usage)); 
        srcu_read_unlock(&(dev_status.rcu), rcu_index);
        return -ENODATA;
    }

    synchronize_srcu(&(dev_status.rcu));

    // update the device map setting invalid the block at offset
    ret = set_invalid_block(&dev_map, offset);
    if(ret < 0){
        printk(KERN_INFO "%s: thread %d request for invalidate_data sys_call error: block is already invalid\n",MOD_NAME,current->pid);
        atomic_fetch_sub(1, &(dev_status.usage));
        mutex_unlock(&f_mutex);
        return 0; //already invalid
    }
    

    // update the metadata of the invalitated block
    bh = (struct buffer_head *)sb_bread(my_bdev_sb, offset + 2);
    if(!bh){
        atomic_fetch_sub(1, &(dev_status.usage));
        mutex_unlock(&f_mutex);
        return -EIO;
    }

    blk = (struct block*)bh->b_data;
    blk->metadata = INVALIDATE(blk->metadata);
    blk->next = -2;

    update_file_size(-(MSG_LEN(blk->metadata) + 1)); // +1 is for the \n that is logically used for the division of the messages

    mark_buffer_dirty(bh);
    brelse(bh);

    printk(KERN_INFO "%s: thread %d request for invalidate_data sys_call on block %d success\n",MOD_NAME,current->pid, offset);

    atomic_fetch_sub(1, &(dev_status.usage));
    mutex_unlock(&f_mutex);
    return 0;

}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
    long sys_put_data = (long) __x64_sys_put_data;
    long sys_get_data = (long) __x64_sys_get_data;
    long sys_invalidate_data = (long) __x64_sys_invalidate_data;
#else
#endif
#endif