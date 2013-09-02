/*
 * policy.h
 *
 *  Created on: May 22, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 *  Copyright (C) 2013 Microsoft Research Asia. All rights reserved.
 */

#ifndef ADAFS_POLICY_H_
#define ADAFS_POLICY_H_

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

#define print_stat(info, log, stat) \
		printk(KERN_INFO "[adafs-stat] log(%d) %s: staleness=%lu, merged=%lu, len=%lu\n", \
				(int)((log) - adafs_logs), info, \
				(stat)->staleness, (stat)->merg_size, (stat)->length)

#define on_write_old_page(log, size) do { \
		struct tran_stat *sp, stat; \
		spin_lock(&(log)->l_lock); \
		sp = &__log_tail_tran(log)->stat; \
		sp->merg_size += size; \
		sp->staleness += size; \
		stat = *sp; \
		spin_unlock(&(log)->l_lock); \
		print_stat("on write old", log, &stat); \
		if (stat.staleness >= staleness_limit) { \
			/* TODO: little chance for race if not protected */ \
			log_seal(log); \
			if (L_DIST((log)->l_begin, (log)->l_head) >= 16) \
				wake_up_process(adafs_flusher); \
		} } while (0)

#define on_write_new_page(log, size) do { \
		struct tran_stat *sp, stat; \
		spin_lock(&(log)->l_lock); \
		sp = &__log_tail_tran(log)->stat; \
		sp->staleness += size; \
		sp->length += 1; \
		stat = *sp; \
		spin_unlock(&(log)->l_lock); \
		print_stat("on write new", log, &stat); \
		if (stat.staleness >= staleness_limit) { \
			/* TODO: little chance for race if not protected */ \
			log_seal(log); \
			if (L_DIST((log)->l_begin, (log)->l_head) >= 16) \
				wake_up_process(adafs_flusher); \
		} } while (0)

#define on_evict_page(log, size) do { \
		struct tran_stat *sp, stat; \
		spin_lock(&(log)->l_lock); \
		sp = &__log_tail_tran(log)->stat; \
		sp->staleness += 1; \
		sp->merg_size += (size); \
		sp->length -= 1; \
		stat = *sp; \
		spin_unlock(&(log)->l_lock); \
		print_stat("on evict page", log, &stat); \
		} while (0)

#endif /* ADAFS_POLICY_H_ */
