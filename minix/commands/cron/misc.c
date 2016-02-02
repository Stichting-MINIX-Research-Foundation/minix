/*	misc.c - Miscellaneous stuff for cron		Author: Kees J. Bot
 *								12 Jan 1997
 */
#define nil ((void*)0)
#include <sys/types.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include "misc.h"

char *prog_name;		/* Name of this program. */
time_t now;			/* Cron's idea of the current time. */
time_t next;			/* Time to run the next job. */

size_t alloc_count;		/* # Of chunks of memory allocated. */

void *allocate(size_t len)
/* Checked malloc().  Better not feed it length 0. */
{
	void *mem;

	if ((mem= malloc(len)) == nil) {
		cronlog(LOG_ALERT, "Out of memory, exiting\n");
		exit(1);
	}
	alloc_count++;
	return mem;
}

void deallocate(void *mem)
{
	if (mem != nil) {
		free(mem);
		alloc_count--;
	}
}

static enum logto logto= SYSLOG;

void selectlog(enum logto where)
/* Select where logging output should go, syslog or stdout. */
{
	logto= where;
}

void cronlog(int level, const char *fmt, ...)
/* Like syslog(), but may go to stderr. */
{
	va_list ap;

	va_start(ap, fmt);

#if __minix_vmd || !__minix
	if (logto == SYSLOG) {
		vsyslog(level, fmt, ap);
	} else
#endif
	{
		fprintf(stderr, "%s: ", prog_name);
		vfprintf(stderr, fmt, ap);
	}
	va_end(ap);
}

/*
 * $PchId: misc.c,v 1.3 2000/07/17 19:01:57 philip Exp $
 */
