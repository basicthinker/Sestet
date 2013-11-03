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

struct adafs_trace {
	char tr_name[FNAME_MAX_LEN];
	struct file *tr_filp;
	unsigned long long tr_len;
	spinlock_t tr_lock;
};

static inline int adafs_trace_open(struct adafs_trace *trace, const char *filename) {
#ifdef ADA_TRACE
	if (strnlen(filename, FNAME_MAX_LEN) == FNAME_MAX_LEN) return -EINVAL;
	strcpy(trace->tr_name, filename);
	trace->tr_filp = filp_open(filename, O_APPEND | O_CREAT, 0644);
	trace->tr_len = 0;
	spin_lock_init(&trace->tr_lock);
	if (IS_ERR(trace->tr_filp)) {
		trace->tr_name[0] = '\0';
		trace->tr_filp = NULL;
		return PTR_ERR(trace->tr_filp);
	}
#endif
	return 0;
}

static inline void adafs_trace_close(struct adafs_trace *trace) {
#ifdef ADA_TRACE
	vfs_fsync(trace->tr_filp, 0);
	filp_close(trace->tr_filp, NULL);
#endif
}

static inline int adafs_trace_write(struct adafs_trace *trace,
		const char header, void *data, size_t size) {
	int ret = 0;
#ifdef ADA_TRACE
	loff_t pos;
	struct timeval tv;
	mm_segment_t fs = get_fs();
	set_fs(get_ds());
	do_gettimeofday(&tv);
	spin_lock(&trace->tr_lock);
	ret = vfs_write(trace->tr_filp, (const char *)&header, sizeof(header), &pos);
	if (ret > 0) ret = vfs_write(trace->tr_filp, (const char *)&tv, sizeof(tv), &pos);
	if (ret > 0) ret = vfs_write(trace->tr_filp, data, size, &pos);
	if (ret > 0) trace->tr_len += (sizeof(header) + sizeof(tv) + size);
	spin_unlock(&trace->tr_lock);

	set_fs(fs);

	if (ret > 0 && trace->tr_len >= FLUSH_LEN) {
		vfs_fsync(trace->tr_filp, 0);
		trace->tr_len = 0;
	}
#endif
	return ret;
}

#define TE_READ  'r'
#define TE_WRITE 'w'
#define TE_EVICT 'e'

#define TE_HIT_YES     'y'
#define TE_HIT_NO      'n'
#define TE_HIT_UNKNOWN 'u'

struct te_page {
	char te_hit;
	unsigned long te_ino;
	unsigned long te_pgi;
};

static inline void adafs_trace_page(struct adafs_trace *trace, char header,
		unsigned long te_ino, unsigned long te_pgi, char te_hit) {
#ifdef ADA_TRACE
	struct te_page te = { .te_ino = te_ino, .te_pgi = te_pgi, .te_hit = te_hit };
	int err = adafs_trace_write(trace, header, &te, sizeof(te));
	if (err < 0) {
		printk(KERN_ERR "[adafs] adafs_trace_write() failed: %d\n", err);
	}
#endif
}

#endif /* ADA_TRACE_H_ */
