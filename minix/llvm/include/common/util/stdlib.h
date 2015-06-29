#ifndef _UTIL_STDLIB_H
#define _UTIL_STDLIB_H

#include "util_def.h"

#include <string.h>

typedef struct util_stdlib_s {
    int id;
    char *name;
    unsigned long flags;
} util_stdlib_t;

typedef enum util_stdlib_id_e {
    STDLIB_ACCEPT = 0,
    STDLIB_ACCEPT4,
    STDLIB_BIND,
    STDLIB_BRK,
    STDLIB_CALLOC,
    STDLIB_EPOLL_CREATE,
    STDLIB_EPOLL_CREATE1,
    STDLIB_EPOLL_WAIT,
    STDLIB_FREE,
    STDLIB_GETSOCKOPT,
    STDLIB_KILL,
    STDLIB_LISTEN,
    STDLIB_MALLOC,
    STDLIB_MEMALIGN,
    STDLIB_MMAP,
    STDLIB_MMAP64,
    STDLIB_MUNMAP,
    STDLIB_POLL,
    STDLIB_POSIX_MEMALIGN,
    STDLIB_PPOLL,
    STDLIB_PTHREAD_COND_WAIT,
    STDLIB_PTHREAD_COND_TIMEDWAIT,
    STDLIB_PTHREAD_JOIN,
    STDLIB_READ,
    STDLIB_REALLOC,
    STDLIB_RECV,
    STDLIB_RECVFROM,
    STDLIB_RECVMSG,
    STDLIB_SBRK,
    STDLIB_SELECT,
    STDLIB_SEMOP,
    STDLIB_SEMTIMEDOP,
    STDLIB_SETSOCKOPT,
    STDLIB_SHMAT,
    STDLIB_SHMDT,
    STDLIB_SIGSUSPEND,
    STDLIB_SIGTIMEDWAIT,
    STDLIB_SIGWAITINFO,
    STDLIB_SLEEP,
    STDLIB_SOCKET,
    STDLIB_USLEEP,
    STDLIB_VALLOC,
    STDLIB_WAITPID,
    __NUM_STDLIBS_IDS
} util_stdlib_id_t;

typedef enum util_stdlib_flag_e {
    STLIB_BLOCK_EXT = 0,
    STLIB_BLOCK_INT,
    __NUM_STDLIBS_FLAGS
} util_stdlib_flag_t;
#define _UTIL_STLIB_FLAGS_STR    "ei"

#define _UTIL_STLIB_FLAG(F) (1 << (F))
#define _UTIL_STLIB_BLOCK_MASK \
    (_UTIL_STLIB_FLAG(STLIB_BLOCK_EXT)|_UTIL_STLIB_FLAG(STLIB_BLOCK_INT))

#define _UTIL_STLIB_FLAGS_STR_BUFF_SIZE (__NUM_STDLIBS_FLAGS+1)
#define _UTIL_STLIB_FLAG_C(F, E) \
    (((F) & _UTIL_STLIB_FLAG(E)) ? _UTIL_STLIB_FLAGS_STR[E] : '-')

#define _UTIL_STDLIB_DEF(ID, N, F) { (ID), (N), (F) }
#define _UTIL_STDLIBS_INITIALIZER { \
    _UTIL_STDLIB_DEF(STDLIB_ACCEPT,                 "accept",                 _UTIL_STLIB_FLAG(STLIB_BLOCK_EXT)), \
    _UTIL_STDLIB_DEF(STDLIB_ACCEPT4,                "accept4",                _UTIL_STLIB_FLAG(STLIB_BLOCK_EXT)), \
    _UTIL_STDLIB_DEF(STDLIB_BIND,                   "bind",                   0), \
    _UTIL_STDLIB_DEF(STDLIB_BRK,                    "brk",                    0), \
    _UTIL_STDLIB_DEF(STDLIB_CALLOC,                 "calloc",                 0), \
    _UTIL_STDLIB_DEF(STDLIB_EPOLL_CREATE,           "epoll_create",           0), \
    _UTIL_STDLIB_DEF(STDLIB_EPOLL_CREATE1,          "epoll_create1",          0), \
    _UTIL_STDLIB_DEF(STDLIB_EPOLL_WAIT,             "epoll_wait",             _UTIL_STLIB_FLAG(STLIB_BLOCK_EXT)), \
    _UTIL_STDLIB_DEF(STDLIB_FREE,                   "free",                   0), \
    _UTIL_STDLIB_DEF(STDLIB_GETSOCKOPT,             "getsockopt",             0), \
    _UTIL_STDLIB_DEF(STDLIB_KILL,                   "kill",                   0), \
    _UTIL_STDLIB_DEF(STDLIB_LISTEN,                 "listen",                 0), \
    _UTIL_STDLIB_DEF(STDLIB_MALLOC,                 "malloc",                 0), \
    _UTIL_STDLIB_DEF(STDLIB_MEMALIGN,               "memalign",               0), \
    _UTIL_STDLIB_DEF(STDLIB_MMAP,                   "mmap",                   0), \
    _UTIL_STDLIB_DEF(STDLIB_MMAP64,                 "mmap64",                 0), \
    _UTIL_STDLIB_DEF(STDLIB_MUNMAP,                 "munmap",                 0), \
    _UTIL_STDLIB_DEF(STDLIB_POLL,                   "poll",                   _UTIL_STLIB_FLAG(STLIB_BLOCK_EXT)), \
    _UTIL_STDLIB_DEF(STDLIB_POSIX_MEMALIGN,         "posix_memalign",         0), \
    _UTIL_STDLIB_DEF(STDLIB_PPOLL,                  "ppoll",                  _UTIL_STLIB_FLAG(STLIB_BLOCK_EXT)), \
    _UTIL_STDLIB_DEF(STDLIB_PTHREAD_COND_WAIT,      "pthread_cond_wait",      _UTIL_STLIB_FLAG(STLIB_BLOCK_INT)), \
    _UTIL_STDLIB_DEF(STDLIB_PTHREAD_COND_TIMEDWAIT, "pthread_cond_timedwait", _UTIL_STLIB_FLAG(STLIB_BLOCK_INT)), \
    _UTIL_STDLIB_DEF(STDLIB_PTHREAD_JOIN,           "pthread_join",           _UTIL_STLIB_FLAG(STLIB_BLOCK_INT)), \
    _UTIL_STDLIB_DEF(STDLIB_READ,                   "read",                   _UTIL_STLIB_FLAG(STLIB_BLOCK_EXT)), \
    _UTIL_STDLIB_DEF(STDLIB_REALLOC,                "realloc",                0), \
    _UTIL_STDLIB_DEF(STDLIB_RECV,                   "recv",                   _UTIL_STLIB_FLAG(STLIB_BLOCK_EXT)), \
    _UTIL_STDLIB_DEF(STDLIB_RECVFROM,               "recvfrom",               _UTIL_STLIB_FLAG(STLIB_BLOCK_EXT)), \
    _UTIL_STDLIB_DEF(STDLIB_RECVMSG,                "recvsmg",                _UTIL_STLIB_FLAG(STLIB_BLOCK_EXT)), \
    _UTIL_STDLIB_DEF(STDLIB_SBRK,                   "sbrk",                   0), \
    _UTIL_STDLIB_DEF(STDLIB_SELECT,                 "select",                 _UTIL_STLIB_FLAG(STLIB_BLOCK_EXT)), \
    _UTIL_STDLIB_DEF(STDLIB_SEMOP,                  "semop",                  _UTIL_STLIB_FLAG(STLIB_BLOCK_INT)), \
    _UTIL_STDLIB_DEF(STDLIB_SEMTIMEDOP,             "semtimedop",             _UTIL_STLIB_FLAG(STLIB_BLOCK_INT)), \
    _UTIL_STDLIB_DEF(STDLIB_SETSOCKOPT,             "setsockopt",             0), \
    _UTIL_STDLIB_DEF(STDLIB_SHMAT,                  "shmat",                  0), \
    _UTIL_STDLIB_DEF(STDLIB_SHMDT,                  "shmdt",                  0), \
    _UTIL_STDLIB_DEF(STDLIB_SIGSUSPEND,             "sigsuspend",             _UTIL_STLIB_FLAG(STLIB_BLOCK_INT)), \
    _UTIL_STDLIB_DEF(STDLIB_SIGTIMEDWAIT,           "sigtimedwait",           _UTIL_STLIB_FLAG(STLIB_BLOCK_INT)), \
    _UTIL_STDLIB_DEF(STDLIB_SIGWAITINFO,            "sigwaitinfo",            _UTIL_STLIB_FLAG(STLIB_BLOCK_INT)), \
    _UTIL_STDLIB_DEF(STDLIB_SLEEP,                  "sleep",                  _UTIL_STLIB_FLAG(STLIB_BLOCK_EXT)), \
    _UTIL_STDLIB_DEF(STDLIB_SOCKET,                 "socket",                 0), \
    _UTIL_STDLIB_DEF(STDLIB_USLEEP,                 "usleep",                 _UTIL_STLIB_FLAG(STLIB_BLOCK_EXT)), \
    _UTIL_STDLIB_DEF(STDLIB_VALLOC,                 "valloc",                 0), \
    _UTIL_STDLIB_DEF(STDLIB_WAITPID,                "waitpid",                _UTIL_STLIB_FLAG(STLIB_BLOCK_INT)) \
}

static inline util_stdlib_t* util_stdlib_lookup_by_name(const char *name, util_stdlib_t *stlib_arr)
{
    int i;
    for (i=0;i<__NUM_STDLIBS_IDS;i++) {
        if (!strcmp(name, stlib_arr[i].name)) {
            return &stlib_arr[i];
        }
    }

    return NULL;
}

static inline char* util_stdlib_flags_to_str(unsigned long flags,
    char* buffer)
{
    int i;
    for(i=0;i<__NUM_STDLIBS_FLAGS;i++) {
        buffer[i] = _UTIL_STLIB_FLAG_C(flags, i);
    }
    buffer[i] = '\0';

    return buffer;
}

#endif /* _UTIL_STDLIB_H */

