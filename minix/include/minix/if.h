#ifndef _MINIX_IF_H
#define _MINIX_IF_H

#include <net/if.h>
#include <net/if_media.h>

/*
 * MINIX3-specific extensions to the network interface headers.  These
 * extensions are necessary because NetBSD IF uses a few ioctl(2) structure
 * formats that contain pointers--something that MINIX3 has to avoid, due to
 * its memory granting mechanisms.  Thus, those ioctl(2) calls have to be
 * converted from NetBSD to MINIX3 format.  We currently do that in libc.
 * This header specifies the numbers and formats for the MINIX3 versions.
 *
 * The general idea is that we rewrite the ioctl request data to include both
 * the original structure and a buffer for the array of values to which the
 * original structure uses a pointer.  Important: in those cases, the original
 * structure is expected to be the first element of the replacement structure.
 *
 * There is typically no configured upper bound for the maximum number of
 * values in the array, and so we pick size values that are hopefully always
 * oversized and yet keep the ioctl sizes within the range of regular ioctls
 * (4095 bytes, as per sys/ioccom.h).  If there may be larger amounts of data,
 * we have to use "big" ioctls.
 *
 * For the replacement ioctl codes, we use the original ioctl class and number
 * with a different size.  That should virtually eliminate the possibility of
 * accidental collisions.
 */

/* SIOCGIFMEDIA: retrieve interface media status and types. */
#define MINIX_IF_MAXMEDIA	256

struct minix_ifmediareq {
	struct ifmediareq mifm_ifm;			/* MUST be first */
	int mifm_list[MINIX_IF_MAXMEDIA];
};

#define MINIX_SIOCGIFMEDIA	_IOWR('i', 54, struct minix_ifmediareq)

/* SIOCIFGCLONERS: retrieve interface "cloners" (virtual types). */
#define MINIX_IF_MAXCLONERS	128

struct minix_if_clonereq {
	struct if_clonereq mifcr_ifcr;			/* MUST be first */
	char mifcr_buffer[MINIX_IF_MAXCLONERS * IFNAMSIZ];
};

#define MINIX_SIOCIFGCLONERS	_IOWR('i', 120, struct minix_if_clonereq)

#endif /* !_MINIX_IF_H */
