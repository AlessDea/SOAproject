#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/version.h>


//#include "singlefilefs.h"
#include "helper.h"



ssize_t onefilefs_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {

    struct buffer_head *bh = NULL;
    struct inode * the_inode = filp->f_inode;
    uint64_t file_size = the_inode->i_size;
    int ret;
    long block_to_read;//index of the block to be read from device
    struct block *msg;
    size_t rem;


    printk(KERN_INFO "%s: read operation called with len %ld - and offset %lld (the current file size is %lld)",MOD_NAME, len, *off, file_size);

    //this operation is not synchronized
    //*off can be changed concurrently
    //add synchronization if you need it for any reason

    //check that *off is within boundaries
    /*if (*off >= file_size)
        return 0;
    else if (*off + len > file_size)
        len = file_size - *off;*/

    // la seguente è un test per verificare se funziona. In teoria bisogna far si che venga aggiornato
    // il file_size, ovvero filp->the_inode->i_size ad ogni scrittura del file

    if(*off < 0) return 0;

    if(*off > BLOCK_SSIZE*NBLOCKS){
        // out of bundaries
        return 0;
    }else if(*off + len > BLOCK_SSIZE*NBLOCKS){
        len = BLOCK_SSIZE*NBLOCKS - *off;
    }else

    // now check if len is the block size boundary
    if(len > BLOCK_SSIZE){
        rem = len - BLOCK_SSIZE;
    }else{
        rem = len;
    }


    block_to_read = BLK_INDX(*off);


    // determine if the block is valid
    if(!list_is_valid(&dev_map, BLK_INDX(*off)))
        block_to_read = list_next_valid(&dev_map, BLK_INDX(*off));

    if(block_to_read == -1)
        return 0; // no data available



    //determine the block level offset for the operation (the offset in the block)
    // TODO: the read operation must must be done from the offset specified and if size goes out the block's boundary then read at next block the remaining bytes
    //offset = *off % DEFAULT_BLOCK_SIZE;


    /*//just read stuff in a single block - residuals will be managed at the applicatin level
    if (offset + len > DEFAULT_BLOCK_SIZE)
        len = DEFAULT_BLOCK_SIZE - offset;
*/
    //compute the actual index of the the block to be read from device
    block_to_read += 2; //the value 2 accounts for superblock and file-inode on device

    printk(KERN_INFO "%s: read operation must access block %ld of the device", MOD_NAME, block_to_read);

    bh = (struct buffer_head *)sb_bread(filp->f_path.dentry->d_inode->i_sb, block_to_read);
    if(!bh){
	    return -EIO;
    }
    //ret = copy_to_user(buf, (bh->b_data + offset), len);

    msg = (struct block *)bh->b_data;

    if(rem > MSG_LEN(msg->metadata))
        rem = MSG_LEN(msg->metadata);

    ret = copy_to_user(buf, msg->data, rem);

    *off += (rem - ret);
    brelse(bh);

    return rem - ret;

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

//look up goes in the inode operations
const struct inode_operations onefilefs_inode_ops = {
    .lookup = onefilefs_lookup,
};

const struct file_operations onefilefs_file_operations = {
    .owner = THIS_MODULE,
    .read = onefilefs_read,
    //.write = onefilefs_write //please implement this function to complete the exercise
};
