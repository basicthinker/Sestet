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

struct kiocb;
struct inovec;

/* Replacements */
extern ssize_t rffs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos);

/* Hooks */
extern int rffs_init_hook(void);
extern void rffs_exit_hook(void);

static inline void rffs_new_inode_hook(struct inode *dir, struct inode *new_inode)
{
	if (dir) {
		new_inode->i_private = dir->i_private;
#ifdef RFFS_DEBUG
		printk(KERN_INFO "[rffs] new_inode_hook: %lu->%lu to %lu\n",
				dir->i_ino, (unsigned long)dir->i_private, new_inode->i_ino);
#endif
	}
}

static inline void rffs_rename_hook(struct inode *new_dir, struct inode *old_inode)
{
	old_inode->i_private = new_dir->i_private;
#ifdef RFFS_DEBUG
	printk(KERN_INFO "[rffs] rename_hook: %lu->%lu to %lu\n",
			new_dir->i_ino, (unsigned long)new_dir->i_private, old_inode->i_ino);
#endif
}

#ifdef RFFS_DEBUG
	#define inline
#endif

#endif /* RFFS_H_ */
