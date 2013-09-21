//
//  log.c
//  sestet-adafs
//
//  Created by Jinglei Ren on 2/27/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#ifdef __KERNEL__
#include "ada_rlog.h"
#include "ada_fs.h"
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/page-flags.h>
#include <linux/pagemap.h>
#include <linux/blkdev.h>
#include <linux/writeback.h>
#endif
#include "ada_log.h"

#define CUT_OFF 4
#define STACK_SIZE 32

#define entry(i) (entries[(i) & LOG_MASK])

#define SWAP_ENTRY(a, b) do { \
        struct log_entry tmp; \
        tmp = (a); \
        (a) = (b); \
        (b) = tmp; } while (0)

#define SWAP_ENTI(i, j) do { \
        if (likely((i) != (j))) \
            SWAP_ENTRY(entry(i), entry(j)); } while (0)

#ifdef __KERNEL__
struct kmem_cache *adafs_tran_cachep;
#endif

struct stack_elem {
    unsigned int left;
    unsigned int right;
};

static struct stack_elem stack[STACK_SIZE];
static unsigned int stack_top = 0;

#define elem_len(elem) (elem.right - elem.left + 1)
#define stack_empty() (stack_top == 0)
#define stack_avail(nr) (STACK_SIZE - stack_top >= nr)
#define stack_push(l, r) do {   \
    stack[stack_top].left = l;  \
    stack[stack_top].right = r; \
    ++stack_top;                \
} while(0)
#define stack_pop(elem) do { elem = stack[--stack_top]; } while(0)


static void short_sort(struct log_entry entries[], int l, int r) {
  int i, max, pos;
  if (l >= r) return;

  for (pos = r; pos > l; --pos) {
    max = pos;
    for (i = l; i < pos; ++i) {
      if (le_cmp(&entry(i), &entry(max)) > 0) max = i;
    }
    SWAP_ENTRY(entry(max), entry(pos));
  }
}

static inline unsigned int partition(struct log_entry entries[], int l, int r) {
    int mi = (l + r) >> 1;
    struct log_entry m = entry(mi);
    entry(mi) = entry(l);
    entry(l) = m;
    while (l < r) {
        while (l < r && le_cmp(&entry(r), &m) >= 0) --r;
        entry(l) = entry(r);
        while (l < r && le_cmp(&entry(l), &m) <= 0) ++l;
        entry(r) = entry(l);
    }
    entry(l) = m;
    return l;
}

int __log_sort(struct adafs_log *log, unsigned int begin, unsigned int end) {
    struct stack_elem elem;
    int p;
    if (begin > end) {
        begin -= LOG_LEN;
        end -= LOG_LEN;
    }
    stack_push(begin, end - 1);
    while (!stack_empty()) {
        stack_pop(elem);
        if (elem_len(elem) > CUT_OFF) {
            p = partition(log->l_entries, elem.left, elem.right);
            if (!stack_avail(2)) return -EOVERFLOW;
            stack_push(elem.left, p - 1);
            stack_push(p + 1, elem.right);
        } else {
            short_sort(log->l_entries, elem.left, elem.right);
        }
    }
    return 0;
}

struct flush_operations flush_ops = { NULL, NULL, NULL };

static inline handle_t *do_trans_begin(struct inode *inode, int nles) {
	if (likely(nles > 0 && flush_ops.trans_begin)) {
		return flush_ops.trans_begin(inode, nles);
	}
	return NULL;
}

/* Adopted from write_cache_pages() in mm/page-writeback.c */
static inline int do_le_flush(handle_t *handle,
		struct log_entry *le, struct writeback_control *wbc) {
	if (likely(handle && flush_ops.entry_flush)) {
		struct page *page = le_page(le);
		ADAFS_DEBUG(KERN_INFO "[adafs] do_le_flush() flushes page: " LE_DUMP_PAGE(le));
		lock_page(page);

		//if (!PageDirty(page)) /* someone wrote it for us */
			//goto out;

		//if (PageWriteback(page)) {
			//if (wbc->sync_mode != WB_SYNC_NONE)
				//wait_on_page_writeback(page);
			//else
				//goto out;
		//}

		BUG_ON(PageWriteback(page));
		//if (!clear_page_dirty_for_io(page))
			//goto out;

		return flush_ops.entry_flush(handle, le, wbc);
	//out:
		//unlock_page(page);
		//PRINT(ERR "[adafs] do_le_flush did NOT flush: " LE_DUMP(le));
	}
	return 0;
}

static inline int do_wait_sync(struct inode *inode, tid_t tid)
{
	if (likely(flush_ops.wait_sync)) {
		return flush_ops.wait_sync(inode, tid);
	}
	return 0;
}

static inline int do_trans_end(handle_t *handle) {
	if (likely(handle && flush_ops.trans_end)) {
		return flush_ops.trans_end(handle);
	}
	return 0;
}

#define le_for_each(le, i, begin, end) \
		for (le = &entry(i = (begin)); seq_less(i, end); le = &entry(++i))

static int __merge_flush(struct log_entry entries[],
		const unsigned int begin, const unsigned int end) {
	struct log_entry *le;
	struct inode *inode;
	handle_t *handle;
	tid_t commit_tid;
	unsigned int b, e, i, nles;
	int err = 0;
	unsigned long ino;

	for (b = begin; seq_less(b, end); b = e) {
		le = &entry(b);
		if (unlikely(le_inval(le))) {
			break;
		} else if (le_meta(le)) {
			e = b + 1;
		} else { // to flush a group of entries
			struct blk_plug plug;
			struct writeback_control wbc = {
					.sync_mode = WB_SYNC_ALL,
					.nr_to_write = LONG_MAX,
					.range_start = 0,
					.range_end = LLONG_MAX };

			le = &entry(b);
			ino = le_ino(le);
			ADAFS_DEBUG(KERN_DEBUG "[adafs-debug] __merge_flush() gets sb from page: "
					LE_DUMP_PAGE(le));
			inode = le_page(le)->mapping->host;

			nles = 1; // counts pages to flush
			le_for_each(le, i, b + 1, end) {
				if (unlikely(le_ino(le) != ino)) break;
				if (le_pgi(&entry(i - 1)) == le_pgi(le)) {
					ADAFS_DEBUG(INFO "[adafs] __merge_flush() invalidates entry: "
							LE_DUMP(&entry(i - 1)));
					le_set_inval(&entry(i - 1));
					evict_entry(&entry(i - 1), page_rlog);

					if (le_len(le) < le_len(&entry(i - 1)))
						le_set_len(le, le_len(&entry(i - 1)));
				} else {
					++nles;
				}
			} // for
			e = i;
			PRINT("[adafs] __merge_flush() begins flushing: ino=%lu "
					"begin=%u, end=%u, num=%u\n", ino, b, e, nles);

			blk_start_plug(&plug);
			handle = do_trans_begin(inode, nles);
			ADAFS_BUG_ON(IS_ERR(handle));
			commit_tid = handle->h_transaction->t_tid;

			le_for_each(le, i, b, e) {
				if (le_inval(le)) continue;
				err = do_le_flush(handle, le, &wbc);
				if (unlikely(err)) {
					PRINT(ERR "[adafs] entry_flush failed: " LE_DUMP(le));
				}
			}
			err = do_trans_end(handle);
			blk_finish_plug(&plug);

			le_for_each(le, i, b, e) {
				if (le_inval(le)) continue;
				wait_on_page_writeback(le_page(le));
				evict_entry(le, page_rlog);
			}
			mutex_lock(&inode->i_mutex);
			do_wait_sync(inode, commit_tid);
			mutex_unlock(&inode->i_mutex);
		}
	} // for all target entries
	return err;
}

int log_flush(struct adafs_log *log, unsigned int nr) {
    unsigned int begin, end;
    int err = 0;
    struct log_entry *entries = log->l_entries;
    struct transaction *tran;

    spin_lock(&log->l_tlock);
    tran = list_first_entry(&log->l_trans, struct transaction, list);
    begin = end = tran->begin;
    while (nr) {
        if (is_tran_open(tran)) break;
        ADAFS_BUG_ON(tran->begin != end);
        end = tran->end;
        list_del(&tran->list);
        evict_tran(tran);
        tran = list_first_entry(&log->l_trans, struct transaction, list);
        --nr;
    }
    spin_unlock(&log->l_tlock);
    if (begin == end) {
    	PRINT(WARNING "[adafs] No transaction flushed: l_begin = %u\n", begin);
        return -ENODATA;
    }

    mutex_lock(&log->l_fmutex);
    __log_sort(log, begin, end);
    err = __merge_flush(entries, begin, end);
    log->l_begin = end;
    mutex_unlock(&log->l_fmutex);
    return err;
}


/* Attributes exported to sysfs */

static ssize_t staleness_sum_show(struct adafs_log *log, char *buf)
{
	unsigned long sum = log_stal_sum(log);
	return snprintf(buf, PAGE_SIZE, "%lu\n", sum);
}

static ssize_t staleness_sum_store(struct adafs_log *log, const char *buf, size_t len)
{
	unsigned long req_sum;

	if (kstrtoul(buf, 0, &req_sum) || req_sum != 0)
		return -EINVAL;

	wake_up_process(adafs_flusher);
	wait_for_completion_timeout(&log->l_fcmpl, HZ);
    return len;
}

unsigned int stal_limit_blocks = 4096;

static ssize_t stal_limit_blocks_show(struct adafs_log *log, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", stal_limit_blocks);
}

static ssize_t stal_limit_blocks_store(struct adafs_log *log, const char *buf, size_t len)
{
	unsigned int limit;

	if (kstrtouint(buf, 0, &limit))
		return -EINVAL;

	stal_limit_blocks = limit;
    return len;
}

ADAFS_RW_LA(staleness_sum);
ADAFS_RW_LA(stal_limit_blocks);

static struct attribute *adafs_log_attrs[] = {
		ADAFS_LA(staleness_sum),
		ADAFS_LA(stal_limit_blocks),
		NULL,
};

static ssize_t adafs_la_show(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct adafs_log *log = container_of(kobj, struct adafs_log, l_kobj);
	struct adafs_log_attr *a = container_of(attr, struct adafs_log_attr, attr);

	return a->show ? a->show(log, buf) : 0;
}

static ssize_t adafs_la_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t len)
{
	struct adafs_log *log = container_of(kobj, struct adafs_log, l_kobj);
	struct adafs_log_attr *a = container_of(attr, struct adafs_log_attr, attr);

	return a->store ? a->store(log, buf, len) : 0;
}

static void adafs_la_release(struct kobject *kobj)
{
	struct adafs_log *log = container_of(kobj, struct adafs_log, l_kobj);
	complete(&log->l_kobj_unregister);
}

static const struct sysfs_ops adafs_la_ops = {
	.show	= adafs_la_show,
	.store	= adafs_la_store,
};

struct kobj_type adafs_la_ktype = {
	.default_attrs	= adafs_log_attrs,
	.sysfs_ops		= &adafs_la_ops,
	.release		= adafs_la_release,
};
