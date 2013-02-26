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
  unsigned int seg_begin;
  unsigned int seg_end;
  spinlock_t lock;
};

static inline struct segment *seg_new() {
  struct segment *seg = (struct segment *)MALLOC(sizeof(struct segment));
  seg->prev = seg->next = 0;
  seg->seg_begin = 0xFFFFFFFF;
  seg->seg_end = 0;
  spin_lock_init(&seg->lock);
  return seg;
}

static inline void seg_free(struct segment *seg) {
  spin_lock_destroy(seg->lock);
  MFREE(seg);
}

static inline unsigned int seg_len(struct segment *seg) {
  return seg->seg_end - seg->seg_begin - 1;
}

struct rffs_log {
  struct segment *log_begin;
  struct segment *log_end;
};

static inline struct rffs_log *log_new() {
  struct segment *seg = seg_new();
  struct rffs_log *log = (struct rffs_log *)MALLOC(sizeof(struct rffs_log));

  seg->seq_num = 0;
  seg->prev = seg->next = seg;

  log->log_begin = log->log_end = seg;
  return log;
}

static inline void log_free(struct rffs_log *log) {
  struct segment *seg = log->log_begin;
  while (seg->next != log->log_begin) {
    seg = seg->next;
    seg_free(seg->prev);
  }
  seg_free(seg);

  MFREE(log);
}

static inline void log_add_seg(struct rffs_log *log) {
  struct segment *new_seg = seg_new();
  struct segment *prev_seg = log->log_begin->prev;
  struct segment *next_seg = prev_seg->next;
  
  spin_lock(&prev_seg->lock);
  if (prev_seg != next_seg) {
    spin_lock(&next_seg->lock);
  }
  
  prev_seg->next = new_seg;
  new_seg->prev = prev_seg;
  next_seg->prev = new_seg;
  new_seg->next = next_seg;
  
  spin_unlock(&prev_seg->lock);
  if (prev_seg != next_seg) {
    spin_unlock(&next_seg->lock);
  }
}

static inline void log_append(struct rffs_log *log, struct seg_entry *entry) {
}

struct log_pos { // only for internal use
  struct segment *seg;
  unsigned int entry;
};

void log_seal(struct rffs_log *log,
              struct log_pos *trans_begin, // output
              struct log_pos *trans_end); // output

// Returns sorted array of pointers to seg_entry
void *log_sort(struct log_pos *begin, struct log_pos *end,
               int *length); // output

#endif
