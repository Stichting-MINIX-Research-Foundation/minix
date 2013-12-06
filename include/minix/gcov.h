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

int gcov_flush_svr(char *buff, int buff_sz, int server_nr);
extern void __gcov_flush (void);
int do_gcov_flush_impl(message *msg);

FILE *_gcov_fopen(char *name, char *mode);
size_t _gcov_fread(void *ptr, size_t itemsize, size_t nitems,
	FILE *stream);
size_t _gcov_fwrite(void *ptr, size_t itemsize, size_t nitems,
	FILE *stream);
int _gcov_fclose(FILE *stream);
int _gcov_fseek(FILE *stream, long offset, int ptrname);
char *_gcov_getenv(const char *name);
