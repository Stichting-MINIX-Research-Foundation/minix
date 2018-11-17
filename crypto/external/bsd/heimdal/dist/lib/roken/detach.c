/*	$NetBSD: detach.c,v 1.2 2017/01/28 21:31:50 christos Exp $	*/

/*-
 * Copyright (c) 2015
 *	Cryptonector LLC.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Cryptonector LLC may not be used to endorse or promote products
 *    derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>
#include <errno.h>
#include <fcntl.h>
#ifdef WIN32
#include <io.h>
#include <stdlib.h>
#else
#include <unistd.h>
#endif
#include <krb5/roken.h>

#ifdef WIN32
#define dup2 _dup2
#endif

static int pipefds[2] = {-1, -1};

ROKEN_LIB_FUNCTION void ROKEN_LIB_CALL
roken_detach_prep(int argc, char **argv, char *special_arg)
{
    pid_t child;
    char buf[1];
    ssize_t bytes;
    int status;

    pipefds[0] = -1;
    pipefds[1] = -1;

#ifdef WIN32
    if (_pipe(pipefds, 4, O_BINARY) == -1)
        err(1, "failed to setup to detach daemon (_pipe failed)");
#else
    if (pipe(pipefds) == -1)
        err(1, "failed to setup to detach daemon (pipe failed)");
#endif

#ifndef WIN32
    fflush(stdout);
    child = fork();
#else
    {
        intptr_t child_handle;
	int write_side;
        size_t i;
	char *fildes;
        char **new_argv;

        new_argv = calloc(argc + 2, sizeof(*new_argv));
        if (new_argv == NULL)
            err(1, "Out of memory");

	write_side = _dup(pipefds[1]); /* The new fd will be inherited */
	if (write_side == -1)
            err(1, "Out of memory");

	if (asprintf(&fildes, "%d", write_side) == -1 ||
	    fildes == NULL)
            err(1, "failed to setup to detach daemon (_dup failed)");

        new_argv[0] = argv[0];
        new_argv[1] = special_arg;
        new_argv[2] = fildes;
        for (i = 1; argv[i] != NULL; i++)
            new_argv[i + 1] = argv[i];
	new_argv[argc + 2] = NULL;

	_flushall();
	child_handle = spawnvp(_P_NOWAIT, argv[0], new_argv);
	if (child_handle == -1)
	  child = (pid_t)-1;
	else
	  child = GetProcessId((HANDLE)child_handle);
    }
#endif
    if (child == (pid_t)-1)
        err(1, "failed to setup to fork daemon (fork failed)");

#ifndef WIN32
    if (child == 0) {
        int fd;

        (void) close(pipefds[0]);
        pipefds[0] = -1;
        /*
         * Keep stdout/stderr for now so output and errors prior to
         * detach_finish() can be seen by the user.
         */
        fd = open(_PATH_DEVNULL, O_RDWR, 0);
        if (fd == -1)
            err(1, "failed to open /dev/null");
        (void) dup2(fd, STDIN_FILENO);
        if (fd > STDERR_FILENO)
            (void) close(fd);
        return;
    }
#endif

    (void) close(pipefds[1]);
    pipefds[1] = -1;
    do {
        bytes = read(pipefds[0], buf, sizeof(buf));
    } while (bytes == -1 && errno == EINTR);
    (void) close(pipefds[0]);
    pipefds[0] = -1;
    if (bytes == -1) {
        /*
         * No need to wait for the process.  We've killed it.  If it
         * doesn't want to exit, we'd have to wait potentially forever,
         * but we want to indicate failure to the user as soon as
         * possible.  A wait with timeout would end the same way
         * (attempting to kill the process).
         */
        err(1, "failed to setup daemon child (read from child pipe)");
    }
    if (bytes == 0) {
        warnx("daemon child preparation failed, waiting for child");
        status = wait_for_process(child);
        if (SE_IS_ERROR(status) || SE_PROCSTATUS(status) != 0)
            errx(SE_PROCSTATUS(status),
                 "daemon child preparation failed (child exited)");
    }
    _exit(0);
}

#ifdef WIN32
#ifdef dup2
#undef dup2
#endif
#define dup2 _dup2
#endif

ROKEN_LIB_FUNCTION void ROKEN_LIB_CALL
roken_detach_finish(const char *dir, int daemon_child_fd)
{
    char buf[1] = "";
    ssize_t bytes;
    int fd;

    rk_pidfile(NULL);
    if (pipefds[1] == -1 && daemon_child_fd != -1)
        pipefds[1] = daemon_child_fd;
    if (pipefds[0] != -1)
	(void) close(pipefds[0]);
    if (pipefds[1] == -1)
        return;

#ifdef HAVE_SETSID
    if (setsid() == -1)
        err(1, "failed to detach from tty");
#endif

#ifndef WIN32
    /*
     * Hopefully we've written any pidfiles by now, if they had to be in
     * the current directory...
     *
     * The daemons do re-open logs and so on, therefore this chdir()
     * call needs to be optional for testing.
     */
    if (dir != NULL && chdir(dir) == -1)
        err(1, "failed to chdir to /");
#endif

    do {
        bytes = write(pipefds[1], buf, sizeof(buf));
    } while (bytes == -1 && errno == EINTR);
    if (bytes == -1)
        err(1, "failed to signal parent while detaching");
    (void) close(pipefds[1]);
    if (bytes != sizeof(buf))
        errx(1, "failed to signal parent while detaching");

    fd = open(_PATH_DEVNULL, O_RDWR, 0);
    if (fd == -1)
        err(1, "failed to open /dev/null");
    /*
     * Maybe we should check that our output got written, if redirected
     * to a file.  File utils normally do this.
     */
    (void) dup2(fd, STDOUT_FILENO);
    (void) dup2(fd, STDERR_FILENO);
    if (fd > 2)
        (void) close(fd);
}
