#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "monitor.h"
#include "btrfs_snap.h"

#define PAGE_SIZE 4096

void init_data(char *data, int len, char c) {
  int i;
  for (i = 0; i < len; ++i) {
    data[i] = c;
  }
}

int main(int argc, char *argv[]) {
  int i, num_pages, sleep_time, mode;
  int fd;
  char *data;
  struct timeval tv;
  double time_begin, time_end;
  char snap_name[BTRFS_VOL_NAME_MAX];
  char *subvol;
  struct timeval *time;
 
  if (argc != 5) {
    printf("Usage: %s TargetFile NumPages SleepTime Mode[0=sync | 1=journal | 2=snapshot]\n", argv[0]);
    return -1;
  }

  fd = open(argv[1], O_RDWR | O_CREAT);
  if (fd < 0) {
    printf("Failed to open file: %d.\n", fd);
    return -1;
  }
  subvol = dirname(argv[1]);

  num_pages = atoi(argv[2]);
  data = malloc(PAGE_SIZE * num_pages);

  sleep_time = atoi(argv[3]);
  mode = atoi(argv[4]);

  sleep(sleep_time);
  fprintf(stderr, "Test begins...\n");

  for (i = 0; i < 16; ++i) {
    sleep(sleep_time);
    init_data(data, PAGE_SIZE * num_pages, i + 'a');

    time_begin = get_time(&tv); 
    write(fd, data, PAGE_SIZE * num_pages);
    if (mode == 0 && (i % 2 == 1)) fsync(fd);
    else if (mode == 1 && (i % 4 == 3)) fsync(fd);
    else if (mode == 2 && (i % 4 == 3)) {
      gettimeofday(time, NULL);
      sprintf(snap_name, "%s-%lu", subvol, time->tv_sec);
      btrfs_snap(subvol, snap_name);
      fsync(fd);
    }
    time_end = get_time(&tv);
    printf("%d\t%.5f\n", mode, time_end - time_begin);

    lseek(fd, 0, SEEK_SET);
  }

  free(data);
  close(fd);
  return 0;
}

/*
 * Test Runs
 *
 * 1. Normal run
 *   Check data after ejection: 't'.
 * 2. Unexpected removal after 5 fsync's (stdout lines)
 *   Check data after removal: 'h'. (If we do not use journal, this would be 'j'.)
*/

