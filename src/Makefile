obj-m += singlefilefs.o
singlefilefs-objs += singlefilefs_src.o file.o dir.o ../rcu_list/rcu-list.o ../scth/scth.o ../scth/newsyscalls.o ../scth/lib/sys_tbl_finder.o ../scth/lib/vtpmo.o

NBLOCKS:=10

all:
	gcc singlefilemakefs.c -o singlefilemakefs
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm singlefilemakefs

create-fs:
	dd bs=4096 count=$(NBLOCKS) if=/dev/zero of=image
	./singlefilemakefs image
	
mount-fs:
	mkdir mount
	mount -o loop -t singlefilefs image ./mount/

ins:
	insmod singlefilefs.ko