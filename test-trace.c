#include <stdio.h>

int main(int argc, char *argv[]) {
  int i;
  FILE *fp;
  char content[1024] = { [0 ... 1022] = 'r' };
  content[1023] = '\0';
  
  fp = fopen("rffs-test-trace.data", "wt");
  if (!fp) {
    printf("Failed to open file.\n");
    return -1;
  }

  // merge
  for (i = 0; i < 10; ++i) {
    fprintf(fp, "%s\n", content);
    rewind(fp);
  }
  // new
  for (i = 0; i < 10; ++i) {
    fprintf(fp, "%s\n", content);
  }

  fclose(fp);
  return 0;
}
