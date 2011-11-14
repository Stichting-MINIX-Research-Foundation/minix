#include <sys/statvfs.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#define TRIALS 10
#define SIZE   65536

#define MAX_ERROR 3

int subtest;
char *filename = "statvfs_test_XXXXXX";
#include "common.c"

void create_file(void)
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

int main(int argc, char *argv[])
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
	int i;

	start(55);

	subtest = 1;
	for(i = 0; i < TRIALS; i++) {
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

	quit();
	return(-1);
}
