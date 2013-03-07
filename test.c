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
  unsigned int i = 0, j = 0;
  struct seg_entry entry;
  struct log_pos begin, end;
  struct entry_array arr;

  // test copy within one segment
  for (; i < SEG_LEN / 2; ++i) {
    entry.chunk_begin = i;
    entry.chunk_end = i + 1;
    entry.data = 0;
    entry.inode_id = i;
    log_append(log, &entry);
  }
  log_seal(log, &begin, &end);

  arr = log_sort(&begin, &end);
  printf("inode ids of %u entries (seg %u - %u):\n",
      arr.len, begin.seg->seq_num, end.seg->seq_num);
  for (j = 0; j < arr.len; ++j) {
    printf(" %lu", arr.entries[j]->inode_id);
  }
  printf("\n");
  entry_array_clear(&arr);

  // test copy cross two segments
  for (; i < SEG_LEN + SEG_LEN / 2; ++i) {
    entry.chunk_begin = i;
    entry.chunk_end = i + 1;
    entry.data = 0;
    entry.inode_id = i;
    log_append(log, &entry);
  }
  log_seal(log, &begin, &end);
  
  arr = log_sort(&begin, &end);
  printf("inode ids of %u entries (seg %u - %u):\n",
      arr.len, begin.seg->seq_num, end.seg->seq_num);
  for (j = 0; j < arr.len; ++j) {
    printf(" %lu", arr.entries[j]->inode_id);
  }
  printf("\n");
  entry_array_clear(&arr);
  
  // test copy cross multiple segments
  for (; i < SEG_LEN * 4 + SEG_LEN / 2; ++i) {
    entry.chunk_begin = i;
    entry.chunk_end = i + 1;
    entry.data = 0;
    entry.inode_id = i;
    log_append(log, &entry);
  }
  log_seal(log, &begin, &end);
  
  arr = log_sort(&begin, &end);
  printf("inode ids of %u entries (seg %u - %u):\n",
      arr.len, begin.seg->seq_num, end.seg->seq_num);
  for (j = 0; j < arr.len; ++j) {
    printf(" %lu", arr.entries[j]->inode_id);
  }
  printf("\n");
  entry_array_clear(&arr);

  log_free(log);
  return 0;
}