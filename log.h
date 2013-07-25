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
#include <asm/types.h>
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

#define LE_PGI_SHIFT	(8)
#define LE_PGI_MASK		(0xFF)

#define LE_FLAGS_SHIFT	(PAGE_CACHE_SHIFT + 4)
#define LE_LEN_SIZE	(PAGE_CACHE_SIZE << 4)
#define LE_FLAG_MASK	(LE_LEN_SIZE - 1)
#define LE_LEN_MASK		(~LE_FLAG_MASK)

#define LE_META_SETTER	(1 << LE_FLAGS_SHIFT)
#define LE_META_CLEAR	(~LE_META_SETTER)
#define LE_INVAL_SETTER	(LE_META_SETTER << 1)
#define LE_INVAL_CLEAR	(~LE_INVAL_SETTER)
#define LE_COW_SETTER	(LE_META_SETTER << 2)
#define LE_COW_CLEAR	(~LE_COW_SETTER)

struct log_entry {
    unsigned long le_ino;
    unsigned long le_pgi; // low bits are version
    unsigned long le_flags; // low bits are length
    void *le_ref;
};

#define le_ino(le)			((le)->le_ino)
#define le_set_ino(le, ino)	((le)->le_ino = (ino))
#define le_pgi(le)				((le)->le_pgi >> LE_PGI_SHIFT)
#define le_init_pgi(le, pgi)	((le)->le_pgi = (pgi) << LE_PGI_SHIFT)
#define le_ver(le)			((le)->le_pgi & LE_PGI_MASK)
#define le_add_ver(le, v)	((le)->le_pgi += (v))
#define le_len(le)			((le)->le_flags & LE_FLAG_MASK)
#define le_init_len(le, l)	((le)->le_flags = (l))
#define le_set_len(le, l)	((le)->le_flags = ((le)->le_flags & LE_LEN_MASK) + (l))

#define le_flags(le)		((le)->le_flags >> LEN_FLAGS_SHIFT)
#define le_meta(le)			((le)->le_flags & LE_META_SETTER)
#define le_set_meta(le)		((le)->le_flags |= LE_META_SETTER)
#define le_set_data(le)		((le)->le_flags &= LE_META_CLEAR)
#define le_inval(le)		((le)->le_flags & LE_INVAL_SETTER)
#define le_valid(le)		(!le_inval(le))
#define le_set_inval(le)	((le)->le_flags |= LE_INVAL_SETTER)
#define le_set_valid(le)	((le)->le_flags &= LE_INVAL_CLEAR)
#define le_cow(le)			((le)->le_flags & LE_COW_SETTER)
#define le_set_cow(le)		((le)->le_flags |= LE_COW_SETTER)
#define le_clear_cow(le)	((le)->le_flags &= LE_COW_CLEAR)

#define le_ref(le)			((le)->le_ref)
#define le_set_ref(le, r)	((le)->le_ref = (r))

static inline int le_cmp(struct log_entry *a, struct log_entry *b) {
	if (le_ino(a) < le_ino(b)) return -1;
	else if (le_ino(a) == le_ino(b)) {
	    if (a->le_pgi < b->le_pgi) return -1;
	    else {
	    	BUG_ON(a->le_pgi == b->le_pgi);
	    	return 1;
	    }
	} else return 1;
}

struct transaction {
    struct tran_stat stat;
    struct list_head list;
    unsigned int begin;
    unsigned int end;
};

#ifdef __KERNEL__
extern struct kmem_cache *rffs_tran_cachep;
#endif

#define init_tran(tran) do { \
		init_stat((tran)->stat); \
		INIT_LIST_HEAD(&(tran)->list); \
		(tran)->begin = (tran)->end = 0; } while(0)

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
#define L_ENT(log, i) ((log)->l_entries + L_INDEX(i))

#define __log_add_tran(log, tran)	\
	list_add_tail(&(tran)->list, &(log)->l_trans)

#define __log_tail_tran(log)	\
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
	struct transaction *tran = __log_tail_tran(log);
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
            PRINT("[Err%d] log failed to append: inode no. = %lu, page index = %lu\n",
                    err, le_ino(entry), le_pgi(entry));
            spin_unlock(&log->l_lock);
            return err;
        }
    }
    *L_ENT(log, tail) = *entry;
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
