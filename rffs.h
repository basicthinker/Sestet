/*
 * rffs.h
 *
 *  Created on: May 20, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 *  Copyright (C) 2013 Microsoft Research Asia. All rights reserved.
 */

#ifndef RFFS_H_
#define RFFS_H_

#include <linux/types.h>

struct kiocb;
struct inovec;

extern struct page *rffs_grab_cache_page_write_begin(struct address_space *mapping,
					pgoff_t index, unsigned flags);

extern ssize_t rffs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
		unsigned long nr_segs, loff_t pos);

extern int init_rffs(void);
extern void exit_rffs(void);

#endif /* RFFS_H_ */
