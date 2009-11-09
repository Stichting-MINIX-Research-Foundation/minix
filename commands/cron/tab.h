/*	tab.h - in-core crontab data			Author: Kees J. Bot
 *								7 Dec 1996
 */
#ifndef TAB__H
#define TAB__H

#include <sys/types.h>
#include <limits.h>

struct crontab;

typedef unsigned char bitmap_t[8];

typedef struct cronjob {	/* One entry in a crontab file */
	struct cronjob	*next;
	struct crontab	*tab;		/* Associated table file. */
	bitmap_t	min;		/* Minute (0-59) */
	bitmap_t	hour;		/* Hour (0-23) */
	bitmap_t	mday;		/* Day of the month (1-31) */
	bitmap_t	mon;		/* Month (1-12) */
	bitmap_t	wday;		/* Weekday (0-7 with 0 = 7 = Sunday) */
	char		*user;		/* User to run it as (nil = root) */
	char		*cmd;		/* Command to run */
	time_t		rtime;		/* When next to run */
	char		do_mday;	/* True iff mon or mday is not '*' */
	char		do_wday;	/* True iff wday is not '*' */
	char		late;		/* True iff the job is late */
	char		atjob;		/* True iff it is an AT job */
	pid_t		pid;		/* Process-id of job if nonzero */
} cronjob_t;

typedef struct crontab {
	struct crontab	*next;
	char		*file;		/* Crontab name */
	char		*user;		/* Owner if non-null */
	time_t		mtime;		/* Last modified time */
	cronjob_t	*jobs;		/* List of jobs in the file */
	char		*data;		/* File data */
	int		current;	/* True if current, i.e. file exists */
} crontab_t;

crontab_t *crontabs;		/* All crontabs. */

/* A time as far in the future as possible. */
#define NEVER		((time_t) ((time_t) -1 < 0 ? LONG_MAX : ULONG_MAX))

/* Don't trust crontabs bigger than this: */
#define TAB_MAX		((sizeof(int) == 2 ? 8 : 128) * 1024)

/* Pid if no process running, or a pid value you'll never see. */
#define IDLE_PID	((pid_t) 0)
#define NO_PID		((pid_t) -1)

/* Bitmap operations. */
#define bit_set(map, n)		((void) ((map)[(n) >> 3] |= (1 << ((n) & 7))))
#define bit_clr(map, n)		((void) ((map)[(n) >> 3] &= ~(1 << ((n) & 7))))
#define bit_isset(map, n)	(!!((map)[(n) >> 3] & (1 << ((n) & 7))))

/* Functions. */
void tab_parse(char *file, char *user);
void tab_find_atjob(char *atdir);
void tab_purge(void);
void tab_reap_job(pid_t pid);
void tab_reschedule(cronjob_t *job);
cronjob_t *tab_nextjob(void);
void tab_print(FILE *fp);

#endif /* TAB__H */

/*
 * $PchId: tab.h,v 1.3 2000/07/17 07:57:27 philip Exp $
 */
