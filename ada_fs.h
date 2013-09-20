/*
 * adafs.h
 *
 *  Created on: May 20, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 *  Copyright (C) 2013 Microsoft Research Asia. All rights reserved.
 */

#ifndef ADAFS_H_
#define ADAFS_H_

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/pagevec.h>

#include "ada_log.h"
#include "ada_rlog.h"
#include "ada_policy.h"

#define MAX_LOG_NUM 16
#define RLOG_HASH_BITS 10

struct kiocb;
extern struct adafs_log *adafs_logs[MAX_LOG_NUM];
extern struct task_struct *adafs_flusher;
extern struct kmem_cache *adafs_rlog_cachep;
extern struct shashtable *page_rlog;

struct flush_operations {
	handle_t *(*trans_begin)(int nent, void *arg);
	int (*entry_flush)(handle_t *handle,
			struct log_entry *le, struct writeback_control *wbc);
	int (*trans_end)(handle_t *handle);
	int (*wait_sync)(struct log_entry *ent, void *arg);
};

/* Hooks */
extern int adafs_init_hook(const struct flush_operations *fops, struct kset *kset);
extern void adafs_exit_hook(void);

static inline void adafs_new_inode_hook(struct inode *dir, struct inode *new_inode, int mode)
{
	if (dir) {
		new_inode->i_private = dir->i_private;
		ADAFS_DEBUG(KERN_INFO "[adafs] new_inode_hook(): dir_ino=%lu, new_ino=%lu, log(%lu)\n",
				dir->i_ino, new_inode->i_ino, (unsigned long)dir->i_private);
	}

	if (S_ISREG(mode)) {
		struct log_entry le = LE_INITIALIZER;
		struct adafs_log *log = adafs_logs[(long)new_inode->i_private];
		le_set_ino(&le, new_inode->i_ino);
		le_set_meta(&le);
		while (log_append(log, &le, NULL) == -EAGAIN) {
			log_seal(log);
			wake_up_process(adafs_flusher);
			wait_for_completion(&log->l_fcmpl);
		}
	}
}

static inline struct rlog *adafs_try_assoc_rlog(struct inode *host,
		struct page* page)
{
	struct rlog *rl;
	unsigned int li = (unsigned int)(long)host->i_private;
	struct adafs_log *log = adafs_logs[li];

	BUG_ON(li > MAX_LOG_NUM);

	rl = find_rlog(page_rlog, page);

	if (!rl) { // new page
        rl = rlog_malloc();
        assoc_rlog(rl, page, L_NULL, page_rlog);
#ifdef DEBUG_PRP
        printk(KERN_DEBUG "[adafs] NP 1: %p\n", rl_page(rl));
#endif
	} else if (seq_less(rl_enti(rl), log->l_head)) { // COW
		struct page *cpage = page_cache_alloc_cold(&host->i_data);
		void *vfrom, *vto;
		struct log_entry *le;
		struct rlog* nrl;

		vfrom = kmap_atomic(page, KM_USER0);
		vto = kmap_atomic(cpage, KM_USER1);
		copy_page(vto, vfrom);
		kunmap_atomic(vfrom, KM_USER0);
		kunmap_atomic(vto, KM_USER1);
		cpage = page; // for retrieval of surrounding data structures

		nrl = rlog_malloc();
		assoc_rlog(nrl, cpage, rl_enti(rl), page_rlog);

		le = L_ENT(log, rl_enti(nrl));
		le_set_ref(le, cpage);
		le_set_cow(le);

#ifdef DEBUG_PRP
		printk(KERN_DEBUG "[adafs] COW 1: %p\n", rl_page(rl));
#endif
	}
#ifdef DEBUG_PRP
	else printk(KERN_DEBUG "[adafs] AP 1: %p - %u - %u\n", rl_page(rl), rl_enti(rl), log->l_head);
#endif

	return rl;
}

static inline int adafs_try_append_log(struct inode *host, struct rlog* rl,
		unsigned long offset, unsigned long copied)
{
	int err = 0;
	unsigned int li = (unsigned int)(long)host->i_private;
	struct adafs_log *log = adafs_logs[li];

	BUG_ON(li > MAX_LOG_NUM);

	if (rl_enti(rl) != L_NULL && seq_ng(log->l_head, rl_enti(rl))) { // active page
		le_set_len(L_ENT(log, rl_enti(rl)), offset + copied);
#ifdef DEBUG_PRP
		printk(KERN_DEBUG "[adafs] AP 2: %p - %u - %u\n", rl_page(rl), rl_enti(rl), log->l_head);
#endif
		on_write_old_page(log, copied);
	} else {
		unsigned int ei = L_NULL, pgv;
		struct log_entry le = LE_INITIALIZER;

		le_set_ino(&le, host->i_ino);
		le_init_pgi(&le, rl_page(rl)->index);
		le_init_len(&le, offset + copied);
		le_set_ref(&le, rl_page(rl));
		if (rl_enti(rl) != L_NULL) { // COW page
			pgv = le_ver(L_ENT(log, rl_enti(rl)));
			le_set_ver(&le, pgv + 1);
#ifdef DEBUG_PRP
			printk(KERN_DEBUG "[adafs] COW 2: %p - %d\n", rl_page(rl), pgv);
#endif
		}
#ifdef DEBUG_PRP
		else printk(KERN_DEBUG "[adafs] NP 2: %p\n", rl_page(rl));
#endif
		while (log_append(log, &le, &ei) == -EAGAIN) {
			log_seal(log);
			wake_up_process(adafs_flusher);
			wait_for_completion(&log->l_fcmpl);
		}
		rl_set_enti(rl, ei);

		on_write_new_page(log, copied);
	}
	return err;
}

// Put before truncate and free related pages
static inline void adafs_truncate_hook(struct inode *inode,
		loff_t lstart, loff_t lend)
{
	struct adafs_log *log = adafs_logs[(long)inode->i_private];
	unsigned int i;
	pgoff_t start, end;
	struct log_entry *le;

	ADAFS_DEBUG(KERN_INFO "[adafs] adafs_truncate_hook() for ino=%lu, start=%lu, end=%lu\n",
			inode->i_ino, (unsigned long)lstart, (unsigned long)lend);

	start = (lstart + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	//BUG_ON((lend & (PAGE_CACHE_SIZE - 1)) != (PAGE_CACHE_SIZE - 1));
	end = (lend >> PAGE_CACHE_SHIFT);

	mutex_lock(&log->l_fmutex);
	for (i = log->l_end - 1;
			seq_ng(log->l_begin, i); --i) {
		le = L_ENT(log, i);
		if (le_inval(le) || le_ino(le) != inode->i_ino)
			continue;
		if (le_meta(le)) {
			le_set_inval(le);
			ADAFS_DEBUG(KERN_INFO "[adafs] adafs_truncate_hook() breaks at %u\n", i);
			break;
		}
		if (le_pgi(le) >= start && le_pgi(le) <= end) {
			le_set_inval(le);
			evict_entry(le, page_rlog);
			on_evict_page(log, le_len(le));
		}
	}
	mutex_unlock(&log->l_fmutex);
}

// Put before freeing in-mapping pages
static inline void adafs_evict_inode_hook(struct inode *inode)
{
	ADAFS_DEBUG(KERN_INFO "[adafs] adafs_evict_inode_hook() for ino=%lu\n", inode->i_ino);

	adafs_truncate_hook(inode, 0, (loff_t)-1);
}

static inline void adafs_rename_hook(struct inode *new_dir, struct inode *old_inode)
{
	old_inode->i_private = new_dir->i_private;
	ADAFS_DEBUG(KERN_INFO "[adafs] rename_hook(): %lu->%lu to %lu\n",
			new_dir->i_ino, (unsigned long)new_dir->i_private, old_inode->i_ino);
}

/* Replacements */
// This covers hooks adafs_try_assoc_rlog() and adafs_try_append_log(),
// and makes a reference implementation.
// All removed lines are commented by the heading "//".
extern ssize_t adafs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos);


/*
 * Cuts
 * Put at the beginning of the target function.
 * The containing function should check the return value.
 * If it is non-zero, the containing function should return.
 */
static inline int adafs_writepage_cut(struct page *page,
		struct writeback_control *wbc)
{
	struct inode *inode = page->mapping->host;
	int ret = 0;
	if (S_ISREG(inode->i_mode)) {
		/* Set the page dirty again, unlock */
		redirty_page_for_writepage(wbc, page);
		unlock_page(page);
		ret = 1;
	}
	return ret;
}

#define adafs_sync_file_cut(inode) (S_ISREG((inode)->i_mode) ? \
		printk(KERN_INFO "[adafs] adafs_sync_file_cut at %s\n", __func__), 1 : 0)

#endif /* ADAFS_H_ */
