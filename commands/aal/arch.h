/* $Header$ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#ifndef __ARCH_H_INCLUDED
#define __ARCH_H_INCLUDED

#define	ARMAG	0177545
#define AALMAG	0177454

struct	ar_hdr {
	char	ar_name[14];
	long	ar_date;
	char	ar_uid;
	char	ar_gid;
	short	ar_mode;
	long	ar_size;
};

#define AR_TOTAL	26
#define AR_SIZE		22

extern int rd_arhdr(int fd, register struct ar_hdr arhdr[]);
extern void wr_arhdr(int fd, struct ar_hdr arhdr[]);

#endif /* __ARCH_H_INCLUDED */
