/*
sys/svrctl.h

Created:	Feb 15, 1994 by Philip Homburg <philip@cs.vu.nl>
*/

#ifndef _SYS__SVRCTL_H
#define _SYS__SVRCTL_H

#include <sys/types.h>

/* Server control commands have the same encoding as the commands for ioctls. */
#include <minix/ioctl.h>

/* PM controls. */
#define PMGETPARAM	_IOWR('P',  0, struct sysgetenv)
#define PMSETPARAM	_IOW('P',  1, struct sysgetenv)

#define OPMGETPARAM	_IOW('M',  5, struct sysgetenv)	/* old, phasing out */
#define OPMSETPARAM	_IOR('M',  7, struct sysgetenv)	/* old, phasing out */

/* VFS controls */
#define VFSGETPARAM	_IOWR('F', 0, struct sysgetenv)
#define VFSSETPARAM	_IOW('F', 1, struct sysgetenv)

struct sysgetenv {
	char		*key;		/* Name requested. */
	size_t		keylen;		/* Length of name including \0. */
	char		*val;		/* Buffer for returned data. */
	size_t		vallen;		/* Size of return data buffer. */
};

int svrctl(unsigned long _request, void *_data);

#endif /* _SYS__SVRCTL_H */
