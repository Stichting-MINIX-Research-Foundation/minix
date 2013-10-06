#include <lib.h>
#include <string.h>
#include <unistd.h>


int mapdriver(label, major)
char *label;
int major;
{
  message m;
  m.m2_p1 = label;
  m.m2_l1 = strlen(label);
  m.m2_i1 = major;
  if (_syscall(VFS_PROC_NR, MAPDRIVER, &m) < 0) return(-1);
  return(0);
}

