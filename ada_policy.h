/*
 * policy.h
 *
 *  Created on: May 22, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 *  Copyright (C) 2013 Microsoft Research Asia. All rights reserved.
 */

#ifndef ADAFS_POLICY_H_
#define ADAFS_POLICY_H_

#define ADAFS_TRAN_LIMIT 256 // in blocks
extern unsigned int stal_limit_blocks;

#include "ada_log.h"

#define print_stat(info, log, stat) \
		ADAFS_TRACE(KERN_INFO "[adafs-stat] log(%d) %s: staleness=%lu, merged=%lu, len=%lu\n", \
				(int)((log) - adafs_logs), info, \
				(stat)->staleness, (stat)->merg_size, (stat)->length)

#define on_write_old_page(log, size) do { \
		struct tran_stat *sp, stat; \
		spin_lock(&(log)->l_tlock); \
		sp = &__log_tail_tran(log)->stat; \
		sp->merg_size += size; \
		sp->staleness += size; \
		stat = *sp; \
		spin_unlock(&(log)->l_tlock); \
		print_stat("on write old", log, &stat); \
		if (stat.staleness >= ADAFS_TRAN_LIMIT) { \
			log_seal(log); \
			if (seq_dist(l_begin(log), l_end(log)) >= stal_limit_blocks) \
				wake_up_process(adafs_flusher); \
		} } while (0)

#define on_write_new_page(log, size) do { \
		struct tran_stat *sp, stat; \
		spin_lock(&(log)->l_tlock); \
		sp = &__log_tail_tran(log)->stat; \
		sp->staleness += size; \
		sp->length += 1; \
		stat = *sp; \
		spin_unlock(&(log)->l_tlock); \
		print_stat("on write new", log, &stat); \
		if (stat.staleness >= ADAFS_TRAN_LIMIT) { \
			log_seal(log); \
			if (seq_dist(l_begin(log), l_end(log)) >= stal_limit_blocks) \
				wake_up_process(adafs_flusher); \
		} } while (0)

#define on_evict_page(log, size) do { \
		struct tran_stat *sp, stat; \
		spin_lock(&(log)->l_tlock); \
		sp = &__log_tail_tran(log)->stat; \
		sp->staleness += 1; \
		sp->merg_size += (size); \
		sp->length -= 1; \
		stat = *sp; \
		spin_unlock(&(log)->l_tlock); \
		print_stat("on evict page", log, &stat); \
		} while (0)

#endif /* ADAFS_POLICY_H_ */
