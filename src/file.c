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

//TODO: aggiungi l'usage: usalo nella read e nelle syscall PUT e INVALIDATE

//#include "singlefilefs.h"
#include "helper.h"

ssize_t onefilefs_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {

    struct buffer_head *bh = NULL;
    struct inode * the_inode = filp->f_inode;
    uint64_t file_size = the_inode->i_size;
    int ret;
    long block_to_read;//index of the block to be read from device
    struct block *msg;
    char *tmp;
    short msg_len;
    size_t tmp_len;
    //loff_t start_off;
    loff_t read;
    char end_str = '\0';

    long start_bindx; //block from which start reading
    loff_t to_read;

    read = 0;

    

    //check if the device is mounted --> fallo in altro modo
    // if(&dev_map == NULL){
    //     //device is not mounted
    //     return -ENODEV;
    // }

    printk(KERN_INFO "%s: read operation called with len %ld - and offset %lld (the current file size is %lld)",MOD_NAME, len, *off, file_size);

    //this operation is not synchronized
    //*off can be changed concurrently
    //add synchronization if you need it for any reason
    mutex_lock(&f_mutex);

    //check that *off is within boundaries
    if(*off < 0){
        mutex_unlock(&f_mutex);
        return 0;
    } 

    if(*off >= file_size){ //EOF
        printk(KERN_INFO "%s: Offset out of boundaries, starting from offset 0", MOD_NAME);
        *off = 0;
        mutex_unlock(&f_mutex);
        return 0;
    }

    //check if there is something in the device
    if(rcu_list_get_first_valid(&dev_map) == -1){
        printk(KERN_INFO "%s: Empty file", MOD_NAME);
        *off = 0;
        mutex_unlock(&f_mutex);
        return 0;
    }

    // if(len > file_size){
    //     to_read = file_size + 1;
    // }else{
    //     to_read = len;
    // }
    to_read = len;
    to_read--; //reserve a byte for the \0
    

    //check from whick block need
    start_bindx = BLK_INDX(*off);

    if(!list_is_valid(&dev_map, start_bindx)){
        //the starting block is not valid, start from the first valid
        start_bindx = list_first_valid(&dev_map);
    }

    //blks_to_read = BLK_INDX(*off + len) - start_bindx; //number of blocks to read

    printk(KERN_INFO "%s: read operation in boundaries (len = %ld; file size = %lld)",MOD_NAME, len, file_size);
    

    block_to_read = start_bindx;
    while(to_read > 0 && block_to_read >= 0){
        block_to_read += 2; //the value 2 accounts for superblock and file-inode on device

        printk(KERN_INFO "%s: read operation must access block %ld of the device", MOD_NAME, block_to_read);

        bh = (struct buffer_head *)sb_bread(filp->f_path.dentry->d_inode->i_sb, block_to_read);
        if(!bh){
            mutex_unlock(&f_mutex);
            return -EIO;
        }

        msg = (struct block *)bh->b_data;
        msg_len = MSG_LEN(msg->metadata); 

        printk(KERN_INFO "%s: read operation of message: %s.", MOD_NAME, msg->data);


        //there is no check if msg_len is in boundary with len
        //es. 
        // b1 len = 10, msg_len = 4 
        // b2 len = 6, msg_len = 3  
        // b3 len = 3, msg_len = 2 
        //read end: len = 1 
        if(msg_len >= to_read)
            msg_len = to_read - 1;

        tmp = kzalloc(sizeof(char)*(msg_len + 1), GFP_KERNEL); // +1 for '/n'
        if(!tmp){
            printk("%s: kzalloc error, unable to allocate memory for read messages as single file\n", MOD_NAME);
            mutex_unlock(&f_mutex);
            return 0;
        }
        
        memcpy(tmp, msg->data, msg_len);
        tmp[msg_len] = '\n';
        printk(KERN_INFO "%s: msg %s", MOD_NAME, tmp);

        brelse(bh);

        ret = copy_to_user(buf + read, tmp, msg_len + 1);
        if(ret != 0){
            //an error occured during the copy, return
            printk(KERN_INFO "%s: An error occured during the copy of the message from kernel space to user space", MOD_NAME);
            kfree(tmp);
            *off = 0;
            mutex_unlock(&f_mutex);
            return 0;
        }

        read += (msg_len + 1 - ret);
        to_read -= (msg_len + 1 - ret);

        block_to_read = list_next_valid(&dev_map, block_to_read - 2);
        printk(KERN_INFO "%s: next block to read: %ld", MOD_NAME, block_to_read);
        printk(KERN_INFO "%s: to_read %ld", MOD_NAME, to_read);

        kfree(tmp);

    }

    ret = copy_to_user(buf + read, &end_str, 1);

    //ret = copy_to_user(buf, buffer, len);
    //ret = copy_to_user(buf + read + 1, '\0', msg_len);

    *off = *off + read;
    printk(KERN_INFO "%s: last offset position %ld", MOD_NAME, *off);

    mutex_unlock(&f_mutex);

    
    return read;

}


// ssize_t onefilefs_read2(struct file *filp, char __user *buf, size_t len, loff_t *off) {

//     struct buffer_head *bh = NULL;
//     struct inode * the_inode = filp->f_inode;
//     loff_t file_size = the_inode->i_size;

//     loff_t to_read;
//     loff_t read;

//     short msg_len;
//     long blks_to_read;

//     char *msg;
//     char *tmp_buf;
//     char *curr;

//     long start_bindx; //block from which start reading
//     short start_inb_off;
//     long next_blk;

//     int ret;


//     //check if there is something in the device
//     if(rcu_list_get_first_valid(&dev_map) == -1){
//         printk(KERN_INFO "%s: Empty file", MOD_NAME);
//         *off = 0;
//         return 0;
//     }

//     if(*off >= file_size){ //EOF
//         printk(KERN_INFO "%s: Offset out of boundaries", MOD_NAME);
//         *off = 0;
//         return 0;
//     }

//     // since the data of the block device has to be accessed like a file,
//     // off is the offset inside the file, the starting point from where to read, not the offset (index) of the block. 
//     // check if it is in boundaries
//     if(*off + len > file_size){
//         to_read = file_size - *off;
//     }else{
//         to_read = len;
//     }


//     //check from whick block need to start reading
//     start_bindx = BLK_INDX(*off);

//     //start_inb_off = IN_BLOCK_OFF(*off); //offset inside the block, lecito? no: l'offset è meglio che venga usato solo per indicare il blocco, quindi se è all'interno del blocco si fa partire dall'inizio di quel blocco
    
//     if(!list_is_valid(&dev_map, start_bindx)){
//         //the starting block is not valid, start from the next first valid
//         start_bindx = list_next_valid(&dev_map, start_bindx);
//         start_inb_off = 0;
//     }


//     blks_to_read = BLK_INDX(*off + len) - start_bindx; //number of blocks to read

//     //check if the number of blocks to read is <= than the actual number of valid blocks
//     if(blks_to_read < dev_map.num_of_valid_blocks){
//         blks_to_read = dev_map.num_of_valid_blocks;
//     }
    

//     tmp_buf = kzalloc(sizeof(char)*to_read);
//     if(!tmp_buf){
//         printk("%s: kzalloc error, unable to allocate memory for read messages as single file\n", MOD_NAME);
//         return -1;
//     }
//     curr = tmp_buf;

//     read = 0;
//     next_blk = start_bindx + 2;
//     while(to_read > 0){

//         bh = (struct buffer_head *)sb_bread(filp->f_path.dentry->d_inode->i_sb, next_blk);
//         if(!bh){
//             return -EIO;
//         }

//         msg = (struct block *)bh->b_data;
//         msg_len = MSG_LEN(msg->metadata);

//         // msg += start_inb_off;
//         // msg_len -= start_inb_off;

//         if(msg_len > len){
//             msg_len = len;
//         }

//         memcpy(curr, msg->data, msg_len);
//         curr[msg_len] = '\n';

//         curr += msg_len*sizeof(char) + 1; //move the pointer (+1 for the '\n')

//         to_read -= msg_len + 1;
//         read += msg_len + 1;

//         next_blk = list_next_valid(&dev_map, next_blk) + 2;

//     }

//     bh = NULL;

//     ret = copy_to_user(buf, tmp_buf, read);

//     kfree(tmp_buf);

//     *off += read;

//     return read - ret;

// }



// /* the write operation must do nothing so it's implementation it's dummy, I need it just for reject the write request */
// ssize_t onefilefs_write(struct file *pfile, const char __user *buffer, size_t length, loff_t *offset) {
//     struct inode * the_inode = pfile->f_inode;
//     uint64_t file_size = the_inode->i_size;

//     printk(KERN_INFO "%s: read operation called with len %ld - and offset %lld (the current file size is %lld)",MOD_NAME, length, *offset, file_size);

//     //set the offset to 0 again (?)
//     *offset = 0;

//     //make sure the write operation returns a error value

//     return -EINTR; 
// }   


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



int onefilefs_open(struct inode *pinode, struct file *pfile) {

    if (bdev_status.bdev == NULL) {
		printk("%s: Block Device not mounted\n", MODNAME);
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

    if (bdev_status.bdev == NULL) {
		printk("%s: Block Device not mounted\n", MODNAME);
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
