#include <helper.h>

int reload_device_map(map *m){
    struct buffer_head *bh;
	struct block *blk;

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
    
    struct buffer_head *bh;
	struct block *blk;

    // search the first free
    for(long i = 0; i < NBLOCKS; i++){
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


long get_block_boundaries(map *m){
    struct buffer_head *bh;
	struct block *blk;

    // ...
}



int is_block_valid(map *m, long idx){
    return m->keys[idx];
}



int set_invalid_block(map *m, long idx){
    struct buffer_head *bh, *bh_nxt;
	struct block *blk, *blk_nxt;

 	long fk; //first key
    fk = m->first;

    long next_key;

    do{

		// get the buffer_head
		bh = (struct buffer_head *)sb_bread(sb, 2 + fk);
		if(!bh){
			return -EIO;
		}

		blk = (struct block*)bh->b_data;

		//printk(KERN_INFO "%s: reloaded block %ld informations\n", MOD_NAME, fk);

		if(blk->next == idx){

            bh_nxt = (struct buffer_head *)sb_bread(sb, 2 + idx);
            if(!bh_nxt){
                return -EIO;
            }
            blk_nxt = (struct block*)bh_nxt->b_data;

            blk->next = blk_nxt->next;

            __sync_fetch_and_sub(&m->keys[idx], 1); // atomic block reservation for the write
			__sync_fetch_and_sub(&m->num_of_valid_blocks, 1);

            brelse(bh_nxt);
            bh_nxt = NULL;
	        blk_nxt = NULL;

            break;
        }

		brelse(bh);
        bh = NULL;
		blk = NULL;

	}while (fk != -1);
    
    return fk;

}