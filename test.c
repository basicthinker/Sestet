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

//#define ALLOC_DATA

int main(int argc, const char *argv[]) {
    struct rffs_log log;
    struct log_entry entry;
    int i;
    log_init(&log);
    for (i = 0; i < LOG_LEN; ++i) {
    	entry.block_begin = rand() & 1023;
    	entry.inode_id = rand() & 1023;
    	log_append(&log, &entry);
    }
    log_flush(&log, LOG_LEN);
    return 0;
}
