/*
 * policy_util.h
 *
 *  Created by Jinglei Ren on Aug 22, 2013
 *  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
 */

#ifndef RFFS_POLICY_UTIL_H_
#define RFFS_POLICY_UTIL_H_

/* Basic Template for History Item Array */

#define FLEX_CREATE_HISTORY_TYPE(name, item_t, state_t) \
struct flex_##name##_history { \
	item_t *array; \
	unsigned long end; \
	unsigned long mask; \
	state_t state; \
};

#define FLEX_HISTORY_INIT(bits, item, stat) { \
		.array = (typeof(item) []){[0 ... ((1 << (bits)) - 1)] = (item)}, \
		.mask = (1 << (bits)) - 1, \
		.end = 0, .state = (stat) }

#define fh_end(fh)	((fh)->end & (fh)->mask)
#define fh_len(fh)	(likely((fh)->end > (fh)->mask) ? (fh)->mask + 1 : (fh)->end)
#define fh_state(fh)	((fh)->state)
#define fh_head_item(fh)	((fh)->array[fh_end(fh)])

#define fh_add(fh, v) { \
		fh_head_item(fh) = (v); \
		++(fh)->end; }

#define for_each_history(pos, fh) \
		for (pos = (fh)->array + (fh)->mask; pos >= (fh)->array; --pos)

#define for_each_history_limit(pos, fh, limit) \
		for (pos = (fh)->array + (limit) - 1; pos >= (fh)->array; --pos)


/* For state machine to store latest intervals */

FLEX_CREATE_HISTORY_TYPE(interval, double, double);

#define fh_update_interval(fh, v) { \
		(fh)->state += (v) - fh_head_item(fh); \
		fh_add(fh, v); }

#endif /* POLICY_UTIL_H_ */
