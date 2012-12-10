/*	sys/ioc_memory.h - Memory ioctl() command codes.
 *							Author: Kees J. Bot
 *								23 Nov 2002
 *
 */

#ifndef _S_I_MEMORY_H
#define _S_I_MEMORY_H

#include <minix/ioctl.h>

#define MIOCRAMSIZE	_IOW('m', 3, u32_t)
#define MIOCMAP		_IOW('m', 4, struct mapreq)
#define MIOCUNMAP	_IOW('m', 5, struct mapreq)

#endif /* _S_I_MEMORY_H */
