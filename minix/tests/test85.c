/* Test for end-of-file during block device I/O - by D.C. van Moolenbroek */
/* This test needs to be run as root; it sets up and uses a VND instance. */
/*
 * The test should work with all root file system block sizes, but only tests
 * certain corner cases if the root FS block size is twice the page size.
 */
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <minix/partition.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#define VNCONFIG "/usr/sbin/vnconfig"

#define SECTOR_SIZE 512		/* this should be the sector size of VND */

#define ITERATIONS 3

enum {
	BEFORE_EOF,
	UPTO_EOF,
	ACROSS_EOF,
	ONEPAST_EOF,
	FROM_EOF,
	BEYOND_EOF
};

#include "common.h"

static int need_cleanup = 0;

static int dev_fd;
static size_t dev_size;
static char *dev_buf;
static char *dev_ref;

static size_t block_size;
static size_t page_size;
static int test_peek;

static char *mmap_ptr = NULL;
static size_t mmap_size;

static int pipe_fd[2];

/*
 * Fill the given buffer with random contents.
 */
static void
fill_buf(char * buf, size_t size)
{

	while (size--)
		*buf++ = lrand48() & 0xff;
}

/*
 * Place the elements of the source array in the destination array in random
 * order.  There are probably better ways to do this, but it is morning, and I
 * haven't had coffee yet, so go away.
 */
static void
scramble(int * dst, const int * src, int count)
{
	int i, j, k;

	for (i = 0; i < count; i++)
		dst[i] = i;

	for (i = count - 1; i >= 0; i--) {
		j = lrand48() % (i + 1);

		k = dst[j];
		dst[j] = dst[i];
		dst[i] = src[k];
	}
}

/*
 * Perform I/O using read(2) and check the returned results against the
 * expected result and the image reference data.
 */
static void
io_read(size_t pos, size_t len, size_t expected)
{
	ssize_t bytes;

	assert(len > 0 && len <= dev_size);
	assert(expected <= len);

	if (lseek(dev_fd, (off_t)pos, SEEK_SET) != pos) e(0);

	memset(dev_buf, 0, len);

	if ((bytes = read(dev_fd, dev_buf, len)) < 0) e(0);

	if (bytes != expected) e(0);

	if (memcmp(&dev_ref[pos], dev_buf, bytes)) e(0);
}

/*
 * Perform I/O using write(2) and check the returned result against the
 * expected result.  Update the image reference data as appropriate.
 */
static void
io_write(size_t pos, size_t len, size_t expected)
{
	ssize_t bytes;

	assert(len > 0 && len <= dev_size);
	assert(expected <= len);

	if (lseek(dev_fd, (off_t)pos, SEEK_SET) != pos) e(0);

	fill_buf(dev_buf, len);

	if ((bytes = write(dev_fd, dev_buf, len)) < 0) e(0);

	if (bytes != expected) e(0);

	if (bytes > 0) {
		assert(pos + bytes <= dev_size);

		memcpy(&dev_ref[pos], dev_buf, bytes);
	}
}

/*
 * Test if reading from the given pointer succeeds or not, and return the
 * result.
 */
static int
is_readable(char * ptr)
{
	ssize_t r;
	char byte;

	/*
	 * If we access the pointer directly, we will get a fatal signal.
	 * Thus, for that to work we would need a child process, making the
	 * whole test slow and noisy.  Let a service try the operation instead.
	 */
	r = write(pipe_fd[1], ptr, 1);

	if (r == 1) {
		/* Don't fill up the pipe. */
		if (read(pipe_fd[0], &byte, 1) != 1) e(0);

		return 1;
	} else if (r != -1 || errno != EFAULT)
		e(0);

	return 0;
}

/*
 * Perform I/O using mmap(2) and check the returned results against the
 * expected result and the image reference data.  Ensure that bytes beyond the
 * device end are either zero (on the remainder of the last page) or
 * inaccessible on pages entirely beyond the device end.
 */
static void
io_peek(size_t pos, size_t len, size_t expected)
{
	size_t n, delta, mapped_size;
	char *ptr;

	assert(test_peek);

	delta = pos % page_size;

	pos -= delta;
	len += delta;

	len = roundup(len, page_size);

	/* Don't bother with the given expected value.  Recompute it. */
	if (pos < dev_size)
		expected = MIN(dev_size - pos, len);
	else
		expected = 0;

	mapped_size = roundup(dev_size, page_size);

	assert(!(len % page_size));

	ptr = mmap(NULL, len, PROT_READ, MAP_PRIVATE | MAP_FILE, dev_fd,
	    (off_t)pos);

	/*
	 * As of writing, VM allows memory mapping at any offset and for any
	 * length.  At least for block devices, VM should probably be changed
	 * to throw ENXIO for any pages beyond the file end, which in turn
	 * renders all the SIGBUS tests below obsolete.
	 */
	if (ptr == MAP_FAILED) {
		if (pos + len <= mapped_size) e(0);
		if (errno != ENXIO) e(0);

		return;
	}

	mmap_ptr = ptr;
	mmap_size = len;

	/*
	 * Any page that contains any valid part of the mapped device should be
	 * readable and have correct contents for that part.  If the last valid
	 * page extends beyond the mapped device, its remainder should be zero.
	 */
	if (pos < dev_size) {
		/* The valid part should have the expected device contents. */
		if (memcmp(&dev_ref[pos], ptr, expected)) e(0);

		/* The remainder, if any, should be zero. */
		for (n = expected; n % page_size; n++)
			if (ptr[n] != 0) e(0);
	}

	/*
	 * Any page entirely beyond EOF should not be mapped in.  In order to
	 * ensure that is_readable() works, also test pages that are mapped in.
	 */
	for (n = pos; n < pos + len; n += page_size)
		if (is_readable(&ptr[n - pos]) != (n < mapped_size)) e(0);

	munmap(ptr, len);

	mmap_ptr = NULL;
}

/*
 * Perform one of the supported end-of-file access attempts using one I/O
 * operation.
 */
static void
do_one_io(int where, void (* io_proc)(size_t, size_t, size_t))
{
	size_t start, bytes;

	switch (where) {
	case BEFORE_EOF:
		bytes = lrand48() % (dev_size - 1) + 1;

		io_proc(dev_size - bytes - 1, bytes, bytes);

		break;

	case UPTO_EOF:
		bytes = lrand48() % dev_size + 1;

		io_proc(dev_size - bytes, bytes, bytes);

		break;

	case ACROSS_EOF:
		start = lrand48() % (dev_size - 1) + 1;
		bytes = dev_size - start + 1;
		assert(start < dev_size && start + bytes > dev_size);
		bytes += lrand48() % (dev_size - bytes + 1);

		io_proc(start, bytes, dev_size - start);

		break;

	case ONEPAST_EOF:
		bytes = lrand48() % (dev_size - 1) + 1;

		io_proc(dev_size - bytes + 1, bytes, bytes - 1);

		break;

	case FROM_EOF:
		bytes = lrand48() % dev_size + 1;

		io_proc(dev_size, bytes, 0);

		break;

	case BEYOND_EOF:
		start = dev_size + lrand48() % dev_size + 1;
		bytes = lrand48() % dev_size + 1;

		io_proc(start, bytes, 0);

		break;

	default:
		assert(0);
	}
}

/*
 * Perform I/O operations, testing all the supported end-of-file access
 * attempts in a random order so as to detect possible problems with caching.
 */
static void
do_io(void (* io_proc)(size_t, size_t, size_t))
{
	static const int list[] = { BEFORE_EOF, UPTO_EOF, ACROSS_EOF,
	    ONEPAST_EOF, FROM_EOF, BEYOND_EOF };
	static const int count = sizeof(list) / sizeof(list[0]);
	int i, where[count];

	scramble(where, list, count);

	for (i = 0; i < count; i++)
		do_one_io(where[i], io_proc);
}

/*
 * Set up an image file of the given size, assign it to a VND, and open the
 * resulting block device.  The size is size_t because we keep a reference copy
 * of its entire contents in memory.
 */
static void
setup_image(size_t size)
{
	struct part_geom part;
	size_t off;
	ssize_t bytes;
	int fd, status;

	dev_size = size;
	if ((dev_buf = malloc(dev_size)) == NULL) e(0);
	if ((dev_ref = malloc(dev_size)) == NULL) e(0);

	if ((fd = open("image", O_CREAT | O_TRUNC | O_RDWR, 0644)) < 0) e(0);

	fill_buf(dev_ref, dev_size);

	for (off = 0; off < dev_size; off += bytes) {
		bytes = write(fd, &dev_ref[off], dev_size - off);

		if (bytes <= 0) e(0);
	}

	close(fd);

	status = system(VNCONFIG " vnd0 image 2>/dev/null");
	if (!WIFEXITED(status)) e(0);
	if (WEXITSTATUS(status) != 0) {
		printf("skipped\n"); /* most likely cause: vnd0 is in use */
		cleanup();
		exit(0);
	}

	need_cleanup = 1;

	if ((dev_fd = open("/dev/vnd0", O_RDWR)) < 0) e(0);

	if (ioctl(dev_fd, DIOCGETP, &part) < 0) e(0);

	if (part.size != dev_size) e(0);
}

/*
 * Clean up the VND we set up previously.  This function is also called in case
 * of an unexpected exit.
 */
static void
cleanup_device(void)
{
	int status;

	if (!need_cleanup)
		return;

	if (mmap_ptr != NULL) {
		munmap(mmap_ptr, mmap_size);

		mmap_ptr = NULL;
	}

	if (dev_fd >= 0)
		close(dev_fd);

	status = system(VNCONFIG " -u vnd0 2>/dev/null");
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(0);

	need_cleanup = 0;
}

/*
 * Signal handler for exceptions.
 */
static void
got_signal(int __unused sig)
{

	cleanup_device();

	exit(1);
}

/*
 * Clean up the VND and image file we set up previously.
 */
static void
cleanup_image(void)
{
	size_t off;
	ssize_t bytes;
	int fd;

	cleanup_device();

	if ((fd = open("image", O_RDONLY, 0644)) < 0) e(0);

	for (off = 0; off < dev_size; off += bytes) {
		bytes = read(fd, &dev_buf[off], dev_size - off);

		if (bytes <= 0) e(0);
	}

	close(fd);

	/* Have all changes written back to the device? */
	if (memcmp(dev_buf, dev_ref, dev_size)) e(0);

	unlink("image");

	free(dev_buf);
	free(dev_ref);
}

/*
 * Run the full test for a block device with the given size.
 */
static void
do_test(size_t size)
{
	int i;

	/*
	 * Using the three I/O primitives (read, write, peek), we run four
	 * sequences, mainly to test the effects of blocks being cached or not.
	 * We set up a new image for each sequence, because -if everything goes
	 * right- closing the device file also clears all cached blocks for it,
	 * in both the root file system's cache and the VM cache.  Note that we
	 * currently do not even attempt to push the blocks out of the root FS'
	 * cache in order to test retrieval from the VM cache, since this would
	 * involve doing a LOT of extra I/O.
	 */
	for (i = 0; i < 4; i++) {
		setup_image(size);

		switch (i) {
		case 0:
			do_io(io_read);

			/* FALLTHROUGH */
		case 1:
			do_io(io_write);

			do_io(io_read);

			break;

		case 2:
			do_io(io_peek);

			/* FALLTHROUGH */

		case 3:
			do_io(io_write);

			do_io(io_peek);

			break;
		}

		cleanup_image();
	}
}

/*
 * Test program for end-of-file conditions during block device I/O.
 */
int
main(void)
{
	static const unsigned int blocks[] = { 1, 4, 3, 5, 2 };
	struct statvfs buf;
	int i, j;

	start(85);

	setuid(geteuid());

	signal(SIGINT, got_signal);
	signal(SIGABRT, got_signal);
	signal(SIGSEGV, got_signal);
	signal(SIGBUS, got_signal);
	atexit(cleanup_device);

	srand48(time(NULL));

	if (pipe(pipe_fd) != 0) e(0);

	/*
	 * Get the system page size, and align all memory mapping offsets and
	 * sizes accordingly.
	 */
	page_size = sysconf(_SC_PAGESIZE);

	/*
	 * Get the root file system block size.  In the current MINIX3 system
	 * architecture, the root file system's block size determines the
	 * transfer granularity for I/O on unmounted block devices.  If this
	 * block size is not a multiple of the page size, we are (currently!)
	 * not expecting memory-mapped block devices to work.
	 */
	if (statvfs("/", &buf) < 0) e(0);

	block_size = buf.f_bsize;

	test_peek = !(block_size % page_size);

	for (i = 0; i < ITERATIONS; i++) {
		/*
		 * The 'blocks' array is scrambled so as to detect any blocks
		 * left in the VM cache (or not) across runs, just in case.
		 */
		for (j = 0; j < sizeof(blocks) / sizeof(blocks[0]); j++) {
			do_test(blocks[j] * block_size + SECTOR_SIZE);

			do_test(blocks[j] * block_size);

			do_test(blocks[j] * block_size - SECTOR_SIZE);
		}
	}

	quit();
}
