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
extern struct page *rffs_grab_cache_page_write_begin(struct address_space *mapping,
					pgoff_t index, unsigned flags);

extern ssize_t rffs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos);

/* Hooks */
extern int rffs_init_hook(void);
extern void rffs_exit_hook(void);

extern int rffs_fill_super_hook(struct inode *root_inode);

static void rffs_new_inode_hook(struct inode *dir, struct inode *new_inode)
{
	if (dir) {
		new_inode->i_private = dir->i_private;
#ifdef DEBUG_INODE_HOOK
		printk(KERN_INFO "[rffs] new_inode_hook: %lu->%d to %lu\n",
				dir->i_ino, (int)dir->i_private, new_inode->i_ino);
#endif
	}
}

static void rffs_rename_hook(struct inode *new_dir, struct inode *old_inode)
{
	old_inode->i_private = new_dir->i_private;
#ifdef DEBUG_INODE_HOOK
	printk(KERN_INFO "[rffs] rename_hook: %lu->%d to %lu\n",
			new_dir->i_ino, (int)new_dir->i_private, old_inode->i_ino);
#endif
}

#endif /* RFFS_H_ */
