/*
 * rffs_file.c
 *
 *  Created on: May 20, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 *  Copyright (C) 2013 Microsoft Research Asia. All rights reserved.
 */

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/fs.h>
#include <asm/errno.h>

#include "rffs.h"
#include "log.h"
#include "hashtable.h"

#define RLOG_HASH_BITS 10
#define MAX_LOG_NUM 20

static struct rffs_log rffs_log[MAX_LOG_NUM];
static atomic_t logi;

struct rlog {
	struct hlist_node hnode;
	int enti;
};

static struct kmem_cache *rffs_rlog_cachep;

#define rlog_malloc() \
	((struct rlog *)kmem_cache_alloc(rffs_rlog_cachep, GFP_KERNEL))
#define rlog_free(p) (kmem_cache_free(rffs_rlog_cachep, p))

static DEFINE_HASHTABLE(page_rlog, RLOG_HASH_BITS);

#define hash_add_rlog(hashtable, key, rlogp) \
	hlist_add_head(rlogp->hnode, &hashtable[hash_32((u32)key, RLOG_HASH_BITS)])

int init_rffs(void)
{
	atomic_set(&logi, -1);
	rffs_rlog_cachep = kmem_cache_create("rffs_rlog_cache", sizeof(struct rlog), 0,
			(SLAB_RECLAIM_ACCOUNT | SLAB_MEM_SPREAD), NULL);
	if (!rffs_rlog_cachep) return -ENOMEM;
	return 0;
}

int rffs_mount(void)
{
	int li = atomic_inc_return(&logi);
	if (li < MAX_LOG_NUM) {
		log_init(rffs_log + li);
		return 0;
	} else return -ENOSPC;
}

void exit_rffs(void)
{
	kmem_cache_destroy(rffs_rlog_cachep);
}

ssize_t rffs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos)
{
	return generic_file_aio_write(iocb, iov, nr_segs, pos);
}

