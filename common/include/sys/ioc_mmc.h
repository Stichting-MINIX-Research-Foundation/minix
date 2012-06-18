/*	sys/ioc_mmc.h - MMC ioctl() command codes.
 *						  Author: Kees Jongenburger
 *								31 Aug 2012
 *
 */

#ifndef _S_I_MMC_H
#define _S_I_MMC_H

#include <minix/ioctl.h>

typedef uint8_t cid[16]; 

/* get the card CID */
#define MMCIOGETCID _IOR('e',10,cid)

#endif /* _S_I_MMC_H */
