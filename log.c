//
//  log.c
//  sestet-rffs
//
//  Created by Jinglei Ren on 2/27/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#include "log.h"

static void sort_entries(struct seg_entry *entries[], unsigned int len) {
}

static void copy_entries(struct log_pos *begin, struct log_pos *end,
    struct seg_entry *entries[]) {
  unsigned int i = 0;
  while (begin->seg != end->seg) {

  }
}

struct seg_entry **log_sort(struct log_pos *begin, struct log_pos *end,
    int *length) { // output
  const unsigned int len = (SEG_LEN - begin->entry) + end->entry + 
      SEG_LEN * (end->seg->seq_num - begin->seg->seq_num - 1);
  struct seg_entry **entries = (struct seg_entry **)MALLOC(len * sizeof(struct seg_entry));
  copy_entries(begin, end, entries);
  sort_entries(entries, len);

  *length = len;
  return entries;
}