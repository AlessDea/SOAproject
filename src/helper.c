#include "helper.h"

int reload_device_map(map *m){
    struct buffer_head *bh;
	struct block *blk;
	struct super_block *sb = my_bdev_sb;

 	long fk; //first key

    fk = m->first;
   
    do{
		printk(KERN_INFO "%s: reloading from %ld\n", MOD_NAME, fk);

		// get the buffer_head
		bh = (struct buffer_head *)sb_bread(sb, 2 + fk);
		if(!bh){
			return -EIO;
		}

		blk = (struct block*)bh->b_data;

		printk(KERN_INFO "%s: msg: %s (%d)\n",MOD_NAME,blk->data,MSG_LEN(blk->metadata));

		m->keys[fk] = 1;
		m->num_of_valid_blocks++;

		printk(KERN_INFO "%s: reloaded block %ld informations\n", MOD_NAME, fk);

		fk = blk->next;

		brelse(bh);
		blk = NULL;

			
	}while (fk != -1);

    return 0;
}



long get_next_free_block(map *m){
    
	long i;
    // search the first free
    for(i = 0; i < NBLOCKS; i++){
        if(m->keys[i] == 0){
            __sync_fetch_and_add(&m->keys[i], 1); // atomic block reservation for the write
			__sync_fetch_and_add(&m->num_of_valid_blocks, 1);
            break;
        }
    }

    if(i >= NBLOCKS){
		// no free blocks available
        printk(KERN_INFO "%s: No free blocks available\n", MOD_NAME);
		return -1;
	}

    
    return i;
}


int device_is_empty(map *m){
    if(m->num_of_valid_blocks > 0)
		return 0;
	return 1;
}



long is_block_valid(map *m, long idx){
    return m->keys[idx];
}


// long get_first_valid_block(map *m){
// 	long i;
// 	for(i = 0; i < NBLOCKS; i++){
// 		if(m->keys[i] == 1)
// 			return i;
// 	}
// 	return -1;
// }


long get_first_valid_block(map *m){
	return m->first;
}


long get_higher_valid_blk_indx(map *m){
	long i;
	long max = -1;
	for(i = 0; i < NBLOCKS; i++){
		if(m->keys[i] == 1 && i > max)
			max = i;
	}
	return max;
}




long get_next_valid_block(map *m, long idx){
	struct buffer_head *bh;
	struct block *blk;
	long next_idx;
	struct super_block *sb = my_bdev_sb;

	// get the buffer_head
	bh = (struct buffer_head *)sb_bread(sb, 2 + idx);
	if(!bh){
		return -EIO;
	}

	blk = (struct block*)bh->b_data;

	next_idx = blk->next;

	brelse(bh);
	blk = NULL;

	return next_idx;
}


long set_invalid_block(map *m, long idx){
    struct buffer_head *bh, *bh_cur;
	struct block *blk, *cur_blk;
	struct super_block *sb = my_bdev_sb;

 	long fk; //first key
    fk = m->first;

	if(fk < 0)
		return fk;

	// check if the block to delete is the first one
	if(idx == m->first){
		// get the buffer_head
		bh = (struct buffer_head *)sb_bread(sb, 2 + fk);
		if(!bh){
			return -EIO;
		}

		blk = (struct block*)bh->b_data;

		m->first = (blk->next != -2) ? blk->next : -1; // update the new first block

		mark_buffer_dirty(bh);
		
		brelse(bh);
        bh = NULL;
		blk = NULL;

		m->last = (idx == m->last) ? -1 : m->last; // update the map with the new last block if the removed one was the last

		return 0;

	}

	// for the other cases get the previouse block of the block idx
    do{

		// get the buffer_head
		bh = (struct buffer_head *)sb_bread(sb, 2 + fk);
		if(!bh){
			return -EIO;
		}

		blk = (struct block*)bh->b_data;

		//printk(KERN_INFO "%s: reloaded block %ld informations\n", MOD_NAME, fk);

		if(blk->next == idx){

            bh_cur = (struct buffer_head *)sb_bread(sb, 2 + idx);
            if(!bh_cur){
                return -EIO;
            }
            cur_blk = (struct block*)bh_cur->b_data;

            blk->next = cur_blk->next;

			mark_buffer_dirty(bh);

            __sync_fetch_and_sub(&m->keys[idx], 1); // atomic block reservation for the write
			__sync_fetch_and_sub(&m->num_of_valid_blocks, 1);

            brelse(bh_cur);
            bh_cur = NULL;
	        cur_blk = NULL;


            break;
        }

		fk = blk->next;

		brelse(bh);
        bh = NULL;
		blk = NULL;

	}while (fk >= 0);

	// update the map with the new last block if the removed one was the last
	if(idx == m->last)
		m->last = fk;
    
    return fk;

}