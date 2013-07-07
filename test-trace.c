#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include "monitor.h"

#define PAGE_SIZE 4096

void init_data(char *data, int len, char c) {
  int i;
  for (i = 0; i < len; ++i) {
    data[i] = c;
  }
}

int main(int argc, char *argv[]) {
  int i, num_pages, sleep_time, is_fsync;
  int fd;
  char *data;
  struct timeval tv;
  double time_init, time_begin, time_end;

  time_init = get_time(&tv);

  if (argc < 4) {
    printf("Usage: %s TargetFile NumPages SleepTime [IsFsync=1]\n", argv[0]);
    return -1;
  }

  fd = open(argv[1], O_RDWR | O_CREAT);
  if (fd < 0) {
    printf("Failed to open file: %d.\n", fd);
    return -1;
  }

  num_pages = atoi(argv[2]);
  data = malloc(PAGE_SIZE * num_pages);

  sleep_time = atoi(argv[3]);

  if (argc == 5) is_fsync = atoi(argv[4]);
  else is_fsync = 1;

  // merge
  for (i = 0; i < 20; ++i) {
    init_data(data, PAGE_SIZE * num_pages, i + 'a');
    write(fd, data, PAGE_SIZE * num_pages);
    lseek(fd, 0, SEEK_SET);

    if (i % 2) {
      time_begin = get_time(&tv) - time_init;
      if (is_fsync) fsync(fd);
      time_end = get_time(&tv) - time_init;

      printf("%d\t%.5f\t%.5f\n", is_fsync, time_begin, time_end);
      sleep(sleep_time);
    }
  }

  free(data);
  close(fd);
  return 0;
}

/*
  Test Runs
  1. Normal run
    Check dmesg log: 5 rffs flushes, each with NUM_PAGES/2 valid entries.
    Check data after removal: 't'.
  2. Unexpected removal after 5 fsync's (stdout lines)
    Check dmesg log: should have 2 rffs flushes.
    Check data after removal: 'h'.
  3. Disable fsync and repeat the above.
*/

