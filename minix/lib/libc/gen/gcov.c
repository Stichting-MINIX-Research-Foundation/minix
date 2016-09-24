#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#define _MINIX_SYSTEM 1
#include <minix/gcov.h>

/* wrappers for file system calls from gcc libgcov library.
   Default calls are wrapped. In libsys, an alternative
   implementation for servers is used.
*/

FILE *_gcov_fopen(const char *name, const char *mode){
	return fopen(name, mode);
}


size_t _gcov_fread(void *ptr, size_t itemsize, size_t nitems
        , FILE *stream){
        return fread(ptr, itemsize, nitems, stream);
}

size_t _gcov_fwrite(const void *ptr, size_t itemsize, size_t nitems
        , FILE *stream){
	return fwrite(ptr, itemsize, nitems, stream);
}

int _gcov_fclose(FILE *stream){
	return fclose(stream);
}

int _gcov_fseek(FILE *stream, long offset, int ptrname){
        return fseek(stream, offset, ptrname);
}

char *_gcov_getenv(const char *name){
        return getenv(name);
}

