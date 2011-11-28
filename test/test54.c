#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>
#include <stdlib.h>

#define MAX_ERROR 3
#include "common.c"

int subtest = -1;

void do_test(void)
{
	int fd;
	char *wbuf, *rbuf;
	off_t off=0;
	size_t size;
	ssize_t nwritten;
	ssize_t nread;
	char *filename = "pwrite_test_XXXXXXX";
	int i;

	subtest = 1;

	if ((fd = mkstemp(filename)) < 0) e(1);

	size = 1 + rand() % 4096;
	off = rand();

	if((wbuf = malloc(sizeof(char)*size)) == NULL) e(2);

	for(i = 0; i < size; i++) {
		wbuf[i] = 1 + rand()%127;
	}
	
	if ((nwritten = pwrite(fd, wbuf, size, off)) < 0) e(3);
	if ((rbuf = malloc(sizeof(char)*nwritten)) == NULL) e(4);
	if ((nread = pread(fd, rbuf, nwritten, off)) < 0) e(5);
	if (strncmp(rbuf, wbuf, nread) != 0) e(6);

	close(fd);
	free(wbuf);
	unlink(filename);
}


int main(void)
{
	start(54);
	do_test();
	quit();
	return(-1);	/* Unreachable */
}
