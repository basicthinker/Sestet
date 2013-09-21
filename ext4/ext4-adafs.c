#include <linux/fs.h>
#include <linux/jbd2.h>
#include <linux/buffer_head.h>
#include "ext4.h"
#include "ext4_jbd2.h"

#include "ada_fs.h"

extern int walk_page_buffers(handle_t *handle, struct buffer_head *head,
		unsigned from, unsigned to,
		int *partial,
		int (*fn)(handle_t *handle, struct buffer_head *bh));
extern int bget_one(handle_t *handle, struct buffer_head *bh);
extern int bput_one(handle_t *handle, struct buffer_head *bh);
extern int ext4_set_bh_endio(struct buffer_head *bh, struct inode *inode);
extern void ext4_end_io_buffer_write(struct buffer_head *bh, int uptodate);
extern int do_journal_get_write_access(handle_t *handle,
		struct buffer_head *bh);
extern int write_end_fn(handle_t *handle, struct buffer_head *bh);

extern int noalloc_get_block_write(struct inode *inode, sector_t iblock,
		struct buffer_head *bh_result, int create);
extern int ext4_bh_delay_or_unwritten(handle_t *handle, struct buffer_head *bh);


//inode.c
static inline int __adafs_journalled_writepage(handle_t *handle, struct page *page,
				       unsigned int len)
{
	struct address_space *mapping = page->mapping;
	struct inode *inode = mapping->host;
	struct buffer_head *page_bufs;
	int ret = 0;
	int err;

	ClearPageChecked(page);
	page_bufs = page_buffers(page);
	BUG_ON(!page_bufs);
	walk_page_buffers(handle, page_bufs, 0, len, NULL, bget_one);
	/* As soon as we unlock the page, it can go away, but we have
	 * references to buffers so we are safe */
	unlock_page(page);

	//handle = ext4_journal_start(inode, ext4_writepage_trans_blocks(inode));
	//if (IS_ERR(handle)) {
		//ret = PTR_ERR(handle);
		//goto out;
	//}

	BUG_ON(!ext4_handle_valid(handle));

	ret = walk_page_buffers(handle, page_bufs, 0, len, NULL,
				do_journal_get_write_access);

	err = walk_page_buffers(handle, page_bufs, 0, len, NULL,
				write_end_fn);
	if (ret == 0)
		ret = err;
	//err = ext4_journal_stop(handle);
	//if (!ret)
		//ret = err;

	walk_page_buffers(handle, page_bufs, 0, len, NULL, bput_one);
	ext4_set_inode_state(inode, EXT4_STATE_JDATA);
//out:
	return ret;
}

//inode.c
static inline int adafs_writepage(handle_t *handle, struct page *page,
		unsigned int len, struct writeback_control *wbc)
{
	int ret = 0, commit_write = 0;
	//loff_t size;
	//unsigned int len;
	struct buffer_head *page_bufs = NULL;
	struct inode *inode = page->mapping->host;

	//trace_ext4_writepage(page);
	//size = i_size_read(inode);
	//if (page->index == size >> PAGE_CACHE_SHIFT)
		//len = size & ~PAGE_CACHE_MASK;
	//else
		//len = PAGE_CACHE_SIZE;

	/*
	 * If the page does not have buffers (for whatever reason),
	 * try to create them using __block_write_begin.  If this
	 * fails, redirty the page and move on.
	 */
	if (!page_has_buffers(page)) {
		if (__block_write_begin(page, 0, len,
					noalloc_get_block_write)) {
		redirty_page:
			redirty_page_for_writepage(wbc, page);
			unlock_page(page);
			return -EIO;
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
		goto redirty_page;
	}
	if (commit_write)
		/* now mark the buffer_heads as dirty and uptodate */
		block_commit_write(page, 0, len);

	if (PageChecked(page) && ext4_should_journal_data(inode))
		/*
		 * It's mmapped pagecache.  Add buffers and journal it.  There
		 * doesn't seem much point in redirtying the page here.
		 */
		return __adafs_journalled_writepage(handle, page, len);

	if (buffer_uninit(page_bufs)) {
		ext4_set_bh_endio(page_bufs, inode);
		ret = block_write_full_page_endio(page, noalloc_get_block_write,
					    wbc, ext4_end_io_buffer_write);
	} else
		ret = block_write_full_page(page, noalloc_get_block_write, wbc);

	return ret;
}

static inline int adafs_sync_file(struct inode *inode, tid_t commit_tid)
{
	//struct inode *inode = file->f_mapping->host;
	//struct ext4_inode_info *ei = EXT4_I(inode);
	journal_t *journal = EXT4_SB(inode->i_sb)->s_journal;
	int ret;
	//tid_t commit_tid;
	bool needs_barrier = false;

	J_ASSERT(ext4_journal_current_handle() == NULL);

	//trace_ext4_sync_file_enter(file, datasync);

	if (inode->i_sb->s_flags & MS_RDONLY)
		return 0;

	ret = ext4_flush_completed_IO(inode);
	if (ret < 0)
		goto out;

	BUG_ON(!journal);

	/*
	 * data=writeback,ordered:
	 *  The caller's filemap_fdatawrite()/wait will sync the data.
	 *  Metadata is in the journal, we wait for proper transaction to
	 *  commit here.
	 *
	 * data=journal:
	 *  filemap_fdatawrite won't do anything (the buffers are clean).
	 *  ext4_force_commit will write the file data into the journal and
	 *  will wait on that.
	 *  filemap_fdatawait() will encounter a ton of newly-dirtied pages
	 *  (they were dirtied by commit).  But that's OK - the blocks are
	 *  safe in-journal, which is all fsync() needs to ensure.
	 */
	if (ext4_should_journal_data(inode)) {
		//ret = ext4_force_commit(inode->i_sb); /* We already set handle->h_sync */
		goto out;
	}

	//commit_tid = datasync ? ei->i_datasync_tid : ei->i_sync_tid;
	if (journal->j_flags & JBD2_BARRIER &&
	    !jbd2_trans_will_send_data_barrier(journal, commit_tid))
		needs_barrier = true;
	jbd2_log_start_commit(journal, commit_tid);
	ret = jbd2_log_wait_commit(journal, commit_tid);
	if (needs_barrier)
		blkdev_issue_flush(inode->i_sb->s_bdev, GFP_KERNEL, NULL);
 out:
	//trace_ext4_sync_file_exit(inode, ret);
	return ret;
}


static handle_t *adafs_trans_begin(struct inode *inode, int nles)
{
	int blocks_per_page = jbd2_journal_blocks_per_page(inode);
	int nblocks = nles * blocks_per_page;
	handle_t *handle = ext4_journal_start(inode, nblocks);
	if (!IS_ERR(handle)) {
		handle->h_sync = 1;
	}
	return handle;
}

static int adafs_entry_flush(handle_t *handle, struct log_entry *le,
		struct writeback_control *wbc)
{
	return adafs_writepage(handle, le_page(le), le_len(le), wbc);
}

static int adafs_trans_end(handle_t *handle)
{
	int err;

	BUG_ON(!ext4_handle_valid(handle));

	err = ext4_journal_stop(handle);
	if (unlikely(err)) {
		printk(KERN_ERR "[adafs] adafs_trans_end fails at ext4_journal_stop: %d\n", err);
	}
	return err;
}

static int adafs_wait_sync(struct inode *inode, tid_t commit_tid)
{
	return adafs_sync_file(inode, commit_tid);
}

const struct flush_operations adafs_fops = {
	.trans_begin = adafs_trans_begin,
	.entry_flush = adafs_entry_flush,
	.trans_end = adafs_trans_end,
	.wait_sync = adafs_wait_sync
};
