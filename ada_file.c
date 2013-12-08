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

#define TRACE_FILE_PATH "/cache/adafs.trace"
struct adafs_trace adafs_trace;

extern struct kobj_type adafs_ta_ktype; // in this file

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
#ifndef ADA_DISABLE
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
	else init_completion(&adafs_logs[0]->l_kobj_unregister);

	if (fops) flush_ops = *fops;

	adafs_flusher = kthread_run(adafs_flush, NULL, "adafs_flusher");
	init_completion(&flush_cmpl);
	if (IS_ERR(adafs_flusher)) {
		printk(KERN_ERR "[adafs] kthread_run() failed: %ld\n", PTR_ERR(adafs_flusher));
		return PTR_ERR(adafs_flusher);
	}
#endif /* ADA_DISABLE */
#ifdef ADA_TRACE
	err = adafs_trace_open(&adafs_trace, TRACE_FILE_PATH, kset);
	if (err) printk(KERN_ERR "[adafs] adafs_trace_open() failed: %d\n", err);
	err = kobject_init_and_add(&adafs_trace.tr_kobj, &adafs_ta_ktype, NULL, "trace");
	if (err) printk(KERN_ERR "[adafs] kobject_init_and_add() failed for trace\n");
	else init_completion(&adafs_trace.tr_kobj_unregister);
#endif /* ADA_TRACE */
	return 0;
}

void adafs_exit_hook(void)
{
#ifndef ADA_DISABLE
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
		kobject_put(&adafs_logs[i]->l_kobj);
		wait_for_completion(&adafs_logs[i]->l_kobj_unregister);

		kfree(adafs_logs[i]);
	}
	kmem_cache_destroy(adafs_tran_cachep);
#endif
	adafs_trace_close(&adafs_trace);
#ifdef ADA_TRACE
	kobject_put(&adafs_trace.tr_kobj);
	wait_for_completion(&adafs_trace.tr_kobj_unregister);
#endif
}

void adafs_put_super_hook(void)
{
#ifdef ADA_RELEASE
	struct adafs_log *log;
	int i;
	int done = 0;
	while (!done) {
		wake_up_process(adafs_flusher);
		if (wait_for_completion_interruptible(&flush_cmpl) < 0) {
			printk(KERN_ERR "[adafs] adafs_put_super_hook "
					"interrupted in waiting for flush_cmpl.\n");
			return;
		}
		done = 1;
		for (i = 0; i < atomic_read(&num_logs); ++i) {
			log = adafs_logs[i];
			if (log->l_begin != log->l_end) done = 0;
		}
	}
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
		{
			int hit = adafs_try_append_log(mapping->host, rl, offset, copied);
			adafs_trace_page(&adafs_trace, TE_TYPE_WRITE,
					mapping->host->i_ino, page->index, (char)hit);
		}

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

/*
 * CD/DVDs are error prone. When a medium error occurs, the driver may fail
 * a _large_ part of the i/o request. Imagine the worst scenario:
 *
 *      ---R__________________________________________B__________
 *         ^ reading here                             ^ bad block(assume 4k)
 *
 * read(R) => miss => readahead(R...B) => media error => frustrating retries
 * => failing the whole request => read(R) => read(R+1) =>
 * readahead(R+1...B+1) => bang => read(R+2) => read(R+3) =>
 * readahead(R+3...B+2) => bang => read(R+3) => read(R+4) =>
 * readahead(R+4...B+3) => bang => read(R+4) => read(R+5) => ......
 *
 * It is going insane. Fix it by quickly scaling down the readahead size.
 */
static void shrink_readahead_size_eio(struct file *filp,
					struct file_ra_state *ra)
{
	ra->ra_pages /= 4;
}

/**
 * do_generic_file_read - generic file read routine
 * @filp:	the file to read
 * @ppos:	current file position
 * @desc:	read_descriptor
 * @actor:	read method
 *
 * This is a generic file read routine, and uses the
 * mapping->a_ops->readpage() function for the actual low-level stuff.
 *
 * This is really ugly. But the goto's actually try to clarify some
 * of the logic when it comes to error handling etc.
 */
static void do_generic_file_read(struct file *filp, loff_t *ppos,
		read_descriptor_t *desc, read_actor_t actor)
{
	struct address_space *mapping = filp->f_mapping;
	struct inode *inode = mapping->host;
	struct file_ra_state *ra = &filp->f_ra;
	pgoff_t index;
	pgoff_t last_index;
	pgoff_t prev_index;
	unsigned long offset;      /* offset into pagecache page */
	unsigned int prev_offset;
	int error;

	index = *ppos >> PAGE_CACHE_SHIFT;
	prev_index = ra->prev_pos >> PAGE_CACHE_SHIFT;
	prev_offset = ra->prev_pos & (PAGE_CACHE_SIZE-1);
	last_index = (*ppos + desc->count + PAGE_CACHE_SIZE-1) >> PAGE_CACHE_SHIFT;
	offset = *ppos & ~PAGE_CACHE_MASK;

	for (;;) {
		struct page *page;
		pgoff_t end_index;
		loff_t isize;
		unsigned long nr, ret;

		cond_resched();
find_page:
		page = find_get_page(mapping, index);
		if (!page) {
			/* AdaFS */
			adafs_trace_page(&adafs_trace, TE_TYPE_READ,
					inode->i_ino, index, TE_HIT_NO);
			page_cache_sync_readahead(mapping,
					ra, filp,
					index, last_index - index);
			page = find_get_page(mapping, index);
			if (unlikely(page == NULL))
				goto no_cached_page;
		} else { /* AdaFS */
			adafs_trace_page(&adafs_trace, TE_TYPE_READ,
					inode->i_ino, index, TE_HIT_YES);
		}
		if (PageReadahead(page)) {
			page_cache_async_readahead(mapping,
					ra, filp, page,
					index, last_index - index);
		}
		if (!PageUptodate(page)) {
			if (inode->i_blkbits == PAGE_CACHE_SHIFT ||
					!mapping->a_ops->is_partially_uptodate)
				goto page_not_up_to_date;
			if (!trylock_page(page))
				goto page_not_up_to_date;
			/* Did it get truncated before we got the lock? */
			if (!page->mapping)
				goto page_not_up_to_date_locked;
			if (!mapping->a_ops->is_partially_uptodate(page,
								desc, offset))
				goto page_not_up_to_date_locked;
			unlock_page(page);
		}
page_ok:
		/*
		 * i_size must be checked after we know the page is Uptodate.
		 *
		 * Checking i_size after the check allows us to calculate
		 * the correct value for "nr", which means the zero-filled
		 * part of the page is not copied back to userspace (unless
		 * another truncate extends the file - this is desired though).
		 */

		isize = i_size_read(inode);
		end_index = (isize - 1) >> PAGE_CACHE_SHIFT;
		if (unlikely(!isize || index > end_index)) {
			page_cache_release(page);
			goto out;
		}

		/* nr is the maximum number of bytes to copy from this page */
		nr = PAGE_CACHE_SIZE;
		if (index == end_index) {
			nr = ((isize - 1) & ~PAGE_CACHE_MASK) + 1;
			if (nr <= offset) {
				page_cache_release(page);
				goto out;
			}
		}
		nr = nr - offset;

		/* If users can be writing to this page using arbitrary
		 * virtual addresses, take care about potential aliasing
		 * before reading the page on the kernel side.
		 */
		if (mapping_writably_mapped(mapping))
			flush_dcache_page(page);

		/*
		 * When a sequential read accesses a page several times,
		 * only mark it as accessed the first time.
		 */
		if (prev_index != index || offset != prev_offset)
			mark_page_accessed(page);
		prev_index = index;

		/*
		 * Ok, we have the page, and it's up-to-date, so
		 * now we can copy it to user space...
		 *
		 * The actor routine returns how many bytes were actually used..
		 * NOTE! This may not be the same as how much of a user buffer
		 * we filled up (we may be padding etc), so we can only update
		 * "pos" here (the actor routine has to update the user buffer
		 * pointers and the remaining count).
		 */
		ret = actor(desc, page, offset, nr);
		offset += ret;
		index += offset >> PAGE_CACHE_SHIFT;
		offset &= ~PAGE_CACHE_MASK;
		prev_offset = offset;

		page_cache_release(page);
		if (ret == nr && desc->count)
			continue;
		goto out;

page_not_up_to_date:
		/* Get exclusive access to the page ... */
		error = lock_page_killable(page);
		if (unlikely(error))
			goto readpage_error;

page_not_up_to_date_locked:
		/* Did it get truncated before we got the lock? */
		if (!page->mapping) {
			unlock_page(page);
			page_cache_release(page);
			continue;
		}

		/* Did somebody else fill it already? */
		if (PageUptodate(page)) {
			unlock_page(page);
			goto page_ok;
		}

readpage:
		/*
		 * A previous I/O error may have been due to temporary
		 * failures, eg. multipath errors.
		 * PG_error will be set again if readpage fails.
		 */
		ClearPageError(page);
		/* Start the actual read. The read will unlock the page. */
		error = mapping->a_ops->readpage(filp, page);

		if (unlikely(error)) {
			if (error == AOP_TRUNCATED_PAGE) {
				page_cache_release(page);
				goto find_page;
			}
			goto readpage_error;
		}

		if (!PageUptodate(page)) {
			error = lock_page_killable(page);
			if (unlikely(error))
				goto readpage_error;
			if (!PageUptodate(page)) {
				if (page->mapping == NULL) {
					/*
					 * invalidate_mapping_pages got it
					 */
					unlock_page(page);
					page_cache_release(page);
					goto find_page;
				}
				unlock_page(page);
				shrink_readahead_size_eio(filp, ra);
				error = -EIO;
				goto readpage_error;
			}
			unlock_page(page);
		}

		goto page_ok;

readpage_error:
		/* UHHUH! A synchronous read error occurred. Report it */
		desc->error = error;
		page_cache_release(page);
		goto out;

no_cached_page:
		/*
		 * Ok, it wasn't cached, so we need to create a new
		 * page..
		 */
		page = page_cache_alloc_cold(mapping);
		if (!page) {
			desc->error = -ENOMEM;
			goto out;
		}
		error = add_to_page_cache_lru(page, mapping,
						index, GFP_KERNEL);
		if (error) {
			page_cache_release(page);
			if (error == -EEXIST)
				goto find_page;
			desc->error = error;
			goto out;
		}
		goto readpage;
	}

out:
	ra->prev_pos = prev_index;
	ra->prev_pos <<= PAGE_CACHE_SHIFT;
	ra->prev_pos |= prev_offset;

	*ppos = ((loff_t)index << PAGE_CACHE_SHIFT) + offset;
	file_accessed(filp);
}

int file_read_actor(read_descriptor_t *desc, struct page *page,
			unsigned long offset, unsigned long size)
{
	char *kaddr;
	unsigned long left, count = desc->count;

	if (size > count)
		size = count;

	/*
	 * Faults on the destination of a read are common, so do it before
	 * taking the kmap.
	 */
	if (!fault_in_pages_writeable(desc->arg.buf, size)) {
		kaddr = kmap_atomic(page, KM_USER0);
		left = __copy_to_user_inatomic(desc->arg.buf,
						kaddr + offset, size);
		kunmap_atomic(kaddr, KM_USER0);
		if (left == 0)
			goto success;
	}

	/* Do it the slow way */
	kaddr = kmap(page);
	left = __copy_to_user(desc->arg.buf, kaddr + offset, size);
	kunmap(page);

	if (left) {
		size -= left;
		desc->error = -EFAULT;
	}
success:
	desc->count = count - size;
	desc->written += size;
	desc->arg.buf += size;
	return size;
}

/**
 * generic_file_aio_read - generic filesystem read routine
 * @iocb:	kernel I/O control block
 * @iov:	io vector request
 * @nr_segs:	number of segments in the iovec
 * @pos:	current file position
 *
 * This is the "read()" routine for all filesystems
 * that can use the page cache directly.
 */
ssize_t
adafs_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
	struct file *filp = iocb->ki_filp;
	ssize_t retval;
	unsigned long seg = 0;
	size_t count;
	loff_t *ppos = &iocb->ki_pos;

	count = 0;
	retval = generic_segment_checks(iov, &nr_segs, &count, VERIFY_WRITE);
	if (retval)
		return retval;


	/* coalesce the iovecs and go direct-to-BIO for O_DIRECT */
	if (filp->f_flags & O_DIRECT) {
		loff_t size;
		struct address_space *mapping;
		struct inode *inode;

		mapping = filp->f_mapping;
		inode = mapping->host;
		if (!count)
			goto out; /* skip atime */
		size = i_size_read(inode);
		if (pos < size) {
			retval = filemap_write_and_wait_range(mapping, pos,
					pos + iov_length(iov, nr_segs) - 1);
			if (!retval) {
				struct blk_plug plug;

				blk_start_plug(&plug);
				retval = mapping->a_ops->direct_IO(READ, iocb,
							iov, pos, nr_segs);
				blk_finish_plug(&plug);
			}
			if (retval > 0) {
				*ppos = pos + retval;
				count -= retval;
			}

			/*
			 * Btrfs can have a short DIO read if we encounter
			 * compressed extents, so if there was an error, or if
			 * we've already read everything we wanted to, or if
			 * there was a short read because we hit EOF, go ahead
			 * and return.  Otherwise fallthrough to buffered io for
			 * the rest of the read.
			 */
			if (retval < 0 || !count || *ppos >= size) {
				file_accessed(filp);
				goto out;
			}
		}
	}

	count = retval;
	for (seg = 0; seg < nr_segs; seg++) {
		read_descriptor_t desc;
		loff_t offset = 0;

		/*
		 * If we did a short DIO read we need to skip the section of the
		 * iov that we've already read data into.
		 */
		if (count) {
			if (count > iov[seg].iov_len) {
				count -= iov[seg].iov_len;
				continue;
			}
			offset = count;
			count = 0;
		}

		desc.written = 0;
		desc.arg.buf = iov[seg].iov_base + offset;
		desc.count = iov[seg].iov_len - offset;
		if (desc.count == 0)
			continue;
		desc.error = 0;
		do_generic_file_read(filp, ppos, &desc, file_read_actor);
		retval += desc.written;
		if (desc.error) {
			retval = retval ?: desc.error;
			break;
		}
		if (desc.count > 0)
			break;
	}
out:
	return retval;
}

#ifdef ADA_TRACE
static struct attribute adafs_tracing_on = {
		.name = "tracing_on", .mode = 0644 };

static struct attribute *adafs_trace_attrs[] = {
		&adafs_tracing_on,
		NULL,
};

static ssize_t adafs_ta_show(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	if (strcmp(attr->name, "tracing_on") == 0) {
		return snprintf(buf, PAGE_SIZE, "%u\n", adafs_trace.tr_on);
	}
	return -EINVAL;
}

static ssize_t adafs_ta_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t len)
{
	if (strcmp(attr->name, "tracing_on") == 0) {
		unsigned int req;

		if (kstrtouint(buf, 0, &req) || req > 1)
			return -EINVAL;

		adafs_trace.tr_on = req;
		return len;
	}
	return -EINVAL;
}

static const struct sysfs_ops adafs_ta_ops = {
	.show	= adafs_ta_show,
	.store	= adafs_ta_store,
};

static void adafs_ta_release(struct kobject *kobj)
{
	complete(&adafs_trace.tr_kobj_unregister);
}

struct kobj_type adafs_ta_ktype = {
	.default_attrs	= adafs_trace_attrs,
	.sysfs_ops		= &adafs_ta_ops,
	.release		= adafs_ta_release,
};
#endif
