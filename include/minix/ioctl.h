/*	minix/ioctl.h - Ioctl helper definitions.	Author: Kees J. Bot
 *								23 Nov 2002
 *
 * This file is included by every header file that defines ioctl codes.
 */

#ifndef _M_IOCTL_H
#define _M_IOCTL_H

#include <sys/cdefs.h>
#include <sys/types.h>

/* Ioctls have the command encoded in the low-order word, and the size
 * of the parameter in the high-order word. The 3 high bits of the high-
 * order word are used to encode the in/out/void status of the parameter.
 */
#define _IOCPARM_MASK		0x0FFF
#define _IOCPARM_MASK_BIG	0x0FFFFF
#define _IOC_VOID		0x20000000
#define _IOCTYPE_MASK		0xFFFF
#define _IOC_IN			0x40000000
#define _IOC_OUT		0x80000000
#define _IOC_INOUT		(_IOC_IN | _IOC_OUT)

/* Flag indicating ioctl format with only one type field, and more bits
 * for the size field (using mask _IOCPARM_MASK_BIG).
 */
#define _IOC_BIG		0x10000000

#define _IO(x,y)	((x << 8) | y | _IOC_VOID)
#define _IOR(x,y,t)	((x << 8) | y | ((sizeof(t) & _IOCPARM_MASK) << 16) |\
				_IOC_OUT)
#define _IOW(x,y,t)	((x << 8) | y | ((sizeof(t) & _IOCPARM_MASK) << 16) |\
				_IOC_IN)
#define _IORW(x,y,t)	((x << 8) | y | ((sizeof(t) & _IOCPARM_MASK) << 16) |\
				_IOC_INOUT)

#define _IOW_BIG(y,t)  (y | ((sizeof(t) & _IOCPARM_MASK_BIG) << 8) \
	| _IOC_IN | _IOC_BIG)
#define _IOR_BIG(y,t)  (y | ((sizeof(t) & _IOCPARM_MASK_BIG) << 8) \
	| _IOC_OUT | _IOC_BIG)
#define _IORW_BIG(y,t) (y | ((sizeof(t) & _IOCPARM_MASK_BIG) << 8) \
	| _IOC_INOUT | _IOC_BIG)

/* Decode an ioctl call. */
#define _MINIX_IOCTL_SIZE(i)		(((i) >> 16) & _IOCPARM_MASK)
#define _MINIX_IOCTL_IOR(i)		((i) & _IOC_OUT)
#define _MINIX_IOCTL_IORW(i)		((i) & _IOC_INOUT)
#define _MINIX_IOCTL_IOW(i)		((i) & _IOC_IN)

/* Recognize and decode size of a 'big' ioctl call. */
#define _MINIX_IOCTL_BIG(i) 		((i) & _IOC_BIG)
#define _MINIX_IOCTL_SIZE_BIG(i)	(((i) >> 8) & _IOCPARM_MASK_BIG)

__BEGIN_DECLS
int ioctl(int _fd, int _request, void *_data);
__END_DECLS

#endif /* _M_IOCTL_H */
