/*
 * rlog.h
 *
 *  Created on: May 26, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 *  Copyright (C) 2013 Microsoft Research Asia. All rights reserved.
 */

#ifndef RFFS_RLOG_H_
#define RFFS_RLOG_H_

#include "hashtable.h"

#define RLOG_HASH_BITS 10

struct rlog {
	struct page *key;
	struct hlist_node hnode;
	unsigned int enti;
};

extern struct kmem_cache *rffs_rlog_cachep;

#define rlog_malloc() \
	((struct rlog *)kmem_cache_alloc(rffs_rlog_cachep, GFP_KERNEL))
#define rlog_free(p) (kmem_cache_free(rffs_rlog_cachep, p))

#define hash_add_rlog(hashtable, rlog) \
	hlist_add_head(&rlog->hnode, &hashtable[hash_32((u32)(long)rlog->key, RLOG_HASH_BITS)])

#define for_each_possible_rlog(hashtable, obj, key)	\
	hlist_for_each_entry(obj, &hashtable[hash_32((u32)(long)key, RLOG_HASH_BITS)], hnode)

#define for_each_possible_rlog_safe(hashtable, obj, tmp, key)	\
	hlist_for_each_entry_safe(obj, tmp, &hashtable[hash_32((u32)(long)key, RLOG_HASH_BITS)], hnode)

#define hash_find_rlog(hashtable, page) ({			\
	struct rlog *rl;								\
	for_each_possible_rlog(hashtable, rl, page) {	\
		if (rl->key == page) break;					\
	}												\
	rl;												\
})

// rffs_file.c
extern struct hlist_head page_rlog[];

#endif /* RFFS_RLOG_H_ */
