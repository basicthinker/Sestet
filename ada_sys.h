//
//  sys.h
//  sestet-adafs
//
//  Created by Jinglei Ren on 2/25/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#ifndef ADAFS_SYS_H_
#define ADAFS_SYS_H_

#define UINT_MAX (~0U)

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
    #define INFO KERN_INFO
    #define WARNING KERN_WARNING
    #define ERR KERN_ERR
#else
    #include <stdio.h>
    #define PRINT(...) printf(__VA_ARGS__)
    #define INFO "INFO - "
    #define WARNING "WARN - "
    #define ERR "ERR - "
#endif

#ifdef __KERNEL__
	#include <linux/list.h>
#else
	#include "ulist.h"
#endif

#ifndef __KERNEL__
    #include "uatomic.h"
    #define likely(cond) (cond)
    #define unlikely(cond) (cond)
    #define ULONG_MAX (~0UL)
    #define PAGE_CACHE_SHIFT 12
    #define PAGE_CACHE_SIZE (1 << PAGE_CACHE_SHIFT)
    #define PAGE_CACHE_MASK (~(PAGE_CACHE_SIZE - 1))
#endif

#if defined(__KERNEL__) && defined(ADAFS_DEBUG)
    #define ADAFS_TRACE(...)	printk(__VA_ARGS__)
    #define ADAFS_BUG_ON(...)	BUG_ON(__VA_ARGS__)
#else
    #define ADAFS_TRACE(...)
    #define ADAFS_BUG_ON(...)
#endif

#endif // ADAFS_SYS_H_
