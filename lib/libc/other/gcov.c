#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include <minix/gcov.h>

int gcov_flush_svr(char *buff, int buff_sz, int server_nr)
{
	message msg;

	msg.GCOV_BUFF_P = buff;
	msg.GCOV_BUFF_SZ = buff_sz;
	msg.GCOV_PID = server_nr;

	/* Make the call to server. It will call the gcov library,
	 * buffer the stdio requests, and copy the buffer to this user
	 * space
	 */
  	return _syscall(VFS_PROC_NR, GCOV_FLUSH, &msg);
}


/* wrappers for file system calls from gcc libgcov library.
   Default calls are wrapped. In libsys, an alternative
   implementation for servers is used.
*/

FILE *_gcov_fopen(char *name, char *mode){
	return fopen(name, mode);
}


size_t _gcov_fread(void *ptr, size_t itemsize, size_t nitems
        , FILE *stream){
        return fread(ptr, itemsize, nitems, stream);
}

size_t _gcov_fwrite(void *ptr, size_t itemsize, size_t nitems
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

