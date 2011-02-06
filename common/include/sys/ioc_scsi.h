/*	sys/ioc_scsi.h - SCSI ioctl() command codes.	Author: Kees J. Bot
 *								23 Nov 2002
 *
 */

#ifndef _S_I_SCSI_H
#define _S_I_SCSI_H

#include <minix/ioctl.h>

#define SCIOCCMD	_IOW('S', 1, struct scsicmd)

#endif /* _S_I_SCSI_H */
