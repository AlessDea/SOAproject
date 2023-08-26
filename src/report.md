
# Idea
Use single-file file system provided by the Professor and modify it to make it fit the requests. In particular, the device is dived in blocks: the first (0) and the second (1) respectively are
the superblock and the inode of the file. The residual blocks are used to store the user message and can be read as a unique file (the one specified by the inode).
Every user-message block has the same size and each of them is formed by two part: data and metadata. The metadata part maintains information about:
- the **validity** of that block: the validity field is a short int in which the first MSb is used as the validity flag and the remaining bits are used to store the len of the message.
- the **next block** (*)

(*)Every message has to be stored in a free block and must follow the chronological order of arrive, so this is the reason for the 'next block' information in metadatas.

The first idea was to have only the validity flag inside the metadata and use a **rcu list** to keep information about the messages' order. This model allow to have user messages larger because of the
metadata small size.


> N.B.
> Having an rcu list is not so bad, because of caching. In fact when an operation is performed on the device, we get the buffer-head of the target block so that it is in cache. Performing all the rcu operations
> on the device would mean that all the blocks has to be moved in cache.
> 
> The 'problem' is that using the rcu list means that the metadata part in each block is useless.

<span style="color: red;">N.B.: Not important because the consistency is not required, we are working on an in-ram device.
The problem here is related to the fact that there is no way to work on an old device because, even if a 'cold-start' routine would be implemented to restore information from 
an old block, the information about the sequence of the messages are lost.
Due to this, I've changed model and now the metadata of each block contains also the 'next-block'. In particular, the validity field is a short int in which the first MSb is used as the validity flag and
the remaining bits are used to store the len of the message. The field 'next-block' must contain the address of the next block, so it has to be a long int (?).
PROBLEM: when a deletion is done there is the necessity of performing an update of the `next_block`in the previous and next blocks, otherwise the messages order is lost:</span>

```
Let's suppose this is the state of the blocks (0 and 1 excluded) at certain time:
         head
[2]      [3]      [4]      [5]      [6] 
          +-------^       
                   +-------^
 ^-------------------------+
 
 So the order of the messages is:
 3 -> 4 -> 5 -> 2
 If the block 5, for example, is deleted then at block 4 there is the necessity to update the next_block
 but the question is: how to know which is the previous block when deleting block 5? The answer can be: make it
 as a double linked list. ==> an rcu list is the better choice, avoiding increasing metadata's size.
 
```


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

#### sys_put_data
This is the first syscall, it's goal is to put user message in a free block of the device. The steps operated by this syscall:
- check if the user message fits the block
- check if there is an available block on the device and get its index
- create a new block and fill its metadata and data
- get the buffer_head at the index of the available block found above
- perform the write operation on the buffer_head of the superblock