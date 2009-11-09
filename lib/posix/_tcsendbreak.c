/*	tcsendbreak() - send a break			Author: Kees J. Bot
 *								13 Jan 1994
 */
#define tcsendbreak _tcsendbreak
#define ioctl _ioctl
#include <termios.h>
#include <sys/ioctl.h>

int tcsendbreak(int fd, int duration)
{
  return(ioctl(fd, TCSBRK, &duration));
}
