/*	$NetBSD: ipropd_common.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "iprop.h"

#if defined(HAVE_FORK) && defined(HAVE_WAITPID)
#include <sys/types.h>
#include <sys/wait.h>
#endif

sig_atomic_t exit_flag;

static RETSIGTYPE
sigterm(int sig)
{
    exit_flag = sig;
}

void
setup_signal(void)
{
#ifdef HAVE_SIGACTION
    {
	struct sigaction sa;

	sa.sa_flags = 0;
	sa.sa_handler = sigterm;
	sigemptyset(&sa.sa_mask);

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGXCPU, &sa, NULL);

	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, NULL);
    }
#else
    signal(SIGINT, sigterm);
    signal(SIGTERM, sigterm);
#ifndef NO_SIGXCPU
    signal(SIGXCPU, sigterm);
#endif
#ifndef NO_SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif
#endif
}

/*
 * Fork a child to run the service, and restart it if it dies.
 *
 * Returns -1 if not supported, else a file descriptor that the service
 * should select() for.  Any events on that file descriptor should cause
 * the caller to exit immediately, as that means that the restarter
 * exited.
 *
 * The service's normal exit status values should be should be taken
 * from enum ipropd_exit_code.  IPROPD_FATAL causes the restarter to
 * stop restarting the service and to exit.
 *
 * A count of restarts is output via the `countp' argument, if it is
 * non-NULL.  This is useful for testing this function (e.g., kill the
 * restarter after N restarts and check that the child gets the signal
 * sent to it).
 *
 * This requires fork() and waitpid() (otherwise returns -1).  Ignoring
 * SIGCHLD, of course, would be bad.
 *
 * We could support this on Windows by spawning a child with mostly the
 * same arguments as the restarter process.
 */
int
restarter(krb5_context context, size_t *countp)
{
#if defined(HAVE_FORK) && defined(HAVE_WAITPID)
    struct timeval tmout;
    pid_t pid = -1;
    pid_t wpid = -1;
    int status;
    int fds[2];
    int fds2[2];
    size_t count = 0;
    fd_set readset;

    fds[0] = -1;
    fds[1] = -1;
    fds2[0] = -1;
    fds2[1] = -1;

    signal(SIGCHLD, SIG_DFL);

    while (!exit_flag) {
        /* Close the pipe ends we keep open */
        if (fds[1] != -1)
            (void) close(fds[1]);
        if (fds2[0] != -1)
            (void) close(fds2[1]);

        /* A pipe so the child can detect the parent's death */
        if (pipe(fds) == -1) {
            krb5_err(context, 1, errno,
                     "Could not setup pipes in service restarter");
        }

        /* A pipe so the parent can detect the child's death */
        if (pipe(fds2) == -1) {
            krb5_err(context, 1, errno,
                     "Could not setup pipes in service restarter");
        }

        fflush(stdout);
        fflush(stderr);

        pid = fork();
        if (pid == -1)
            krb5_err(context, 1, errno, "Could not fork in service restarter");
        if (pid == 0) {
            if (countp != NULL)
                *countp = count;
            (void) close(fds[1]);
            (void) close(fds2[0]);
            return fds[0];
        }

        count++;

        (void) close(fds[0]);
        (void) close(fds2[1]);

        do {
            wpid = waitpid(pid, &status, 0);
        } while (wpid == -1 && errno == EINTR && !exit_flag);
        if (wpid == -1 && errno == EINTR)
            break; /* We were signaled; gotta kill the child and exit */
        if (wpid == -1) {
            if (errno != ECHILD) {
                warn("waitpid() failed; killing restarter's child process");
                kill(pid, SIGTERM);
            }
            krb5_err(context, 1, errno, "restarter failed waiting for child");
        }

        assert(wpid == pid);
        wpid = -1;
        pid = -1;
        if (WIFEXITED(status)) {
            switch (WEXITSTATUS(status)) {
            case IPROPD_DONE:
                exit(0);
            case IPROPD_RESTART_SLOW:
                if (exit_flag)
                    exit(1);
                krb5_warnx(context, "Waiting 2 minutes to restart");
                sleep(120);
                continue;
            case IPROPD_FATAL:
                krb5_errx(context, WEXITSTATUS(status),
                         "Sockets and pipes not supported for "
                         "iprop log files");
            case IPROPD_RESTART:
            default:
                if (exit_flag)
                    exit(1);
                /* Add exponential backoff (with max backoff)? */
                krb5_warnx(context, "Waiting 30 seconds to restart");
                sleep(30);
                continue;
            }
        }
        /* else */
        krb5_warnx(context, "Child was killed; waiting 30 seconds to restart");
        sleep(30);
    }

    if (pid == -1)
        exit(0); /* No dead child to reap; done */

    assert(pid > 0);
    if (wpid != pid) {
        warnx("Interrupted; killing child (pid %ld) with %d",
              (long)pid, exit_flag);
        krb5_warnx(context, "Interrupted; killing child (pid %ld) with %d",
                   (long)pid, exit_flag);
        kill(pid, exit_flag);

        /* Wait up to one second for the child */
        tmout.tv_sec = 1;
        tmout.tv_usec = 0;
        FD_ZERO(&readset);
        FD_SET(fds2[0], &readset);
        /* We don't care why select() returns */
        (void) select(fds2[0] + 1, &readset, NULL, NULL, &tmout);
        /*
         * We haven't reaped the child yet; if it's a zombie, then
         * SIGKILLing it won't hurt.  If it's not a zombie yet, well,
         * we're out of patience.
         */
        kill(pid, SIGKILL);
        do {
            wpid = waitpid(pid, &status, 0);
        } while (wpid != pid && errno == EINTR);
        if (wpid == -1)
            krb5_err(context, 1, errno, "restarter failed waiting for child");
    }

    /* Finally, the child is dead and reaped */
    if (WIFEXITED(status))
        exit(WEXITSTATUS(status));
    if (WIFSIGNALED(status)) {
        switch (WTERMSIG(status)) {
        case SIGTERM:
        case SIGXCPU:
        case SIGINT:
            exit(0);
        default:
            /*
             * Attempt to set the same exit status for the parent as for
             * the child.
             */
            kill(getpid(), WTERMSIG(status));
            /*
             * We can get past the self-kill if we inherited a SIG_IGN
             * disposition that the child reset to SIG_DFL.
             */
        }
    }
    exit(1);
#else
    if (countp != NULL)
        *countp = 0;
    errno = ENOTSUP;
    return -1;
#endif
}

