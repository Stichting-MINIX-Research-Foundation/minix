#include <lib.h>
#define devctl	_devctl
#include <unistd.h>


PUBLIC int devctl(int ctl_req, int proc_nr, int dev_nr, int dev_style)
{
  message m;
  m.m4_l1 = ctl_req;
  m.m4_l2 = proc_nr;
  m.m4_l3 = dev_nr;
  m.m4_l4 = dev_style;
  if (_syscall(FS, DEVCTL, &m) < 0) return(-1);
  return(0);
}

