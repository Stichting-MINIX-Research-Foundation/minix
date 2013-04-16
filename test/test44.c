
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/wait.h>

int max_error = 2;
#include "common.h"


int subtest = 0;

int
main(int argc, char *argv[])
{
#define CHUNKSIZE 8192
#define CHUNKS1	3
#define CHUNKS2	2
#define CHUNKS (CHUNKS1+CHUNKS2)
#define LARGESIZE 262144
	int i, fd;
	char *v[CHUNKS];
#define STARTV 0x90000000
	char *vaddr = (char *) STARTV;
	ssize_t l;
	pid_t f;

	start(44);

	for(i = 0; i < CHUNKS; i++) {
		v[i] = minix_mmap(vaddr, CHUNKSIZE, PROT_READ|PROT_WRITE, 0,
				  -1, 0);
		if(v[i] == MAP_FAILED) {
			perror("minix_mmap");
			fprintf(stderr, "minix_mmap failed\n");
			quit();
		}
		if(v[i] != vaddr) {
			fprintf(stderr,
				"minix_mmap said 0x%p but i wanted 0x%p\n",
				v[i], vaddr);
			quit();
		}
		vaddr += CHUNKSIZE;
	}

#define DEV_ZERO "/dev/zero"
	if((fd=open(DEV_ZERO, O_RDONLY)) < 0) {
		perror("open");
		fprintf(stderr, "open failed for %s\n", DEV_ZERO);
		quit();
	}

#define TOTAL1 (CHUNKS1*CHUNKSIZE)
	/* Make single read cross region boundary. */
	if((l=read(fd, v[0], TOTAL1)) != TOTAL1) {
		fprintf(stderr, "read %d but expected %d\n", l, TOTAL1);
		quit();
	}

	/* Force single copy to cross region boundary. */
	{
		char *t;
		t = v[CHUNKS1]+CHUNKSIZE-2;
		if((l=read(fd, t, CHUNKSIZE)) != CHUNKSIZE) {
			fprintf(stderr, "read %d but expected %d\n", l, CHUNKSIZE);
			quit();
		}
	}

	/* Now start a child to test bogus memory access */
	if((f = fork()) == -1) {
		perror("fork");
		quit();
	}

	if(f > 0) {
		int st;
		/* Parent waits. */
		if(waitpid(f, &st, 0) < 0) {
			perror("waitpid");
			quit();
		}
		if(!WIFEXITED(st)) {
			fprintf(stderr, "child not signaled\n");
			quit();
		}
		if(WEXITSTATUS(st) != 0) {
			fprintf(stderr, "child exited with nonzero status\n");
			quit();
		}
	} else {
		/* Child performs bogus read */
		int res;
		char *buf = v[CHUNKS-1];
		errno = 0;
		res = read(fd, buf, LARGESIZE);
		if(res >= 0)  {
			fprintf(stderr, "res %d\n", res);
			quit();
		}
		if(errno != EFAULT) {
			fprintf(stderr, "errno %d\n", errno);
			quit();
		}
		return(0);
	}

	quit();
	return(-1);
}
