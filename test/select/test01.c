/*
 * Test name: test01.c
 *
 * Objective: The purpose of this test is to make sure that the timeout mechanisms
 * work without errors.
 * 
 * Description: Executes a select as if it was a sleep and compares the time it
 * has been actually sleeping against the specified time in the select call.
 * Three cases are tested: first, a timeout specified in seconds, second, a timeout in 
 * microseconds, and third, a timeout with more precision than seconds.
 *
 * Jose M. Gomez
 */

#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define SECONDS 3
#define USECONDS 3000000L

int main(void) {
	int r;
	time_t start, end;	/* variables for timing */ 
	struct timeval timeout;	/* timeout structure */

	/* Set timeout for 3 seconds */
	timeout.tv_sec = SECONDS;
	timeout.tv_usec = 0;
   	printf("Sleeping now for %d seconds...\n", SECONDS);
	/* Record time before starting */
	start = time(NULL);
	r = select(0, NULL, NULL, NULL, &timeout); 
	printf("select return code: %d error: %s\n",
		r, strerror(errno));
	end = time(NULL);
	printf("For a timeout with select of %d seconds, it took %d actual seconds\n",
		SECONDS, end-start);

	/* Set timeout for 3 seconds , but specified in microseconds */

	timeout.tv_sec = 0;
	timeout.tv_usec = USECONDS;
	printf("\n***************************\n");
   	printf("Sleeping now for %ld microseconds...\n", USECONDS);
	/* Record time before starting */
	start = time(NULL);
	r = select(0, NULL, NULL, NULL, &timeout); 
	printf("select return code: %d error: %s\n",
		r, strerror(errno));
	end = time(NULL);
	printf("For a timeout with select of %ld useconds, it took %d actual seconds\n",
		USECONDS, end-start);

	/* Set timeout for 1.5 seconds, but specified in microseconds */

	timeout.tv_sec = 0;
	timeout.tv_usec = USECONDS/2;
	printf("\n***************************\n");
   	printf("Sleeping now for %ld microseconds...\n", USECONDS/2);
	/* Record time before starting */
	start = time(NULL);
	r = select(0, NULL, NULL, NULL, &timeout); 
	printf("select return code: %d error: %s\n",
		r, strerror(errno));
	end = time(NULL);
	printf("For a timeout with select of %ld useconds, it took %d actual seconds\n",
		USECONDS/2, end-start);
	return 0;
}
