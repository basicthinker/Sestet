//
//  sys.h
//  sestet-rffs
//
//  Created by Jinglei Ren on 2/25/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#ifndef SESTET_RFFS_SYS_H_
#define SESTET_RFFS_SYS_H_

#define UINT_MAX (~0U)

#ifdef __KERNEL__ // Linux kernel space
    #include <linux/slab.h>
    #define MALLOC(size) kmalloc(size, GFP_KERNEL)
    #define MFREE(p) kfree(p)
#else
    #include <stdlib.h>
    #define MALLOC(size) malloc(size)
    #define MFREE(p) free(p)
#endif

#ifdef __KERNEL__
    #include <linux/spinlock.h>
    #define spin_lock_destroy(lock)
#else
    #include <pthread.h>
    typedef pthread_spinlock_t spinlock_t;
    #define spin_lock_init(lock) pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE)
    #define spin_lock(lock) pthread_spin_lock(lock)
    #define spin_unlock(lock) pthread_spin_unlock(lock)
    #define spin_lock_destroy(lock) pthread_spin_destroy(lock)
#endif // Linux kernel

#ifdef __KERNEL__
    #define PRINT(...) printk(__VA_ARGS__)
#else
    #include <stdio.h>
    #define PRINT(...) printf(__VA_ARGS__)
#endif

#ifdef __KERNEL__
	#include <linux/list.h>
#else
	#include "list.h"
#endif

#ifndef __KERNEL__
	#include "atomic.h"
    #define likely(cond) (cond)
    #define unlikely(cond) (cond)
    #define ULONG_MAX (~0UL)
    #define PAGE_CACHE_SIZE 4096
    #define PAGE_CACHE_MASK (~(PAGE_CACHE_SIZE - 1))
#endif

#endif // SESTET_RFFS_SYS_H_
