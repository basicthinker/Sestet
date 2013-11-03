/*
 * adafs_file.c
 *
 *  Created on: May 20, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 *  Copyright (C) 2013 Microsoft Research Asia. All rights reserved.
 */

#include <linux/file.h>
#include <linux/blkdev.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/swap.h>
#include <linux/sched.h>
#include <linux/pagemap.h>
#include <asm/errno.h>

#include "ada_fs.h"
#include "ada_policy.h"

struct adafs_log *adafs_logs[MAX_LOG_NUM];
static atomic_t num_logs;

struct kmem_cache *adafs_rlog_cachep;

struct shashtable *page_rlog = &(struct shashtable)
		SHASHTABLE_UNINIT(RLOG_HASH_BITS);

struct task_struct *adafs_flusher;
struct completion flush_cmpl;

#define TRACE_FILE_PATH "/var/log/adafs.trace"
struct adafs_trace adafs_trace;

int adafs_flush(void *data)
{
	int i;
	struct adafs_log *log;
	while (!kthread_should_stop()) {
		INIT_COMPLETION(flush_cmpl);
		for (i = 0; i < atomic_read(&num_logs); ++i) {
			log = adafs_logs[i];
			while (log_flush(log, UINT_MAX) == -ENODATA &&
					log->l_head != log->l_end) {
				log_seal(log);
			}
		}
		complete_all(&flush_cmpl);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
	return 0;
}

int adafs_init_hook(const struct flush_operations *fops, struct kset *kset)
{
	int err;

	adafs_rlog_cachep = kmem_cache_create("adafs_rlog_cache", sizeof(struct rlog),
			0, (SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), NULL);
	adafs_tran_cachep = kmem_cache_create("adafs_tran_cachep", sizeof(struct transaction),
			0, (SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), NULL);
	if (!adafs_rlog_cachep || !adafs_tran_cachep)
		return -ENOMEM;

	sht_init(page_rlog);

	atomic_set(&num_logs, 1);
	adafs_logs[0] = new_log(kset);

	err = kobject_init_and_add(&adafs_logs[0]->l_kobj, &adafs_la_ktype, NULL,
	            "log%d", 0);
	if (err) printk(KERN_ERR "[adafs] kobject_init_and_add() failed for log0\n");

	if (fops) flush_ops = *fops;

	adafs_flusher = kthread_run(adafs_flush, NULL, "adafs_flusher");
	init_completion(&flush_cmpl);
	if (IS_ERR(adafs_flusher)) {
		printk(KERN_ERR "[adafs] kthread_run() failed: %ld\n", PTR_ERR(adafs_flusher));
		return PTR_ERR(adafs_flusher);
	}

	err = adafs_trace_open(&adafs_trace, TRACE_FILE_PATH);
	if (err) printk(KERN_ERR "[adafs] adafs_trace_open() failed: %d\n", err);
	return 0;
}

void adafs_exit_hook(void)
{
	int i, j;
	struct sht_list *sl;
	struct hlist_head *hl;
	struct hlist_node *pos, *tmp;
	struct rlog *rl;

	if (kthread_stop(adafs_flusher) != 0) {
		printk(KERN_INFO "[adafs] adafs_flusher thread exits unclearly.\n");
	}

	i = 0;
	j = (1 << (RLOG_HASH_BITS - 3));
	for_each_hlist_safe(page_rlog, sl, hl) {
		hlist_for_each_entry_safe(rl, pos, tmp, hl, rl_hnode) {
			evict_rlog(rl);
			++i;
		}
		if (--j == 0) {
			printk(KERN_INFO "[adafs] adafs_exit_hook: evicted rlogs num=%d\n", i);
			i = 0;
			j = (1 << (RLOG_HASH_BITS - 3));
		}
	}
	kmem_cache_destroy(adafs_rlog_cachep);

	for (i = 0; i < atomic_read(&num_logs); ++i) {
		log_destroy(adafs_logs[i]);
		kfree(adafs_logs[i]);
	}
	kmem_cache_destroy(adafs_tran_cachep);

	adafs_trace_close(&adafs_trace);
}

void adafs_put_super_hook(void)
{
#ifdef ADA_RELEASE
	struct adafs_log *log;
	int i;
	int done = 1;
	do {
		wake_up_process(adafs_flusher);
		if (wait_for_completion_interruptible(&flush_cmpl) < 0) {
			printk(KERN_ERR "[adafs] adafs_put_super_hook "
					"interrupted in waiting for flush_cmpl.\n");
			return;
		}
		for (i = 0; i < atomic_read(&num_logs); ++i) {
			log = adafs_logs[i];
			if (log->l_begin != log->l_end) done = 0;
		}
	} while(!done);
#endif
}

// mm/filemap.c
// generic_perform_write
static ssize_t adafs_perform_write(struct file *file,
				struct iov_iter *i, loff_t pos)
{
	struct address_space *mapping = file->f_mapping;
	const struct address_space_operations *a_ops = mapping->a_ops;
	long status = 0;
	ssize_t written = 0;
	unsigned int flags = 0;

	/*
	 * Copies from kernel address space cannot fail (NFSD is a big user).
	 */
	if (segment_eq(get_fs(), KERNEL_DS))
		flags |= AOP_FLAG_UNINTERRUPTIBLE;

	do {
		struct page *page;
		unsigned long offset;	/* Offset into pagecache page */
		unsigned long bytes;	/* Bytes to write to page */
		size_t copied;		/* Bytes copied from user */
		void *fsdata;

		struct rlog *rl = NULL;
		int re_entry = 0;

		offset = (pos & (PAGE_CACHE_SIZE - 1));
		bytes = min_t(unsigned long, PAGE_CACHE_SIZE - offset,
						iov_iter_count(i));

again:

		/*
		 * Bring in the user page that we will copy from _first_.
		 * Otherwise there's a nasty deadlock on copying from the
		 * same page as we're writing to, without it being marked
		 * up-to-date.
		 *
		 * Not only is this an optimisation, but it is also required
		 * to check that the address is actually valid, when atomic
		 * usercopies are used, below.
		 */
		if (unlikely(iov_iter_fault_in_readable(i, bytes))) {
			status = -EFAULT;
			break;
		}

		status = a_ops->write_begin(file, mapping, pos, bytes, flags,
						&page, &fsdata);
		if (unlikely(status))
			break;

		if (mapping_writably_mapped(mapping))
			flush_dcache_page(page);

		/* AdaFS */
		if (!re_entry) rl = adafs_try_assoc_rlog(mapping->host, page);

		pagefault_disable();
		copied = iov_iter_copy_from_user_atomic(page, i, offset, bytes);
		pagefault_enable();
		flush_dcache_page(page);

		mark_page_accessed(page);
		status = a_ops->write_end(file, mapping, pos, bytes, copied,
						page, fsdata);
		if (unlikely(status < 0))
			break;
		copied = status;

		cond_resched();

		iov_iter_advance(i, copied);
		if (unlikely(copied == 0)) {
			/*
			 * If we were unable to copy any data at all, we must
			 * fall back to a single segment length write.
			 *
			 * If we didn't fallback here, we could livelock
			 * because not all segments in the iov can be copied at
			 * once without a pagefault.
			 */
			bytes = min_t(unsigned long, PAGE_CACHE_SIZE - offset,
						iov_iter_single_seg_count(i));
			re_entry = 1;
			goto again;
		}
		pos += copied;
		written += copied;

		/* AdaFS */
		adafs_try_append_log(mapping->host, rl, offset, copied);
		adafs_trace_page(&adafs_trace, TE_WRITE,
				mapping->host->i_ino, page->index, TE_HIT_UNKNOWN);

//		balance_dirty_pages_ratelimited(mapping);

	} while (iov_iter_count(i));

	return written ? written : status;
}

// mm/filemap.c
// generic_file_buffered_write
static inline ssize_t adafs_file_buffered_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos, loff_t *ppos, size_t count,
		ssize_t written)
{
	struct file *file = iocb->ki_filp;
	ssize_t status;
	struct iov_iter i;

	iov_iter_init(&i, iov, nr_segs, count, written);
//	status = generic_perform_write(file, &i, pos);
	status = adafs_perform_write(file, &i, pos);

	if (likely(status >= 0)) {
		written += status;
		*ppos = pos + status;
	}

	return written ? written : status;
}

// mm/filemap.c
// __generic_file_aio_write
/**
 * This function does all the work needed for actually writing data to a
 * file. It does all basic checks, removes SUID from the file, updates
 * modification times and calls proper subroutines depending on whether we
 * do direct IO or a standard buffered write.
 *
 * It expects i_mutex to be grabbed unless we work on a block device or similar
 * object which does not need locking at all.
 *
 * This function does *not* take care of syncing data in case of O_SYNC write.
 * A caller has to handle it. This is mainly due to the fact that we want to
 * avoid syncing under i_mutex.
 */
static inline ssize_t __adafs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t *ppos)
{
	struct file *file = iocb->ki_filp;
	struct address_space *mapping = file->f_mapping;
	size_t ocount; /* original count */
	size_t count; /* after file limit checks */
	struct inode *inode = mapping->host;
	loff_t pos;
	ssize_t written;
	ssize_t err;

	ocount = 0;
	err = generic_segment_checks(iov, &nr_segs, &ocount, VERIFY_READ);
	if (err)
		return err;

	count = ocount;
	pos = *ppos;

	vfs_check_frozen(inode->i_sb, SB_FREEZE_WRITE);

	/* We can write back this queue in page reclaim */
	current->backing_dev_info = mapping->backing_dev_info;
	written = 0;

	err = generic_write_checks(file, &pos, &count, S_ISBLK(inode->i_mode));
	if (err)
		goto out;

	if (count == 0)
		goto out;

	err = file_remove_suid(file);
	if (err)
		goto out;

	file_update_time(file);

	/* coalesce the iovecs and go direct-to-BIO for O_DIRECT */
//	if (unlikely(file->f_flags & O_DIRECT)) {
//		...
//	} else {
//		written = generic_file_buffered_write(iocb, iov, nr_segs,
//				pos, ppos, count, written);
		written = adafs_file_buffered_write(iocb, iov, nr_segs,
				pos, ppos, count, written);
//	}
out:
	current->backing_dev_info = NULL;
	return written ? written : err;
}

// mm/filemap.c
// generic_file_aio_write
/**
 * This is a wrapper around __generic_file_aio_write() to be used by most
 * filesystems. It takes care of syncing the file in case of O_SYNC file
 * and acquires i_mutex as needed.
 */
ssize_t adafs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file->f_mapping->host;
	struct blk_plug plug;
	ssize_t ret;

	BUG_ON(iocb->ki_pos != pos);

	mutex_lock(&inode->i_mutex);
	blk_start_plug(&plug);
//	ret = __generic_file_aio_write(iocb, iov, nr_segs, &iocb->ki_pos);
	ret = __adafs_file_aio_write(iocb, iov, nr_segs, &iocb->ki_pos);
	mutex_unlock(&inode->i_mutex);

//	if (ret > 0 || ret == -EIOCBQUEUED) {
//		ssize_t err;
//
//		err = generic_write_sync(file, pos, ret);
//		if (err < 0 && ret > 0)
//			ret = err;
//	}
	blk_finish_plug(&plug);
	return ret;
}

