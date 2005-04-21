/*
posix/_tcdrain.c

Created:	July 26, 1994 by Philip Homburg
*/

#define tcdrain _tcdrain
#define ioctl _ioctl
#include <termios.h>
#include <sys/ioctl.h>

int tcdrain(fd)
int fd;
{
  return(ioctl(fd, TCDRAIN, (void *)0));
}
