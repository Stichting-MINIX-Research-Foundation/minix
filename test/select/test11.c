/*
 * Test name: test11.c
 *
 * Objetive: The purpose of this test is to make sure that select works
 * with pipes.
 *
 * Description:  The select checks are divided in checks on writing for the
 * parent process, which has the writing end of the pipe, and checks on reading
 * and exception on the child process, which has the reading end of pipe. So
 * when the first process is ready to write to the pipe it will request a string
 * from the terminal and send it through the pipe. If the string is 'exit' then
 * the pipe is closed. The child process is blocked in a select checking for read
 * and exception. If there is data to be read then it will perform the read and
 * prints the read data. If the pipe is closed (user typed 'exit'), the child 
 * process finishes. 
 * 
 * Jose M. Gomez
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

void pipehandler(int sig)
{

}

void do_child(int data_pipe[]) 
{
	/* reads from pipe and prints out the data */
	char data[2048];
	int retval;	 
	fd_set fds_read;
	fd_set fds_exception;	
	struct timeval timeout;

	signal(SIGPIPE, pipehandler);
	signal(SIGUSR1, pipehandler);

	/* first, close the write part of the pipe, since it is not needed */
	close(data_pipe[1]);
	
	while(1) {
		FD_ZERO(&fds_read);
		FD_ZERO(&fds_exception);
		FD_SET(data_pipe[0], &fds_read);
		FD_SET(data_pipe[0], &fds_exception);
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		retval = select(data_pipe[0]+1,  &fds_read, NULL, &fds_exception, &timeout);
		if (retval == -1) {
			perror("select");
			fprintf(stderr, "child: Error in select\n");
			continue;
		} else printf("child select: %d\n", retval);
		if (FD_ISSET(data_pipe[0], &fds_exception)) {
			printf("child: exception fd set. quitting.\n");
			break;
		}
		if (FD_ISSET(data_pipe[0], &fds_read)) {
			printf("child: read fd set. reading.\n");
			if ((retval = read(data_pipe[0], data, sizeof(data))) < 0) {
				perror("read");
				fprintf(stderr, "child: couldn't read from pipe\n");
				exit(-1);
			}
			if(retval == 0) {
				fprintf(stderr, "child: eof on pipe\n");
				break;
			}
			data[retval] = '\0';
			printf("pid %d Pipe reads (%d): %s\n", getpid(), retval, data);
		} else printf("child: no fd set\n");
	}
	
	/* probably pipe was broken, or got EOF via the pipe. */
	exit(0);
}

void do_parent(int data_pipe[]) 
{
	char data[1024];
	int retval;
	fd_set fds_write;

	signal(SIGPIPE, pipehandler);
	signal(SIGUSR1, pipehandler);

	/* first, close the read part of pipe, since it is not needed */
	close(data_pipe[0]);

	/* now enter a loop of read user input, and writing it to the pipe */
	while (1) {
		FD_ZERO(&fds_write);
		FD_SET(data_pipe[1], &fds_write);
		printf("pid %d Waiting for pipe ready to write...\n", getpid());
		retval = select(data_pipe[1]+1, NULL, &fds_write, NULL, NULL);
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
		if (!FD_ISSET(data_pipe[1], &fds_write)) {
			fprintf(stderr, "parent: write fd not set?! retrying\n");
			continue;
		}
		retval = write(data_pipe[1], &data, 1024);
		if (retval == -1) {
			perror("write");
			fprintf(stderr, "Error writing on pipe\n");
			exit(-1);
		}
	}

	/* got exit from user */
	close(data_pipe[1]);	/* close pipe, let child know we're done */
	wait(&retval);
	printf("Child exited with status: %d\n", retval);
	exit(0);
}

int main(int argc, char *argv[])
{
	int pipes[2];
	int retval;
	int pid;	

	/* create the pipe */
	retval = pipe(pipes);
	if (retval == -1) {
		perror("pipe");
		fprintf(stderr, "Error creating the pipe\n");
		exit(-1);
	}
	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "Error forking\n");
		exit(-1);
	}

	if (pid == 0) /* child proc */
		do_child(pipes);
	else
		do_parent(pipes);
	return 0;
}
