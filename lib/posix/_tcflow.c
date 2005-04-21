/*
posix/_tcflow.c

Created:	June 8, 1993 by Philip Homburg
*/

#define tcflow _tcflow
#define ioctl _ioctl
#include <termios.h>
#include <sys/ioctl.h>

int tcflow(fd, action)
int fd;
int action;
{
  return(ioctl(fd, TCFLOW, &action));
}
