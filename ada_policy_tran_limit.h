/*
 * policy.h
 *
 *  Created on: May 22, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 *  Copyright (C) 2013 Microsoft Research Asia. All rights reserved.
 */

#ifndef ADAFS_POLICY_H_
#define ADAFS_POLICY_H_

#include "ada_policy_util.h"

#define ADAFS_TRAN_LIMIT 16128 // in blocks, 63 MB

#define on_write_old_page(log, size) do { \
		struct tran_stat *sp, stat; \
		spin_lock(&(log)->l_lock); \
		sp = &__log_tail_tran(log)->stat; \
		sp->merg_size += size; \
		sp->staleness += size; \
		stat = *sp; \
		spin_unlock(&(log)->l_lock); \
		print_stat("on write old", log, &stat); \
		if (stat.staleness >= ADAFS_TRAN_LIMIT) { \
			/* TODO: little chance for race if not protected */ \
			log_seal(log); \
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
		if (stat.staleness >= ADAFS_TRAN_LIMIT) { \
			/* TODO: little chance for race if not protected */ \
			log_seal(log); \
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
