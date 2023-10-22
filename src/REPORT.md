
The single-file file system has been used and modified to reach the project requests. In particular, the device is dived in blocks: the first (0) and the second (1) respectively are the superblock and the inode of the file. The residual blocks are used to store the user message and can be read as a unique file (the one specified by the inode).
Every user-message block has the same size and each of them is formed by two part: data and metadata. The metadata part maintains information about:
- the **validity + message length** of that block: the validity field is a short int in which the first MSb is used as the validity flag and the remaining bits are used to store the len of the message.
- the **next block** : idicates the next block of the messages arriving-order. 




## Pre-module
### singlefilemakefs.c
This takes as argument the device and write in it the superblock. It is called in the make file:
```Makefile
create-fs:
	dd bs=4096 count=$(NBLOCKS) if=/dev/zero of=image
	./singlefilemakefs image
	mkdir mount
```
In particular:
- a struct called `onefilefs_sb_info` is filled up with the information about the filesystem to be mounted on the device
- the struct is written on the device
- a struct called `onefilefs_inode` is filled with the information for the inode
- the struct is written on the device
- padding is generated to fill the remaing device's free space

The resulting device organization is the following:
```
    Block 0     Block 1       Block 2     Block 3      Block 4
---------------------------------------------------------------------
|            |              | File meta  |  File meta  | File meta  |
| Superblock | Inode of the | ---------- | ----------- | ---------- |
|            |      File    | File data  |  File data  | File data  |
---------------------------------------------------------------------
```

Before initializing the device, a consistency check is performed to retrieve old information from the device.

## Module
### singlefilefs-src.c
When the module is inserted the following routines are set as init and exit:
```C
module_init(singlefilefs_init);
module_exit(singlefilefs_exit);
```
#### singlefilefs_init
In this function there is a call to `syscall_table_finder()` which finds out the syscall table address and the sys_ni_syscall
entries in the table. Such entries will be used to insert the three needed new syscall (put, get, invalidate).
When the entries are found, then the new syscall are inserted in the syscall table.

Then the file system is registered.

#### singlefilefs_mount
This function is invoked on the file system mounting. It's goal is to set up the file system and this is done thanks to 
the `mount_bdev` function. This function takes some arguments and one of this is  a function pointer which specify the function
that is in charge to fill the superblock. This function is `singlefilefs_fill_super`.


### newsyscall.c
There is the implementation of the three new syscall.


## Loading the module
### Configuration
The following paramaters can be configured (or default values will be used):
- desired number of block of the device
- synchronous write (1/0): if the writes on the device has to be done in a synch-manner (1) or simply by the kernel deamon in wirte-back manner.
- verbouse output (1/0)

These parameters can be configured changing the value inside the 'Makefile.env' file, located in the root folder of the project.

### Load
A usefull script is provided to facilitate the insertion of the module. This script can be invoked in the on the shell using sh:
```Shell
$ sh load.sh
```
> N.B. root privileges are required.

This script:
- compiles all the needed .c files
- check if there is an old image of the device and in case asks the user to use it or not
- mount the file system
- insert the module

To remove the module the unload.sh script can be used. 
