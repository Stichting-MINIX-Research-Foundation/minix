
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <minix/com.h>
#include <sys/mman.h>
#include <sys/wait.h>

int
main(int argc, char *argv[])
{
#define CHUNKSIZE 8192
#define CHUNKS1	3
#define CHUNKS2	2
#define CHUNKS (CHUNKS1+CHUNKS2)
	int i, fd;
	char *v[CHUNKS];
#define STARTV 0x90000000
	char *vaddr = (char *) STARTV;
	ssize_t l;
	pid_t f;

	printf("Test 44 ");
	fflush(stdout);

	for(i = 0; i < CHUNKS; i++) {
		v[i] = mmap(vaddr, CHUNKSIZE, PROT_READ|PROT_WRITE, 0, -1, 0);
		if(v[i] == MAP_FAILED) {
			perror("mmap");
			fprintf(stderr, "mmap failed\n");
			exit(1);
		}
		if(v[i] != vaddr) {
			fprintf(stderr, "mmap said 0x%p but i wanted 0x%p\n",
				v[i], vaddr);
			exit(1);
		}
		vaddr += CHUNKSIZE;
	}

#define DEV_ZERO "/dev/zero"
	if((fd=open(DEV_ZERO, O_RDONLY)) < 0) {
		perror("open");
		fprintf(stderr, "open failed for %s\n", DEV_ZERO);
		exit(1);
	}

#define TOTAL1 (CHUNKS1*CHUNKSIZE)
	/* Make single read cross region boundary. */
	if((l=read(fd, v[0], TOTAL1)) != TOTAL1) {
		fprintf(stderr, "read %d but expected %d\n", l, TOTAL1);
		exit(1);
	}

	/* Force single copy to cross region boundary. */
	{
		char *t;
		t = v[CHUNKS1]+CHUNKSIZE-2;
		if((l=read(fd, t, CHUNKSIZE)) != CHUNKSIZE) {
			fprintf(stderr, "read %d but expected %d\n", l, CHUNKSIZE);
			exit(1);
		}
	}

	/* Now start a child to test bogus memory access */
	if((f = fork()) == -1) {
		perror("fork");
		exit(1);
	}

	if(f > 0) {
		int st;
		/* Parent waits. */
		if(waitpid(f, &st, 0) < 0) {
			perror("waitpid");
			exit(1);
		}
		if(!WIFEXITED(st)) {
			fprintf(stderr, "child not signaled\n");
			exit(1);
		}
		if(WEXITSTATUS(st) != 0) {
			fprintf(stderr, "child exited with nonzero status\n");
			exit(1);
		}
	} else {
		/* Child performs bogus getsysinfo */
		int res;
		char *buf = v[CHUNKS-1];
		errno = 0;
      		res = getsysinfo( PM_PROC_NR, SI_PROC_TAB, buf);
		if(res >= 0)  {
			fprintf(stderr, "res %d\n", res);
			exit(1);
		}
		if(errno != EFAULT) {
			fprintf(stderr, "errno %d\n", errno);
			exit(1);
		}
		exit(0);
	}

	printf("ok\n");

	exit(0);
}
