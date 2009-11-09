/*
posix/_cfsetispeed

Created:	June 11, 1993 by Philip Homburg
*/

#include <termios.h>

int _cfsetispeed(struct termios *termios_p, speed_t speed)
{
  termios_p->c_ispeed= speed;
  return 0;
}
