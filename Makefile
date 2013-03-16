KSRC = ~/Documents/GT-I9260_CHN_JB/Kernel/

obj-m += rffs.o
rffs-objs := file.o inode.o log.o

default:
	$(MAKE) ARCH=arm CROSS_COMPILE=~/CodeSourcery/Sourcery_G++_Lite/bin/arm-none-linux-gnueabi- -C $(KSRC) SUBDIRS=`pwd` modules

clean:
	$(MAKE) -C $(KSRC) SUBDIRS=`pwd` clean
