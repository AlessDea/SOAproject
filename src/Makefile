obj-m += singlefilefs.o
singlefilefs-objs += singlefilefs_src.o file.o dir.o ../rcu_list/rcu-list.o ../scth/scth.o ../scth/newsyscalls.o ../scth/lib/sys_tbl_finder.o ../scth/lib/vtpmo.o

NBLOCKS:=10
DEVNAME:=image
MOUNTPOINT:=./mount/


all:
	gcc singlefilemakefs.c -o singlefilemakefs
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

create-fs:
	dd bs=4096 count=$(NBLOCKS) if=/dev/zero of=$(DEVNAME)
	./singlefilemakefs image
	
mount-fs:
	mkdir mount
	mount -o loop -t singlefilefs $(DEVNAME) $(MOUNTPOINT)

ins:
	insmod singlefilefs.ko

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm singlefilemakefs