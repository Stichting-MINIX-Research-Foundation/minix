/* wall - write to all logged in users			Author: V. Archer */
/*
   Edvard Tuinder    v892231@si.hhs.NL
    Modified some things to include this with my shutdown/halt
    package
 */

#define _POSIX_SOURCE	1
#include <sys/types.h>
#include <fcntl.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <utmp.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#undef UTMP

static char UTMP[] = "/etc/utmp";	/* Currently logged in users. */

void wall _ARGS(( char *when, char *extra ));
void crnlcat _ARGS(( char *message, char *more ));

void
wall(when, extra)
char *when;			/* When is shutdown */
char *extra;			/* If non-nil, why is the shutdown */
{
  struct utmp utmp;
  char utmptty[5 + sizeof(utmp.ut_line) + 1];
  char message[1024];
  struct passwd *pw;
  int utmpfd, ttyfd;
  char *ourtty, *ourname;
  time_t now;
  struct utsname utsname;
  struct stat con_st, tty_st;

  if (ourtty = ttyname(1)) {
	if (ourname = strrchr(ourtty, '/')) ourtty = ourname+1;
  } else ourtty = "system task";
  if (pw = getpwuid(getuid())) ourname = pw->pw_name;
  else ourname = "unknown";

  time(&now);
  if (uname(&utsname) != 0) strcpy(utsname.nodename, "?");
  sprintf(message, "\r\nBroadcast message from %s@%s (%s)\r\n%.24s...\007\007\007\r\n",
		ourname, utsname.nodename, ourtty, ctime(&now));

  crnlcat(message, when);
  crnlcat(message, extra);

/* Search the UTMP database for all logged-in users. */

  if ((utmpfd = open(UTMP, O_RDONLY)) < 0) {
	fprintf(stderr, "Cannot open utmp file\r\n");
	return;
  }

  /* first the console */
  strcpy(utmptty, "/dev/console");
  if ((ttyfd = open(utmptty, O_WRONLY | O_NONBLOCK)) < 0) {
	perror(utmptty);
  } else {
	fstat(ttyfd, &con_st);
	write(ttyfd, message, strlen(message));
	close(ttyfd);
  }

  while (read(utmpfd, (char *) &utmp, sizeof(utmp)) == sizeof(utmp)) {
	/* is this the user we are looking for? */
	if (utmp.ut_type != USER_PROCESS) continue;

	strncpy(utmptty+5, utmp.ut_line, sizeof(utmp.ut_line));
	utmptty[5 + sizeof(utmp.ut_line) + 1] = 0;
	if ((ttyfd = open(utmptty, O_WRONLY | O_NONBLOCK)) < 0) {
		perror(utmptty);
		continue;
	}
	fstat(ttyfd, &tty_st);
	if (tty_st.st_rdev != con_st.st_rdev)
		write(ttyfd, message, strlen(message));
	close(ttyfd);
  }
  close(utmpfd);
  return;
}

void
crnlcat(message, more)
char *message, *more;
{
  char *p = message;
  char *m = more;
  char *end = message + 1024 - 1;

  while (p < end && *p != 0) *p++;

  while (p < end && *m != 0) {
    if (*m == '\n' && (p == message || p[-1] != '\n')) {
      *p++ = '\r';
      if (p == end) p--;
    }
    *p++ = *m++;
  }
  *p = 0;
}
