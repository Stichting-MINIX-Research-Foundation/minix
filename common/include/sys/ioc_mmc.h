/*	sys/ioc_mmc.h - MMC ioctl() command codes.
 *						  Author: Kees Jongenburger
 *								31 Aug 2012
 *
 */

#ifndef _S_I_MMC_H
#define _S_I_MMC_H

#include <minix/ioctl.h>

typedef uint8_t cid[16];

/* Get the card CID */
#define MMCIOC_GETCID _IOR('e',10,cid)
/* Change the log level */
#define MMCIOC_LOGLEVEL _IOW('e',11,int)
#endif /* _S_I_MMC_H */
