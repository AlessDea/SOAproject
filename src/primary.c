//
// Created by alessandrodea on 01/03/23.
// https://linux-kernel-labs.github.io/refs/heads/master/labs/block_device_drivers.html#struct-bio-structure

#include <linux/module.h>       /* Needed by all modules */
#include <linux/kernel.h>       /* Needed for KERN_INFO */
#include <linux/init.h>         /* Needed for the macros */

#include <linux/version.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/blkdev.h>
#include <linux/string.h>
#include <linux/blk-mq.h>


#include "helper.h"

MODULE_DESCRIPTION("Block device driver for storing user messages");
MODULE_AUTHOR("Alessandro De Angelis");
MODULE_LICENSE("GPL");

int c = 0;

/* function which transfers data in/from the device */
static void my_block_transfer(struct my_block_dev *dev, sector_t sector, unsigned long len, char *buffer, int cmd)
{
    /* TODO: understand the role of sector and offset, so that you can read from the start.
     * From the specification of the project, the classic read must be done of the whole device (only valid blocks of course).
     * So the write handle can be removed, it's not required.
     */

    //qui si scrive ogni 512 byte, come se i blocchi fossero da 512 ma questi sono i settori. OCCHIO!!!
     unsigned long offset = sector * BLOCK_SSIZE;

    /* check for read/write beyond end of block device */
    if ((offset + len) > dev->size){
        printk("%s: transfer: msg too long for the device %s\n", MODNAME, MY_BLKDEV_NAME);
        return;
    }


    /* here you must check in the device_map which is the status of the device (dirty blocks and empty blocks ...)
     * to perform the request */

    /* read/write to dev buffer depending on cmd */
    if (cmd == 0) {        /* read */
        printk("%s: transfer: it's a read on %s\n", MODNAME, MY_BLKDEV_NAME);
        memcpy(buffer, dev->data + offset, len);
    }else {
        /* before the transfer, the validity flag must be controlled for each block */
        printk("%s: put_data: it's a write on %s\n", MODNAME, MY_BLKDEV_NAME);
        memcpy(dev->data + offset, buffer, len);
    }
}


/* open and release functions */
static int my_block_open(struct block_device *bdev, fmode_t mode)
{
    //TODO: a printk it's enough
    return 0;
}

static void my_block_release(struct gendisk *gd, fmode_t mode)
{
    //TODO: a printk it's enough

}





static blk_status_t my_block_request(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
{
    struct request *rq = bd->rq;
    struct my_block_dev *dev = hctx->queue->queuedata;

    blk_mq_start_request(rq); /* start the request */

    if (blk_rq_is_passthrough(rq)) {    /* in some situation (fs requests) the driver doesn't know how to handle the req; */
        printk (KERN_NOTICE "Skip non-fs request\n");
        blk_mq_end_request(rq, BLK_STS_IOERR);
        goto out;
    }

    c++;
    /* print the request information */
    printk("request received (%d): pos=%llu bytes=%u cur_bytes=%u dir=%c\n", c, (unsigned long long) blk_rq_pos(rq), blk_rq_bytes(rq), blk_rq_cur_bytes(rq),rq_data_dir(rq) ? 'W' : 'R');

    /* do the transfer */
    my_block_transfer(dev, blk_rq_pos(rq), blk_rq_bytes(rq), bio_data(rq->bio), rq_data_dir(rq));

    /* end request successfully */
    blk_mq_end_request(rq, BLK_STS_OK);

    out:
    return BLK_STS_OK;
}



static struct blk_mq_ops my_queue_ops = {
        .queue_rq = my_block_request,
};



/* used to put into one free block of the block-device size bytes of the user-space data identified
 * by the source pointer, this operation must be executed all or nothing; the system call returns an
 * integer representing the offset of the device (the block index) where data have been put; if there
 * is currently no room available on the device, the service should simply return the ENOMEM error; */
int put_data(char * source, size_t size, struct my_block_dev *dev){

    int off;
    /* control if the size fits in the block */
    if(size > MSG_MAX_SIZE)
        return ENOMEM;


    printk("%s: put_data: checking for free space %s\n", MODNAME, MY_BLKDEV_NAME);

    /* control if there is free room */
    for(off = 0; off < NBLOCKS; off++){
        if(map[off].validity == 0)
            break;
    }

    /* no free blocks */
    if(off >= NBLOCKS)
        return ENOMEM;

    printk("%s: put_data: doing the transfer of %s at offset %d\n", MODNAME, MY_BLKDEV_NAME, off);
    /* do the transfer */
    my_block_transfer(dev, off, size, source, 1); //controlla bene se l'off (a 4096) va bene con quello usato in transfer, che va a settori di 512

    return 0;
}



/* used to read up to size bytes from the block at a given offset, if it currently keeps data; this
 * system call should return the amount of bytes actually loaded into the destination area or zero
 * if no data is currently kept by the device block; this service should return the ENODATA error if
 * no data is currently valid and associated with the offset parameter. */
int get_data(int offset, char * destination, size_t size, struct my_block_dev *dev){
    return 0;
}



/* used to invalidate data in a block at a given offset; invalidation means that data should logically
 * disappear from the device; this service should return the ENODATA error if no data is currently valid
 * and associated with the offset parameter.
 */
int invalidate_data(int offset, struct my_block_dev *dev){
    return 0;
}


/* TODO: ioctl() */
int my_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
    long size;
    int ret;
    struct my_block_dev *dev = bdev->bd_disk->private_data;
    struct _ioctl_put_data_args buf1;
    struct _ioctl_get_data_args buf2;
    struct _ioctl_invalidate_args buf3;

    printk("%s: ioctl function for %s\n", MODNAME, MY_BLKDEV_NAME);

    switch(cmd) {
        case IOCTL_PUT_DATA:
            printk("%s: put_data called on %s\n", MODNAME, MY_BLKDEV_NAME);
            //get user arguments
            ret = copy_from_user(&buf1, (struct _ioctl_put_data_args *)arg, sizeof(buf1));
            if(ret != 0) {
                printk("%s: copy_from_user error\n", MODNAME);
                return -1;
            }
            printk("%s: put_data of msg %s (%ld)\n", MODNAME, buf1.src, buf1.size);
            ret = put_data(buf1.src, buf1.size, dev);

            return ret;

        case IOCTL_GET_DATA:
            printk("%s: get_data called on %s\n", MODNAME, MY_BLKDEV_NAME);
            //get user arguments
            ret = copy_from_user(&buf2, (struct _ioctl_get_data_args *)arg, sizeof(buf2));
            if(ret != 0) {
                printk("%s: copy_from_user error\n", MODNAME);
                return -1;
            }

            ret = get_data(buf2.offset, buf2.dst, buf2.size, dev);

            return ret;


        case IOCTL_INVALIDATE_DATA:
            printk("%s: invalidate_data called on %s\n", MODNAME, MY_BLKDEV_NAME);

            ret = copy_from_user(&buf3, (struct _ioctl_invalidate_args *)arg, sizeof(buf3));
            if(ret != 0) {
                printk("%s: copy_from_user error\n", MODNAME);
                return -1;
            }

            ret = invalidate_data(buf3.offset, dev);

            return ret;

    }

    return -ENOTTY; /* unknown command */
}

static const struct block_device_operations my_block_ops = {
        .owner = THIS_MODULE,
        .open = my_block_open,
        .release = my_block_release,
        .ioctl = my_ioctl
};


/* create a disk and adding it to the system */
static int create_block_device(struct my_block_dev *dev)
{

    int err, i;

    dev->size = DEVICE_SIZE; //before: NR_SECTORS * KERNEL_SECTOR_SIZE;
    dev->data = vmalloc(dev->size);
    if (dev->data == NULL) {
        printk(KERN_ERR "vmalloc: out of memory\n");
        err = -ENOMEM;
        goto out_vmalloc;
    }


    /* Initialize tag set. */
    dev->tag_set.ops = &my_queue_ops;
    dev->tag_set.nr_hw_queues = 1;
    dev->tag_set.queue_depth = 128;
    dev->tag_set.numa_node = NUMA_NO_NODE;
    dev->tag_set.cmd_size = 0;
    dev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
    err = blk_mq_alloc_tag_set(&dev->tag_set);
    if (err) {
        printk(KERN_ERR "blk_mq_alloc_tag_set: can't allocate tag set\n");
        goto out_alloc_tag_set;
    }

    /* Allocate queue. */
    dev->queue = blk_mq_init_queue(&dev->tag_set);
    if (IS_ERR(dev->queue)) {
        printk(KERN_ERR "blk_mq_init_queue: out of memory\n");
        err = -ENOMEM;
        goto out_blk_init;
    }

    blk_queue_logical_block_size(dev->queue, BLOCK_SSIZE);

    /* Assign private data to queue structure. */
    dev->queue->queuedata = dev; //queuedata field can be used for whatever you like

    /* Initialize the gendisk structure */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0))
    dev->gd = alloc_disk(MY_BLOCK_MINORS);
#else
    dev->gd = blk_alloc_disk(MY_BLOCK_MINORS);
#endif
    if (!dev->gd) {
        printk (KERN_NOTICE "alloc_disk failure\n");
        err = -ENOMEM;
        goto out_alloc_disk;
    }

    dev->gd->major = major;
    dev->gd->first_minor = 0;
    dev->gd->fops = &my_block_ops;
    dev->gd->queue = dev->queue;
    dev->gd->private_data = dev;
    snprintf (dev->gd->disk_name, 32, MY_BLKDEV_NAME);
    set_capacity(dev->gd, NR_SECTORS);

    add_disk(dev->gd);

    /* init device map */
    for(i = 0; i < NBLOCKS; i++) {
        map[i].validity = 0;
        map[i].dirty_len = 0;
    }

    return 0;

    out_alloc_disk:
    blk_cleanup_queue(dev->queue);
    out_blk_init:
    blk_mq_free_tag_set(&dev->tag_set);
    out_alloc_tag_set:
    vfree(dev->data);
    out_vmalloc:
    return err;
}


/* init and exit */
static int __init my_block_init(void)
{
    int status;
    major = 0;

    status = register_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
    if (status < 0) {
        printk(KERN_ERR "%s: unable to register block device %s\n", MODNAME, MY_BLKDEV_NAME);
        return -EBUSY;
    }
    major = status;
    printk(KERN_INFO "%s: new device registered, it is assigned major number %d\n",MODNAME, major);

    printk("%s: sucessfully registered block device %s\n", MODNAME, MY_BLKDEV_NAME);

    status = create_block_device(&dev); //create the disk
    if (status < 0)
        goto out;

    printk("%s: sucessfully created block device %s\n", MODNAME, MY_BLKDEV_NAME);
    return 0;

    out:
    unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);
    return status;
}

/* deleting disk from the system */
static void delete_block_device(struct my_block_dev *dev)
{
    if (dev->gd) {
        del_gendisk(dev->gd);
        put_disk(dev->gd);
    }

    if (dev->queue)
        blk_cleanup_queue(dev->queue);
    if (dev->tag_set.tags)
        blk_mq_free_tag_set(&dev->tag_set);
    if (dev->data)
        vfree(dev->data);
}


static void my_block_exit(void)
{
    delete_block_device(&dev);

    unregister_blkdev(MY_BLOCK_MAJOR, MY_BLKDEV_NAME);  //delete the disk
    printk("%s: sucessfully unregistered block device\n", MODNAME);

}

module_init(my_block_init);
module_exit(my_block_exit);