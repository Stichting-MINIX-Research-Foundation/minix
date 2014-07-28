/* A general i/o consistency test library. It performs i/o
 * using functions provided by the client (readblock, dowriteblock)
 * with a working set size specified by the client. It checks that
 * blocks that were written have the same contents when later read,
 * using different access patterns. The assumption is the various
 * cache layers so exercised are forced into many different states
 * (reordering, eviction, etc), hopefully triggering bugs if present.
 *
 * Entry point: dotest()
 */

#include <sys/types.h>
#include <sys/ioc_memory.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "testcache.h"
#include "common.h"

extern int quietflag;

int fds[MAXFILES];

static void
genblock(int b, char *blockdata, int blocksize, u32_t seed)
{
	u32_t *p = (u32_t *) blockdata,
		*plimit = (u32_t *) (blockdata + blocksize),
		i = 0;

	srandom(seed ^ b);

	for(p = (u32_t *) blockdata; p < plimit; p++) {
		i++;
		*p = random();
	}
}

static int
checkblock(int b, int blocksize, u32_t seed)
{
	static char data[MAXBLOCKSIZE], expected_data[MAXBLOCKSIZE];
	int r;

	genblock(b, expected_data, blocksize, seed);

	r = readblock(b, blocksize, seed, data);

	if(r == OK_BLOCK_GONE) { return 0; }

	if(r != blocksize) {
		fprintf(stderr, "readblock failed\n");
		return 1;
	}

	if(memcmp(expected_data, data, blocksize)) {
		fprintf(stderr, "comparison of %d failed\n", b);
		return 1;
	}

	return 0;
}

static int
writeblock(int b, int blocksize, u32_t seed)
{
	static char data[MAXBLOCKSIZE];

	genblock(b, data, blocksize, seed);

	if(dowriteblock(b, blocksize, seed, data) != blocksize) {
		fprintf(stderr, "writeblock of %d failed\n", b);
		return 0;
	}

	return blocksize;
}

static int *
makepermutation(int nblocks, int *permutation)
{
	int b;

	assert(nblocks > 0 && nblocks <= MAXBLOCKS);

	for(b = 0; b < nblocks; b++) permutation[b] = b;

	for(b = 0; b < nblocks-1; b++) {
		int s, other = b + random() % (nblocks - b - 1);
		assert(other >= b && other < nblocks);
		s = permutation[other];
		permutation[other] = permutation[b];
		permutation[b] = s;
	}

	return permutation;
}

static int
checkblocks(int nblocks, int blocksize, u32_t seed)
{
	int b;
	int nrandom = nblocks * 3;
	static int perm1[MAXBLOCKS];

	if(!quietflag) { fprintf(stderr, "\nverifying "); fflush(stderr); }

	makepermutation(nblocks, perm1);

	assert(nblocks > 0 && nblocks <= MAXBLOCKS);
	assert(blocksize > 0 && blocksize <= MAXBLOCKSIZE);

	for(b = 0; b < nblocks; b++) {
		if(checkblock(b, blocksize, seed)) { return 1; }
	}

	for(b = 0; b < nrandom; b++) {
		if(checkblock(random() % nblocks, blocksize, seed)) { return 1; }
	}

	for(b = 0; b < nblocks; b++) {
		if(checkblock(b, blocksize, seed)) { return 1; }
	}

	for(b = 0; b < nblocks; b++) {
		if(checkblock(perm1[b], blocksize, seed)) { return 1; }
	}

	if(!quietflag) { fprintf(stderr, "done\n"); }

	return 0;
}

int
dotest(int blocksize, int nblocks, int iterations)
{
	int b, i;
	int nrandom = nblocks * iterations;
	static int perm1[MAXBLOCKS], perm2[MAXBLOCKS];
	static int newblock[MAXBLOCKS];
	u32_t seed = random(), newseed;
	int mb;

	assert(nblocks > 0 && nblocks <= MAXBLOCKS);

	mb = (int) ((u64_t) blocksize * nblocks / 1024 / 1024);

	if(!quietflag) { fprintf(stderr, "test: %d * %d = %dMB\n", blocksize, nblocks, mb); }

	for(b = 0; b < nblocks; b++) {
		if(writeblock(b, blocksize, seed) < blocksize) { return 1; }
		if(checkblock(b, blocksize, seed)) { return 1; }
		printprogress("writing sequential", b, nblocks);
	}

	if(checkblocks(nblocks, blocksize, seed)) { return 1; }

	makepermutation(nblocks, perm1);

	for(b = 0; b < nblocks; b++) {
		if(writeblock(perm1[b], blocksize, seed) < blocksize) { return 1; }
		if(checkblock(perm1[b], blocksize, seed)) { return 1; }
		printprogress("writing permutation", b, nblocks);
	}

	if(checkblocks(nblocks, blocksize, seed)) { return 1; }

	for(i = 0; i < iterations; i++) {
		makepermutation(nblocks, perm1);
		makepermutation(nblocks, perm2);
		memset(newblock, 0, sizeof(newblock));

		newseed = random();

		if(!quietflag) { fprintf(stderr, "iteration %d/%d\n", i, iterations); }

		for(b = 0; b < nblocks; b++) {
			int wr = perm1[b], check = perm2[b];
			if(writeblock(wr, blocksize, newseed) < blocksize) { return 1; }
			newblock[wr] = 1;
			if(checkblock(check, blocksize, newblock[check] ? newseed : seed)) { return 1; }
			printprogress("interleaved permutation read, write", b, nblocks);
		}

		seed = newseed;

		if(checkblocks(nblocks, blocksize, seed)) { return 1; }
	}

	newseed = random();

	memset(newblock, 0, sizeof(newblock));

	for(b = 0; b < nrandom; b++) {
		int wr = random() % nblocks, check = random() % nblocks;
		if(writeblock(wr, blocksize, newseed) < blocksize) { return 1; }
		newblock[wr] = 1;
		if(checkblock(check, blocksize,
			newblock[check] ? newseed : seed)) { return 1; }
		printprogress("1 random verify, 1 random write", b, nrandom);
	}

	seed = newseed;

	if(!quietflag) { fprintf(stderr, "\n"); }
	testend();

	return 0;
}

void
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

void
makefiles(int n)
{
	int f;
        for(f = 0; f < n; f++) {
                char tempfilename[] = "cachetest.XXXXXXXX";
                fds[f] = mkstemp(tempfilename);
                if(fds[f] < 0) {
			perror("mkstemp");
			fprintf(stderr, "mkstemp %d/%d failed\n", f, n);
			exit(1);
		}
                assert(fds[f] > 0);
        }
}

void cachequiet(int quiet)
{
	quietflag = quiet;
}

