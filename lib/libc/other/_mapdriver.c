#include <lib.h>
#define mapdriver	_mapdriver
#include <string.h>
#include <unistd.h>


PUBLIC int mapdriver(label, major, dev_style, force)
char *label;
int major;
int dev_style;
int force;
{
  message m;
  m.m2_p1 = label;
  m.m2_l1 = strlen(label);
  m.m2_i1 = major;
  m.m2_i2 = dev_style;
  m.m2_i3 = force;
  if (_syscall(FS, MAPDRIVER, &m) < 0) return(-1);
  return(0);
}

