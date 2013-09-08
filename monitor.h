#include <sys/param.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

/*
 * Fork the current process to make daemon
 * @dir: new working directory. No change with `pwd` if it is NULL.
*/
static inline void init_daemon(const char *dir) {
  int pid;
  int i;
  if ((pid = fork())) {
    exit(0); // parent process
  } else if (pid < 0) {
    exit(-1);
  }

  // child process
  setsid();
  if ((pid = fork())) {
    exit(0); // session leader
  } else if (pid < 0) {
    exit(-1);
  }

  // daemon process
  for (i = 0; i < NOFILE; ++i) {
    close(i);
  }

  if (dir != NULL) {
    chdir(dir);
  }
}

#define sec(tv) (tv->tv_sec + (double)tv->tv_usec / 1000 / 1000)

static inline double get_time(struct timeval *tv) {
  gettimeofday(tv, NULL);
  return sec(tv);
}

struct cpu_stat {
  long long unsigned user;
  long long unsigned nice;
  long long unsigned system;
  long long unsigned idle;
  long long unsigned iowait;
  long long unsigned irq;
  long long unsigned softirq; 
};

#define stat_sum(statp) \
    ((statp)->user + (statp)->nice + (statp)->system + (statp)->idle + \
    (statp)->iowait + (statp)->irq + (statp)->softirq)

static inline int cpu_util_init(struct cpu_stat *stat) {
  FILE *fp;
  fp = fopen("/proc/stat", "r");
  if (!fp) return -EIO;

  fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu",
    &stat->user, &stat->nice, &stat->system, &stat->idle,
    &stat->iowait, &stat->irq, &stat->softirq);

  return fclose(fp);
}

static inline double cpu_util(struct cpu_stat *stat) {
  struct cpu_stat cur;
  double rate;
  FILE *fp = fopen("/proc/stat", "r");
  if (!fp) return 0;

  fscanf(fp, "cpu %llu %llu %llu %llu %llu %llu %llu",
    &cur.user, &cur.nice, &cur.system, &cur.idle,
    &cur.iowait, &cur.irq, &cur.softirq);

  fclose(fp);

  rate = (double)(cur.idle - stat->idle)/(stat_sum(&cur) - stat_sum(stat));
  *stat = cur;
  return 1 - rate;
}

static inline int cpu_freq(uint32_t *freq, int num) {
  char path[64];
  int i;
  FILE *fp;
  for (i = 0; i < num; ++i) {
    sprintf(path, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
    fp = fopen(path, "r");
    if (!fp) break;
    fscanf(fp, "%u", freq + i);
    fclose(fp);
  }
  return i;
}

#define WAKE_LOCK "write-energy-bench-wl"

static inline int wake_lock(void) {
  FILE *fp = fopen("/sys/power/wake_lock", "a");
  if (!fp) {
    return -EIO;
  }
  fprintf(fp, "%s", WAKE_LOCK);
  return fclose(fp);
}

static inline int wake_unlock(void) {
  FILE *fp = fopen("/sys/power/wake_unlock", "a");
  if (!fp) {
    return -EIO;
  }
  fprintf(fp, "%s", WAKE_LOCK);
  return fclose(fp);
}

static inline int set_cpufreq(uint32_t freq, int cpu) {
  char path[64];
  FILE *fp;
  sprintf(path, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed", cpu);
  fp = fopen(path, "w");
  if (!fp) {
    return -EIO;
  }
  fprintf(fp, "%u", freq);
  return fclose(fp);
}

