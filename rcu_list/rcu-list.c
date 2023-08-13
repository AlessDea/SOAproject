#include <linux/version.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/buffer_head.h>
#include <linux/wait.h>
#include <linux/signal.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/module.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/signal.h>
#endif

#include "../src/helper.h"

#define SCALING (1000)  // please thake this value from CONFIG_HZ in your kernel config file 

static int shutdown_daemon = 0;// this can be configured at run time via the sys file system - 1 lead to daemon thread shutdown 
module_param(shutdown_daemon,int,0660);

static int shutdown_main = 0;// this can be configured at run time via the sys file system - 1 lead to daemon thread shutdown 
module_param(shutdown_main,int,0660);

static int sleep_enabled = 1;// this can be configured at run time via the sys file system 
module_param(sleep_enabled,int,0660);

static int timeout = 1;// this can be configured at run time via the sys file system 
module_param(timeout,int,0660);

// wait_queue_head_t *main_queue;
// wait_queue_head_t *my_sleep_queue;

DECLARE_WAIT_QUEUE_HEAD(main_queue);
DECLARE_WAIT_QUEUE_HEAD(my_sleep_queue);


int house_keeper(void * the_list);

void rcu_list_free(rcu_list * l){
	element *p, *tmp;

	//stop the house keeper kernel thread
	//atomic_inc((atomic_t*)&shutdown_daemon);
	shutdown_daemon = 1;

	wait_event_interruptible(main_queue, shutdown_main == 1);
	if(shutdown_main == 1){
		printk(KERN_INFO "%s: kernel thread deamon stopped\n", MOD_NAME);		
		//module_put(THIS_MODULE);
		return;
	}

	if(signal_pending(current)){
		printk(KERN_INFO "%s: kernel thread deamon killed\n",MOD_NAME);
		//module_put(THIS_MODULE);
		return;
	}
	

	//msleep(5000);

	write_lock(&(l->write_lock));

	p = l->head;
	while(p != NULL){
		tmp = p->next;
		kfree(p);
		p = tmp;
	}

	write_unlock(&(l->write_lock));

	// kfree(l); NO: dev_map non è mai stata allocata con kmalloc

	printk(KERN_INFO "%s: rcu-list freed\n", MOD_NAME);



}

void rcu_list_init(rcu_list * l){

	int i;
    struct task_struct *ts;

	l->epoch = 0x0;
	l->next_epoch_index = 0x1;
	for(i=0;i<EPOCHS;i++){
		l->standing[i] = 0x0;
	}
    for(i = 0; i < NBLOCKS; i++){
        l->keys[i] = 0;
    }
	l->head = NULL;
	//pthread_spin_init(&l->write_lock,PTHREAD_PROCESS_PRIVATE);
    rwlock_init(&(l->write_lock));


    ts = kthread_run(house_keeper, (void *)&dev_map, "house-keeper-deamon");
    if (ts == NULL){//this thread can be activated
                                    //using some different solution
        printk(KERN_INFO "%s: application startup error - RCU-list house-keeper not activated\n", MOD_NAME);
		wake_up_process(ts);
	
	}else{
		
		// module_put(THIS_MODULE);
	}


}


int rcu_list_reload(rcu_list * l){

    struct task_struct *ts;

	struct buffer_head *bh;
	struct block *blk;

	int lk; //last's key

	element *p, *next;


	lk = l->last;
	next = NULL;

	do{

		// get the buffer_head
		bh = (struct buffer_head *)sb_bread(my_bdev_sb, 2 + lk);
		if(!bh){
			return -EIO;
		}

		blk = (struct block*)bh->b_data;

		// check if the block is valid
		// if the block is valid get the next field and go to that block
		if(IS_VALID(blk->metadata) == CHCECK_V_MASK){ // this check is useless if all the update logic of the blocks has been implemented in the right way
			//the block is valid
			p = kmalloc(sizeof(element), GFP_KERNEL);

			if(!p) return 0;

			p->key = lk;
			p->next = next;
			p->validity = 1;

			next = p;

			printk(KERN_INFO "%s: reloaded block %ld informations\n", MOD_NAME, p->key);

		}

		lk = blk->prev;
			
	}while (lk != -1);
		
	brelse(bh);

	l->head->next = next;

	ts = kthread_run(house_keeper, (void *)&dev_map, "house-keeper-deamon");
    if (ts == NULL){//this thread can be activated
                                    //using some different solution
            printk(KERN_INFO "%s: application startup error - RCU-list house-keeper not activated\n", MOD_NAME);
			wake_up_process(ts);
	}else{
		
		// module_put(THIS_MODULE);
	}

	return 0;

}


int rcu_list_is_valid(rcu_list *l, long key){

	unsigned long * epoch = &(l->epoch);
	unsigned long my_epoch;
	element *p;
	int index;

	my_epoch = __sync_fetch_and_add(epoch,1);
	
	//actual list traversal		
	p = l->head;
	while(p!=NULL){
		if ( p->key == key){
			break;
		}
		p = p->next;
	}

	
	index = (my_epoch & MASK) ? 1 : 0; //get_index(my_epoch);
	__sync_fetch_and_add(&l->standing[index],1);

	if (p){ // the block exists, it's valid
        return 1;
    }

    // the block is not valid
	return 0;

}



int rcu_list_first_free(rcu_list *l){

    unsigned long * epoch = &(l->epoch);
    unsigned long my_epoch;
    int index;
    int i;

    my_epoch = __sync_fetch_and_add(epoch,1);

    write_lock(&(l->write_lock));

    for(i = 0; i < NBLOCKS; i++){
        if(l->keys[i] == 0){
            __sync_fetch_and_add(&l->keys[i], 1); // atomic block reservation for the write
            break;
        }
    }

    write_unlock(&(l->write_lock));


    index = (my_epoch & MASK) ? 1 : 0; //get_index(my_epoch);
    __sync_fetch_and_add(&l->standing[index],1);

    if (i < NBLOCKS){ // the block exists and it's valid
        return i;
    }

    // no valid blocks
    return -1;

}

long rcu_list_get_first_valid(rcu_list *l){
    unsigned long * epoch = &(l->epoch);
    unsigned long my_epoch;
    element *p;
    int index;

    my_epoch = __sync_fetch_and_add(epoch,1);

    // arrive at start_key (the required one)
    p = l->head->next;


    index = (my_epoch & MASK) ? 1 : 0; //get_index(my_epoch);
    __sync_fetch_and_add(&l->standing[index],1);

    if (p) return p->key;

    return -1; //no valid blocks

}




long rcu_list_next_valid(rcu_list *l, long start_key){
    unsigned long * epoch = &(l->epoch);
    unsigned long my_epoch;
    element *p;
    int index;

    my_epoch = __sync_fetch_and_add(epoch,1);

    // arrive at start_key (the required one)
    p = l->head;
    while(p!=NULL){
        if ( p->key == start_key){
            break;
        }
        p = p->next;
    }

    p = p->next;


    index = (my_epoch & MASK) ? 1 : 0; //get_index(my_epoch);
    __sync_fetch_and_add(&l->standing[index],1);

    if (p) return p->key;

    return -1; //no valid blocks
}



int rcu_list_insert(rcu_list *l, long key){

	element *p, *tmp;

	p = kmalloc(sizeof(element), GFP_KERNEL);

	if(!p) return 0;

	p->key = key;
	p->next = NULL;
    p->validity = 1;

	AUDIT
	printk(KERN_INFO "%s: insertion: waiting for lock\n", MOD_NAME);

    write_lock(&(l->write_lock));

	l->last = key;
	AUDIT
	printk(KERN_INFO "%s: last key update: %ld\n", MOD_NAME, key);


    //traverse and insert
    tmp = l->head;
    while(tmp->next != NULL){
        tmp = tmp->next;
    }


	tmp->next = p;
	asm volatile("mfence");

	while(p){
		AUDIT
		printk(KERN_INFO "%s: elem %ld\n", MOD_NAME, p->key);
		p = p->next;
	}

    write_unlock(&l->write_lock);
	
	return 1;

}





int rcu_list_remove(rcu_list *l, long key){

	element * p, *removed = NULL;
	unsigned long last_epoch;
	unsigned long updated_epoch;
	unsigned long grace_period_threads;
	int index;


    write_lock(&l->write_lock);

	//traverse and delete
	p = l->head;

	//check if it's the head to be removed
	if(p != NULL && p->key == key){
		removed = p;
		l->head = removed->next;
		asm volatile("mfence");//make it visible to readers
	}
	else{
		while(p != NULL){
			if ( p->next != NULL && p->next->key == key){
				if(p->next->next == NULL){ //if it is the last block then update field 'last'
					l->last = p->key; 
				}
                __sync_fetch_and_sub(&l->keys[key], 1); // atomic block free key for new use
				removed = p->next;
				p->next = p->next->next;

				asm volatile("mfence");//make it visible to readers
				break;
			}	
			p = p->next;	
		}
	}
	//move to a new epoch - still under write lock
	updated_epoch = (l->next_epoch_index) ? MASK : 0;

    l->next_epoch_index += 1;
	l->next_epoch_index %= 2;	

	last_epoch = __atomic_exchange_n (&(l->epoch), updated_epoch, __ATOMIC_SEQ_CST); 
	index = (last_epoch & MASK) ? 1 : 0; 
	grace_period_threads = last_epoch & (~MASK); 

	AUDIT
	printk(KERN_INFO "%s: deletion: waiting grace-full period (target value is %lu)\n",MOD_NAME, grace_period_threads);
	while(l->standing[index] < grace_period_threads);
	l->standing[index] = 0;



    write_unlock(&l->write_lock);

	if(removed){
		kfree(removed);
		return 1;
	}
	
	return 0;

}


int house_keeper(void * the_list){
	unsigned long last_epoch;
	unsigned long updated_epoch;
	unsigned long grace_period_threads;
	int index;

	rcu_list * l = (rcu_list*) the_list;

	// DECLARE_WAIT_QUEUE_HEAD(my_sleep_queue);

	allow_signal(SIGKILL);
	allow_signal(SIGTERM);

	

redo:

	wait_event_interruptible_timeout(my_sleep_queue,!sleep_enabled,timeout*SCALING);

	if(shutdown_daemon){
		printk(KERN_INFO "%s: deamon thread (pid = %d) - ending execution\n", MOD_NAME, current->pid);
		//atomic_inc((atomic_t*)&shutdown_main);
		shutdown_main = 1;
		wake_up(&main_queue);
		return 0;
	}


	if(signal_pending(current)){
		printk(KERN_INFO "%s: deamon thread (pid = %d) - killed\n",MOD_NAME,current->pid);
		//atomic_inc((atomic_t*)&shutdown_main);
		shutdown_main = 1;
		wake_up(&main_queue);
		return -1;
	}

	//msleep(PERIOD*1000); //*1000 is 2 seconds

    write_lock(&l->write_lock);
	
	updated_epoch = (l->next_epoch_index) ? MASK : 0;
	//printk("next epoch index is %d - next epoch is %p\n",l->next_epoch_index,updated_epoch);

    l->next_epoch_index += 1;
	l->next_epoch_index %= 2;	

	last_epoch = __atomic_exchange_n (&(l->epoch), updated_epoch, __ATOMIC_SEQ_CST); 
	index = (last_epoch & MASK) ? 1 : 0; 
	grace_period_threads = last_epoch & (~MASK); 

	AUDIT
	printk(KERN_INFO "%s: house keeping: waiting grace-full period (target value is %lu)\n", MOD_NAME, grace_period_threads);
	while(l->standing[index] < grace_period_threads);
	l->standing[index] = 0;

    write_unlock(&l->write_lock);

	goto redo;

	return 0;
}

