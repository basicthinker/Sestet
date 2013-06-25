#include <linux/fs.h>
#include "ext4.h"
#include "ext4_jbd2.h"
#include "ext4-rffs.h"

//inode.c
static inline int __rffs_journalled_writepage(handle_t *handle, struct page *page,
				       unsigned int len)
{
	//struct address_space *mapping = page->mapping;
	//struct inode *inode = mapping->host;
	struct buffer_head *page_bufs;
	int ret = 0;
	int err;

	//ClearPageChecked(page); // done by rffs
	page_bufs = page_buffers(page);
	BUG_ON(!page_bufs);
	walk_page_buffers(handle, page_bufs, 0, len, NULL, bget_one);
	/* As soon as we unlock the page, it can go away, but we have
	 * references to buffers so we are safe */
	unlock_page(page);

	BUG_ON(!ext4_handle_valid(handle));

	ret = walk_page_buffers(handle, page_bufs, 0, len, NULL,
				do_journal_get_write_access);

	err = walk_page_buffers(handle, page_bufs, 0, len, NULL,
				write_end_fn);
	if (ret == 0)
		ret = err;

	walk_page_buffers(handle, page_bufs, 0, len, NULL, bput_one);
	//ext4_set_inode_state(inode, EXT4_STATE_JDATA); // moved to rffs_writepage()
	return ret;
}

//fs/buffer.c
static inline int rffs_block_commit_write(struct inode *inode, struct page *page,
		unsigned from, unsigned to)
{
	unsigned block_start, block_end;
	int partial = 0;
	unsigned blocksize;
	struct buffer_head *bh, *head;

	blocksize = 1 << inode->i_blkbits;

	for(bh = head = page_buffers(page), block_start = 0;
	    bh != head || !block_start;
	    block_start=block_end, bh = bh->b_this_page) {
		block_end = block_start + blocksize;
		if (block_end <= from || block_start >= to) {
			if (!buffer_uptodate(bh))
				partial = 1;
		} else {
			set_buffer_uptodate(bh);
			mark_buffer_dirty(bh);
		}
		clear_buffer_new(bh);
	}

	/*
	 * If this is a partial write which happened to make all buffers
	 * uptodate then we can optimize away a bogus readpage() for
	 * the next read(). Here we 'discover' whether the page went
	 * uptodate as a result of this (potentially partial) write.
	 */
	if (!partial)
		SetPageUptodate(page);
	return 0;
}

//inode.c
static inline int rffs_writepage(handle_t *handle, struct page *page, unsigned int len)
{
	int commit_write = 0;
	//loff_t size;
	//unsigned int len;
	struct buffer_head *page_bufs = NULL;
	struct inode *inode = page->mapping->host;

	//trace_ext4_writepage(page);
	__lock_page(page);

	//size = i_size_read(inode);
	//if (page->index == size >> PAGE_CACHE_SHIFT)
	//	len = size & ~PAGE_CACHE_MASK;
	//else
	//	len = PAGE_CACHE_SIZE;

	/*
	 * If the page does not have buffers (for whatever reason),
	 * try to create them using __block_write_begin.  If this
	 * fails, redirty the page and move on.
	 */
	if (!page_has_buffers(page)) {
		if (__block_write_begin(page, 0, len,
					noalloc_get_block_write)) {
			unlock_page(page);
			return 0;
		}
		commit_write = 1;
	}
	page_bufs = page_buffers(page);
	if (walk_page_buffers(NULL, page_bufs, 0, len, NULL,
			      ext4_bh_delay_or_unwritten)) {
		/*
		 * We don't want to do block allocation, so redirty
		 * the page and return.  We may reach here when we do
		 * a journal commit via journal_submit_inode_data_buffers.
		 * We can also reach here via shrink_page_list
		 */
		unlock_page(page);
		return 0;
	}
	if (commit_write) {
		if (PageChecked(page)) page->mapping = NULL; // prevents further dirty operation
		/* now mark the buffer_heads as dirty and uptodate */
		rffs_block_commit_write(inode, page, 0, len);
	}

	ext4_set_inode_state(inode, EXT4_STATE_JDATA);
	return __rffs_journalled_writepage(handle, page, len);
}

static handle_t *rffs_trans_begin(int npages, void *data) {
	struct inode *inode = (struct inode *)data;
	int nblocks = npages * jbd2_journal_blocks_per_page(inode);
	return ext4_journal_start_sb(inode->i_sb, nblocks);
}

static int rffs_ent_flush(handle_t *handle, struct log_entry *ent) {
	return rffs_writepage(handle, (struct page *)ent->data, ent->len);
}

static int rffs_trans_end(handle_t *handle) {
	return ext4_journal_stop(handle);
}

const struct flush_operations rffs_fops = {
	.trans_begin = rffs_trans_begin,
	.ent_flush = rffs_ent_flush,
	.trans_end = rffs_trans_end,
};
