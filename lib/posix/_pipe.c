#include <lib.h>
#define pipe	_pipe
#include <unistd.h>

PUBLIC int pipe(fild)
int fild[2];
{
  message m;

  if (_syscall(FS, PIPE, &m) < 0) return(-1);
  fild[0] = m.m1_i1;
  fild[1] = m.m1_i2;
  return(0);
}
