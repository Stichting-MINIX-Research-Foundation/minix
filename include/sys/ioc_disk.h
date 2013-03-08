/*	sys/ioc_disk.h - Disk ioctl() command codes.	Author: Kees J. Bot
 *								23 Nov 2002
 *
 */

#ifndef _S_I_DISK_H
#define _S_I_DISK_H

#include <minix/ioctl.h>

#define DIOCSETP	_IOW('d', 3, struct part_geom)
#define DIOCGETP	_IOR('d', 4, struct part_geom)
#define DIOCEJECT	_IO ('d', 5)
#define DIOCTIMEOUT	_IORW('d', 6, int)
#define DIOCOPENCT	_IOR('d', 7, int)
#define DIOCFLUSH	_IO ('d', 8)
#define DIOCSETWC	_IOW('d', 9, int)
#define DIOCGETWC	_IOR('d', 10, int)

#endif /* _S_I_DISK_H */
