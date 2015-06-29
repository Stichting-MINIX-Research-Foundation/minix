
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#ifndef __USE_GNU
#define __USE_GNU 1
#endif

#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE	4096
#endif

#ifndef MIN_MMAP_ADDR
#define MIN_MMAP_ADDR   ((void*)(PAGE_SIZE*100))
#endif

#ifndef _UTIL_PRINTF
#define _UTIL_PRINTF              printf
#endif

#ifndef _UTIL_PTHREAD_CREATE
#define _UTIL_PTHREAD_CREATE      pthread_create
#endif

#ifndef _UTIL_PTHREAD_JOIN
#define _UTIL_PTHREAD_JOIN        pthread_join
#endif

#ifndef _UTIL_PTHREAD_CANCEL
#define _UTIL_PTHREAD_CANCEL      pthread_cancel
#endif

#ifndef _UTIL_PTHREAD_SIGMASK
#define _UTIL_PTHREAD_SIGMASK     pthread_sigmask
#endif

#ifndef _UTIL_MALLOC
#define _UTIL_MALLOC              malloc
#endif

#ifndef _UTIL_CALLOC
#define _UTIL_CALLOC              calloc
#endif

#ifndef _UTIL_FREE
#define _UTIL_FREE                free
#endif

