#ifndef _MINIX_GCOV_H
#define _MINIX_GCOV_H

#include <sys/types.h>
#include <lib.h>
#include <stdlib.h>
#include <minix/syslib.h>

/* opcodes for use in gcov buffer */
#define GCOVOP_OPEN	23
#define GCOVOP_WRITE	24
#define GCOVOP_CLOSE	25
#define GCOVOP_END	26

/* More information on the GCOV Minix Wiki page. */

int gcov_flush_svr(const char * label, char * buff, size_t buff_sz);

#if _MINIX_SYSTEM
extern void __gcov_flush(void);
int do_gcov_flush_impl(message *msg);

FILE *_gcov_fopen(const char *name, const char *mode);
size_t _gcov_fread(void *ptr, size_t itemsize, size_t nitems,
	FILE *stream);
size_t _gcov_fwrite(const void *ptr, size_t itemsize, size_t nitems,
	FILE *stream);
int _gcov_fclose(FILE *stream);
int _gcov_fseek(FILE *stream, long offset, int ptrname);
char *_gcov_getenv(const char *name);
#endif /* _MINIX_SYSTEM */

#endif /* !_MINIX_GCOV_H */
