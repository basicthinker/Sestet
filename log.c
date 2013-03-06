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

static void copy_entries(const struct log_pos *begin, const struct log_pos *end,
    struct seg_entry *entries[]) {
  unsigned int i = 0;
  unsigned int len = 0;
  if (begin->seg == end->seg) {
    len = end->entry - begin->entry;
    for (i = 0; i < len; ++i) {
      entries[i] = (struct seg_entry *)begin->seg + i;
    }
  } else {
    struct segment *seg;
    unsigned int j = 0;
    i = 0;
    len = SEG_LEN - begin->entry;
    for (; i < len; ++i) {
      entries[i] = begin->seg->entries + i;
    }
    for (seg = begin->seg->next; seg != end->seg; seg = seg->next) {
      len += SEG_LEN;
      for (j = 0; i < len; ++i, ++j) {
        entries[i] = seg->entries + j;
      }
    }
    len += end->entry;
    for (j = 0; i < len; ++i, ++j) {
      entries[i] = end->seg->entries + j;
    }
  }
}

struct seg_entry **log_sort(const struct log_pos *begin,
    const struct log_pos *end,
    unsigned int *length) { // output
  const unsigned int len = (SEG_LEN - begin->entry) + end->entry + 
      SEG_LEN * (end->seg->seq_num - begin->seg->seq_num - 1);
  struct seg_entry **entries = (struct seg_entry **)MALLOC(len * sizeof(struct seg_entry));
  copy_entries(begin, end, entries);
  sort_entries(entries, len);

  *length = len;
  return entries;
}