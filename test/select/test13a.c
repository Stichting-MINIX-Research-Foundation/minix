/*
 * Test name: test13a.c
 *
 * Objective: The purpose of this tests is to show how a select can 
 * get into a race condition when dealing with signals.
 * 
 * Description: The program waits for SIGHUP or input in the terminal          
 */         

#include <sys/types.h>
#include <sys/select.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>

int got_sighup = 0;

void catch_hup(int sig_num)
{
	printf("Received a SIGHUP, set global vble \n");
	got_sighup = 1;
}

int main(void) {
	int ret;		/* return value */
	fd_set read_fds;	
	char data[1024];

	/* Init read fd_set */
	FD_ZERO(&read_fds);
	FD_SET(0, &read_fds);

	/* set the HUP signal handler to 'catch_hup' */
	signal(SIGHUP, catch_hup);

	/* Get proc_id and print it */
	printf("Send a signal from other terminal with: kill -1 %d\n", getpid()); 
	printf("Going to sleep for 5 seconds, if the signal arrives meanwhile\n");
	printf("the process will be blocked until there is input in the keyboard\n");
	printf("if the signal arrives after the timeout and while in select, it will\n");
	printf("behave as it should.\n");
	printf("Sleeping for 5 secs\n");
	sleep(5);

   	printf("Blocking now on select...\n");
	ret = select(1, &read_fds, NULL, NULL, NULL); 
	if (got_sighup) {
		printf("We have a sighup signal so exit the program\n");
		exit(0);
	}
	gets(data);
	printf("Got entry for terminal then, bye\n");
}
