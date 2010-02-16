/*
posix/_cfgetospeed

Created:	June 11, 1993 by Philip Homburg
*/

#include <termios.h>

speed_t _cfgetospeed(const struct termios *termios_p)
{
  return termios_p->c_ospeed;
}
