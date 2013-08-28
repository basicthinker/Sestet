#include <stdio.h>
#include <stdlib.h>
#include "policy_util.h"

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define LEN_BITS 2
#define INVAL_TIME 1024.0

double THR_INT = 1.0;
double THR_M = 3.0;

enum state {
  ST_CON = 0,
  ST_DIS = 1,
  ST_DIS_DOWN,
  ST_DIS_UP
};

enum event {
  EV_USER,
  EV_TIMER // either threshold or predicted value passed
};

static inline double min_time(double a, double b) {
  return a < b ? a : b;
}

#define update_hist_int(ts, s, in) \
    (fh_update_interval(&(ts)[(s)].int_hist, &(in)))

#define update_timer(ts, s) ({ \
    struct flex_interval_history *fh = &(ts)[(s)].int_hist; \
    (ts)[(s)].timer = fh->seq ? fh_state(fh) / fh_len(fh) : THR_INT; })

#define predict_int(ts) update_timer(ts, ST_DIS)
//#define threshold(ts) (min_time((ts)[ST_CON].timer * THR_M, THR_INT))
#define threshold(ts) ((ts)[ST_CON].timer * THR_M)

int num_conflicts = 0;
int num_pred=0;
double total_len = 0.0;
#define rec_result(ts, in) do { \
    double pred = (ts)[ST_DIS].timer; \
    ++num_pred; \
    total_len += pred; \
    if (pred > in) ++num_conflicts; \
    printf("%f\t%f\n", pred, in); \
} while (0)

// Returns timer value
double transfer(struct flex_touch_state ts[],
    enum state *s, enum event ev, double in);

int main(int argc, char *argv[]) {
  struct flex_touch_state ts[2] = { FLEX_TOUCH_STATE_INIT(LEN_BITS), FLEX_TOUCH_STATE_INIT(LEN_BITS) };
  enum state s;
  double log_int;
  double time_out;

  if (argc != 4) {
    fprintf(stderr, "Usage: %s EventLog MultiThreshold IntervalThreshold\n",
        argv[0]);
    return -1;
  }
  
  freopen(argv[1], "r", stdin);
  THR_M = atof(argv[2]);
  THR_INT = atof(argv[3]);

  s = ST_CON;
  time_out = 1;
  while (scanf("%lf", &log_int) == 1) {
    while (log_int > time_out) {
      log_int -= time_out;
      time_out = transfer(ts, &s, EV_TIMER, log_int + time_out);
    }
    time_out = transfer(ts, &s, EV_USER, log_int);
  }
  printf("Summary:\n%d\t%f\t%f\n", num_conflicts, total_len/num_pred, total_len);
}

double transfer(struct flex_touch_state ts[],
    enum state *s, enum event ev, double in) {
  struct flex_interval_history *fh;
  switch (*s) {
  case ST_CON:
    if (ev == EV_USER) {
      update_hist_int(ts, ST_CON, in);
      update_timer(ts, ST_CON);
      return threshold(ts);
    } else if (ev == EV_TIMER) {
      *s = ST_DIS;
      return predict_int(ts);
    } else fprintf(stderr, "Invalid event %d on state %d\n", ev, *s);
    break;
  case ST_DIS:
    rec_result(ts, in);
    if (ev == EV_USER) {
      if (in <= threshold(ts)) {
        *s = ST_CON;
        update_hist_int(ts, ST_CON, in);
        update_timer(ts, ST_CON);
        return threshold(ts);
      } else {
        *s = ST_DIS_DOWN;
        update_hist_int(ts, ST_DIS, in);
        return threshold(ts);
      }
    } else if (ev == EV_TIMER) {
      *s = ST_DIS_UP;
    } else fprintf(stderr, "Invalid event %d on state %d\n", ev, *s); 
    break;
  case ST_DIS_DOWN:
    if (ev == EV_USER) {
      *s = ST_CON;
      update_hist_int(ts, ST_CON, in);
      update_timer(ts, ST_CON);
      return threshold(ts);
    } else if (ev == EV_TIMER) {
      *s = ST_DIS;
      return predict_int(ts);
    } else fprintf(stderr, "Invalid event %d on state %d\n", ev, *s);
    break;
  case ST_DIS_UP:
    if (ev == EV_USER) {
      *s = ST_CON;
      update_hist_int(ts, ST_DIS, in);
      return threshold(ts);
    } else fprintf(stderr, "Invalid event %d on state %d\n", ev, *s);
    break;
  }
  return INVAL_TIME;
}
