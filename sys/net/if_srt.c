/* $NetBSD: if_srt.c,v 1.20 2015/08/24 22:21:26 pooka Exp $ */
/* This file is in the public domain. */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_srt.c,v 1.20 2015/08/24 22:21:26 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#if !defined(INET) && !defined(INET6)
#error "srt without INET/INET6?"
#endif

#ifndef SRT_MAXUNIT
#define SRT_MAXUNIT 255
#endif

/* include-file bug workarounds */
#include <sys/types.h>		/* sys/conf.h */
#include <sys/resource.h>	/* sys/resourcevar.h
				 * (uvm/uvm_param.h, sys/mbuf.h)
				 */
#include <netinet/in.h>		/* netinet/ip.h */
#include <sys/param.h>		/* sys/mbuf.h */
#include <netinet/in_systm.h>	/* netinet/ip.h */

#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <net/if_types.h>

#include "if_srt.h"

/* until we know what to pass to bpfattach.... */
/* #define BPFILTER_NOW_AVAILABLE */

struct srt_softc {
	struct ifnet intf;	/* XXX interface botch */
	int unit;
	int nrt;
	struct srt_rt **rts;
	unsigned int flags;	/* SSF_* values from if_srt.h */
#define SSF_UCHG (SSF_MTULOCK) /* userland-changeable bits */
	unsigned int kflags;	/* bits private to this file */
#define SKF_CDEVOPEN 0x00000001
};

void srtattach(void);

static struct srt_softc *softcv[SRT_MAXUNIT+1];
static unsigned int global_flags;

/* Internal routines. */

static unsigned int ipv4_masks[33] = {
	0x00000000, /* /0 */
	0x80000000, 0xc0000000, 0xe0000000, 0xf0000000, /* /1 - /4 */
	0xf8000000, 0xfc000000, 0xfe000000, 0xff000000, /* /5 - /8 */
	0xff800000, 0xffc00000, 0xffe00000, 0xfff00000, /* /9 - /12 */
	0xfff80000, 0xfffc0000, 0xfffe0000, 0xffff0000, /* /13 - /16 */
	0xffff8000, 0xffffc000, 0xffffe000, 0xfffff000, /* /17 - /20 */
	0xfffff800, 0xfffffc00, 0xfffffe00, 0xffffff00, /* /21 - /24 */
	0xffffff80, 0xffffffc0, 0xffffffe0, 0xfffffff0, /* /25 - /28 */
	0xfffffff8, 0xfffffffc, 0xfffffffe, 0xffffffff  /* /29 - /32 */
};

static void
update_mtu(struct srt_softc *sc)
{
	int mtu;
	int i;
	struct srt_rt *r;

	if (sc->flags & SSF_MTULOCK)
		return;
	mtu = 65535;
	for (i=sc->nrt-1;i>=0;i--) {
		r = sc->rts[i];
		if (r->u.dstifp->if_mtu < mtu)
			mtu = r->u.dstifp->if_mtu;
	}
	sc->intf.if_mtu = mtu;
}

static struct srt_rt *
find_rt(struct srt_softc *sc, int af, ...)
{
	int i;
	struct srt_rt *r;
	struct in_addr ia;
	struct in6_addr ia6;
	va_list ap;

	ia.s_addr = 0; ia6.s6_addr[0] = 0; /* shut up incorrect -Wuninitialized */
	va_start(ap,af);
	switch (af) {
	case AF_INET:
		ia = va_arg(ap,struct in_addr);
		break;
	case AF_INET6:
		ia6 = va_arg(ap,struct in6_addr);
		break;
	default:
		panic("if_srt find_rt: impossible address family");
		break;
	}
	va_end(ap);
	for (i=0; i < sc->nrt; i++) {
		r = sc->rts[i];
		if (r->af != af)
			continue;
		switch (af) {
		case AF_INET:
			if ((ia.s_addr & htonl(ipv4_masks[r->srcmask])) ==
			    r->srcmatch.v4.s_addr)
				return r;
			break;
		case AF_INET6:
			if ((r->srcmask >= 8) &&
			    memcmp(&ia6,&r->srcmatch.v6,r->srcmask / 8) != 0)
				continue;
			if ((r->srcmask % 8) &&
			    ((ia6.s6_addr[r->srcmask / 8] ^
			     r->srcmatch.v6.s6_addr[r->srcmask / 8]) &
			     0xff & (0xff00 >> (r->srcmask % 8))))
				continue;
			return r;
		default:
			panic("if_srt find_rt: impossible address family 2");
			break;
		}
	}
	return 0;
}

/* Network device interface. */

static int
srt_if_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct ifaddr *ifa;
	int s;
	int err;

	err = 0;
	s = splnet();
	switch (cmd) {
	case SIOCINITIFADDR:
		ifa = (void *) data;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			break;
#endif
#ifdef INET6
		case AF_INET6:
			break;
#endif
		default:
			err = EAFNOSUPPORT;
			break;
		}
		break;
	default:
		if ((err = ifioctl_common(ifp, cmd, data)) == ENETRESET)
			err = 0;
		break;
	}
	splx(s);
	return err;
}

static int
srt_if_output(
	struct ifnet *ifp,
	struct mbuf *m,
	const struct sockaddr *to,
	struct rtentry *rtp)
{
	struct srt_softc *sc;
	struct srt_rt *r;

	sc = ifp->if_softc;
	if (! (ifp->if_flags & IFF_UP)) {
		m_freem(m);
		return ENETDOWN;
	}
	switch (to->sa_family) {
#ifdef INET
	case AF_INET: {
		struct ip *ip;
		ip = mtod(m,struct ip *);
		r = find_rt(sc,AF_INET,ip->ip_src);
		break;
	}
#endif
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr *ip;
		ip = mtod(m,struct ip6_hdr *);
		r = find_rt(sc,AF_INET6,ip->ip6_src);
		break;
	}
#endif
	default:
		IF_DROP(&ifp->if_snd);
		m_freem(m);
		return EAFNOSUPPORT;
	}
	/* XXX Do we need to bpf_tap?  Or do higher layers now handle that? */
	/* if_gif.c seems to imply the latter. */
	ifp->if_opackets ++;
	if (! r) {
		ifp->if_oerrors ++;
		m_freem(m);
		return 0;
	}
	if (! (m->m_flags & M_PKTHDR)) {
		printf("srt_if_output no PKTHDR\n");
		m_freem(m);
		return 0;
	}
	ifp->if_obytes += m->m_pkthdr.len;
	if (! (r->u.dstifp->if_flags & IFF_UP)) {
		m_freem(m);
		return 0; /* XXX ENETDOWN? */
	}
	/* XXX is 0 the right last arg here? */
	return (*r->u.dstifp->if_output)(r->u.dstifp,m,&r->dst.sa,0);
}

static int
srt_clone_create(struct if_clone *cl, int unit)
{
	struct srt_softc *sc;

	if (unit < 0 || unit > SRT_MAXUNIT)
		return ENXIO;
	if (softcv[unit])
		return EBUSY;
	sc = malloc(sizeof(struct srt_softc), M_DEVBUF, M_WAITOK|M_ZERO);
	sc->unit = unit;
	sc->nrt = 0;
	sc->rts = 0;
	sc->flags = 0;
	sc->kflags = 0;
	if_initname(&sc->intf,cl->ifc_name,unit);
	sc->intf.if_softc = sc;
	sc->intf.if_mtu = 65535;
	sc->intf.if_flags = IFF_POINTOPOINT;
	sc->intf.if_type = IFT_OTHER;
	sc->intf.if_ioctl = &srt_if_ioctl;
	sc->intf.if_output = &srt_if_output;
	sc->intf.if_dlt = DLT_RAW;
	if_attach(&sc->intf);
	if_alloc_sadl(&sc->intf);
#ifdef BPFILTER_NOW_AVAILABLE
	bpf_attach(&sc->intf, 0, 0);
#endif
	softcv[unit] = sc;
	return 0;
}

static int
srt_clone_destroy(struct ifnet *ifp)
{
	struct srt_softc *sc;

	sc = ifp->if_softc;
	if ((ifp->if_flags & IFF_UP) || (sc->kflags & SKF_CDEVOPEN))
		return EBUSY;
#ifdef BPFILTER_NOW_AVAILABLE
	bpf_detach(ifp);
#endif
	if_detach(ifp);
	if (sc->unit < 0 || sc->unit > SRT_MAXUNIT) {
		panic("srt_clone_destroy: impossible unit %d\n",sc->unit);
	}
	if (softcv[sc->unit] != sc) {
		panic("srt_clone_destroy: bad backpointer ([%d]=%p not %p)\n",
		sc->unit,(void *)softcv[sc->unit],(void *)sc);
	}
	softcv[sc->unit] = 0;
	free(sc,M_DEVBUF);
	return 0;
}

struct if_clone srt_clone =
    IF_CLONE_INITIALIZER("srt",&srt_clone_create,&srt_clone_destroy);

void
srtattach(void)
{
	int i;

	for (i=SRT_MAXUNIT;i>=0;i--)
		softcv[i] = 0;
	global_flags = 0;
	if_clone_attach(&srt_clone);
}

/* Special-device interface. */

static int
srt_open(dev_t dev, int flag, int mode, struct lwp *l)
{
	int unit;
	struct srt_softc *sc;

	unit = minor(dev);
	if (unit < 0 || unit > SRT_MAXUNIT)
		return ENXIO;
	sc = softcv[unit];
	if (! sc)
		return ENXIO;
	sc->kflags |= SKF_CDEVOPEN;
	return 0;
}

static int
srt_close(dev_t dev, int flag, int mode, struct lwp *l)
{
	int unit;
	struct srt_softc *sc;

	unit = minor(dev);
	if (unit < 0 || unit > SRT_MAXUNIT)
		return ENXIO;
	sc = softcv[unit];
	if (! sc)
		return ENXIO;
	sc->kflags &= ~SKF_CDEVOPEN;
	return 0;
}

static int
srt_ioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	unsigned int f, i, n, o;
	struct srt_softc *sc;
	struct srt_rt *dr;
	struct srt_rt *scr;
	struct ifnet *ifp;
	char nbuf[IFNAMSIZ];

	sc = softcv[minor(dev)];
	if (! sc)
		panic("srt_ioctl: softc disappeared");
	switch (cmd) {
	case SRT_GETNRT:
		if (! (flag & FREAD))
			return EBADF;
		*(unsigned int *)data = sc->nrt;
		return 0;
	case SRT_GETRT:
		if (! (flag & FREAD))
			return EBADF;
		dr = (struct srt_rt *) data;
		if (dr->inx >= sc->nrt)
			return EDOM;
		scr = sc->rts[dr->inx];
		dr->af = scr->af;
		dr->srcmatch = scr->srcmatch;
		dr->srcmask = scr->srcmask;
		strncpy(&dr->u.dstifn[0],&scr->u.dstifp->if_xname[0],IFNAMSIZ);
		memcpy(&dr->dst,&scr->dst,scr->dst.sa.sa_len);
		return 0;
	case SRT_SETRT:
		if (! (flag & FWRITE))
			return EBADF;
		dr = (struct srt_rt *) data;
		if (dr->inx > sc->nrt)
			return EDOM;
		strncpy(&nbuf[0],&dr->u.dstifn[0],IFNAMSIZ);
		nbuf[IFNAMSIZ-1] = '\0';
		if (dr->dst.sa.sa_family != dr->af)
			return EIO;
		switch (dr->af) {
#ifdef INET
		case AF_INET:
			if (dr->dst.sa.sa_len != sizeof(dr->dst.sin))
				return EIO;
			if (dr->srcmask > 32)
				return EIO;
		break;
#endif
#ifdef INET6
		case AF_INET6:
			if (dr->dst.sa.sa_len != sizeof(dr->dst.sin6))
				return EIO;
			if (dr->srcmask > 128)
				return EIO;
		break;
#endif
		default:
			return EAFNOSUPPORT;
		}
		ifp = ifunit(&nbuf[0]);
		if (ifp == 0)
			return ENXIO; /* needs translation */
		if (dr->inx == sc->nrt) {
			struct srt_rt **tmp;
			tmp = malloc((sc->nrt+1)*sizeof(*tmp), M_DEVBUF,
			    M_WAITOK);
			if (tmp == 0)
				return ENOBUFS;
			tmp[sc->nrt] = 0;
			if (sc->nrt > 0) {
				memcpy(tmp, sc->rts, sc->nrt*sizeof(*tmp));
				free(sc->rts, M_DEVBUF);
			}
			sc->rts = tmp;
			sc->nrt ++;
		}
		scr = sc->rts[dr->inx];
		if (scr == 0) {
			scr = malloc(sizeof(struct srt_rt),M_DEVBUF,M_WAITOK);
			if (scr == 0)
				return ENOBUFS;
			scr->inx = dr->inx;
			scr->af = AF_UNSPEC;
			sc->rts[dr->inx] = scr;
		}
		scr->af = dr->af;
		scr->srcmatch = dr->srcmatch;
		scr->srcmask = dr->srcmask;
		scr->u.dstifp = ifp;
		memcpy(&scr->dst,&dr->dst,dr->dst.sa.sa_len);
		update_mtu(sc);
		return 0;
	case SRT_DELRT:
		if (! (flag & FWRITE))
			return EBADF;
		i = *(unsigned int *)data;
		if (i >= sc->nrt)
			return EDOM;
		scr = sc->rts[i];
		sc->rts[i] = 0;
		free(scr, M_DEVBUF);
		sc->nrt--;
		if (i < sc->nrt) {
			memcpy(sc->rts+i, sc->rts+i+1,
			    (sc->nrt-i)*sizeof(*sc->rts));
		}
		if (sc->nrt == 0) {
			free(sc->rts, M_DEVBUF);
			sc->rts = 0;
			sc->intf.if_flags &= ~IFF_UP;
		}
		update_mtu(sc);
		return 0;
	case SRT_SFLAGS:
		if (! (flag & FWRITE))
			return EBADF;
		f = *(unsigned int *)data & SSF_UCHG;
		global_flags = (global_flags & ~SSF_UCHG) | (f & SSF_GLOBAL);
		sc->flags = (sc->flags & ~SSF_UCHG) | (f & ~SSF_GLOBAL);
		return 0;
	case SRT_GFLAGS:
		if (! (flag & FREAD))
			return EBADF;
		*(unsigned int *)data = sc->flags | global_flags;
		return 0;
	case SRT_SGFLAGS:
		if ((flag & (FWRITE|FREAD)) != (FWRITE|FREAD))
			return EBADF;
		o = sc->flags | global_flags;
		n = *(unsigned int *)data & SSF_UCHG;
		global_flags = (global_flags & ~SSF_UCHG) | (n & SSF_GLOBAL);
		sc->flags = (sc->flags & ~SSF_UCHG) | (n & ~SSF_GLOBAL);
		*(unsigned int *)data = o;
		return 0;
	case SRT_DEBUG:
		return 0;
		break;
	}
	return ENOTTY;
}

const struct cdevsw srt_cdevsw = {
	.d_open = srt_open,
	.d_close = srt_close,
	.d_read = nullread,
	.d_write = nullwrite,
	.d_ioctl = srt_ioctl,
	.d_stop = nullstop,
	.d_tty = notty,
	.d_poll = nullpoll,
	.d_mmap = nommap,
	.d_kqfilter = nullkqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};
