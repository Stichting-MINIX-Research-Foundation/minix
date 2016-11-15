/*	$NetBSD: bpf.c,v 1.191 2015/05/30 19:14:46 joerg Exp $	*/

/*
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
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
 *	@(#)bpf.c	8.4 (Berkeley) 1/9/95
 * static char rcsid[] =
 * "Header: bpf.c,v 1.67 96/09/26 22:00:52 leres Exp ";
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bpf.c,v 1.191 2015/05/30 19:14:46 joerg Exp $");

#if defined(_KERNEL_OPT)
#include "opt_bpf.h"
#include "sl.h"
#include "strip.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/module.h>
#include <sys/once.h>
#include <sys/atomic.h>

#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/tty.h>
#include <sys/uio.h>

#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/poll.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>

#include <net/if.h>
#include <net/slip.h>

#include <net/bpf.h>
#include <net/bpfdesc.h>
#include <net/bpfjit.h>

#include <net/if_arc.h>
#include <net/if_ether.h>

#include <netinet/in.h>
#include <netinet/if_inarp.h>


#include <compat/sys/sockio.h>

#ifndef BPF_BUFSIZE
/*
 * 4096 is too small for FDDI frames. 8192 is too small for gigabit Ethernet
 * jumbos (circa 9k), ATM, or Intel gig/10gig ethernet jumbos (16k).
 */
# define BPF_BUFSIZE 32768
#endif

#define PRINET  26			/* interruptible */

/*
 * The default read buffer size, and limit for BIOCSBLEN, is sysctl'able.
 * XXX the default values should be computed dynamically based
 * on available memory size and available mbuf clusters.
 */
int bpf_bufsize = BPF_BUFSIZE;
int bpf_maxbufsize = BPF_DFLTBUFSIZE;	/* XXX set dynamically, see above */
bool bpf_jit = false;

struct bpfjit_ops bpfjit_module_ops = {
	.bj_generate_code = NULL,
	.bj_free_code = NULL
};

/*
 * Global BPF statistics returned by net.bpf.stats sysctl.
 */
struct bpf_stat	bpf_gstats;

/*
 * Use a mutex to avoid a race condition between gathering the stats/peers
 * and opening/closing the device.
 */
static kmutex_t bpf_mtx;

/*
 *  bpf_iflist is the list of interfaces; each corresponds to an ifnet
 *  bpf_dtab holds the descriptors, indexed by minor device #
 */
struct bpf_if	*bpf_iflist;
LIST_HEAD(, bpf_d) bpf_list;

static int	bpf_allocbufs(struct bpf_d *);
static void	bpf_deliver(struct bpf_if *,
		            void *(*cpfn)(void *, const void *, size_t),
		            void *, u_int, u_int, const bool);
static void	bpf_freed(struct bpf_d *);
static void	bpf_ifname(struct ifnet *, struct ifreq *);
static void	*bpf_mcpy(void *, const void *, size_t);
static int	bpf_movein(struct uio *, int, uint64_t,
			        struct mbuf **, struct sockaddr *);
static void	bpf_attachd(struct bpf_d *, struct bpf_if *);
static void	bpf_detachd(struct bpf_d *);
static int	bpf_setif(struct bpf_d *, struct ifreq *);
static void	bpf_timed_out(void *);
static inline void
		bpf_wakeup(struct bpf_d *);
static int	bpf_hdrlen(struct bpf_d *);
static void	catchpacket(struct bpf_d *, u_char *, u_int, u_int,
    void *(*)(void *, const void *, size_t), struct timespec *);
static void	reset_d(struct bpf_d *);
static int	bpf_getdltlist(struct bpf_d *, struct bpf_dltlist *);
static int	bpf_setdlt(struct bpf_d *, u_int);

static int	bpf_read(struct file *, off_t *, struct uio *, kauth_cred_t,
    int);
static int	bpf_write(struct file *, off_t *, struct uio *, kauth_cred_t,
    int);
static int	bpf_ioctl(struct file *, u_long, void *);
static int	bpf_poll(struct file *, int);
static int	bpf_stat(struct file *, struct stat *);
static int	bpf_close(struct file *);
static int	bpf_kqfilter(struct file *, struct knote *);
static void	bpf_softintr(void *);

static const struct fileops bpf_fileops = {
	.fo_read = bpf_read,
	.fo_write = bpf_write,
	.fo_ioctl = bpf_ioctl,
	.fo_fcntl = fnullop_fcntl,
	.fo_poll = bpf_poll,
	.fo_stat = bpf_stat,
	.fo_close = bpf_close,
	.fo_kqfilter = bpf_kqfilter,
	.fo_restart = fnullop_restart,
};

dev_type_open(bpfopen);

const struct cdevsw bpf_cdevsw = {
	.d_open = bpfopen,
	.d_close = noclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

bpfjit_func_t
bpf_jit_generate(bpf_ctx_t *bc, void *code, size_t size)
{

	membar_consumer();
	if (bpfjit_module_ops.bj_generate_code != NULL) {
		return bpfjit_module_ops.bj_generate_code(bc, code, size);
	}
	return NULL;
}

void
bpf_jit_freecode(bpfjit_func_t jcode)
{
	KASSERT(bpfjit_module_ops.bj_free_code != NULL);
	bpfjit_module_ops.bj_free_code(jcode);
}

static int
bpf_movein(struct uio *uio, int linktype, uint64_t mtu, struct mbuf **mp,
	   struct sockaddr *sockp)
{
	struct mbuf *m;
	int error;
	size_t len;
	size_t hlen;
	size_t align;

	/*
	 * Build a sockaddr based on the data link layer type.
	 * We do this at this level because the ethernet header
	 * is copied directly into the data field of the sockaddr.
	 * In the case of SLIP, there is no header and the packet
	 * is forwarded as is.
	 * Also, we are careful to leave room at the front of the mbuf
	 * for the link level header.
	 */
	switch (linktype) {

	case DLT_SLIP:
		sockp->sa_family = AF_INET;
		hlen = 0;
		align = 0;
		break;

	case DLT_PPP:
		sockp->sa_family = AF_UNSPEC;
		hlen = 0;
		align = 0;
		break;

	case DLT_EN10MB:
		sockp->sa_family = AF_UNSPEC;
		/* XXX Would MAXLINKHDR be better? */
 		/* 6(dst)+6(src)+2(type) */
		hlen = sizeof(struct ether_header);
		align = 2;
		break;

	case DLT_ARCNET:
		sockp->sa_family = AF_UNSPEC;
		hlen = ARC_HDRLEN;
		align = 5;
		break;

	case DLT_FDDI:
		sockp->sa_family = AF_LINK;
		/* XXX 4(FORMAC)+6(dst)+6(src) */
		hlen = 16;
		align = 0;
		break;

	case DLT_ECONET:
		sockp->sa_family = AF_UNSPEC;
		hlen = 6;
		align = 2;
		break;

	case DLT_NULL:
		sockp->sa_family = AF_UNSPEC;
		hlen = 0;
		align = 0;
		break;

	default:
		return (EIO);
	}

	len = uio->uio_resid;
	/*
	 * If there aren't enough bytes for a link level header or the
	 * packet length exceeds the interface mtu, return an error.
	 */
	if (len - hlen > mtu)
		return (EMSGSIZE);

	/*
	 * XXX Avoid complicated buffer chaining ---
	 * bail if it won't fit in a single mbuf.
	 * (Take into account possible alignment bytes)
	 */
	if (len + align > MCLBYTES)
		return (EIO);

	m = m_gethdr(M_WAIT, MT_DATA);
	m->m_pkthdr.rcvif = NULL;
	m->m_pkthdr.len = (int)(len - hlen);
	if (len + align > MHLEN) {
		m_clget(m, M_WAIT);
		if ((m->m_flags & M_EXT) == 0) {
			error = ENOBUFS;
			goto bad;
		}
	}

	/* Insure the data is properly aligned */
	if (align > 0) {
		m->m_data += align;
		m->m_len -= (int)align;
	}

	error = uiomove(mtod(m, void *), len, uio);
	if (error)
		goto bad;
	if (hlen != 0) {
		memcpy(sockp->sa_data, mtod(m, void *), hlen);
		m->m_data += hlen; /* XXX */
		len -= hlen;
	}
	m->m_len = (int)len;
	*mp = m;
	return (0);

bad:
	m_freem(m);
	return (error);
}

/*
 * Attach file to the bpf interface, i.e. make d listen on bp.
 * Must be called at splnet.
 */
static void
bpf_attachd(struct bpf_d *d, struct bpf_if *bp)
{
	/*
	 * Point d at bp, and add d to the interface's list of listeners.
	 * Finally, point the driver's bpf cookie at the interface so
	 * it will divert packets to bpf.
	 */
	d->bd_bif = bp;
	d->bd_next = bp->bif_dlist;
	bp->bif_dlist = d;

	*bp->bif_driverp = bp;
}

/*
 * Detach a file from its interface.
 */
static void
bpf_detachd(struct bpf_d *d)
{
	struct bpf_d **p;
	struct bpf_if *bp;

	bp = d->bd_bif;
	/*
	 * Check if this descriptor had requested promiscuous mode.
	 * If so, turn it off.
	 */
	if (d->bd_promisc) {
		int error __diagused;

		d->bd_promisc = 0;
		/*
		 * Take device out of promiscuous mode.  Since we were
		 * able to enter promiscuous mode, we should be able
		 * to turn it off.  But we can get an error if
		 * the interface was configured down, so only panic
		 * if we don't get an unexpected error.
		 */
  		error = ifpromisc(bp->bif_ifp, 0);
#ifdef DIAGNOSTIC
		if (error)
			printf("%s: ifpromisc failed: %d", __func__, error);
#endif
	}
	/* Remove d from the interface's descriptor list. */
	p = &bp->bif_dlist;
	while (*p != d) {
		p = &(*p)->bd_next;
		if (*p == NULL)
			panic("%s: descriptor not in list", __func__);
	}
	*p = (*p)->bd_next;
	if (bp->bif_dlist == NULL)
		/*
		 * Let the driver know that there are no more listeners.
		 */
		*d->bd_bif->bif_driverp = NULL;
	d->bd_bif = NULL;
}

static int
doinit(void)
{

	mutex_init(&bpf_mtx, MUTEX_DEFAULT, IPL_NONE);

	LIST_INIT(&bpf_list);

	bpf_gstats.bs_recv = 0;
	bpf_gstats.bs_drop = 0;
	bpf_gstats.bs_capt = 0;

	return 0;
}

/*
 * bpfilterattach() is called at boot time.
 */
/* ARGSUSED */
void
bpfilterattach(int n)
{
	static ONCE_DECL(control);

	RUN_ONCE(&control, doinit);
}

/*
 * Open ethernet device. Clones.
 */
/* ARGSUSED */
int
bpfopen(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct bpf_d *d;
	struct file *fp;
	int error, fd;

	/* falloc() will fill in the descriptor for us. */
	if ((error = fd_allocfile(&fp, &fd)) != 0)
		return error;

	d = malloc(sizeof(*d), M_DEVBUF, M_WAITOK|M_ZERO);
	d->bd_bufsize = bpf_bufsize;
	d->bd_seesent = 1;
	d->bd_feedback = 0;
	d->bd_pid = l->l_proc->p_pid;
#ifdef _LP64
	if (curproc->p_flag & PK_32)
		d->bd_compat32 = 1;
#endif
	getnanotime(&d->bd_btime);
	d->bd_atime = d->bd_mtime = d->bd_btime;
	callout_init(&d->bd_callout, 0);
	selinit(&d->bd_sel);
	d->bd_sih = softint_establish(SOFTINT_CLOCK, bpf_softintr, d);
	d->bd_jitcode = NULL;

	mutex_enter(&bpf_mtx);
	LIST_INSERT_HEAD(&bpf_list, d, bd_list);
	mutex_exit(&bpf_mtx);

	return fd_clone(fp, fd, flag, &bpf_fileops, d);
}

/*
 * Close the descriptor by detaching it from its interface,
 * deallocating its buffers, and marking it free.
 */
/* ARGSUSED */
static int
bpf_close(struct file *fp)
{
	struct bpf_d *d = fp->f_bpf;
	int s;

	KERNEL_LOCK(1, NULL);

	/*
	 * Refresh the PID associated with this bpf file.
	 */
	d->bd_pid = curproc->p_pid;

	s = splnet();
	if (d->bd_state == BPF_WAITING)
		callout_stop(&d->bd_callout);
	d->bd_state = BPF_IDLE;
	if (d->bd_bif)
		bpf_detachd(d);
	splx(s);
	bpf_freed(d);
	mutex_enter(&bpf_mtx);
	LIST_REMOVE(d, bd_list);
	mutex_exit(&bpf_mtx);
	callout_destroy(&d->bd_callout);
	seldestroy(&d->bd_sel);
	softint_disestablish(d->bd_sih);
	free(d, M_DEVBUF);
	fp->f_bpf = NULL;

	KERNEL_UNLOCK_ONE(NULL);

	return (0);
}

/*
 * Rotate the packet buffers in descriptor d.  Move the store buffer
 * into the hold slot, and the free buffer into the store slot.
 * Zero the length of the new store buffer.
 */
#define ROTATE_BUFFERS(d) \
	(d)->bd_hbuf = (d)->bd_sbuf; \
	(d)->bd_hlen = (d)->bd_slen; \
	(d)->bd_sbuf = (d)->bd_fbuf; \
	(d)->bd_slen = 0; \
	(d)->bd_fbuf = NULL;
/*
 *  bpfread - read next chunk of packets from buffers
 */
static int
bpf_read(struct file *fp, off_t *offp, struct uio *uio,
    kauth_cred_t cred, int flags)
{
	struct bpf_d *d = fp->f_bpf;
	int timed_out;
	int error;
	int s;

	getnanotime(&d->bd_atime);
	/*
	 * Restrict application to use a buffer the same size as
	 * the kernel buffers.
	 */
	if (uio->uio_resid != d->bd_bufsize)
		return (EINVAL);

	KERNEL_LOCK(1, NULL);
	s = splnet();
	if (d->bd_state == BPF_WAITING)
		callout_stop(&d->bd_callout);
	timed_out = (d->bd_state == BPF_TIMED_OUT);
	d->bd_state = BPF_IDLE;
	/*
	 * If the hold buffer is empty, then do a timed sleep, which
	 * ends when the timeout expires or when enough packets
	 * have arrived to fill the store buffer.
	 */
	while (d->bd_hbuf == NULL) {
		if (fp->f_flag & FNONBLOCK) {
			if (d->bd_slen == 0) {
				splx(s);
				KERNEL_UNLOCK_ONE(NULL);
				return (EWOULDBLOCK);
			}
			ROTATE_BUFFERS(d);
			break;
		}

		if ((d->bd_immediate || timed_out) && d->bd_slen != 0) {
			/*
			 * A packet(s) either arrived since the previous
			 * read or arrived while we were asleep.
			 * Rotate the buffers and return what's here.
			 */
			ROTATE_BUFFERS(d);
			break;
		}
		error = tsleep(d, PRINET|PCATCH, "bpf",
				d->bd_rtout);
		if (error == EINTR || error == ERESTART) {
			splx(s);
			KERNEL_UNLOCK_ONE(NULL);
			return (error);
		}
		if (error == EWOULDBLOCK) {
			/*
			 * On a timeout, return what's in the buffer,
			 * which may be nothing.  If there is something
			 * in the store buffer, we can rotate the buffers.
			 */
			if (d->bd_hbuf)
				/*
				 * We filled up the buffer in between
				 * getting the timeout and arriving
				 * here, so we don't need to rotate.
				 */
				break;

			if (d->bd_slen == 0) {
				splx(s);
				KERNEL_UNLOCK_ONE(NULL);
				return (0);
			}
			ROTATE_BUFFERS(d);
			break;
		}
		if (error != 0)
			goto done;
	}
	/*
	 * At this point, we know we have something in the hold slot.
	 */
	splx(s);

	/*
	 * Move data from hold buffer into user space.
	 * We know the entire buffer is transferred since
	 * we checked above that the read buffer is bpf_bufsize bytes.
	 */
	error = uiomove(d->bd_hbuf, d->bd_hlen, uio);

	s = splnet();
	d->bd_fbuf = d->bd_hbuf;
	d->bd_hbuf = NULL;
	d->bd_hlen = 0;
done:
	splx(s);
	KERNEL_UNLOCK_ONE(NULL);
	return (error);
}


/*
 * If there are processes sleeping on this descriptor, wake them up.
 */
static inline void
bpf_wakeup(struct bpf_d *d)
{
	wakeup(d);
	if (d->bd_async)
		softint_schedule(d->bd_sih);
	selnotify(&d->bd_sel, 0, 0);
}

static void
bpf_softintr(void *cookie)
{
	struct bpf_d *d;

	d = cookie;
	if (d->bd_async)
		fownsignal(d->bd_pgid, SIGIO, 0, 0, NULL);
}

static void
bpf_timed_out(void *arg)
{
	struct bpf_d *d = arg;
	int s;

	s = splnet();
	if (d->bd_state == BPF_WAITING) {
		d->bd_state = BPF_TIMED_OUT;
		if (d->bd_slen != 0)
			bpf_wakeup(d);
	}
	splx(s);
}


static int
bpf_write(struct file *fp, off_t *offp, struct uio *uio,
    kauth_cred_t cred, int flags)
{
	struct bpf_d *d = fp->f_bpf;
	struct ifnet *ifp;
	struct mbuf *m, *mc;
	int error, s;
	static struct sockaddr_storage dst;

	m = NULL;	/* XXX gcc */

	KERNEL_LOCK(1, NULL);

	if (d->bd_bif == NULL) {
		KERNEL_UNLOCK_ONE(NULL);
		return (ENXIO);
	}
	getnanotime(&d->bd_mtime);

	ifp = d->bd_bif->bif_ifp;

	if (uio->uio_resid == 0) {
		KERNEL_UNLOCK_ONE(NULL);
		return (0);
	}

	error = bpf_movein(uio, (int)d->bd_bif->bif_dlt, ifp->if_mtu, &m,
		(struct sockaddr *) &dst);
	if (error) {
		KERNEL_UNLOCK_ONE(NULL);
		return (error);
	}

	if (m->m_pkthdr.len > ifp->if_mtu) {
		KERNEL_UNLOCK_ONE(NULL);
		m_freem(m);
		return (EMSGSIZE);
	}

	if (d->bd_hdrcmplt)
		dst.ss_family = pseudo_AF_HDRCMPLT;

	if (d->bd_feedback) {
		mc = m_dup(m, 0, M_COPYALL, M_NOWAIT);
		if (mc != NULL)
			mc->m_pkthdr.rcvif = ifp;
		/* Set M_PROMISC for outgoing packets to be discarded. */
		if (1 /*d->bd_direction == BPF_D_INOUT*/)
			m->m_flags |= M_PROMISC;
	} else  
		mc = NULL;

	s = splsoftnet();
	error = (*ifp->if_output)(ifp, m, (struct sockaddr *) &dst, NULL);

	if (mc != NULL) {
		if (error == 0)
			(*ifp->if_input)(ifp, mc);
		m_freem(mc);
	}
	splx(s);
	KERNEL_UNLOCK_ONE(NULL);
	/*
	 * The driver frees the mbuf.
	 */
	return (error);
}

/*
 * Reset a descriptor by flushing its packet buffer and clearing the
 * receive and drop counts.  Should be called at splnet.
 */
static void
reset_d(struct bpf_d *d)
{
	if (d->bd_hbuf) {
		/* Free the hold buffer. */
		d->bd_fbuf = d->bd_hbuf;
		d->bd_hbuf = NULL;
	}
	d->bd_slen = 0;
	d->bd_hlen = 0;
	d->bd_rcount = 0;
	d->bd_dcount = 0;
	d->bd_ccount = 0;
}

/*
 *  FIONREAD		Check for read packet available.
 *  BIOCGBLEN		Get buffer len [for read()].
 *  BIOCSETF		Set ethernet read filter.
 *  BIOCFLUSH		Flush read packet buffer.
 *  BIOCPROMISC		Put interface into promiscuous mode.
 *  BIOCGDLT		Get link layer type.
 *  BIOCGETIF		Get interface name.
 *  BIOCSETIF		Set interface.
 *  BIOCSRTIMEOUT	Set read timeout.
 *  BIOCGRTIMEOUT	Get read timeout.
 *  BIOCGSTATS		Get packet stats.
 *  BIOCIMMEDIATE	Set immediate mode.
 *  BIOCVERSION		Get filter language version.
 *  BIOCGHDRCMPLT	Get "header already complete" flag.
 *  BIOCSHDRCMPLT	Set "header already complete" flag.
 *  BIOCSFEEDBACK	Set packet feedback mode.
 *  BIOCGFEEDBACK	Get packet feedback mode.
 *  BIOCGSEESENT  	Get "see sent packets" mode.
 *  BIOCSSEESENT  	Set "see sent packets" mode.
 */
/* ARGSUSED */
static int
bpf_ioctl(struct file *fp, u_long cmd, void *addr)
{
	struct bpf_d *d = fp->f_bpf;
	int s, error = 0;

	/*
	 * Refresh the PID associated with this bpf file.
	 */
	KERNEL_LOCK(1, NULL);
	d->bd_pid = curproc->p_pid;
#ifdef _LP64
	if (curproc->p_flag & PK_32)
		d->bd_compat32 = 1;
	else
		d->bd_compat32 = 0;
#endif

	s = splnet();
	if (d->bd_state == BPF_WAITING)
		callout_stop(&d->bd_callout);
	d->bd_state = BPF_IDLE;
	splx(s);

	switch (cmd) {

	default:
		error = EINVAL;
		break;

	/*
	 * Check for read packet available.
	 */
	case FIONREAD:
		{
			int n;

			s = splnet();
			n = d->bd_slen;
			if (d->bd_hbuf)
				n += d->bd_hlen;
			splx(s);

			*(int *)addr = n;
			break;
		}

	/*
	 * Get buffer len [for read()].
	 */
	case BIOCGBLEN:
		*(u_int *)addr = d->bd_bufsize;
		break;

	/*
	 * Set buffer length.
	 */
	case BIOCSBLEN:
		if (d->bd_bif != NULL)
			error = EINVAL;
		else {
			u_int size = *(u_int *)addr;

			if (size > bpf_maxbufsize)
				*(u_int *)addr = size = bpf_maxbufsize;
			else if (size < BPF_MINBUFSIZE)
				*(u_int *)addr = size = BPF_MINBUFSIZE;
			d->bd_bufsize = size;
		}
		break;

	/*
	 * Set link layer read filter.
	 */
	case BIOCSETF:
		error = bpf_setf(d, addr);
		break;

	/*
	 * Flush read packet buffer.
	 */
	case BIOCFLUSH:
		s = splnet();
		reset_d(d);
		splx(s);
		break;

	/*
	 * Put interface into promiscuous mode.
	 */
	case BIOCPROMISC:
		if (d->bd_bif == NULL) {
			/*
			 * No interface attached yet.
			 */
			error = EINVAL;
			break;
		}
		s = splnet();
		if (d->bd_promisc == 0) {
			error = ifpromisc(d->bd_bif->bif_ifp, 1);
			if (error == 0)
				d->bd_promisc = 1;
		}
		splx(s);
		break;

	/*
	 * Get device parameters.
	 */
	case BIOCGDLT:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			*(u_int *)addr = d->bd_bif->bif_dlt;
		break;

	/*
	 * Get a list of supported device parameters.
	 */
	case BIOCGDLTLIST:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			error = bpf_getdltlist(d, addr);
		break;

	/*
	 * Set device parameters.
	 */
	case BIOCSDLT:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			error = bpf_setdlt(d, *(u_int *)addr);
		break;

	/*
	 * Set interface name.
	 */
#ifdef OBIOCGETIF
	case OBIOCGETIF:
#endif
	case BIOCGETIF:
		if (d->bd_bif == NULL)
			error = EINVAL;
		else
			bpf_ifname(d->bd_bif->bif_ifp, addr);
		break;

	/*
	 * Set interface.
	 */
#ifdef OBIOCSETIF
	case OBIOCSETIF:
#endif
	case BIOCSETIF:
		error = bpf_setif(d, addr);
		break;

	/*
	 * Set read timeout.
	 */
	case BIOCSRTIMEOUT:
		{
			struct timeval *tv = addr;

			/* Compute number of ticks. */
			d->bd_rtout = tv->tv_sec * hz + tv->tv_usec / tick;
			if ((d->bd_rtout == 0) && (tv->tv_usec != 0))
				d->bd_rtout = 1;
			break;
		}

#ifdef BIOCGORTIMEOUT
	/*
	 * Get read timeout.
	 */
	case BIOCGORTIMEOUT:
		{
			struct timeval50 *tv = addr;

			tv->tv_sec = d->bd_rtout / hz;
			tv->tv_usec = (d->bd_rtout % hz) * tick;
			break;
		}
#endif

#ifdef BIOCSORTIMEOUT
	/*
	 * Set read timeout.
	 */
	case BIOCSORTIMEOUT:
		{
			struct timeval50 *tv = addr;

			/* Compute number of ticks. */
			d->bd_rtout = tv->tv_sec * hz + tv->tv_usec / tick;
			if ((d->bd_rtout == 0) && (tv->tv_usec != 0))
				d->bd_rtout = 1;
			break;
		}
#endif

	/*
	 * Get read timeout.
	 */
	case BIOCGRTIMEOUT:
		{
			struct timeval *tv = addr;

			tv->tv_sec = d->bd_rtout / hz;
			tv->tv_usec = (d->bd_rtout % hz) * tick;
			break;
		}
	/*
	 * Get packet stats.
	 */
	case BIOCGSTATS:
		{
			struct bpf_stat *bs = addr;

			bs->bs_recv = d->bd_rcount;
			bs->bs_drop = d->bd_dcount;
			bs->bs_capt = d->bd_ccount;
			break;
		}

	case BIOCGSTATSOLD:
		{
			struct bpf_stat_old *bs = addr;

			bs->bs_recv = d->bd_rcount;
			bs->bs_drop = d->bd_dcount;
			break;
		}

	/*
	 * Set immediate mode.
	 */
	case BIOCIMMEDIATE:
		d->bd_immediate = *(u_int *)addr;
		break;

	case BIOCVERSION:
		{
			struct bpf_version *bv = addr;

			bv->bv_major = BPF_MAJOR_VERSION;
			bv->bv_minor = BPF_MINOR_VERSION;
			break;
		}

	case BIOCGHDRCMPLT:	/* get "header already complete" flag */
		*(u_int *)addr = d->bd_hdrcmplt;
		break;

	case BIOCSHDRCMPLT:	/* set "header already complete" flag */
		d->bd_hdrcmplt = *(u_int *)addr ? 1 : 0;
		break;

	/*
	 * Get "see sent packets" flag
	 */
	case BIOCGSEESENT:
		*(u_int *)addr = d->bd_seesent;
		break;

	/*
	 * Set "see sent" packets flag
	 */
	case BIOCSSEESENT:
		d->bd_seesent = *(u_int *)addr;
		break;

	/*
	 * Set "feed packets from bpf back to input" mode
	 */
	case BIOCSFEEDBACK:
		d->bd_feedback = *(u_int *)addr;
		break;

	/*
	 * Get "feed packets from bpf back to input" mode
	 */
	case BIOCGFEEDBACK:
		*(u_int *)addr = d->bd_feedback;
		break;

	case FIONBIO:		/* Non-blocking I/O */
		/*
		 * No need to do anything special as we use IO_NDELAY in
		 * bpfread() as an indication of whether or not to block
		 * the read.
		 */
		break;

	case FIOASYNC:		/* Send signal on receive packets */
		d->bd_async = *(int *)addr;
		break;

	case TIOCSPGRP:		/* Process or group to send signals to */
	case FIOSETOWN:
		error = fsetown(&d->bd_pgid, cmd, addr);
		break;

	case TIOCGPGRP:
	case FIOGETOWN:
		error = fgetown(d->bd_pgid, cmd, addr);
		break;
	}
	KERNEL_UNLOCK_ONE(NULL);
	return (error);
}

/*
 * Set d's packet filter program to fp.  If this file already has a filter,
 * free it and replace it.  Returns EINVAL for bogus requests.
 */
int
bpf_setf(struct bpf_d *d, struct bpf_program *fp)
{
	struct bpf_insn *fcode, *old;
	bpfjit_func_t jcode, oldj;
	size_t flen, size;
	int s;

	jcode = NULL;
	flen = fp->bf_len;

	if ((fp->bf_insns == NULL && flen) || flen > BPF_MAXINSNS) {
		return EINVAL;
	}

	if (flen) {
		/*
		 * Allocate the buffer, copy the byte-code from
		 * userspace and validate it.
		 */
		size = flen * sizeof(*fp->bf_insns);
		fcode = malloc(size, M_DEVBUF, M_WAITOK);
		if (copyin(fp->bf_insns, fcode, size) != 0 ||
		    !bpf_validate(fcode, (int)flen)) {
			free(fcode, M_DEVBUF);
			return EINVAL;
		}
		membar_consumer();
		if (bpf_jit)
			jcode = bpf_jit_generate(NULL, fcode, flen);
	} else {
		fcode = NULL;
	}

	s = splnet();
	old = d->bd_filter;
	d->bd_filter = fcode;
	oldj = d->bd_jitcode;
	d->bd_jitcode = jcode;
	reset_d(d);
	splx(s);

	if (old) {
		free(old, M_DEVBUF);
	}
	if (oldj) {
		bpf_jit_freecode(oldj);
	}

	return 0;
}

/*
 * Detach a file from its current interface (if attached at all) and attach
 * to the interface indicated by the name stored in ifr.
 * Return an errno or 0.
 */
static int
bpf_setif(struct bpf_d *d, struct ifreq *ifr)
{
	struct bpf_if *bp;
	char *cp;
	int unit_seen, i, s, error;

	/*
	 * Make sure the provided name has a unit number, and default
	 * it to '0' if not specified.
	 * XXX This is ugly ... do this differently?
	 */
	unit_seen = 0;
	cp = ifr->ifr_name;
	cp[sizeof(ifr->ifr_name) - 1] = '\0';	/* sanity */
	while (*cp++)
		if (*cp >= '0' && *cp <= '9')
			unit_seen = 1;
	if (!unit_seen) {
		/* Make sure to leave room for the '\0'. */
		for (i = 0; i < (IFNAMSIZ - 1); ++i) {
			if ((ifr->ifr_name[i] >= 'a' &&
			     ifr->ifr_name[i] <= 'z') ||
			    (ifr->ifr_name[i] >= 'A' &&
			     ifr->ifr_name[i] <= 'Z'))
				continue;
			ifr->ifr_name[i] = '0';
		}
	}

	/*
	 * Look through attached interfaces for the named one.
	 */
	for (bp = bpf_iflist; bp != NULL; bp = bp->bif_next) {
		struct ifnet *ifp = bp->bif_ifp;

		if (ifp == NULL ||
		    strcmp(ifp->if_xname, ifr->ifr_name) != 0)
			continue;
		/* skip additional entry */
		if (bp->bif_driverp != &ifp->if_bpf)
			continue;
		/*
		 * We found the requested interface.
		 * Allocate the packet buffers if we need to.
		 * If we're already attached to requested interface,
		 * just flush the buffer.
		 */
		if (d->bd_sbuf == NULL) {
			error = bpf_allocbufs(d);
			if (error != 0)
				return (error);
		}
		s = splnet();
		if (bp != d->bd_bif) {
			if (d->bd_bif)
				/*
				 * Detach if attached to something else.
				 */
				bpf_detachd(d);

			bpf_attachd(d, bp);
		}
		reset_d(d);
		splx(s);
		return (0);
	}
	/* Not found. */
	return (ENXIO);
}

/*
 * Copy the interface name to the ifreq.
 */
static void
bpf_ifname(struct ifnet *ifp, struct ifreq *ifr)
{
	memcpy(ifr->ifr_name, ifp->if_xname, IFNAMSIZ);
}

static int
bpf_stat(struct file *fp, struct stat *st)
{
	struct bpf_d *d = fp->f_bpf;

	(void)memset(st, 0, sizeof(*st));
	KERNEL_LOCK(1, NULL);
	st->st_dev = makedev(cdevsw_lookup_major(&bpf_cdevsw), d->bd_pid);
	st->st_atimespec = d->bd_atime;
	st->st_mtimespec = d->bd_mtime;
	st->st_ctimespec = st->st_birthtimespec = d->bd_btime;
	st->st_uid = kauth_cred_geteuid(fp->f_cred);
	st->st_gid = kauth_cred_getegid(fp->f_cred);
	st->st_mode = S_IFCHR;
	KERNEL_UNLOCK_ONE(NULL);
	return 0;
}

/*
 * Support for poll() system call
 *
 * Return true iff the specific operation will not block indefinitely - with
 * the assumption that it is safe to positively acknowledge a request for the
 * ability to write to the BPF device.
 * Otherwise, return false but make a note that a selnotify() must be done.
 */
static int
bpf_poll(struct file *fp, int events)
{
	struct bpf_d *d = fp->f_bpf;
	int s = splnet();
	int revents;

	/*
	 * Refresh the PID associated with this bpf file.
	 */
	KERNEL_LOCK(1, NULL);
	d->bd_pid = curproc->p_pid;

	revents = events & (POLLOUT | POLLWRNORM);
	if (events & (POLLIN | POLLRDNORM)) {
		/*
		 * An imitation of the FIONREAD ioctl code.
		 */
		if (d->bd_hlen != 0 ||
		    ((d->bd_immediate || d->bd_state == BPF_TIMED_OUT) &&
		     d->bd_slen != 0)) {
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			selrecord(curlwp, &d->bd_sel);
			/* Start the read timeout if necessary */
			if (d->bd_rtout > 0 && d->bd_state == BPF_IDLE) {
				callout_reset(&d->bd_callout, d->bd_rtout,
					      bpf_timed_out, d);
				d->bd_state = BPF_WAITING;
			}
		}
	}

	KERNEL_UNLOCK_ONE(NULL);
	splx(s);
	return (revents);
}

static void
filt_bpfrdetach(struct knote *kn)
{
	struct bpf_d *d = kn->kn_hook;
	int s;

	KERNEL_LOCK(1, NULL);
	s = splnet();
	SLIST_REMOVE(&d->bd_sel.sel_klist, kn, knote, kn_selnext);
	splx(s);
	KERNEL_UNLOCK_ONE(NULL);
}

static int
filt_bpfread(struct knote *kn, long hint)
{
	struct bpf_d *d = kn->kn_hook;
	int rv;

	KERNEL_LOCK(1, NULL);
	kn->kn_data = d->bd_hlen;
	if (d->bd_immediate)
		kn->kn_data += d->bd_slen;
	rv = (kn->kn_data > 0);
	KERNEL_UNLOCK_ONE(NULL);
	return rv;
}

static const struct filterops bpfread_filtops =
	{ 1, NULL, filt_bpfrdetach, filt_bpfread };

static int
bpf_kqfilter(struct file *fp, struct knote *kn)
{
	struct bpf_d *d = fp->f_bpf;
	struct klist *klist;
	int s;

	KERNEL_LOCK(1, NULL);

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &d->bd_sel.sel_klist;
		kn->kn_fop = &bpfread_filtops;
		break;

	default:
		KERNEL_UNLOCK_ONE(NULL);
		return (EINVAL);
	}

	kn->kn_hook = d;

	s = splnet();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);
	KERNEL_UNLOCK_ONE(NULL);

	return (0);
}

/*
 * Copy data from an mbuf chain into a buffer.  This code is derived
 * from m_copydata in sys/uipc_mbuf.c.
 */
static void *
bpf_mcpy(void *dst_arg, const void *src_arg, size_t len)
{
	const struct mbuf *m;
	u_int count;
	u_char *dst;

	m = src_arg;
	dst = dst_arg;
	while (len > 0) {
		if (m == NULL)
			panic("bpf_mcpy");
		count = min(m->m_len, len);
		memcpy(dst, mtod(m, const void *), count);
		m = m->m_next;
		dst += count;
		len -= count;
	}
	return dst_arg;
}

/*
 * Dispatch a packet to all the listeners on interface bp.
 *
 * pkt     pointer to the packet, either a data buffer or an mbuf chain
 * buflen  buffer length, if pkt is a data buffer
 * cpfn    a function that can copy pkt into the listener's buffer
 * pktlen  length of the packet
 * rcv     true if packet came in
 */
static inline void
bpf_deliver(struct bpf_if *bp, void *(*cpfn)(void *, const void *, size_t),
    void *pkt, u_int pktlen, u_int buflen, const bool rcv)
{
	uint32_t mem[BPF_MEMWORDS];
	bpf_args_t args = {
		.pkt = (const uint8_t *)pkt,
		.wirelen = pktlen,
		.buflen = buflen,
		.mem = mem,
		.arg = NULL
	};
	bool gottime = false;
	struct timespec ts;

	/*
	 * Note that the IPL does not have to be raised at this point.
	 * The only problem that could arise here is that if two different
	 * interfaces shared any data.  This is not the case.
	 */
	for (struct bpf_d *d = bp->bif_dlist; d != NULL; d = d->bd_next) {
		u_int slen;

		if (!d->bd_seesent && !rcv) {
			continue;
		}
		d->bd_rcount++;
		bpf_gstats.bs_recv++;

		if (d->bd_jitcode)
			slen = d->bd_jitcode(NULL, &args);
		else
			slen = bpf_filter_ext(NULL, d->bd_filter, &args);

		if (!slen) {
			continue;
		}
		if (!gottime) {
			gottime = true;
			nanotime(&ts);
		}
		catchpacket(d, pkt, pktlen, slen, cpfn, &ts);
	}
}

/*
 * Incoming linkage from device drivers.  Process the packet pkt, of length
 * pktlen, which is stored in a contiguous buffer.  The packet is parsed
 * by each process' filter, and if accepted, stashed into the corresponding
 * buffer.
 */
static void
_bpf_tap(struct bpf_if *bp, u_char *pkt, u_int pktlen)
{

	bpf_deliver(bp, memcpy, pkt, pktlen, pktlen, true);
}

/*
 * Incoming linkage from device drivers, when the head of the packet is in
 * a buffer, and the tail is in an mbuf chain.
 */
static void
_bpf_mtap2(struct bpf_if *bp, void *data, u_int dlen, struct mbuf *m)
{
	u_int pktlen;
	struct mbuf mb;

	/* Skip outgoing duplicate packets. */
	if ((m->m_flags & M_PROMISC) != 0 && m->m_pkthdr.rcvif == NULL) {
		m->m_flags &= ~M_PROMISC;
		return;
	}

	pktlen = m_length(m) + dlen;

	/*
	 * Craft on-stack mbuf suitable for passing to bpf_filter.
	 * Note that we cut corners here; we only setup what's
	 * absolutely needed--this mbuf should never go anywhere else.
	 */
	(void)memset(&mb, 0, sizeof(mb));
	mb.m_next = m;
	mb.m_data = data;
	mb.m_len = dlen;

	bpf_deliver(bp, bpf_mcpy, &mb, pktlen, 0, m->m_pkthdr.rcvif != NULL);
}

/*
 * Incoming linkage from device drivers, when packet is in an mbuf chain.
 */
static void
_bpf_mtap(struct bpf_if *bp, struct mbuf *m)
{
	void *(*cpfn)(void *, const void *, size_t);
	u_int pktlen, buflen;
	void *marg;

	/* Skip outgoing duplicate packets. */
	if ((m->m_flags & M_PROMISC) != 0 && m->m_pkthdr.rcvif == NULL) {
		m->m_flags &= ~M_PROMISC;
		return;
	}

	pktlen = m_length(m);

	if (pktlen == m->m_len) {
		cpfn = (void *)memcpy;
		marg = mtod(m, void *);
		buflen = pktlen;
	} else {
		cpfn = bpf_mcpy;
		marg = m;
		buflen = 0;
	}

	bpf_deliver(bp, cpfn, marg, pktlen, buflen, m->m_pkthdr.rcvif != NULL);
}

/*
 * We need to prepend the address family as
 * a four byte field.  Cons up a dummy header
 * to pacify bpf.  This is safe because bpf
 * will only read from the mbuf (i.e., it won't
 * try to free it or keep a pointer a to it).
 */
static void
_bpf_mtap_af(struct bpf_if *bp, uint32_t af, struct mbuf *m)
{
	struct mbuf m0;

	m0.m_flags = 0;
	m0.m_next = m;
	m0.m_len = 4;
	m0.m_data = (char *)&af;

	_bpf_mtap(bp, &m0);
}

/*
 * Put the SLIP pseudo-"link header" in place.
 * Note this M_PREPEND() should never fail,
 * swince we know we always have enough space
 * in the input buffer.
 */
static void
_bpf_mtap_sl_in(struct bpf_if *bp, u_char *chdr, struct mbuf **m)
{
	int s;
	u_char *hp;

	M_PREPEND(*m, SLIP_HDRLEN, M_DONTWAIT);
	if (*m == NULL)
		return;

	hp = mtod(*m, u_char *);
	hp[SLX_DIR] = SLIPDIR_IN;
	(void)memcpy(&hp[SLX_CHDR], chdr, CHDR_LEN);

	s = splnet();
	_bpf_mtap(bp, *m);
	splx(s);

	m_adj(*m, SLIP_HDRLEN);
}

/*
 * Put the SLIP pseudo-"link header" in
 * place.  The compressed header is now
 * at the beginning of the mbuf.
 */
static void
_bpf_mtap_sl_out(struct bpf_if *bp, u_char *chdr, struct mbuf *m)
{
	struct mbuf m0;
	u_char *hp;
	int s;

	m0.m_flags = 0;
	m0.m_next = m;
	m0.m_data = m0.m_dat;
	m0.m_len = SLIP_HDRLEN;

	hp = mtod(&m0, u_char *);

	hp[SLX_DIR] = SLIPDIR_OUT;
	(void)memcpy(&hp[SLX_CHDR], chdr, CHDR_LEN);

	s = splnet();
	_bpf_mtap(bp, &m0);
	splx(s);
	m_freem(m);
}

static int
bpf_hdrlen(struct bpf_d *d)
{
	int hdrlen = d->bd_bif->bif_hdrlen;
	/*
	 * Compute the length of the bpf header.  This is not necessarily
	 * equal to SIZEOF_BPF_HDR because we want to insert spacing such
	 * that the network layer header begins on a longword boundary (for
	 * performance reasons and to alleviate alignment restrictions).
	 */
#ifdef _LP64
	if (d->bd_compat32)
		return (BPF_WORDALIGN32(hdrlen + SIZEOF_BPF_HDR32) - hdrlen);
	else
#endif
		return (BPF_WORDALIGN(hdrlen + SIZEOF_BPF_HDR) - hdrlen);
}

/*
 * Move the packet data from interface memory (pkt) into the
 * store buffer. Call the wakeup functions if it's time to wakeup
 * a listener (buffer full), "cpfn" is the routine called to do the
 * actual data transfer. memcpy is passed in to copy contiguous chunks,
 * while bpf_mcpy is passed in to copy mbuf chains.  In the latter case,
 * pkt is really an mbuf.
 */
static void
catchpacket(struct bpf_d *d, u_char *pkt, u_int pktlen, u_int snaplen,
    void *(*cpfn)(void *, const void *, size_t), struct timespec *ts)
{
	char *h;
	int totlen, curlen, caplen;
	int hdrlen = bpf_hdrlen(d);
	int do_wakeup = 0;

	++d->bd_ccount;
	++bpf_gstats.bs_capt;
	/*
	 * Figure out how many bytes to move.  If the packet is
	 * greater or equal to the snapshot length, transfer that
	 * much.  Otherwise, transfer the whole packet (unless
	 * we hit the buffer size limit).
	 */
	totlen = hdrlen + min(snaplen, pktlen);
	if (totlen > d->bd_bufsize)
		totlen = d->bd_bufsize;
	/*
	 * If we adjusted totlen to fit the bufsize, it could be that
	 * totlen is smaller than hdrlen because of the link layer header.
	 */
	caplen = totlen - hdrlen;
	if (caplen < 0)
		caplen = 0;

	/*
	 * Round up the end of the previous packet to the next longword.
	 */
#ifdef _LP64
	if (d->bd_compat32)
		curlen = BPF_WORDALIGN32(d->bd_slen);
	else
#endif
		curlen = BPF_WORDALIGN(d->bd_slen);
	if (curlen + totlen > d->bd_bufsize) {
		/*
		 * This packet will overflow the storage buffer.
		 * Rotate the buffers if we can, then wakeup any
		 * pending reads.
		 */
		if (d->bd_fbuf == NULL) {
			/*
			 * We haven't completed the previous read yet,
			 * so drop the packet.
			 */
			++d->bd_dcount;
			++bpf_gstats.bs_drop;
			return;
		}
		ROTATE_BUFFERS(d);
		do_wakeup = 1;
		curlen = 0;
	} else if (d->bd_immediate || d->bd_state == BPF_TIMED_OUT) {
		/*
		 * Immediate mode is set, or the read timeout has
		 * already expired during a select call.  A packet
		 * arrived, so the reader should be woken up.
		 */
		do_wakeup = 1;
	}

	/*
	 * Append the bpf header.
	 */
	h = (char *)d->bd_sbuf + curlen;
#ifdef _LP64
	if (d->bd_compat32) {
		struct bpf_hdr32 *hp32;

		hp32 = (struct bpf_hdr32 *)h;
		hp32->bh_tstamp.tv_sec = ts->tv_sec;
		hp32->bh_tstamp.tv_usec = ts->tv_nsec / 1000;
		hp32->bh_datalen = pktlen;
		hp32->bh_hdrlen = hdrlen;
		hp32->bh_caplen = caplen;
	} else
#endif
	{
		struct bpf_hdr *hp;

		hp = (struct bpf_hdr *)h;
		hp->bh_tstamp.tv_sec = ts->tv_sec;
		hp->bh_tstamp.tv_usec = ts->tv_nsec / 1000;
		hp->bh_datalen = pktlen;
		hp->bh_hdrlen = hdrlen;
		hp->bh_caplen = caplen;
	}

	/*
	 * Copy the packet data into the store buffer and update its length.
	 */
	(*cpfn)(h + hdrlen, pkt, caplen);
	d->bd_slen = curlen + totlen;

	/*
	 * Call bpf_wakeup after bd_slen has been updated so that kevent(2)
	 * will cause filt_bpfread() to be called with it adjusted.
	 */
	if (do_wakeup)
		bpf_wakeup(d);
}

/*
 * Initialize all nonzero fields of a descriptor.
 */
static int
bpf_allocbufs(struct bpf_d *d)
{

	d->bd_fbuf = malloc(d->bd_bufsize, M_DEVBUF, M_WAITOK | M_CANFAIL);
	if (!d->bd_fbuf)
		return (ENOBUFS);
	d->bd_sbuf = malloc(d->bd_bufsize, M_DEVBUF, M_WAITOK | M_CANFAIL);
	if (!d->bd_sbuf) {
		free(d->bd_fbuf, M_DEVBUF);
		return (ENOBUFS);
	}
	d->bd_slen = 0;
	d->bd_hlen = 0;
	return (0);
}

/*
 * Free buffers currently in use by a descriptor.
 * Called on close.
 */
static void
bpf_freed(struct bpf_d *d)
{
	/*
	 * We don't need to lock out interrupts since this descriptor has
	 * been detached from its interface and it yet hasn't been marked
	 * free.
	 */
	if (d->bd_sbuf != NULL) {
		free(d->bd_sbuf, M_DEVBUF);
		if (d->bd_hbuf != NULL)
			free(d->bd_hbuf, M_DEVBUF);
		if (d->bd_fbuf != NULL)
			free(d->bd_fbuf, M_DEVBUF);
	}
	if (d->bd_filter)
		free(d->bd_filter, M_DEVBUF);

	if (d->bd_jitcode != NULL) {
		bpf_jit_freecode(d->bd_jitcode);
	}
}

/*
 * Attach an interface to bpf.  dlt is the link layer type;
 * hdrlen is the fixed size of the link header for the specified dlt
 * (variable length headers not yet supported).
 */
static void
_bpfattach(struct ifnet *ifp, u_int dlt, u_int hdrlen, struct bpf_if **driverp)
{
	struct bpf_if *bp;
	bp = malloc(sizeof(*bp), M_DEVBUF, M_DONTWAIT);
	if (bp == NULL)
		panic("bpfattach");

	bp->bif_dlist = NULL;
	bp->bif_driverp = driverp;
	bp->bif_ifp = ifp;
	bp->bif_dlt = dlt;

	bp->bif_next = bpf_iflist;
	bpf_iflist = bp;

	*bp->bif_driverp = NULL;

	bp->bif_hdrlen = hdrlen;
#if 0
	printf("bpf: %s attached\n", ifp->if_xname);
#endif
}

/*
 * Remove an interface from bpf.
 */
static void
_bpfdetach(struct ifnet *ifp)
{
	struct bpf_if *bp, **pbp;
	struct bpf_d *d;
	int s;

	/* Nuke the vnodes for any open instances */
	LIST_FOREACH(d, &bpf_list, bd_list) {
		if (d->bd_bif != NULL && d->bd_bif->bif_ifp == ifp) {
			/*
			 * Detach the descriptor from an interface now.
			 * It will be free'ed later by close routine.
			 */
			s = splnet();
			d->bd_promisc = 0;	/* we can't touch device. */
			bpf_detachd(d);
			splx(s);
		}
	}

  again:
	for (bp = bpf_iflist, pbp = &bpf_iflist;
	     bp != NULL; pbp = &bp->bif_next, bp = bp->bif_next) {
		if (bp->bif_ifp == ifp) {
			*pbp = bp->bif_next;
			free(bp, M_DEVBUF);
			goto again;
		}
	}
}

/*
 * Change the data link type of a interface.
 */
static void
_bpf_change_type(struct ifnet *ifp, u_int dlt, u_int hdrlen)
{
	struct bpf_if *bp;

	for (bp = bpf_iflist; bp != NULL; bp = bp->bif_next) {
		if (bp->bif_driverp == &ifp->if_bpf)
			break;
	}
	if (bp == NULL)
		panic("bpf_change_type");

	bp->bif_dlt = dlt;

	bp->bif_hdrlen = hdrlen;
}

/*
 * Get a list of available data link type of the interface.
 */
static int
bpf_getdltlist(struct bpf_d *d, struct bpf_dltlist *bfl)
{
	int n, error;
	struct ifnet *ifp;
	struct bpf_if *bp;

	ifp = d->bd_bif->bif_ifp;
	n = 0;
	error = 0;
	for (bp = bpf_iflist; bp != NULL; bp = bp->bif_next) {
		if (bp->bif_ifp != ifp)
			continue;
		if (bfl->bfl_list != NULL) {
			if (n >= bfl->bfl_len)
				return ENOMEM;
			error = copyout(&bp->bif_dlt,
			    bfl->bfl_list + n, sizeof(u_int));
		}
		n++;
	}
	bfl->bfl_len = n;
	return error;
}

/*
 * Set the data link type of a BPF instance.
 */
static int
bpf_setdlt(struct bpf_d *d, u_int dlt)
{
	int s, error, opromisc;
	struct ifnet *ifp;
	struct bpf_if *bp;

	if (d->bd_bif->bif_dlt == dlt)
		return 0;
	ifp = d->bd_bif->bif_ifp;
	for (bp = bpf_iflist; bp != NULL; bp = bp->bif_next) {
		if (bp->bif_ifp == ifp && bp->bif_dlt == dlt)
			break;
	}
	if (bp == NULL)
		return EINVAL;
	s = splnet();
	opromisc = d->bd_promisc;
	bpf_detachd(d);
	bpf_attachd(d, bp);
	reset_d(d);
	if (opromisc) {
		error = ifpromisc(bp->bif_ifp, 1);
		if (error)
			printf("%s: bpf_setdlt: ifpromisc failed (%d)\n",
			    bp->bif_ifp->if_xname, error);
		else
			d->bd_promisc = 1;
	}
	splx(s);
	return 0;
}

static int
sysctl_net_bpf_maxbufsize(SYSCTLFN_ARGS)
{
	int newsize, error;
	struct sysctlnode node;

	node = *rnode;
	node.sysctl_data = &newsize;
	newsize = bpf_maxbufsize;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return (error);

	if (newsize < BPF_MINBUFSIZE || newsize > BPF_MAXBUFSIZE)
		return (EINVAL);

	bpf_maxbufsize = newsize;

	return (0);
}

#if defined(MODULAR) || defined(BPFJIT)
static int
sysctl_net_bpf_jit(SYSCTLFN_ARGS)
{
	bool newval;
	int error;
	struct sysctlnode node;

	node = *rnode;
	node.sysctl_data = &newval;
	newval = bpf_jit;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error != 0 || newp == NULL)
		return error;

	bpf_jit = newval;

	/*
	 * Do a full sync to publish new bpf_jit value and
	 * update bpfjit_module_ops.bj_generate_code variable.
	 */
	membar_sync();

	if (newval && bpfjit_module_ops.bj_generate_code == NULL) {
		printf("JIT compilation is postponed "
		    "until after bpfjit module is loaded\n");
	}

	return 0;
}
#endif

static int
sysctl_net_bpf_peers(SYSCTLFN_ARGS)
{
	int    error, elem_count;
	struct bpf_d	 *dp;
	struct bpf_d_ext  dpe;
	size_t len, needed, elem_size, out_size;
	char   *sp;

	if (namelen == 1 && name[0] == CTL_QUERY)
		return (sysctl_query(SYSCTLFN_CALL(rnode)));

	if (namelen != 2)
		return (EINVAL);

	/* BPF peers is privileged information. */
	error = kauth_authorize_network(l->l_cred, KAUTH_NETWORK_INTERFACE,
	    KAUTH_REQ_NETWORK_INTERFACE_GETPRIV, NULL, NULL, NULL);
	if (error)
		return (EPERM);

	len = (oldp != NULL) ? *oldlenp : 0;
	sp = oldp;
	elem_size = name[0];
	elem_count = name[1];
	out_size = MIN(sizeof(dpe), elem_size);
	needed = 0;

	if (elem_size < 1 || elem_count < 0)
		return (EINVAL);

	mutex_enter(&bpf_mtx);
	LIST_FOREACH(dp, &bpf_list, bd_list) {
		if (len >= elem_size && elem_count > 0) {
#define BPF_EXT(field)	dpe.bde_ ## field = dp->bd_ ## field
			BPF_EXT(bufsize);
			BPF_EXT(promisc);
			BPF_EXT(state);
			BPF_EXT(immediate);
			BPF_EXT(hdrcmplt);
			BPF_EXT(seesent);
			BPF_EXT(pid);
			BPF_EXT(rcount);
			BPF_EXT(dcount);
			BPF_EXT(ccount);
#undef BPF_EXT
			if (dp->bd_bif)
				(void)strlcpy(dpe.bde_ifname,
				    dp->bd_bif->bif_ifp->if_xname,
				    IFNAMSIZ - 1);
			else
				dpe.bde_ifname[0] = '\0';

			error = copyout(&dpe, sp, out_size);
			if (error)
				break;
			sp += elem_size;
			len -= elem_size;
		}
		needed += elem_size;
		if (elem_count > 0 && elem_count != INT_MAX)
			elem_count--;
	}
	mutex_exit(&bpf_mtx);

	*oldlenp = needed;

	return (error);
}

static struct sysctllog *bpf_sysctllog;
static void
sysctl_net_bpf_setup(void)
{
	const struct sysctlnode *node;

	node = NULL;
	sysctl_createv(&bpf_sysctllog, 0, NULL, &node,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "bpf",
		       SYSCTL_DESCR("BPF options"),
		       NULL, 0, NULL, 0,
		       CTL_NET, CTL_CREATE, CTL_EOL);
	if (node != NULL) {
#if defined(MODULAR) || defined(BPFJIT)
		sysctl_createv(&bpf_sysctllog, 0, NULL, NULL,
			CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			CTLTYPE_BOOL, "jit",
			SYSCTL_DESCR("Toggle Just-In-Time compilation"),
			sysctl_net_bpf_jit, 0, &bpf_jit, 0,
			CTL_NET, node->sysctl_num, CTL_CREATE, CTL_EOL);
#endif
		sysctl_createv(&bpf_sysctllog, 0, NULL, NULL,
			CTLFLAG_PERMANENT|CTLFLAG_READWRITE,
			CTLTYPE_INT, "maxbufsize",
			SYSCTL_DESCR("Maximum size for data capture buffer"),
			sysctl_net_bpf_maxbufsize, 0, &bpf_maxbufsize, 0,
			CTL_NET, node->sysctl_num, CTL_CREATE, CTL_EOL);
		sysctl_createv(&bpf_sysctllog, 0, NULL, NULL,
			CTLFLAG_PERMANENT,
			CTLTYPE_STRUCT, "stats",
			SYSCTL_DESCR("BPF stats"),
			NULL, 0, &bpf_gstats, sizeof(bpf_gstats),
			CTL_NET, node->sysctl_num, CTL_CREATE, CTL_EOL);
		sysctl_createv(&bpf_sysctllog, 0, NULL, NULL,
			CTLFLAG_PERMANENT,
			CTLTYPE_STRUCT, "peers",
			SYSCTL_DESCR("BPF peers"),
			sysctl_net_bpf_peers, 0, NULL, 0,
			CTL_NET, node->sysctl_num, CTL_CREATE, CTL_EOL);
	}

}

struct bpf_ops bpf_ops_kernel = {
	.bpf_attach =		_bpfattach,
	.bpf_detach =		_bpfdetach,
	.bpf_change_type =	_bpf_change_type,

	.bpf_tap =		_bpf_tap,
	.bpf_mtap =		_bpf_mtap,
	.bpf_mtap2 =		_bpf_mtap2,
	.bpf_mtap_af =		_bpf_mtap_af,
	.bpf_mtap_sl_in =	_bpf_mtap_sl_in,
	.bpf_mtap_sl_out =	_bpf_mtap_sl_out,
};

MODULE(MODULE_CLASS_DRIVER, bpf, NULL);

static int
bpf_modcmd(modcmd_t cmd, void *arg)
{
	devmajor_t bmajor, cmajor;
	int error;

	bmajor = cmajor = NODEVMAJOR;

	switch (cmd) {
	case MODULE_CMD_INIT:
		bpfilterattach(0);
		error = devsw_attach("bpf", NULL, &bmajor,
		    &bpf_cdevsw, &cmajor);
		if (error == EEXIST)
			error = 0; /* maybe built-in ... improve eventually */
		if (error)
			break;

		bpf_ops_handover_enter(&bpf_ops_kernel);
		atomic_swap_ptr(&bpf_ops, &bpf_ops_kernel);
		bpf_ops_handover_exit();
		sysctl_net_bpf_setup();
		break;

	case MODULE_CMD_FINI:
		/*
		 * While there is no reference counting for bpf callers,
		 * unload could at least in theory be done similarly to 
		 * system call disestablishment.  This should even be
		 * a little simpler:
		 * 
		 * 1) replace op vector with stubs
		 * 2) post update to all cpus with xc
		 * 3) check that nobody is in bpf anymore
		 *    (it's doubtful we'd want something like l_sysent,
		 *     but we could do something like *signed* percpu
		 *     counters.  if the sum is 0, we're good).
		 * 4) if fail, unroll changes
		 *
		 * NOTE: change won't be atomic to the outside.  some
		 * packets may be not captured even if unload is
		 * not succesful.  I think packet capture not working
		 * is a perfectly logical consequence of trying to
		 * disable packet capture.
		 */
		error = EOPNOTSUPP;
		/* insert sysctl teardown */
		break;

	default:
		error = ENOTTY;
		break;
	}

	return error;
}
