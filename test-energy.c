#include <stdio.h>
#include <unistd.h>

#define TEST_NUM 3

int main(int argc, char *argv[]) {
  int i, j, fd, err;
  FILE *fp;
  char file_name[256];
  char content[4096] = { [0 ... 4094] = 'r' };
  content[4095] = '\0';
  
  sleep(15);

  for (i = 0; i < TEST_NUM; ++i) {
    sprintf(file_name, "rffs-test-energy.data-%d-m", i);
    
    fp = fopen(file_name, "wt");
    if (!fp) {
      printf("Failed to open file %s.\n", file_name);
      return -1;
    }
    fd = fileno(fp);
    for (j = 0; j < 10; ++j) {
      fprintf(fp, "%s\n", content);
      err = fdatasync(fd);
      if (err) {
        printf("Failed to fdatasync file %s part %d: %d\n", file_name, j, err);
        return -2;
      }
    }
    fclose(fp);
    sleep(10);

    sprintf(file_name, "rffs-test-energy.data-%d-s", i);
    
    fp = fopen(file_name, "wt");
    if (!fp) {
      printf("Failed to open file %s.\n", file_name);
      return -1;
    }
    fd = fileno(fp);
    for (j = 0; j < 10; ++j) {
      fprintf(fp, "%s\n", content);
    }
    err = fdatasync(fd);
    if (err) {
      printf("Failed to fdatasync whole file %s: %d\n", file_name, err);
      return -2;
    }
    fclose(fp);
    sleep(10);
  }
  return 0;
}

