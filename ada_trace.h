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
	struct mutex tr_mtx;

#ifdef ADA_TRACE
    struct kobject tr_kobj;
    struct completion tr_kobj_unregister;
	int tr_on;
#endif
};

static inline int adafs_trace_open(struct adafs_trace *trace,
		const char *filename, struct kset *kset) {
#ifdef ADA_TRACE
	if (strnlen(filename, FNAME_MAX_LEN) == FNAME_MAX_LEN) return -EINVAL;
	strcpy(trace->tr_name, filename);

	trace->tr_filp = filp_open(filename, O_WRONLY | O_APPEND | O_CREAT, 0644);
	if (IS_ERR(trace->tr_filp)) {
		trace->tr_name[0] = '\0';
		trace->tr_filp = NULL;
		return PTR_ERR(trace->tr_filp);
	}
	printk(KERN_INFO "[adafs] AdaFS tracing is up.\n");

	trace->tr_len = 0;
	mutex_init(&trace->tr_mtx);
	trace->tr_on = 0;

	memset(&trace->tr_kobj, 0, sizeof(struct kobject));
	trace->tr_kobj.kset = kset;
#endif
	return 0;
}

static inline void adafs_trace_close(struct adafs_trace *trace) {
#ifdef ADA_TRACE
	if (!trace->tr_filp) return;
	vfs_fsync(trace->tr_filp, 0);
	filp_close(trace->tr_filp, NULL);
#endif
}

static inline int adafs_trace_output(struct adafs_trace *trace,
		void *data, size_t size) {
	int ret = 0;
#ifdef ADA_TRACE
	struct timeval tv;
	mm_segment_t fs = get_fs();
	struct file *filp = trace->tr_filp;
	if (!filp) return ret;

	set_fs(get_ds());
	do_gettimeofday(&tv);
	mutex_lock(&trace->tr_mtx);
	ret = vfs_write(filp, (const char *)&tv, sizeof(tv), &filp->f_pos);
	if (ret > 0) ret = vfs_write(filp, data, size, &filp->f_pos);
	if (ret > 0) trace->tr_len += (sizeof(tv) + size);
	mutex_unlock(&trace->tr_mtx);
	set_fs(fs);

	if (ret > 0 && trace->tr_len >= FLUSH_LEN) {
		vfs_fsync(filp, 0);
		trace->tr_len = 0;
	}
#endif
	return ret;
}

#define TE_TYPE_READ  'r'
#define TE_TYPE_WRITE 'w'
#define TE_TYPE_FSYNC 's'
#define TE_TYPE_EVICT 'e'

#define TE_HIT_YES     'y'
#define TE_HIT_NO      'n'
#define TE_HIT_UNKNOWN 'u'

struct te_page {
	char te_type;
	char te_hit;
	unsigned long te_ino;
	unsigned long te_pgi;
};

static inline void adafs_trace_page(struct adafs_trace *trace, char te_type,
		unsigned long te_ino, unsigned long te_pgi, char te_hit) {
#ifdef ADA_TRACE
	if (trace->tr_on) {
		struct te_page te = { .te_type = te_type, .te_hit = te_hit,
				.te_ino = te_ino, .te_pgi = te_pgi };
		int err = adafs_trace_output(trace, &te, sizeof(te));
		if (err < 0) {
			printk(KERN_ERR "[adafs] adafs_trace_output() failed: %d\n", err);
		}
	}
#endif
}

#endif /* ADA_TRACE_H_ */
