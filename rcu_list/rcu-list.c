#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include "../src/helper.h"

extern int main(int argc, char** argv);

void * house_keeper(void * the_list);

void rcu_list_init(rcu_list * l){

	int i;
	pthread_t tid;

	l->epoch = 0x0;
	l->next_epoch_index = 0x1;
	for(i=0;i<EPOCHS;i++){
		l->standing[i] = 0x0;
	}
    for(i = 0; i < NBLOCKS; i++){
        l->keys[i] = 0;
    }
	l->head = NULL;
	pthread_spin_init(&l->write_lock,PTHREAD_PROCESS_PRIVATE);

    //TODO: make it a kernel thread (deamon)
    if ((pthread_create(&tid,NULL,house_keeper,(void*)(&dev_map)) != 0)){//this thread can be activated
                                    //using some different solution
            errno = ESRCH;
            perror("application startup error - RCU-list house-keeper not activated\n");
            fflush(stderr);
    };

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
    element *p;
    int index;
    int i;

    my_epoch = __sync_fetch_and_add(epoch,1);

    pthread_spin_lock(&(l->write_lock));

    for(i = 0; i < NBLOCKS; i++){
        if(l->keys[i] == 0){
            __sync_fetch_and_add(&l->keys[i], 1); // atomic block reservation for the write
            break;
        }
    }

    pthread_spin_unlock(&(l->write_lock));


    index = (my_epoch & MASK) ? 1 : 0; //get_index(my_epoch);
    __sync_fetch_and_add(&l->standing[index],1);

    if (i < NBLOCKS){ // the block exists and it's valid
        return i;
    }

    // no valid blocks
    return -1;

}




int rcu_list_next_valid(rcu_list *l, long start_key){
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

    // from start_key check the first valid
    while (p!=NULL){
        if ( p->validity == 1){
            break;
        }
        p = p->next;
    }

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
	printf("insertion: waiting for lock\n");

	pthread_spin_lock(&(l->write_lock));

    //traverse and insert
    tmp = l->head;
    while(tmp->next != NULL){
        tmp = tmp->next;
    }


	tmp->next = p;
	asm volatile("mfence");

	AUDIT
	while(p){
		printf("elem %d\n",p->key);
		p = p->next;
	}

	pthread_spin_unlock(&l->write_lock);
	
	return 1;

}





int rcu_list_remove(rcu_list *l, long key){

	element * p, *removed = NULL;
	int grace_epoch;
	unsigned long last_epoch;
	unsigned long updated_epoch;
	unsigned long grace_period_threads;
	int index;
	

	pthread_spin_lock(&l->write_lock);

	//traverse and delete
	p = l->head;

	if(p != NULL && p->key == key){
		removed = p;
		l->head = removed->next;
		asm volatile("mfence");//make it visible to readers
	}
	else{
		while(p != NULL){
			if ( p->next != NULL && p->next->key == key){
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
	printf("deletion: waiting grace-full period (target value is %d)\n",grace_period_threads);
	while(l->standing[index] < grace_period_threads);
	l->standing[index] = 0;
    __sync_fetch_and_sub(&l->keys[key],1);


    pthread_spin_unlock(&l->write_lock);

	if(removed){
		free(removed);
		return 1;
	}
	
	return 0;

}


void * house_keeper(void * the_list){

	int grace_epoch;
	unsigned long last_epoch;
	unsigned long updated_epoch;
	unsigned long grace_period_threads;
	int index;

	rcu_list * l = (rcu_list*) the_list;

redo:
	sleep(PERIOD);

	pthread_spin_lock(&l->write_lock);
	
	updated_epoch = (l->next_epoch_index) ? MASK : 0;
	//printf("next epoch index is %d - next epoch is %p\n",l->next_epoch_index,updated_epoch);

    	l->next_epoch_index += 1;
	l->next_epoch_index %= 2;	

	last_epoch = __atomic_exchange_n (&(l->epoch), updated_epoch, __ATOMIC_SEQ_CST); 
	index = (last_epoch & MASK) ? 1 : 0; 
	grace_period_threads = last_epoch & (~MASK); 

	AUDIT
	printf("house keeping: waiting grace-full period (target value is %d)\n",grace_period_threads);
	while(l->standing[index] < grace_period_threads);
	l->standing[index] = 0;

	pthread_spin_unlock(&l->write_lock);

	goto redo;

	return NULL;
}

