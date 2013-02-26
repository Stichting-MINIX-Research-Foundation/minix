#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_ERROR 5
#include "common.c"

static int fd = 0;

void copy_subtests(void);
void test_open_cloexec(void);
void test_open_fork(void);

void
copy_subtests()
{
	char *subtests[] = { "t67a", "t67b" };
	char copy_cmd[8 + PATH_MAX + 1];
	int i, no_tests;

	no_tests = sizeof(subtests) / sizeof(char *);

	for (i = 0; i < no_tests; i++) {
		snprintf(copy_cmd, 8 + PATH_MAX, "cp ../%s .", subtests[i]);
		system(copy_cmd);
	}
}

void
test_open_cloexec()
{
	int flags;
	pid_t pid;

	/* Let's create a file with O_CLOEXEC turned on */
	fd = open("file", O_RDWR|O_CREAT|O_CLOEXEC);
	if (fd < 0) e(1);

	/* Now verify through fcntl the flag is indeed set */
	flags = fcntl(fd, F_GETFD);
	if (flags < 0) e(2);
	if (!(flags & FD_CLOEXEC)) e(3);

	/* Fork a child and let child exec a test program that verifies
	 * fd is not a valid file */
	pid = fork();
	if (pid == -1) e(4);	
	else if (pid == 0) {
		/* We're the child */
		char fd_buf[2];

		/* Verify again O_CLOEXEC is on */
		flags = fcntl(fd, F_GETFD);
		if (flags < 0) e(5);
		if (!(flags & FD_CLOEXEC)) e(6);

		snprintf(fd_buf, sizeof(fd_buf), "%d", fd);
		execl("./t67b", "t67b", fd_buf, NULL);

		/* Should not reach this */
		exit(1);
	} else {
		/* We're the parent */
		int result;

		if (waitpid(pid, &result, 0) == -1) e(7);
		if (WEXITSTATUS(result) != 0) e(8);
	}
	close(fd);
}

void
test_open_fork()
{
	int flags;
	pid_t pid;

	/* Let's create a file with O_CLOEXEC NOT turned on */
	fd = open("file", O_RDWR|O_CREAT);
	if (fd < 0) e(1);

	/* Now verify through fcntl the flag is indeed not set */
	flags = fcntl(fd, F_GETFD);
	if (flags < 0) e(2);
	if (flags & FD_CLOEXEC) e(3);

	/* Fork a child and let child exec a test program that verifies
	 * fd is a valid file */
	pid = fork();
	if (pid == -1) e(4);	
	else if (pid == 0) {
		/* We're the child */
		char fd_buf[2];

		/* Verify again O_CLOEXEC is off */
		flags = fcntl(fd, F_GETFD);
		if (flags < 0) e(5);
		if (flags & FD_CLOEXEC) e(6);

		snprintf(fd_buf, sizeof(fd_buf), "%d", fd);
		execl("./t67a", "t67a", fd_buf, NULL);

		/* Should not reach this */
		exit(1);
	} else {
		/* We're the parent */
		int result = 0;

		if (waitpid(pid, &result, 0) == -1) e(7);
		if (WEXITSTATUS(result) != 0) e(8);
	}
	close(fd);
}

int
main(int argc, char *argv[])
{
	start(67);
	copy_subtests();
	test_open_fork();
	test_open_cloexec();
	quit();
	return(-1);	/* Unreachable */
}

