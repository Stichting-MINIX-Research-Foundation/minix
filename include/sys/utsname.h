#ifndef	_SYS_UTSNAME_H_
#define	_SYS_UTSNAME_H_

#include <sys/featuretest.h>

#define	_SYS_NMLN	256

#if defined(_NETBSD_SOURCE)
#define	SYS_NMLN	_SYS_NMLN
#endif

struct utsname {
	char	sysname[_SYS_NMLN];	/* Name of this OS. */
	char	nodename[_SYS_NMLN];	/* Name of this network node. */
	char	release[_SYS_NMLN];	/* Release level. */
	char	version[_SYS_NMLN];	/* Version level. */
	char	machine[_SYS_NMLN];	/* Hardware type. */
	char	arch[_SYS_NMLN];
};

#include <sys/cdefs.h>

__BEGIN_DECLS
int	uname(struct utsname *);
#ifdef __minix
int 	sysuname(int _req, int _field, char *_value, size_t _len);
#endif
__END_DECLS

#ifdef __minix
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
#endif /* __minix */

#endif	/* !_SYS_UTSNAME_H_ */
