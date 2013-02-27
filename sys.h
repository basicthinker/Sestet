//
//  sys.h
//  sestet-rffs
//
//  Created by Jinglei Ren on 2/25/13.
//  Copyright (c) 2013 Microsoft Research Asia. All rights reserved.
//

#ifndef SESTET_RFFS_SYS_H_
#define SESTET_RFFS_SYS_H_

#ifdef __linux__ // Linux kernel space
  #define MALLOC(size) // TODO
  #define MFREE(size) // TODO
#else
  #include <stdlib.h>
  #define MALLOC(size) malloc(size)
  #define MFREE(p) free(p)
#endif

#ifdef _MSC_VER // for Microsoft compiler
  #define inline __inline
#endif

#ifdef __linux__
  #define spin_lock_destroy(lock)
#else
  #include <pthread.h>
#if defined(__APPLE__) && defined(__MACH__)
  #include <libkern/OSAtomic.h>
  typedef OSSpinLock spinlock_t;
  #define spin_lock_init(lock) (*lock = OS_SPINLOCK_INIT)
  #define spin_lock(lock) OSSpinLockLock(lock)
  #define spin_unlock(lock) OSSpinLockUnlock(lock)
  #define spin_lock_destroy(lock)
#else
  typedef pthread_spinlock_t spinlock_t;
  #define spin_lock_init(lock) pthread_spin_init(lock, PTHREAD_PROCESS_PRIVATE)
  #define spin_lock(lock) pthread_spin_lock(lock)
  #define spin_unlock(lock) pthread_spin_unlock(lock)
  #define spin_lock_destroy(lock) pthread_spin_destroy(lock)
#endif // Mac OS
#endif // Linux kernel

#ifdef __linux__
  #define ERR_PRINT(str) // TODO
#else
  #define ERR_PRINT(str) // TODO
#endif

#endif