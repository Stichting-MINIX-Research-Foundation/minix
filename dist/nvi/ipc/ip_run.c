/*	$NetBSD: ip_run.c,v 1.1.1.2 2008/05/18 14:31:25 aymeric Exp $ */

/*-
 * Copyright (c) 1996
 *	Rob Zimmermann.  All rights reserved.
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: ip_run.c,v 8.17 2000/07/04 21:48:54 skimo Exp (Berkeley) Date: 2000/07/04 21:48:54";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>

#include "../common/common.h"
#include "ip.h"
#include "pathnames.h"

static void arg_format __P((char *, int *, char **[], int, int));
static void fatal __P((void));
#ifdef DEBUG
static void attach __P((void));
#endif
static int channel(int rpipe[2], int wpipe[2]);

char	*vi_progname = "vi";			/* Global: program name. */

/*
 * vi_run --
 *	Run the vi program.
 *
 * PUBLIC: int vi_run __P((IPVI *, int, char *[]));
 */
int
vi_run(ipvi, argc, argv)
	IPVI *ipvi;
	int argc;
	char *argv[];
{
	struct stat sb;
	int pflag, rpipe[2], wpipe[2];
	char *execp, **p_av, **t_av;

	pflag = 0;
	execp = VI;

	/* Strip out any arguments that vi isn't going to understand. */
	for (p_av = t_av = argv;;) {
		if (*t_av == NULL) {
			*p_av = NULL;
			break;
		}
		if (!strcmp(*t_av, "--")) {
			while ((*p_av++ = *++t_av) != NULL);
			break;
		}
#ifdef DEBUG
		if (!memcmp(*t_av, "-D", sizeof("-D") - 1)) {
			attach();

			++t_av;
			--argc;
			continue;
		}
#endif
#ifdef TRACE
		if (!memcmp(*t_av, "-T", sizeof("-T") - 1)) {
			char *p = &t_av[0][sizeof("-T") - 1];
			if (*p == '\0') {
				--argc;
				p = *++t_av;
			}
			vtrace_init(p);
			++t_av;
			--argc;
			continue;
		}
#endif
		if (!memcmp(*t_av, "-P", sizeof("-P") - 1)) {
			if (t_av[0][2] != '\0') {
				pflag = 1;
				execp = t_av[0] + 2;
				++t_av;
				--argc;
				continue;
			}
			if (t_av[1] != NULL) {
				pflag = 1;
				execp = t_av[1];
				t_av += 2;
				argc -= 2;
				continue;
			}
		}
		*p_av++ = *t_av++;
	}

	/*
	 * Open the communications channels.  The pipes are named from the
	 * parent's viewpoint, meaning the screen reads from rpipe[0] and
	 * writes to wpipe[1].  The vi process reads from wpipe[0], and it
	 * writes to rpipe[1].
	 */
	if (channel(rpipe, wpipe) == -1)
		fatal();
	ipvi->ifd = rpipe[0];
	ipvi->ofd = wpipe[1];

	/*
	 * Reformat our arguments, adding a -I to the list.  The first file
	 * descriptor for the -I argument is vi's input, and the second is
	 * vi's output.
	 */
	arg_format(execp, &argc, &argv, wpipe[0], rpipe[1]);

	/* Run vi. */
	switch (ipvi->pid = fork()) {
	case -1:				/* Error. */
		fatal();
		/* NOTREACHED */
	case 0:					/* Child: Vi. */
		(void)close(rpipe[0]);
		(void)close(wpipe[1]);

		/*
		 * If the user didn't override the path and there's a local
		 * (debugging) nvi, run it, otherwise run the user's path,
		 * if specified, else run the compiled in path.
		 */
		if (!pflag && stat("vi-ipc", &sb) == 0)
			execv("vi-ipc", argv);
		execv(execp, argv);
		(void)fprintf(stderr,
		    "%s: %s %s\n", vi_progname, execp, strerror(errno));
		(void)fprintf(stderr,
#ifdef DEBUG
	    "usage: %s [-D] [-P vi_program] [-T trace] [vi arguments]\n",
#else
	    "usage: %s [-P vi_program] [vi arguments]\n",
#endif
		    vi_progname);
		_exit (1);
	default:				/* Parent: Screen. */
		(void)close(rpipe[1]);
		(void)close(wpipe[0]);
		break;
	}
	return (0);
}

/*
 * fatal --
 *	Fatal error.
 */
static void
fatal()
{
	(void)fprintf(stderr, "%s: %s\n", vi_progname, strerror(errno));
	exit (1);
}

static 
int channel(int rpipe[2], int wpipe[2])
{
	if (0) {

	if (pipe(rpipe) == -1 || pipe(wpipe) == -1)
		return -1;

	} else {

	int sockets[2];

	if (socketpair(AF_LOCAL, SOCK_STREAM, 0, sockets) == -1)
		return -1;

	rpipe[0] = sockets[0];
	wpipe[0] = sockets[1];
	if (((rpipe[1] = dup(sockets[1])) == -1) ||
	    ((wpipe[1] = dup(sockets[0])) == -1))
		return -1;

	}
}

/*
 * arg_format --
 *	Reformat our arguments to add the -I argument for vi.
 */
static void
arg_format(execp, argcp, argvp, i_fd, o_fd)
	char *execp, **argvp[];
	int *argcp, i_fd, o_fd;
{
	char *iarg, **largv, *p, **p_av, **t_av;

	/* Get space for the argument array and the -I argument. */
	if ((iarg = malloc(64)) == NULL ||
	    (largv = malloc((*argcp + 3) * sizeof(char *))) == NULL)
		fatal();
	memcpy(largv + 2, *argvp, *argcp * sizeof(char *) + 1);

	/* Reset argv[0] to be the exec'd program. */
	if ((p = strrchr(execp, '/')) == NULL)
		largv[0] = execp;
	else
		largv[0] = p + 1;

	/* Create the -I argument. */
	(void)sprintf(iarg, "-I%d%s%d", i_fd, ".", o_fd);
	largv[1] = iarg;

	/* Copy any remaining arguments into the array. */
	for (p_av = (*argvp) + 1, t_av = largv + 2;;)
		if ((*t_av++ = *p_av++) == NULL)
			break;

	/* Reset the argument array. */
	*argvp = largv;
}

#ifdef DEBUG
/*
 * attach --
 *	Pause and let the user attach a debugger.
 */
static void
attach()
{
	int fd;
	char ch;

	(void)printf("process %lu waiting, enter <CR> to continue: ",
	    (u_long)getpid());
	(void)fflush(stdout);

	if ((fd = open(_PATH_TTY, O_RDONLY, 0)) < 0) {
		(void)fprintf(stderr,
		    "%s: %s, %s\n", vi_progname, _PATH_TTY, strerror(errno));
		exit (1);;
	}
	do {
		if (read(fd, &ch, 1) != 1) {
			(void)close(fd);
			return;
		}
	} while (ch != '\n' && ch != '\r');
	(void)close(fd);
}
#endif
