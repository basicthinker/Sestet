//
//  log.h
//  sestet-adafs
//
//  Created by Jinglei Ren on 2/24/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#ifndef ADAFS_LOG_H_
#define ADAFS_LOG_H_

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/types.h>
#include <asm/errno.h>

#ifdef __KERNEL__
#include <linux/jbd2.h>
#include <linux/sysfs.h>
#else
typedef int handle_t;
#endif

#include "ada_sys.h"

extern struct flush_operations flush_ops;
extern struct kobj_type adafs_la_ktype;

#if !defined(LOG_LEN) || !defined(LOG_MASK)
    #define LOG_LEN 16384 // 16k * 4KB = 64MB
    #define LOG_MASK 16383
#endif

#define L_INDEX(p)  ((p) & LOG_MASK)

#define seq_dist(a, b)  ((unsigned int)((b) - (a)))
#define seq_less(a, b)  ((int)((a) - (b)) < 0)
#define seq_ng(a, b)    ((int)((a) - (b)) <= 0)
#define L_NULL          UINT_MAX
#define LE_INVAL_INO	ULONG_MAX

#define LE_PGI_SHIFT	(8)
#define LE_PGI_MASK		(0xFF)

#define LE_FLAGS_SHIFT	(PAGE_CACHE_SHIFT + 4)
#define LE_LEN_SIZE		(PAGE_CACHE_SIZE << 4)
#define LE_FLAG_MASK	(LE_LEN_SIZE - 1)
#define LE_LEN_MASK		(~LE_FLAG_MASK)

#define LE_META_SETTER	(1 << LE_FLAGS_SHIFT)
#define LE_META_CLEAR	(~LE_META_SETTER)
#define LE_COW_SETTER	(LE_META_SETTER << 1)
#define LE_COW_CLEAR	(~LE_COW_SETTER)

struct log_entry {
    unsigned long le_ino;
    unsigned long le_pgi; // low bits are version
    unsigned long le_flags; // low bits are length
    void *le_ref;
};

#define le_ino(le)			((le)->le_ino)
#define le_set_ino(le, ino)	((le)->le_ino = (ino))
#define le_inval(le)		(le_ino(le) == LE_INVAL_INO)
#define le_valid(le)		(!le_inval(le))
#define le_set_inval(le)	(le_set_ino(le, LE_INVAL_INO))

#define le_pgi(le)				((le)->le_pgi >> LE_PGI_SHIFT)
#define le_init_pgi(le, pgi)	((le)->le_pgi = ((pgi) << LE_PGI_SHIFT) + 1)
#define le_ver(le)			((le)->le_pgi & LE_PGI_MASK)
#define le_set_ver(le, v)	((le)->le_pgi += (v) - le_ver(le))
#define le_len(le)			((le)->le_flags & LE_FLAG_MASK)
#define le_init_len(le, l)	((le)->le_flags = (l))
#define le_set_len(le, l)	((le)->le_flags = ((le)->le_flags & LE_LEN_MASK) + (l))

#define le_flags(le)		((le)->le_flags >> LE_FLAGS_SHIFT)
#define le_meta(le)			((le)->le_flags & LE_META_SETTER)
#define le_set_meta(le)		((le)->le_flags |= LE_META_SETTER)
#define le_set_data(le)		((le)->le_flags &= LE_META_CLEAR)
#define le_cow(le)			((le)->le_flags & LE_COW_SETTER)
#define le_set_cow(le)		((le)->le_flags |= LE_COW_SETTER)
#define le_clear_cow(le)	((le)->le_flags &= LE_COW_CLEAR)

#define le_page(le)			((struct page *)(le)->le_ref)
#define le_set_ref(le, r)	((le)->le_ref = (r))

#define LE_INITIALIZER	{ 0, 0, 0, NULL }

#define LE_DUMP(le)	"ino=%ld, pgi=%lu, ver=%lu, page=%p, len=%lu, flags=%lu\n", \
		le_ino(le), le_pgi(le), le_ver(le), le_page(le), le_len(le), le_flags(le)

#define LE_DUMP_PAGE(le)	"ino=%ld, mapping=%p, index=%lu, page=%p\n", \
		le_page(le)->mapping && le_page(le)->mapping->host ? le_page(le)->mapping->host->i_ino : LE_INVAL_INO, \
		le_page(le)->mapping, le_page(le)->index, le_page(le)

static inline int le_cmp(struct log_entry *a, struct log_entry *b) {
	if (le_ino(a) < le_ino(b)) return -1;
	else if (le_ino(a) == le_ino(b)) {
	    return a->le_pgi - b->le_pgi;
	} else return 1;
}

struct tran_stat {
	unsigned long merg_size;
	unsigned long staleness;
	unsigned long length;
};

#define init_stat(stat) do {    \
	stat.merg_size = 0;	\
	stat.staleness = 0;	\
	stat.length = 0;	\
} while(0)

struct transaction {
    struct tran_stat stat;
    struct list_head list;
    unsigned int begin;
    unsigned int end;
};

#ifdef __KERNEL__
extern struct kmem_cache *adafs_tran_cachep;
#endif

#define init_tran(tran) do { \
		init_stat((tran)->stat); \
		INIT_LIST_HEAD(&(tran)->list); \
		(tran)->begin = (tran)->end = 0; } while(0)

static inline struct transaction *new_tran(void) {
	struct transaction *tran;
#ifdef __KERNEL__
	tran = (struct transaction *)kmem_cache_alloc(adafs_tran_cachep, GFP_KERNEL);
#else
	tran = (struct transaction *)malloc(sizeof(struct transaction));
#endif
	init_tran(tran);
	return tran;
}

static inline void evict_tran(struct transaction *tran) {
#ifdef __KERNEL__
        kmem_cache_free(adafs_tran_cachep, tran);
#else
        free(tran);
#endif
}

#define is_tran_open(tran) ((tran)->begin == (tran)->end)

struct adafs_log {
    struct log_entry l_entries[LOG_LEN];
    unsigned int l_begin;
    struct mutex l_fmutex;  /* protects l_begin and flushing */

    unsigned int l_head;    /* begin of active entries */
    unsigned int l_end;
    struct list_head l_trans;
    spinlock_t l_tlock;     /* protects l_head, l_end, and l_trans */

    struct kobject l_kobj;
    struct completion l_kobj_unregister;
};

#define L_ENT(log, i) ((log)->l_entries + L_INDEX(i))

#define __log_add_tran(log, tran)	\
	list_add_tail(&(tran)->list, &(log)->l_trans)

#define __log_tail_tran(log)	\
	list_entry((log)->l_trans.prev, struct transaction, list)

static inline void log_init(struct adafs_log *log, struct kset *kset) {
	struct transaction *tran = new_tran();
    log->l_begin = 0;
    log->l_end = 0;
    mutex_init(&log->l_fmutex);

    log->l_head = 0;
    INIT_LIST_HEAD(&log->l_trans);
    spin_lock_init(&log->l_tlock);
    __log_add_tran(log, tran);

    memset(&log->l_kobj, 0, sizeof(struct kobject));
    log->l_kobj.kset = kset;
    init_completion(&log->l_kobj_unregister);
}

static inline struct adafs_log *new_log(struct kset *kset)
{
	struct adafs_log *log;
#ifdef __KERNEL__
	log = (struct adafs_log *)kmalloc(sizeof(struct adafs_log), GFP_KERNEL);
#else
	log = (struct adafs_log *)malloc(sizeof(struct adafs_log));
#endif
	log_init(log, kset);
	return log;
}

extern int log_flush(struct adafs_log *log, unsigned int nr);

static inline void log_destroy(struct adafs_log *log) {
	struct transaction *pos, *tmp;

	list_for_each_entry_safe(pos, tmp, &log->l_trans, list) {
		kmem_cache_free(adafs_tran_cachep, pos);
	}

	kobject_put(&log->l_kobj);
	wait_for_completion(&log->l_kobj_unregister);
}

static inline int __log_seal(struct adafs_log *log) {
	struct transaction *tran = __log_tail_tran(log);

    if (log->l_head == log->l_end) {
        return -ENODATA;
    }

    tran->begin = log->l_head;
    tran->end = log->l_end;
    log->l_head = tran->end;
    return 0;
}

static inline int log_seal(struct adafs_log *log) {
	int err;
	struct transaction *tran = new_tran(); // should not in atomic

	spin_lock(&log->l_tlock);
    err = __log_seal(log);
    if (likely(!err)) __log_add_tran(log, tran);
    spin_unlock(&log->l_tlock);

    if (unlikely(err)) evict_tran(tran);
	return err;
}

static inline int log_append(struct adafs_log *log, struct log_entry *le,
		unsigned int *le_seq)
{
	int err = 0;
	spin_lock(&log->l_tlock);
	if (seq_dist(log->l_begin, log->l_end) < LOG_LEN) {
		*L_ENT(log, log->l_end) = *le;
		if (likely(le_seq)) *le_seq = log->l_end;
		ADAFS_DEBUG(INFO "[adafs] log_append(): " LE_DUMP(le));
		++log->l_end;
	} else err = -EAGAIN;
	spin_unlock(&log->l_tlock);
	return err;
}

static inline unsigned long __log_stal_sum(struct adafs_log *log) {
    unsigned long sum = 0;
    struct transaction *tran;
    list_for_each_entry(tran, &log->l_trans, list) {
        sum += tran->stat.staleness;
    }
    return sum;
}

static inline unsigned long log_stal_sum(struct adafs_log *log) {
    unsigned long sum;
    spin_lock(&log->l_tlock);
    sum = __log_stal_sum(log);
    spin_unlock(&log->l_tlock);
    return sum;
}

extern int __log_sort(struct adafs_log *log, unsigned int begin, unsigned int end);

#ifdef __KERNEL__ /* for sysfs */

struct adafs_log_attr {
	struct attribute attr;
	ssize_t (*show)(struct adafs_log *, char *);
	ssize_t (*store)(struct adafs_log *, const char *, size_t);
};

#define ADAFS_LOG_ATTR(name, mode, show, store) \
		static struct adafs_log_attr adafs_la_##name = __ATTR(name, mode, show, store)
#define ADAFS_RO_LA(name) ADAFS_LOG_ATTR(name, 0444, name##_show, NULL)
#define ADAFS_RW_LA(name) ADAFS_LOG_ATTR(name, 0644, name##_show, name##_store)

#define ADAFS_LA(name) &adafs_la_##name.attr

#endif

#endif
