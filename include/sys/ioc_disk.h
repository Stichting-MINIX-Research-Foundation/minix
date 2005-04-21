/*	sys/ioc_disk.h - Disk ioctl() command codes.	Author: Kees J. Bot
 *								23 Nov 2002
 *
 */

#ifndef _S_I_DISK_H
#define _S_I_DISK_H

#include <minix/ioctl.h>

#define DIOCSETP	_IOW('d', 3, struct partition)
#define DIOCGETP	_IOR('d', 4, struct partition)
#define DIOCEJECT	_IO ('d', 5)

#endif /* _S_I_DISK_H */
