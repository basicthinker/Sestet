//
//  ext4-adafs.h
//  sestet-adafs
//
//  Created by Jinglei Ren on 6/20/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#ifndef ADAFS_EXT4_ADAFS_H_
#define ADAFS_EXT4_ADAFS_H_

#include <linux/fs.h>
#include <linux/jbd2.h>
#include <linux/buffer_head.h>

#include "ada_log.h"

extern const struct flush_operations adafs_fops;

extern int walk_page_buffers(handle_t *handle,
                             struct buffer_head *head,
                             unsigned from,
                             unsigned to,
                             int *partial,
                             int (*fn)(handle_t *handle,
                                       struct buffer_head *bh));

extern int bget_one(handle_t *handle, struct buffer_head *bh);
extern int bput_one(handle_t *handle, struct buffer_head *bh);

extern int do_journal_get_write_access(handle_t *handle,
		struct buffer_head *bh);

extern int write_end_fn(handle_t *handle, struct buffer_head *bh);

extern int noalloc_get_block_write(struct inode *inode, sector_t iblock,
		struct buffer_head *bh_result, int create);

extern int ext4_bh_delay_or_unwritten(handle_t *handle, struct buffer_head *bh);

#endif // ADAFS_EXT4_ADAFS_H_
