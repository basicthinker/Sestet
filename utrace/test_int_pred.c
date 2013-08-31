#include <stdio.h>
#include <stdlib.h>
#include "ada_policy_util.h"

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

char *state_name[] = {
    [ST_CON] = "ST_CON",
    [ST_DIS] = "ST_DIS",
    [ST_DIS_DOWN] = "ST_DIS_DOWN",
    [ST_DIS_UP] = "ST_DIS_UP" };

enum event {
  EV_USER,
  EV_TIMER // either threshold or predicted value passed
};

char *event_name[] = {
    [EV_USER] = "EV_USER",
    [EV_TIMER] = "EV_TIMER" };

#define min_time(a, b) ((a) < (b) ? (a) : (b))

static inline void trace_event(enum event ev, double in, enum state s) {
  fprintf(stderr, "%s input=%f: %s => ", event_name[ev], in, state_name[s]);
}

static inline void trace_state(enum state ns, double timeout) {
  fprintf(stderr, "%s timeout=%f\n", state_name[ns], timeout == INVAL_TIME ? -1 : timeout);
}

#define update_hist_int(ts, s, in) \
    (fh_update_interval(&(ts)[(s)].int_hist, &(in)))

#define update_timer(ts, s) ({ \
    struct adafs_interval_history *fh = &(ts)[(s)].int_hist; \
    (ts)[(s)].timer = fh->seq ? fh_state(fh) / fh_len(fh) : THR_INT; })

#define predict_int(ts) update_timer(ts, ST_DIS)
//#define threshold(ts) (min_time((ts)[ST_CON].timer * THR_M, THR_INT))
#define threshold(ts) \
    ((ts)[ST_CON].int_hist.seq ? (ts)[ST_CON].timer * THR_M : THR_INT)

int num_conflicts = 0;
int num_pred=0;
double total_len = 0.0;
#define rec_result(ts, in) do { \
    double pred = (ts)[ST_DIS].timer; \
    ++num_pred; \
    total_len += pred; \
    if (pred > in) ++num_conflicts; \
} while (0)

// Returns timer value
double transfer(struct adafs_touch_state ts[],
    enum state *s, enum event ev, double in);

int main(int argc, char *argv[]) {
  struct adafs_touch_state ts[2] = { ADAFS_TOUCH_STATE_INIT(LEN_BITS), ADAFS_TOUCH_STATE_INIT(LEN_BITS) };
  enum state s;
  double log_int;
  double timeout;

  if (argc != 4) {
    fprintf(stderr, "Usage: %s EventLog MultiThreshold IntervalThreshold\n",
        argv[0]);
    return -1;
  }
  
  freopen(argv[1], "r", stdin);
  THR_M = atof(argv[2]);
  THR_INT = atof(argv[3]);

  s = ST_CON;
  timeout = THR_INT;
  while (scanf("%lf", &log_int) == 1) {
    while (log_int > timeout) {
      log_int -= timeout;
      trace_event(EV_TIMER, log_int + timeout, s);
      timeout = transfer(ts, &s, EV_TIMER, log_int + timeout);
      trace_state(s, timeout);
    }
    trace_event(EV_USER, log_int, s);
    timeout = transfer(ts, &s, EV_USER, log_int);
    trace_state(s, timeout);
  }
  printf("%s:\t%d\t%d\t%f\t%f\n", argv[1],
      num_conflicts, num_pred, total_len, total_len/num_pred);
  return 0;
}

double transfer(struct adafs_touch_state ts[],
    enum state *s, enum event ev, double in) {
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
