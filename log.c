//
//  log.c
//  sestet-rffs
//
//  Created by Jinglei Ren on 2/27/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#ifdef __KERNEL__
#include "rlog.h"
#include <linux/highmem.h>
#include <linux/mmdebug.h>
#include <linux/page-flags.h>
#endif
#include "log.h"

#define CUT_OFF 4
#define STACK_SIZE 32

#define entry(i) (entries[i & LOG_MASK])

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

int log_sort(struct rffs_log *log, int begin, int end) {
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

#ifdef __KERNEL__

static inline void do_flush(struct page *page)
{
	void *addr;
	addr = kmap_atomic(page, KM_USER0);
	printk("[rffs] flushed page %p with %c.\n", addr, *(char *)addr);
	kunmap_atomic(addr, KM_USER0);
}

#endif

int __log_flush(struct rffs_log *log, unsigned int nr) {
    unsigned int begin, end;
    unsigned int i;
    int err;
    struct log_entry *entries = log->l_entries;
    struct transaction *tran;

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
#ifdef __KERNEL__
        for (i = tran->begin; i < tran->end; ++i) {
        	struct rlog *rl = hash_find_rlog(page_rlog, entry(i).data);
        	hash_del(&rl->hnode);
        	if (PageError(rl->key)) {
        		ClearPageError(rl->key);
        		do_flush(rl->key);
        		__free_page(rl->key);
        	} else {
        		do_flush(rl->key);
        	}
        	rlog_free(rl);
        }
#endif
        list_del(&tran->list);
        MFREE(tran);
        --nr;
    }
    if (begin == end) {
    	PRINT("[Warn] No transaction flushed: l_begin = %u\n", begin);
    	return -ENODATA;
    }

    PRINT("(-1)\t%d\n", end - begin);
    err = log_sort(log, begin, end);
    if (err) {
        PRINT("[Err%d] log_sort() failed.\n", err);
        tran = (struct transaction *)MALLOC(sizeof(struct transaction));
        tran->begin = begin;
        tran->end = end;
        list_add(&tran->list, &log->l_trans);
        return -EAGAIN;
    }
    for (i = begin; i < end; ++i) {
        PRINT("(%d)\t%lu\t%lu\n", i, entry(i).inode_id, entry(i).block_begin);
    }

    log->l_begin = end;

    return 0;
}
