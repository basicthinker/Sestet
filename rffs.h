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

#include "log.h"

#define MAX_LOG_NUM 20

struct kiocb;
struct inovec;
struct flush_operations;
extern struct rffs_log rffs_logs[MAX_LOG_NUM];

#ifdef RFFS_DEBUG
	#define RFFS_TRACE(...) printk(__VA_ARGS__)
#else
	#define RFFS_TRACE(...)
#endif

#define RLOG_HASH_BITS 10

/* Replacements */
extern ssize_t rffs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos);

extern int rffs_sync_file(struct file *file, int datasync);

/* Hooks */
extern int rffs_init_hook(const struct flush_operations *fops);
extern void rffs_exit_hook(void);

static inline void rffs_new_inode_hook(struct inode *dir, struct inode *new_inode)
{
	struct log_entry le;

	if (dir) {
		new_inode->i_private = dir->i_private;
		RFFS_TRACE(KERN_INFO "[rffs] new_inode_hook: %lu->%lu to %lu\n",
				dir->i_ino, (unsigned long)dir->i_private, new_inode->i_ino);
	}

	le.inode_id = new_inode->i_ino;
	le.index = LE_META_NEW;
	log_append(rffs_logs + (unsigned int)(long)new_inode->i_private,
			&le, NULL);
}

static inline void rffs_free_inode_hook(struct inode *inode)
{
	struct log_entry le;
	struct rffs_log *log = rffs_logs + (unsigned int)(long)inode->i_private;
	struct transaction *tran;
	unsigned int ei;
	le.inode_id = inode->i_ino;
	le.index = LE_META_RM;
	log_append(log, &le, &ei);

	spin_lock(&log->l_lock);
	tran = __log_tail_tran(log);
	if (ei < tran->l_meta_min) tran->l_meta_min = ei;
	spin_unlock(&log->l_lock);
}

static inline void rffs_rename_hook(struct inode *new_dir, struct inode *old_inode)
{
	old_inode->i_private = new_dir->i_private;
	RFFS_TRACE(KERN_INFO "[rffs] rename_hook: %lu->%lu to %lu\n",
			new_dir->i_ino, (unsigned long)new_dir->i_private, old_inode->i_ino);
}

// Put before freeing in-mapping pages


/*
 * Internal things.
 * Used by RFFS implementation.
 */

extern struct task_struct *rffs_flusher;

extern struct kmem_cache *rffs_rlog_cachep;

extern struct shashtable *page_rlog;

#endif /* RFFS_H_ */
