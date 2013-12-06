/*	$NetBSD: conf.h,v 1.144 2012/10/27 17:18:40 chs Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)conf.h	8.5 (Berkeley) 1/9/95
 */

#ifndef _SYS_CONF_H_
#define _SYS_CONF_H_

/*
 * Definitions of device driver entry switches
 */

#include <sys/queue.h>
#include <sys/device_if.h>

struct buf;
struct knote;
struct lwp;
struct tty;
struct uio;
struct vnode;

/*
 * Types for d_flag
 */
#define D_OTHER		0x0000
#define	D_TAPE		0x0001
#define	D_DISK		0x0002
#define	D_TTY		0x0003
#define	D_TYPEMASK	0x00ff
#define	D_MPSAFE	0x0100
#define	D_NEGOFFSAFE	0x0200

/*
 * Block device switch table
 */
struct bdevsw {
	int		(*d_open)(dev_t, int, int, struct lwp *);
	int		(*d_close)(dev_t, int, int, struct lwp *);
	void		(*d_strategy)(struct buf *);
	int		(*d_ioctl)(dev_t, u_long, void *, int, struct lwp *);
	int		(*d_dump)(dev_t, daddr_t, void *, size_t);
	int		(*d_psize)(dev_t);
	int		d_flag;
};

/*
 * Character device switch table
 */
struct cdevsw {
	int		(*d_open)(dev_t, int, int, struct lwp *);
	int		(*d_close)(dev_t, int, int, struct lwp *);
	int		(*d_read)(dev_t, struct uio *, int);
	int		(*d_write)(dev_t, struct uio *, int);
	int		(*d_ioctl)(dev_t, u_long, void *, int, struct lwp *);
	void		(*d_stop)(struct tty *, int);
	struct tty *	(*d_tty)(dev_t);
	int		(*d_poll)(dev_t, int, struct lwp *);
	paddr_t		(*d_mmap)(dev_t, off_t, int);
	int		(*d_kqfilter)(dev_t, struct knote *);
	int		d_flag;
};

#ifdef _KERNEL

#include <sys/mutex.h>
extern kmutex_t device_lock;

int devsw_attach(const char *, const struct bdevsw *, devmajor_t *,
		 const struct cdevsw *, devmajor_t *);
int devsw_detach(const struct bdevsw *, const struct cdevsw *);
const struct bdevsw *bdevsw_lookup(dev_t);
const struct cdevsw *cdevsw_lookup(dev_t);
devmajor_t bdevsw_lookup_major(const struct bdevsw *);
devmajor_t cdevsw_lookup_major(const struct cdevsw *);

#define	dev_type_open(n)	int n (dev_t, int, int, struct lwp *)
#define	dev_type_close(n)	int n (dev_t, int, int, struct lwp *)
#define	dev_type_read(n)	int n (dev_t, struct uio *, int)
#define	dev_type_write(n)	int n (dev_t, struct uio *, int)
#define	dev_type_ioctl(n) \
		int n (dev_t, u_long, void *, int, struct lwp *)
#define	dev_type_stop(n)	void n (struct tty *, int)
#define	dev_type_tty(n)		struct tty * n (dev_t)
#define	dev_type_poll(n)	int n (dev_t, int, struct lwp *)
#define	dev_type_mmap(n)	paddr_t n (dev_t, off_t, int)
#define	dev_type_strategy(n)	void n (struct buf *)
#define	dev_type_dump(n)	int n (dev_t, daddr_t, void *, size_t)
#define	dev_type_size(n)	int n (dev_t)
#define	dev_type_kqfilter(n)	int n (dev_t, struct knote *)

#define	noopen		((dev_type_open((*)))enodev)
#define	noclose		((dev_type_close((*)))enodev)
#define	noread		((dev_type_read((*)))enodev)
#define	nowrite		((dev_type_write((*)))enodev)
#define	noioctl		((dev_type_ioctl((*)))enodev)
#define	nostop		((dev_type_stop((*)))enodev)
#define	notty		NULL
#define	nopoll		seltrue
#define	nommap		((dev_type_mmap((*)))enodev)
#define	nodump		((dev_type_dump((*)))enodev)
#define	nosize		NULL
#define	nokqfilter	seltrue_kqfilter

#define	nullopen	((dev_type_open((*)))nullop)
#define	nullclose	((dev_type_close((*)))nullop)
#define	nullread	((dev_type_read((*)))nullop)
#define	nullwrite	((dev_type_write((*)))nullop)
#define	nullioctl	((dev_type_ioctl((*)))nullop)
#define	nullstop	((dev_type_stop((*)))nullop)
#define	nullpoll	((dev_type_poll((*)))nullop)
#define	nullmmap	((dev_type_mmap((*)))nullop)
#define	nulldump	((dev_type_dump((*)))nullop)
#define	nullkqfilter	((dev_type_kqfilter((*)))eopnotsupp)

/* device access wrappers. */

dev_type_open(bdev_open);
dev_type_close(bdev_close);
dev_type_strategy(bdev_strategy);
dev_type_ioctl(bdev_ioctl);
dev_type_dump(bdev_dump);
dev_type_size(bdev_size);

dev_type_open(cdev_open);
dev_type_close(cdev_close);
dev_type_read(cdev_read);
dev_type_write(cdev_write);
dev_type_ioctl(cdev_ioctl);
dev_type_stop(cdev_stop);
dev_type_tty(cdev_tty);
dev_type_poll(cdev_poll);
dev_type_mmap(cdev_mmap);
dev_type_kqfilter(cdev_kqfilter);

int	cdev_type(dev_t);
int	bdev_type(dev_t);

/* symbolic sleep message strings */
extern	const char devopn[], devio[], devwait[], devin[], devout[];
extern	const char devioc[], devcls[];

#endif /* _KERNEL */

/*
 * Line discipline switch table
 */
struct linesw {
	const char *l_name;	/* Linesw name */

	LIST_ENTRY(linesw) l_list;
	u_int	l_refcnt;	/* locked by ttyldisc_list_slock */
	int	l_no;		/* legacy discipline number (for TIOCGETD) */

	int	(*l_open)	(dev_t, struct tty *);
	int	(*l_close)	(struct tty *, int);
	int	(*l_read)	(struct tty *, struct uio *, int);
	int	(*l_write)	(struct tty *, struct uio *, int);
	int	(*l_ioctl)	(struct tty *, u_long, void *, int,
				    struct lwp *);
	int	(*l_rint)	(int, struct tty *);
	int	(*l_start)	(struct tty *);
	int	(*l_modem)	(struct tty *, int);
	int	(*l_poll)	(struct tty *, int, struct lwp *);
};

#ifdef _KERNEL
void	       ttyldisc_init(void);
int	       ttyldisc_attach(struct linesw *);
int	       ttyldisc_detach(struct linesw *);
struct linesw *ttyldisc_lookup(const char *);
struct linesw *ttyldisc_lookup_bynum(int);
struct linesw *ttyldisc_default(void);
void	       ttyldisc_release(struct linesw *);

/* For those defining their own line disciplines: */
#define	ttynodisc ((int (*)(dev_t, struct tty *))enodev)
#define	ttyerrclose ((int (*)(struct tty *, int))enodev)
#define	ttyerrio ((int (*)(struct tty *, struct uio *, int))enodev)
#define	ttyerrinput ((int (*)(int, struct tty *))enodev)
#define	ttyerrstart ((int (*)(struct tty *))enodev)

int	ttyerrpoll (struct tty *, int, struct lwp *);
int	ttynullioctl(struct tty *, u_long, void *, int, struct lwp *);

int	iskmemdev(dev_t);
int	seltrue_kqfilter(dev_t, struct knote *);
#endif

#ifdef _KERNEL

#define	DEV_MEM		0	/* minor device 0 is physical memory */
#define	DEV_KMEM	1	/* minor device 1 is kernel memory */
#define	DEV_NULL	2	/* minor device 2 is EOF/rathole */
#ifdef COMPAT_16
#define	_DEV_ZERO_oARM	3	/* reserved: old ARM /dev/zero minor */
#endif
#define	DEV_ZERO	12	/* minor device 12 is '\0'/rathole */

enum devnode_class {
	DEVNODE_DONTBOTHER,
	DEVNODE_SINGLE,
	DEVNODE_VECTOR,
};
#define DEVNODE_FLAG_LINKZERO	0x01	/* create name -> name0 link */
#define DEVNODE_FLAG_ISMINOR0	0x02	/* vector[0] specifies minor */
#ifdef notyet
#define DEVNODE_FLAG_ISMINOR1	0x04	/* vector[1] specifies starting minor */
#endif

struct devsw_conv {
	const char *d_name;
	devmajor_t d_bmajor;
	devmajor_t d_cmajor;

	/* information about /dev nodes related to the device */
	enum devnode_class d_class;
	int d_flags;
	int d_vectdim[2];
};

void devsw_init(void);
const char *devsw_blk2name(devmajor_t);
const char *cdevsw_getname(devmajor_t);
const char *bdevsw_getname(devmajor_t);
devmajor_t devsw_name2blk(const char *, char *, size_t);
devmajor_t devsw_name2chr(const char *, char *, size_t);
dev_t devsw_chr2blk(dev_t);
dev_t devsw_blk2chr(dev_t);

void mm_init(void);
#endif /* _KERNEL */

#ifdef _KERNEL
void	setroot(device_t, int);
void	rootconf(void);
void	swapconf(void);
#endif /* _KERNEL */

#endif /* !_SYS_CONF_H_ */
