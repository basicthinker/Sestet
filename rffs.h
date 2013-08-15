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
#define RLOG_HASH_BITS 10

struct kiocb;
struct flush_operations;
extern struct rffs_log rffs_logs[MAX_LOG_NUM];
extern struct task_struct *rffs_flusher;
extern struct kmem_cache *rffs_rlog_cachep;
extern struct shashtable *page_rlog;


/* Hooks */
extern int rffs_init_hook(const struct flush_operations *fops, struct kset *kset);
extern void rffs_exit_hook(void);

static inline void rffs_new_inode_hook(struct inode *dir, struct inode *new_inode, int mode)
{
	if (dir) {
		new_inode->i_private = dir->i_private;
		RFFS_TRACE(KERN_INFO "[rffs] new_inode_hook(): dir_ino=%lu, new_ino=%lu, log(%lu)\n",
				dir->i_ino, new_inode->i_ino, (unsigned long)dir->i_private);
	}

	if (S_ISREG(mode)) {
		struct log_entry le = LE_INITIALIZER;
		le_set_ino(&le, new_inode->i_ino);
		le_set_meta(&le);
		log_append(rffs_logs + (unsigned int)(long)new_inode->i_private,
				&le, NULL);
	}
}

static inline struct rlog *rffs_try_assoc_rlog(struct inode *host,
		struct page* page)
{
	struct rlog *rl;
	unsigned int li = (unsigned int)(long)host->i_private;
	struct rffs_log *log = rffs_logs + li;

	BUG_ON(li > MAX_LOG_NUM);

	rl = find_rlog(page_rlog, page);

	if (!rl) { // new page
        rl = rlog_malloc();
        assoc_rlog(rl, page, L_NULL, page_rlog);
#ifdef DEBUG_PRP
        printk(KERN_DEBUG "[rffs] NP 1: %p\n", rl_page(rl));
#endif
	} else if (L_LESS(rl_enti(rl), log->l_head)) { // COW
		struct page *cpage = page_cache_alloc_cold(&host->i_data);
		void *vfrom, *vto;
		struct log_entry *le;
		struct rlog* nrl;

		vfrom = kmap_atomic(page, KM_USER0);
		vto = kmap_atomic(cpage, KM_USER1);
		copy_page(vto, vfrom);
		kunmap_atomic(vfrom, KM_USER0);
		kunmap_atomic(vto, KM_USER1);
		cpage->mapping = page->mapping; // for retrieval of inode later on

		nrl = rlog_malloc();
		assoc_rlog(nrl, cpage, rl_enti(rl), page_rlog);

		le = L_ENT(log, rl_enti(nrl));
		le_set_ref(le, cpage);
		le_set_cow(le);

#ifdef DEBUG_PRP
		printk(KERN_DEBUG "[rffs] COW 1: %p\n", rl_page(rl));
#endif
	}
#ifdef DEBUG_PRP
	else printk(KERN_DEBUG "[rffs] AP 1: %p - %u - %u\n", rl_page(rl), rl_enti(rl), log->l_head);
#endif

	return rl;
}

static inline int rffs_try_append_log(struct inode *host, struct rlog* rl,
		unsigned long offset, unsigned long copied)
{
	int err = 0;
	unsigned int li = (unsigned int)(long)host->i_private;
	struct rffs_log *log = rffs_logs + li;

	BUG_ON(li > MAX_LOG_NUM);

	if (rl_enti(rl) != L_NULL && L_NG(log->l_head, rl_enti(rl))) { // active page
		struct transaction *tran = __log_tail_tran(log);
		le_set_len(L_ENT(log, rl_enti(rl)), offset + copied);
#ifdef DEBUG_PRP
		printk(KERN_DEBUG "[rffs] AP 2: %p - %u - %u\n", rl_page(rl), rl_enti(rl), log->l_head);
#endif
		on_write_old_page(log, tran->stat, copied);
	} else {
		unsigned int ei, pgv;
		struct log_entry le = LE_INITIALIZER;
		struct transaction *tran;

		le_set_ino(&le, host->i_ino);
		le_init_pgi(&le, rl_page(rl)->index);
		le_init_len(&le, offset + copied);
		le_set_ref(&le, rl_page(rl));
		if (rl_enti(rl) != L_NULL) { // COW page
			pgv = le_ver(L_ENT(log, rl_enti(rl)));
			le_set_ver(&le, pgv + 1);
#ifdef DEBUG_PRP
			printk(KERN_DEBUG "[rffs] COW 2: %p - %d\n", rl_page(rl), pgv);
#endif
		}
#ifdef DEBUG_PRP
		else printk(KERN_DEBUG "[rffs] NP 2: %p\n", rl_page(rl));
#endif
		err = log_append(log, &le, &ei);
		if (unlikely(err)) return err;
		rl_set_enti(rl, ei);

		tran = __log_tail_tran(log);
		on_write_new_page(log, tran->stat, copied);
	}
	return err;
}

// Put before truncate and free related pages
static inline void rffs_truncate_hook(struct inode *inode, loff_t newsize)
{
	struct rffs_log *log = rffs_logs + (unsigned int)(long)inode->i_private;
	struct address_space *mapping = inode->i_mapping;
	unsigned int i;
	struct pagevec pvec;
	pgoff_t pgi, start, next;
	struct page *page;
	struct rlog *rl;
	struct log_entry *le;

	RFFS_TRACE(KERN_INFO "[rffs] rffs_truncate_hook() for ino=%lu, newsize=%lu\n",
			inode->i_ino, (unsigned long)newsize);

	start = (newsize + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;

	spin_lock(&log->l_lock);
	for (i = L_END(log) - 1;
			L_NG(log->l_begin, i); --i) {
		le = L_ENT(log, i);
		if (le_inval(le) || le_ino(le) != inode->i_ino)
			continue;
		if (le_meta(le)) {
			RFFS_TRACE(KERN_INFO "[rffs] rffs_truncate_hook() breaks at %u\n", i);
			break;
		}
		if (le_pgi(le) >= start)
			le_set_inval(le);
	}
	spin_unlock(&log->l_lock);

	pagevec_init(&pvec, 0);
	next = start;
	while (pagevec_lookup(&pvec, mapping, next, PAGEVEC_SIZE)) {
		for (i = 0; i < pagevec_count(&pvec); i++) {
			page = pvec.pages[i];
			pgi = page->index;
			if (pgi > next)
				next = pgi;
			next++;
			// TODO: check page reference first
			rl = find_rlog(page_rlog, page);
			if (rl) evict_rlog(rl);
		}
		pagevec_release(&pvec);
	}
}

// Put before freeing in-mapping pages
static inline void rffs_evict_inode_hook(struct inode *inode)
{
	RFFS_TRACE(KERN_INFO "[rffs] rffs_evict_inode_hook() for ino=%lu\n", inode->i_ino);

	rffs_truncate_hook(inode, 0);
}

static inline void rffs_rename_hook(struct inode *new_dir, struct inode *old_inode)
{
	old_inode->i_private = new_dir->i_private;
	RFFS_TRACE(KERN_INFO "[rffs] rename_hook(): %lu->%lu to %lu\n",
			new_dir->i_ino, (unsigned long)new_dir->i_private, old_inode->i_ino);
}

/* Replacements */
// This covers hooks rffs_try_assoc_rlog() and rffs_try_append_log(),
// and makes a reference implementation.
// All removed lines are commented by the heading "//".
extern ssize_t rffs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos);

extern int rffs_sync_file(struct file *file, int datasync);

#endif /* RFFS_H_ */
