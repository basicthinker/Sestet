/*
 * rffs.h
 *
 *  Created on: May 20, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 *  Copyright (C) 2013 Microsoft Research Asia. All rights reserved.
 */

#ifndef RFFS_H_
#define RFFS_H_

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kthread.h>

struct kiocb;
struct inovec;
struct flush_operations;

#ifdef RFFS_DEBUG
	#define RFFS_TRACE(...) printk(__VA_ARGS__)
#else
	#define RFFS_TRACE(...)
#endif

/* Replacements */
extern ssize_t rffs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos);

extern int rffs_sync_file(struct file *file, int datasync);

/* Hooks */
extern int rffs_init_hook(const struct flush_operations *fops);
extern void rffs_exit_hook(void);

static inline void rffs_new_inode_hook(struct inode *dir, struct inode *new_inode)
{
	if (dir) {
		new_inode->i_private = dir->i_private;
		RFFS_TRACE(KERN_INFO "[rffs] new_inode_hook: %lu->%lu to %lu\n",
				dir->i_ino, (unsigned long)dir->i_private, new_inode->i_ino);
	}
}

static inline void rffs_rename_hook(struct inode *new_dir, struct inode *old_inode)
{
	old_inode->i_private = new_dir->i_private;
	RFFS_TRACE(KERN_INFO "[rffs] rename_hook: %lu->%lu to %lu\n",
			new_dir->i_ino, (unsigned long)new_dir->i_private, old_inode->i_ino);
}


/*
 * Internal things.
 * Used by RFFS implementation.
 */

extern struct task_struct *rffs_flusher;

#endif /* RFFS_H_ */
