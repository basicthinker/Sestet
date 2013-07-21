/*
 * rlog.h
 *
 *  Created on: May 26, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 *  Copyright (C) 2013 Microsoft Research Asia. All rights reserved.
 */

#ifndef RFFS_RLOG_H_
#define RFFS_RLOG_H_

#include <linux/hash.h>
#include "shashtable.h"

struct rlog {
	struct page *key;
	struct hlist_node hnode;
	unsigned int enti;
};

#define rlog_malloc() \
		((struct rlog *)kmem_cache_alloc(rffs_rlog_cachep, GFP_KERNEL))

#define rlog_free(p) (kmem_cache_free(rffs_rlog_cachep, p))

#define hash_add_rlog(sht, rlog) \
		sht_add_entry(sht, rlog, key, hnode)

#define hash_find_rlog(sht, page) \
		sht_find_entry(sht, page, struct rlog, key, hnode)

#endif /* RFFS_RLOG_H_ */
