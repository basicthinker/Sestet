/*
 * policy.h
 *
 *  Created on: May 22, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 *  Copyright (C) 2013 Microsoft Research Asia. All rights reserved.
 */

#ifndef RFFS_POLICY_H_
#define RFFS_POLICY_H_

extern unsigned long staleness_limit;

struct tran_stat {
	unsigned long merg_size;
	unsigned long staleness;
	unsigned long length;
};

#define init_stat(stat) do {    \
	stat.merg_size = 0;	\
	stat.staleness = 0;	\
	stat.length = 0;	\
} while(0)

#define opt_ratio(tran_stat)	\
	((float)tran_stat.merg_size/tran_stat.staleness)
#define ls_ratio(tran_stat)	\
	((float)tran_stat.length/tran_stat.staleness)

#define on_write_old_page(logp, tran_stat, size) {	\
	tran_stat.merg_size += size;	\
	tran_stat.staleness += size;	\
	printk(KERN_INFO "[rffs-stat] log(%d) on write old: staleness=%lu, merged=%lu, len=%lu\n", \
			(int)(logp - rffs_logs), tran_stat.staleness, tran_stat.merg_size, tran_stat.length); \
	if (tran_stat.staleness >= staleness_limit) {	\
		log_seal(logp);	\
		if (L_DIST(logp->l_begin, logp->l_head) >= 16)	\
			wake_up_process(rffs_flusher);	\
	}	\
}

#define on_write_new_page(logp, tran_stat, size) {	\
	tran_stat.staleness += size; \
	tran_stat.length += 1; \
	printk(KERN_INFO "[rffs-stat] log(%d) on write new: staleness=%lu, merged=%lu, len=%lu\n", \
			(int)(logp - rffs_logs), tran_stat.staleness, tran_stat.merg_size, tran_stat.length); \
	if (tran_stat.staleness >= staleness_limit) { \
		log_seal(logp); \
		if (L_DIST(logp->l_begin, logp->l_head) >= 16) \
			wake_up_process(rffs_flusher); \
	} \
}

#endif /* RFFS_POLICY_H_ */
