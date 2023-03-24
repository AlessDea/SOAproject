obj-m += singlefilefs.o
singlefilefs-objs += singlefilefs_src.o file.o dir.o ../rcu_list/rcu-list.o ../scth/scth.o ../scth/newsyscalls.o

A = $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address)
NBLOCKS:= 10

all:
	gcc singlefilemakefs.c -o singlefilemakefs
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm singlefilemakefs

create-fs:
	dd bs=4096 count=$(NBLOCKS) if=/dev/zero of=image
	./singlefilemakefs image
	mkdir mount
	
mount-fs:
	mount -o loop -t singlefilefs image ./mount/

int:
	insmod singlefilefs.ko the_syscall_table=$(A)