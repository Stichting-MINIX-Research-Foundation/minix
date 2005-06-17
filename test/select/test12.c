/*
 * Test name: test12.c
 *
 * Objective: The purpose of this check the behaviour when a signal is received
 * and there is a handler.
 * 
 * Description: The program handles SIGHUP and expects the user to actually send
 * the signal from other terminal with 'kill -1 pid' 

 * Jose M. Gomez
 */

#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>

#define SECONDS 15

void catch_hup(int sig_num)
{
	/* don't need to reset signal handler */
	printf("Received a SIGHUP, inside the handler now\n");
}

int main(void) {
	int ret;		/* return value */

	/* set the HUP sginal handler to 'catch_hup' */
	signal(SIGHUP, catch_hup);
	/* Get proc_id and print it */
	printf("Send a signal from other terminal with: kill -1 %d\n", getpid()); 
   	printf("Blocking now on select...\n", SECONDS);
	ret = select(0, NULL, NULL, NULL, NULL); 
	printf("Errno: %d\n", errno);
}
