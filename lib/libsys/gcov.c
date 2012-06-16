/* This code can be linked into minix servers that are compiled 
 * with gcc gcov flags.
 * Author: Anton Kuijsten
 */

#include <lib.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <minix/syslib.h>
#include <minix/gcov.h>

static int grant, pos;           /* data-buffer pointer from user space tool */
static int gcov_enable=0;     /* nothing will be done with gcov-data if zero */
static int gcov_buff_sz;                        /* size of user space buffer */
static FILE gcov_file;                      /* used as fopen() return value. */
static int gcov_opened;

/* copies <size> bytes from <ptr> to <gcov_buff> */
static void add_buff(void *ptr, int size)
{
	int r;
	assert(pos <= gcov_buff_sz);

	if(pos+size > gcov_buff_sz) {
		size = pos - gcov_buff_sz;
	}

	r = sys_safecopyto(VFS_PROC_NR, grant, pos, (vir_bytes)ptr, size);

	if(r) {
		printf("libsys: gcov: safecopy failed (%d)\n", r);
  	}

	pos += size;

	assert(pos <= gcov_buff_sz);
}

/* easy wrapper for add_buff */
static void add_int(int value)
{
	add_buff((void *) &value, sizeof(int));
}

/* These functions are meant to replace standard file 
 * system calls (fopen, etc)
 */

FILE *_gcov_fopen(char *name, char *mode)
{
	if(!gcov_enable) return NULL;

	assert(!gcov_opened);

	/* write information to buffer */
	add_int(GCOVOP_OPEN);
	add_int(strlen(name)+1);
        add_buff(name, strlen(name)+1);

	gcov_opened = 1;

	/* return dummy FILE *. */
        return &gcov_file;
}


size_t _gcov_fread(void *ptr, size_t itemsize, size_t nitems, FILE *stream)
{
        return 0;
}

size_t _gcov_fwrite(void *ptr, size_t itemsize, size_t nitems, FILE *stream)
{
	int size = itemsize * nitems;

	if(!gcov_enable) return -1;

	/* only have one file open at a time to ensure writes go
	 * to the right place.
	 */
	assert(gcov_opened);
	assert(stream == &gcov_file);

	/* write information to buffer */
	add_int(GCOVOP_WRITE);
	add_int(size);
	add_buff(ptr, size);

        return nitems;
}

int _gcov_fclose(FILE *stream)
{
	if(!gcov_enable) return EOF;

	add_int(GCOVOP_CLOSE);
	assert(gcov_opened);
	gcov_opened = 0;
        return 0;
}

int _gcov_fseek(FILE *stream, long offset, int ptrname)
{
        return 0;
}

char *_gcov_getenv(const char *name)
{
        return NULL;
}

int gcov_flush(cp_grant_id_t grantid, int bufsize)
{
	/* Initialize global state. */
	pos=0;
	grant = grantid;
	gcov_buff_sz = bufsize;
	assert(!gcov_enable);
	assert(!gcov_opened);
	gcov_enable = 1;

	/* Trigger copying.
	 * This function is not always available, but there is a do-nothing
	 * version in libc so that executables can be linked even without
	 * this code ever being activated.
	 */
	__gcov_flush();

	/* Mark the end of the data, stop. */
	add_int(GCOVOP_END);	 
	assert(!gcov_opened);
	assert(gcov_enable);
	gcov_enable = 0;

	/* Return number of bytes used in buffer. */
	return pos;
}

/* This function can be called to perform the copying.
 * It sends its own reply message and can thus be
 * registered as a SEF * callback.
 */
int do_gcov_flush_impl(message *msg)
{
	message replymsg;
	memset(&replymsg, 0, sizeof(replymsg));

	assert(msg->m_type == COMMON_REQ_GCOV_DATA);
	assert(msg->m_source == VFS_PROC_NR);

	replymsg.m_type = gcov_flush(msg->GCOV_GRANT, msg->GCOV_BUFF_SZ);
	return send(msg->m_source, &replymsg);
}

