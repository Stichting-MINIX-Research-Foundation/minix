/*	sys/ioc_memory.h - Memory ioctl() command codes.
 *							Author: Kees J. Bot
 *								23 Nov 2002
 *
 */

#ifndef _S_I_MEMORY_H
#define _S_I_MEMORY_H

#include <minix/ioctl.h>

#define MIOCRAMSIZE	_IOW('m', 3, u32_t)
#define MIOCSPSINFO	_IOW('m', 4, void *)
#define MIOCGPSINFO	_IOR('m', 5, struct psinfo)
#define MIOCINT86	_IORW('m', 6, struct mio_int86)
#define MIOCGLDT86	_IORW('m', 7, struct mio_ldt86)
#define MIOCSLDT86	_IOW('m', 8, struct mio_ldt86)

#endif /* _S_I_MEMORY_H */
