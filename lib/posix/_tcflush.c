/*	tcflush() - flush buffered characters		Author: Kees J. Bot
 *								13 Jan 1994
 */
#define tcflush _tcflush
#define ioctl _ioctl
#include <termios.h>
#include <sys/ioctl.h>

int tcflush(int fd, int queue_selector)
{
  return(ioctl(fd, TCFLSH, &queue_selector));
}
