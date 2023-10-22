# include makefile.env

obj-m += singlefilefs.o
singlefilefs-objs += singlefilefs_src.o file.o dir.o helper.o ../scth/scth.o ../scth/newsyscalls.o ../scth/lib/sys_tbl_finder.o ../scth/lib/vtpmo.o

NBLOCKS:=10
DEVNAME:=image
MOUNTPOINT:=./mount/
SYNCHRONOUS_WRITE:=1

all:
	gcc singlefilemakefs.c -o singlefilemakefs
	make CFLAGS_MODULE='-DNBLOCKS=$(NBLOCKS) -DSYNCHRONOUS_WRITE=$(SYNCHRONOUS_WRITE)' -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

create-fs:
	dd bs=4096 count=$(NBLOCKS) if=/dev/zero of=$(DEVNAME)
	./singlefilemakefs $(DEVNAME)
	
mount-fs:
	mkdir mount
	mount -o loop -t singlefilefs $(DEVNAME) $(MOUNTPOINT)

ins:
	insmod singlefilefs.ko

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm singlefilemakefs