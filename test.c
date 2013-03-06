//
//  test.c
//  sestet-rffs
//
//  Created by Jinglei Ren on 2/24/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#include <stdio.h>

#include "log.h"

int main(int argc, const char *argv[]) {
  struct rffs_log *log = log_new();
  int i = 0, len = 0;
  struct seg_entry entry;
  struct log_pos begin, end;
  struct seg_entry **entries;

  for (i = 0; i < SEG_LEN / 2; ++i) {
    entry.chunk_begin = i;
    entry.chunk_end = i + 1;
    entry.data = 0;
    entry.inode_id = i;
    log_append(log, &entry);
  }
  log_seal(log, &begin, &end);

  entries = log_sort(&begin, &end, &len);
  printf("inode ids of entries:\n");
  for (i = 0; i < len; ++i) {
    printf("\t%d", entries[i]->inode_id);
  }
  printf("\n");

  log_free(log);
  return 0;
}