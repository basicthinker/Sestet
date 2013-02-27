//
//  log.h
//  sestet-rffs
//
//  Created by Jinglei Ren on 2/24/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#ifndef SESTET_RFFS_LOG_H_
#define SESTET_RFFS_LOG_H_

#include "sys.h"

#define SEG_LEN 10240 // 10k
#define CHUNK_SIZE 4096 // 512 bytes

struct seg_entry {
  unsigned long int inode_id;
  unsigned long int chunk_begin; // FFFFFFFF denotes inode entry
  unsigned long int chunk_end;
  char *data; // refers to inode info when this is inode entry
};

struct segment {
  struct seg_entry entries[SEG_LEN];
  struct segment *prev;
  struct segment *next;
  unsigned int seq_num;
};

static inline struct segment *seg_new(unsigned int seq_num) {
  struct segment *seg = (struct segment *)MALLOC(sizeof(struct segment));
  seg->prev = seg->next = 0;
  seg->seq_num = seq_num;
  return seg;
}

static inline void seg_free(struct segment *seg) {
  MFREE(seg);
}

static inline void seg_insert(struct segment *prev_seg) {
  struct segment *new_seg = seg_new();
  struct segment *next_seg = prev_seg->next;

  prev_seg->next = new_seg;
  new_seg->prev = prev_seg;
  next_seg->prev = new_seg;
  new_seg->next = next_seg;
}

struct log_pos { // only for internal use
  struct segment *seg;
  unsigned int entry;
};

struct rffs_log {
  struct log_pos log_begin; // begin of sealed entries
  struct log_pos log_head; // begin of active entries
  struct log_pos log_end;
  spinlock_t lock;
};

static inline struct rffs_log *log_new() {
  struct segment *seg = seg_new(0);
  struct rffs_log *log = (struct rffs_log *)MALLOC(sizeof(struct rffs_log));

  seg->prev = seg->next = seg;

  log->log_begin.seg = log->log_head.seg = log->log_end.seg = seg;
  log->log_begin.entry = log->log_head.entry = log->log_end.entry = 0;
  spin_lock_init(&log->lock);
  return log;
}

static inline void log_free(struct rffs_log *log) {
  struct segment *seg = log->log_begin;
  while (seg->next != log->log_begin) {
    seg = seg->next;
    seg_free(seg->prev);
  }
  seg_free(seg);

  spin_lock_destroy(&log->lock);
  MFREE(log);
}

static inline void log_append(struct rffs_log *log, struct seg_entry *entry) {
  struct segment *end_seg = log->log_end;
  spin_lock(&log->lock);
    if (seg->seg_end < SEG_LEN) {
    seg->entries[seg->seg_end] = *entry;
    return ++seg->seg_end;
  } else if (seg->seg_end == SEG_LEN) {
    return 0;
  } else {
    ERR_PRINT("[__seg_append] Out of index.");
    return 0;
  }
  spin_unlock(&log->lock);
}

void log_seal(struct rffs_log *log,
              struct log_pos *trans_begin, // output
              struct log_pos *trans_end); // output

// Returns sorted array of pointers to seg_entry
void *log_sort(struct log_pos *begin, struct log_pos *end,
               int *length); // output

#endif
