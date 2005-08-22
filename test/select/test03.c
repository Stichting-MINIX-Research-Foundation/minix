/*
 * Test name: test02.c
 *
 * Objetive: The purpose of this test is to make sure that select works
 * when working with files.
 *
 * Description: This test shows more cases than in test02.c, where every
 * descriptor is ready. Here in one select call only half of the fd's will
 * be ready and in the next one none of them will be ready.  
 *
 * Jose M. Gomez
 */

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>

void dump_fdset(fd_set *set) {
	int i;
	for (i = 0; i < OPEN_MAX; i++)  
	if (FD_ISSET(i, set))
		printf(" %d ", i);
	printf("\n");
}

void main(void) {
	int fd1, fd2, fd3, fd4, fd5, fd6;	/* file descriptors of files */
	fd_set fds_read, fds_write;		/* bit maps */
	struct timeval timeout;			/* timeout */
	int retval;				/* ret value */		

	/* Creates the dummy files with different modes */
	fd1 = open("dummy1.txt", O_CREAT | O_RDONLY);
	if (fd1 < 0) {
		perror("Error opening file"); 
		exit(-1);
	}
	
	fd2 = open("dummy2.txt", O_CREAT | O_RDONLY);
	if (fd2 < 0) {
		perror("Error opening file");
		exit(-1);
	}

	fd3 = open("dummy3.txt", O_CREAT | O_WRONLY);
	if (fd3 < 0) {
		perror("Error opening file");
		exit(-1);
	}

	fd4 = open("dummy4.txt", O_CREAT | O_WRONLY);
	if (fd4 < 0) {
		perror("Error opening file");
		exit(-1);
	}

	fd5 = open("dummy5.txt", O_CREAT | O_RDWR);
	if (fd5 < 0) {
		perror("Error opening file");
		exit(-1);
	}

	fd6 = open("dummy6.txt", O_CREAT | O_RDWR);
	if (fd6 < 0) {
		perror("Error opening file");
		exit(-1);
	}

	/* Create the fd_set structures */
	FD_ZERO(&fds_read);
	FD_ZERO(&fds_write);
	FD_SET(fd1, &fds_write);	/* fd1 => O_RDONLY */
	FD_SET(fd2, &fds_write);	/* fd2 => O_RDONLY */
	FD_SET(fd3, &fds_read);		/* fd3 => O_WRONLY */
	FD_SET(fd4, &fds_read);		/* fd4 => O_WRONLY */
	FD_SET(fd5, &fds_read);		/* fd5 => O_RDWR */
	FD_SET(fd5, &fds_write);	/* fd5 => O_RDWR */
	FD_SET(fd6, &fds_read);		/* fd6 => O_RDWR */
	FD_SET(fd6, &fds_write);	/* fd6 => O_RDWR */

	printf("* Dump INPUT fds_read:\n"); 
	dump_fdset(&fds_read);
	printf("* Dump INPUT fds_write:\n");
	dump_fdset(&fds_write);

	retval=select(9, &fds_read, &fds_write, NULL, NULL); 
	printf("\n***********************\n");
	printf("After select: \n");
	printf("Return value: %d\n", retval);
	printf("* Dump RESULTING fds_read:\n");
	dump_fdset(&fds_read);
	printf("* Dump RESULTING fds_write:\n");
	dump_fdset(&fds_write);	

	/* make a select call where none of them are ready (don't use fd5 and fd6) */

	FD_ZERO(&fds_read);
	FD_ZERO(&fds_write);
	FD_SET(fd1, &fds_write);	/* fd1 => O_RDONLY */
	FD_SET(fd2, &fds_write);	/* fd2 => O_RDONLY */
	FD_SET(fd3, &fds_read);		/* fd3 => O_WRONLY */
	FD_SET(fd4, &fds_read);		/* fd4 => O_WRONLY */

	/* make a select call where none of them are ready (don't use fd5 and fd6) */
	/* create a timeout as well */
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	retval=select(7, &fds_read, &fds_write, NULL, NULL); 
	printf("\n***********************\n");
	printf("After select: \n");
	printf("Return value: %d\n", retval);
	printf("* Dump RESULTING fds_read:\n");
	dump_fdset(&fds_read);
	printf("* Dump RESULTING fds_write:\n");
	dump_fdset(&fds_write);	

	/* close and delete dummy files */
	close(fd1);
	close(fd2);
	close(fd3);
	close(fd4);
	close(fd5);
	close(fd6);
	unlink("dummy1.txt");
	unlink("dummy2.txt");
	unlink("dummy3.txt");
	unlink("dummy4.txt");
	unlink("dummy5.txt");
	unlink("dummy6.txt");
}
