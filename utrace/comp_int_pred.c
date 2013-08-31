#include <stdio.h>
#include <stdlib.h>
#include "ada_policy_util.h"

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define LEN_BITS 2

double THR_INT = 1.0;

#define min_time(a, b) ((a) < (b) ? (a) : (b))

#define NUM_TESTS 2
#define TEST_MIN 0
#define TEST_AVG 1

int num_confl[NUM_TESTS];
int num_pred[NUM_TESTS];
double last_pred[NUM_TESTS];
double total_len[NUM_TESTS];

static void init_pred(double pred) {
  int i;
  for (i = 0; i < NUM_TESTS; ++i) {
    num_confl[i] = 0;
    num_pred[i] = 0;
    last_pred[i] = pred;
    total_len[i] = 0.0;
  }
}

static void update_pred(struct adafs_interval_history *int_hist, double in) {
  double min = 1024, *pos;
  fh_update_interval(int_hist, &in);
  for_each_history(pos, int_hist) {
    if (*pos < min) min = *pos;
  }
  last_pred[TEST_MIN] = min;
  last_pred[TEST_AVG] = fh_state(int_hist) / fh_len(int_hist);
}

static void update_stat() {
  int i;
  for (i = 0; i < NUM_TESTS; ++i) {
    ++num_pred[i];
    total_len[i] += last_pred[i];
  }
}

static void check_pred(double in) {
  int i;
  for (i = 0; i < NUM_TESTS; ++i) {
    if (last_pred[i] > in) ++num_confl[i];
  }
}

static void print_stat(char *filename) {
  int i;
  for (i = 0; i < NUM_TESTS; ++i) {
    printf("%s:\t%d\t%d\t%f\t%f\n", filename,
        num_confl[i], num_pred[i], total_len[i], total_len[i]/num_pred[i]);
  }
}

int main(int argc, char *argv[]) {
  struct adafs_interval_history int_hist = ADAFS_HISTORY_INIT(LEN_BITS, 0.0, 0.0);
  double log_int;
  if (argc != 3) {
    fprintf(stderr, "Usage: %s EventLog IntervalThreshold\n",
        argv[0]);
    return -1;
  }
  
  freopen(argv[1], "r", stdin);
  THR_INT = atof(argv[2]);
  
  init_pred(THR_INT);
  while (scanf("%lf", &log_int) == 1) {
    check_pred(log_int);
    update_pred(&int_hist, log_int);
    update_stat();
  }
  print_stat(argv[1]);
  return 0;
}

