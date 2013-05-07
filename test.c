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

int main(int argc, const char *argv[]) {
    struct rffs_log log;
    struct log_entry entry;
    int i, j, err, nr_trans, trans_len;
    log_init(&log);
    // Two-round appending
    for (i = 0, err = 0; i < (LOG_LEN * 2) && !err; ++i) {
    	entry.inode_id = rand() & 255;
    	entry.block_begin = rand() & 255;
    	err = log_append(&log, &entry);
    }
    log_seal(&log);
    log_flush(&log, LOG_LEN);

    // Random appending, sealing and flushing
    nr_trans = 10;
    trans_len = LOG_LEN >> 1;
    for (i = 0; i < nr_trans; ++i) {
    	int len = rand() % trans_len;
    	PRINT("(-2)\t%d\n", len);
    	for (j = 0, err = 0; j < len && !err; ++j) {
    		entry.inode_id = rand() & 1023;
    		entry.block_begin = rand() & 1023;
    		err = log_append(&log, &entry);
    	}
    	log_seal(&log);
    	if ((i & 1) == 1) log_flush(&log, 2);
    }
    log_seal(&log);
    log_flush(&log, LOG_LEN);
    return 0;
}
