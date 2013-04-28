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
#include <linux/list.h>
#include "sys.h"

#if !defined(LOG_LEN) || !defined(LOG_MASK)
#define LOG_LEN 8192 // 8k
#define LOG_MASK 0x1fff
#endif

struct entry {
    unsigned long int inode_id;
    unsigned long int chunk_begin; // FFFFFFFF denotes inode entry
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

struct log {
    struct entry l_entries[LOG_LEN];
    unsigned int l_begin; // begin of prepared entries
    unsigned int l_head; // begin of active entries
    atomic_t l_end;
    struct list_head l_trans;
    enum l_order l_order;  // 'b' means log_begin <= log_head < log_end
};

#define L_END(log) (atomic_read(&log->l_end))

static inline void log_init(struct log *log) {
    log->l_begin = 0;
    log->l_head = 0;
    atomic_set(&log->l_end, 0);
    log->l_order = L_BEGIN;
    INIT_LIST_HEAD(&log->l_trans);
}

static inline void log_append(struct log *log, struct entry *entry) {
    unsigned int tail = atomic_inc_return(&log->l_end) - 1;
    // We assume that the order is not changed immediately
    if (tail == -1) log->l_order = L_END;
    while (log->l_order != L_BEGIN && tail >= log->l_begin) {
        // TODO
        PRINT("Out of log: sn = %ud", tail);
        return;
    }
    log->l_entries[tail & LOG_MASK] = *entry;
}

static inline void log_seal(struct log *log) {
    struct transaction *trans;
    if (log->l_order != L_END && log->l_head == L_END(log)) return;

    trans = (struct transaction *)MALLOC(sizeof(struct transaction));
    trans->begin = log->l_head;
    trans->end = L_END(log);
    list_add_tail(&trans->list, &log->l_trans);

    log->l_head = trans->end;
    if (log->l_order == L_END && log->l_head < log->l_begin)
        log->l_order = L_HEAD;
}

extern int log_sort(struct log *log, unsigned int begin, unsigned int end);

#endif
