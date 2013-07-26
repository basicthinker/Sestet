//
//  log.c
//  sestet-rffs
//
//  Created by Jinglei Ren on 2/27/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#ifdef __KERNEL__
#include "rlog.h"
#include "rffs.h"
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/page-flags.h>
#include <linux/pagemap.h>
#endif
#include "log.h"

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
struct kmem_cache *rffs_tran_cachep;
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

int __log_sort(struct rffs_log *log, int begin, int end) {
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

static void merge_inval(struct log_entry entries[], int begin, int end) {
	int i;
	for (i = begin + 1; i < end; ++i) {
		if (le_ino(&entry(i - 1)) == le_ino(&entry(i)) &&
				le_pgi(&entry(i - 1)) == le_pgi(&entry(i))) {
			le_set_inval(&entry(i - 1));
			if (le_len(&entry(i)) < le_len(&entry(i - 1)))
				le_set_len(&entry(i), le_len(&entry(i - 1)));
		}
	}
}

struct flush_operations flush_ops = { NULL, NULL, NULL };

static inline handle_t *do_trans_begin(int nent, void *arg) {
	if (flush_ops.trans_begin)
		return flush_ops.trans_begin(nent, arg);
	else return NULL;
}

static inline int do_flush(handle_t *handle, struct log_entry *le) {
	PRINT(INFO "[rffs]\t%ld\t%lu\t%lu\t%lu\n",
	        (long int)le_ino(le), le_pgi(le), le_ver(le), le_len(le));
#ifdef __KERNEL__
	if (le_meta(le)) return 0;

	if (le_valid(le) && flush_ops.ent_flush)
		flush_ops.ent_flush(handle, le);

	if (le_cow(le) || le_valid(le)) {
		struct rlog *rl = find_rlog(page_rlog, le_ref(le));
		BUG_ON(!rl);
		evict_rlog(rl);
	}
#endif
	return 0;
}

static inline int do_trans_end(handle_t *handle, void *arg) {
	if (flush_ops.trans_end) return flush_ops.trans_end(handle, arg);
	else return 0;
}

static inline int __log_flush(struct rffs_log *log, unsigned int nr) {
    unsigned int begin, end;
    unsigned int i;
    int err = 0;
    struct log_entry *entries = log->l_entries;
    struct transaction *tran;
    handle_t *handle;
#ifdef __KERNEL__
    struct super_block *sb;
#endif

    begin = end = log->l_begin;
    while (nr) {
        tran = list_first_entry(&log->l_trans, struct transaction, list);
        if (is_tran_open(tran)) break;
        if (tran->begin != end) {
            PRINT(ERR "[rffs] Transactions do not join: %u <-> %u\n",
                    end, tran->begin);
            return -EFAULT;
        }
        end = tran->end;
        list_del(&tran->list);
        --nr;
#ifdef __KERNEL__
        kmem_cache_free(rffs_tran_cachep, tran);
#else
        free(tran);
#endif
    }
    if (begin == end) {
    	PRINT(WARNING "[rffs] No transaction flushed: l_begin = %u\n", begin);
        return -ENODATA;
    }
    spin_unlock(&log->l_lock);

    PRINT(INFO "[rffs] num_entries=%d\n", end - begin);
    err = __log_sort(log, begin, end);
    if (err) {
        PRINT(ERR "[rffs] log_sort(%u, %u) failed for log %p: %d.\n",
                begin, end, log, err);
    }
    merge_inval(entries, begin, end);
#ifdef __KERNEL__
    while (le_inval(&entry(begin))) {
        do_flush(NULL, &entry(begin)); // only clears page/rlog if necessary
        ++begin;
    }
    sb = ((struct page *)le_ref(&entry(begin)))->mapping->host->i_sb;
    handle = do_trans_begin(end - begin, sb);
#else
    handle = do_trans_begin(end - begin, NULL);
#endif
    for (i = begin; i < end; ++i) {
        err = do_flush(handle, &entry(i));
        if (unlikely(err)) {
            log->l_begin = i;
            PRINT(ERR "[rffs] __log_flush stops at %d (%d - %d)\n",
            		i, begin, end);
            break;
        }
    }

#ifdef __KERNEL__
    err = do_trans_end(handle, sb);
#else
    err = do_trans_end(handle, NULL);
#endif

    spin_lock(&log->l_lock);
    log->l_begin = end; // assumes no other competitors for flush
    return err;
}

int log_flush(struct rffs_log *log, unsigned int nr) {
    int ret;
    spin_lock(&log->l_lock);
    ret = __log_flush(log, nr);
    spin_unlock(&log->l_lock);
    return ret;
}
