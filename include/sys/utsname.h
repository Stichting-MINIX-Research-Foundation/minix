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

#ifdef _MINIX
/* Uname() is implemented with sysuname(). */

_PROTOTYPE( int sysuname, (int _req, int _field, char *_value, 
							size_t _len));

/* req: Get or set a string. */
#define _UTS_GET	0
#define _UTS_SET	1

/* field: What field to get or set.  These values can't be changed lightly. */
#define _UTS_ARCH	0
#define _UTS_KERNEL	1
#define _UTS_MACHINE	2
#define _UTS_HOSTNAME	3
#define _UTS_NODENAME	4
#define _UTS_RELEASE	5
#define _UTS_VERSION	6
#define _UTS_SYSNAME	7
#define _UTS_BUS	8
#define _UTS_MAX	9	/* Number of strings. */
#endif /* _MINIX */

#endif /* _UTSNAME_H */
