#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>

#define TEST_NUM 10
#define WRITE_SIZE (1024 * 1024 * 4) //bytes

#define sec(tv) (tv.tv_sec + (double)tv.tv_usec / 1000 / 1000)

int main(int argc, char *argv[]) {
  int i, j, fd;
  FILE *fp;
  char file_name[256];
  char content[WRITE_SIZE] = { [0 ... (WRITE_SIZE - 1)] = 'r' };
  double tv_begin, wt_begin[TEST_NUM * 2], wt_end[TEST_NUM * 2];
  int ti = 0;
  struct timeval tv;
 
  gettimeofday(&tv, NULL);
  tv_begin = sec(tv);

  for (i = 0; i < TEST_NUM; ++i) {
    sprintf(file_name, "rffs-test-energy.data-%d-m", i);
    
    fp = fopen(file_name, "w");
    if (!fp) {
      printf("Failed to open file %s.\n", file_name);
      return -1;
    }
    fd = fileno(fp);

    sleep(10);
    gettimeofday(&tv, NULL);
    wt_begin[ti] = sec(tv) - tv_begin;
    for (j = 0; j < 10; ++j) {
      fwrite(content, sizeof(content), 1, fp);
      if (fflush(fp) || fsync(fd)) {
        printf("Failed to flush or fsync file %s part %d.\n", file_name, j);
        return -2;
      }
    }
    gettimeofday(&tv, NULL);
    wt_end[ti] = sec(tv) - tv_begin;
    ++ti;
    fclose(fp);

    sprintf(file_name, "rffs-test-energy.data-%d-s", i);
    
    fp = fopen(file_name, "w");
    if (!fp) {
      printf("Failed to open file %s.\n", file_name);
      return -1;
    }
    fd = fileno(fp);

    sleep(10);
    gettimeofday(&tv, NULL);
    wt_begin[ti] = sec(tv) - tv_begin;
    for (j = 0; j < 10; ++j) {
      fwrite(content, sizeof(content), 1, fp);
    }
    if (fflush(fp) || fsync(fd)) {
      printf("Failed to flush or fsync whole file %s.\n", file_name);
      return -2;
    }
    gettimeofday(&tv, NULL);
    wt_end[ti] = sec(tv) - tv_begin;
    ++ti;
    fclose(fp);
  }

  for (i = 0; i < TEST_NUM * 2; ++i) {
    printf("%.5f\t%.5f\n", wt_begin[i], wt_end[i]);
  }
  return 0;
}

