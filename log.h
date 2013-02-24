//
//  log.h
//  sestet-rffs
//
//  Created by Jinglei Ren on 2/24/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#ifndef SESTET_RFFS_LOG_H_
#define SESTET_RFFS_LOG_H_

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
};

struct log_pos { // only for internal use
  struct segment *seg;
  unsigned int entry;
};

struct log {
  struct log_pos log_begin;
  struct log_pos log_end;
  unsigned int length;
};

struct log *log_new();

void log_del(struct log *log);

void log_append(struct log *log, struct seg_entry *entry);

void log_seal(struct log *log,
              struct log_pos *trans_begin, // output
              struct log_pos *trans_end); // output

// Returns sorted array of pointers to seg_entry
void *log_sort(struct log_pos *begin, struct log_pos *end,
               int *length); // output

#endif
