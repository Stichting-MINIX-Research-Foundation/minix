#include <lib.h>
#define alarm	_alarm
#include <unistd.h>

PUBLIC unsigned int alarm(sec)
unsigned int sec;
{
  message m;

  m.m1_i1 = (int) sec;
  return( (unsigned) _syscall(MM, ALARM, &m));
}
