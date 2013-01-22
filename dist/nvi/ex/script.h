/*	$NetBSD: script.h,v 1.1.1.2 2008/05/18 14:31:21 aymeric Exp $ */

/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	Id: script.h,v 10.2 1996/03/06 19:53:00 bostic Exp (Berkeley) Date: 1996/03/06 19:53:00
 */

struct _script {
	pid_t	 sh_pid;		/* Shell pid. */
	int	 sh_master;		/* Master pty fd. */
	int	 sh_slave;		/* Slave pty fd. */
	char	*sh_prompt;		/* Prompt. */
	size_t	 sh_prompt_len;		/* Prompt length. */
	char	 sh_name[64];		/* Pty name */
#ifdef TIOCGWINSZ
	struct winsize sh_win;		/* Window size. */
#endif
	struct termios sh_term;		/* Terminal information. */
};
