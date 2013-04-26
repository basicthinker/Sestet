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

struct log {
    struct entry l_entries[LOG_LEN];
    atomic_t l_begin; // begin of prepared entries
    atomic_t l_head; // begin of active entries
    atomic_t l_end;
    atomic_t l_order;  // 0 means log_begin <= log_head < log_end
};

static inline void log_init(struct log *log) {
    atomic_set(&log->l_begin, -1);
    atomic_set(&log->l_head, -1);
    atomic_set(&log->l_end, 0);
    atomic_set(&log->l_order, 0);
}

static inline void log_append(struct log *log, struct entry *entry) {

}

static inline void log_seal(struct log *log) {

}

extern int log_sort(struct log *log, unsigned int begin, unsigned int end);

#endif
