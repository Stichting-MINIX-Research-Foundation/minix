/*
posix/_cfsetospeed

Created:	June 11, 1993 by Philip Homburg
*/

#include <termios.h>

int _cfsetospeed(struct termios *termios_p, speed_t speed)
{
  termios_p->c_ospeed= speed;
  return 0;
}
