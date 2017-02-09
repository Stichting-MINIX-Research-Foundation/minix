/*	$NetBSD: if_ppp.c,v 1.149 2015/08/24 22:21:26 pooka Exp $	*/
/*	Id: if_ppp.c,v 1.6 1997/03/04 03:33:00 paulus Exp 	*/

/*
 * if_ppp.c - Point-to-Point Protocol (PPP) Asynchronous driver.
 *
 * Copyright (c) 1984-2000 Carnegie Mellon University. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Based on:
 *	@(#)if_sl.c	7.6.1.2 (Berkeley) 2/15/89
 *
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Serial Line interface
 *
 * Rick Adams
 * Center for Seismic Studies
 * 1300 N 17th Street, Suite 1450
 * Arlington, Virginia 22209
 * (703)276-7900
 * rick@seismo.ARPA
 * seismo!rick
 *
 * Pounded on heavily by Chris Torek (chris@mimsy.umd.edu, umcp-cs!chris).
 * Converted to 4.3BSD Beta by Chris Torek.
 * Other changes made at Berkeley, based in part on code by Kirk Smith.
 *
 * Converted to 4.3BSD+ 386BSD by Brad Parker (brad@cayman.com)
 * Added VJ tcp header compression; more unified ioctls
 *
 * Extensively modified by Paul Mackerras (paulus@cs.anu.edu.au).
 * Cleaned up a lot of the mbuf-related code to fix bugs that
 * caused system crashes and packet corruption.  Changed pppstart
 * so that it doesn't just give up with a collision if the whole
 * packet doesn't fit in the output ring buffer.
 *
 * Added priority queueing for interactive IP packets, following
 * the model of if_sl.c, plus hooks for bpf.
 * Paul Mackerras (paulus@cs.anu.edu.au).
 */

/* from if_sl.c,v 1.11 84/10/04 12:54:47 rick Exp */
/* from NetBSD: if_ppp.c,v 1.15.2.2 1994/07/28 05:17:58 cgd Exp */

/*
 * XXX IMP ME HARDER
 *
 * This is an explanation of that comment.  This code used to use
 * splimp() to block both network and tty interrupts.  However,
 * that call is deprecated.  So, we have replaced the uses of
 * splimp() with splhigh() in order to applomplish what it needs
 * to accomplish, and added that happy little comment.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ppp.c,v 1.149 2015/08/24 22:21:26 pooka Exp $");

#include "ppp.h"

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#include "opt_gateway.h"
#include "opt_ppp.h"
#endif

#ifdef INET
#define VJC
#endif
#define PPP_COMPRESS

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/once.h>
#include <sys/conf.h>
#include <sys/kauth.h>
#include <sys/intr.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#ifdef PPP_FILTER
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#ifdef INET
#include <netinet/ip.h>
#endif

#include <net/bpf.h>

#include <net/slip.h>

#ifdef VJC
#include <net/slcompress.h>
#endif

#include <net/ppp_defs.h>
#include <net/if_ppp.h>
#include <net/if_pppvar.h>
#include <sys/cpu.h>

#ifdef PPP_COMPRESS
#define PACKETPTR	struct mbuf *
#include <net/ppp-comp.h>
#endif

#include "ioconf.h"

static int	pppsioctl(struct ifnet *, u_long, void *);
static void	ppp_requeue(struct ppp_softc *);
static void	ppp_ccp(struct ppp_softc *, struct mbuf *m, int rcvd);
static void	ppp_ccp_closed(struct ppp_softc *);
static void	ppp_inproc(struct ppp_softc *, struct mbuf *);
static void	pppdumpm(struct mbuf *m0);
#ifdef ALTQ
static void	ppp_ifstart(struct ifnet *ifp);
#endif

static void	pppintr(void *);

/*
 * Some useful mbuf macros not in mbuf.h.
 */
#define M_IS_CLUSTER(m)	((m)->m_flags & M_EXT)

#define M_DATASTART(m)							\
	(M_IS_CLUSTER(m) ? (m)->m_ext.ext_buf :				\
	    (m)->m_flags & M_PKTHDR ? (m)->m_pktdat : (m)->m_dat)

#define M_DATASIZE(m)							\
	(M_IS_CLUSTER(m) ? (m)->m_ext.ext_size :			\
	    (m)->m_flags & M_PKTHDR ? MHLEN: MLEN)

/*
 * We define two link layer specific mbuf flags, to mark high-priority
 * packets for output, and received packets following lost/corrupted
 * packets.
 */
#define	M_HIGHPRI	M_LINK0	/* output packet for sc_fastq */
#define	M_ERRMARK	M_LINK1	/* rx packet following lost/corrupted pkt */

static int ppp_clone_create(struct if_clone *, int);
static int ppp_clone_destroy(struct ifnet *);

static struct ppp_softc *ppp_create(const char *, int);

static LIST_HEAD(, ppp_softc) ppp_softc_list;
static kmutex_t ppp_list_lock;

struct if_clone ppp_cloner =
    IF_CLONE_INITIALIZER("ppp", ppp_clone_create, ppp_clone_destroy);

#ifdef PPP_COMPRESS
ONCE_DECL(ppp_compressor_mtx_init);
static LIST_HEAD(, compressor) ppp_compressors = { NULL };
static kmutex_t ppp_compressors_mtx;

static int ppp_compressor_init(void);
static struct compressor *ppp_get_compressor(uint8_t);
static void ppp_compressor_rele(struct compressor *);
#endif /* PPP_COMPRESS */


/*
 * Called from boot code to establish ppp interfaces.
 */
void
pppattach(int n __unused)
{
	extern struct linesw ppp_disc;

	if (ttyldisc_attach(&ppp_disc) != 0)
		panic("pppattach");

	mutex_init(&ppp_list_lock, MUTEX_DEFAULT, IPL_NONE);
	LIST_INIT(&ppp_softc_list);
	if_clone_attach(&ppp_cloner);
	RUN_ONCE(&ppp_compressor_mtx_init, ppp_compressor_init);
}

static struct ppp_softc *
ppp_create(const char *name, int unit)
{
	struct ppp_softc *sc, *sci, *scl = NULL;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAIT|M_ZERO);

	mutex_enter(&ppp_list_lock);
	if (unit == -1) {
		int i = 0;
		LIST_FOREACH(sci, &ppp_softc_list, sc_iflist) {
			scl = sci;
			if (i < sci->sc_unit) {
				unit = i;
				break;
			} else {
#ifdef DIAGNOSTIC
				KASSERT(i == sci->sc_unit);
#endif
				i++;
			}
		}
		if (unit == -1)
			unit = i;
	} else {
		LIST_FOREACH(sci, &ppp_softc_list, sc_iflist) {
			scl = sci;
			if (unit < sci->sc_unit)
				break;
			else if (unit == sci->sc_unit) {
				free(sc, M_DEVBUF);
				return NULL;
			}
		}
	}

	if (sci != NULL)
		LIST_INSERT_BEFORE(sci, sc, sc_iflist);
	else if (scl != NULL)
		LIST_INSERT_AFTER(scl, sc, sc_iflist);
	else
		LIST_INSERT_HEAD(&ppp_softc_list, sc, sc_iflist);

	mutex_exit(&ppp_list_lock);

	if_initname(&sc->sc_if, name, sc->sc_unit = unit);
	callout_init(&sc->sc_timo_ch, 0);
	sc->sc_if.if_softc = sc;
	sc->sc_if.if_mtu = PPP_MTU;
	sc->sc_if.if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
	sc->sc_if.if_type = IFT_PPP;
	sc->sc_if.if_hdrlen = PPP_HDRLEN;
	sc->sc_if.if_dlt = DLT_NULL;
	sc->sc_if.if_ioctl = pppsioctl;
	sc->sc_if.if_output = pppoutput;
#ifdef ALTQ
	sc->sc_if.if_start = ppp_ifstart;
#endif
	IFQ_SET_MAXLEN(&sc->sc_if.if_snd, IFQ_MAXLEN);
	sc->sc_inq.ifq_maxlen = IFQ_MAXLEN;
	sc->sc_fastq.ifq_maxlen = IFQ_MAXLEN;
	sc->sc_rawq.ifq_maxlen = IFQ_MAXLEN;
	/* Ratio of 1:2 packets between the regular and the fast queue */
	sc->sc_maxfastq = 2;	
	IFQ_SET_READY(&sc->sc_if.if_snd);
	if_attach(&sc->sc_if);
	if_alloc_sadl(&sc->sc_if);
	bpf_attach(&sc->sc_if, DLT_NULL, 0);
	return sc;
}

static int
ppp_clone_create(struct if_clone *ifc, int unit)
{
	return ppp_create(ifc->ifc_name, unit) == NULL ? EEXIST : 0;
}

static int
ppp_clone_destroy(struct ifnet *ifp)
{
	struct ppp_softc *sc = (struct ppp_softc *)ifp->if_softc;

	if (sc->sc_devp != NULL)
		return EBUSY; /* Not removing it */

	mutex_enter(&ppp_list_lock);
	LIST_REMOVE(sc, sc_iflist);
	mutex_exit(&ppp_list_lock);

	bpf_detach(ifp);
	if_detach(ifp);

	free(sc, M_DEVBUF);
	return 0;
}

/*
 * Allocate a ppp interface unit and initialize it.
 */
struct ppp_softc *
pppalloc(pid_t pid)
{
	struct ppp_softc *sc = NULL, *scf;
	int i;

	mutex_enter(&ppp_list_lock);
	LIST_FOREACH(scf, &ppp_softc_list, sc_iflist) {
		if (scf->sc_xfer == pid) {
			scf->sc_xfer = 0;
			mutex_exit(&ppp_list_lock);
			return scf;
		}
		if (scf->sc_devp == NULL && sc == NULL)
			sc = scf;
	}
	mutex_exit(&ppp_list_lock);

	if (sc == NULL)
		sc = ppp_create(ppp_cloner.ifc_name, -1);

	sc->sc_si = softint_establish(SOFTINT_NET, pppintr, sc);
	if (sc->sc_si == NULL) {
		printf("%s: unable to establish softintr\n",
		    sc->sc_if.if_xname);
		return (NULL);
	}
	sc->sc_flags = 0;
	sc->sc_mru = PPP_MRU;
	sc->sc_relinq = NULL;
	(void)memset(&sc->sc_stats, 0, sizeof(sc->sc_stats));
#ifdef VJC
	sc->sc_comp = malloc(sizeof(struct slcompress), M_DEVBUF, M_NOWAIT);
	if (sc->sc_comp)
		sl_compress_init(sc->sc_comp);
#endif
#ifdef PPP_COMPRESS
	sc->sc_xc_state = NULL;
	sc->sc_rc_state = NULL;
#endif /* PPP_COMPRESS */
	for (i = 0; i < NUM_NP; ++i)
		sc->sc_npmode[i] = NPMODE_ERROR;
	sc->sc_npqueue = NULL;
	sc->sc_npqtail = &sc->sc_npqueue;
	sc->sc_last_sent = sc->sc_last_recv = time_second;

	return sc;
}

/*
 * Deallocate a ppp unit.  Must be called at splsoftnet or higher.
 */
void
pppdealloc(struct ppp_softc *sc)
{
	struct mbuf *m;

	softint_disestablish(sc->sc_si);
	if_down(&sc->sc_if);
	sc->sc_if.if_flags &= ~(IFF_UP|IFF_RUNNING);
	sc->sc_devp = NULL;
	sc->sc_xfer = 0;
	for (;;) {
		IF_DEQUEUE(&sc->sc_rawq, m);
		if (m == NULL)
			break;
		m_freem(m);
	}
	for (;;) {
		IF_DEQUEUE(&sc->sc_inq, m);
		if (m == NULL)
			break;
		m_freem(m);
	}
	for (;;) {
		IF_DEQUEUE(&sc->sc_fastq, m);
		if (m == NULL)
			break;
		m_freem(m);
	}
	while ((m = sc->sc_npqueue) != NULL) {
		sc->sc_npqueue = m->m_nextpkt;
		m_freem(m);
	}
	if (sc->sc_togo != NULL) {
		m_freem(sc->sc_togo);
		sc->sc_togo = NULL;
	}
#ifdef PPP_COMPRESS
	ppp_ccp_closed(sc);
	sc->sc_xc_state = NULL;
	sc->sc_rc_state = NULL;
#endif /* PPP_COMPRESS */
#ifdef PPP_FILTER
	if (sc->sc_pass_filt_in.bf_insns != 0) {
		free(sc->sc_pass_filt_in.bf_insns, M_DEVBUF);
		sc->sc_pass_filt_in.bf_insns = 0;
		sc->sc_pass_filt_in.bf_len = 0;
	}
	if (sc->sc_pass_filt_out.bf_insns != 0) {
		free(sc->sc_pass_filt_out.bf_insns, M_DEVBUF);
		sc->sc_pass_filt_out.bf_insns = 0;
		sc->sc_pass_filt_out.bf_len = 0;
	}
	if (sc->sc_active_filt_in.bf_insns != 0) {
		free(sc->sc_active_filt_in.bf_insns, M_DEVBUF);
		sc->sc_active_filt_in.bf_insns = 0;
		sc->sc_active_filt_in.bf_len = 0;
	}
	if (sc->sc_active_filt_out.bf_insns != 0) {
		free(sc->sc_active_filt_out.bf_insns, M_DEVBUF);
		sc->sc_active_filt_out.bf_insns = 0;
		sc->sc_active_filt_out.bf_len = 0;
	}
#endif /* PPP_FILTER */
#ifdef VJC
	if (sc->sc_comp != 0) {
		free(sc->sc_comp, M_DEVBUF);
		sc->sc_comp = 0;
	}
#endif
	(void)ppp_clone_destroy(&sc->sc_if);
}

/*
 * Ioctl routine for generic ppp devices.
 */
int
pppioctl(struct ppp_softc *sc, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	int s, error, flags, mru, npx;
	u_int nb;
	struct ppp_option_data *odp;
	struct compressor *cp;
	struct npioctl *npi;
	time_t t;
#ifdef PPP_FILTER
	struct bpf_program *bp, *nbp;
	struct bpf_insn *newcode, *oldcode;
	int newcodelen;
#endif /* PPP_FILTER */
#ifdef	PPP_COMPRESS
	u_char ccp_option[CCP_MAX_OPTION_LENGTH];
#endif

	switch (cmd) {
	case PPPIOCSFLAGS:
	case PPPIOCSMRU:
	case PPPIOCSMAXCID:
	case PPPIOCSCOMPRESS:
	case PPPIOCSNPMODE:
		if (kauth_authorize_network(l->l_cred, KAUTH_NETWORK_INTERFACE,
			KAUTH_REQ_NETWORK_INTERFACE_SETPRIV, &sc->sc_if,
			KAUTH_ARG(cmd), NULL) != 0)
			return (EPERM);
		break;
	case PPPIOCXFERUNIT:
		/* XXX: Why is this privileged?! */
		if (kauth_authorize_network(l->l_cred, KAUTH_NETWORK_INTERFACE,
			KAUTH_REQ_NETWORK_INTERFACE_GETPRIV, &sc->sc_if,
			KAUTH_ARG(cmd), NULL) != 0)
			return (EPERM);
		break;
	default:
		break;
	}

	switch (cmd) {
	case FIONREAD:
		*(int *)data = sc->sc_inq.ifq_len;
		break;

	case PPPIOCGUNIT:
		*(int *)data = sc->sc_unit;
		break;

	case PPPIOCGFLAGS:
		*(u_int *)data = sc->sc_flags;
		break;

	case PPPIOCGRAWIN:
	{
		struct ppp_rawin *rwin = (struct ppp_rawin *)data;
		u_char c, q = 0;

		for (c = sc->sc_rawin_start; c < sizeof(sc->sc_rawin.buf);)
			rwin->buf[q++] = sc->sc_rawin.buf[c++];

		for (c = 0; c < sc->sc_rawin_start;)
			rwin->buf[q++] = sc->sc_rawin.buf[c++];

		rwin->count = sc->sc_rawin.count;
	}
	break;

	case PPPIOCSFLAGS:
		flags = *(int *)data & SC_MASK;
		s = splsoftnet();
#ifdef PPP_COMPRESS
		if (sc->sc_flags & SC_CCP_OPEN && !(flags & SC_CCP_OPEN))
			ppp_ccp_closed(sc);
#endif
		splhigh();	/* XXX IMP ME HARDER */
		sc->sc_flags = (sc->sc_flags & ~SC_MASK) | flags;
		splx(s);
		break;

	case PPPIOCSMRU:
		mru = *(int *)data;
		if (mru >= PPP_MINMRU && mru <= PPP_MAXMRU)
			sc->sc_mru = mru;
		break;

	case PPPIOCGMRU:
		*(int *)data = sc->sc_mru;
		break;

#ifdef VJC
	case PPPIOCSMAXCID:
		if (sc->sc_comp) {
			s = splsoftnet();
			sl_compress_setup(sc->sc_comp, *(int *)data);
			splx(s);
		}
		break;
#endif

	case PPPIOCXFERUNIT:
		sc->sc_xfer = l->l_proc->p_pid;
		break;

#ifdef PPP_COMPRESS
	case PPPIOCSCOMPRESS:
		odp = (struct ppp_option_data *) data;
		nb = odp->length;
		if (nb > sizeof(ccp_option))
			nb = sizeof(ccp_option);
		if ((error = copyin(odp->ptr, ccp_option, nb)) != 0)
			return (error);
		/* preliminary check on the length byte */
		if (ccp_option[1] < 2)
			return (EINVAL);
		cp = ppp_get_compressor(ccp_option[0]);
		if (cp == NULL) {
			if (sc->sc_flags & SC_DEBUG)
				printf("%s: no compressor for [%x %x %x], %x\n",
				    sc->sc_if.if_xname, ccp_option[0],
				    ccp_option[1], ccp_option[2], nb);
			return (EINVAL);	/* no handler found */
		}
		/*
		 * Found a handler for the protocol - try to allocate
		 * a compressor or decompressor.
		 */
		error = 0;
		if (odp->transmit) {
			s = splsoftnet();
			if (sc->sc_xc_state != NULL) {
				(*sc->sc_xcomp->comp_free)(sc->sc_xc_state);
				ppp_compressor_rele(sc->sc_xcomp);
			}
			sc->sc_xcomp = cp;
			sc->sc_xc_state = cp->comp_alloc(ccp_option, nb);
			if (sc->sc_xc_state == NULL) {
				if (sc->sc_flags & SC_DEBUG)
					printf("%s: comp_alloc failed\n",
					    sc->sc_if.if_xname);
				error = ENOBUFS;
			}
			splhigh();	/* XXX IMP ME HARDER */
			sc->sc_flags &= ~SC_COMP_RUN;
			splx(s);
		} else {
			s = splsoftnet();
			if (sc->sc_rc_state != NULL) {
				(*sc->sc_rcomp->decomp_free)(sc->sc_rc_state);
				ppp_compressor_rele(sc->sc_rcomp);
			}
			sc->sc_rcomp = cp;
			sc->sc_rc_state = cp->decomp_alloc(ccp_option, nb);
			if (sc->sc_rc_state == NULL) {
				if (sc->sc_flags & SC_DEBUG)
					printf("%s: decomp_alloc failed\n",
					    sc->sc_if.if_xname);
				error = ENOBUFS;
			}
			splhigh();	/* XXX IMP ME HARDER */
			sc->sc_flags &= ~SC_DECOMP_RUN;
			splx(s);
		}
		return (error);
#endif /* PPP_COMPRESS */

	case PPPIOCGNPMODE:
	case PPPIOCSNPMODE:
		npi = (struct npioctl *) data;
		switch (npi->protocol) {
		case PPP_IP:
			npx = NP_IP;
			break;
		case PPP_IPV6:
			npx = NP_IPV6;
			break;
		default:
			return EINVAL;
		}
		if (cmd == PPPIOCGNPMODE) {
			npi->mode = sc->sc_npmode[npx];
		} else {
			if (npi->mode != sc->sc_npmode[npx]) {
				s = splnet();
				sc->sc_npmode[npx] = npi->mode;
				if (npi->mode != NPMODE_QUEUE) {
					ppp_requeue(sc);
					ppp_restart(sc);
				}
				splx(s);
			}
		}
		break;

	case PPPIOCGIDLE:
		s = splsoftnet();
		t = time_second;
		((struct ppp_idle *)data)->xmit_idle = t - sc->sc_last_sent;
		((struct ppp_idle *)data)->recv_idle = t - sc->sc_last_recv;
		splx(s);
		break;

#ifdef PPP_FILTER
	case PPPIOCSPASS:
	case PPPIOCSACTIVE:
		/* These are no longer supported. */
		return EOPNOTSUPP;

	case PPPIOCSIPASS:
	case PPPIOCSOPASS:
	case PPPIOCSIACTIVE:
	case PPPIOCSOACTIVE:
		nbp = (struct bpf_program *) data;
		if ((unsigned) nbp->bf_len > BPF_MAXINSNS)
			return EINVAL;
		newcodelen = nbp->bf_len * sizeof(struct bpf_insn);
		if (newcodelen != 0) {
			newcode = malloc(newcodelen, M_DEVBUF, M_WAITOK);
			/* WAITOK -- malloc() never fails. */
			if ((error = copyin((void *)nbp->bf_insns,
				    (void *)newcode, newcodelen)) != 0) {
				free(newcode, M_DEVBUF);
				return error;
			}
			if (!bpf_validate(newcode, nbp->bf_len)) {
				free(newcode, M_DEVBUF);
				return EINVAL;
			}
		} else
			newcode = 0;
		switch (cmd) {
		case PPPIOCSIPASS:
			bp = &sc->sc_pass_filt_in;
			break;

		case PPPIOCSOPASS:
			bp = &sc->sc_pass_filt_out;
			break;

		case PPPIOCSIACTIVE:
			bp = &sc->sc_active_filt_in;
			break;

		case PPPIOCSOACTIVE:
			bp = &sc->sc_active_filt_out;
			break;
		default:
			free(newcode, M_DEVBUF);
			return (EPASSTHROUGH);
		}
		oldcode = bp->bf_insns;
		s = splnet();
		bp->bf_len = nbp->bf_len;
		bp->bf_insns = newcode;
		splx(s);
		if (oldcode != 0)
			free(oldcode, M_DEVBUF);
		break;
#endif /* PPP_FILTER */

	default:
		return (EPASSTHROUGH);
	}
	return (0);
}

/*
 * Process an ioctl request to the ppp network interface.
 */
static int
pppsioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct ppp_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ppp_stats *psp;
#ifdef	PPP_COMPRESS
	struct ppp_comp_stats *pcp;
#endif
	int s = splnet(), error = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;
		if ((ifp->if_flags & IFF_RUNNING) == 0)
			ifp->if_flags &= ~IFF_UP;
		break;

	case SIOCINITIFADDR:
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
			error = EAFNOSUPPORT;
			break;
		}
		ifa->ifa_rtrequest = p2p_rtrequest;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifr == NULL) {
			error = EAFNOSUPPORT;
			break;
		}
		switch (ifreq_getaddr(cmd, ifr)->sa_family) {
#ifdef INET
		case AF_INET:
			break;
#endif
#ifdef INET6
		case AF_INET6:
			break;
#endif
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCGPPPSTATS:
		psp = &((struct ifpppstatsreq *) data)->stats;
		memset(psp, 0, sizeof(*psp));
		psp->p = sc->sc_stats;
#if defined(VJC) && !defined(SL_NO_STATS)
		if (sc->sc_comp) {
			psp->vj.vjs_packets = sc->sc_comp->sls_packets;
			psp->vj.vjs_compressed = sc->sc_comp->sls_compressed;
			psp->vj.vjs_searches = sc->sc_comp->sls_searches;
			psp->vj.vjs_misses = sc->sc_comp->sls_misses;
			psp->vj.vjs_uncompressedin = sc->sc_comp->sls_uncompressedin;
			psp->vj.vjs_compressedin = sc->sc_comp->sls_compressedin;
			psp->vj.vjs_errorin = sc->sc_comp->sls_errorin;
			psp->vj.vjs_tossed = sc->sc_comp->sls_tossed;
		}
#endif /* VJC */
		break;

#ifdef PPP_COMPRESS
	case SIOCGPPPCSTATS:
		pcp = &((struct ifpppcstatsreq *) data)->stats;
		memset(pcp, 0, sizeof(*pcp));
		if (sc->sc_xc_state != NULL)
			(*sc->sc_xcomp->comp_stat)(sc->sc_xc_state, &pcp->c);
		if (sc->sc_rc_state != NULL)
			(*sc->sc_rcomp->decomp_stat)(sc->sc_rc_state, &pcp->d);
		break;
#endif /* PPP_COMPRESS */

	default:
		if ((error = ifioctl_common(&sc->sc_if, cmd, data)) == ENETRESET)
			error = 0;
		break;
	}
	splx(s);
	return (error);
}

/*
 * Queue a packet.  Start transmission if not active.
 * Packet is placed in Information field of PPP frame.
 */
int
pppoutput(struct ifnet *ifp, struct mbuf *m0, const struct sockaddr *dst,
    struct rtentry *rtp)
{
	struct ppp_softc *sc = ifp->if_softc;
	int protocol, address, control;
	u_char *cp;
	int s, error;
#ifdef INET
	struct ip *ip;
#endif
	struct ifqueue *ifq;
	enum NPmode mode;
	int len;
	ALTQ_DECL(struct altq_pktattr pktattr;)

	    if (sc->sc_devp == NULL || (ifp->if_flags & IFF_RUNNING) == 0
		|| ((ifp->if_flags & IFF_UP) == 0 && dst->sa_family != AF_UNSPEC)) {
		    error = ENETDOWN;	/* sort of */
		    goto bad;
	    }

	IFQ_CLASSIFY(&ifp->if_snd, m0, dst->sa_family, &pktattr);

	/*
	 * Compute PPP header.
	 */
	m0->m_flags &= ~M_HIGHPRI;
	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
		address = PPP_ALLSTATIONS;
		control = PPP_UI;
		protocol = PPP_IP;
		mode = sc->sc_npmode[NP_IP];

		/*
		 * If this packet has the "low delay" bit set in the IP header,
		 * put it on the fastq instead.
		 */
		ip = mtod(m0, struct ip *);
		if (ip->ip_tos & IPTOS_LOWDELAY)
			m0->m_flags |= M_HIGHPRI;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		address = PPP_ALLSTATIONS;	/*XXX*/
		control = PPP_UI;		/*XXX*/
		protocol = PPP_IPV6;
		mode = sc->sc_npmode[NP_IPV6];

#if 0	/* XXX flowinfo/traffic class, maybe? */
	/*
	 * If this packet has the "low delay" bit set in the IP header,
	 * put it on the fastq instead.
	 */
		ip = mtod(m0, struct ip *);
		if (ip->ip_tos & IPTOS_LOWDELAY)
			m0->m_flags |= M_HIGHPRI;
#endif
		break;
#endif
	case AF_UNSPEC:
		address = PPP_ADDRESS(dst->sa_data);
		control = PPP_CONTROL(dst->sa_data);
		protocol = PPP_PROTOCOL(dst->sa_data);
		mode = NPMODE_PASS;
		break;
	default:
		printf("%s: af%d not supported\n", ifp->if_xname,
		    dst->sa_family);
		error = EAFNOSUPPORT;
		goto bad;
	}

	/*
	 * Drop this packet, or return an error, if necessary.
	 */
	if (mode == NPMODE_ERROR) {
		error = ENETDOWN;
		goto bad;
	}
	if (mode == NPMODE_DROP) {
		error = 0;
		goto bad;
	}

	/*
	 * Add PPP header.
	 */
	M_PREPEND(m0, PPP_HDRLEN, M_DONTWAIT);
	if (m0 == NULL) {
		error = ENOBUFS;
		goto bad;
	}

	cp = mtod(m0, u_char *);
	*cp++ = address;
	*cp++ = control;
	*cp++ = protocol >> 8;
	*cp++ = protocol & 0xff;

	len = m_length(m0);

	if (sc->sc_flags & SC_LOG_OUTPKT) {
		printf("%s output: ", ifp->if_xname);
		pppdumpm(m0);
	}

	if ((protocol & 0x8000) == 0) {
#ifdef PPP_FILTER
		/*
		 * Apply the pass and active filters to the packet,
		 * but only if it is a data packet.
		 */
		if (sc->sc_pass_filt_out.bf_insns != 0
		    && bpf_filter(sc->sc_pass_filt_out.bf_insns,
			(u_char *)m0, len, 0) == 0) {
			error = 0;		/* drop this packet */
			goto bad;
		}

		/*
		 * Update the time we sent the most recent packet.
		 */
		if (sc->sc_active_filt_out.bf_insns == 0
		    || bpf_filter(sc->sc_active_filt_out.bf_insns,
			(u_char *)m0, len, 0))
			sc->sc_last_sent = time_second;
#else
		/*
		 * Update the time we sent the most recent packet.
		 */
		sc->sc_last_sent = time_second;
#endif /* PPP_FILTER */
	}

	/*
	 * See if bpf wants to look at the packet.
	 */
	bpf_mtap(&sc->sc_if, m0);

	/*
	 * Put the packet on the appropriate queue.
	 */
	s = splnet();
	if (mode == NPMODE_QUEUE) {
		/* XXX we should limit the number of packets on this queue */
		*sc->sc_npqtail = m0;
		m0->m_nextpkt = NULL;
		sc->sc_npqtail = &m0->m_nextpkt;
	} else {
		ifq = (m0->m_flags & M_HIGHPRI) ? &sc->sc_fastq : NULL;
		if ((error = ifq_enqueue2(&sc->sc_if, ifq, m0
			    ALTQ_COMMA ALTQ_DECL(&pktattr))) != 0) {
			splx(s);
			sc->sc_if.if_oerrors++;
			sc->sc_stats.ppp_oerrors++;
			return (error);
		}
		ppp_restart(sc);
	}
	ifp->if_opackets++;
	ifp->if_obytes += len;

	splx(s);
	return (0);

bad:
	m_freem(m0);
	return (error);
}

/*
 * After a change in the NPmode for some NP, move packets from the
 * npqueue to the send queue or the fast queue as appropriate.
 * Should be called at splnet, since we muck with the queues.
 */
static void
ppp_requeue(struct ppp_softc *sc)
{
	struct mbuf *m, **mpp;
	struct ifqueue *ifq;
	enum NPmode mode;
	int error;

	for (mpp = &sc->sc_npqueue; (m = *mpp) != NULL; ) {
		switch (PPP_PROTOCOL(mtod(m, u_char *))) {
		case PPP_IP:
			mode = sc->sc_npmode[NP_IP];
			break;
		case PPP_IPV6:
			mode = sc->sc_npmode[NP_IPV6];
			break;
		default:
			mode = NPMODE_PASS;
		}

		switch (mode) {
		case NPMODE_PASS:
			/*
			 * This packet can now go on one of the queues to
			 * be sent.
			 */
			*mpp = m->m_nextpkt;
			m->m_nextpkt = NULL;
			ifq = (m->m_flags & M_HIGHPRI) ? &sc->sc_fastq : NULL;
			if ((error = ifq_enqueue2(&sc->sc_if, ifq, m ALTQ_COMMA
				    ALTQ_DECL(NULL))) != 0) {
				sc->sc_if.if_oerrors++;
				sc->sc_stats.ppp_oerrors++;
			}
			break;

		case NPMODE_DROP:
		case NPMODE_ERROR:
			*mpp = m->m_nextpkt;
			m_freem(m);
			break;

		case NPMODE_QUEUE:
			mpp = &m->m_nextpkt;
			break;
		}
	}
	sc->sc_npqtail = mpp;
}

/*
 * Transmitter has finished outputting some stuff;
 * remember to call sc->sc_start later at splsoftnet.
 */
void
ppp_restart(struct ppp_softc *sc)
{
	int s = splhigh();	/* XXX IMP ME HARDER */

	sc->sc_flags &= ~SC_TBUSY;
	softint_schedule(sc->sc_si);
	splx(s);
}

/*
 * Get a packet to send.  This procedure is intended to be called at
 * splsoftnet, since it may involve time-consuming operations such as
 * applying VJ compression, packet compression, address/control and/or
 * protocol field compression to the packet.
 */
struct mbuf *
ppp_dequeue(struct ppp_softc *sc)
{
	struct mbuf *m, *mp;
	u_char *cp;
	int address, control, protocol;
	int s;

	/*
	 * Grab a packet to send: first try the fast queue, then the
	 * normal queue.
	 */
	s = splnet();
	if (sc->sc_nfastq < sc->sc_maxfastq) {
		IF_DEQUEUE(&sc->sc_fastq, m);
		if (m != NULL)
			sc->sc_nfastq++;
		else
			IFQ_DEQUEUE(&sc->sc_if.if_snd, m);
	} else {
		sc->sc_nfastq = 0;
		IFQ_DEQUEUE(&sc->sc_if.if_snd, m);
		if (m == NULL) {
			IF_DEQUEUE(&sc->sc_fastq, m);
			if (m != NULL)
				sc->sc_nfastq++;
		}
	}
	splx(s);

	if (m == NULL)
		return NULL;

	++sc->sc_stats.ppp_opackets;

	/*
	 * Extract the ppp header of the new packet.
	 * The ppp header will be in one mbuf.
	 */
	cp = mtod(m, u_char *);
	address = PPP_ADDRESS(cp);
	control = PPP_CONTROL(cp);
	protocol = PPP_PROTOCOL(cp);

	switch (protocol) {
	case PPP_IP:
#ifdef VJC
		/*
		 * If the packet is a TCP/IP packet, see if we can compress it.
		 */
		if ((sc->sc_flags & SC_COMP_TCP) && sc->sc_comp != NULL) {
			struct ip *ip;
			int type;

			mp = m;
			ip = (struct ip *) (cp + PPP_HDRLEN);
			if (mp->m_len <= PPP_HDRLEN) {
				mp = mp->m_next;
				if (mp == NULL)
					break;
				ip = mtod(mp, struct ip *);
			}
			/*
			 * This code assumes the IP/TCP header is in one
			 * non-shared mbuf
			 */
			if (ip->ip_p == IPPROTO_TCP) {
				type = sl_compress_tcp(mp, ip, sc->sc_comp,
				    !(sc->sc_flags & SC_NO_TCP_CCID));
				switch (type) {
				case TYPE_UNCOMPRESSED_TCP:
					protocol = PPP_VJC_UNCOMP;
					break;
				case TYPE_COMPRESSED_TCP:
					protocol = PPP_VJC_COMP;
					cp = mtod(m, u_char *);
					cp[0] = address; /* Header has moved */
					cp[1] = control;
					cp[2] = 0;
					break;
				}
				/* Update protocol in PPP header */
				cp[3] = protocol;
			}
		}
#endif	/* VJC */
		break;

#ifdef PPP_COMPRESS
	case PPP_CCP:
		ppp_ccp(sc, m, 0);
		break;
#endif	/* PPP_COMPRESS */
	}

#ifdef PPP_COMPRESS
	if (protocol != PPP_LCP && protocol != PPP_CCP
	    && sc->sc_xc_state && (sc->sc_flags & SC_COMP_RUN)) {
		struct mbuf *mcomp = NULL;
		int slen;

		slen = 0;
		for (mp = m; mp != NULL; mp = mp->m_next)
			slen += mp->m_len;
		(*sc->sc_xcomp->compress)
		    (sc->sc_xc_state, &mcomp, m, slen, sc->sc_if.if_mtu + PPP_HDRLEN);
		if (mcomp != NULL) {
			if (sc->sc_flags & SC_CCP_UP) {
				/*
				 * Send the compressed packet instead of the
				 * original.
				 */
				m_freem(m);
				m = mcomp;
				cp = mtod(m, u_char *);
				protocol = cp[3];
			} else {
				/*
				 * Can't transmit compressed packets until CCP
				 * is up.
				 */
				m_freem(mcomp);
			}
		}
	}
#endif	/* PPP_COMPRESS */

	/*
	 * Compress the address/control and protocol, if possible.
	 */
	if (sc->sc_flags & SC_COMP_AC && address == PPP_ALLSTATIONS &&
	    control == PPP_UI && protocol != PPP_ALLSTATIONS &&
	    protocol != PPP_LCP) {
		/* can compress address/control */
		m->m_data += 2;
		m->m_len -= 2;
	}
	if (sc->sc_flags & SC_COMP_PROT && protocol < 0xFF) {
		/* can compress protocol */
		if (mtod(m, u_char *) == cp) {
			cp[2] = cp[1];	/* move address/control up */
			cp[1] = cp[0];
		}
		++m->m_data;
		--m->m_len;
	}

	return m;
}

/*
 * Software interrupt routine, called at splsoftnet.
 */
static void
pppintr(void *arg)
{
	struct ppp_softc *sc = arg;
	struct mbuf *m;
	int s;

	mutex_enter(softnet_lock);
	if (!(sc->sc_flags & SC_TBUSY)
	    && (IFQ_IS_EMPTY(&sc->sc_if.if_snd) == 0 || sc->sc_fastq.ifq_head
		|| sc->sc_outm)) {
		s = splhigh();	/* XXX IMP ME HARDER */
		sc->sc_flags |= SC_TBUSY;
		splx(s);
		(*sc->sc_start)(sc);
	}
	for (;;) {
		s = splnet();
		IF_DEQUEUE(&sc->sc_rawq, m);
		splx(s);
		if (m == NULL)
			break;
		ppp_inproc(sc, m);
	}
	mutex_exit(softnet_lock);
}

#ifdef PPP_COMPRESS
/*
 * Handle a CCP packet.	 `rcvd' is 1 if the packet was received,
 * 0 if it is about to be transmitted.
 */
static void
ppp_ccp(struct ppp_softc *sc, struct mbuf *m, int rcvd)
{
	u_char *dp, *ep;
	struct mbuf *mp;
	int slen, s;

	/*
	 * Get a pointer to the data after the PPP header.
	 */
	if (m->m_len <= PPP_HDRLEN) {
		mp = m->m_next;
		if (mp == NULL)
			return;
		dp = mtod(mp, u_char *);
	} else {
		mp = m;
		dp = mtod(mp, u_char *) + PPP_HDRLEN;
	}

	ep = mtod(mp, u_char *) + mp->m_len;
	if (dp + CCP_HDRLEN > ep)
		return;
	slen = CCP_LENGTH(dp);
	if (dp + slen > ep) {
		if (sc->sc_flags & SC_DEBUG)
			printf("if_ppp/ccp: not enough data in mbuf (%p+%x > %p+%x)\n",
			    dp, slen, mtod(mp, u_char *), mp->m_len);
		return;
	}

	switch (CCP_CODE(dp)) {
	case CCP_CONFREQ:
	case CCP_TERMREQ:
	case CCP_TERMACK:
		/* CCP must be going down - disable compression */
		if (sc->sc_flags & SC_CCP_UP) {
			s = splhigh();	/* XXX IMP ME HARDER */
			sc->sc_flags &= ~(SC_CCP_UP | SC_COMP_RUN | SC_DECOMP_RUN);
			splx(s);
		}
		break;

	case CCP_CONFACK:
		if (sc->sc_flags & SC_CCP_OPEN && !(sc->sc_flags & SC_CCP_UP)
		    && slen >= CCP_HDRLEN + CCP_OPT_MINLEN
		    && slen >= CCP_OPT_LENGTH(dp + CCP_HDRLEN) + CCP_HDRLEN) {
			if (!rcvd) {
				/* We're agreeing to send compressed packets. */
				if (sc->sc_xc_state != NULL
				    && (*sc->sc_xcomp->comp_init)
				    (sc->sc_xc_state, dp + CCP_HDRLEN,
					slen - CCP_HDRLEN, sc->sc_unit, 0,
					sc->sc_flags & SC_DEBUG)) {
					s = splhigh();	/* XXX IMP ME HARDER */
					sc->sc_flags |= SC_COMP_RUN;
					splx(s);
				}
			} else {
				/*
				 * Peer is agreeing to send compressed
				 * packets.
				 */
				if (sc->sc_rc_state != NULL
				    && (*sc->sc_rcomp->decomp_init)
				    (sc->sc_rc_state, dp + CCP_HDRLEN, slen - CCP_HDRLEN,
					sc->sc_unit, 0, sc->sc_mru,
					sc->sc_flags & SC_DEBUG)) {
					s = splhigh();	/* XXX IMP ME HARDER */
					sc->sc_flags |= SC_DECOMP_RUN;
					sc->sc_flags &= ~(SC_DC_ERROR | SC_DC_FERROR);
					splx(s);
				}
			}
		}
		break;

	case CCP_RESETACK:
		if (sc->sc_flags & SC_CCP_UP) {
			if (!rcvd) {
				if (sc->sc_xc_state && (sc->sc_flags & SC_COMP_RUN))
					(*sc->sc_xcomp->comp_reset)(sc->sc_xc_state);
			} else {
				if (sc->sc_rc_state && (sc->sc_flags & SC_DECOMP_RUN)) {
					(*sc->sc_rcomp->decomp_reset)(sc->sc_rc_state);
					s = splhigh();	/* XXX IMP ME HARDER */
					sc->sc_flags &= ~SC_DC_ERROR;
					splx(s);
				}
			}
		}
		break;
	}
}

/*
 * CCP is down; free (de)compressor state if necessary.
 */
static void
ppp_ccp_closed(struct ppp_softc *sc)
{
	if (sc->sc_xc_state) {
		(*sc->sc_xcomp->comp_free)(sc->sc_xc_state);
		ppp_compressor_rele(sc->sc_xcomp);
		sc->sc_xc_state = NULL;
	}
	if (sc->sc_rc_state) {
		(*sc->sc_rcomp->decomp_free)(sc->sc_rc_state);
		ppp_compressor_rele(sc->sc_rcomp);
		sc->sc_rc_state = NULL;
	}
}
#endif /* PPP_COMPRESS */

/*
 * PPP packet input routine.
 * The caller has checked and removed the FCS and has inserted
 * the address/control bytes and the protocol high byte if they
 * were omitted.
 */
void
ppppktin(struct ppp_softc *sc, struct mbuf *m, int lost)
{
	int s = splhigh();	/* XXX IMP ME HARDER */

	if (lost)
		m->m_flags |= M_ERRMARK;
	IF_ENQUEUE(&sc->sc_rawq, m);
	softint_schedule(sc->sc_si);
	splx(s);
}

/*
 * Process a received PPP packet, doing decompression as necessary.
 * Should be called at splsoftnet.
 */
#define COMPTYPE(proto)	((proto) == PPP_VJC_COMP ? TYPE_COMPRESSED_TCP:	      \
	    TYPE_UNCOMPRESSED_TCP)

static void
ppp_inproc(struct ppp_softc *sc, struct mbuf *m)
{
	struct ifnet *ifp = &sc->sc_if;
	pktqueue_t *pktq = NULL;
	struct ifqueue *inq = NULL;
	int s, ilen, proto, rv;
	u_char *cp, adrs, ctrl;
	struct mbuf *mp, *dmp = NULL;
#ifdef VJC
	int xlen;
	u_char *iphdr;
	u_int hlen;
#endif

	sc->sc_stats.ppp_ipackets++;

	if (sc->sc_flags & SC_LOG_INPKT) {
		ilen = 0;
		for (mp = m; mp != NULL; mp = mp->m_next)
			ilen += mp->m_len;
		printf("%s: got %d bytes\n", ifp->if_xname, ilen);
		pppdumpm(m);
	}

	cp = mtod(m, u_char *);
	adrs = PPP_ADDRESS(cp);
	ctrl = PPP_CONTROL(cp);
	proto = PPP_PROTOCOL(cp);

	if (m->m_flags & M_ERRMARK) {
		m->m_flags &= ~M_ERRMARK;
		s = splhigh();	/* XXX IMP ME HARDER */
		sc->sc_flags |= SC_VJ_RESET;
		splx(s);
	}

#ifdef PPP_COMPRESS
	/*
	 * Decompress this packet if necessary, update the receiver's
	 * dictionary, or take appropriate action on a CCP packet.
	 */
	if (proto == PPP_COMP && sc->sc_rc_state && (sc->sc_flags & SC_DECOMP_RUN)
	    && !(sc->sc_flags & SC_DC_ERROR) && !(sc->sc_flags & SC_DC_FERROR)) {
		/* Decompress this packet */
		rv = (*sc->sc_rcomp->decompress)(sc->sc_rc_state, m, &dmp);
		if (rv == DECOMP_OK) {
			m_freem(m);
			if (dmp == NULL) {
				/*
				 * No error, but no decompressed packet
				 * produced
				 */
				return;
			}
			m = dmp;
			cp = mtod(m, u_char *);
			proto = PPP_PROTOCOL(cp);

		} else {
			/*
			 * An error has occurred in decompression.
			 * Pass the compressed packet up to pppd, which may
			 * take CCP down or issue a Reset-Req.
			 */
			if (sc->sc_flags & SC_DEBUG)
				printf("%s: decompress failed %d\n",
				    ifp->if_xname, rv);
			s = splhigh();	/* XXX IMP ME HARDER */
			sc->sc_flags |= SC_VJ_RESET;
			if (rv == DECOMP_ERROR)
				sc->sc_flags |= SC_DC_ERROR;
			else
				sc->sc_flags |= SC_DC_FERROR;
			splx(s);
		}

	} else {
		if (sc->sc_rc_state && (sc->sc_flags & SC_DECOMP_RUN))
			(*sc->sc_rcomp->incomp)(sc->sc_rc_state, m);
		if (proto == PPP_CCP)
			ppp_ccp(sc, m, 1);
	}
#endif

	ilen = 0;
	for (mp = m; mp != NULL; mp = mp->m_next)
		ilen += mp->m_len;

#ifdef VJC
	if (sc->sc_flags & SC_VJ_RESET) {
		/*
		 * If we've missed a packet, we must toss subsequent compressed
		 * packets which don't have an explicit connection ID.
		 */
		if (sc->sc_comp)
			sl_uncompress_tcp(NULL, 0, TYPE_ERROR, sc->sc_comp);
		s = splhigh();	/* XXX IMP ME HARDER */
		sc->sc_flags &= ~SC_VJ_RESET;
		splx(s);
	}

	/*
	 * See if we have a VJ-compressed packet to uncompress.
	 */
	if (proto == PPP_VJC_COMP) {
		if ((sc->sc_flags & SC_REJ_COMP_TCP) || sc->sc_comp == 0)
			goto bad;

		xlen = sl_uncompress_tcp_core(cp + PPP_HDRLEN,
		    m->m_len - PPP_HDRLEN, ilen - PPP_HDRLEN,
		    TYPE_COMPRESSED_TCP, sc->sc_comp, &iphdr, &hlen);

		if (xlen <= 0) {
			if (sc->sc_flags & SC_DEBUG)
				printf("%s: VJ uncompress failed on type comp\n",
				    ifp->if_xname);
			goto bad;
		}

		/* Copy the PPP and IP headers into a new mbuf. */
		MGETHDR(mp, M_DONTWAIT, MT_DATA);
		if (mp == NULL)
			goto bad;
		mp->m_len = 0;
		mp->m_next = NULL;
		if (hlen + PPP_HDRLEN > MHLEN) {
			MCLGET(mp, M_DONTWAIT);
			if (M_TRAILINGSPACE(mp) < hlen + PPP_HDRLEN) {
				/* Lose if big headers and no clusters */
				m_freem(mp);
				goto bad;
			}
		}
		cp = mtod(mp, u_char *);
		cp[0] = adrs;
		cp[1] = ctrl;
		cp[2] = 0;
		cp[3] = PPP_IP;
		proto = PPP_IP;
		bcopy(iphdr, cp + PPP_HDRLEN, hlen);
		mp->m_len = hlen + PPP_HDRLEN;

		/*
		 * Trim the PPP and VJ headers off the old mbuf
		 * and stick the new and old mbufs together.
		 */
		m->m_data += PPP_HDRLEN + xlen;
		m->m_len -= PPP_HDRLEN + xlen;
		if (m->m_len <= M_TRAILINGSPACE(mp)) {
			bcopy(mtod(m, u_char *),
			    mtod(mp, u_char *) + mp->m_len, m->m_len);
			mp->m_len += m->m_len;
			MFREE(m, mp->m_next);
		} else
			mp->m_next = m;
		m = mp;
		ilen += hlen - xlen;

	} else if (proto == PPP_VJC_UNCOMP) {
		if ((sc->sc_flags & SC_REJ_COMP_TCP) || sc->sc_comp == 0)
			goto bad;

		xlen = sl_uncompress_tcp_core(cp + PPP_HDRLEN,
		    m->m_len - PPP_HDRLEN, ilen - PPP_HDRLEN,
		    TYPE_UNCOMPRESSED_TCP, sc->sc_comp, &iphdr, &hlen);

		if (xlen < 0) {
			if (sc->sc_flags & SC_DEBUG)
				printf("%s: VJ uncompress failed on type uncomp\n",
				    ifp->if_xname);
			goto bad;
		}

		proto = PPP_IP;
		cp[3] = PPP_IP;
	}
#endif /* VJC */

	/*
	 * If the packet will fit in a header mbuf, don't waste a
	 * whole cluster on it.
	 */
	if (ilen <= MHLEN && M_IS_CLUSTER(m)) {
		MGETHDR(mp, M_DONTWAIT, MT_DATA);
		if (mp != NULL) {
			m_copydata(m, 0, ilen, mtod(mp, void *));
			m_freem(m);
			m = mp;
			m->m_len = ilen;
		}
	}
	m->m_pkthdr.len = ilen;
	m->m_pkthdr.rcvif = ifp;

	if ((proto & 0x8000) == 0) {
#ifdef PPP_FILTER
		/*
		 * See whether we want to pass this packet, and
		 * if it counts as link activity.
		 */
		if (sc->sc_pass_filt_in.bf_insns != 0
		    && bpf_filter(sc->sc_pass_filt_in.bf_insns,
			(u_char *)m, ilen, 0) == 0) {
			/* drop this packet */
			m_freem(m);
			return;
		}
		if (sc->sc_active_filt_in.bf_insns == 0
		    || bpf_filter(sc->sc_active_filt_in.bf_insns,
			(u_char *)m, ilen, 0))
			sc->sc_last_recv = time_second;
#else
		/*
		 * Record the time that we received this packet.
		 */
		sc->sc_last_recv = time_second;
#endif /* PPP_FILTER */
	}

	/* See if bpf wants to look at the packet. */
	bpf_mtap(&sc->sc_if, m);

	switch (proto) {
#ifdef INET
	case PPP_IP:
		/*
		 * IP packet - take off the ppp header and pass it up to IP.
		 */
		if ((ifp->if_flags & IFF_UP) == 0
		    || sc->sc_npmode[NP_IP] != NPMODE_PASS) {
			/* Interface is down - drop the packet. */
			m_freem(m);
			return;
		}
		m->m_pkthdr.len -= PPP_HDRLEN;
		m->m_data += PPP_HDRLEN;
		m->m_len -= PPP_HDRLEN;
#ifdef GATEWAY
		if (ipflow_fastforward(m))
			return;
#endif
		pktq = ip_pktq;
		break;
#endif

#ifdef INET6
	case PPP_IPV6:
		/*
		 * IPv6 packet - take off the ppp header and pass it up to
		 * IPv6.
		 */
		if ((ifp->if_flags & IFF_UP) == 0
		    || sc->sc_npmode[NP_IPV6] != NPMODE_PASS) {
			/* interface is down - drop the packet. */
			m_freem(m);
			return;
		}
		m->m_pkthdr.len -= PPP_HDRLEN;
		m->m_data += PPP_HDRLEN;
		m->m_len -= PPP_HDRLEN;
#ifdef GATEWAY	
		if (ip6flow_fastforward(&m))
			return;
#endif
		pktq = ip6_pktq;
		break;
#endif

	default:
		/*
		 * Some other protocol - place on input queue for read().
		 */
		inq = &sc->sc_inq;
		pktq = NULL;
		break;
	}

	/*
	 * Put the packet on the appropriate input queue.
	 */
	s = splnet();

	/* pktq: inet or inet6 cases */
	if (__predict_true(pktq)) {
		if (__predict_false(!pktq_enqueue(pktq, m, 0))) {
			ifp->if_iqdrops++;
			goto bad;
		}
		ifp->if_ipackets++;
		ifp->if_ibytes += ilen;
		splx(s);
		return;
	}

	/* ifq: other protocol cases */
	if (!inq) {
		goto bad;
	}
	if (IF_QFULL(inq)) {
		IF_DROP(inq);
		splx(s);
		if (sc->sc_flags & SC_DEBUG)
			printf("%s: input queue full\n", ifp->if_xname);
		ifp->if_iqdrops++;
		goto bad;
	}
	IF_ENQUEUE(inq, m);
	splx(s);
	ifp->if_ipackets++;
	ifp->if_ibytes += ilen;

	(*sc->sc_ctlp)(sc);

	return;

bad:
	m_freem(m);
	sc->sc_if.if_ierrors++;
	sc->sc_stats.ppp_ierrors++;
}

#define MAX_DUMP_BYTES	128

static void
pppdumpm(struct mbuf *m0)
{
	char buf[3*MAX_DUMP_BYTES+4];
	char *bp = buf;
	struct mbuf *m;

	for (m = m0; m; m = m->m_next) {
		int l = m->m_len;
		u_char *rptr = (u_char *)m->m_data;

		while (l--) {
			if (bp > buf + sizeof(buf) - 4)
				goto done;
			/* Convert byte to ascii hex */
			*bp++ = hexdigits[*rptr >> 4];
			*bp++ = hexdigits[*rptr++ & 0xf];
		}

		if (m->m_next) {
			if (bp > buf + sizeof(buf) - 3)
				goto done;
			*bp++ = '|';
		} else
			*bp++ = ' ';
	}
done:
	if (m)
		*bp++ = '>';
	*bp = 0;
	printf("%s\n", buf);
}

#ifdef ALTQ
/*
 * A wrapper to transmit a packet from if_start since ALTQ uses
 * if_start to send a packet.
 */
static void
ppp_ifstart(struct ifnet *ifp)
{
	struct ppp_softc *sc;

	sc = ifp->if_softc;
	(*sc->sc_start)(sc);
}
#endif

static const struct ppp_known_compressor {
	uint8_t code;
	const char *module;
} ppp_known_compressors[] = {
	{ CI_DEFLATE, "ppp_deflate" },
	{ CI_DEFLATE_DRAFT, "ppp_deflate" },
	{ CI_BSD_COMPRESS, "ppp_bsdcomp" },
	{ CI_MPPE, "ppp_mppe" },
	{ 0, NULL }
};

static int
ppp_compressor_init(void)
{

	mutex_init(&ppp_compressors_mtx, MUTEX_DEFAULT, IPL_NONE);
	return 0;
}

static void
ppp_compressor_rele(struct compressor *cp)
{

	mutex_enter(&ppp_compressors_mtx);
	--cp->comp_refcnt;
	mutex_exit(&ppp_compressors_mtx);
}

static struct compressor *
ppp_get_compressor_noload(uint8_t ci, bool hold)
{
	struct compressor *cp;

	KASSERT(mutex_owned(&ppp_compressors_mtx));
	LIST_FOREACH(cp, &ppp_compressors, comp_list) {
		if (cp->compress_proto == ci) {
			if (hold)
				++cp->comp_refcnt;
			return cp;
		}
	}

	return NULL;
}

static struct compressor *
ppp_get_compressor(uint8_t ci)
{
	struct compressor *cp = NULL;
	const struct ppp_known_compressor *pkc;

	mutex_enter(&ppp_compressors_mtx);
	cp = ppp_get_compressor_noload(ci, true);
	mutex_exit(&ppp_compressors_mtx);
	if (cp != NULL)
		return cp;

	kernconfig_lock();
	mutex_enter(&ppp_compressors_mtx);
	cp = ppp_get_compressor_noload(ci, true);
	mutex_exit(&ppp_compressors_mtx);
	if (cp == NULL) {
		/* Not found, so try to autoload a module */
		for (pkc = ppp_known_compressors; pkc->module != NULL; pkc++) {
			if (pkc->code == ci) {
				if (module_autoload(pkc->module,
					MODULE_CLASS_MISC) != 0)
					break;
				mutex_enter(&ppp_compressors_mtx);
				cp = ppp_get_compressor_noload(ci, true);
				mutex_exit(&ppp_compressors_mtx);
				break;
			}
		}
	}
	kernconfig_unlock();

	return cp;
}

int
ppp_register_compressor(struct compressor *pc, size_t ncomp)
{
	int error = 0;
	size_t i;

	RUN_ONCE(&ppp_compressor_mtx_init, ppp_compressor_init);

	mutex_enter(&ppp_compressors_mtx);
	for (i = 0; i < ncomp; i++) {
		if (ppp_get_compressor_noload(pc[i].compress_proto,
			false) != NULL)
			error = EEXIST;
	}
	if (!error) {
		for (i = 0; i < ncomp; i++) {
			pc[i].comp_refcnt = 0;
			LIST_INSERT_HEAD(&ppp_compressors, &pc[i], comp_list);
		}
	}
	mutex_exit(&ppp_compressors_mtx);

	return error;
}

int
ppp_unregister_compressor(struct compressor *pc, size_t ncomp)
{
	int error = 0;
	size_t i;

	mutex_enter(&ppp_compressors_mtx);
	for (i = 0; i < ncomp; i++) {
		if (ppp_get_compressor_noload(pc[i].compress_proto,
			false) != &pc[i])
			error = ENOENT;
		else if (pc[i].comp_refcnt != 0)
			error = EBUSY;
	}
	if (!error) {
		for (i = 0; i < ncomp; i++) {
			LIST_REMOVE(&pc[i], comp_list);
		}
	}
	mutex_exit(&ppp_compressors_mtx);

	return error;
}
