/*	misc.h - miscellaneous stuff			Author: Kees J. Bot
 *								7 Dec 1996
 */
#ifndef MISC__H
#define MISC__H

#include <time.h>

/* The name of the program. */
extern char *prog_name;

/* Where cron stores it pid. */
#define PIDFILE	"/usr/run/cron.pid"

/* Cron's idea of the current time, and the time next to run something. */
extern time_t now;
extern time_t next;

/* Memory allocation. */
void *allocate(size_t len);
void deallocate(void *mem);
extern size_t alloc_count;

/* Logging, by syslog or to stderr. */
#if __minix_vmd || !__minix
#include <sys/syslog.h>
#else
enum log_dummy { LOG_ERR, LOG_CRIT, LOG_ALERT };
#define openlog(ident, opt, facility)	((void) 0)
#define closelog()			((void) 0)
#define setlogmask(mask)		(0)
#endif

enum logto { SYSLOG, STDERR };
void selectlog(enum logto where);
void log(int level, const char *fmt, ...);

#endif /* MISC__H */

/*
 * $PchId: misc.h,v 1.3 2000/07/17 18:56:02 philip Exp $
 */
