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
	struct hlist_node rl_hnode;
	struct page *rl_page;
	unsigned int rl_enti;
};

#define rlog_malloc() \
		((struct rlog *)kmem_cache_alloc(rffs_rlog_cachep, GFP_KERNEL))

#define rlog_free(p) (kmem_cache_free(rffs_rlog_cachep, p))

#define add_rlog(sht, rlog) \
		sht_add_entry(sht, rlog, rl_page, rl_hnode)

#define find_rlog(sht, page) \
		sht_find_entry(sht, page, struct rlog, rl_page, rl_hnode)

#define rl_page(rl)	((rl)->rl_page)
#define rl_assoc_page(rl, page) { \
		get_page((rl)->rl_page); \
		(rl)->rl_page = (page); }

#define rl_enti(rl)				((rl)->rl_enti)
#define rl_set_enti(rl, enti)	((rl)->rl_enti = (enti))

#define evict_rlog(rl) { \
		hlist_del(&rl->rl_hnode); \
		put_page((rl)->rl_page); \
		rlog_free(rl); }

#endif /* RFFS_RLOG_H_ */
