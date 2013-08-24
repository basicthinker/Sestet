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
	unsigned long seq; \
	unsigned long mask; \
	state_t state; \
};

#define FLEX_HISTORY_INIT(bits, item, stat) { \
		.array = (typeof(item) []){[0 ... ((1 << (bits)) - 1)] = (item)}, \
		.mask = (1 << (bits)) - 1, \
		.seq = 0, .state = (stat) }

#define fh_end(fh)	((fh)->seq & (fh)->mask)
#define fh_len(fh)	(likely((fh)->seq > (fh)->mask) ? (fh)->mask + 1 : (fh)->seq)
#define fh_state(fh)	((fh)->state)
#define fh_head_item(fh)	((fh)->array[fh_end(fh)])

#define fh_add(fh, v) { \
		fh_head_item(fh) = *(v); \
		++(fh)->seq; }

#define for_each_history(pos, fh) \
		for (pos = (fh)->array + (fh)->mask; pos >= (fh)->array; --pos)

#define for_each_history_limit(pos, fh, limit) \
		for (pos = (fh)->array + (limit) - 1; pos >= (fh)->array; --pos)


/* For state machine to store latest intervals */

FLEX_CREATE_HISTORY_TYPE(interval, double, double);

#define fh_update_interval(fh, v) { \
		(fh)->state += *(v) - fh_head_item(fh); \
		fh_add(fh, v); }

/* For locator to store latest curve points */

typedef struct {
	double x;
	double y;
} flex_point;

#define FLEX_POINT_INIT	{ 0.0, 0.0 }
#define FLEX_POINT_ZERO	((flex_point)FLEX_POINT_INIT)

typedef struct {
	double s_x;
	double s_y;
	double s_x2;
	double s_xy;
	double slope;
} flex_line_state;

#define FLEX_LINE_STATE_INIT	{ 0.0, 0.0, 0.0, 0.0, 0.0 }
#define FLEX_LINE_STATE_ZERO	((flex_line_state)FLEX_LINE_STATE_INIT)

static inline double inc_fit_linear(flex_point *op, flex_point *np,
		flex_line_state *stat, int n)
{
	stat->s_x += np->x - op->x;
	stat->s_y += np->y - op->y;
	stat->s_x2 += np->x * np->x - op->x * op->x;
	stat->s_xy += np->x * np->y - op->x * op->y;
	return (n * stat->s_xy - stat->s_x * stat->s_y) /
			(n * stat->s_x2 - stat->s_x * stat->s_x);
}

FLEX_CREATE_HISTORY_TYPE(curve, flex_point, flex_line_state);

#define fh_update_curve(fh, v) { \
		fh_state(fh).slope = inc_fit_linear(&fh_head_item(fh), v, \
				&fh_state(fh), fh_len(fh)); \
		fh_add(fh, v); }

#endif /* POLICY_UTIL_H_ */
