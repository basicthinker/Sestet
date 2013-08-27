#include <stdio.h>
#include <math.h>
#include "policy_util.h"

#define THR_INT 1
#define THR_M 3

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

#define update_hist_int(ts, s, in) \
    (fh_update_interval(&(ts)[(s)].int_hist, &(in)))

#define update_timer(ts, s) ({ \
    struct flex_interval_history *fh = &(ts)[(s)].int_hist; \
    (ts)[(s)].timer = fh_state(fh) / fh_len(fh); })

#define predict_int(ts) update_timer(ts, ST_DIS)
#define threshold(ts) (fmin((ts)[ST_CON].timer * THR_M, THR_INT))

// Returns timer value
static inline double transfer(struct flex_touch_state ts[],
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
    if (ev == EV_USER) {
      if (in <= threshold(ts)) {
        *s = ST_CON;
        update_hist_int(ts, ST_CON, in);
        update_timer(ts, ST_CON);
        return threshold(ts);
      } else {
        *s = ST_DIS_DOWN;
        update_hist_int(ts, ST_DIS, in);
        return thrshold(ts);
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
      return thrshold(ts);
    } else fprintf(stderr, "Invalid event %d on state %d\n", ev, *s);
    break;
  }
  return -1;
}

int main(int argc, char *argv[]) {
  struct flex_touch_state ts[2];
  enum state s;
  
}
