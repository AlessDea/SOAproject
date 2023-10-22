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
    // struct inode * the_inode = filp->f_inode;
    int ret;
    long block_to_read;//index of the block to be read from device
    struct block *msg;
    char *tmp;
    short msg_len;
    loff_t read;
    char end_str = '\0';

    loff_t start_bindx; //block from which start reading
    size_t to_read;

    int rcu_index;

    /**
     * TODO: devo fottermene di off, infatti quando viene invocata la read con off 0 (quasi sempre) e se 0 non è il primo blocco, allora so cazzi!
     * 
     * 
    */
    
    read = 0;

    if(dev_status.bdev == NULL){
        __sync_fetch_and_sub(&(dev_status.usage), 1);
        return -ENODEV;
    }

    __sync_fetch_and_add(&(dev_status.usage), 1); 


    printk(KERN_INFO "%s: read operation called with len %ld - and offset %lld",MOD_NAME, len, *off);

    rcu_index = srcu_read_lock(&(dev_status.rcu));



    //check if there is something in the device
    if(device_is_empty(&dev_map)){
        printk(KERN_INFO "%s: Empty file", MOD_NAME);
        *off = 0;
        __sync_fetch_and_sub(&(dev_status.usage), 1);
        srcu_read_unlock(&(dev_status.rcu), rcu_index);
        return 0;
    }

    if(*off != 0){
        *off = 0;
        __sync_fetch_and_sub(&(dev_status.usage), 1);
        srcu_read_unlock(&(dev_status.rcu), rcu_index);
        return 0;
    }
    

    
    to_read = len;
    to_read--; //reserve a byte for the \0
    

    //check from whick block need

    start_bindx = get_first_valid_block(&dev_map);
    if(start_bindx < 0){
        printk(KERN_INFO "%s: Empty file", MOD_NAME);
        *off = 0;
        __sync_fetch_and_sub(&(dev_status.usage), 1);
        srcu_read_unlock(&(dev_status.rcu), rcu_index);
        return 0;
    }
    printk(KERN_INFO "%s: read operation must access block %lld of the device", MOD_NAME, start_bindx);


    printk(KERN_INFO "%s: read operation request is valid (len = %ld): to_read %ld bytes starting from offset: %lld",MOD_NAME, len, to_read, start_bindx);
    

    block_to_read = start_bindx;
    while(to_read > 0 && block_to_read >= 0){
        block_to_read += 2; //the value 2 accounts for superblock and file-inode on device

        printk(KERN_INFO "%s: read operation must access block %ld of the device", MOD_NAME, block_to_read);

        bh = (struct buffer_head *)sb_bread(filp->f_path.dentry->d_inode->i_sb, block_to_read);
        if(!bh){
            __sync_fetch_and_sub(&(dev_status.usage), 1);
            srcu_read_unlock(&(dev_status.rcu), rcu_index);
            return -EIO;
        }

        msg = (struct block *)bh->b_data;
        msg_len = MSG_LEN(msg->metadata); 

        printk(KERN_INFO "%s: read operation of message: %s.", MOD_NAME, msg->data);



        if(msg_len >= to_read)
            msg_len = to_read - 1;

        tmp = kzalloc(sizeof(char)*(msg_len + 1), GFP_KERNEL); // +1 for '/n'
        if(!tmp){
            printk("%s: kzalloc error, unable to allocate memory for read messages as single file\n", MOD_NAME);
            __sync_fetch_and_sub(&(dev_status.usage), 1);
            srcu_read_unlock(&(dev_status.rcu), rcu_index);
            return 0;
        }
        
        memcpy(tmp, msg->data, msg_len);
        tmp[msg_len] = '\n';
        printk(KERN_INFO "%s: msg %s", MOD_NAME, tmp);

        brelse(bh);

        ret = copy_to_user(buf + read, tmp, msg_len + 1);
        if(ret != 0){
            printk(KERN_INFO "%s: An error occured during the copy of the message from kernel space to user space", MOD_NAME);
            kfree(tmp);
            *off = 0;
            __sync_fetch_and_sub(&(dev_status.usage), 1);
            srcu_read_unlock(&(dev_status.rcu), rcu_index);
            return 0;
        }

        read += (msg_len + 1 - ret);
        to_read -= (msg_len + 1 - ret);

        //block_to_read = list_next_valid(&dev_map, block_to_read - 2);
        block_to_read = get_next_valid_block(&dev_map, block_to_read - 2);
        printk(KERN_INFO "%s: next block to read: %ld", MOD_NAME, block_to_read);
        printk(KERN_INFO "%s: to_read %ld", MOD_NAME, to_read);

        kfree(tmp);

    }

    ret = copy_to_user(buf + read, &end_str, 1);

    //ret = copy_to_user(buf, buffer, len);
    //ret = copy_to_user(buf + read + 1, '\0', msg_len);

    *off = *off + read;
    printk(KERN_INFO "%s: last offset position %lld", MOD_NAME, *off);

    srcu_read_unlock(&(dev_status.rcu), rcu_index);
    __sync_fetch_and_sub(&(dev_status.usage), 1);
    return read;

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
        bh = (struct buffer_head *)sb_bread(sb, SINGLEFILEFS_INODES_BLOCK_NUMBER);
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



int onefilefs_open(struct inode *pinode, struct file *pfile) {

    if (dev_status.bdev == NULL) {
		printk("%s: Block Device not mounted\n", MOD_NAME);
		return -ENODEV;
	}


    if(pfile->f_mode & FMODE_WRITE){
        printk(KERN_INFO "%s: open in write mode not permitted", MOD_NAME);
        return -EPERM;
    }

    mutex_init(&f_mutex);

    printk(KERN_INFO "%s: open operation called: success",MOD_NAME);

    return 0;
}

int onefilefs_release(struct inode *pinode, struct file *pfile) {

    if (dev_status.bdev == NULL) {
		printk("%s: Block Device not mounted\n", MOD_NAME);
		return -ENODEV;
	}

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
    .open = onefilefs_open,
    .release = onefilefs_release,
};
