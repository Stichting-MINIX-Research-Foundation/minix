/*
**  Copyright (c) 1983, 1988
**  The Regents of the University of California.  All rights reserved.
**
**  Redistribution and use in source and binary forms, with or without
**  modification, are permitted provided that the following conditions
**  are met:
**  1. Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**  2. Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in the
**     documentation and/or other materials provided with the distribution.
**  3. All advertising materials mentioning features or use of this software
**     must display the following acknowledgement:
**       This product includes software developed by the University of
**       California, Berkeley and its contributors.
**  4. Neither the name of the University nor the names of its contributors
**     may be used to endorse or promote products derived from this software
**     without specific prior written permission.
**
**  THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
**  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
**  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
**  ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
**  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
**  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
**  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
**  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
**  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
**  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
**  SUCH DAMAGE.
**
**  #ifndef lint
**  char copyright2[] =
**  "@(#) Copyright (c) 1983, 1988 Regents of the University of California.\n\
**   All rights reserved.\n";
**  #endif
**
**  #ifndef lint
**  static char sccsid[] = "@(#)syslogd.c	5.27 (Berkeley) 10/10/88";
**  #endif
**
**  -----------------------------------------------------------------------
**
**  SYSLOGD -- log system messages
**	This program implements a system log.
**	It takes a series of lines and outputs them according to the setup
**	defined in the configuration file.
**	Each line may have a priority, signified as "<n>" as
**	the first characters of the line.  If this is
**	not present, a default priority is used.
**
**	To kill syslogd, send a signal 15 (terminate).
**	A signal 1 (hup) will cause it to reread its configuration file.
**
**  Defined Constants:
**	MAXLINE   -- the maximimum line length that can be handled.
**	MAXSVLINE -- the length of saved messages (for filtering)
**	DEFUPRI   -- the default priority for user messages
**	DEFSPRI   -- the default priority for kernel messages
**
**  Author: Eric Allman
**  extensive changes by Ralph Campbell
**  more extensive changes by Eric Allman (again)
**  changes by Steve Lord
**
**  Extensive rewriting by G. Falzoni <gfalzoni@inwind.it> for porting to Minix
** 
**  $Log$
**  Revision 1.3  2006/04/04 14:22:40  beng
**  Fix
**
**  Revision 1.2  2006/04/04 14:18:16  beng
**  Make syslogd work, even if it can only open klog and not udp or vice versa
**  (but not neither)
**
**  Revision 1.1  2006/04/03 13:07:42  beng
**  Kick out usyslogd in favour of syslogd Giovanni's syslogd port
**
**  Revision 1.3  2005/09/16 10:10:12  lsodgf0
**  Rework for Minix 3.  Adds kernel logs from /dev/klogd
*/

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <net/netlib.h>
#include <net/hton.h>
#include <net/gen/in.h>
#include <net/gen/udp.h>
#include <net/gen/udp_io.h>
#include <net/gen/netdb.h>

#define SYSLOG_NAMES
#include <syslog.h>
#define KLOGD 1
/** Define following values to your requirements **/
#define	MAXLINE		512	/* maximum line length */
#define	MAXSVLINE	256	/* maximum saved line length */

#define DEFUPRI		(LOG_USER|LOG_NOTICE)
#define DEFSPRI 	(LOG_KERN|LOG_CRIT)

/* Flags to logmsg() */
#define IGN_CONS	0x001	/* don't print on console */
#define SYNC_FILE	0x002	/* do fsync on file after printing */
#define ADDDATE		0x004	/* add a date to the message */
#define MARK		0x008	/* this message is a mark */

#define	CTTY		"/dev/log"	/* Minix log device (console) */

#define	dprintf	if(DbgOpt!=0)printf
#if debug == 0
#define DEBUG(statement)
#else
#define DEBUG(statement) statement
#endif
#if !defined PIDFILE
#define PIDFILE	"/var/run/syslogd.pid"
#endif

#define UNAMESZ		8	/* length of a login name */
#define MAXUNAMES	20	/* maximum number of user names */
#define MAXFNAME	200	/* max file pathname length */
#define MAXHOSTNAMELEN 	64	/* max length of FQDN host name */

/* Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.  After each flush,
 * we move to the next interval until we reach the largest.  */
#define TIMERINTVL	30	/* interval for checking flush, mark */
#define INTERVAL1 	30
#define INTERVAL2 	60
#define	MAXREPEAT ((sizeof(repeatinterval)/sizeof(repeatinterval[0]))-1)
#define	REPEATTIME(f) ((f)->f_time+repeatinterval[(f)->f_repeatcount])
#define	BACKOFF(f) {if(++(f)->f_repeatcount>MAXREPEAT)(f)->f_repeatcount=MAXREPEAT;}

/* Values for f_type */
#define F_UNUSED 	0	/* unused entry */
#define F_FILE 		1	/* regular file */
#define F_TTY 		2	/* terminal */
#define F_CONSOLE 	3	/* console terminal */
#define F_FORW 		4	/* remote machine */
#define F_USERS 	5	/* list of users */
#define F_WALL 		6	/* everyone logged on */

#define max(a,b) ((a)>=(b)?(a):(b))

/* This structure represents the files that will have log copies printed */
struct filed {
  struct filed *f_next;		/* next in linked list */
  short f_type;			/* entry type, see below */
  short f_file;			/* file descriptor */
  time_t f_time;		/* time this was last written */
  char f_pmask[LOG_NFACILITIES + 1];	/* priority mask */
  union {
	char f_uname[MAXUNAMES][UNAMESZ + 1];
	char f_fname[MAXFNAME];
  } f_un;
  char f_prevline[MAXSVLINE];	/* last message logged */
  char f_lasttime[16];		/* time of last occurrence */
  char f_prevhost[MAXHOSTNAMELEN + 1];	/* host from which recd. */
  int f_prevpri;		/* pri of f_prevline */
  int f_prevlen;		/* length of f_prevline */
  int f_prevcount;		/* repetition cnt of prevline */
  int f_repeatcount;		/* number of "repeated" msgs */
  int f_flags;			/* store some additional flags */
};

static const char *const TypeNames[] =
{
 "UNUSED", "FILE", "TTY", "CONSOLE", "FORW", "USERS", "WALL", NULL,
};

static struct filed *Files = NULL;
static struct filed consfile;
static int DbgOpt = 0;		/* debug flag */
static char LocalHostName[MAXHOSTNAMELEN + 1];	/* our hostname */
static int Initialized = 0;	/* set when we have initialized ourselves */
static int MarkInterval = 20 * 60;	/* interval between marks in seconds */
static int MarkSeq = 0;		/* mark sequence number */
static time_t now;

static const char *ConfFile = "/etc/syslog.conf";
static const char *PidFile = PIDFILE;	/* "/var/run/syslogd.pid" */
static const char ctty[] = CTTY;

static const char ProgName[] = "syslogd:";
static const char version[] = "1.3 (Minix)";
static const char usage[] =
 /* */ "usage:\tsyslogd [-d] [-m markinterval] [-f conf-file]\n"
       "\t\t[-p listeningport] [-v] [-?]\n" ;
static const int repeatinterval[] =
 /* */ {INTERVAL1, INTERVAL2,};	/* # of secs before flush */

/*
**  Name:	void wallmsg(struct filed *fLog, char *message);
**  Function:	Write the specified message to either the entire
**		world, or a list of approved users.
*/
void wallmsg(struct filed * fLog, char *message)
{

  return;
}

/*
**  Name:	void fprintlog(struct filed *fLog, int flags, char *message);
**  Function:
*/
void fprintlog(struct filed * fLog, int flags, char *message)
{
  int len;
  char line[MAXLINE + 1];
  char repbuf[80];

  if (message == NULL) {
	if (fLog->f_prevcount > 1) {
		sprintf(repbuf, "last message repeated %d times", fLog->f_prevcount);
		message = repbuf;
	} else
		message = fLog->f_prevline;
  }
  snprintf(line, sizeof(line), "%s %s %s",
	fLog->f_lasttime, fLog->f_prevhost, message);
  DEBUG(dprintf("Logging to %s", TypeNames[fLog->f_type]);)
  fLog->f_time = now;
  switch (fLog->f_type) {
      case F_UNUSED:		/* */
	DEBUG(dprintf("\n");)
	break;
      case F_CONSOLE:
	if (flags & IGN_CONS) {
      case F_FORW:		/* */
		DEBUG(dprintf(" (ignored)\n");)
		break;
	}			/* else Fall Through */
      case F_TTY:
      case F_FILE:
	DEBUG(dprintf(" %s\n", fLog->f_un.f_fname);)
	strcat(line, fLog->f_type != F_FILE ? "\r\n" : "\n");
	len = strlen(line);
	if (write(fLog->f_file, line, len) != len) {
		 /* Handle errors */ ;
	} else if (flags & SYNC_FILE)
		sync();
	break;
      case F_USERS:
      case F_WALL:
	DEBUG(dprintf("\n");)
	strcat(line, "\r\n");
	wallmsg(fLog, line);
	break;
  }
  fLog->f_prevcount = 0;
  return;
}

/*
**  Name:	void logmsg(int pri, char *msg, char *from, int flags);
**  Function:	Log a message to the appropriate log files, users, etc.
**		based on the priority.
*/
void logmsg(int pri, char *msg, char *from, int flags)
{
  struct filed *f;
  int fac, prilev;
  int /*omask,*/ msglen;
  char *timestamp;

  DEBUG(dprintf("logmsg: pri %o, flags %x, from %s, msg %s\n", pri, flags, from, msg);)
/*
  omask = sigblock(__sigmask(SIGHUP) | __sigmask(SIGALRM));
*/
  /* Check to see if msg looks non-standard. */
  msglen = strlen(msg);
  if (msglen < 16 || msg[3] != ' ' || msg[6] != ' ' ||
      msg[9] != ':' || msg[12] != ':' || msg[15] != ' ')
	flags |= ADDDATE;

  time(&now);
  if (flags & ADDDATE)
	timestamp = ctime(&now) + 4;
  else {
	timestamp = msg;
	msg += 16;
	msglen -= 16;
  }

  /* Extract facility and priority level */
  fac = (flags & MARK) ? LOG_NFACILITIES : LOG_FAC(pri);
  prilev = LOG_PRI(pri);

  /* Log the message to the particular outputs */
  if (!Initialized) {
	/* Not yet initialized. Every message goes to console */
	f = &consfile;
	f->f_file = open(ctty, O_WRONLY | O_NOCTTY);
	if (f->f_file >= 0) {
		if (!DbgOpt) setsid();
		fprintlog(f, flags, msg);
		close(f->f_file);
	}
  } else {
	for (f = Files; f; f = f->f_next) {

		/* Skip messages that are incorrect priority */
		if (f->f_pmask[fac] < prilev || f->f_pmask[fac] == INTERNAL_NOPRI)
			continue;

		if (f->f_type == F_CONSOLE && (flags & IGN_CONS)) continue;

		/* Don't output marks to recently written files */
		if ((flags & MARK) && (now - f->f_time) < MarkInterval / 2)
			continue;

		/* Suppress duplicate lines to this file */
		if ((flags & MARK) == 0 && msglen == f->f_prevlen &&
		    !strcmp(msg, f->f_prevline) &&
		    !strcmp(from, f->f_prevhost)) {
			strncpy(f->f_lasttime, timestamp, 15);
			f->f_prevcount += 1;
			DEBUG(dprintf("msg repeated %d times, %ld sec of %d\n",
				f->f_prevcount, now - f->f_time,
				repeatinterval[f->f_repeatcount]);)
			/* If domark would have logged this by now,
			 * flush it now (so we don't hold isolated
			 * messages), but back off so we'll flush
			 * less often in the future. */
			if (now > REPEATTIME(f)) {
				fprintlog(f, flags, (char *) NULL);
				BACKOFF(f);
			}
		} else {
			/* New line, save it */
			if (f->f_prevcount) fprintlog(f, 0, (char *) NULL);
			f->f_repeatcount = 0;
			strncpy(f->f_lasttime, timestamp, 15);
			strncpy(f->f_prevhost, from, sizeof(f->f_prevhost));
			if (msglen < MAXSVLINE) {
				f->f_prevlen = msglen;
				f->f_prevpri = pri;
				strcpy(f->f_prevline, msg);
				fprintlog(f, flags, (char *) NULL);
			} else {
				f->f_prevline[0] = 0;
				f->f_prevlen = 0;
				fprintlog(f, flags, msg);
			}
		}
	}
  }

/*
  sigsetmask(omask);
*/
  return;
}

/*
**  Name:	void logerror(char *type);
**  Function:	Prints syslogd errors in some place.
*/
void logerror(char *type)
{
  char buf[100];

  if (errno == 0) sprintf(buf, "%s %s", ProgName, type);

  sprintf(buf, "%s %s - %s", ProgName, type, strerror(errno));

  errno = 0;
  dprintf("%s\n", buf);
  logmsg(LOG_SYSLOG | LOG_ERR, buf, LocalHostName, ADDDATE);
  return;
}

/*
**  Name:	void die(int sig);
**  Function:	Signal handler for kill signals.
*/
void die(int sig)
{
  struct filed *f;
  char buf[100];

  for (f = Files; f != NULL; f = f->f_next) {
	/* Flush any pending output */
	if (f->f_prevcount) fprintlog(f, 0, NULL);
  }
  if (sig >= 0) {
	DEBUG(dprintf("%s exiting on signal %d\n", ProgName, sig);)
	sprintf(buf, "exiting on signal %d", sig);
	errno = 0;
	logerror(buf);
  }
  unlink(PidFile);
  exit(sig == (-1) ? EXIT_FAILURE : EXIT_SUCCESS);
}

/*
**  Name:	void domark(int sig);
**  Function:	Signal handler for alarm.
**		Used for messages filtering and mark facility.
*/
void domark(int sig)
{
  struct filed *f;

  now = time(NULL);
  MarkSeq += TIMERINTVL;
  if (MarkSeq >= MarkInterval) {
	logmsg(LOG_INFO, "-- MARK --", LocalHostName, ADDDATE | MARK);
	MarkSeq = 0;
  }
  for (f = Files; f; f = f->f_next) {
	if (f->f_prevcount && now >= REPEATTIME(f)) {
		DEBUG(dprintf("flush %s: repeated %d times, %d sec.\n",
			TypeNames[f->f_type], f->f_prevcount,
			repeatinterval[f->f_repeatcount]);)
		fprintlog(f, 0, NULL);
		BACKOFF(f);
	}
  }
  signal(SIGALRM, domark);
  alarm(TIMERINTVL);
  return;
}

/*
**  Name:	int decode(char *name, struct _code *codetab);
**  Function:	Decode a symbolic name to a numeric value
*/
int decode(char *name, const struct _code *codetab)
{
  const struct _code *c;
  char *p;
  char buf[40];

  DEBUG(dprintf("symbolic name: %s", name);)
  if (isdigit(*name)) return (atoi(name));

  strcpy(buf, name);
  for (p = buf; *p; p += 1) {
	if (isupper(*p)) *p = tolower(*p);
  }
  for (c = codetab; c->c_name; c += 1) {
	if (!strcmp(buf, c->c_name)) {
		DEBUG(dprintf(" ==> %d\n", c->c_val);)
		return (c->c_val);
	}
  }
  return (-1);
}

/*
**  Name:	void cfline(char *line, struct filed *f);
**  Function:	Parse a configuration file line
*/
void cfline(char *line, struct filed * fLog)
{
  char *p, *q, *bp;
  int ix, pri;
  char buf[MAXLINE];
  char xbuf[200];

  DEBUG(dprintf("cfline(%s)\n", line);)

  /* Keep sys_errlist stuff out of logerror messages */
  errno = 0;

  /* Clear out file entry */
  memset(fLog, 0, sizeof(*fLog));
  for (ix = 0; ix <= LOG_NFACILITIES; ix += 1)	/* */
	fLog->f_pmask[ix] = INTERNAL_NOPRI;

  /* Scan through the list of selectors */
  for (p = line; *p && *p != '\t';) {

	/* Find the end of this facility name list */
	for (q = p; *q && *q != '\t' && *q++ != '.';) continue;

	/* Collect priority name */
	for (bp = buf; *q && !strchr("\t,;", *q);) *bp++ = *q++;
	*bp = '\0';

	/* Skip cruft */
	while (strchr(", ;", *q)) q++;

	/* Decode priority name */
	pri = decode(buf, prioritynames);
	if (pri < 0) {
		sprintf(xbuf, "unknown priority name \"%s\"", buf);
		logerror(xbuf);
		return;
	}

	/* Scan facilities */
	while (*p && !strchr("\t.;", *p)) {
		for (bp = buf; *p && !strchr("\t,;.", *p);) *bp++ = *p++;
		*bp = '\0';
		if (*buf == '*') {
			for (ix = 0; ix <= LOG_NFACILITIES; ix += 1)
				if ((fLog->f_pmask[ix] < pri) ||
				    (fLog->f_pmask[ix] == INTERNAL_NOPRI)) {
					fLog->f_pmask[ix] = pri;
				}
		} else {
			ix = decode(buf, facilitynames);
			if (ix < 0) {
				sprintf(xbuf, "unknown facility name \"%s\"", buf);
				logerror(xbuf);
				return;
			}
			if ((fLog->f_pmask[ix >> 3] < pri) ||
			(fLog->f_pmask[ix >> 3] == INTERNAL_NOPRI)) {
				fLog->f_pmask[ix >> 3] = pri;
			}
		}
		while (*p == ',' || *p == ' ') p++;
	}
	p = q;
  }

  /* Skip to action part */
  while (*p == '\t' || *p == ' ') p++;

  DEBUG(dprintf("leading char in action: %c\n", *p);)
  switch (*p) {
      case '@':			/* Logging to a remote host */
	break;			/* NOT IMPLEMENTED */

      case '/':			/* Logging to a local file/device */
	strcpy(fLog->f_un.f_fname, p);
	DEBUG(dprintf("filename: %s\n", p);	/* ASP */)
	if ((fLog->f_file = open(p, O_WRONLY | O_APPEND | O_CREAT | O_NOCTTY, 0644)) < 0) {
		fLog->f_file = F_UNUSED;
		sprintf(xbuf, "unknown file/device (%s)", p);
		logerror(xbuf);
		break;
	}
	if (isatty(fLog->f_file)) {
		if (!DbgOpt) setsid();
		fLog->f_type = F_TTY;
	} else
		fLog->f_type = F_FILE;
	if (strcmp(p, ctty) == 0) fLog->f_type = F_CONSOLE;
	break;

      case '*':			/* Logging to all users */
	DEBUG(dprintf("write-all\n");)
	fLog->f_type = F_WALL;
	break;

      default:			/* Logging to selected users */
	DEBUG(dprintf("users: %s\n", p);	/* ASP */)
	for (ix = 0; ix < MAXUNAMES && *p; ix += 1) {
		for (q = p; *q && *q != ',';) q += 1;
		strncpy(fLog->f_un.f_uname[ix], p, UNAMESZ);
		if ((q - p) > UNAMESZ)
			fLog->f_un.f_uname[ix][UNAMESZ] = '\0';
		else
			fLog->f_un.f_uname[ix][q - p] = '\0';
		while (*q == ',' || *q == ' ') q++;
		p = q;
	}
	fLog->f_type = F_USERS;
	break;
  }
}

/*
**  Name:  	void printline(char *hname, char *msg);
**  Function:	Takes a raw input line, decodes the message and
**	  	prints the message on the appropriate log files.
*/
void printline(char *hname, char *msg)
{
  char line[MAXLINE + 1];
  char *p = msg, *q = line;
  int ch, pri = DEFUPRI;

  /* Test for special codes */
  if (*p == '<') {
	pri = 0;
	while (isdigit(*++p)) {
		if ((*p - '0') < 8) {
			/* Only 3 bits allocated for pri -- ASP */
			pri = 10 * pri + (*p - '0');
		} else
			pri = 10 * pri + 7;
	}
	if (*p == '>') ++p;
  }
  if (pri & ~(LOG_FACMASK | LOG_PRIMASK)) pri = DEFUPRI;

  /* Does not allow users to log kernel messages */
  if (LOG_FAC(pri) == LOG_KERN) pri = LOG_MAKEPRI(LOG_USER, LOG_PRI(pri));

  /* Copies message to local buffer, translating control characters */
  while ((ch = *p++ & 0177) != '\0' && q < &line[sizeof(line) - 1]) {
	if (ch == '\n')		/* Removes newlines */
		*q++ = ' ';
	else if (iscntrl(ch)) {	/* Translates control characters */
		*q++ = '^';
		*q++ = ch ^ 0100;
	} else
		*q++ = ch;
  }
  *q = '\0';

  logmsg(pri, line, hname, 0);
  return;
}

/*
**  Name:  	void printkline(char *hname, char *msg);
**  Function:	Takes a raw input line from kernel and 
**	  	prints the message on the appropriate log files.
*/
void printkline(char *hname, char *msg)
{
  char line[MAXLINE + 1];

  /* Copies message to local buffer, adding source program tag */
  snprintf(line, sizeof(line), "kernel: %s", msg);

  logmsg(LOG_KERN | LOG_INFO, line, hname, ADDDATE);
  return;
}

/*
**  Name:	void init(int sig);
**  Function:	Initialize syslogd from configuration file.
**		Used at startup or after a SIGHUP signal.
*/
void init(int sig)
{
  FILE *cf;
  struct filed *fLog, *next, **nextp;
  char *p;
  char cline[BUFSIZ];

  DEBUG(dprintf("init\n");)

  /* Close all open log files. */
  Initialized = 0;
  for (fLog = Files; fLog != NULL; fLog = next) {

	/* Flush any pending output */
	if (fLog->f_prevcount) fprintlog(fLog, 0, NULL);

	switch (fLog->f_type) {
	    case F_FILE:
	    case F_TTY:
	    case F_CONSOLE:	close(fLog->f_file);	break;
	}
	next = fLog->f_next;
	free((char *) fLog);
  }
  Files = NULL;
  nextp = &Files;

  /* Open the configuration file */
  if ((cf = fopen(ConfFile, "r")) != NULL) {
	/* Foreach line in the configuration table, open that file. */
	fLog = NULL;
	while (fgets(cline, sizeof(cline), cf) != NULL) {
		/* Check for end-of-section, comments, strip off
		 * trailing spaces and newline character. */
		for (p = cline; isspace(*p); p += 1);
		if (*p == '\0' || *p == '#') continue;
		for (p = strchr(cline, '\0'); isspace(*--p););
		*++p = '\0';
		fLog = (struct filed *) calloc(1, sizeof(*fLog));
		*nextp = fLog;
		nextp = &fLog->f_next;
		cfline(cline, fLog);
	}

	/* Close the configuration file */
	fclose(cf);
	Initialized = 1;
DEBUG (
	if (DbgOpt) {
		for (fLog = Files; fLog; fLog = fLog->f_next) {
			for (i = 0; i <= LOG_NFACILITIES; i += 1)
				if (fLog->f_pmask[i] == INTERNAL_NOPRI)
					printf("X ");
				else
					printf("%d ", fLog->f_pmask[i]);
			printf("%s: ", TypeNames[fLog->f_type]);
			switch (fLog->f_type) {
			    case F_FILE:
			    case F_TTY:
			    case F_CONSOLE:
				printf("%s", fLog->f_un.f_fname);
				break;
			    case F_FORW:
				break;
			    case F_USERS:
				for (i = 0; i < MAXUNAMES && *fLog->f_un.f_uname[i]; i += 1)
					printf("%s, ", fLog->f_un.f_uname[i]);
				break;
			}
			printf("\n");
		}
	}
  )
	logmsg(LOG_SYSLOG | LOG_INFO, "syslogd: restart", LocalHostName, ADDDATE);
	signal(SIGHUP, init);
	DEBUG(dprintf("%s restarted\n", ProgName);)
  } else {
	DEBUG(dprintf("cannot open %s\n", ConfFile);)
	*nextp = (struct filed *) calloc(1, sizeof(*fLog));
	cfline("*.ERR\t" CTTY, *nextp);
	(*nextp)->f_next = (struct filed *) calloc(1, sizeof(*fLog));
	cfline("*.PANIC\t*", (*nextp)->f_next);
	Initialized = 1;
  }
  return;
}

/*
**  Name:	void daemonize(char *line);
**  Function:	Clone itself and becomes a daemon releasing unnecessay resources.
*/
void daemonize(char *line)
{
  int lfd, len, pid;

  if ((lfd = open(PidFile, O_CREAT | O_RDWR, 0600)) > 0) {
	len = read(lfd, line, 10);
	line[len] = '\0';
	close(lfd);
	if ((kill(len = atoi(line), 0) < 0 && errno == ESRCH) || len == 0) {
		if (!DbgOpt) {
			/* Parent ends and child becomes a daemon */
			if ((pid = fork()) > 0) {
				/* Write process id. in pid file */
				lfd = open(PidFile, O_TRUNC | O_WRONLY);
				len = sprintf(line, "%5d", pid);
				write(lfd, line, len);
				close(lfd);

				/* Wait for initialization to complete */
				exit(EXIT_SUCCESS);
			}
			sleep(1);
			setsid();	/* Set as session leader */
			chdir("/");	/* Change to the root directory */
			/* Get rid of all open files */
			for (lfd = STDERR_FILENO + 1; lfd < OPEN_MAX; lfd += 1)
				close(lfd);
		}
	} else {
		fprintf(stderr, "\n%s already running\n", ProgName);
		exit(EXIT_FAILURE);
	}
  } else {
	fprintf(stderr, "\n%s can't open %s (%s)\n", ProgName, PidFile, strerror(errno));
	exit(EXIT_FAILURE);
  }
  return;
}

/*
**  Name:	int main(int argc, char **argv);
**  Function:	Syslog daemon entry point
*/
int main(int argc, char **argv)
{
  char *p, *udpdev, *eol;
  int nfd, kfd, len, fdmax;
  int ch, port = 0;
  fd_set fdset;
  struct nwio_udpopt udpopt;
  struct servent *sp;
  char line[MAXLINE + 1];

  while ((ch = getopt(argc, argv, "df:m:p:v?")) != EOF) {
	switch ((char) ch) {
	    case 'd':		/* Debug */
		DbgOpt += 1;
		break;
	    case 'f':		/* Set configuration file */
		ConfFile = optarg;
		break;
	    case 'm':		/* Set mark interval */
		MarkInterval = atoi(optarg) * 60;
		break;
	    case 'p':		/* Set listening port */
		port = atoi(optarg);
		break;
	    case 'v':		/* Print version */
		fprintf(stderr, "%s version %s\n", ProgName, version);
		return EXIT_FAILURE;
	    case '?':		/* Help */
	    default:
		fprintf(stderr, usage);
		return EXIT_FAILURE;
	}
  }
  if (argc -= optind) {
	fprintf(stderr, usage);
	return EXIT_FAILURE;
  }

  daemonize(line);

  /* Get the official name of local host. */
  gethostname(LocalHostName, sizeof(LocalHostName) - 1);
  if ((p = strchr(LocalHostName, '.'))) *p = '\0';

  udpdev = (p = getenv("UDP_DEVICE")) ? p : UDP_DEVICE;
  sp = getservbyname("syslog", "udp");

  signal(SIGTERM, die);
  signal(SIGINT, DbgOpt ? die : SIG_IGN);
  signal(SIGQUIT, DbgOpt ? die : SIG_IGN);
  signal(SIGALRM, domark);

  alarm(TIMERINTVL);

  /* Open UDP device */
  nfd = open(udpdev, O_NONBLOCK | O_RDONLY);

  /* Configures the UDP device */
  udpopt.nwuo_flags = NWUO_SHARED | NWUO_LP_SET | NWUO_EN_LOC |
	NWUO_DI_BROAD | NWUO_RP_SET | NWUO_RA_SET |
	NWUO_RWDATONLY | NWUO_DI_IPOPT;
  udpopt.nwuo_locport = udpopt.nwuo_remport =
			port == 0 ? sp->s_port : htons(port);
  udpopt.nwuo_remaddr = udpopt.nwuo_locaddr = htonl(0x7F000001L);
  
 if(nfd >= 0) {
  while (ioctl(nfd, NWIOSUDPOPT, &udpopt) < 0 ||
      ioctl(nfd, NWIOGUDPOPT, &udpopt) < 0) {
	if (errno == EAGAIN) {
		sleep(1);
		continue;
	}
	logerror("Set/Get UDP options failed");
	return EXIT_FAILURE;
  }
 }

  /* Open kernel log device */
  kfd = open("/dev/klog", O_NONBLOCK | O_RDONLY);

  if(kfd < 0 && nfd < 0) {
	logerror("open /dev/klog and udp device failed - can't log anything");
	return EXIT_FAILURE;
  }

  fdmax = max(nfd, kfd) + 1;

  DEBUG(dprintf("off & running....\n");)
  
  init(-1);			/* Initilizes log data structures */

  for (;;) {			/* Main loop */

	FD_ZERO(&fdset);	/* Setup descriptors for select */
	if(nfd >= 0) FD_SET(nfd, &fdset);
	if(kfd >= 0) FD_SET(kfd, &fdset);

	if (select(fdmax, &fdset, NULL, NULL, NULL) <= 0) {
		sleep(1);
		continue;

	}
	if (nfd >= 0 && FD_ISSET(nfd, &fdset)) {

		/* Read a message from application programs */
		len = read(nfd, line, MAXLINE);
		if (len > 0) {		/* Got a message */
			line[len] = '\0';
			dprintf("got a message (%d, %#x)\n", nfd, len);
			printline(LocalHostName, line);

		} else if (len < 0) {	/* Got an error or signal while reading */
			if (errno != EINTR)	/* */
			{
				logerror("Receive error from UDP channel");
				close(nfd);
				nfd= -1;
			}

		} else {	/* (len == 0) Channel has been closed */
			logerror("UDP channel has closed");
			close(nfd);
			die(-1);
		}
	}
	if (kfd >= 0 && FD_ISSET(kfd, &fdset)) {
		static char linebuf[5*1024];

		/* Read a message from kernel (klog) */
		len = read(kfd, linebuf, sizeof(linebuf)-2);
		dprintf("got a message (%d, %#x)\n", kfd, len);
		for (ch = 0; ch < len; ch += 1)
			if (linebuf[ch] == '\0') linebuf[ch] = ' ';
		if (linebuf[len - 1] == '\n') len -= 1;
		linebuf[len] = '\n';
		linebuf[len + 1] = '\0';
		p = linebuf;
		while((eol = strchr(p, '\n'))) {
			*eol = '\0';
			printkline(LocalHostName, p);
			p = eol+1;
		}
	}
  }
  /* Control never gets here */
}

/** syslogd.c **/
