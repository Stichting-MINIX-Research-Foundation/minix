/* testvm - service-started code that goes with test73.o
 */

#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <minix/ds.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "testvm.h"
#include "common.h"
#include "testcache.h"

#define MYMAJOR 40      /* doesn't really matter, shouldn't be NO_DEV though */
#define MYDEV   makedev(MYMAJOR, 1)

static char *pipefilename = NULL, *progname;
int pipefd = -1;

int memfd;

static char *bdata = NULL;

int dowriteblock(int b, int blocksize, u32_t seed, char *block)
{
	int r;
	char *bdata;
	int mustset = 0;
	u64_t dev_off = (u64_t) b * blocksize;

	if((bdata = vm_map_cacheblock(MYDEV, dev_off,
		VMC_NO_INODE, 0, NULL, blocksize)) == MAP_FAILED) {
		if((bdata = mmap(0, blocksize,
		       PROT_READ|PROT_WRITE, MAP_ANON, -1, 0)) == MAP_FAILED) {
			printf("mmap failed\n");
			exit(1);
		}
		mustset = 1;
	}

	memcpy(bdata, block, blocksize);

	if(mustset && (r=vm_set_cacheblock(bdata, MYDEV, dev_off,
		VMC_NO_INODE, 0, NULL, blocksize, 0)) != OK) {
		printf("dowriteblock: vm_set_cacheblock failed %d\n", r);
		exit(1);
	}

	if(munmap(bdata, blocksize) < 0) {
		printf("dowriteblock: munmap failed %d\n", r);
		exit(1);
	}

	return blocksize;
}

int readblock(int b, int blocksize, u32_t seed, char *block)
{
	char *bdata;
	u64_t dev_off = (u64_t) b * blocksize;

	if((bdata = vm_map_cacheblock(MYDEV, dev_off,
		VMC_NO_INODE, 0, NULL, blocksize)) == MAP_FAILED) {
		return OK_BLOCK_GONE;
	}

	memcpy(block, bdata, blocksize);

	if(munmap(bdata, blocksize) < 0) {
		printf("dowriteblock: munmap failed\n");
		exit(1);
	}

	return blocksize;
}

void testend(void) { }

static void
writepipe(struct info *i)
{
	if(write(pipefd, i, sizeof(*i)) != sizeof(*i)) {
		printf("%s: pipe write failed\n", progname);
		exit(1);
	}
}

static int
testinit(void)
{
	struct stat st;
	int attempts = 0;

	for(attempts = 0; attempts < 5 && pipefd < 0; attempts++) {
		if(attempts > 0) sleep(1);
		pipefd = open(pipefilename, O_WRONLY | O_NONBLOCK);
	}

	if(pipefd < 0) {
		printf("%s: could not open pipe %s, errno %d\n",
			progname, pipefilename, errno);
		exit(1);
	}

	if(fstat(pipefd, &st) < 0) {
		printf("%s: could not fstat pipe %s\n", progname, pipefilename);
		exit(1);
	}

	if(!(st.st_mode & I_NAMED_PIPE)) {
		printf("%s: file %s is not a pipe\n", progname, pipefilename);
		exit(1);
	}

	return OK;
}

static int
sef_cb_init(int type, sef_init_info_t *UNUSED(info))
{
	return OK;
}

static void
init(void)
{
	/* SEF init */
	sef_setcb_init_fresh(sef_cb_init);
	sef_setcb_init_lu(sef_cb_init);
	sef_setcb_init_restart(sef_cb_init);

	sef_startup();
}



int
main(int argc, char *argv[])
{
	struct info info;
	int big;
	u32_t totalmem, freemem, cachedmem;

	progname = argv[0];

	if(argc < 2) { printf("no args\n"); return 1; }

	pipefilename=argv[1];

	big = !!strstr(pipefilename, "big");

	init();

	info.result = time(NULL);

	if(testinit() != OK) { printf("%s: testinit failed\n", progname); return 1; }

	cachequiet(!big);

	if(!(bdata = alloc_contig(PAGE_SIZE, 0, NULL))) {
		printf("could not allocate block\n");
		exit(1);
	}

	if(dotest(PAGE_SIZE,       10, 3)) { e(11); exit(1); } 
	if(dotest(PAGE_SIZE,     1000, 3)) { e(11); exit(1); } 
	if(dotest(PAGE_SIZE,    50000, 3)) { e(11); exit(1); } 
	if(big) {
		getmem(&totalmem, &freemem, &cachedmem);
		if(dotest(PAGE_SIZE, totalmem*1.5, 3)) { e(11); exit(1); } 
	}

	info.result = 0;

	writepipe(&info);

	vm_clear_cache(MYDEV);

	return 0;
}

