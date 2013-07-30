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
		get_page(page); \
		(rl)->rl_page = (page); }

#define rl_enti(rl)				((rl)->rl_enti)
#define rl_set_enti(rl, enti)	((rl)->rl_enti = (enti))

#define RL_DUMP(rl) "ino=%lu, mapping=%p, index=%lu, page=%p, enti=%d", \
		rl_page(rl)->mapping && rl_page(rl)->mapping->host ? rl_page(rl)->mapping->host->i_ino : LE_INVAL_INO, \
		rl_page(rl)->mapping, rl_page(rl)->index, rl_page(rl), rl_enti(rl)

#define assoc_rlog(rl, page, enti, sht) { \
		rl_assoc_page(rl, page); \
		rl_set_enti(rl, enti); \
		RFFS_TRACE(INFO "[rffs] assoc_rlog(): " RL_DUMP(rl)); \
		add_rlog(sht, rl); }

#define evict_rlog(rl) { \
		hlist_del(&rl->rl_hnode); \
		RFFS_TRACE(INFO "[rffs] evict_rlog(): " RL_DUMP(rl)); \
		put_page((rl)->rl_page); \
		rlog_free(rl); }

#endif /* RFFS_RLOG_H_ */
