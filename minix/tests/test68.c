#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syslimits.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int max_error = 5;
#include "common.h"


void copy_subtests(void);
void test_pipe_cloexec(void);
void test_pipe_flag_setting(void);
void test_pipe_nonblock(void);
void test_pipe_normal(void);
void test_pipe_nosigpipe(void);
void alarm_handler(int sig);
void pipe_handler(int sig);

static int seen_pipe_signal = 0;
static int seen_alarm_signal = 0;

void
alarm_handler(int sig)
{
	if (seen_pipe_signal == 0)
		seen_pipe_signal = -1;
	seen_alarm_signal = 1;
}

void
pipe_handler(int sig)
{
	seen_pipe_signal = 1;
}

void
copy_subtests()
{
	char *subtests[] = { "t68a", "t68b" };
	char copy_cmd[8 + PATH_MAX + 1];
	int i, no_tests;

	no_tests = sizeof(subtests) / sizeof(char *);

	for (i = 0; i < no_tests; i++) {
		snprintf(copy_cmd, 8 + PATH_MAX, "cp ../%s .", subtests[i]);
		system(copy_cmd);
	}
}

void
test_pipe_normal()
{
/* Verify pipe2 creates pipes that behave like a normal pipe */

	int pipes[2];
	char buf_in[1], buf_out[1];
	pid_t pid;

	subtest = 2;

	if (pipe2(pipes, 0) != 0) e(1);

	buf_out[0] = 'T';
	if (write(pipes[1], buf_out, sizeof(buf_out)) != sizeof(buf_out)) e(2);
	if (read(pipes[0], buf_in, sizeof(buf_in)) != sizeof(buf_in)) e(3);
	if (buf_out[0] != buf_in[0]) e(4);

	/* When we close the write end, reading should fail */
	if (close(pipes[1]) != 0) e(5);
	if (read(pipes[0], buf_in, sizeof(buf_in)) != 0) e(6);

	/* Let's retry that experiment the other way around. Install a signal
	 * handler to catch SIGPIPE. Install an alarm handler to make sure
	 * this test finishes in finite time. */
	if (pipe2(pipes, 0) != 0) e(7);
	signal(SIGPIPE, pipe_handler);
	signal(SIGALRM, alarm_handler);
	seen_pipe_signal = 0;
	seen_alarm_signal = 0;
	alarm(1);
	if (close(pipes[0]) != 0) e(8);
	if (write(pipes[1], buf_out, sizeof(buf_out)) != -1) e(9);
	while (seen_pipe_signal == 0)
		;
	if (seen_pipe_signal != 1) e(10);
	if (close(pipes[1]) != 0) e(11);

	/* Collect alarm signal */
	while (seen_alarm_signal == 0)
		;

	if (pipe2(pipes, 0) != 0) e(12);

	/* Now fork and verify we can write to the pipe */
	pid = fork();
	if (pid < 0) e(13);
	if (pid == 0) {
		/* We're the child */
		char fd_buf[2];

		/* Verify we can still write a byte into the pipe */
		if (write(pipes[1], buf_out, sizeof(buf_out)) != 1) e(14);
		
		snprintf(fd_buf, sizeof(fd_buf), "%d", pipes[1]);
		execl("./t68a", "t68a", fd_buf, NULL);

		exit(1); /* Should not be reached */
	} else {
		/* We're the parent */
		int result;

		if (waitpid(pid, &result, 0) == -1) e(15);
		if (WEXITSTATUS(result) != 0) e(16);
	}

	if (close(pipes[0]) != 0) e(17);
	if (close(pipes[1]) != 0) e(18);
}

void
test_pipe_cloexec()
{
/* Open a pipe with O_CLOEXEC */
	int flags;
	int pipes[2];
	pid_t pid;
	char buf_in[1], buf_out[1];

	subtest = 3;

	if (pipe2(pipes, O_CLOEXEC) != 0) e(1);

	/* Verify O_CLOEXEC flag is set */
	flags = fcntl(pipes[0], F_GETFD);
	if (flags < 0) e(2);
	if (!(flags & FD_CLOEXEC)) e(3);

	pid = fork();
	if (pid < 0) e(4);
	if (pid == 0) {
		/* We're the child */
		char fd_buf[2];

		/* Verify we can still write a byte into the pipe */
		buf_in[0] = 0;
		buf_out[0] = 'T';
		if (write(pipes[1], buf_out, sizeof(buf_out)) != 1) e(5);
		if (read(pipes[0], buf_in, sizeof(buf_in)) != 1) e(6);
		if (buf_out[0] != buf_in[0]) e(7);
		
		/* Verify FD_CLOEXEC flag is still set */
		flags = fcntl(pipes[0], F_GETFD);
		if (flags < 0) e(8);
		if (!(flags & FD_CLOEXEC)) e(9);
		
		snprintf(fd_buf, sizeof(fd_buf), "%d", pipes[0]);
		execl("./t68b", "t68b", fd_buf, NULL);

		exit(1); /* Should not be reached */
	} else {
		/* We're the parent */
		int result;

		if (waitpid(pid, &result, 0) == -1) e(10);
		if (WEXITSTATUS(result) != 0) e(11);
	}

	/* Eventhough our child's pipe should've been closed upon exec, our
	 * pipe should still be functioning.
	 */
	buf_in[0] = 0;
	buf_out[0] = 't';
	if (write(pipes[1], buf_out, sizeof(buf_out)) != sizeof(buf_out)) e(12);
	if (read(pipes[0], buf_in, sizeof(buf_in)) != sizeof(buf_in)) e(13);
	if (buf_out[0] != buf_in[0]) e(14);

	if (close(pipes[0]) != 0) e(15);
	if (close(pipes[1]) != 0) e(16);
}

void
test_pipe_nonblock()
{
/* Open a pipe with O_NONBLOCK */
	char *buf_in, *buf_out;
	int pipes[2];
	size_t pipe_size;

	subtest = 4;

	if (pipe2(pipes, O_NONBLOCK) != 0) e(1);
	if ((pipe_size = fpathconf(pipes[0], _PC_PIPE_BUF)) == -1) e(2);
	buf_in = calloc(2, pipe_size); /* Allocate twice the buffer size */
	if (buf_in == NULL) e(3);
	buf_out = calloc(2, pipe_size); /* Idem dito for output buffer */
	if (buf_out == NULL) e(4);

	/* According to POSIX, a pipe with O_NONBLOCK set shall never block.
	 * When we attempt to write PIPE_BUF or less bytes, and there is
	 * sufficient space available, write returns nbytes. Else write will
	 * return -1 and not transfer any data.
	 */
	if (write(pipes[1], buf_out, 1) != 1) e(5);	/* Write 1 byte */
	if (write(pipes[1], buf_out, pipe_size) != -1) e(6);	/* Can't fit */
	if (errno != EAGAIN) e(7);

	/* When writing more than PIPE_BUF bytes and when at least 1 byte can
	 * be tranferred, return the number of bytes written. We've written 1
	 * byte, so there are PIPE_BUF - 1 bytes left. */
	if (write(pipes[1], buf_out, pipe_size + 1) != pipe_size - 1) e(8);

	/* Read out all data and try again. This time we should be able to
	 * write PIPE_BUF bytes. */
	if (read(pipes[0], buf_in, pipe_size) != pipe_size) e(9);
	if (read(pipes[0], buf_in, 1) != -1) e(10);	/* Empty, can't read */
	if (errno != EAGAIN) e(11);
	if (write(pipes[1], buf_out, pipe_size + 1) != pipe_size) e(12);
	if (close(pipes[0]) != 0) e(13);
	if (close(pipes[1]) != 0) e(14);
	free(buf_in);
	free(buf_out);
}

void
test_pipe_nosigpipe(void)
{
/* Let's retry the writing to pipe without readers experiment. This time we set
 * the O_NOSIGPIPE flag to prevent getting a signal. */
	int pipes[2];
	char buf_out[1];

	subtest = 5;

	if (pipe2(pipes, O_NOSIGPIPE) != 0) e(7);
	signal(SIGPIPE, pipe_handler);
	signal(SIGALRM, alarm_handler);
	seen_pipe_signal = 0;
	seen_alarm_signal = 0;
	alarm(1);
	if (close(pipes[0]) != 0) e(8);
	if (write(pipes[1], buf_out, sizeof(buf_out)) != -1) e(9);

	/* Collect alarm signal */
	while (seen_alarm_signal == 0)
		;
	if (errno != EPIPE) e(10);
	if (seen_pipe_signal != -1) e(11); /* Alarm sig handler set it to -1 */
	if (close(pipes[1]) != 0) e(12);
}

void
test_pipe_flag_setting()
{
	int pipes[2];

	subtest = 1;

	/* Create standard pipe with no flags and verify they're off */
	if (pipe2(pipes, 0) != 0) e(1);
	if (fcntl(pipes[0], F_GETFD) != 0) e(2);
	if (fcntl(pipes[1], F_GETFD) != 0) e(3);
	if (fcntl(pipes[0], F_GETFL) & O_NONBLOCK) e(4);
	if (fcntl(pipes[1], F_GETFL) & O_NONBLOCK) e(5);
	if (fcntl(pipes[0], F_GETNOSIGPIPE) != 0) e(6);
	if (fcntl(pipes[1], F_GETNOSIGPIPE) != 0) e(7);
	if (close(pipes[0]) != 0) e(8);
	if (close(pipes[1]) != 0) e(9);

	/* Create pipe with all flags and verify they're on */
	if (pipe2(pipes, O_CLOEXEC|O_NONBLOCK|O_NOSIGPIPE) != 0) e(10);
	if (fcntl(pipes[0], F_GETFD) != FD_CLOEXEC) e(11);
	if (fcntl(pipes[1], F_GETFD) != FD_CLOEXEC) e(12);
	if (!(fcntl(pipes[0], F_GETFL) & O_NONBLOCK)) e(13);
	if (!(fcntl(pipes[1], F_GETFL) & O_NONBLOCK)) e(14);
	if (fcntl(pipes[0], F_GETNOSIGPIPE) == 0) e(15);
	if (fcntl(pipes[1], F_GETNOSIGPIPE) == 0) e(16);
	if (fcntl(pipes[0], F_SETNOSIGPIPE, 0) != 0) e(17);
	if (fcntl(pipes[0], F_GETNOSIGPIPE) != 0) e(18);
	if (fcntl(pipes[0], F_SETNOSIGPIPE, 1) != 0) e(19);
	if (fcntl(pipes[0], F_GETNOSIGPIPE) == 0) e(20);
	if (close(pipes[0]) != 0) e(21);
	if (close(pipes[1]) != 0) e(22);
}

/*
 * Test the behavior of a large pipe write that achieves partial progress
 * before the reader end is closed.  The write call is expected to return EPIPE
 * and generate a SIGPIPE signal, and otherwise leave the system in good order.
 */
static void
test_pipe_partial_write(void)
{
	char buf[PIPE_BUF + 2];
	int pfd[2], status;

	signal(SIGPIPE, pipe_handler);

	if (pipe(pfd) < 0) e(1);

	switch (fork()) {
	case 0:
		close(pfd[1]);

		sleep(1); /* let the parent block on the write(2) */

		/*
		 * This one-byte read raises the question whether the write
		 * should return partial progress or not, since consumption of
		 * part of its data is now clearly visible.  NetBSD chooses
		 * *not* to return partial progress, and MINIX3 follows suit.
		 */
		if (read(pfd[0], buf, 1) != 1) e(2);

		sleep(1); /* let VFS retry satisfying the write(2) */

		exit(errct); /* close the reader side of the pipe */

	case -1:
		e(3);

	default:
		break;
	}

	close(pfd[0]);

	seen_pipe_signal = 0;

	/*
	 * The following large write should block until the child exits, as
	 * that is when the read end closes, thus making eventual completion of
	 * the write impossible.
	 */
	if (write(pfd[1], buf, sizeof(buf)) != -1) e(4);
	if (errno != EPIPE) e(5);
	if (seen_pipe_signal != 1) e(6);

	seen_pipe_signal = 0;

	/* A subsequent write used to cause a system crash. */
	if (write(pfd[1], buf, 1) != -1) e(7);
	if (errno != EPIPE) e(8);
	if (seen_pipe_signal != 1) e(9);

	/* Clean up. */
	close(pfd[1]);

	if (wait(&status) <= 0) e(10);
	if (!WIFEXITED(status)) e(11);
	errct += WEXITSTATUS(status);
}

int
main(int argc, char *argv[])
{
	start(68);
	copy_subtests();
	test_pipe_flag_setting();
	test_pipe_normal();
	test_pipe_cloexec();
	test_pipe_nonblock();
	test_pipe_nosigpipe();
	test_pipe_partial_write();
	quit();
	return(-1);	/* Unreachable */
}

