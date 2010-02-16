/*
posix/_cfgetispeed

Created:	June 11, 1993 by Philip Homburg
*/

#include <termios.h>

speed_t _cfgetispeed(const struct termios *termios_p)
{
  return termios_p->c_ispeed;
}
