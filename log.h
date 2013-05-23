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
#include <asm/errno.h>

#include "sys.h"
#include "policy.h"

#if !defined(LOG_LEN) || !defined(LOG_MASK)
    #define LOG_LEN 8192 // 8k
    #define LOG_MASK 8191
#endif

#define L_INDEX(p) (p & LOG_MASK)

#define DIST(a, b) (a <= b ? b - a : UINT_MAX - a + b + 1)

#define LESS(a, b) (a != b && DIST(a, b) <= LOG_LEN)

struct log_entry {
    unsigned long inode_id;
    unsigned long block_begin;
    void *data;
};

static inline int comp_entry(struct log_entry *a, struct log_entry *b) {
	if (a->inode_id < b->inode_id) return -1;
	else if (a->inode_id == b->inode_id) return a->block_begin - b->block_begin;
	else return 1;
}

struct transaction {
    unsigned int begin;
    unsigned int end;
    struct tran_stat stat;
    struct list_head list;
};

#define init_tran(tran) do {		\
	tran->begin = tran->end = 0;	\
	init_stat(tran->stat);			\
} while(0)

#define is_tran_open(tran) (tran->begin == tran->end)

struct rffs_log {
    struct log_entry l_entries[LOG_LEN];
    unsigned int l_begin; // begin of prepared entries
    unsigned int l_head; // begin of active entries
    atomic_t l_end;
    struct list_head l_trans;
    spinlock_t l_lock; // to protect the prepared entries
};

#define L_END(log) (atomic_read(&log->l_end))

static inline struct transaction *__log_add_tran(struct rffs_log *log)
{
	struct transaction *tran =
				(struct transaction *)MALLOC(sizeof(struct transaction));
	init_tran(tran);
	list_add_tail(&tran->list, &log->l_trans);
	return tran;
}

#define log_tail_tran(log)	\
	list_entry(log->l_trans.prev, struct transaction, list)

static inline void log_init(struct rffs_log *log) {
    log->l_begin = 0;
    log->l_head = 0;
    atomic_set(&log->l_end, 0);
    INIT_LIST_HEAD(&log->l_trans);
    spin_lock_init(&log->l_lock);
    __log_add_tran(log);
}

extern int __log_flush(struct rffs_log *log, unsigned int nr);

static inline int log_flush(struct rffs_log *log, unsigned int nr) {
    int ret;
    spin_lock(&log->l_lock);
    ret = __log_flush(log, nr);
    spin_unlock(&log->l_lock);
    return ret;
}

static inline void __log_seal(struct rffs_log *log) {
    struct transaction *tran = log_tail_tran(log);
    unsigned int end = L_END(log);
    if (log->l_head == end) {
        PRINT("[Warn] nothing to seal: %u-%u-%u\n",
                log->l_begin, log->l_head, end);
        return;
    }
    if (DIST(log->l_begin, end) > LOG_LEN) {
        end = log->l_begin + LOG_LEN;
    }

    tran->begin = log->l_head;
    tran->end = end;
    log->l_head = tran->end;

    __log_add_tran(log);
}

static inline void log_seal(struct rffs_log *log) {
    spin_lock(&log->l_lock);
    __log_seal(log);
    spin_unlock(&log->l_lock);
}

static inline int log_append(struct rffs_log *log, struct log_entry *entry,
		unsigned int *enti) {
	int err;
    unsigned int tail = (unsigned int)atomic_inc_return(&log->l_end) - 1;

    if (DIST(log->l_begin, tail) >= LOG_LEN) {
        spin_lock(&log->l_lock);
        while (DIST(log->l_begin, tail) >= LOG_LEN) {
            err = __log_flush(log, 1);
            if (err == -ENODATA) {
                __log_seal(log);
                err = __log_flush(log, 1);
            }
            if (err) {
                PRINT("[Err%d] log failed to append: inode = %lu, block = %lu\n",
                        err, entry->inode_id, entry->block_begin);
                spin_unlock(&log->l_lock);
                return err;
            }
        }
        spin_unlock(&log->l_lock);
    }
    log->l_entries[L_INDEX(tail)] = *entry;
    if (likely(enti)) *enti = tail;
    return 0;
}

extern int log_sort(struct rffs_log *log, int begin, int end);

#endif
