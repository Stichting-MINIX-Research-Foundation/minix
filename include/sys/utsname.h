/* The <sys/utsname.h> header gives the system name. */

#ifndef _UTSNAME_H
#define _UTSNAME_H

#ifndef _ANSI_H
#include <ansi.h>
#endif

struct utsname {
  char sysname[15+1];
  char nodename[255+1];
  char release[11+1];
  char version[7+1];
  char machine[11+1];
  char arch[11+1];
};

/* Function Prototypes. */
_PROTOTYPE( int uname, (struct utsname *_name)				);

#endif /* _UTSNAME_H */
