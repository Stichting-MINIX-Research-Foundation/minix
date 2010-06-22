#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>
#include <stdlib.h>

int main(void)
{
	int fd;
	char *wbuf, *rbuf;
	off_t off=0;
	size_t size;
	ssize_t nwritten;
	ssize_t nread;
	char *filename;
	int i;

	printf("Test 54 ");
	fflush(stdout);

	if((filename = mktemp("/tmp/pwrite_test_XXXXXXX")) == NULL) {
		err(1, "Failed to create tempfile");
	}

	if((fd = open(filename, O_CREAT|O_RDWR)) < 0) {
		err(1, "Failed to open %s", filename);
	}

	size = 1 + rand() % 4096;
	off = rand();

	if((wbuf = malloc(sizeof(char)*size)) == NULL) {
		errx(1, "Malloc failed.\n");
	}

	for(i = 0; i < size; i++) {
		wbuf[i] = 1 + rand()%127;
	}
	
	if((nwritten = pwrite(fd, wbuf, size, off)) < 0) {
		err(1, "pwrite failed");
	}

	if((rbuf = malloc(sizeof(char)*nwritten)) == NULL) {
		errx(1, "Malloc failed.\n");
	}

	if((nread = pread(fd, rbuf, nwritten, off)) < 0) {
		err(1, "pread failed");
	}

	if(strncmp(rbuf, wbuf, nread) != 0) {
		err(1, "Test failed.\n");
	}

	printf(" ok\n");

	close(fd);
	free(wbuf);

	return 0;
}
