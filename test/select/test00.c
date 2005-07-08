/*
 * Test name: test00.c
 *
 * Objetive: The purpose of this test is to make sure that the bitmap 
 * manipulation macros work without problems.  
 *
 * Description: This tests first fills a fd_set bit by bit, and shows it, then
 * it clears the fd_set bit by bit as well. 
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

int main(int argc, char *argv[]) {
	fd_set fds;
	int i,j;	

	FD_ZERO(&fds);
	for (i=0;i<FD_SETSIZE;i++) {
		/* see if SET works */
		FD_SET(i, &fds);
		if(!FD_ISSET(i, &fds))
			return 1;
	}
	FD_ZERO(&fds);
	for (i=0;i<FD_SETSIZE;i++) {
		/* see if ZERO works */
		if(FD_ISSET(i, &fds))
			return 1;
	}
	for (i=0;i<FD_SETSIZE;i++) {
		FD_SET(i, &fds);
		for(j = 0; j <= i; j++)
			if(!FD_ISSET(j, &fds))
				return 1;
		for(; j < FD_SETSIZE; j++) 
			if(FD_ISSET(j, &fds))
				return 1;
	}
	for (i=0;i<FD_SETSIZE;i++) {
		FD_CLR(i, &fds);
		for(j = 0; j <= i; j++)
			if(FD_ISSET(j, &fds))
				return 1;
		for(; j < FD_SETSIZE; j++) 
			if(!FD_ISSET(j, &fds))
				return 1;
	}
	printf("ok\n");
	return 0;
}
