/*
 * Test name: speed.c
 *
 * Objetive:  Test the time it takes for select to run. 
 * 
 * Description: This tests creates a number of udp connections and performs 
 * a select call waiting on them for reading with timeout of 0.
 * This is done 10,000 thousands of times and then the average time it takes
 * is computed
 *
 * Jose M. Gomez
 */

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/asynchio.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <net/netlib.h>
#include <time.h>

#define NUMBER 12

void main(void) {
	char *udp_device;
	int fd[NUMBER];
	fd_set fds_write;
	struct timeval timeout;
	time_t start_time, end_time;
	int i;

	FD_ZERO(&fds_write);
	for (i = 0; i < NUMBER; i++) {
		fd[i] = open("/dev/tty", O_RDWR);
		if (fd[i] < 0) {
			fprintf(stderr, "Error opening tty %d\n", i); 
			exit(-1);
		}
		FD_SET(fd[i], &fds_write);
	}	

	printf("Select will send 1 msg to terminal and %d to inet: \n", NUMBER);
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	/* get initial time */
	start_time = time(NULL);
	for (i = 0; i < 32000; i++) {
		select(NUMBER + 4, NULL, &fds_write, NULL, &timeout); 
	}
	/* get final time */
	end_time = time(NULL);
	printf("The select call took on average: %f\n", (float)(end_time - start_time) /  32000.0);
	for (i = 0; i < NUMBER; i++) {
		close(fd[i]);
	}
}
