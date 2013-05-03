//
//  log.c
//  sestet-rffs
//
//  Created by Jinglei Ren on 2/27/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#include "log.h"

#define CUT_OFF 4
#define STACK_SIZE 32

#define entries(i) (entries[i & LOG_MASK])

#define SWAP_ENTRY(a, b) do {   \
  struct log_entry tmp;         \
  tmp = a;                      \
  a = b;                        \
  b = tmp;                      \
} while(0)

#define comp_entry(a, b) (a.inode_id < b.inode_id ?   \
    -1 : (a.inode_id == b.inode_id ?                  \
            a.block_begin - b.block_begin : 1))

struct stack_elem {
    unsigned int left;
    unsigned int right;
};

static struct stack_elem stack[STACK_SIZE];
static unsigned int stack_top = 0;

#define elem_len(elem) (elem.right - elem.left + 1)
#define stack_empty() (stack_top == 0)
#define stack_avail() (stack_top < STACK_SIZE)
#define stack_push(l, r) do {   \
    stack[stack_top].left = l;  \
    stack[stack_top].right = r; \
    ++stack_top;                \
} while(0)
#define stack_pop(elem) do { elem = stack[--stack_top]; } while(0)

static void short_sort(struct log_entry entries[],
        unsigned int l, unsigned int r) {
  unsigned int i, max, pos;
  if (l == r) return;

  for (pos = r; pos > l; --pos) {
    max = pos;
    for (i = l; i < pos; ++i) {
      if (comp_entry(entries(i), entries(max)) > 0) max = i;
    }
    SWAP_ENTRY(entries(max), entries(pos));
  }
}

static inline unsigned int partition(struct log_entry entries[],
        unsigned int l, unsigned int r) {
    unsigned int mi = (l + r) >> 2;
    struct log_entry m = entries(mi);
    entries(mi) = entries(l);
    entries(l) = m;
    while (l < r) {
        while (l < r && comp_entry(entries(r), m) > 0) --r;
        entries(l++) = entries(r);
        while (l < r && comp_entry(entries(l), m) < 0) ++l;
        entries(r--) = entries(l);
    }
    entries(l) = m;
    return l;
}

int log_sort(struct rffs_log *log, unsigned int begin, unsigned int end) {
    struct stack_elem elem;
    unsigned int p;
    if (begin >= end) return -EINVAL;
    stack_push(begin, end - 1);
    while (!stack_empty()) {
        stack_pop(elem);
        if (elem_len(elem) > CUT_OFF) {
            p = partition(log->l_entries, elem.left, elem.right);
            if (!stack_avail()) return -EOVERFLOW;
            stack_push(elem.left, p - 1);
            if (!stack_avail()) return -EOVERFLOW;
            stack_push(p + 1, elem.right);
        } else {
            short_sort(log->l_entries, elem.left, elem.right);
        }
    }
    return 0;
}


int log_flush(struct rffs_log *log, unsigned int nr) {
    unsigned int begin, end;
    unsigned int i;
    int err;
    struct log_entry *entries = log->l_entries;
    struct transaction *trans;

    spin_lock(&log->l_lock);
    begin = end = log->l_begin;
    while (nr && !list_empty(&log->l_trans)) {
        trans = list_entry(log->l_trans.next, struct transaction, list);
        if (trans->begin != end) {
            PRINT("[Err] Transactions do not join: %u <-> %u\n",
                    end, trans->begin);
            return -EFAULT;
        }
        end = trans->end;
        list_del(&trans->list);
        MFREE(trans);
        --nr;
    }
    if (begin == end) {
    	PRINT("[Warn] No transaction flushed: begin = end = %u\n", begin);
    	return -ENODATA;
    }
    ADJUST(begin, end);
    PRINT("-1\t0\n");
    err = log_sort(log, begin, end);
    if (err) {
        PRINT("[Err-%d] log_sort() failed.\n", err);
        return -EAGAIN;
    }
    for (i = begin; i < end; ++i) {
        PRINT("%lu\t%lu\n", entries(i).inode_id, entries(i).block_begin);
    }
    log->l_begin = end;
    spin_unlock(&log->l_lock);
    return 0;
}
