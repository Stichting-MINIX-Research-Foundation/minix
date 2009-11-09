#define tcgetattr _tcgetattr
#define ioctl _ioctl
#include <sys/ioctl.h>
#include <errno.h>
#include <termios.h>

int tcgetattr(fd, termios_p)
int fd;
struct termios *termios_p;
{
  return(ioctl(fd, TCGETS, termios_p));
}
