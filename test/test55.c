#include <sys/statvfs.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <err.h>
#include <unistd.h>
#define TRIALS 10
#define SIZE   65536

#define TMPPATH "/usr/tmp/"

char *create_file(void)
{
	char buf[SIZE]={0};
	char *p;
	ssize_t ntowrite, nwritten;
	int fd;
	char *filename;

	if((filename = mktemp(TMPPATH "statvfs_test_XXXXXXX")) == NULL) {
		err(1, "mktemp failed");
	}

	if((fd = open(filename, O_CREAT|O_WRONLY)) < 0) {
		err(1, "open failed");
	}

	ntowrite = SIZE;
	p = &buf[0];
	while(ntowrite > 0) {
		if((nwritten = write(fd, p, ntowrite)) < 0) {
			err(1, "write failed");
		}
		p += nwritten;
		ntowrite -= nwritten;
	}

	close(fd);

	return filename;
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

	printf("Test 55 ");
	
	for(i = 0; i < TRIALS; i++) {
		int r;
		char *filename;

		if(statvfs(TMPPATH, &stats) < 0) {
			perror("statvfs failed");
			return 1;
		}

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
		
		filename = create_file();

		r = statvfs(TMPPATH, &stats);

		unlink(filename);

		if(r < 0) {
			perror("statvfs failed");
			return 1;
		}

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

		if ((f_bsize   == f_bsize_new) && 
    		(f_frsize  == f_frsize_new) && 
    		(f_blocks  == f_blocks_new) && 
    		(f_bfree   > f_bfree_new) && 
    		(f_bavail  > f_bavail_new) && 
    		(f_files   == f_files_new) && 
    		(f_ffree   == f_ffree_new + 1) && 
    		(f_favail  == f_favail_new + 1) && 
    		(f_fsid    == f_fsid_new) && 
    		(f_flag    == f_flag_new) && 
    		(f_namemax == f_namemax_new) ) {
				printf("ok\n");
				return 0;
		}
	}

	return 1;
}
