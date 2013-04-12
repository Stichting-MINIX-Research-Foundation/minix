/* Test 71 - full hierachy storage test.
 *
 * Black box test of storage: test consistent file contents
 * under various working sets and access patterns.
 *
 * Using varying working set sizes, exercise various cache
 * layers separately.
 *
 * There is a 'smoke test' mode, suitable for running interactively,
 * and a 'regression test' (big) mode, meant for batch invocation only
 * as it takes very long.
 */

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

	return blocksize;
}

void testend(void) { }

int
main(int argc, char *argv[])
{
	int f, big = !!getenv(BIGVARNAME), iter = 2;

	start(71);

	cachequiet(!big);
	if(big) iter = 3;

	for(f = 0; f < MAXFILES; f++) {
        	char tempfilename[] = "cachetest.XXXXXXXX";
	        fds[f] = mkstemp(tempfilename);
		if(fds[f] < 0) { perror("mkstemp"); e(20); return 1; }
		assert(fds[f] > 0);
	}

	/* Try various combinations working set sizes
	 * and block sizes in order to specifically 
	 * target the primary cache, then primary+secondary
	 * cache, then primary+secondary cache+secondary
	 * cache eviction.
	 */

	if(dotest(PAGE_SIZE,    100, iter)) e(5);
	if(dotest(PAGE_SIZE*2,  100, iter)) e(2);
	if(dotest(PAGE_SIZE*3,  100, iter)) e(3);
	if(dotest(PAGE_SIZE,  20000, iter)) e(5);

	if(big) {
		u32_t totalmem, freemem, cachedmem;
		if(dotest(PAGE_SIZE,  150000, iter)) e(5);
		getmem(&totalmem, &freemem, &cachedmem);
		if(dotest(PAGE_SIZE,  totalmem*1.5, iter)) e(6);
	}

	for(f = 0; f < MAXFILES; f++) {
		assert(fds[f] > 0);
		close(fds[f]);
	}

	quit();

	return 0;
}

