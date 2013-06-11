/*
 * rffs_file.c
 *
 *  Created on: May 20, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 *  Copyright (C) 2013 Microsoft Research Asia. All rights reserved.
 */

#include "list.h" // overrides linux/list.h

#include <linux/file.h>
#include <linux/blkdev.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/swap.h>
#include <asm/errno.h>

#include "log.h"
#include "rlog.h"
#include "policy.h"
#include "rffs.h"

#define MAX_LOG_NUM 20

struct rffs_log rffs_logs[MAX_LOG_NUM];
static atomic_t logi;

struct kmem_cache *rffs_rlog_cachep;

DEFINE_HASHTABLE(page_rlog, RLOG_HASH_BITS);

int rffs_init_hook(void)
{
	atomic_set(&logi, 0);
	log_init(&rffs_logs[0]);
	rffs_rlog_cachep = kmem_cache_create("rffs_rlog_cache", sizeof(struct rlog),
			0, (SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), NULL);
	if (!rffs_rlog_cachep)
		return -ENOMEM;
	return 0;
}

void rffs_exit_hook(void)
{
	kmem_cache_destroy(rffs_rlog_cachep);
}

static inline struct rlog *rffs_try_assoc_rlog(struct inode *host,
		struct page* page)
{
	struct rlog *rl;
	unsigned int li = (unsigned int)(long)host->i_private;
	struct rffs_log *log = rffs_logs + li;

	BUG_ON(li > MAX_LOG_NUM);

	rl = hash_find_rlog(page_rlog, page);
	BUG_ON(PageError(page));

	if (rl && LESS(rl->enti, log->l_head)) { // COW
		struct page *cpage = page_cache_alloc_cold(&host->i_data);
		void *vfrom, *vto;
		struct rlog* nrl;
		vfrom = kmap_atomic(page, KM_USER0);
		vto = kmap_atomic(cpage, KM_USER1);
		copy_page(vto, vfrom);
		kunmap_atomic(vfrom, KM_USER0);
		kunmap_atomic(vto, KM_USER1);

		nrl = rlog_malloc();
		nrl->key = cpage;
		nrl->enti = rl->enti;
		L_ENT(log, nrl->enti).data = cpage;

		SetPageError(cpage); // indicates out of page cache
		hash_add_rlog(page_rlog, nrl);

		return rl;
	} else if (!rl) { // new page
		rl = rlog_malloc();
		rl->key = page;
		hash_add_rlog(page_rlog, rl);
		return rl;
	} else return NULL; // no new rlog
}

static inline int rffs_try_append_log(struct inode *host, struct rlog* rl,
		unsigned long size)
{
	int err = 0;
	unsigned int li = (unsigned int)(long)host->i_private;
	struct rffs_log *log = rffs_logs + li;

	BUG_ON(li > MAX_LOG_NUM);

	if (!rl) {
		struct transaction *tran = log_tail_tran(log);
		on_write_old_page(log, tran->stat, size);
#ifdef RFFS_TRACE
		printk(KERN_INFO "[rffs] for log(%u) old:\t%lu\t%lu\t%lu\n", li, tran->stat.staleness,
				tran->stat.merg_size, tran->stat.latency);
#endif
	} else {
		unsigned int ei;
		struct log_entry ent;

		ent.inode_id = host->i_ino;
		ent.block_begin = ((struct page *)rl->key)->index;
		ent.data = rl->key;

		err = log_append(log, &ent, &ei);
		if (likely(!err)) {
			struct transaction *tran = log_tail_tran(log);
			on_write_new_page(log, tran->stat, size);
			rl->enti = ei;
#ifdef RFFS_TRACE
			printk(KERN_INFO "[rffs] for log(%u) new:\t%lu\t%lu\t%lu\n", li, tran->stat.staleness,
					tran->stat.merg_size, tran->stat.latency);
#endif
		}
	}
	return err;
}


// mm/filemap.c
static ssize_t rffs_perform_write(struct file *file,
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

		if (!re_entry) rl = rffs_try_assoc_rlog(mapping->host, page);

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

		rffs_try_append_log(mapping->host, rl, copied);

		//TODO
		//balance_dirty_pages_ratelimited(mapping);

	} while (iov_iter_count(i));

	return written ? written : status;
}

// mm/filemap.c
static inline ssize_t rffs_file_buffered_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos, loff_t *ppos, size_t count,
		ssize_t written)
{
	struct file *file = iocb->ki_filp;
	ssize_t status;
	struct iov_iter i;

	iov_iter_init(&i, iov, nr_segs, count, written);
	status = rffs_perform_write(file, &i, pos);

	if (likely(status >= 0)) {
		written += status;
		*ppos = pos + status;
	}

	return written ? written : status;
}

// mm/filemap.c
static inline ssize_t __rffs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t *ppos)
{
	struct file *file = iocb->ki_filp;
	struct address_space * mapping = file->f_mapping;
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
	//if (unlikely(file->f_flags & O_DIRECT)) {
	//} else {
	written = rffs_file_buffered_write(iocb, iov, nr_segs, pos, ppos, count,
			written);
	//}
	out: current->backing_dev_info = NULL;
	return written ? written : err;
}

// mm/filemap.c
ssize_t rffs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file->f_mapping->host;
	struct blk_plug plug;
	ssize_t ret;

	BUG_ON(iocb->ki_pos != pos);

	mutex_lock(&inode->i_mutex);
	blk_start_plug(&plug);
	ret = __rffs_file_aio_write(iocb, iov, nr_segs, &iocb->ki_pos);
	mutex_unlock(&inode->i_mutex);

	/*
	if (ret > 0 || ret == -EIOCBQUEUED) {
		ssize_t err;

		err = generic_write_sync(file, pos, ret);
		if (err < 0 && ret > 0)
			ret = err;
	}
	*/
	blk_finish_plug(&plug);
	return ret;
}

