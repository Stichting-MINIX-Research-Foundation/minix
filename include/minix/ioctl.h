/*	minix/ioctl.h - Ioctl helper definitions.	Author: Kees J. Bot
 *								23 Nov 2002
 *
 * This file is included by every header file that defines ioctl codes.
 */

#ifndef _M_IOCTL_H
#define _M_IOCTL_H

#ifndef _TYPES_H
#include <sys/types.h>
#endif

#if _EM_WSIZE >= 4
/* Ioctls have the command encoded in the low-order word, and the size
 * of the parameter in the high-order word. The 3 high bits of the high-
 * order word are used to encode the in/out/void status of the parameter.
 */
#define _IOCPARM_MASK	0x1FFF
#define _IOC_VOID	0x20000000
#define _IOCTYPE_MASK	0xFFFF
#define _IOC_IN		0x40000000
#define _IOC_OUT	0x80000000
#define _IOC_INOUT	(_IOC_IN | _IOC_OUT)

#define _IO(x,y)	((x << 8) | y | _IOC_VOID)
#define _IOR(x,y,t)	((x << 8) | y | ((sizeof(t) & _IOCPARM_MASK) << 16) |\
				_IOC_OUT)
#define _IOW(x,y,t)	((x << 8) | y | ((sizeof(t) & _IOCPARM_MASK) << 16) |\
				_IOC_IN)
#define _IORW(x,y,t)	((x << 8) | y | ((sizeof(t) & _IOCPARM_MASK) << 16) |\
				_IOC_INOUT)
#else
/* No fancy encoding on a 16-bit machine. */

#define _IO(x,y)	((x << 8) | y)
#define _IOR(x,y,t)	_IO(x,y)
#define _IOW(x,y,t)	_IO(x,y)
#define _IORW(x,y,t)	_IO(x,y)
#endif

int ioctl(int _fd, int _request, void *_data);

#endif /* _M_IOCTL_H */
