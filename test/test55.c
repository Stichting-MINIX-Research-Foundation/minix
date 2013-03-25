/* Tests for statvfs(2) call family */
#include <sys/statvfs.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#define TRIALS 10
#define SIZE   65536
#define FSMAX	64

int max_error = 3;
#include "common.h"

int do_getvfsstat(struct statvfs *buf, size_t bufsz, int flags, int count);
void compare_statvfs(struct statvfs *st1, struct statvfs *st2);
void test55a(void);
void test55b(void);

int subtest;
char filename[] = "statvfs_test_XXXXXX";

static void create_file(void)
{
	char buf[SIZE]={0};
	char *p;
	ssize_t ntowrite, nwritten;
	int fd;

	subtest = 2;
	if ((fd = mkstemp(filename)) < 0) e(1);

	ntowrite = SIZE;
	p = &buf[0];
	while (ntowrite > 0) {
		if ((nwritten = write(fd, p, ntowrite)) < 0) e(2);
		p += nwritten;
		ntowrite -= nwritten;
	}

	if (close(fd) < 0) e(3);
}

void test55a(void)
{
	struct statvfs stats;
	unsigned long f_bsize,   f_bsize_new;   
	unsigned long f_frsize,  f_frsize_new;  
	fsblkcnt_t    f_blocks,  f_blocks_new;  
	fsblkcnt_t    f_bfree,   f_bfree_new;   
	fsblkcnt_t    f_bavail,  f_bavail_new;  
	fsfilcnt_t    f_files,   f_files_new;   
	fsfilcnt_t    f_ffree,   f_ffree_new;   
	fsfilcnt_t    f_favail,  f_favail_new;  
	unsigned long f_fsid,    f_fsid_new;    
	unsigned long f_flag,    f_flag_new;    
	unsigned long f_namemax, f_namemax_new; 

	subtest = 1;

	if (statvfs(".", &stats) < 0) e(1);

	f_bsize   = stats.f_bsize  ;
	f_frsize  = stats.f_frsize ;
	f_blocks  = stats.f_blocks ;
	f_bfree   = stats.f_bfree  ;
	f_bavail  = stats.f_bavail ;
	f_files   = stats.f_files  ;
	f_ffree   = stats.f_ffree  ;
	f_favail  = stats.f_favail ;
	f_fsid    = stats.f_fsid   ;
	f_flag    = stats.f_flag   ;
	f_namemax = stats.f_namemax;

	create_file();

	if (statvfs(".", &stats) < 0) e(2);
	if (unlink(filename) < 0) e(3);

	f_bsize_new   = stats.f_bsize  ;
	f_frsize_new  = stats.f_frsize ;
	f_blocks_new  = stats.f_blocks ;
	f_bfree_new   = stats.f_bfree  ;
	f_bavail_new  = stats.f_bavail ;
	f_files_new   = stats.f_files  ;
	f_ffree_new   = stats.f_ffree  ;
	f_favail_new  = stats.f_favail ;
	f_fsid_new    = stats.f_fsid   ;
	f_flag_new    = stats.f_flag   ;
	f_namemax_new = stats.f_namemax;

	if (!((f_bsize   == f_bsize_new) &&
		(f_frsize  == f_frsize_new) &&
		(f_blocks  == f_blocks_new) &&
		(f_bfree   > f_bfree_new) &&
		(f_bavail  > f_bavail_new) &&
		(f_files   == f_files_new) &&
		(f_ffree   == f_ffree_new + 1) &&
		(f_favail  == f_favail_new + 1) &&
		(f_fsid    == f_fsid_new) &&
		(f_flag    == f_flag_new) &&
		(f_namemax == f_namemax_new))) {
			e(4);
	}
}

int do_getvfsstat(struct statvfs *buf, size_t bufsz, int flags, int count)
{
	int i, j;

	if (getvfsstat(buf, bufsz, flags) != count) e(101);

	/* All file system identifiers should be unique. */
	for (i = 0; i < count - 1; i++) {
		for (j = i + 1; j < count; j++) {
			if (buf[i].f_fsid == buf[j].f_fsid) e(102);
			if (!memcmp(&buf[i].f_fsidx, &buf[j].f_fsidx,
				sizeof(buf[j].f_fsidx))) e(103);
		}
	}

	/* Expect one root file system. */
	j = -1;
	for (i = 0; i < count; i++) {
		if (!strcmp(buf[i].f_mntonname, "/")) {
			if (j != -1) e(104);
			j = i;
		}
	}
	if (j == -1) e(105);

	return j;
}

void compare_statvfs(struct statvfs *st1, struct statvfs *st2)
{
	int i;

	/* The structures should basically be identical, but a background
	 * process calling statvfs for some reason might screw things up.
	 * Thus, we only compare fields that we know should be identical.
	 * For the strings, we use memcmp rather than strcmp to ensure that
	 * no garbage is left in the fields.
	 */
	if (st1->f_flag != st2->f_flag) e(201);
	if (st1->f_bsize != st2->f_bsize) e(202);
	if (st1->f_frsize != st2->f_frsize) e(203);
	if (st1->f_iosize != st2->f_iosize) e(204);

	if (st1->f_fsid != st2->f_fsid) e(205);
	if (memcmp(&st1->f_fsidx, &st2->f_fsidx, sizeof(st1->f_fsidx))) e(206);

	if (st1->f_namemax != st2->f_namemax) e(207);
	if (st1->f_owner != st2->f_owner) e(208);

	for (i = 0; i < sizeof(st1->f_spare) / sizeof(st1->f_spare[0]); i++) {
		if (st1->f_spare[i] != 0) e(209);
		if (st2->f_spare[i] != 0) e(210);
	}

	if (memcmp(st1->f_fstypename, st2->f_fstypename,
		sizeof(st1->f_fstypename))) e(211);
	if (memcmp(st1->f_mntonname, st2->f_mntonname,
		sizeof(st1->f_mntonname))) e(212);
	if (memcmp(st1->f_mntfromname, st2->f_mntfromname,
		sizeof(st1->f_mntfromname))) e(213);
}

void test55b(void)
{
	static struct statvfs buf[FSMAX];
	struct statvfs rootbuf;
	int count, root;

	subtest = 2;

	count = getvfsstat(NULL, 0, ST_WAIT);
	if (count < 2) e(1); /* we have at least the root FS and ProcFS */
	if (count > FSMAX) e(2);

	if (getvfsstat(buf, 0, ST_WAIT) != 0) e(3);
	if (getvfsstat(buf, sizeof(buf[0]) - 1, ST_WAIT) != 0) e(4);
	if (getvfsstat(buf, sizeof(buf[0]), ST_WAIT) != 1) e(5);
	if (getvfsstat(buf, sizeof(buf[0]) + 1, ST_WAIT) != 1) e(6);
	if (getvfsstat(buf, sizeof(buf[0]) * 2, ST_WAIT) != 2) e(7);

	/* We assume that nothing is being un/mounted right now. */
	root = do_getvfsstat(buf, sizeof(buf), ST_WAIT, count);

	/* Compare cached and uncached copies. */
	if (statvfs1("/", &rootbuf, ST_NOWAIT) != 0) e(13);
	compare_statvfs(&buf[root], &rootbuf);

	/* Do the same again, but now the other way around. */
	rootbuf = buf[root];
	root = do_getvfsstat(buf, sizeof(buf), ST_NOWAIT, count);
	compare_statvfs(&buf[root], &rootbuf);
}

int main(int argc, char **argv)
{
	int i;

	start(55);

	for(i = 0; i < TRIALS; i++) {
		test55a();
		test55b();
	}

	quit();
	return(-1);
}
