//
//  log.h
//  sestet-rffs
//
//  Created by Jinglei Ren on 2/24/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#ifndef RFFS_LOG_H_
#define RFFS_LOG_H_

#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/errno.h>

#ifdef __KERNEL__
#include <linux/jbd2.h>
#else
typedef int handle_t;
#endif

#include "sys.h"
#include "policy.h"

#if !defined(LOG_LEN) || !defined(LOG_MASK)
    #define LOG_LEN 8192 // 8k
    #define LOG_MASK 8191
#endif

#define L_INDEX(p) ((p) & LOG_MASK)
#define L_NULL  LOG_LEN

#define L_DIST(a, b) ((a) <= (b) ? (b) - (a) : UINT_MAX - (a) + (b) + 1)
#define L_LESS(a, b) ((a) != (b) && L_DIST(a, b) <= LOG_LEN)
#define L_NG(a, b) (L_DIST(a, b) <= LOG_LEN)

#define LEN_SHIFT   (PAGE_CACHE_SHIFT + 1)
#define LEN_SIZE    (PAGE_CACHE_SIZE << 1)
#define LEN_MASK    (~(LEN_SIZE - 1))

struct log_entry {
    unsigned long inode_id; // ULONG_MAX indicates invalid entry
    unsigned long index;
    unsigned long length;
    void *data;
};

#define ent_inval(ent) { (ent).inode_id = ULONG_MAX; }
#define ent_valid(ent) ((ent).inode_id != ULONG_MAX)
#define ent_len(ent) ((ent).length & (LEN_SIZE - 1))
#define set_len(ent, l) ((ent).length &= LEN_MASK, (ent).length += (l))
#define ent_seq(ent) ((ent).length >> LEN_SHIFT)
#define add_seq(ent, n) ((ent).length += (n) << LEN_SHIFT)

static inline int comp_entry(struct log_entry *a, struct log_entry *b) {
	if (a->inode_id < b->inode_id) return -1;
	else if (a->inode_id == b->inode_id) {
	    if (a->index < b->index) return -1;
	    else if (a->index == b->index) return ent_seq(*a) - ent_seq(*b);
	    else return 1;
	} else return 1;
}

struct transaction {
    unsigned int begin;
    unsigned int end;
    struct tran_stat stat;
    struct list_head list;
};

#ifdef __KERNEL__
extern struct kmem_cache *rffs_tran_cachep;
#endif

#define init_tran(tran) do {		\
	(tran)->begin = (tran)->end = 0;	\
	init_stat((tran)->stat);			\
} while(0)

static inline struct transaction *new_tran(void) {
	struct transaction *tran;
#ifdef __KERNEL__
	tran = (struct transaction *)kmem_cache_alloc(rffs_tran_cachep, GFP_KERNEL);
#else
	tran = (struct transaction *)malloc(sizeof(struct transaction));
#endif
	init_tran(tran);
	return tran;
}

#define is_tran_open(tran) ((tran)->begin == (tran)->end)

struct rffs_log {
    struct log_entry l_entries[LOG_LEN];
    unsigned int l_begin; // begin of prepared entries
    unsigned int l_head; // begin of active entries
    atomic_t l_end;
    struct list_head l_trans;
    spinlock_t l_lock; // to protect the prepared entries
};

#define L_END(log) (atomic_read(&(log)->l_end))
#define L_ENT(log, i) ((log)->l_entries[L_INDEX(i)])

#define __log_add_tran(log, tran)	\
	list_add_tail(&(tran)->list, &(log)->l_trans)

#define log_tail_tran(log)	\
	list_entry((log)->l_trans.prev, struct transaction, list)

static inline void log_init(struct rffs_log *log) {
	struct transaction *tran = new_tran();
    log->l_begin = 0;
    log->l_head = 0;
    atomic_set(&log->l_end, 0);
    INIT_LIST_HEAD(&log->l_trans);
    spin_lock_init(&log->l_lock);
    __log_add_tran(log, tran);
}

extern int log_flush(struct rffs_log *log, unsigned int nr);

static inline void log_destroy(struct rffs_log *log) {
	struct transaction *pos, *tmp;

	log_flush(log, UINT_MAX);

	list_for_each_entry_safe(pos, tmp, &log->l_trans, list) {
		kmem_cache_free(rffs_tran_cachep, pos);
	}
}

static inline void __log_seal(struct rffs_log *log) {
	struct transaction *tran = log_tail_tran(log);
    unsigned int end = L_END(log);
    if (log->l_head == end) {
        PRINT(WARNING "[rffs] nothing to seal: %u-%u-%u\n",
                log->l_begin, log->l_head, end);
        return;
    }
    if (L_DIST(log->l_begin, end) > LOG_LEN) {
        end = log->l_begin + LOG_LEN;
    }

    tran->begin = log->l_head;
    tran->end = end;
    log->l_head = tran->end;
}

static inline void log_seal(struct rffs_log *log) {
	struct transaction *tran = new_tran();

    spin_lock(&log->l_lock);
    __log_seal(log);
    __log_add_tran(log, tran);
    spin_unlock(&log->l_lock);
}

static inline int log_append(struct rffs_log *log, struct log_entry *entry,
        unsigned int *enti) {
    int err;
    unsigned int tail = (unsigned int)atomic_inc_return(&log->l_end) - 1;

    while (L_DIST(log->l_begin, tail) >= LOG_LEN) {
        err = log_flush(log, 1);
        if (err == -ENODATA) {
            log_seal(log);
            err = log_flush(log, 1);
        }
        if (err) {
            PRINT("[Err%d] log failed to append: inode = %lu, block = %lu\n",
                    err, entry->inode_id, entry->index);
            spin_unlock(&log->l_lock);
            return err;
        }
    }
    L_ENT(log, tail) = *entry;
    if (likely(enti)) *enti = tail;
    return 0;
}

extern int __log_sort(struct rffs_log *log, int begin, int end);

static inline int log_sort(struct rffs_log *log, int begin, int end) {
    int ret;
    spin_lock(&log->l_lock);
    ret = __log_sort(log, begin, end);
    spin_unlock(&log->l_lock);
    return ret;
}

struct flush_operations {
	handle_t *(*trans_begin)(int nent, void *data);
	int (*ent_flush)(handle_t *handle, struct log_entry *ent);
	int (*trans_end)(handle_t *handle, void *data);
};

extern struct flush_operations flush_ops;

#endif
