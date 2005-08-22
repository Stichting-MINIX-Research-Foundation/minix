/*
 * Test name: test09.c
 *
 * Objetive: The purpose of this test is to make sure that select works
 * when working with the terminal.
 *
 * Description:  This tests wait entry from stdin using select and displays
 * it again in stdout when it is ready to write (which is always)
 * 
 * Jose M. Gomez
 */

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

void main(void) {
	fd_set fds_read, fds_write;
	int retval;
	char data[1024];

	FD_ZERO(&fds_read);
	FD_ZERO(&fds_write);
	FD_SET(0, &fds_read);		/* stdin */
	FD_SET(1, &fds_write);		/* stdout */

	while(1) {
		printf("Input some data: ");
		fflush(stdout);
		retval=select(3, &fds_read, NULL, NULL, NULL); 
		if (retval < 0) {
			fprintf(stderr, "Error while executing select\n");
			exit(-1);
		}
		printf("select retval: %d\n", retval);
		if (!FD_ISSET(0, &fds_read)) {
			fprintf(stderr, "Error: stdin not ready (?)\n");
			exit(-1);
		}
		printf("gets..\n");
		gets(data);
		printf("gets done..\n");
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
