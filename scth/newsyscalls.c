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
    struct insert_ret ret;
    char* addr;
    int ret1;

    printk(KERN_INFO "%s: thread %d requests a put_data sys_call\n",MOD_NAME,current->pid);

    /* the operation must be done all or nothing so if there is no enough space returns ENOMEM.
     * Two things must be checked:
     * - the user message fits in the block (size <= MSG_MAX_SIZE)
     * - there is a free block
     * */

    /* check if size is less-equal of the block size */
    // if((size + 1) > MSG_MAX_SIZE)  //+1 for the null terminator
    if((size) >= MSG_MAX_SIZE)  //+1 for the null terminato
        return -ENOMEM; // = doesn't permit to store the '/0' at the end
    


    /* check if there is a free block available and if there is then 'book' it. */
    // off = list_first_free(&dev_map);
    // if(off < 0){
    //     printk(KERN_INFO "%s: thread %d request for put_data sys_call: no free blocks available", MOD_NAME, current->pid);
    //     return -ENOMEM;
    // }

/* the block in the bh must be visible when the insert in rcu list is done (for concurrency)*/
    ret = list_insert(&dev_map);
    if(ret.curr < 0){
        printk(KERN_INFO "%s: thread %d request for put_data sys_call error\n",MOD_NAME,current->pid);
        return -ENOMEM;
    }


    if (ret.prev != -1){
   
        // if the prev is the rcu list head then don't need to have the next field

        // update the previouse block's next field with this one
        bh = (struct buffer_head *)sb_bread(my_bdev_sb, ret.prev+2); // +2 for the superblock and inode blocks
        if(!bh){
            return -EIO;
        }
        blk = (struct block*)bh->b_data;
        blk->next = ret.curr;

        //printk(KERN_INFO "%s: prev's (%ld) next %ld\n",MOD_NAME,ret.prev,blk->next);

        mark_buffer_dirty(bh);
        brelse(bh);

        //blk = NULL;

    }


    addr = kzalloc(sizeof(char)*MSG_MAX_SIZE, GFP_KERNEL);
     if(!addr){
        printk(KERN_INFO "%s: kzalloc error\n",MOD_NAME);
        return -EIO;
    }

    /* perform the write in bh */
    blk1 = kzalloc(sizeof(struct block *), GFP_KERNEL);
    if(!blk1){
        printk(KERN_INFO "%s: kzalloc error\n",MOD_NAME);
        return -EIO;
    }

    ret1 = copy_from_user((char*)addr,(char*)source, size);//returns the number of bytes NOT copied
    addr[size] = '\0';

    blk1->metadata = VALID_MASK ^ (size);

    memcpy(blk1->data, addr, size+1);

    blk1->next = -1; // the next of the last inserted block is always null

    // get the buffer_head
    bh = (struct buffer_head *)sb_bread(my_bdev_sb, ret.curr+2); // +2 for the superblock and inode blocks
    if(!bh){
        return -EIO;
    }

    //printk(KERN_INFO "%s: thread %d request for put_data sys_call .. trying to write msg: %s\n",MOD_NAME,current->pid, blk1->data);

    memcpy(bh->b_data, (char *)blk1, sizeof(struct block));

    //printk(KERN_INFO "%s: thread %d request for put_data sys_call .. wrote msg: %s (%ld)\n",MOD_NAME,current->pid, ((struct block *)(bh->b_data))->data, size);

    mark_buffer_dirty(bh);
    brelse(bh);

    

    //+1 for the null terminator which become a \n in the read operation
    //when a read is performed, with cat for example, the buffer has len as the file, so we need
    //space for the \n for each message
    // update_file_size(size+1);
    update_file_size(size);
    printk(KERN_INFO "%s: thread %d request for put_data sys_call success\n",MOD_NAME,current->pid);

    // kfree(blk);
    // kfree(blk1);
    kfree(addr);
    return ret.curr;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
    __SYSCALL_DEFINEx(3, _get_data, int, offset, char *, destination, size_t, size){
#else
    asmlinkage int sys_get_data(int offset, char * destination, size_t size){
#endif

    short len;
    struct buffer_head *bh;
    struct block *blk;
    char *addr;
    int ret;
    short to_cpy;


    //printk(KERN_INFO "%s: thread %d requests a get_data sys_call\n",MOD_NAME,current->pid);

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
    //destination[(len > size) ? (size) : (len)] = 0x00; non serve perchè tanto copio o len o size

    ret = copy_to_user((char*)destination,(char*)addr,to_cpy);//returns the number of bytes NOT copied

    brelse(bh);

    kfree(addr);

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
    
    printk("%s: thread %d requests a invalidate_data sys_call\n",MOD_NAME,current->pid);

    // check if the block is already invalid
    // ret = list_is_valid(&dev_map, offset);
    // if(ret == 0){
    //     printk(KERN_INFO "%s: thread %d request for invalidate_data sys_call error: block is already invalid\n",MOD_NAME,current->pid);
    //     return 0; //already invalid
    // } controllare se è valido non serve, lo fa già remove il controllo


    // update the device map setting invalid the block at offset
    ret = list_remove(&dev_map, offset);
    if(ret.next == -1 && ret.prev == -1){
        printk(KERN_INFO "%s: thread %d request for invalidate_data sys_call error: block is already invalid\n",MOD_NAME,current->pid);
        return 0; //already invalid
    }
    
    // update the previouse block's next field
    bh = (struct buffer_head *)sb_bread(my_bdev_sb, ret.prev + 2);
    if(!bh){
        return -EIO;
    }

    blk = (struct block*)bh->b_data;
    blk->next = ret.next;

    mark_buffer_dirty(bh);
    brelse(bh);

    blk = NULL;
    bh = NULL;

    // at this point the upcoming reads operations don't find the invalidated block in the list
    // update the metadata of that block
    bh = (struct buffer_head *)sb_bread(my_bdev_sb, offset + 2);
    if(!bh){
        return -EIO;
    }

    blk = (struct block*)bh->b_data;
    blk->metadata = INVALIDATE(blk->metadata);
    blk->next = -2;

    update_file_size(-(MSG_LEN(blk->metadata)+1)); // +1 is for the \n that is logically used for the division of the messages

    mark_buffer_dirty(bh);
    brelse(bh);

    printk(KERN_INFO "%s: thread %d request for invalidate_data sys_call on block %d success\n",MOD_NAME,current->pid, offset);

    return 0;

}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
    long sys_put_data = (long) __x64_sys_put_data;
    long sys_get_data = (long) __x64_sys_get_data;
    long sys_invalidate_data = (long) __x64_sys_invalidate_data;
#else
#endif
#endif