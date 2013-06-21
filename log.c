//
//  log.c
//  sestet-rffs
//
//  Created by Jinglei Ren on 2/27/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#ifdef __KERNEL__
#include "rlog.h"
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/mmdebug.h>
#include <linux/page-flags.h>
#endif
#include "log.h"

#define CUT_OFF 4
#define STACK_SIZE 32

#define entry(i) (entries[(i) & LOG_MASK])

#define SWAP_ENTRY(a, b) do {   \
  struct log_entry tmp;         \
  tmp = a;                      \
  a = b;                        \
  b = tmp;                      \
} while(0)

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
      if (comp_entry(&entry(i), &entry(max)) > 0) max = i;
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
        while (l < r && comp_entry(&entry(r), &m) >= 0) --r;
        entry(l) = entry(r);
        while (l < r && comp_entry(&entry(l), &m) <= 0) ++l;
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
		if (comp_entry(&entry(i - 1), &entry(i)) == 0) {
			ent_inval(entry(i - 1));
		}
	}
}

struct flush_operations flush_ops = { NULL, NULL, NULL };

static inline handle_t *do_trans_begin(int nent, void *arg) {
	if (flush_ops.trans_begin)
		return flush_ops.trans_begin(nent, arg);
	else return NULL;
}

#ifdef __KERNEL__

static inline void flush_page(handle_t *handle, struct page *page)
{
	if (flush_ops.ent_flush)
		flush_ops.ent_flush(handle, page);
	else {
		void *addr = kmap_atomic(page, KM_USER0);
		printk("[rffs] flushing page %p with %c.\n", addr, *(char *)addr);
		kunmap_atomic(addr, KM_USER0);
	}
}

#endif

static inline int do_flush(handle_t *handle, struct log_entry *ent)
{
#ifdef __KERNEL__
	struct rlog *rl = hash_find_rlog(page_rlog, ent->data);
	hash_del(&rl->hnode);
	if (PageError(rl->key)) {
		ClearPageError(rl->key);
		if (ent_valid(*ent)) flush_page(handle, rl->key);
		__free_page(rl->key);
	} else if (ent_valid(*ent)) {
		flush_page(handle, rl->key);
	}
	rlog_free(rl);
	return 0;
#endif
	PRINT("[rffs] flushing ent data %p.\n", ent->data);
	return 0;
}

static inline int do_trans_end(handle_t *handle) {
	if (flush_ops.trans_end) return flush_ops.trans_end(handle);
	else return 0;
}

int __log_flush(struct rffs_log *log, unsigned int nr) {
    unsigned int begin, end;
    unsigned int i;
    int err = 0;
    struct log_entry *entries = log->l_entries;
    struct transaction *tran;
    handle_t *handle;

    begin = end = log->l_begin;
    while (nr) {
        tran = list_first_entry(&log->l_trans, struct transaction, list);
        if (is_tran_open(tran)) break;
        if (tran->begin != end) {
            PRINT("[Err] Transactions do not join: %u <-> %u\n",
                    end, tran->begin);
            return -EFAULT;
        }
        end = tran->end;
        list_del(&tran->list);
        MFREE(tran);
        --nr;
    }
    if (begin == end) {
    	PRINT("[Warn] No transaction flushed: l_begin = %u\n", begin);
    	return -ENODATA;
    }

    PRINT("[rffs]\t(-1)\t%d\n", end - begin);
    err = __log_sort(log, begin, end);
    if (err) {
        PRINT("[Err%d] log_sort() failed.\n", err);
        tran = (struct transaction *)MALLOC(sizeof(struct transaction));
        tran->begin = begin;
        tran->end = end;
        list_add(&tran->list, &log->l_trans);
        return -EAGAIN;
    }
    merge_inval(entries, begin, end);
#ifdef __KERNEL__
    while (!ent_valid(entry(begin))) ++begin;
    handle = do_trans_begin(end - begin,
    		((struct page *)entry(begin).data)->mapping->host);
#else
    handle = do_trans_begin(end - begin, NULL);
#endif
    for (i = begin; i < end; ++i) {
    	err = do_flush(handle, &entry(i));
    	if (unlikely(err)) {
    		log->l_begin = i;
    		break;
    	} else {
    		PRINT("[rffs]\t(%d)\t%lu\t%lu\n",
    				i, entry(i).inode_id, entry(i).block_begin);
    	}
    }
    err = do_trans_end(handle);
    log->l_begin = end;

    return err;
}
