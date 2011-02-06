/*	sys/ioc_tape.h - Magnetic Tape ioctl() command codes.
 *							Author: Kees J. Bot
 *								23 Nov 2002
 *
 */

#ifndef _S_I_TAPE_H
#define _S_I_TAPE_H

#include <minix/ioctl.h>

#define MTIOCTOP	_IOW('M', 1, struct mtop)
#define MTIOCGET	_IOR('M', 2, struct mtget)

#endif /* _S_I_TAPE_H */
