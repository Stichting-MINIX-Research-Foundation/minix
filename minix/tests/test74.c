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
#include <minix/paths.h>
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
				TARGETNAME, (char *)buf);
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
	/* the system call just has to fail gracefully */
	(void)symlink(buf, NODENAME);
}

static void do_symlink2(void *buf, int fd, int writable)
{
	/* the system call just has to fail gracefully */
	(void)symlink(NODENAME, buf);
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
	struct timeval timeout = { 0, 200000 };	/* 0.2 sec */
	/* the system call just has to fail gracefully */
	(void)select(1, buf, NULL, NULL, &timeout);
}

static void do_select2(void *buf, int fd, int writable)
{
	struct timeval timeout = { 0, 200000 };	/* 1 sec */
	/* the system call just has to fail gracefully */
	(void)select(1, NULL, buf, NULL, &timeout);
}

static void do_select3(void *buf, int fd, int writable)
{
	struct timeval timeout = { 0, 200000 };	/* 1 sec */
	/* the system call just has to fail gracefully */
	(void)select(1, NULL, NULL, buf, &timeout);
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

static void test_memory_types_vs_operations(void)
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

static void basic_regression(void)
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
	if (fd1 < 0) fd1 = open("../testsh1.sh", O_RDONLY);
	fd2 = open("../testsh2", O_RDONLY);
	if (fd2 < 0) fd2 = open("../testsh2.sh", O_RDONLY);
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
#define LOCATION2 ((void *)((char *)LOCATION1 + PAGE_SIZE))
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

/*
 * Test mmap on none-dev file systems - file systems that do not have a buffer
 * cache and therefore have to fake mmap support.  We use procfs as target.
 * The idea is that while we succeed in mapping in /proc/uptime, we also get
 * a new uptime value every time we map in the page -- VM must not cache it.
 */
static void
nonedev_regression(void)
{
	int fd, fd2;
	char *buf;
	unsigned long uptime1, uptime2, uptime3;

	subtest++;

	if ((fd = open(_PATH_PROC "uptime", O_RDONLY)) < 0) e(1);

	buf = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE | MAP_FILE, fd, 0);
	if (buf == MAP_FAILED) e(2);

	if (buf[4095] != 0) e(3);

	if ((uptime1 = atoi(buf)) == 0) e(4);

	if (munmap(buf, 4096) != 0) e(5);

	sleep(2);

	buf = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FILE,
	    fd, 0);
	if (buf == MAP_FAILED) e(6);

	if (buf[4095] != 0) e(7);

	if ((uptime2 = atoi(buf)) == 0) e(8);

	if (uptime1 == uptime2) e(9);

	if (munmap(buf, 4096) != 0) e(10);

	sleep(2);

	buf = mmap(NULL, 4096, PROT_READ, MAP_SHARED | MAP_FILE, fd, 0);
	if (buf == MAP_FAILED) e(11);

	if (buf[4095] != 0) e(12);

	if ((uptime3 = atoi(buf)) == 0) e(13);

	if (uptime1 == uptime3) e(14);
	if (uptime2 == uptime3) e(15);

	if (munmap(buf, 4096) != 0) e(16);

	/* Also test page faults not incurred by the process itself. */
	if ((fd2 = open("testfile", O_CREAT | O_TRUNC | O_WRONLY)) < 0) e(17);

	if (unlink("testfile") != 0) e(18);

	buf = mmap(NULL, 4096, PROT_READ, MAP_SHARED | MAP_FILE, fd, 0);
	if (buf == MAP_FAILED) e(19);

	if (write(fd2, buf, 10) != 10) e(20);

	if (munmap(buf, 4096) != 0) e(21);

	close(fd2);
	close(fd);
}

/*
 * Regression test for a nasty memory-mapped file corruption bug, which is not
 * easy to reproduce but, before being solved, did occur in practice every once
 * in a while.  The executive summary is that through stale inode associations,
 * VM could end up using an old block to satisfy a memory mapping.
 *
 * This subtest relies on a number of assumptions regarding allocation and
 * reuse of inode numbers and blocks.  These assumptions hold for MFS but
 * possibly no other file system.  However, if the subtest's assumptions are
 * not met, it will simply succeed.
 */
static void
corruption_regression(void)
{
	char *ptr, *buf;
	struct statvfs sf;
	struct stat st;
	size_t block_size;
	off_t size;
	int fd, fd2;

	subtest = 1;

	if (statvfs(".", &sf) != 0) e(0);
	block_size = sf.f_bsize;

	if ((buf = malloc(block_size * 2)) == NULL) e(0);

	/*
	 * We first need a file that is just large enough that it requires the
	 * allocation of a metadata block - an indirect block - when more data
	 * is written to it.  This is fileA.  We keep it open throughout the
	 * test so we can unlink it immediately.
	 */
	if ((fd = open("fileA", O_CREAT | O_TRUNC | O_WRONLY, 0600)) == -1)
		e(0);
	if (unlink("fileA") != 0) e(0);

	/*
	 * Write to fileA until its next block requires the allocation of an
	 * additional metadata block - an indirect block.
	 */
	size = 0;
	memset(buf, 'A', block_size);
	do {
		/*
		 * Repeatedly write an extra block, until the file consists of
		 * more blocks than just the file data.
		 */
		if (write(fd, buf, block_size) != block_size) e(0);
		size += block_size;
		if (size >= block_size * 64) {
			/*
			 * It doesn't look like this is going to work.
			 * Skip this subtest altogether.
			 */
			if (close(fd) != 0) e(0);
			free(buf);

			return;
		}
		if (fstat(fd, &st) != 0) e(0);
	} while (st.st_blocks * 512 == size);

	/* Once we get there, go one step back by truncating by one block. */
	size -= block_size; /* for MFS, size will end up being 7*block_size */
	if (ftruncate(fd, size) != 0) e(0);

	/*
	 * Create a first file, fileB, and write two blocks to it.  FileB's
	 * blocks are going to end up in the secondary VM cache, associated to
	 * fileB's inode number (and two different offsets within the file).
	 * The block cache does not know about files getting deleted, so we can
	 * unlink fileB immediately after creating it.  So far so good.
	 */
	if ((fd2 = open("fileB", O_CREAT | O_TRUNC | O_WRONLY, 0600)) == -1)
		e(0);
	if (unlink("fileB") != 0) e(0);
	memset(buf, 'B', block_size * 2);
	if (write(fd2, buf, block_size * 2) != block_size * 2) e(0);
	if (close(fd2) != 0) e(0);

	/*
	 * Write one extra block to fileA, hoping that this causes allocation
	 * of a metadata block as well.  This is why we tried to get fileA to
	 * the point that one more block would also require the allocation of a
	 * metadata block.  Our intent is to recycle the blocks that we just
	 * allocated and freed for fileB.  As of writing, for the metadata
	 * block, this will *not* break the association with fileB's inode,
	 * which by itself is not a problem, yet crucial to reproducing
	 * the actual problem a bit later.  Note that the test does not rely on
	 * whether the file system allocates the data block or the metadata
	 * block first, although it does need reverse deallocation (see below).
	 */
	memset(buf, 'A', block_size);
	if (write(fd, buf, block_size) != block_size) e(0);

	/*
	 * Create a new file, fileC, which recycles the inode number of fileB,
	 * but uses two new blocks to store its data.  These new blocks will
	 * get associated to the fileB inode number, and one of them will
	 * thereby eclipse (but not remove) the association of fileA's metadata
	 * block to the inode of fileB.
	 */
	if ((fd2 = open("fileC", O_CREAT | O_TRUNC | O_WRONLY, 0600)) == -1)
		e(0);
	if (unlink("fileC") != 0) e(0);
	memset(buf, 'C', block_size * 2);
	if (write(fd2, buf, block_size * 2) != block_size * 2) e(0);
	if (close(fd2) != 0) e(0);

	/*
	 * Free up the extra fileA blocks for reallocation, in particular
	 * including the metadata block.  Again, this will not affect the
	 * contents of the VM cache in any way.  FileA's metadata block remains
	 * cached in VM, with the inode association for fileB's block.
	 */
	if (ftruncate(fd, size) != 0) e(0);

	/*
	 * Now create yet one more file, fileD, which also recycles the inode
	 * number of fileB and fileC.  Write two blocks to it; these blocks
	 * should recycle the blocks we just freed.  One of these is fileA's
	 * just-freed metadata block, for which the new inode association will
	 * be equal to the inode association it had already (as long as blocks
	 * are freed in reverse order of their allocation, which happens to be
	 * the case for MFS).  As a result, the block is not updated in the VM
	 * cache, and VM will therefore continue to see the inode association
	 * for the corresponding block of fileC which is still in the VM cache.
	 */
	if ((fd2 = open("fileD", O_CREAT | O_TRUNC | O_RDWR, 0600)) == -1)
		e(0);
	memset(buf, 'D', block_size * 2);
	if (write(fd2, buf, block_size * 2) != block_size * 2) e(0);

	ptr = mmap(NULL, block_size * 2, PROT_READ, MAP_FILE, fd2, 0);
	if (ptr == MAP_FAILED) e(0);

	/*
	 * Finally, we can test the issue.  Since fileC's block is still the
	 * block for which VM has the corresponding inode association, VM will
	 * now find and map in fileC's block, instead of fileD's block.  The
	 * result is that we get a memory-mapped area with stale contents,
	 * different from those of the underlying file.
	 */
	if (memcmp(buf, ptr, block_size * 2)) e(0);

	/* Clean up. */
	if (munmap(ptr, block_size * 2) != 0) e(0);

	if (close(fd2) != 0) e(0);
	if (unlink("fileD") != 0) e(0);

	if (close(fd) != 0) e(0);

	free(buf);
}

/*
 * Test mmap on file holes.  Holes are a tricky case with the current VM
 * implementation.  There are two main issues.  First, whenever a file data
 * block is freed, VM has to know about this, or it will later blindly map in
 * the old data.  This, file systems explicitly tell VM (through libminixfs)
 * whenever a block is freed, upon which VM cache forgets the block.  Second,
 * blocks are accessed primarily by a <dev,dev_off> pair and only additionally
 * by a <ino,ino_off> pair.  Holes have no meaningful value for the first pair,
 * but do need to be registered in VM with the second pair, or accessing them
 * will generate a segmentation fault.  Thus, file systems explicitly tell VM
 * (through libminixfs) when a hole is being peeked; libminixfs currently fakes
 * a device offset to make this work.
 */
static void
hole_regression(void)
{
	struct statvfs st;
	size_t block_size;
	char *buf;
	int fd;

	if (statvfs(".", &st) < 0) e(1);

	block_size = st.f_bsize;

	if ((buf = malloc(block_size)) == NULL) e(2);

	if ((fd = open("testfile", O_CREAT | O_TRUNC | O_RDWR)) < 0) e(3);

	if (unlink("testfile") != 0) e(4);

	/*
	 * We perform the test twice, in a not-so-perfect attempt to test the
	 * two aspects independently.  The first part immediately creates a
	 * hole, and is supposed to fail only if reporting holes to VM does not
	 * work.  However, it may also fail if a page for a previous file with
	 * the same inode number as "testfile" is still in the VM cache.
	 */
	memset(buf, 12, block_size);

	if (write(fd, buf, block_size) != block_size) e(5);

	if (lseek(fd, block_size * 2, SEEK_CUR) != block_size * 3) e(6);

	memset(buf, 78, block_size);

	if (write(fd, buf, block_size) != block_size) e(7);

	free(buf);

	if ((buf = mmap(NULL, 4 * block_size, PROT_READ, MAP_SHARED | MAP_FILE,
	    fd, 0)) == MAP_FAILED) e(8);

	if (buf[0 * block_size] != 12 || buf[1 * block_size - 1] != 12) e(9);
	if (buf[1 * block_size] !=  0 || buf[2 * block_size - 1] !=  0) e(10);
	if (buf[2 * block_size] !=  0 || buf[3 * block_size - 1] !=  0) e(11);
	if (buf[3 * block_size] != 78 || buf[4 * block_size - 1] != 78) e(12);

	if (munmap(buf, 4 * block_size) != 0) e(13);

	/*
	 * The second part first creates file content and only turns part of it
	 * into a file hole, thus ensuring that VM has previously cached pages
	 * for the blocks that are freed.  The test will fail if VM keeps the
	 * pages around in its cache.
	 */
	if ((buf = malloc(block_size)) == NULL) e(14);

	if (lseek(fd, block_size, SEEK_SET) != block_size) e(15);

	memset(buf, 34, block_size);

	if (write(fd, buf, block_size) != block_size) e(16);

	memset(buf, 56, block_size);

	if (write(fd, buf, block_size) != block_size) e(17);

	if (ftruncate(fd, block_size) != 0) e(18);

	if (lseek(fd, block_size * 3, SEEK_SET) != block_size * 3) e(19);

	memset(buf, 78, block_size);

	if (write(fd, buf, block_size) != block_size) e(20);

	free(buf);

	if ((buf = mmap(NULL, 4 * block_size, PROT_READ, MAP_SHARED | MAP_FILE,
	    fd, 0)) == MAP_FAILED) e(21);

	if (buf[0 * block_size] != 12 || buf[1 * block_size - 1] != 12) e(22);
	if (buf[1 * block_size] !=  0 || buf[2 * block_size - 1] !=  0) e(23);
	if (buf[2 * block_size] !=  0 || buf[3 * block_size - 1] !=  0) e(24);
	if (buf[3 * block_size] != 78 || buf[4 * block_size - 1] != 78) e(25);

	if (munmap(buf, 4 * block_size) != 0) e(26);

	close(fd);
}

/*
 * Test that soft faults during file system I/O do not cause functions to
 * return partial I/O results.
 *
 * We refer to the faults that are caused internally within the operating
 * system as a result of the deadlock mitigation described at the top of this
 * file, as a particular class of "soft faults".  Such soft faults may occur in
 * the middle of an I/O operation, and general I/O semantics dictate that upon
 * partial success, the partial success is returned (and *not* an error).  As a
 * result, these soft faults, if not handled as special cases, may cause even
 * common file system operations such as read(2) on a regular file to return
 * fewer bytes than requested.  Such unexpected short reads are typically not
 * handled well by userland, and the OS must prevent them from occurring if it
 * can.  Note that read(2) is the most problematic, but certainly not the only,
 * case where this problem can occur.
 *
 * Unfortunately, several file system services are not following the proper
 * general I/O semantics - and this includes MFS.  Therefore, for now, we have
 * to test this case using block device I/O, which does do the right thing.
 * In this test we hope that the root file system is mounted on a block device
 * usable for (read-only!) testing purposes.
 */
static void
softfault_partial(void)
{
	struct statvfs stf;
	struct stat st;
	char *buf, *buf2;
	ssize_t size;
	int fd;

	if (statvfs("/", &stf) != 0) e(0);

	/*
	 * If the root file system is not mounted off a block device, or if we
	 * cannot open that device ourselves, simply skip this subtest.
	 */
	if (stat(stf.f_mntfromname, &st) != 0 || !S_ISBLK(st.st_mode))
		return; /* skip subtest */

	if ((fd = open(stf.f_mntfromname, O_RDONLY)) == -1)
		return; /* skip subtest */

	/*
	 * See if we can read in the first two full blocks, or two pages worth
	 * of data, whichever is larger.  If that fails, there is no point in
	 * continuing the test.
	 */
	size = MAX(stf.f_bsize, PAGE_SIZE) * 2;

	if ((buf = mmap(NULL, size, PROT_READ | PROT_READ,
	    MAP_ANON | MAP_PRIVATE | MAP_PREALLOC, -1, 0)) == MAP_FAILED) e(0);

	if (read(fd, buf, size) != size) {
		munmap(buf, size);
		close(fd);
		return; /* skip subtest */
	}

	lseek(fd, 0, SEEK_SET);

	/*
	 * Now attempt a read to a partially faulted-in buffer.  The first time
	 * around, the I/O transfer will generate a fault and return partial
	 * success.  In that case, the entire I/O transfer should be retried
	 * after faulting in the missing page(s), thus resulting in the read
	 * succeeding in full.
	 */
	if ((buf2 = mmap(NULL, size, PROT_READ | PROT_READ,
	    MAP_ANON | MAP_PRIVATE, -1, 0)) == MAP_FAILED) e(0);
	buf2[0] = '\0'; /* fault in the first page */

	if (read(fd, buf2, size) != size) e(0);

	/* The result should be correct, too. */
	if (memcmp(buf, buf2, size)) e(0);

	/* Clean up. */
	munmap(buf2, size);
	munmap(buf, size);

	close(fd);
}

int
main(int argc, char *argv[])
{
	int i, iter = 2;

	start(74);

	basic_regression();

	nonedev_regression();

	/*
	 * Any inode or block allocation happening concurrently with this
	 * subtest will make the subtest succeed without testing the actual
	 * issue.  Thus, repeat the subtest a fair number of times.
	 */
	for (i = 0; i < 10; i++)
		corruption_regression();

	hole_regression();

	test_memory_types_vs_operations();

	softfault_partial();

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

