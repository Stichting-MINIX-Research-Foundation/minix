#ifndef lint
#ifndef NOID
static char	elsieid[] = "@(#)logwtmp.c	7.7";
/* As received from UCB, with include reordering and OLD_TIME condition. */
#endif /* !defined NOID */
#endif /* !defined lint */

/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANT[A]BILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
#ifdef LIBC_SCCS
static char sccsid[] = "@(#)logwtmp.c	5.2 (Berkeley) 9/20/88";
#endif /* defined LIBC_SCCS */
#endif /* !defined lint */

#include <sys/types.h>
#include <time.h>
#include <utmp.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef OLD_TIME

char dummy_to_keep_linker_happy;

#endif /* defined OLD_TIME */

#ifndef OLD_TIME

#include <sys/file.h>
#include <sys/time.h>
#include <sys/stat.h>

#define WTMPFILE	"/usr/adm/wtmp"

void logwtmp( char *line, char *name, char *host)
{
	struct utmp ut;
	struct stat buf;
	int fd;

	if ((fd = open(WTMPFILE, O_WRONLY|O_APPEND, 0)) < 0)
		return;
	if (!fstat(fd, &buf)) {
		(void)strncpy(ut.ut_line, line, sizeof(ut.ut_line));
		(void)strncpy(ut.ut_name, name, sizeof(ut.ut_name));
		(void)strncpy(ut.ut_host, host, sizeof(ut.ut_host));
		(void)time(&ut.ut_time);
		if (write(fd, (char *)&ut, sizeof(struct utmp)) !=
			sizeof(struct utmp))
				(void)ftruncate(fd, buf.st_size);
	}
	(void)close(fd);
}

#endif /* !defined OLD_TIME */
