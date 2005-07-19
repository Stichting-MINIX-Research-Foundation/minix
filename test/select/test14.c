/*
 * Test name: test14.c
 *
 * Objetive: The purpose of this test is to make sure that select works
 * with ptys.
 *
 * Adapted from test11.c (pipe test).
 * 
 * Ben Gras
 */

#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/asynchio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <libutil.h>

void pipehandler(int sig)
{

}

void do_child(int pty_fds[]) 
{
	/* reads from pipe and prints out the data */
	char data[2048];
	int retval;	 
	fd_set fds_read;
	fd_set fds_exception;	
	struct timeval timeout;

	signal(SIGPIPE, pipehandler);
	signal(SIGUSR1, pipehandler);

	/* first, close the write part, since it is not needed */
	close(pty_fds[1]);
	
	while(1) {
		FD_ZERO(&fds_read);
		FD_ZERO(&fds_exception);
		FD_SET(pty_fds[0], &fds_read);
		FD_SET(pty_fds[0], &fds_exception);
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		retval = select(pty_fds[0]+1,  &fds_read, NULL, &fds_exception, &timeout);
		if (retval == -1) {
			perror("select");
			fprintf(stderr, "child: Error in select\n");
			continue;
		} else printf("child select: %d\n", retval);
		if (FD_ISSET(pty_fds[0], &fds_exception)) {
			printf("child: exception fd set. quitting.\n");
			break;
		}
		if (FD_ISSET(pty_fds[0], &fds_read)) {
			printf("child: read fd set. reading.\n");
			if ((retval = read(pty_fds[0], data, sizeof(data))) < 0) {
				perror("read");
				fprintf(stderr, "child: couldn't read from pipe\n");
				exit(-1);
			}
			if(retval == 0) {
				fprintf(stderr, "child: eof on pipe\n");
				break;
			}
			data[retval] = '\0';
			printf("pid %d eipe reads (%d): %s\n", getpid(), retval, data);
		} else printf("child: no fd set\n");
	}
	
	exit(0);
}

void do_parent(int pty_fds[]) 
{
	char data[1024];
	int retval;
	fd_set fds_write;

	signal(SIGPIPE, pipehandler);
	signal(SIGUSR1, pipehandler);

	/* first, close the read part of pty, since it is not needed */
	close(pty_fds[0]);

	/* now enter a loop of read user input, and writing it to the pty */
	while (1) {
		FD_ZERO(&fds_write);
		FD_SET(pty_fds[1], &fds_write);
		printf("pid %d Waiting for pty ready to write...\n", getpid());
		retval = select(pty_fds[1]+1, NULL, &fds_write, NULL, NULL);
		if (retval == -1) {
			perror("select");
			fprintf(stderr, "Parent: Error in select\n");
			exit(-1);
		}		

		printf("Input data: ");
		if(!gets(data)) {
			printf("parent: eof; exiting\n");
			break;
		}
		if (!strcmp(data, "exit"))
			break;
		if (!FD_ISSET(pty_fds[1], &fds_write)) {
			fprintf(stderr, "parent: write fd not set?! retrying\n");
			continue;
		}
		retval = write(pty_fds[1], &data, 1024);
		if (retval == -1) {
			perror("write");
			fprintf(stderr, "Error writing on pty\n");
			exit(-1);
		}
	}

	/* got exit from user */
	close(pty_fds[1]);	/* close pty, let child know we're done */
	wait(&retval);
	printf("Child exited with status: %d\n", retval);
	exit(0);
}

int main(int argc, char *argv[])
{
	int pipes[2];
	int retval;
	int pid;	

	if(openpty(&pipes[0], &pipes[1], NULL, NULL, NULL) < 0) {
		perror("openpty");
		return 1;
	}
	sleep(50);
	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "Error forking\n");
		exit(-1);
	}

	if (pid == 0) /* child proc */
		do_child(pipes);
	else
		do_parent(pipes);

	/* not reached */
	return 0;
}
