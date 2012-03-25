#include <lib.h>
#include <string.h>
#include <unistd.h>


int mapdriver(label, major, dev_style, flags)
char *label;
int major;
int dev_style;
int flags;
{
  message m;
  m.m2_p1 = label;
  m.m2_l1 = strlen(label);
  m.m2_i1 = major;
  m.m2_i2 = dev_style;
  m.m2_i3 = flags;
  if (_syscall(VFS_PROC_NR, MAPDRIVER, &m) < 0) return(-1);
  return(0);
}

