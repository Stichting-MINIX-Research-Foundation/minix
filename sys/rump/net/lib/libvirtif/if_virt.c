/*	$NetBSD: if_virt.c,v 1.49 2014/11/06 23:25:16 pooka Exp $	*/

/*
 * Copyright (c) 2008, 2013 Antti Kantee.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_virt.c,v 1.49 2014/11/06 23:25:16 pooka Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/cprng.h>
#include <sys/module.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>

#include <netinet/in.h>
#include <netinet/in_var.h>

#include "if_virt.h"
#include "virtif_user.h"

/*
 * Virtual interface.  Uses hypercalls to shovel packets back
 * and forth.  The exact method for shoveling depends on the
 * hypercall implementation.
 */

static int	virtif_init(struct ifnet *);
static int	virtif_ioctl(struct ifnet *, u_long, void *);
static void	virtif_start(struct ifnet *);
static void	virtif_stop(struct ifnet *, int);

struct virtif_sc {
	struct ethercom sc_ec;
	struct virtif_user *sc_viu;

	int sc_num;
	char *sc_linkstr;
};

static int  virtif_clone(struct if_clone *, int);
static int  virtif_unclone(struct ifnet *);

struct if_clone VIF_CLONER =
    IF_CLONE_INITIALIZER(VIF_NAME, virtif_clone, virtif_unclone);

static int
virtif_create(struct ifnet *ifp)
{
	uint8_t enaddr[ETHER_ADDR_LEN] = { 0xb2, 0x0a, 0x00, 0x0b, 0x0e, 0x01 };
	char enaddrstr[3*ETHER_ADDR_LEN];
	struct virtif_sc *sc = ifp->if_softc;
	int error;

	if (sc->sc_viu)
		panic("%s: already created", ifp->if_xname);

	enaddr[2] = cprng_fast32() & 0xff;
	enaddr[5] = sc->sc_num & 0xff;

	if ((error = VIFHYPER_CREATE(sc->sc_linkstr,
	    sc, enaddr, &sc->sc_viu)) != 0) {
		printf("VIFHYPER_CREATE failed: %d\n", error);
		return error;
	}

	ether_ifattach(ifp, enaddr);
	ether_snprintf(enaddrstr, sizeof(enaddrstr), enaddr);
	aprint_normal_ifnet(ifp, "Ethernet address %s\n", enaddrstr);

	IFQ_SET_READY(&ifp->if_snd);

	return 0;
}

static int
virtif_clone(struct if_clone *ifc, int num)
{
	struct virtif_sc *sc;
	struct ifnet *ifp;
	int error = 0;

	sc = kmem_zalloc(sizeof(*sc), KM_SLEEP);
	sc->sc_num = num;
	ifp = &sc->sc_ec.ec_if;

	if_initname(ifp, VIF_NAME, num);
	ifp->if_softc = sc;

	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = virtif_init;
	ifp->if_ioctl = virtif_ioctl;
	ifp->if_start = virtif_start;
	ifp->if_stop = virtif_stop;
	ifp->if_mtu = ETHERMTU;
	ifp->if_dlt = DLT_EN10MB;

	if_attach(ifp);

#ifndef RUMP_VIF_LINKSTR
	/*
	 * if the underlying interface does not expect linkstr, we can
	 * create everything now.  Otherwise, we need to wait for
	 * SIOCSLINKSTR.
	 */
#define LINKSTRNUMLEN 16
	sc->sc_linkstr = kmem_alloc(LINKSTRNUMLEN, KM_SLEEP);
	snprintf(sc->sc_linkstr, LINKSTRNUMLEN, "%d", sc->sc_num);
#undef LINKSTRNUMLEN
	error = virtif_create(ifp);
	if (error) {
		if_detach(ifp);
		kmem_free(sc, sizeof(*sc));
		ifp->if_softc = NULL;
	}
#endif /* !RUMP_VIF_LINKSTR */

	return error;
}

static int
virtif_unclone(struct ifnet *ifp)
{
	struct virtif_sc *sc = ifp->if_softc;
	int rv;

	if (ifp->if_flags & IFF_UP)
		return EBUSY;

	if ((rv = VIFHYPER_DYING(sc->sc_viu)) != 0)
		return rv;

	virtif_stop(ifp, 1);
	if_down(ifp);

	VIFHYPER_DESTROY(sc->sc_viu);

	kmem_free(sc, sizeof(*sc));

	ether_ifdetach(ifp);
	if_detach(ifp);

	return 0;
}

static int
virtif_init(struct ifnet *ifp)
{
	struct virtif_sc *sc = ifp->if_softc;

	if (sc->sc_viu == NULL)
		return ENXIO;

	ifp->if_flags |= IFF_RUNNING;
	return 0;
}

static int
virtif_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct virtif_sc *sc = ifp->if_softc;
	int rv;

	switch (cmd) {
#ifdef RUMP_VIF_LINKSTR
	struct ifdrv *ifd;
	size_t linkstrlen;

#ifndef RUMP_VIF_LINKSTRMAX
#define RUMP_VIF_LINKSTRMAX 4096
#endif

	case SIOCGLINKSTR:
		ifd = data;

		if (!sc->sc_linkstr) {
			rv = ENOENT;
			break;
		}
		linkstrlen = strlen(sc->sc_linkstr)+1;

		if (ifd->ifd_cmd == IFLINKSTR_QUERYLEN) {
			ifd->ifd_len = linkstrlen;
			rv = 0;
			break;
		}
		if (ifd->ifd_cmd != 0) {
			rv = ENOTTY;
			break;
		}

		rv = copyoutstr(sc->sc_linkstr,
		    ifd->ifd_data, MIN(ifd->ifd_len,linkstrlen), NULL);
		break;
	case SIOCSLINKSTR:
		if (ifp->if_flags & IFF_UP) {
			rv = EBUSY;
			break;
		}

		ifd = data;

		if (ifd->ifd_cmd == IFLINKSTR_UNSET) {
			panic("unset linkstr not implemented");
		} else if (ifd->ifd_cmd != 0) {
			rv = ENOTTY;
			break;
		} else if (sc->sc_linkstr) {
			rv = EBUSY;
			break;
		}

		if (ifd->ifd_len > RUMP_VIF_LINKSTRMAX) {
			rv = E2BIG;
			break;
		} else if (ifd->ifd_len < 1) {
			rv = EINVAL;
			break;
		}


		sc->sc_linkstr = kmem_alloc(ifd->ifd_len, KM_SLEEP);
		rv = copyinstr(ifd->ifd_data, sc->sc_linkstr,
		    ifd->ifd_len, NULL);
		if (rv) {
			kmem_free(sc->sc_linkstr, ifd->ifd_len);
			break;
		}

		rv = virtif_create(ifp);
		if (rv) {
			kmem_free(sc->sc_linkstr, ifd->ifd_len);
		}
		break;
#endif /* RUMP_VIF_LINKSTR */
	default:
		if (!sc->sc_linkstr)
			rv = ENXIO;
		else
			rv = ether_ioctl(ifp, cmd, data);
		if (rv == ENETRESET)
			rv = 0;
		break;
	}

	return rv;
}

/*
 * Output packets in-context until outgoing queue is empty.
 * Leave responsibility of choosing whether or not to drop the
 * kernel lock to VIPHYPER_SEND().
 */
#define LB_SH 32
static void
virtif_start(struct ifnet *ifp)
{
	struct virtif_sc *sc = ifp->if_softc;
	struct mbuf *m, *m0;
	struct iovec io[LB_SH];
	int i;

	ifp->if_flags |= IFF_OACTIVE;

	for (;;) {
		IF_DEQUEUE(&ifp->if_snd, m0);
		if (!m0) {
			break;
		}

		m = m0;
		for (i = 0; i < LB_SH && m; ) {
			if (m->m_len) {
				io[i].iov_base = mtod(m, void *);
				io[i].iov_len = m->m_len;
				i++;
			}
			m = m->m_next;
		}
		if (i == LB_SH && m)
			panic("lazy bum");
		bpf_mtap(ifp, m0);

		VIFHYPER_SEND(sc->sc_viu, io, i);

		m_freem(m0);
		ifp->if_opackets++;
	}

	ifp->if_flags &= ~IFF_OACTIVE;
}

static void
virtif_stop(struct ifnet *ifp, int disable)
{

	/* XXX: VIFHYPER_STOP() */

	ifp->if_flags &= ~IFF_RUNNING;
}

void
VIF_DELIVERPKT(struct virtif_sc *sc, struct iovec *iov, size_t iovlen)
{
	struct ifnet *ifp = &sc->sc_ec.ec_if;
	struct ether_header *eth;
	struct mbuf *m;
	size_t i;
	int off, olen;
	bool passup;
	const int align
	    = ALIGN(sizeof(struct ether_header)) - sizeof(struct ether_header);

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL)
		return; /* drop packet */
	m->m_len = m->m_pkthdr.len = 0;

	for (i = 0, off = align; i < iovlen; i++) {
		olen = m->m_pkthdr.len;
		m_copyback(m, off, iov[i].iov_len, iov[i].iov_base);
		off += iov[i].iov_len;
		if (olen + off != m->m_pkthdr.len) {
			aprint_verbose_ifnet(ifp, "m_copyback failed\n");
			m_freem(m);
			return;
		}
	}
	m->m_data += align;
	m->m_pkthdr.len -= align;
	m->m_len -= align;

	eth = mtod(m, struct ether_header *);
	if (memcmp(eth->ether_dhost, CLLADDR(ifp->if_sadl),
	    ETHER_ADDR_LEN) == 0) {
		passup = true;
	} else if (ETHER_IS_MULTICAST(eth->ether_dhost)) {
		passup = true;
	} else if (ifp->if_flags & IFF_PROMISC) {
		m->m_flags |= M_PROMISC;
		passup = true;
	} else {
		passup = false;
	}

	if (passup) {
		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		KERNEL_LOCK(1, NULL);
		bpf_mtap(ifp, m);
		ifp->if_input(ifp, m);
		KERNEL_UNLOCK_LAST(NULL);
	} else {
		m_freem(m);
	}
	m = NULL;
}

/*
 * The following ensures that no two modules using if_virt end up with
 * the same module name.  MODULE() and modcmd wrapped in ... bad mojo.
 */
#define VIF_MOJO(x) MODULE(MODULE_CLASS_DRIVER,x,NULL);
#define VIF_MODULE() VIF_MOJO(VIF_BASENAME(if_virt_,VIRTIF_BASE))
#define VIF_MODCMD VIF_BASENAME3(if_virt_,VIRTIF_BASE,_modcmd)
VIF_MODULE();
static int
VIF_MODCMD(modcmd_t cmd, void *opaque)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
		if_clone_attach(&VIF_CLONER);
		break;
	case MODULE_CMD_FINI:
		/*
		 * not sure if interfaces are refcounted
		 * and properly protected
		 */
#if 0
		if_clone_detach(&VIF_CLONER);
#else
		error = ENOTTY;
#endif
		break;
	default:
		error = ENOTTY;
	}
	return error;
}
