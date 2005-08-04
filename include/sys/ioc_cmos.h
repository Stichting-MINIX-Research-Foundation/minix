/*	sys/ioc_cmos.h - CMOS ioctl() command codes.
 */

#ifndef _S_I_CMOS_H
#define _S_I_CMOS_H

#include <minix/ioctl.h>

#define CIOCGETTIME	_IOR('c', 1, u32_t)
#define CIOCGETTIMEY2K	_IOR('c', 2, u32_t)
#define CIOCSETTIME	_IOW('c', 3, u32_t)
#define CIOCSETTIMEY2K	_IOW('c', 4, u32_t)

#endif /* _S_I_CMOS_H */

