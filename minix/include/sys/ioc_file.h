/*	sys/ioc_file.h - File ioctl() command codes.
 */

#ifndef _SYS_IOC_FILE_H
#define _SYS_IOC_FILE_H

#include <minix/ioctl.h>
#include <sys/types.h>

#define FIONREAD	_IOR('f', 1, int)

/* mmap() of a character device.  When a process mmap()s a character device,
 * VFS sends this ioctl to the driver (with a grant to a VFS-owned struct,
 * not to the user process).  The driver either fails it (drivers that do
 * not support mmap answer ENOTTY, which the caller sees as ENODEV) or
 * returns the physical address backing the byte range [dm_offset,
 * dm_offset + dm_len).  VM maps that range into the process only if the
 * driver itself holds the privilege to access that memory (system.conf).
 */
struct devmmap {
	u_int64_t dm_offset;	/* in: device offset, page aligned */
	u_int32_t dm_len;	/* in: length in bytes, page multiple */
	u_int32_t dm_access;	/* in: DEVMMAP_R/DEVMMAP_W per the fd mode */
	u_int64_t dm_phys;	/* out: physical address of dm_offset */
};
#define DEVMMAP_R	0x1
#define DEVMMAP_W	0x2

#define FIOMMAP		_IOWR('f', 80, struct devmmap)

#endif /* _SYS_IOC_FILE_H */
