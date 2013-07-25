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
#include <linux/pagevec.h>

#include "log.h"
#include "rlog.h"

#define MAX_LOG_NUM 20

struct kiocb;
struct flush_operations;
extern struct rffs_log rffs_logs[MAX_LOG_NUM];

#define RLOG_HASH_BITS 10

/*
 * Internal things.
 * Used by RFFS implementation.
 */

#ifdef RFFS_DEBUG
	#define RFFS_TRACE(...) printk(__VA_ARGS__)
#else
	#define RFFS_TRACE(...)
#endif

extern struct task_struct *rffs_flusher;

extern struct kmem_cache *rffs_rlog_cachep;

extern struct shashtable *page_rlog;

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

// Put before freeing in-mapping pages
static inline void rffs_evict_inode_hook(struct inode *inode)
{
	struct rffs_log *log = rffs_logs + (unsigned int)(long)inode->i_private;
    int i;
    struct pagevec pvec;
    pgoff_t page_index, next;
    struct page *page;
    struct rlog *rl;

	spin_lock(&log->l_lock);
	for (i = L_END(log) - 1; i >= log->l_begin; --i) {
		if (!ent_valid(L_ENT(log, i)) || L_ENT(log, i).inode_id != inode->i_ino)
			continue;
		ent_inval(L_ENT(log, i), LE_PAGE_EVICTED);
		if (is_meta_new(L_ENT(log, i))) break;
	}
	spin_unlock(&log->l_lock);

	pagevec_init(&pvec, 0);
	next = 0;
	while (pagevec_lookup(&pvec, &inode->i_data, next, PAGEVEC_SIZE)) {
		for (i = 0; i < pagevec_count(&pvec); i++) {
			page = pvec.pages[i];
			page_index = page->index;
			if (page_index > next)
				next = page_index;
			next++;

			rl = find_rlog(page_rlog, page);
			if (rl) {
				hlist_del(&rl->hnode);
				rlog_free(rl);
			}
		}
		pagevec_release(&pvec);
	}
}

static inline void rffs_rename_hook(struct inode *new_dir, struct inode *old_inode)
{
	old_inode->i_private = new_dir->i_private;
	RFFS_TRACE(KERN_INFO "[rffs] rename_hook: %lu->%lu to %lu\n",
			new_dir->i_ino, (unsigned long)new_dir->i_private, old_inode->i_ino);
}

#endif /* RFFS_H_ */
