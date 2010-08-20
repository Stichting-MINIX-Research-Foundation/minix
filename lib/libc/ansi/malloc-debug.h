#include <minix/u64.h>
#include <sys/types.h>

/* malloc-debug.c */
void *_dbg_malloc(size_t size);
void *_dbg_realloc(void *oldp, size_t size);
void _dbg_free(void *ptr);

