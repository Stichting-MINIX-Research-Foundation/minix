
#include <sys/types.h>
#include <sys/ioc_memory.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "testcache.h"

/* we want to flexibly split this test over multiple files
 * - for big working sets we might run over the 2GB MFS file limit
 * - we might want to test the FS being able to handle lots of
 *   files / unusual metadata situations
 */
#define MBPERFILE 100
#define MB (1024*1024)
#define MAXFILES ((u64_t) MAXBLOCKS * MAXBLOCKSIZE / MB / MBPERFILE + 1)

static int fds[MAXFILES];

static void
get_fd_offset(int b, int blocksize, u64_t *file_offset, int *fd)
{
	u64_t offset = (u64_t) b * blocksize;
	int filenumber;

	filenumber = offset / MB / MBPERFILE;

	assert(filenumber >= 0 && filenumber < MAXFILES);
	assert(fds[filenumber] > 0);

	*fd = fds[filenumber];
	*file_offset = offset - (filenumber * MBPERFILE * MB);
}

int
dowriteblock(int b, int blocksize, u32_t seed, char *data)
{
	u64_t offset;
	int fd;

	get_fd_offset(b, blocksize, &offset, &fd);

	if(pwrite(fd, data, blocksize, offset) < blocksize) {
		perror("pwrite");
		return -1;
	}

	return blocksize;
}

int
readblock(int b, int blocksize, u32_t seed, char *data)
{
	u64_t offset;
	int fd;

	get_fd_offset(b, blocksize, &offset, &fd);

	if(pread(fd, data, blocksize, offset) < blocksize) {
		perror("pread");
		return -1;
	}

	return 0;
}

void testend(void) { }

int
main(int argc, char *argv[])
{
	int f, big = !!getenv("BIGTEST");

	start(71);

	cachequiet(!big);

	for(f = 0; f < MAXFILES; f++) {
        	char tempfilename[] = "cachetest.XXXXXXXX";
	        fds[f] = mkstemp(tempfilename);
		if(fds[f] < 0) { perror("mkstemp"); e(20); return 1; }
/*		if(unlink(tempfilename) < 0) { perror("unlink"); e(21); return 1; } */
		assert(fds[f] > 0);
	}

#define ITER 3

	if(dotest(PAGE_SIZE, 1500000, ITER)) e(7);

	if(dotest(PAGE_SIZE,    500, ITER)) e(1);
	if(dotest(PAGE_SIZE*2, 1000, ITER)) e(2);
	if(dotest(PAGE_SIZE*3, 1000, ITER)) e(3);
	if(dotest(PAGE_SIZE,   1500, ITER)) e(4);
	if(dotest(PAGE_SIZE,  15000, ITER)) e(5);
	if(big) {
		if(dotest(PAGE_SIZE,  50000, ITER)) e(6);
		if(dotest(PAGE_SIZE, 500000, ITER)) e(7);
	}

	for(f = 0; f < MAXFILES; f++) {
		assert(fds[f] > 0);
		close(fds[f]);
	}

	quit();

	return 0;
}

