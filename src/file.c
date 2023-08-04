#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/delay.h>



//#include "singlefilefs.h"
#include "helper.h"


ssize_t onefilefs_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {

    struct buffer_head *bh = NULL;
    struct inode * the_inode = filp->f_inode;
    uint64_t file_size = the_inode->i_size;
    int ret;
    long block_to_read;//index of the block to be read from device
    struct block *msg;
    char *buffer, *tmp;
    short msg_len;
    size_t tmp_len;
    //loff_t start_off;
    long read;

    read = 0;

    //check if the device is mounted
    if(&dev_map == NULL){
        //device is not mounted
        return -ENODEV;
    }

    printk(KERN_INFO "%s: read operation called with len %ld - and offset %lld (the current file size is %lld)",MOD_NAME, len, *off, file_size);

    //this operation is not synchronized
    //*off can be changed concurrently
    //add synchronization if you need it for any reason
    //check that *off is within boundaries
    if(*off < 0) return 0;

    if (*off > file_size)
        return 0;
    else if ((*off + len) > file_size)
        len = file_size - *off;

    //start_off = *off % BLOCK_SSIZE;

    printk(KERN_INFO "%s: read operation in boundaries (len = %ld)",MOD_NAME, len);


// TODO: get a lock on the device

    tmp_len = len;
    /* read until there are not remaining bytes */
    block_to_read = list_first_valid(&dev_map); // get the first valid block in according to message arrive order
    //block_to_read = list_is_valid(&dev_map, BLK_INDX(*off));
    if(block_to_read == -1){
        //no valid block
        printk(KERN_INFO "%s: read operation error no valid blocks", MOD_NAME);
        return 0;
    }
    while(tmp_len > 0 && block_to_read >= 0){
        block_to_read += 2; //the value 2 accounts for superblock and file-inode on device

        printk(KERN_INFO "%s: read operation must access block %ld of the device", MOD_NAME, block_to_read);

        bh = (struct buffer_head *)sb_bread(filp->f_path.dentry->d_inode->i_sb, block_to_read);
        if(!bh){
            return -EIO;
        }

        msg = (struct block *)bh->b_data;
        msg_len = MSG_LEN(msg->metadata); //contains +1 for the null terminator

        printk(KERN_INFO "%s: read operation of message: %s", MOD_NAME, msg->data);


        tmp = kmalloc(msg_len, GFP_KERNEL);
        if(!tmp){
            printk("%s: kmalloc error, unable to allocate memory for read messages as single file\n", MOD_NAME);
            return 0;
        }

        memcpy(tmp, msg->data, msg_len - 1); //-1 to not to count the terminator
        tmp[msg_len - 1] = '\n';
        printk(KERN_INFO "%s: tmp %s", MOD_NAME, tmp);


        brelse(bh);

        ret = copy_to_user(buf + read, tmp, msg_len);

        printk(KERN_INFO "%s: copy success (%d)", MOD_NAME, ret);

        read += (msg_len - ret);

        tmp_len -= (msg_len);
//        start_off = 0;

        block_to_read = list_next_valid(&dev_map, block_to_read - 2);
        printk(KERN_INFO "%s: next block to read: %ld", MOD_NAME, block_to_read);
        printk(KERN_INFO "%s: tmp_len %ld", MOD_NAME, tmp_len);

        kfree(tmp);

    }

    // TODO: release lock

    //ret = copy_to_user(buf, buffer, len);

    //ret = copy_to_user(buf + read + 1, '\0', msg_len);

    *off += read;

    return len;

}

/* the write operation must do nothing so it's implementation it's dummy, I need it just for reject the write request */
ssize_t exer_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset) {

    printk(KERN_INFO "%s: read operation called with len %ld - and offset %lld (the current file size is %lld)",MOD_NAME, len, *off, file_size);

    //make sure the write operation returns a error value
    return -1; 
}   

struct dentry *onefilefs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {

    struct onefilefs_inode *FS_specific_inode;
    struct super_block *sb = parent_inode->i_sb;
    struct buffer_head *bh = NULL;
    struct inode *the_inode = NULL;

    printk(KERN_INFO "%s: running the lookup inode-function for name %s",MOD_NAME,child_dentry->d_name.name);

    if(!strcmp(child_dentry->d_name.name, UNIQUE_FILE_NAME)){


	    //get a locked inode from the cache
        the_inode = iget_locked(sb, 1);
        if (!the_inode)
       		 return ERR_PTR(-ENOMEM);

        //already cached inode - simply return successfully
        if(!(the_inode->i_state & I_NEW)){
            return child_dentry;
        }


        //this work is done if the inode was not already cached
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,12,0)
        inode_init_owner(sb->s_user_ns, the_inode, NULL, S_IFDIR);//set the root user as owned of the FS root
#else
        inode_init_owner(the_inode, NULL, S_IFREG);
#endif
        the_inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH;
        the_inode->i_fop = &onefilefs_file_operations;
        the_inode->i_op = &onefilefs_inode_ops;

        //just one link for this file
        set_nlink(the_inode,1);

        //now we retrieve the file size via the FS specific inode, putting it into the generic inode
        bh = (struct buffer_head *)sb_bread(sb, SINGLEFILEFS_INODES_BLOCK_NUMBER );
        if(!bh){
            iput(the_inode);
            return ERR_PTR(-EIO);
        }
        FS_specific_inode = (struct onefilefs_inode*)bh->b_data;
        the_inode->i_size = FS_specific_inode->file_size;
        brelse(bh);

        d_add(child_dentry, the_inode);
        dget(child_dentry);

        //unlock the inode to make it usable
        unlock_new_inode(the_inode);

        return child_dentry;
    }

    return NULL;

}



ssize_t onefilefs_open(struct inode *pinode, struct file *pfile) {

    printk(KERN_INFO "%s: open operation called",MOD_NAME);

    return 0;
}

ssize_t onefilefs_release(struct inode *pinode, struct file *pfile) {

    printk(KERN_INFO "%s: release operation called",MOD_NAME);

    return 0;
}


//look up goes in the inode operations
const struct inode_operations onefilefs_inode_ops = {
    .lookup = onefilefs_lookup,
};

const struct file_operations onefilefs_file_operations = {
    .owner = THIS_MODULE,
    .read = onefilefs_read,
    //.write = onefilefs_write //please implement this function to complete the exercise
    .open = onefilefs_open,
    .release = onefilefs_release,
};
