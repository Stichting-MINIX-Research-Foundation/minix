/*
 * Based off tests/lib/libc/gen/posix_spawn/t_spawn.c
 */

/* $NetBSD: t_spawn.c,v 1.1 2012/02/13 21:03:08 martin Exp $ */

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles Zhang <charles@NetBSD.org> and
 * Martin Husemann <martin@NetBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>

#include "common.h"

/* reroute stdout to /dev/null while returning another fd for the old stdout */
/* this is just for aesthetics: we don't want to see the output of 'ls' */
static int
sink_stdout(void)
{
	int fd, fd2;

	if ((fd = fcntl(1, F_DUPFD, 3)) == -1 || close(1) == -1) {
		e(0);
		quit();
	}

	if ((fd2 = open("/dev/null", O_WRONLY)) != 1) {
		if (fd2 == -1 || dup2(fd2, 1) != 1) {
			dup2(fd, 1);
			e(0);
			quit();
		}
	}

	return fd;
}

/* restore stdout */
static void
restore_stdout(int fd)
{

	dup2(fd, 1);
	close(fd);
}

/* tests a simple posix_spawn executing /bin/ls */
static void
test_posix_spawn_ls(void)
{
	char * const args[] = { "ls", "-la", NULL };
	int err;

	err = posix_spawn(NULL, "/bin/ls", NULL, NULL, args, NULL);
	if (err != 0)
		e(1);
}

/* tests a simple posix_spawnp executing ls via $PATH */
static void
test_posix_spawnp_ls(void)
{
	char * const args[] = { "ls", "-la", NULL };
	int err;

	err = posix_spawnp(NULL, "ls", NULL, NULL, args, NULL);
	if(err != 0)
		e(2);
}

/* posix_spawn a non existant binary */
static void
test_posix_spawn_missing(void)
{
	char * const args[] = { "t84_h_nonexist", NULL };
	int err;

	err = posix_spawn(NULL, "../t84_h_nonexist", NULL, NULL, args, NULL);
	if (err != ENOENT)
		e(4);
}

/* posix_spawn a script with non existing interpreter */
static void
test_posix_spawn_nonexec(void)
{
	char * const args[] = { "t84_h_nonexec", NULL };
	int err;

	err = posix_spawn(NULL, "../t84_h_nonexec", NULL, NULL, args, NULL);
	if (err != ENOENT)
		e(5);
}

/* posix_spawn a child and get it's return code */
static void
test_posix_spawn_child(void)
{
	char * const args0[] = { "t84_h_spawn", "0", NULL };
	char * const args1[] = { "t84_h_spawn", "1", NULL };
	char * const args7[] = { "t84_h_spawn", "7", NULL };
	int err, status;
	pid_t pid;

	err = posix_spawn(&pid, "../t84_h_spawn", NULL, NULL, args0, NULL);
	if (err != 0 || pid < 1)
		e(1);
	waitpid(pid, &status, 0);
	if (! (WIFEXITED(status) && WEXITSTATUS(status) == 0))
		e(2);

	err = posix_spawn(&pid, "../t84_h_spawn", NULL, NULL, args1, NULL);
	if (err != 0 || pid < 1)
		e(3);
	waitpid(pid, &status, 0);
	if (! (WIFEXITED(status) && WEXITSTATUS(status) == 1))
		e(4);

	err = posix_spawn(&pid, "../t84_h_spawn", NULL, NULL, args7, NULL);
	if (err != 0 || pid < 1)
		e(5);
	waitpid(pid, &status, 0);
	if (! (WIFEXITED(status) && WEXITSTATUS(status) == 7))
		e(6);
}

/* test spawn attributes */
static void
test_posix_spawnattr(void)
{
	int pid, status, err, pfd[2];
	char helper_arg[128];
	char * const args[] = { "t84_h_spawnattr", helper_arg, NULL };
	sigset_t sig;
	posix_spawnattr_t attr;

	/*
	 * create a pipe to controll the child
	 */
	err = pipe(pfd);
	if (err != 0)
		e(1);
	sprintf(helper_arg, "%d", pfd[0]);

	posix_spawnattr_init(&attr);

	sigemptyset(&sig);
	sigaddset(&sig, SIGUSR1);

	posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSCHEDULER |
		POSIX_SPAWN_SETSCHEDPARAM | POSIX_SPAWN_SETPGROUP |
		POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF |
		POSIX_SPAWN_SETSIGDEF);
	posix_spawnattr_setpgroup(&attr, 0);
#if 0
	posix_spawnattr_setschedparam(&attr, &sp);
	posix_spawnattr_setschedpolicy(&attr, scheduler);
#endif
	posix_spawnattr_setsigmask(&attr, &sig);
	posix_spawnattr_setsigdefault(&attr, &sig);

	err = posix_spawn(&pid, "../t84_h_spawnattr", NULL, &attr, args, NULL);
	if (err != 0)
		e(2);

	/* ready, let child go */
	write(pfd[1], "q", 1);
	close(pfd[0]);
	close(pfd[1]);

	/* wait and check result from child */
	waitpid(pid, &status, 0);
	if (! (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS))
		e(3);

	posix_spawnattr_destroy(&attr);
}

/* tests a simple posix_spawn executing /bin/ls with file actions */
static void
test_posix_spawn_file_actions(void)
{
	char * const args[] = { "ls", "-la", NULL };
	int err;
	posix_spawn_file_actions_t file_actions;

	/*
	 * Just do a bunch of random operations which should leave console
	 * output intact.
	 */
	posix_spawn_file_actions_init(&file_actions);
	posix_spawn_file_actions_adddup2(&file_actions, 1, 3);
	posix_spawn_file_actions_adddup2(&file_actions, 1, 4);
	posix_spawn_file_actions_adddup2(&file_actions, 1, 6);
	posix_spawn_file_actions_adddup2(&file_actions, 1, 5);
	posix_spawn_file_actions_addclose(&file_actions, 3);
	posix_spawn_file_actions_addclose(&file_actions, 4);
	posix_spawn_file_actions_addclose(&file_actions, 6);
	posix_spawn_file_actions_addclose(&file_actions, 5);

	posix_spawn_file_actions_addclose(&file_actions, 0);
	posix_spawn_file_actions_addclose(&file_actions, 2);
	posix_spawn_file_actions_addopen(&file_actions, 0, "/dev/null",
	    O_RDONLY, 0);
	posix_spawn_file_actions_adddup2(&file_actions, 1, 2);
	posix_spawn_file_actions_addclose(&file_actions, 1);
	posix_spawn_file_actions_adddup2(&file_actions, 2, 1);

	err = posix_spawn(NULL, "/bin/ls", &file_actions, NULL, args, NULL);
	posix_spawn_file_actions_destroy(&file_actions);

	if (err != 0)
		e(1);
}

/* tests failures with file actions */
static void
test_posix_spawn_file_actions_failures(void)
{
	char * const args[] = { "ls", "-la", NULL };
	int err, i;
	posix_spawn_file_actions_t file_actions;

	/* Test bogus open */
	posix_spawn_file_actions_init(&file_actions);
	posix_spawn_file_actions_addclose(&file_actions, 0);
	posix_spawn_file_actions_addopen(&file_actions, 0, "t84_h_nonexist",
	    O_RDONLY, 0);

	err = posix_spawn(NULL, "/bin/ls", &file_actions, NULL, args, NULL);
	posix_spawn_file_actions_destroy(&file_actions);

	if (err == 0)
		e(1);

	/* Test bogus dup2 */
	for (i = 3; i < 10; i++) {
		posix_spawn_file_actions_init(&file_actions);
		posix_spawn_file_actions_adddup2(&file_actions, i, i+1);

		err = posix_spawn(NULL, "/bin/ls", &file_actions, NULL, args,
		    NULL);
		posix_spawn_file_actions_destroy(&file_actions);

		if (err == 0)
			e(i-2);
	}

	/*
	 * Test bogus exec with dup2 (to mess with the pipe error reporting in
	 * posix_spawn.c)
	 */
	posix_spawn_file_actions_init(&file_actions);
	posix_spawn_file_actions_adddup2(&file_actions, 1, 3);
	posix_spawn_file_actions_adddup2(&file_actions, 1, 4);
	posix_spawn_file_actions_adddup2(&file_actions, 1, 6);
	posix_spawn_file_actions_adddup2(&file_actions, 1, 5);
	posix_spawn_file_actions_adddup2(&file_actions, 1, 7);

	err = posix_spawn(NULL, "t84_h_nonexist", &file_actions, NULL, args,
	    NULL);
	posix_spawn_file_actions_destroy(&file_actions);

	if (err == 0)
		e(9);
}

int
main(void)
{
	int fd;

	start(84);

	subtest = 1;
	fd = sink_stdout();
	test_posix_spawn_ls();
	test_posix_spawnp_ls();
	restore_stdout(fd);

	test_posix_spawn_missing();
	test_posix_spawn_nonexec();

	subtest = 2;
	test_posix_spawn_child();

	subtest = 3;
	test_posix_spawnattr();
	subtest = 4;
	fd = sink_stdout();
	test_posix_spawn_file_actions();
	restore_stdout(fd);
	subtest = 5;
	test_posix_spawn_file_actions_failures();

	/* TODO: Write/port more tests */

	quit();

	/* Not reached */
	return -1;
}
