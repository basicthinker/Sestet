/*
 * ada_trace.h
 *
 *  Created on: Nov 2, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 */

#ifndef ADA_TRACE_H_
#define ADA_TRACE_H_

#include <linux/string.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <asm/segment.h>
#include <asm/uaccess.h>

#define FNAME_MAX_LEN 255
#define FLUSH_LEN (4 * 4 * 1024) //bytes

struct ada_trace {
	char tr_name[FNAME_MAX_LEN];
	struct file *tr_filp;
	unsigned long long tr_len;
	spinlock_t tr_lock;
};

static inline int trace_open(struct ada_trace *trace, const char *filename) {
	if (strnlen(filename, FNAME_MAX_LEN) == FNAME_MAX_LEN) return -EINVAL;
	strcpy(trace->tr_name, FNAME_MAX_LEN);
	trace->tr_filp = filp_open(filename, O_APPEND | O_CREAT, 0644);
	trace->tr_len = 0;
	spin_lock_init(&trace->tr_lock);
	if (IS_ERR(trace->flip)) {
		trace->tr_name = "";
		trace->tr_filp = NULL;
		return PTR_ERR(trace->tr_filp);
	}
	return 0;
}

static inline int trace_close(struct ada_trace *trace) {
	vfs_fsync(trace->tr_filp, 0);
	filp_close(trace->tr_filp, NULL);
}

static inline int trace_write(struct ada_trace *trace,
		const char header, void *data, size_t size) {
	int ret;
	loff_t pos;
	struct timeval tv;
	mm_segment_t fs = get_fs();
	set_fs(get_ds());
	do_gettimeofday(&tv);
	spin_lock(&trace->tr_lock);
	ret = vfs_write(trace->tr_filp, &header, sizeof(header), &pos);
	if (ret > 0) ret = vfs_write(trace->tr_filp, &tv, sizeof(tv), &pos);
	if (ret > 0) ret = vfs_write(trace->tr_filp, data, size, &pos);
	if (ret > 0) trace->tr_len += (sizeof(header) + sizeof(tv) + size);
	spin_unlock(&trace->tr_lock);

	set_fs(fs);

	if (ret > 0 && trace->tr_len >= FLUSH_LEN) {
		ret = vfs_fsync(trace->tr_filp, 0);
		trace->tr_len = 0;
	}
	return -(ret < 0);
}

#define TE_READ		'r'
#define TE_WRITE	'w'
#define TE_META		'm'

struct te_data {
	unsigned long te_ino;
	loff_t te_off;
	size_t te_len;
	char te_hit;
};

#define TE_OP 'd' // delete inode

struct te_meta {
	unsigned long te_ino;
	char te_op;
};

#endif /* ADA_TRACE_H_ */
