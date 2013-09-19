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
  int i, num_pages, sleep_time;
  int fd;
  char *data;
  struct timeval tv;
  double time_begin, time_end;

  if (argc != 4) {
    printf("Usage: %s TargetFile NumPages SleepTime\n", argv[0]);
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

  for (i = 0; i < 16; ++i) {
    sleep(sleep_time);
    init_data(data, PAGE_SIZE * num_pages, i + 'a');

    time_begin = get_time(&tv); 
    write(fd, data, PAGE_SIZE * num_pages);
    if (i % 2) fsync(fd);
    time_end = get_time(&tv);
    printf("%.5f\n", time_end - time_begin);

    lseek(fd, 0, SEEK_SET);
  }

  free(data);
  close(fd);
  return 0;
}

/*
 * Test Runs
 *
 * The policy should be:
 * (1) to seal when staleness is over 2 * num_pages pages, and
 * (2) to flush when length is over 2 * num_pages.
 *
 * 1. Normal run
 *   Check dmesg log: 4 AdaFS flushes, each with num_pages valid entries.
 *   Check data after removal: 't'.
 * 2. Unexpected removal after 5 fsync's (stdout lines)
 *   Check dmesg log: should have 2 adafs flushes.
 *   Check data after removal: 'h'. (If we use Ext4, this would be 'j'.)
 * 3. Disable fsync and repeat the above.
 *
 * A typical configuration:
 * for 8 MB each write, num_pages = 2048,
 * AdaFS transaction limit = 16777216, staleness limit = 4096.
*/

