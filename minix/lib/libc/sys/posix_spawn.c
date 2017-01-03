/*
 * Taken from newlib/libc/posix/posix_spawn.c
 */

/*-
 * Copyright (c) 2008 Ed Schouten <ed@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/queue.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <spawn.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

/* Only deal with a pointer to environ, to work around subtle bugs with shared
   libraries and/or small data systems where the user declares his own
   'environ'.  */
static char ***p_environ = &environ;

/*
 * Spawn routines
 */

static int
process_spawnattr(const posix_spawnattr_t * sa)
{
	struct sigaction sigact = { .sa_flags = 0, .sa_handler = SIG_DFL };
	int i;

	/*
	 * POSIX doesn't really describe in which order everything
	 * should be set. We'll just set them in the order in which they
	 * are mentioned.
	 */

	/* Set process group */
	if (sa->sa_flags & POSIX_SPAWN_SETPGROUP) {
		if (setpgid(0, sa->sa_pgroup) != 0)
			return errno;
	}

	/* Set scheduler policy */
	/* XXX: We don't have scheduler policy for now */
#if 0
	if (sa->sa_flags & POSIX_SPAWN_SETSCHEDULER) {
		if (sched_setscheduler(0, sa->sa_schedpolicy,
		    &sa->sa_schedparam) != 0)
			return errno;
	} else if (sa->sa_flags & POSIX_SPAWN_SETSCHEDPARAM) {
		if (sched_setparam(0, &sa->sa_schedparam) != 0)
			return errno;
	}
#endif

	/* Reset user ID's */
	if (sa->sa_flags & POSIX_SPAWN_RESETIDS) {
		if (setegid(getgid()) != 0)
			return errno;
		if (seteuid(getuid()) != 0)
			return errno;
	}

	/* Set signal masks/defaults */
	if (sa->sa_flags & POSIX_SPAWN_SETSIGMASK) {
		sigprocmask(SIG_SETMASK, &sa->sa_sigmask, NULL);
	}

	if (sa->sa_flags & POSIX_SPAWN_SETSIGDEF) {
		for (i = 1; i < NSIG; i++) {
			if (sigismember(&sa->sa_sigdefault, i))
				if (sigaction(i, &sigact, NULL) != 0)
					return errno;
		}
	}

	return 0;
}

static int
move_fd_up(int * statusfd)
{
	/*
	 * Move given file descriptor on a higher fd number.
	 *
	 * This is used to hide the status file descriptor from the application
	 * by pushing it out of the way if it tries to use its number.
	 */
	int newstatusfd;

	newstatusfd = fcntl(*statusfd, F_DUPFD, *statusfd+1);
	if (newstatusfd == -1)
		return -1;

	close(*statusfd);
	*statusfd = newstatusfd;
	return 0;
}

static int
process_file_actions_entry(posix_spawn_file_actions_entry_t * fae,
	int * statusfd)
{
	int fd;

	switch (fae->fae_action) {
	case FAE_OPEN:
		/* Perform an open(), make it use the right fd */
		fd = open(fae->fae_path, fae->fae_oflag, fae->fae_mode);
		if (fd < 0)
			return errno;
		if (fd != fae->fae_fildes) {
			if (fae->fae_fildes == *statusfd) {
				/* Move the status fd out of the way */
				if (move_fd_up(statusfd) == -1)
					return errno;
			}
			if (dup2(fd, fae->fae_fildes) == -1)
				return errno;
			if (close(fd) != 0) {
				if (errno == EBADF)
					return EBADF;
			}
		}
		if (fcntl(fae->fae_fildes, F_SETFD, 0) == -1)
			return errno;
		break;

	case FAE_DUP2:
		if (fae->fae_fildes == *statusfd) {
			/* Nice try */
			return EBADF;
		}
		if (fae->fae_newfildes == *statusfd) {
			/* Move the status file descriptor out of the way */
			if (move_fd_up(statusfd) == -1)
				return errno;
		}
		/* Perform a dup2() */
		if (dup2(fae->fae_fildes, fae->fae_newfildes) == -1)
			return errno;
		if (fcntl(fae->fae_newfildes, F_SETFD, 0) == -1)
			return errno;
		break;

	case FAE_CLOSE:
		/* Perform a close(), do not fail if already closed */
		if (fae->fae_fildes != *statusfd)
			(void)close(fae->fae_fildes);
		break;
	}
	return 0;
}

static int
process_file_actions(const posix_spawn_file_actions_t * fa, int * statusfd)
{
	posix_spawn_file_actions_entry_t *fae;
	int error;

	/* Replay all file descriptor modifications */
	for (unsigned i = 0; i < fa->len; i++) {
		fae = &fa->fae[i];
		error = process_file_actions_entry(fae, statusfd);
		if (error)
			return error;
	}
	return 0;
}

int
posix_spawn(pid_t * __restrict pid, const char * __restrict path,
	const posix_spawn_file_actions_t * fa,
	const posix_spawnattr_t * __restrict sa,
	char * const * __restrict argv, char * const * __restrict envp)
{
	pid_t p;
	int r, error, pfd[2];

	/*
	 * Due to the lack of vfork() in Minix, an alternative solution with
	 * pipes is used. The writing end is set to close on exec() and the
	 * parent performs a read() on it.
	 *
	 * On success, a successful 0-length read happens.
	 * On failure, the child writes the errno to the pipe before exiting,
	 * the error is thus transmitted to the parent.
	 *
	 * This solution was taken from stackoverflow.com question 3703013.
	 */
	if (pipe(pfd) == -1)
		return errno;

	p = fork();
	switch (p) {
	case -1:
		close(pfd[0]);
		close(pfd[1]);

		return errno;

	case 0:
		close(pfd[0]);

		if (fcntl(pfd[1], F_SETFD, FD_CLOEXEC) != 0) {
			error = errno;
			break;
		}

		if (sa != NULL) {
			error = process_spawnattr(sa);
			if (error)
				break;
		}
		if (fa != NULL) {
			error = process_file_actions(fa, &pfd[1]);
			if (error)
				break;
		}

		(void)execve(path, argv, envp != NULL ? envp : *p_environ);

		error = errno;
		break;

	default:
		close(pfd[1]);

		/* Retrieve child process status through pipe. */
		r = read(pfd[0], &error, sizeof(error));
		if (r == 0)
			error = 0;
		else if (r == -1)
			error = errno;
		close(pfd[0]);

		if (error != 0)
			(void)waitpid(p, NULL, 0);

		if (pid != NULL)
			*pid = p;
		return error;
	}

	/* Child failed somewhere, propagate error through pipe and exit. */
	write(pfd[1], &error, sizeof(error));
	close(pfd[1]);
	_exit(127);
}
