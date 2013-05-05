//
//  log.h
//  sestet-rffs
//
//  Created by Jinglei Ren on 2/24/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#ifndef SESTET_RFFS_LOG_H_
#define SESTET_RFFS_LOG_H_

#include <linux/types.h>
#include <asm/errno.h>
#include "sys.h"

#if !defined(LOG_LEN) || !defined(LOG_MASK)
#define LOG_LEN 8192 // 8k
#define LOG_MASK 8191
#endif

#define L_INDEX(p) (p & LOG_MASK)

#define ADJUST(a, b) do {       \
    a &= LOG_MASK;              \
    b &= LOG_MASK;              \
    if (a >= b) b += LOG_LEN;    \
} while(0)

struct log_entry {
    unsigned long int inode_id;
    unsigned long int block_begin; // FFFFFFFF denotes inode entry
    void *data; // refers to inode info when this is inode entry
};

struct transaction {
    unsigned int begin;
    unsigned int end;
    struct list_head list;
};

enum l_order {
    L_BEGIN,
    L_HEAD,
    L_END
};

struct rffs_log {
    struct log_entry l_entries[LOG_LEN];
    unsigned int l_begin; // begin of prepared entries
    unsigned int l_head; // begin of active entries
    atomic_t l_end;
    struct list_head l_trans;
    enum l_order l_order;  // 'b' means log_begin <= log_head < log_end
    spinlock_t l_lock; // to protect the prepared entries
};

#define L_END(log) (atomic_read(&log->l_end))

static inline void log_init(struct rffs_log *log) {
    log->l_begin = 0;
    log->l_head = 0;
    atomic_set(&log->l_end, 0);
    log->l_order = L_BEGIN;
    INIT_LIST_HEAD(&log->l_trans);
    spin_lock_init(&log->l_lock);
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
    struct transaction *trans;
    unsigned int end = L_END(log);
    if (log->l_head == end) {
        PRINT("[Warn] log failed to seal: %u-%u-%u\n",
                L_INDEX(log->l_begin), L_INDEX(log->l_head), L_INDEX(end));
        return;
    }
    if (end - log->l_begin > LOG_LEN) {
        end = log->l_begin + LOG_LEN;
    }
    trans = (struct transaction *)MALLOC(sizeof(struct transaction));

    trans->begin = log->l_head;
    trans->end = end;
    list_add_tail(&trans->list, &log->l_trans);

    log->l_head = trans->end;
    if (log->l_order == L_END && L_INDEX(log->l_head) <= L_INDEX(log->l_begin))
        log->l_order = L_HEAD;
}

static inline void log_seal(struct rffs_log *log) {
    spin_lock(&log->l_lock);
    __log_seal(log);
    spin_unlock(&log->l_lock);
}

static inline int log_append(struct rffs_log *log, struct log_entry *entry) {
    unsigned int tail = atomic_inc_return(&log->l_end) - 1;
    int err;
    // We assume that the order is not changed immediately
    if (L_INDEX(tail) == LOG_MASK) log->l_order = L_END;
    if (tail - log->l_begin >= LOG_LEN) {
        spin_lock(&log->l_lock);
        while (tail - log->l_begin >= LOG_LEN) {
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
    return 0;
}

extern int log_sort(struct rffs_log *log, unsigned int begin, unsigned int end);

#endif
