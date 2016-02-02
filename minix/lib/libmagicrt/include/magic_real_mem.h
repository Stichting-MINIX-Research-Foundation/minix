#ifndef _MAGIC_REAL_MEM_H
#define _MAGIC_REAL_MEM_H

#include <magic_mem.h>

#define malloc              magic_real_malloc
#define calloc              magic_real_calloc
#define free                magic_real_free
#define realloc             magic_real_realloc

#define posix_memalign      magic_real_posix_memalign
#define valloc              magic_real_valloc
#define memalign            magic_real_memalign

#define mmap                magic_real_mmap
#define munmap              magic_real_munmap

#define brk                 magic_real_brk
#define sbrk                magic_real_sbrk

#define shmat               magic_real_shmat
#define shmdt               magic_real_shmdt

#define mmap64              magic_real_mmap64
#define vm_map_cacheblock   magic_real_vm_map_cacheblock

#endif

