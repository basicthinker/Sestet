#include <stdio.h>

#define PAGE_SIZE 4096

void init_page(char *page, char c) {
  int i;
  for (i = 0; i < PAGE_SIZE; ++i) {
    page[i] = c;
  }
}

int main(int argc, char *argv[]) {
  int i;
  FILE *fp;
  char page[PAGE_SIZE];

  if (argc != 2) {
    printf("Usage: %s TargetFile\n", argv[0]);
    return -1;
  }

  fp = fopen(argv[1], "wt");
  if (!fp) {
    printf("Failed to open file.\n");
    return -1;
  }

  // merge
  for (i = 0; i < 6; ++i) {
    init_page(page, i + '0');
    fwrite(page, sizeof(page), 1, fp);
    fflush(fp);
    if (i % 2) rewind(fp);
  }

  fclose(fp);
  return 0;
}
