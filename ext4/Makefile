#
# Makefile for the linux ext4-filesystem routines.
#

obj-m += rffs.o

rffs-y	:= balloc.o bitmap.o dir.o file.o fsync.o ialloc.o inode.o page-io.o \
		ioctl.o namei.o super.o symlink.o hash.o resize.o extents.o \
		ext4_jbd2.o migrate.o mballoc.o block_validity.o move_extent.o \
		mmp.o

rffs-$(CONFIG_EXT4_FS_XATTR)		+= xattr.o xattr_user.o xattr_trusted.o
rffs-$(CONFIG_EXT4_FS_POSIX_ACL)	+= acl.o
rffs-$(CONFIG_EXT4_FS_SECURITY)		+= xattr_security.o

KSRC = /usr/src/linux-source-3.0.0

default:
	$(MAKE) -C $(KSRC) SUBDIRS=`pwd` modules

clean:
	$(MAKE) -C $(KSRC) SUBDIRS=`pwd` clean

