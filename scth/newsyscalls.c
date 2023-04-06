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

    int off;
    struct block *blk;
    struct buffer_head *bh;

    printk(KERN_INFO "%s: thread %d requests a put_data sys_call\n",MOD_NAME,current->pid);

    /* the operation must be done all or nothing so if there is no enoug space returns ENOMEM.
     * Two things must be checked:
     * - the user message fits in the block (size <= MSG_MAX_SIZE)
     * - there is a free block
     * */

    /* check if size is less-equal of the block size */
    if(size > MSG_MAX_SIZE)
        return -ENOMEM;


    /* check if there is a free block available */
    off = list_first_free(&dev_map);
    if(off < 0){
        printk(KERN_INFO "%s: thread %d request for put_data sys_call: no free blocks available", MOD_NAME, current->pid);
        return -ENOMEM;
    }



    /* perform the write in bh */
    blk = kzalloc(sizeof(struct block *), GFP_KERNEL);

    blk->metadata = VALID_MASK ^ (size + 1);

    memcpy(blk->data, source, size + 1); // +1 for the null terminator

    // get the buffer_head
    bh = (struct buffer_head *)sb_bread(my_bdev_sb, off+2); // +2 for the superblock and inode blocks
    if(!bh){
        return -EIO;
    }

    printk(KERN_INFO "%s: thread %d request for put_data sys_call .. trying to write msg: %s\n",MOD_NAME,current->pid, blk->data);

    memcpy(bh->b_data, (char *)blk, sizeof(struct block));

    printk(KERN_INFO "%s: thread %d request for put_data sys_call .. wrote msg: %s (%ld) [metadata: %d]\n",MOD_NAME,current->pid, ((struct block *)(bh->b_data))->data, size, blk->metadata);

    mark_buffer_dirty(bh);
    brelse(bh);

    /* the block in the list must be visible when the write in bh is done */
    if(!list_insert(&dev_map, off)){
        printk(KERN_INFO "%s: thread %d request for put_data sys_call error\n",MOD_NAME,current->pid);
        return -ENOMEM;
    }

    update_file_size(size);
    printk(KERN_INFO "%s: thread %d request for put_data sys_call success\n",MOD_NAME,current->pid);


    return off;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
    __SYSCALL_DEFINEx(3, _get_data, int, offset, char *, destination, size_t, size){
#else
    asmlinkage int sys_get_data(int offset, char * destination, size_t size){
#endif

    int len;
    struct buffer_head *bh;
    struct block *blk;


    printk(KERN_INFO "%s: thread %d requests a get_data sys_call\n",MOD_NAME,current->pid);

    /* check if offset exists */
    //if(BLK_INDX(offset) > NBLOCKS-1)
    //    return -ENODATA;

    //off = BLK_INDX(offset);

    if(rcu_list_is_valid(&dev_map, offset) == 0){
        printk(KERN_INFO "%s: thread %d request for get_data sys_call error: the block is not available\n", MOD_NAME, current->pid);
        return -ENODATA;
    }

    // get the buffer_head
    bh = (struct buffer_head *)sb_bread(my_bdev_sb, offset+2);
    if(!bh){
        return -EIO;
    }

    blk = (struct block*)bh->b_data;

    len = MSG_LEN(blk->metadata);

    memcpy(destination, blk->data, (len > size) ? (size+1) : (len+1));

    brelse(bh);

    printk(KERN_INFO "%s: thread %d request for get_data sys_call success\n",MOD_NAME,current->pid);

    return (len > size) ? size : len;

}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
    __SYSCALL_DEFINEx(1, _invalidate_data, int, offset){
#else
    asmlinkage int sys_invalidate_data(int, offset){
#endif

    int ret;
    struct buffer_head *bh;
    struct block *blk;


    printk("%s: thread %d requests a invalidate_data sys_call\n",MOD_NAME,current->pid);

    // check if the block is already invalid
    ret = list_is_valid(&dev_map, offset);
    if(ret == 0){
        printk(KERN_INFO "%s: thread %d request for invalidate_data sys_call error: block is already invalid\n",MOD_NAME,current->pid);
        return 0; //already invalid
    }
    // update the device map setting invalid the block at offset
    list_remove(&dev_map, offset);
    // at this point the upcoming reads operations don't find the invalidated block in the list

    // update the metadata of that block
    bh = (struct buffer_head *)sb_bread(my_bdev_sb, offset);
    if(!bh){
        return -EIO;
    }

    blk = (struct block*)bh->b_data;
    blk->metadata = INVALIDATE(blk->metadata);

    update_file_size(-(MSG_LEN(blk->metadata) + 1)); // +1 is for the \n thai is logically used for the division of the messages

    mark_buffer_dirty(bh);
    brelse(bh);

    printk(KERN_INFO "%s: thread %d request for invalidate_data sys_call success\n",MOD_NAME,current->pid);


    return 0;

}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
    long sys_put_data = (long) __x64_sys_put_data;
    long sys_get_data = (long) __x64_sys_get_data;
    long sys_invalidate_data = (long) __x64_sys_invalidate_data;
#else
#endif
#endif