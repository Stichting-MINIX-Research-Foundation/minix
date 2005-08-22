/*
 * Test name: test10.c
 *
 * Objetive: The purpose of this test is to make sure that select works
 * when working with the terminal with timeouts
 *
 * Description:  This tests wait entry from stdin using select and displays
 * it again in stdout when it is ready to write (which is always). It has
 * a timeout value as well.
 * 
 * Jose M. Gomez
 */

#include <time.h>
#include <sys/types.h>
#include <sys/asynchio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

void main(void) {
	fd_set fds_read, fds_write;
	int retval;
	char data[1024];
	struct timeval timeout;

	while(1) {
		timeout.tv_sec = 3;
		timeout.tv_usec = 0;
		FD_ZERO(&fds_read);
		FD_ZERO(&fds_write);
		FD_SET(0, &fds_read);
		FD_SET(1, &fds_write);
		printf("Input some data: ");
		fflush(stdout);
		retval=select(3, &fds_read, NULL, NULL, &timeout); 
		if (retval < 0) {
			fprintf(stderr, "Error while executing select\n");
			exit(-1);
		}
		if (retval == 0) {
			printf("\n Hey! Feed me some data!\n");
			fflush(stdout);
			continue;
		}
		if (!FD_ISSET(0, &fds_read)) {
			fprintf(stderr, "Error: stdin not ready (?)\n");
			exit(-1);
		}
		gets(data);
		if (!strcmp(data, "exit"))
			exit(0);
		printf("Try to write it back\n");
		retval=select(3, NULL, &fds_write, NULL, NULL);
		if (retval < 0) {
			fprintf(stderr, "Error while executing select\n");
			exit(-1);
		}
		if (!FD_ISSET(1, &fds_write)) {
			fprintf(stderr, "Error: stdout not ready (?)\n");
			exit(-1);
		}
		printf("Data: %s\n", data);
	}
}
