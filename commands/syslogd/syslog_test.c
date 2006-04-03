/*
**	syslog_test
**
**	Author:		Giovanni Falzoni <gfalzoni@inwind.it>
**	$Id$
*/

#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>

/*
**	SYSLOG TEST
**	Very simple utility to test syslog facility.
*/
void main(void)
{
  int ix;

  openlog("syslog_test", LOG_PID | LOG_NDELAY | LOG_PERROR | LOG_CONS, LOG_DAEMON);

  for (ix = LOG_EMERG; ix <= LOG_DEBUG; ix += 1) {
	sleep(2);
	syslog(ix, "message from test program - log level %d", ix);
  }
  closelog();
  return;
}

/** syslog_test.c **/
