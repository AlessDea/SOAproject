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
    if(off < 0)
        return -ENOMEM;


    /* perform the write in bh */

    blk = kzalloc(sizeof(struct block *), GFP_KERNEL);

    blk->metadata = VALID_MASK ^ size;


    memcpy(blk->data, source, size);

    // get the buffer_head
    bh = (struct buffer_head *)sb_bread(my_bdev_sb, off+2);
    if(!bh){
        return -EIO;
    }

    memcpy(bh->b_data, (char *)blk, BLOCK_SSIZE);

    mark_buffer_dirty(bh);
    brelse(bh);

    /* the block in the list must be visible when the write in bh is done */
    if(!list_insert(&dev_map, off))
        return -ENOMEM;



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


    printk("%s: thread %d requests a get_data sys_call\n",MOD_NAME,current->pid);

    /* check if offset exists */
    if(BLK_INDX(offset) > NBLOCKS-1)
        return -ENODATA;

    if(rcu_list_is_valid(&dev_map, BLK_INDX(offset)) == 0)
        return -ENODATA;

    // get the buffer_head
    bh = (struct buffer_head *)sb_bread(my_bdev_sb, BLK_INDX(offset)+2);
    if(!bh){
        return -EIO;
    }

    blk = (struct block*)bh->b_data;

    len = MSG_LEN(blk->metadata);

    memcpy(destination, blk->data, (len > size) ? size : len);

    brelse(bh);

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
    ret = list_is_valid(&dev_map, BLK_INDX(offset));
    if(ret == 0)
    return 0; //already invalid

    // update the device map setting invalid the block at offset
    list_remove(&dev_map, BLK_INDX(offset));
    // at this point the upcoming reads operations don't find the invalidated block in the list

    // update the metadata of that block
    bh = (struct buffer_head *)sb_bread(my_bdev_sb, BLK_INDX(offset));
    if(!bh){
    return -EIO;
    }

    blk = (struct block*)bh->b_data;
    blk->metadata = INVALIDATE(blk->metadata);

    mark_buffer_dirty(bh);
    brelse(bh);


    return 0;

}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
    long sys_put_data = (long) __x64_sys_put_data;
    long sys_get_data = (long) __x64_sys_get_data;
    long sys_invalidate_data = (long) __x64_sys_invalidate_data;
#else
#endif
#endif