/*
 * policy.h
 *
 *  Created on: May 22, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 *  Copyright (C) 2013 Microsoft Research Asia. All rights reserved.
 */

#ifndef RFFS_POLICY_H_
#define RFFS_POLICY_H_

struct tran_stat {
	unsigned long merg_size;
	unsigned long staleness;
	unsigned long latency;
};

#define init_stat(stat) do {    \
	stat.merg_size = 0;	\
	stat.staleness = 0;	\
	stat.latency = 0;	\
} while(0)

#define opt_ratio(tran_stat)	\
	((float)tran_stat.merg_size/tran_stat.staleness)
#define ls_ratio(tran_stat)	\
	((float)tran_stat.latency/tran_stat.staleness)

#define num_pages 8

#define on_write_old_page(logp, tran_stat, size) {	\
	tran_stat.merg_size += size;	\
	tran_stat.staleness += size;	\
	if (tran_stat.staleness >= (2 * num_pages * PAGE_SIZE)) {	\
		log_seal(logp);	\
		if (L_DIST(logp->l_begin, logp->l_head) >= 2 * num_pages)	\
			wake_up_process(rffs_flusher);	\
	}	\
}

#define on_write_new_page(logp, tran_stat, size) {	\
	tran_stat.staleness += size;	\
	if (tran_stat.staleness >= (2 * num_pages * PAGE_SIZE)) {	\
		log_seal(logp);	\
		if (L_DIST(logp->l_begin, logp->l_head) >= 2 * num_pages)	\
			wake_up_process(rffs_flusher);	\
	}	\
	\
	tran_stat.latency += 1;	\
}

#endif /* RFFS_POLICY_H_ */
