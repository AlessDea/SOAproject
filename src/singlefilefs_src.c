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
#include <linux/srcu.h>
#include <linux/blkdev.h>

#include "../scth/include/scth.h"
#include "helper.h"


unsigned long the_syscall_table = 0x0;

unsigned long the_ni_syscall;

unsigned long new_sys_call_array[] = {0x0, 0x0, 0x0};
#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};


map dev_map;/* map of the device */
struct super_block *my_bdev_sb; // superblock ref to be used in the systemcalls
struct bdev_status dev_status __attribute__((aligned(64))) = {0, NULL};
struct mutex f_mutex;


static struct super_operations singlefilefs_super_ops = {
        //not required
};


static struct dentry_operations singlefilefs_dentry_ops = {
        //not required
};



int singlefilefs_fill_super(struct super_block *sb, void *data, int silent) {   

    struct inode *root_inode;
    struct buffer_head *bh;
    struct onefilefs_sb_info *sb_disk;
    struct timespec64 curr_time;
    uint64_t magic;


    //Unique identifier of the filesystem
    sb->s_magic = MAGIC;

    bh = sb_bread(sb, SB_BLOCK_NUMBER);
    if(!bh){
	    return -EIO;
    }
    sb_disk = (struct onefilefs_sb_info *)bh->b_data;
    magic = sb_disk->magic;
    brelse(bh);

    //check on the expected magic number
    if(magic != sb->s_magic){
	    return -EBADF;
    }

    sb->s_fs_info = NULL; //FS specific data (the magic number) already reported into the generic superblock
    sb->s_op = &singlefilefs_super_ops;//set our own operations


    root_inode = iget_locked(sb, 0); //get a root inode indexed with 0 from cache
    if (!root_inode){
        return -ENOMEM;
    }

    root_inode->i_ino = SINGLEFILEFS_ROOT_INODE_NUMBER; //this is actually 10

    // From kernel 5.12 inode_init_owner gets one more argument: @mnt_userns:	User namespace of the mount the inode was created from
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,12,0)
    inode_init_owner(sb->s_user_ns, root_inode, NULL, S_IFDIR);//set the root user as owned of the FS root
#else
    inode_init_owner(root_inode, NULL, S_IFDIR);//set the root user as owned of the FS root
#endif

    root_inode->i_sb = sb;
    root_inode->i_op = &onefilefs_inode_ops; //set our inode operations
    root_inode->i_fop = &onefilefs_dir_operations; //set our file operations
    //update access permission
    root_inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH;

    //baseline alignment of the FS timestamp to the current time
    ktime_get_real_ts64(&curr_time);
    root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = curr_time;

    // no inode from device is needed - the root of our file system is an in memory object
    root_inode->i_private = NULL;

    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root)
        return -ENOMEM;

    sb->s_root->d_op = &singlefilefs_dentry_ops;//set our dentry operations

    my_bdev_sb = sb;

    //unlock the inode to make it usable
    unlock_new_inode(root_inode);

    return 0;
}


static void singlefilefs_kill_superblock(struct super_block *s) {
    // write last_key in the superblock
    struct buffer_head *bh;
    struct onefilefs_sb_info *sb_disk;


    if(dev_status.usage > 0){
        printk(KERN_INFO "%s: Unmount process stopped. Device is still in use! \n",MOD_NAME);
        return;
    }

    bh = sb_bread(s, SB_BLOCK_NUMBER);
    if(!bh){
	    return;
    }
    sb_disk = (struct onefilefs_sb_info *)bh->b_data;
    sb_disk->first_key = dev_map.first;
    sb_disk->last_key = dev_map.last;
    sb_disk->f_size = dev_map.size;

    printk(KERN_INFO "%s: last key was %ld and first key was %ld\n",MOD_NAME,sb_disk->last_key,sb_disk->first_key);
    
    mark_buffer_dirty(bh);
    brelse(bh);

    cleanup_srcu_struct(&(dev_status.rcu)); // reset srcu_struct

    kill_block_super(s);
    printk(KERN_INFO "%s: singlefilefs unmount succesful.\n",MOD_NAME);
    return;
}


struct dentry *singlefilefs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {

    struct dentry *ret;
    int last_k, first_k;
    struct onefilefs_sb_info *sb;
    struct buffer_head *bh;
    int srcu_ret;


    printk(KERN_INFO "%s: mounting", MOD_NAME);

    ret = mount_bdev(fs_type, flags, dev_name, data, singlefilefs_fill_super);
    if (unlikely(IS_ERR(ret))) {
        printk(KERN_INFO "%s: error mounting onefilefs", MOD_NAME);
        return ret;
    }else {
        printk(KERN_INFO "%s: singlefilefs is succesfully mounted from device %s\n", MOD_NAME, dev_name);
    }

    dev_status.bdev = blkdev_get_by_path(dev_name, FMODE_READ|FMODE_WRITE, NULL);
    dev_status.usage = 0;
    mutex_init(&f_mutex);

    srcu_ret = init_srcu_struct(&(dev_status.rcu));
    if (srcu_ret != 0) {
        printk(KERN_CRIT "%s: error mounting onefilefs\n", MOD_NAME);
        return ERR_PTR(-ENOMEM);
    }

    
    bh = (struct buffer_head *)sb_bread(my_bdev_sb, 0);
    if(!bh){
        return NULL;
    }

    sb = (struct onefilefs_sb_info*)bh->b_data;


    last_k = sb->last_key;
    first_k = sb->first_key;
   

    if(last_k > -1){
        dev_map.last = last_k;
        dev_map.first = first_k;
        dev_map.size = sb->f_size;
        printk(KERN_INFO "%s: the device was found written: starting up with the old data (last key %ld, first key %ld)\n", MOD_NAME, dev_map.last, dev_map.first);
        reload_device_map(&dev_map);
    }else{
        dev_map.last = -1; //-1 indicates that the device is empty
        dev_map.first = -1;
        dev_map.size = 0;
        printk(KERN_INFO "%s: the device was found empty: starting up with empty device\n", MOD_NAME);
    }

    brelse(bh);

    return ret;
}


//file system structure
static struct file_system_type onefilefs_type = {
	    .owner = THIS_MODULE,
        .name           = "singlefilefs",
        .mount          = singlefilefs_mount,
        .kill_sb        = singlefilefs_kill_superblock,
};


static int singlefilefs_init(void) {

    int ret;

    int i;

    printk(KERN_INFO
    "%s: initializing\n", MOD_NAME);


    /* looking for syscall table */
    syscall_table_finder();

    if (!hacked_syscall_tbl) {
        printk(KERN_INFO
        "%s: failed to find the sys_call_table\n", MOD_NAME);
        return -1;
    }

    AUDIT {
        printk(KERN_INFO
        "%s: sys_call_table address %px\n", MOD_NAME, (void *) the_syscall_table);
        printk(KERN_INFO
        "%s: initializing - hacked entries %d\n", MOD_NAME, HACKED_ENTRIES);
    }

    new_sys_call_array[0] = (unsigned long) sys_put_data;
    new_sys_call_array[1] = (unsigned long) sys_get_data;
    new_sys_call_array[2] = (unsigned long) sys_invalidate_data;

    ret = get_entries(restore, HACKED_ENTRIES, (unsigned long *) the_syscall_table, &the_ni_syscall);


    if (ret != HACKED_ENTRIES) {
        printk(KERN_INFO
        "%s: could not hack %d entries (just %d)\n", MOD_NAME, HACKED_ENTRIES, ret);
        return -1;
    }

    unprotect_memory();

    for (i = 0; i < HACKED_ENTRIES; i++) {
        ((unsigned long *) the_syscall_table)[restore[i]] = (unsigned long) new_sys_call_array[i];
    }

    protect_memory();

    printk(KERN_INFO
    "%s: all new system-calls correctly installed on sys-call table\n", MOD_NAME);



    //register filesystem
    ret = register_filesystem(&onefilefs_type);
    if (likely(ret == 0))
        printk(KERN_INFO "%s: sucessfully registered singlefilefs\n",MOD_NAME);
    else
        printk(KERN_INFO "%s: failed to register singlefilefs - error %d", MOD_NAME,ret);

    return ret;
}


static void singlefilefs_exit(void) {

    int ret, i;

    unprotect_memory();
    for(i=0;i<HACKED_ENTRIES;i++){
        ((unsigned long *)the_syscall_table)[restore[i]] = the_ni_syscall;
    }
    protect_memory();
    printk(KERN_INFO "%s: sys-call table restored to its original content\n",MOD_NAME);

    //unregister filesystem
    ret = unregister_filesystem(&onefilefs_type);

    if (likely(ret == 0))
        printk(KERN_INFO "%s: sucessfully unregistered file system driver\n",MOD_NAME);
    else
        printk(KERN_INFO "%s: failed to unregister singlefilefs driver - error %d", MOD_NAME, ret);
}


module_init(singlefilefs_init);
module_exit(singlefilefs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alessandro De Angelis <alessandro.deangelis97@gmail.com>");
MODULE_DESCRIPTION("SINGLE-FILE-FS");
