/* Test 74 - mmap functionality & regression test.
 *
 * This test tests some basic functionality of mmap, and also some
 * cases that are quite complex for the system to handle.
 *
 * Memory pages are generally made available on demand. Memory copying
 * is done by the kernel. As the kernel may encounter pagefaults in
 * legitimate memory ranges (e.g. pages that aren't mapped; pages that
 * are mapped RO as they are COW), it cooperates with VM to make the
 * mappings and let the copy succeed transparently.
 *
 * With file-mapped ranges this can result in a deadlock, if care is
 * not taken, as the copy might be request by VFS or an FS. This test
 * triggers as many of these states as possible to ensure they are
 * successful or (where appropriate) fail gracefully, i.e. without 
 * deadlock.
 *
 * To do this, system calls are done with source or target buffers with
 * missing or readonly mappings, both anonymous and file-mapped. The
 * cache is flushed before mmap() so that we know the mappings should
 * not be present on mmap() time. Then e.g. a read() or write() is
 * executed with that buffer as target. This triggers a FS copying
 * to or from a missing range that it itself is needed to map in first.
 * VFS detects this, requests VM to map in the pages, which does so with
 * the help of another VFS thread and the FS, and then re-issues the
 * request to the FS.
 *
 * Another case is the VFS itself does such a copy. This is actually
 * unusual as filenames are already faulted in by the requesting process
 * in libc by strlen(). select() allows such a case, however, so this
 * is tested too. We are satisfied if the call completes.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/ioc_memory.h>
#include <sys/param.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include "common.h"
#include "testcache.h"

int max_error = 0;	/* make all e()'s fatal */

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
	char *mmapdata;
	int pread_first = random() % 2;

	get_fd_offset(b, blocksize, &offset, &fd);

	if(pread_first) {
		if(pread(fd, data, blocksize, offset) < blocksize) {
			perror("pread");
			return -1;
		}
	}

	if((mmapdata = mmap(NULL, blocksize, PROT_READ, MAP_PRIVATE | MAP_FILE,
		fd, offset)) == MAP_FAILED) {
		perror("mmap");
		return -1;
	}

	if(!pread_first) {
		if(pread(fd, data, blocksize, offset) < blocksize) {
			perror("pread");
			return -1;
		}
	}

	if(memcmp(mmapdata, data, blocksize)) {
		fprintf(stderr, "readblock: mmap, pread mismatch\n");
		return -1;
	}

	if(munmap(mmapdata, blocksize) < 0) {
		perror("munmap");
		return -1;
	}

	return blocksize;
}

void testend(void) { }

static void do_read(void *buf, int fd, int writable)
{
	ssize_t ret;
	size_t n = PAGE_SIZE;
	struct stat sb;
	if(fstat(fd, &sb) < 0) e(1);
	if(S_ISDIR(sb.st_mode)) return;
	ret = read(fd, buf, n);

	/* if the buffer is writable, it should succeed */
	if(writable) { if(ret != n) e(3); return; }

	/* if the buffer is not writable, it should fail with EFAULT */
	if(ret >= 0) e(4);
	if(errno != EFAULT) e(5);
}

static void do_write(void *buf, int fd, int writable)
{
	size_t n = PAGE_SIZE;
	struct stat sb;
	if(fstat(fd, &sb) < 0) e(1);
	if(S_ISDIR(sb.st_mode)) return;
	if(write(fd, buf, n) != n) e(3);
}

static void do_stat(void *buf, int fd, int writable)
{
	int r;
	struct stat sb;
	r = fstat(fd, (struct stat *) buf);

	/* should succeed if buf is writable */
	if(writable) { if(r < 0) e(3); return; }

	/* should fail with EFAULT if buf is not */
	if(r >= 0) e(4);
	if(errno != EFAULT) e(5);
}

static void do_getdents(void *buf, int fd, int writable)
{
	struct stat sb;
	int r;
	if(fstat(fd, &sb) < 0) e(1);
	if(!S_ISDIR(sb.st_mode)) return;	/* OK */
	r = getdents(fd, buf, PAGE_SIZE);
	if(writable) { if(r < 0) e(3); return; }

	/* should fail with EFAULT if buf is not */
	if(r >= 0) e(4);
	if(errno != EFAULT) e(5);
}

static void do_readlink1(void *buf, int fd, int writable)
{
	char target[200];
	/* the system call just has to fail gracefully */
	readlink(buf, target, sizeof(target));
}

#define NODENAME	"a"
#define TARGETNAME	"b"

static void do_readlink2(void *buf, int fd, int writable)
{
	ssize_t rl;
	unlink(NODENAME);
	if(symlink(TARGETNAME, NODENAME) < 0) e(1);
	rl=readlink(NODENAME, buf, sizeof(buf));

	/* if buf is writable, it should succeed, with a certain result */
	if(writable) {
		if(rl < 0) e(2);
		((char *) buf)[rl] = '\0';
		if(strcmp(buf, TARGETNAME)) {
			fprintf(stderr, "readlink: expected %s, got %s\n",
				TARGETNAME, buf);
			e(3);
		}
		return;
	}

	/* if buf is not writable, it should fail with EFAULT */
	if(rl >= 0) e(4);

	if(errno != EFAULT) e(5);
}

static void do_symlink1(void *buf, int fd, int writable)
{
	int r;
	/* the system call just has to fail gracefully */
	r = symlink(buf, NODENAME);
}

static void do_symlink2(void *buf, int fd, int writable)
{
	int r;
	/* the system call just has to fail gracefully */
	r = symlink(NODENAME, buf);
}

static void do_open(void *buf, int fd, int writable)
{
	int r;
	/* the system call just has to fail gracefully */
	r = open(buf, O_RDONLY);
	if(r >= 0) close(r);
}

static void do_select1(void *buf, int fd, int writable)
{
	int r;
	struct timeval timeout = { 0, 200000 };	/* 0.2 sec */
	/* the system call just has to fail gracefully */
	r = select(1, buf, NULL, NULL, &timeout);
}

static void do_select2(void *buf, int fd, int writable)
{
	int r;
	struct timeval timeout = { 0, 200000 };	/* 1 sec */
	/* the system call just has to fail gracefully */
	r = select(1, NULL, buf, NULL, &timeout);
}

static void do_select3(void *buf, int fd, int writable)
{
	int r;
	struct timeval timeout = { 0, 200000 };	/* 1 sec */
	/* the system call just has to fail gracefully */
	r = select(1, NULL, NULL, buf, &timeout);
}

static void fillfile(int fd, int size)
{
	char *buf = malloc(size);

	if(size < 1 || size % PAGE_SIZE || !buf) { e(1); }
	memset(buf, 'A', size);
	buf[50] = '\0';	/* so it can be used as a filename arg */
	buf[size-1] = '\0';
	if(write(fd, buf, size) != size) { e(2); }
	if(lseek(fd, SEEK_SET, 0) < 0) { e(3); }
	free(buf);
}

static void make_buffers(int size,
	int *ret_fd_rw, int *ret_fd_ro,
	void **filebuf_rw, void **filebuf_ro, void **anonbuf)
{
	char fn_rw[] = "testfile_rw.XXXXXX", fn_ro[] = "testfile_ro.XXXXXX";
	*ret_fd_rw = mkstemp(fn_rw);
	*ret_fd_ro = mkstemp(fn_ro);

	if(size < 1 || size % PAGE_SIZE) { e(2); }
	if(*ret_fd_rw < 0) { e(1); }
	if(*ret_fd_ro < 0) { e(1); }
	fillfile(*ret_fd_rw, size);
	fillfile(*ret_fd_ro, size);
	if(fcntl(*ret_fd_rw, F_FLUSH_FS_CACHE) < 0) { e(4); }
	if(fcntl(*ret_fd_ro, F_FLUSH_FS_CACHE) < 0) { e(4); }

	if((*filebuf_rw = mmap(0, size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_FILE, *ret_fd_rw, 0)) == MAP_FAILED) {
		e(5);
		quit();
	}

	if((*filebuf_ro = mmap(0, size, PROT_READ,
		MAP_PRIVATE | MAP_FILE, *ret_fd_ro, 0)) == MAP_FAILED) {
		e(5);
		quit();
	}

	if((*anonbuf = mmap(0, size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANON, -1, 0)) == MAP_FAILED) {
		e(6);
		quit();
	}

	if(unlink(fn_rw) < 0) { e(12); }
	if(unlink(fn_ro) < 0) { e(12); }
}

static void forget_buffers(void *buf1, void *buf2, void *buf3, int fd1, int fd2, int size)
{
	if(munmap(buf1, size) < 0) { e(1); }
	if(munmap(buf2, size) < 0) { e(2); }
	if(munmap(buf3, size) < 0) { e(2); }
	if(fcntl(fd1, F_FLUSH_FS_CACHE) < 0) { e(3); }
	if(fcntl(fd2, F_FLUSH_FS_CACHE) < 0) { e(3); }
	if(close(fd1) < 0) { e(4); }
	if(close(fd2) < 0) { e(4); }
}

#define NEXPERIMENTS 12
struct {
	void (*do_operation)(void * buf, int fd, int writable);
} experiments[NEXPERIMENTS] = {
	{ do_read },
	{ do_write },
	{ do_stat },
	{ do_getdents },
	{ do_readlink1 },
	{ do_readlink2 },
	{ do_symlink1 },
	{ do_symlink2 },
	{ do_open, },
	{ do_select1 },
	{ do_select2 },
	{ do_select3 },
};

void test_memory_types_vs_operations(void)
{
#define NFDS 4
#define BUFSIZE (10 * PAGE_SIZE)
	int exp, fds[NFDS];
	int f = 0, size = BUFSIZE;

	/* open some test fd's */
#define OPEN(fn, mode) { assert(f >= 0 && f < NFDS); \
	fds[f] = open(fn, mode); if(fds[f] < 0) { e(2); } f++; }
	OPEN("regular", O_RDWR | O_CREAT);
	OPEN(".", O_RDONLY);
	OPEN("/dev/ram", O_RDWR);
	OPEN("/dev/zero", O_RDWR);

	/* make sure the regular file has plenty of size to play with */
	fillfile(fds[0], BUFSIZE);

	/* and the ramdisk too */
        if(ioctl(fds[2], MIOCRAMSIZE, &size) < 0) { e(3); }

	for(exp = 0; exp < NEXPERIMENTS; exp++) {
		for(f = 0; f < NFDS; f++) {
			void *anonmem, *filemem_rw, *filemem_ro;
			int buffd_rw, buffd_ro;

			make_buffers(BUFSIZE, &buffd_rw, &buffd_ro,
				&filemem_rw, &filemem_ro, &anonmem);

			if(lseek(fds[f], 0, SEEK_SET) != 0) { e(10); }
			experiments[exp].do_operation(anonmem, fds[f], 1);

			if(lseek(fds[f], 0, SEEK_SET) != 0) { e(11); }
			experiments[exp].do_operation(filemem_rw, fds[f], 1);

			if(lseek(fds[f], 0, SEEK_SET) != 0) { e(12); }
			experiments[exp].do_operation(filemem_ro, fds[f], 0);

			forget_buffers(filemem_rw, filemem_ro, anonmem, buffd_rw, buffd_ro, BUFSIZE);
		}
	}
}

void basic_regression(void)
{
	int fd, fd1, fd2;
	ssize_t rb, wr;
	char buf[PAGE_SIZE*2];
	void *block, *block1, *block2;
#define BLOCKSIZE (PAGE_SIZE*10)
	block = mmap(0, BLOCKSIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANON, -1, 0);

	if(block == MAP_FAILED) { e(1); }

	memset(block, 0, BLOCKSIZE);

	/* shrink from bottom */
	munmap(block, PAGE_SIZE);

	/* Next test: use a system call write() to access a block of
	 * unavailable file-mapped memory.
	 * 
	 * This is a thorny corner case to make succeed transparently
	 * because 
	 *   (1) it is a filesystem that is doing the memory access
	 *       (copy from the constblock1 range in this process to the
	 *       FS) but is also the FS needed to satisfy the range if it
	 *       isn't in the cache.
	 *   (2) there are two separate memory regions involved, requiring
	 *       separate VFS requests from VM to properly satisfy, requiring
	 *       some complex state to be kept.
	 */

	fd1 = open("../testsh1", O_RDONLY);
	fd2 = open("../testsh2", O_RDONLY);
	if(fd1 < 0 || fd2 < 0) { e(2); }

	/* just check that we can't mmap() a file writable */
	if(mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FILE, fd1, 0) != MAP_FAILED) {
		e(1);
	}

	/* check that we can mmap() a file MAP_SHARED readonly */
	if(mmap(NULL, PAGE_SIZE, PROT_READ, MAP_SHARED | MAP_FILE, fd1, 0) == MAP_FAILED) {
		e(1);
	}

	/* clear cache of files before mmap so pages won't be present already */
	if(fcntl(fd1, F_FLUSH_FS_CACHE) < 0) { e(1); }
	if(fcntl(fd2, F_FLUSH_FS_CACHE) < 0) { e(1); }

#define LOCATION1 (void *) 0x90000000
#define LOCATION2 (LOCATION1 + PAGE_SIZE)
	block1 = mmap(LOCATION1, PAGE_SIZE, PROT_READ, MAP_PRIVATE | MAP_FILE, fd1, 0);
	if(block1 == MAP_FAILED) { e(4); }
	if(block1 != LOCATION1) { e(5); }

	block2 = mmap(LOCATION2, PAGE_SIZE, PROT_READ, MAP_PRIVATE | MAP_FILE, fd2, 0);
	if(block2 == MAP_FAILED) { e(10); }
	if(block2 != LOCATION2) { e(11); }

	unlink("testfile");
	fd = open("testfile", O_CREAT | O_RDWR);
	if(fd < 0) { e(15); }

	/* write() using the mmap()ped memory as buffer */

	if((wr=write(fd, LOCATION1, sizeof(buf))) != sizeof(buf)) {
		fprintf(stderr, "wrote %zd bytes instead of %zd\n",
			wr, sizeof(buf));
		e(20);
		quit();
	}

	/* verify written contents */

	if((rb=pread(fd, buf, sizeof(buf), 0)) != sizeof(buf)) {
		if(rb < 0) perror("pread");
		fprintf(stderr, "wrote %zd bytes\n", wr);
		fprintf(stderr, "read %zd bytes instead of %zd\n",
			rb, sizeof(buf));
		e(21);
		quit();
	}

	if(memcmp(buf, LOCATION1, sizeof(buf))) {
		e(22);
		quit();
	}

	close(fd);
	close(fd1);
	close(fd2);

}

int
main(int argc, char *argv[])
{
	int iter = 2;

	start(74);

	basic_regression();

	test_memory_types_vs_operations();

	makefiles(MAXFILES);

	cachequiet(!bigflag);
	if(bigflag) iter = 3;

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

