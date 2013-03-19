/* file.c: rffs MMU-based file operations
 *
 * Resizable simple ram filesystem for Linux.
 *
 * Copyright (C) 2013 Jinglei Ren <jinglei.ren@stanzax.org>
 *               2000 Linus Torvalds.
 *               2000 Transmeta Corp.
 *
 * Usage limits added by David Gibson, Linuxcare Australia.
 * This file is released under the GPL.
 */

#include <linux/fs.h>
#include <linux/aio.h>
#include <linux/page-flags.h>

#include "rffs.h"

static int __set_page_dirty_no_writeback(struct page *page)
{
	if (!PageDirty(page))
		return !TestSetPageDirty(page);
	return 0;
}

static ssize_t rffs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
	struct file *filp = iocb->ki_filp;
	struct inode *inode = filp->f_mapping->host;
	ssize_t written = generic_file_aio_write(iocb, iov, nr_segs, pos);
	printk(KERN_INFO "[sestet-aw]\t%lu\t%lld\t%d\n",
		inode->i_ino, pos, written);
	return written;
}

static ssize_t
rffs_file_splice_write(struct pipe_inode_info *pipe, struct file *out,
		loff_t *ppos, size_t len, unsigned int flags)
{
	struct inode *inode = out->f_mapping->host;
        printk(KERN_INFO "[sestet-sw]\t%lu\t%lld\t%d\n",
                inode->i_ino, *ppos, len);
	return generic_file_splice_write(pipe, out, ppos, len, flags);
}

const struct address_space_operations rffs_aops = {
	.readpage	= simple_readpage,
	.write_begin	= simple_write_begin,
	.write_end	= simple_write_end,
	.set_page_dirty = __set_page_dirty_no_writeback,
};

const struct file_operations rffs_file_operations = {
	.read		= do_sync_read,
	.aio_read	= generic_file_aio_read,
	.write		= do_sync_write,
	.aio_write	= rffs_file_aio_write,
	.mmap		= generic_file_mmap,
	.fsync		= noop_fsync,
	.splice_read	= generic_file_splice_read,
	.splice_write	= rffs_file_splice_write,
	.llseek		= generic_file_llseek,
};

const struct inode_operations rffs_file_inode_operations = {
	.setattr	= simple_setattr,
	.getattr	= simple_getattr,
};

