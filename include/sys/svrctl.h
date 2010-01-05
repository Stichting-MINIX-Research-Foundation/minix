/*
sys/svrctl.h

Created:	Feb 15, 1994 by Philip Homburg <philip@cs.vu.nl>
*/

#ifndef _SYS__SVRCTL_H
#define _SYS__SVRCTL_H

#ifndef _TYPES_H
#include <minix/types.h>
#endif

/* Server control commands have the same encoding as the commands for ioctls. */
#include <minix/ioctl.h>

/* PM controls. */
#define MMGETPARAM	_IOW('M',  5, struct sysgetenv)
#define MMSETPARAM	_IOR('M',  7, struct sysgetenv)

/* FS controls. */
#define FSSIGNON	_IOW('F',  2, struct fssignon)

/* A proper system call must be created later. */
#include <minix/dmap.h>
struct fssignon {
	dev_t		dev;		/* Device to manage. */
	enum dev_style	style;		/* Management style. */
};

struct sysgetenv {
	char		*key;		/* Name requested. */
	size_t		keylen;		/* Length of name including \0. */
	char		*val;		/* Buffer for returned data. */
	size_t		vallen;		/* Size of return data buffer. */
};

_PROTOTYPE( int svrctl, (int _request, void *_data)			);

#endif /* _SYS__SVRCTL_H */
