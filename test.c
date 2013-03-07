//
//  test.c
//  sestet-rffs
//
//  Created by Jinglei Ren on 2/24/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>

#include "log.h"

#define RAND_CHUNK (rand() % 1024)
#define RAND_INODE (rand() % 1024)

//#define ALLOC_DATA

int main(int argc, const char *argv[]) {
  struct rffs_log *log = log_new();
  unsigned int i = 0, j = 0;
  struct seg_entry entry;
  struct log_pos begin, end;
  struct entry_array arr;

  // test copy within one segment
  for (; i < SEG_LEN / 2; ++i) {
    entry.chunk_begin = RAND_CHUNK;
    entry.chunk_end = entry.chunk_begin + RAND_CHUNK + 1;
#ifdef ALLOC_DATA
    entry.data = MALLOC((entry.chunk_end - entry.chunk_begin) * 512);
#else
    entry.data = 0;
#endif
    entry.inode_id = RAND_INODE;
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
    entry.chunk_begin = RAND_CHUNK;
    entry.chunk_end = entry.chunk_begin + RAND_CHUNK;
#ifdef ALLOC_DATA
    entry.data = MALLOC((entry.chunk_end - entry.chunk_begin) * 512);
#else
    entry.data = 0;
#endif
    entry.inode_id = RAND_INODE;
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
  for (; i < SEG_LEN * 8 + SEG_LEN / 2; ++i) {
    entry.chunk_begin = RAND_CHUNK;
    entry.chunk_end = entry.chunk_begin + RAND_CHUNK;
#ifdef ALLOC_DATA
    entry.data = MALLOC((entry.chunk_end - entry.chunk_begin) * 512);
#else
    entry.data = 0;
#endif
    entry.inode_id = RAND_INODE;
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