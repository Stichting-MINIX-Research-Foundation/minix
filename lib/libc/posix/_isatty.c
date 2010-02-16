#include <lib.h>
#define isatty _isatty
#define tcgetattr _tcgetattr
#include <termios.h>
#include <unistd.h>

PUBLIC int isatty(fd)
int fd;
{
  struct termios dummy;

  return(tcgetattr(fd, &dummy) == 0);
}
