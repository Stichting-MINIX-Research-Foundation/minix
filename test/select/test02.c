/*
 * Test name: test02.c
 *
 * Objetive: The purpose of this test is to make sure that select works
 * when working with files.
 *
 * Description: This tests first creates six dummy files with different
 * modes and performs select calls on them evaluating the input and resulting
 * bitmaps. 
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

void dump_fdset(fd_set *set) {
	int i;
	for (i = 0; i < OPEN_MAX; i++)  
	if (FD_ISSET(i, set))
		printf(" %d->1 ", i);
	else 
		printf(" %d->0  ", i);
	printf("\n");
}

void main(void) {
	int fd1, fd2, fd3, fd4, fd5, fd6;	/* file descriptors of files */
	fd_set fds_read, fds_write;
	int retval;

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
	FD_SET(fd1, &fds_read);		/* fd1 => O_RDONLY */
	FD_SET(fd2, &fds_read);		/* fd2 => O_RDONLY */
	FD_SET(fd3, &fds_write);	/* fd3 => O_WRONLY */
	FD_SET(fd4, &fds_write);	/* fd4 => O_WRONLY */
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
