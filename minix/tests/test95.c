/* test95: writable MAP_SHARED file mappings and POSIX shm_open().
 *
 * Covers the VM support for MAP_SHARED|PROT_WRITE file mappings (the page
 * cache block is shared in place, no COW) and the libc shm_open/shm_unlink
 * shims over /dev/shm, as used by wayland's wl_shm:
 *   1. writes by a forked child through an inherited mapping
 *   2. writes through independent mappings in two processes
 *   3. two mappings of the same file at different offsets in one process
 *   4. MAP_PRIVATE writes stay private
 *   5. PROT_WRITE|MAP_SHARED on an O_RDONLY fd fails with EACCES
 *   6. shm_open + shm_unlink + ftruncate + mmap shared across fork
 *
 * Note: dirty shared pages are not yet written back to the file on
 * munmap/msync, so this test does not check the file contents via read(2)
 * after modification through a mapping.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int max_error = 5;
#include "common.h"

#define PGSZ	4096
#define NPAGES	3
#define FSIZE	(NPAGES * PGSZ)
#define TFILE	"t95.data"
#define SHMNAME	"/t95shm"

static void
make_file(const char *path, char fill)
{
	char buf[PGSZ];
	int fd, i;

	memset(buf, fill, sizeof(buf));
	if ((fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
		e(1);
		quit();
	}
	for (i = 0; i < NPAGES; i++)
		if (write(fd, buf, sizeof(buf)) != sizeof(buf)) e(2);
	close(fd);
}

static char *
map_file(const char *path, int oflags, int prot, int flags, off_t off,
	size_t len)
{
	char *p;
	int fd;

	if ((fd = open(path, oflags)) < 0) {
		e(3);
		return MAP_FAILED;
	}
	p = mmap(NULL, len, prot, flags, fd, off);
	close(fd);	/* the mapping must outlive the fd */
	return p;
}

static int
check_fill(const char *p, size_t len, char c)
{
	size_t i;

	for (i = 0; i < len; i++)
		if (p[i] != c) return 0;
	return 1;
}

static void
wait_child_ok(pid_t pid, int errnum)
{
	int status;

	if (waitpid(pid, &status, 0) != pid) e(errnum);
	else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) e(errnum);
}

/* 1. A forked child writes through the inherited shared mapping. */
static void
test_fork_inherit(void)
{
	char *p;
	pid_t pid;

	subtest = 1;
	make_file(TFILE, 'a');
	p = map_file(TFILE, O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED, 0,
	    FSIZE);
	if (p == MAP_FAILED) { e(10); return; }
	if (!check_fill(p, FSIZE, 'a')) e(11);

	switch ((pid = fork())) {
	case -1:
		e(12);
		break;
	case 0:
		memset(p, 'b', PGSZ);
		memset(p + 2 * PGSZ, 'c', PGSZ);
		exit(0);
	default:
		wait_child_ok(pid, 13);
		if (!check_fill(p, PGSZ, 'b')) e(14);
		if (!check_fill(p + PGSZ, PGSZ, 'a')) e(15);
		if (!check_fill(p + 2 * PGSZ, PGSZ, 'c')) e(16);
	}

	if (munmap(p, FSIZE) != 0) e(17);
	unlink(TFILE);
}

/* 2. Independent mappings in two processes, both directions. */
static void
test_two_procs(void)
{
	char *p, c;
	int to_child[2], to_parent[2];
	pid_t pid;

	subtest = 2;
	make_file(TFILE, 'a');
	if (pipe(to_child) != 0 || pipe(to_parent) != 0) { e(20); return; }

	switch ((pid = fork())) {
	case -1:
		e(21);
		return;
	case 0:
		close(to_child[1]);
		close(to_parent[0]);
		p = map_file(TFILE, O_RDWR, PROT_READ | PROT_WRITE,
		    MAP_SHARED, 0, FSIZE);
		if (p == MAP_FAILED) exit(1);
		memset(p + PGSZ, 'x', PGSZ);	/* child -> parent */
		if (write(to_parent[1], "w", 1) != 1) exit(2);
		if (read(to_child[0], &c, 1) != 1) exit(3);
		if (!check_fill(p, PGSZ, 'y')) exit(4); /* parent -> child */
		exit(0);
	}

	close(to_child[0]);
	close(to_parent[1]);
	p = map_file(TFILE, O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED, 0,
	    FSIZE);
	if (p == MAP_FAILED) e(22);
	if (read(to_parent[0], &c, 1) != 1) e(23);
	if (p != MAP_FAILED) {
		if (!check_fill(p + PGSZ, PGSZ, 'x')) e(24);
		memset(p, 'y', PGSZ);
	}
	if (write(to_child[1], "r", 1) != 1) e(25);
	wait_child_ok(pid, 26);

	close(to_child[1]);
	close(to_parent[0]);
	if (p != MAP_FAILED && munmap(p, FSIZE) != 0) e(27);
	unlink(TFILE);
}

/* 3. Two shared mappings of one file at different offsets alias. */
static void
test_alias_offset(void)
{
	char *a, *b;

	subtest = 3;
	make_file(TFILE, 'a');
	a = map_file(TFILE, O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED, 0,
	    FSIZE);
	b = map_file(TFILE, O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED,
	    PGSZ, PGSZ);
	if (a == MAP_FAILED || b == MAP_FAILED) { e(30); return; }
	if (a + PGSZ == b) e(31);	/* must be distinct mappings */

	strcpy(b, "via-b");
	if (strcmp(a + PGSZ, "via-b") != 0) e(32);
	strcpy(a + PGSZ + 100, "via-a");
	if (strcmp(b + 100, "via-a") != 0) e(33);

	if (munmap(a, FSIZE) != 0) e(34);
	if (munmap(b, PGSZ) != 0) e(35);
	unlink(TFILE);
}

/* 4. MAP_PRIVATE writes are not visible through a shared mapping. */
static void
test_private_isolation(void)
{
	char *s, *pv;

	subtest = 4;
	make_file(TFILE, 'a');
	s = map_file(TFILE, O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED, 0,
	    FSIZE);
	pv = map_file(TFILE, O_RDONLY, PROT_READ | PROT_WRITE, MAP_PRIVATE,
	    0, FSIZE);
	if (s == MAP_FAILED || pv == MAP_FAILED) { e(40); return; }

	memset(pv, 'p', PGSZ);
	if (!check_fill(s, PGSZ, 'a')) e(41);
	if (!check_fill(pv, PGSZ, 'p')) e(42);

	/* After the private page was copied, shared writes stay out of it. */
	memset(s, 's', PGSZ);
	if (!check_fill(pv, PGSZ, 'p')) e(43);

	if (munmap(s, FSIZE) != 0) e(44);
	if (munmap(pv, FSIZE) != 0) e(45);
	unlink(TFILE);
}

/* 5. A writable shared mapping requires an fd open for writing. */
static void
test_rdonly_fd(void)
{
	char *p, *w;

	subtest = 5;
	make_file(TFILE, 'a');

	p = map_file(TFILE, O_RDONLY, PROT_READ | PROT_WRITE, MAP_SHARED, 0,
	    FSIZE);
	if (p != MAP_FAILED) {
		e(50);
		munmap(p, FSIZE);
	} else if (errno != EACCES) e(51);

	/* Read-only shared mappings of an O_RDONLY fd are fine, and see
	 * writes made through a writable mapping. */
	p = map_file(TFILE, O_RDONLY, PROT_READ, MAP_SHARED, 0, FSIZE);
	w = map_file(TFILE, O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED, 0,
	    FSIZE);
	if (p == MAP_FAILED || w == MAP_FAILED) { e(52); return; }
	if (!check_fill(p, FSIZE, 'a')) e(53);
	memset(w + 2 * PGSZ, 'w', PGSZ);
	if (!check_fill(p + 2 * PGSZ, PGSZ, 'w')) e(54);

	if (munmap(p, FSIZE) != 0) e(55);
	if (munmap(w, FSIZE) != 0) e(56);
	unlink(TFILE);
}

/* 6. The wl_shm pattern: shm_open, unlink, ftruncate, mmap, share. */
static void
test_shm(void)
{
	struct stat st;
	char *p;
	pid_t pid;
	int fd;

	subtest = 6;
	(void)shm_unlink(SHMNAME);

	if ((fd = shm_open(SHMNAME, O_RDWR | O_CREAT | O_EXCL, 0600)) < 0) {
		e(60);
		return;
	}
	if (shm_open(SHMNAME, O_RDWR | O_CREAT | O_EXCL, 0600) >= 0) e(61);
	else if (errno != EEXIST) e(62);
	if (stat("/dev/shm" SHMNAME, &st) != 0) e(63);

	if (shm_unlink(SHMNAME) != 0) e(64);
	if (stat("/dev/shm" SHMNAME, &st) == 0) e(65);
	if (shm_unlink(SHMNAME) == 0 || errno != ENOENT) e(66);

	if (ftruncate(fd, FSIZE) != 0) e(67);
	p = mmap(NULL, FSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) { e(68); close(fd); return; }
	if (!check_fill(p, FSIZE, 0)) e(69);	/* holes read as zero */

	switch ((pid = fork())) {
	case -1:
		e(70);
		break;
	case 0:
		{
			/* map independently through the inherited fd */
			char *q = mmap(NULL, FSIZE, PROT_READ | PROT_WRITE,
			    MAP_SHARED, fd, 0);
			if (q == MAP_FAILED) exit(1);
			memset(q + PGSZ, 0x5a, PGSZ);
			exit(0);
		}
	default:
		wait_child_ok(pid, 71);
		if (!check_fill(p + PGSZ, PGSZ, 0x5a)) e(72);
		if (!check_fill(p, PGSZ, 0)) e(73);
	}

	if (munmap(p, FSIZE) != 0) e(74);
	close(fd);

	/* Bad names. */
	if (shm_open("/a/b", O_RDWR | O_CREAT, 0600) >= 0 || errno != EINVAL)
		e(75);
	if (shm_open("/", O_RDWR | O_CREAT, 0600) >= 0 || errno != EINVAL)
		e(76);
}

int
main(int argc, char **argv)
{
	start(95);

	test_fork_inherit();
	test_two_procs();
	test_alias_offset();
	test_private_isolation();
	test_rdonly_fd();
	test_shm();

	quit();
	return 0;
}
