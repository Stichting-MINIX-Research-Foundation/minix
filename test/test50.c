/* Tests for truncate(2) call family - by D.C. van Moolenbroek */
#define _POSIX_SOURCE 1
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#define ITERATIONS 1
#define MAX_ERROR 4

#define TESTFILE "testfile"
#define TESTSIZE 4096
#define THRESHOLD 1048576

#include "common.c"

int main(int argc, char *argv[]);
void prepare(void);
int make_file(off_t size);
void check_file(int fd, off_t size, off_t hole_start, off_t hole_end);
void all_sizes(void (*call) (off_t osize, off_t nsize));
void test50a(void);
void test50b(void);
void test50c(void);
void test50d(void);
void sub50e(off_t osize, off_t nsize);
void test50e(void);
void sub50f(off_t osize, off_t nsize);
void test50f(void);
void sub50g(off_t osize, off_t nsize);
void test50g(void);
void sub50h(off_t osize, off_t nsize);
void test50h(void);
void sub50i(off_t size, off_t off, size_t len, int type);
void test50i(void);

/* Some of the sizes have been chosen in such a way that they should be on the
 * edge of direct/single indirect/double indirect switchovers for a MINIX
 * file system with 4K block size.
 */
static off_t sizes[] = {
  0L, 1L, 511L, 512L, 513L, 1023L, 1024L, 1025L, 2047L, 2048L, 2049L, 3071L,
  3072L, 3073L, 4095L, 4096L, 4097L, 16383L, 16384L, 16385L, 28671L, 28672L,
  28673L, 65535L, 65536L, 65537L, 4222975L, 4222976L, 4222977L
};

static unsigned char *data;

int main(argc, argv)
int argc;
char *argv[];
{
  int j, m = 0xFFFF;

  start(50);
  prepare();
  if (argc == 2) m = atoi(argv[1]);
  for (j = 0; j < ITERATIONS; j++) {
	if (m & 00001) test50a();
	if (m & 00002) test50b();
	if (m & 00004) test50c();
	if (m & 00010) test50d();
	if (m & 00020) test50e();
	if (m & 00040) test50f();
	if (m & 00100) test50g();
	if (m & 00200) test50h();
	if (m & 00400) test50i();
  }

  quit();
  return(-1);			/* impossible */
}

void prepare()
{
  size_t largest;
  int i;

  largest = 0;
  for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
	if (largest < sizes[i]) largest = sizes[i];

  /* internal integrity check: this is needed for early tests */
  assert(largest >= TESTSIZE);

  data = malloc(largest);
  if (data == NULL) e(1000);

  srand(1);

  for (i = 0; i < largest; i++)
	data[i] = (unsigned char) (rand() % 255 + 1);
}

void all_sizes(call)
void(*call) (off_t osize, off_t nsize);
{
  int i, j;

  for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
	for (j = 0; j < sizeof(sizes) / sizeof(sizes[0]); j++)
		call(sizes[i], sizes[j]);
}

int make_file(size)
off_t size;
{
  off_t off;
  int fd, r;

  if ((fd = open(TESTFILE, O_RDWR|O_CREAT|O_EXCL, 0600)) < 0) e(1001);

  off = 0;
  while (off < size) {
	r = write(fd, data + off, size - off);

	if (r != size - off) e(1002);

	off += r;
  }

  return fd;
}

void check_file(fd, hole_start, hole_end, size)
int fd;
off_t hole_start;
off_t hole_end;
off_t size;
{
  static unsigned char buf[16384];
  struct stat statbuf;
  off_t off;
  int i, chunk;

  /* The size must match. */
  if (fstat(fd, &statbuf) != 0) e(1003);
  if (statbuf.st_size != size) e(1004);

  if (lseek(fd, 0L, SEEK_SET) != 0L) e(1005);

  /* All bytes in the file must be equal to what we wrote, except for the bytes
   * in the hole, which must be zero.
   */
  for (off = 0; off < size; off += chunk) {
	chunk = MIN(sizeof(buf), size - off);

	if (read(fd, buf, chunk) != chunk) e(1006);

	for (i = 0; i < chunk; i++) {
		if (off + i >= hole_start && off + i < hole_end) {
			if (buf[i] != 0) e(1007);
		}
		else {
			if (buf[i] != data[off+i]) e(1008);
		}
	}
  }

  /* We must get back EOF at the end. */
  if (read(fd, buf, sizeof(buf)) != 0) e(1009);
}

void test50a()
{
  struct stat statbuf;
  int fd;

  subtest = 1;

  if ((fd = open(TESTFILE, O_WRONLY|O_CREAT|O_EXCL, 0600)) < 0) e(1);

  if (write(fd, data, TESTSIZE) != TESTSIZE) e(2);

  /* Negative sizes should result in EINVAL. */
  if (truncate(TESTFILE, -1) != -1) e(3);
  if (errno != EINVAL) e(4);

  /* Make sure the file size did not change. */
  if (fstat(fd, &statbuf) != 0) e(5);
  if (statbuf.st_size != TESTSIZE) e(6);

  close(fd);
  if (unlink(TESTFILE) != 0) e(7);

  /* An empty path should result in ENOENT. */
  if (truncate("", 0) != -1) e(8);
  if (errno != ENOENT) e(9);

  /* A non-existing file name should result in ENOENT. */
  if (truncate(TESTFILE"2", 0) != -1) e(10);
  if (errno != ENOENT) e(11);
}

void test50b()
{
  struct stat statbuf;
  int fd;

  subtest = 2;

  if ((fd = open(TESTFILE, O_WRONLY|O_CREAT|O_EXCL, 0600)) < 0) e(1);

  if (write(fd, data, TESTSIZE) != TESTSIZE) e(2);

  /* Negative sizes should result in EINVAL. */
  if (ftruncate(fd, -1) != -1) e(3);
  if (errno != EINVAL) e(4);

  /* Make sure the file size did not change. */
  if (fstat(fd, &statbuf) != 0) e(5);
  if (statbuf.st_size != TESTSIZE) e(6);

  close(fd);

  /* Calls on an invalid file descriptor should return EBADF or EINVAL. */
  if (ftruncate(fd, 0) != -1) e(7);
  if (errno != EBADF && errno != EINVAL) e(8);

  if ((fd = open(TESTFILE, O_RDONLY)) < 0) e(9);

  /* Calls on a file opened read-only should return EBADF or EINVAL. */
  if (ftruncate(fd, 0) != -1) e(10);
  if (errno != EBADF && errno != EINVAL) e(11);

  close(fd);

  if (unlink(TESTFILE) != 0) e(12);
}

void test50c()
{
  struct stat statbuf;
  struct flock flock;
  off_t off;
  int fd;

  subtest = 3;

  if ((fd = open(TESTFILE, O_WRONLY|O_CREAT|O_EXCL, 0600)) < 0) e(1);

  if (write(fd, data, TESTSIZE) != TESTSIZE) e(2);

  off = TESTSIZE / 2;
  if (lseek(fd, off, SEEK_SET) != off) e(3);

  flock.l_len = 0;

  /* Negative sizes should result in EINVAL. */
  flock.l_whence = SEEK_SET;
  flock.l_start = -1;
  if (fcntl(fd, F_FREESP, &flock) != -1) e(4);
  if (errno != EINVAL) e(5);

  flock.l_whence = SEEK_CUR;
  flock.l_start = -off - 1;
  if (fcntl(fd, F_FREESP, &flock) != -1) e(6);
  if (errno != EINVAL) e(7);

  flock.l_whence = SEEK_END;
  flock.l_start = -TESTSIZE - 1;
  if (fcntl(fd, F_FREESP, &flock) != -1) e(8);
  if (errno != EINVAL) e(9);

  /* Make sure the file size did not change. */
  if (fstat(fd, &statbuf) != 0) e(10);
  if (statbuf.st_size != TESTSIZE) e(11);

  /* Proper negative values should work, however. */
  flock.l_whence = SEEK_CUR;
  flock.l_start = -1;
  if (fcntl(fd, F_FREESP, &flock) != 0) e(12);

  if (fstat(fd, &statbuf) != 0) e(13);
  if (statbuf.st_size != off - 1) e(14);

  flock.l_whence = SEEK_END;
  flock.l_start = -off + 1;
  if (fcntl(fd, F_FREESP, &flock) != 0) e(15);

  if (fstat(fd, &statbuf) != 0) e(16);
  if (statbuf.st_size != 0L) e(17);

  close(fd);

  /* Calls on an invalid file descriptor should return EBADF or EINVAL. */
  flock.l_whence = SEEK_SET;
  flock.l_start = 0;
  if (fcntl(fd, F_FREESP, &flock) != -1) e(18);
  if (errno != EBADF && errno != EINVAL) e(19);

  if ((fd = open(TESTFILE, O_RDONLY)) < 0) e(20);

  /* Calls on a file opened read-only should return EBADF or EINVAL. */
  if (fcntl(fd, F_FREESP, &flock) != -1) e(21);
  if (errno != EBADF && errno != EINVAL) e(22);

  close(fd);

  if (unlink(TESTFILE) != 0) e(23);
}

void test50d()
{
  struct stat statbuf;
  struct flock flock;
  off_t off;
  int fd;

  subtest = 4;

  if ((fd = open(TESTFILE, O_WRONLY|O_CREAT|O_EXCL, 0600)) < 0) e(1);

  if (write(fd, data, TESTSIZE) != TESTSIZE) e(2);

  off = TESTSIZE / 2;
  if (lseek(fd, off, SEEK_SET) != off) e(3);

  /* The given length must be positive. */
  flock.l_whence = SEEK_CUR;
  flock.l_start = 0;
  flock.l_len = -1;
  if (fcntl(fd, F_FREESP, &flock) != -1) e(4);
  if (errno != EINVAL) e(5);

  /* Negative start positions are not allowed. */
  flock.l_whence = SEEK_SET;
  flock.l_start = -1;
  flock.l_len = 1;
  if (fcntl(fd, F_FREESP, &flock) != -1) e(6);
  if (errno != EINVAL) e(7);

  flock.l_whence = SEEK_CUR;
  flock.l_start = -off - 1;
  if (fcntl(fd, F_FREESP, &flock) != -1) e(8);
  if (errno != EINVAL) e(9);

  flock.l_whence = SEEK_END;
  flock.l_start = -TESTSIZE - 1;
  if (fcntl(fd, F_FREESP, &flock) != -1) e(10);
  if (errno != EINVAL) e(11);

  /* Start positions at or beyond the end of the file are no good, either. */
  flock.l_whence = SEEK_SET;
  flock.l_start = TESTSIZE;
  if (fcntl(fd, F_FREESP, &flock) != -1) e(12);
  if (errno != EINVAL) e(13);

  flock.l_start = TESTSIZE + 1;
  if (fcntl(fd, F_FREESP, &flock) != -1) e(13);
  if (errno != EINVAL) e(14);

  flock.l_whence = SEEK_CUR;
  flock.l_start = TESTSIZE - off;
  if (fcntl(fd, F_FREESP, &flock) != -1) e(15);
  if (errno != EINVAL) e(16);

  flock.l_whence = SEEK_END;
  flock.l_start = 1;
  if (fcntl(fd, F_FREESP, &flock) != -1) e(17);
  if (errno != EINVAL) e(18);

  /* End positions beyond the end of the file may be silently bounded. */
  flock.l_whence = SEEK_SET;
  flock.l_start = 0;
  flock.l_len = TESTSIZE + 1;
  if (fcntl(fd, F_FREESP, &flock) != 0) e(19);

  flock.l_whence = SEEK_CUR;
  flock.l_len = TESTSIZE - off + 1;
  if (fcntl(fd, F_FREESP, &flock) != 0) e(20);

  flock.l_whence = SEEK_END;
  flock.l_start = -1;
  flock.l_len = 2;
  if (fcntl(fd, F_FREESP, &flock) != 0) e(21);

  /* However, this must never cause the file size to change. */
  if (fstat(fd, &statbuf) != 0) e(22);
  if (statbuf.st_size != TESTSIZE) e(23);

  close(fd);

  /* Calls on an invalid file descriptor should return EBADF or EINVAL. */
  flock.l_whence = SEEK_SET;
  flock.l_start = 0;
  if (fcntl(fd, F_FREESP, &flock) != -1) e(24);
  if (errno != EBADF && errno != EINVAL) e(25);

  if ((fd = open(TESTFILE, O_RDONLY)) < 0) e(26);

  /* Calls on a file opened read-only should return EBADF or EINVAL. */
  if (fcntl(fd, F_FREESP, &flock) != -1) e(27);
  if (errno != EBADF && errno != EINVAL) e(28);

  close(fd);

  if (unlink(TESTFILE) != 0) e(29);
}

void sub50e(osize, nsize)
off_t osize;
off_t nsize;
{
  int fd;

  fd = make_file(osize);

  if (truncate(TESTFILE, nsize) != 0) e(1);

  check_file(fd, osize, nsize, nsize);

  if (nsize < osize) {
	if (truncate(TESTFILE, osize) != 0) e(2);

	check_file(fd, nsize, osize, osize);
  }

  close(fd);

  if (unlink(TESTFILE) != 0) e(3);

}

void test50e()
{
  subtest = 5;

  /* truncate(2) on a file that is open. */
  all_sizes(sub50e);
}

void sub50f(osize, nsize)
off_t osize;
off_t nsize;
{
  int fd;

  fd = make_file(osize);

  close(fd);

  if (truncate(TESTFILE, nsize) != 0) e(1);

  if ((fd = open(TESTFILE, O_RDONLY)) < 0) e(2);

  check_file(fd, osize, nsize, nsize);

  if (nsize < osize) {
	close(fd);

	if (truncate(TESTFILE, osize) != 0) e(3);

	if ((fd = open(TESTFILE, O_RDONLY)) < 0) e(4);

	check_file(fd, nsize, osize, osize);
  }

  close(fd);

  if (unlink(TESTFILE) != 0) e(5);
}

void test50f()
{
  subtest = 6;

  /* truncate(2) on a file that is not open. */
  all_sizes(sub50f);
}

void sub50g(osize, nsize)
off_t osize;
off_t nsize;
{
  int fd;

  fd = make_file(osize);

  if (ftruncate(fd, nsize) != 0) e(1);

  check_file(fd, osize, nsize, nsize);

  if (nsize < osize) {
	if (ftruncate(fd, osize) != 0) e(2);

	check_file(fd, nsize, osize, osize);
  }

  close(fd);

  if (unlink(TESTFILE) != 0) e(3);
}

void test50g()
{
  subtest = 7;

  /* ftruncate(2) on an open file. */
  all_sizes(sub50g);
}

void sub50h(osize, nsize)
off_t osize;
off_t nsize;
{
  struct flock flock;
  int fd;

  fd = make_file(osize);

  flock.l_whence = SEEK_SET;
  flock.l_start = nsize;
  flock.l_len = 0;
  if (fcntl(fd, F_FREESP, &flock) != 0) e(1);

  check_file(fd, osize, nsize, nsize);

  if (nsize < osize) {
	flock.l_whence = SEEK_SET;
	flock.l_start = osize;
	flock.l_len = 0;
	if (fcntl(fd, F_FREESP, &flock) != 0) e(2);

	check_file(fd, nsize, osize, osize);
  }

  close(fd);

  if (unlink(TESTFILE) != 0) e(3);
}

void test50h()
{
  subtest = 8;

  /* fcntl(2) with F_FREESP and l_len=0. */
  all_sizes(sub50h);
}

void sub50i(size, off, len, type)
off_t size;
off_t off;
size_t len;
int type;
{
  struct flock flock;
  int fd;

  fd = make_file(size);

  switch (type) {
  case 0:
	flock.l_whence = SEEK_SET;
	flock.l_start = off;
	break;
  case 1:
	if (lseek(fd, off, SEEK_SET) != off) e(1);
	flock.l_whence = SEEK_CUR;
	flock.l_start = 0;
	break;
  case 2:
	flock.l_whence = SEEK_END;
	flock.l_start = off - size;
	break;
  default:
	e(1);
  }

  flock.l_len = len;
  if (fcntl(fd, F_FREESP, &flock) != 0) e(2);

  check_file(fd, off, off + len, size);

  /* Repeat the call in order to see whether the file system can handle holes
   * while freeing up. If not, the server would typically crash; we need not
   * check the results again.
   */
  flock.l_whence = SEEK_SET;
  flock.l_start = off;
  if (fcntl(fd, F_FREESP, &flock) != 0) e(3);

  close(fd);

  if (unlink(TESTFILE) != 0) e(4);
}

void test50i()
{
  off_t off;
  int i, j, k, l;

  subtest = 9;

  /* fcntl(2) with F_FREESP and l_len>0. */

  /* This loop determines the size of the test file. */
  for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
	/* Big files simply take too long. We have to compromise here. */
	if (sizes[i] >= THRESHOLD) continue;

	/* This loop determines one of the two values for the offset. */
	for (j = 0; j < sizeof(sizes) / sizeof(sizes[0]); j++) {
		if (sizes[j] >= sizes[i]) continue;

		/* This loop determines the other. */
		for (k = 0; k < sizeof(sizes) / sizeof(sizes[0]); k++) {
			if (sizes[k] > sizes[j]) continue;

			/* Construct an offset by adding the two sizes. */
			off = sizes[j] + sizes[k];

			if (j + 1 < sizeof(sizes) / sizeof(sizes[0]) &&
				off >= sizes[j + 1]) continue;

			/* This loop determines the length of the hole. */
			for (l = 0; l < sizeof(sizes) / sizeof(sizes[0]); l++) {
				if (sizes[l] == 0 || off + sizes[l] > sizes[i])
					continue;

				/* This could have been a loop, too! */
				sub50i(sizes[i], off, sizes[l], 0);
				sub50i(sizes[i], off, sizes[l], 1);
				sub50i(sizes[i], off, sizes[l], 2);
			}
		}
	}
  }
}
