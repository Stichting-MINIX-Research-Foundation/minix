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
	int iter = 2;

	start(71);

	cachequiet(!bigflag);
	if(bigflag) iter = 3;

	makefiles(MAXFILES);

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

	if(bigflag) {
		u32_t totalmem, freemem, cachedmem;
		if(dotest(PAGE_SIZE,  150000, iter)) e(5);
		getmem(&totalmem, &freemem, &cachedmem);
		if(dotest(PAGE_SIZE,  totalmem*1.5, iter)) e(6);
	}

	quit();

	return 0;
}

