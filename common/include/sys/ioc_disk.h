/*	sys/ioc_disk.h - Disk ioctl() command codes.	Author: Kees J. Bot
 *								23 Nov 2002
 *
 */

#ifndef _S_I_DISK_H
#define _S_I_DISK_H

#include <minix/ioctl.h>

/*
 * Disk drivers should implement the following ioctl requests:
 *   DIOCGETP, DIOCSETP, DIOCFLUSH, DIOCOPENCT.
 *
 * Disk drivers may implement the following ioctl requests:
 *  DIOCGETWC, DIOCSETWC, DIOCEJECT, DIOCTIMEOUT.
 *
 * DIOCSETP
 * 	The DIOCSETP ioctl sets the base and size of a subdevice. its effects
 * 	are temporary and in-memory only. the call makes use of a struct
 * 	partition structure that is defined in <minix/partition.h>.
 *
 * DIOCGETP
 *  DIOCGETP ioctl may be used to obtain the base, size, and geometry of
 *  a (sub)device. Geometry data may be faked if the device does not have
 *  real geometry.the call makes use of a struct partition structure that
 *  is defined in <minix/partition.h>.
 *
 * DIOCEJECT
 *  The DIOCEJECT ioctl tells the driver to eject the medium from a device,
 *  if possible. There is no matching call to request the device to load a
 *  medium.
 *
 * DIOCTIMEOUT
 *  The DIOCTIMEOUT ioctl sets the driver's command timeout, in clock ticks.
 *  The previous command timeout is copied back to the caller. A timeout of 0
 *  signifies the driver default timeout. It is up to the driver to decide
 *  whether this is a device-specific operation or not.
 *
 * DIOCOPENCT
 *  The DIOCOPENCT ioctl allows applications to request the open count of a
 *  particular device. An integer that contains the open count of the device
 *  is copied to the caller.
 *
 * DIOCFLUSH
 *  The DIOCFLUSH ioctl tells a device to flush its write cache, returning
 *  only once this operation has completed.
 *
 * DIOCSETWC
 *  The DIOCSETWC ioctl disables or enables the device's write cache, depending
 *  on whether the integer value passed in is zero or nonzero, respectively. On
 *  some devices, disabling the write cache also invokes a cache flush.
 *
 * DIOCSETWC
 *  The DIOCGETWC ioctl retrieves the current state of the device's write cache,
 *  copying back a value of 1 if it is enabled and a value of 0 if it is disabled.
 */
#define DIOCSETP	_IOW('d', 3, struct partition)
#define DIOCGETP	_IOR('d', 4, struct partition)
#define DIOCEJECT	_IO ('d', 5)
#define DIOCTIMEOUT	_IORW('d', 6, int)
#define DIOCOPENCT	_IOR('d', 7, int)
#define DIOCFLUSH	_IO ('d', 8)
#define DIOCSETWC	_IOW('d', 9, int)
#define DIOCGETWC	_IOR('d', 10, int)

#endif /* _S_I_DISK_H */
