//
//  log.c
//  sestet-rffs
//
//  Created by Jinglei Ren on 2/27/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#include "log.h"

#define CUTOFF 0
#define STACK_SIZE 32

#define SWAP_ENTRY(a, b) {  \
  struct seg_entry *tmp;    \
  tmp = a;                  \
  a = b;                    \
  b = tmp;                  \
}

static inline int comp_entry(struct seg_entry *a, struct seg_entry *b) {
  if (a->inode_id < b->inode_id) {
    return -1;
  } else if (a->inode_id == b->inode_id) {
    return (int)(a->chunk_begin - b->chunk_begin);
  } else return 1;
}

static void short_sort(struct seg_entry *entries[],
                       unsigned int begin, unsigned int end) {
  unsigned int i, max, sort;
  if (end - begin < 2) {
    return;
  }
  for (sort = end - 1; sort > begin; --sort) {
    max = sort;
    for (i = 0; i < sort; ++i) {
      if (comp_entry(entries[i], entries[max]) > 0) {
        max = i;
      }
    }
    SWAP_ENTRY(entries[max], entries[sort]);
  }
}

static unsigned int partition(struct seg_entry *entries[],
                              unsigned int begin, unsigned int end) {
  unsigned int mid = (begin + end) / 2;
  struct seg_entry *pivot = entries[mid];
  unsigned int left = begin + 1;
  unsigned int right = end - 1;
  SWAP_ENTRY(entries[mid], entries[begin]);
  
  while (left < right) {
    while (left < end && comp_entry(entries[left], pivot) < 0) {
      ++left;
    }
    while (comp_entry(entries[right], pivot) > 0) {
      --right;
    }
    if (left == right) {
      return right; // no matter where pivot points to
    } else if (left < right) {
      SWAP_ENTRY(entries[left], entries[right]);
      ++left;
      --right;
    }
  }
  SWAP_ENTRY(entries[right], entries[begin]);
  return right;
}

static void sort_entries(struct seg_entry *entries[], unsigned int len) {
  unsigned int stk_left[STACK_SIZE];
  unsigned int stk_right[STACK_SIZE];
  unsigned int stk_top = 0;
  unsigned int mid = 0;
  
  // push (0, len)
  stk_left[stk_top] = 0;
  stk_right[stk_top] = len;
  ++stk_top;
  
  while (stk_top) {
    // pop
    unsigned int begin = stk_left[stk_top - 1];
    unsigned int end = stk_right[stk_top - 1];
    --stk_top;
    
    if (end - begin <= CUTOFF) {
      short_sort(entries, begin, end);
    } else {
      mid = partition(entries, begin, end);
      stk_left[stk_top] = begin;
      stk_right[stk_top] = mid;
      ++stk_top;
      stk_left[stk_top] = mid + 1;
      stk_right[stk_top] = end;
      ++stk_top;
    }
  }
}

static void copy_entries(const struct log_pos *begin, const struct log_pos *end,
    struct seg_entry *entries[]) {
  unsigned int i = 0;
  unsigned int len = 0;
  struct seg_entry *base;
  if (begin->seg == end->seg) {
    len = end->entry - begin->entry;
    base = begin->seg->entries + begin->entry;
    for (i = 0; i < len; ++i) {
      entries[i] = base + i;
    }
  } else {
    struct segment *seg;
    unsigned int j = 0;
    i = 0;
    len = SEG_LEN - begin->entry;
    base = begin->seg->entries + begin->entry;
    for (; i < len; ++i) {
      entries[i] = base + i;
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

struct entry_array log_sort(const struct log_pos *begin,
    const struct log_pos *end) {
  const unsigned int len = (SEG_LEN - begin->entry) + end->entry + 
      SEG_LEN * (end->seg->seq_num - begin->seg->seq_num - 1);
  struct entry_array arr;
  entry_array_init(&arr, len);
  copy_entries(begin, end, arr.entries);
  sort_entries(arr.entries, len);
  return arr;
}