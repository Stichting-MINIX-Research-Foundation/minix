/*	$NetBSD: smb_iod.c,v 1.40 2012/04/29 20:27:31 dsl Exp $	*/

/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * FreeBSD: src/sys/netsmb/smb_iod.c,v 1.4 2001/12/09 17:48:08 arr Exp
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smb_iod.c,v 1.40 2012/04/29 20:27:31 dsl Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/unistd.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_tran.h>
#include <netsmb/smb_trantcp.h>

#define	SMB_IOD_EVLOCKPTR(iod)	(&((iod)->iod_evlock))
#define	SMB_IOD_EVLOCK(iod)	smb_sl_lock(&((iod)->iod_evlock))
#define	SMB_IOD_EVUNLOCK(iod)	smb_sl_unlock(&((iod)->iod_evlock))

#define	SMB_IOD_RQLOCKPTR(iod)	(&((iod)->iod_rqlock))
#define	SMB_IOD_RQLOCK(iod)	smb_sl_lock(&((iod)->iod_rqlock))
#define	SMB_IOD_RQUNLOCK(iod)	smb_sl_unlock(&((iod)->iod_rqlock))

#define	smb_iod_wakeup(iod)	wakeup(&(iod)->iod_flags)

MALLOC_DEFINE(M_SMBIOD, "SMBIOD", "SMB network io daemon");
MALLOC_DECLARE(M_SMBIOD);

static int smb_iod_next;

static bool smb_iod_sendall(struct smbiod *iod);
static int  smb_iod_disconnect(struct smbiod *iod);
static void smb_iod_thread(void *);

static void
smb_iod_rqprocessed(struct smb_rq *rqp, int error)
{
	SMBRQ_SLOCK(rqp);
	rqp->sr_lerror = error;
	rqp->sr_rpgen++;
	rqp->sr_state = SMBRQ_NOTIFIED;
	wakeup(&rqp->sr_state);
	if (rqp->sr_timo > 0)
		callout_stop(&rqp->sr_timo_ch);
	if (rqp->sr_recvcallback)
		(*rqp->sr_recvcallback)(rqp->sr_recvarg);
	SMBRQ_SUNLOCK(rqp);
}

static void
smb_iod_rqtimedout(void *arg)
{
	smb_iod_rqprocessed((struct smb_rq *)arg, ETIMEDOUT);
}

static void
smb_iod_invrq(struct smbiod *iod)
{
	struct smb_rq *rqp;

	/*
	 * Invalidate all outstanding requests for this connection
	 */
	SMB_IOD_RQLOCK(iod);
	SIMPLEQ_FOREACH(rqp, &iod->iod_rqlist, sr_link) {
		if (rqp->sr_flags & SMBR_INTERNAL)
			SMBRQ_SUNLOCK(rqp);
		rqp->sr_flags |= SMBR_RESTART;
		smb_iod_rqprocessed(rqp, ENOTCONN);
	}
	SMB_IOD_RQUNLOCK(iod);
}

static void
smb_iod_closetran(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;
	struct lwp *l = iod->iod_l;

	if (vcp->vc_tdata == NULL)
		return;
	SMB_TRAN_DISCONNECT(vcp, l);
	SMB_TRAN_DONE(vcp, l);
	vcp->vc_tdata = NULL;
}

static void
smb_iod_dead(struct smbiod *iod)
{
	iod->iod_state = SMBIOD_ST_DEAD;
	smb_iod_closetran(iod);
	smb_iod_invrq(iod);
}

static int
smb_iod_connect(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;
	struct lwp *l = iod->iod_l;
	int error;

	SMBIODEBUG(("%d\n", iod->iod_state));
	switch(iod->iod_state) {
	case SMBIOD_ST_VCACTIVE:
		SMBIODEBUG(("called for already opened connection\n"));
		return EISCONN;
	case SMBIOD_ST_DEAD:
		return ENOTCONN;	/* XXX: last error code ? */
	default:
		break;
	}
	vcp->vc_genid++;

#define ithrow(cmd)			\
		if ((error = cmd))	\
			goto fail

	ithrow(SMB_TRAN_CREATE(vcp, l));
	SMBIODEBUG(("tcreate\n"));
	if (vcp->vc_laddr) {
		ithrow(SMB_TRAN_BIND(vcp, vcp->vc_laddr, l));
	}
	SMBIODEBUG(("tbind\n"));
	ithrow(SMB_TRAN_CONNECT(vcp, vcp->vc_paddr, l));
	SMB_TRAN_SETPARAM(vcp, SMBTP_SELECTID, &iod->iod_flags);
	iod->iod_state = SMBIOD_ST_TRANACTIVE;
	SMBIODEBUG(("tconnect\n"));
/*	vcp->vc_mid = 0;*/
	ithrow(smb_smb_negotiate(vcp, &iod->iod_scred));
	SMBIODEBUG(("snegotiate\n"));
	ithrow(smb_smb_ssnsetup(vcp, &iod->iod_scred));
	iod->iod_state = SMBIOD_ST_VCACTIVE;

#undef ithrow

	SMBIODEBUG(("completed\n"));
	smb_iod_invrq(iod);

	return (0);

    fail:
	smb_iod_dead(iod);
	return (error);
}

static int
smb_iod_disconnect(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;

	SMBIODEBUG(("\n"));
	if (iod->iod_state == SMBIOD_ST_VCACTIVE) {
		smb_smb_ssnclose(vcp, &iod->iod_scred);
		iod->iod_state = SMBIOD_ST_TRANACTIVE;
	}
	vcp->vc_smbuid = SMB_UID_UNKNOWN;
	smb_iod_closetran(iod);
	iod->iod_state = SMBIOD_ST_NOTCONN;
	return 0;
}

static int
smb_iod_treeconnect(struct smbiod *iod, struct smb_share *ssp)
{
	int error;

	if (iod->iod_state != SMBIOD_ST_VCACTIVE) {
		if (iod->iod_state != SMBIOD_ST_DEAD)
			return ENOTCONN;
		iod->iod_state = SMBIOD_ST_RECONNECT;
		error = smb_iod_connect(iod);
		if (error)
			return error;
	}
	SMBIODEBUG(("tree reconnect\n"));
	SMBS_ST_LOCK(ssp);
	ssp->ss_flags |= SMBS_RECONNECTING;
	SMBS_ST_UNLOCK(ssp);
	error = smb_smb_treeconnect(ssp, &iod->iod_scred);
	SMBS_ST_LOCK(ssp);
	ssp->ss_flags &= ~SMBS_RECONNECTING;
	SMBS_ST_UNLOCK(ssp);
	wakeup(&ssp->ss_vcgenid);
	return error;
}

static int
smb_iod_sendrq(struct smbiod *iod, struct smb_rq *rqp)
{
	struct lwp *l = iod->iod_l;
	struct smb_vc *vcp = iod->iod_vc;
	struct smb_share *ssp = rqp->sr_share;
	struct mbuf *m;
	int error;

	SMBIODEBUG(("iod_state = %d, rqmid %d\n", iod->iod_state, rqp->sr_mid));
	switch (iod->iod_state) {
	case SMBIOD_ST_NOTCONN:
		smb_iod_rqprocessed(rqp, ENOTCONN);
		return 0;
	case SMBIOD_ST_DEAD:
		iod->iod_state = SMBIOD_ST_RECONNECT;
		return 0;
	case SMBIOD_ST_RECONNECT:
		return 0;
	default:
		break;
	}
	if (rqp->sr_sendcnt == 0) {
		u_int16_t tid = ssp ? ssp->ss_tid : SMB_TID_UNKNOWN;
		u_int16_t rquid = vcp ? vcp->vc_smbuid : 0;
#ifdef movedtoanotherplace
		if (vcp->vc_maxmux != 0 && iod->iod_muxcnt >= vcp->vc_maxmux)
			return 0;
#endif
		SMBRQ_PUTLE16(rqp->sr_rqtid, tid);
		SMBRQ_PUTLE16(rqp->sr_rquid, rquid);
		mb_fixhdr(&rqp->sr_rq);
	}
	if (rqp->sr_sendcnt++ > 5) {
		rqp->sr_flags |= SMBR_RESTART;
		smb_iod_rqprocessed(rqp, rqp->sr_lerror);
		/*
		 * If all attempts to send a request failed, then
		 * something is seriously hosed.
		 */
		return ENOTCONN;
	}
	SMBSDEBUG(("M:%04x, P:%04x, U:%04x, T:%04x\n", rqp->sr_mid, 0, 0, 0));
	m_dumpm(rqp->sr_rq.mb_top);
	m = m_copym(rqp->sr_rq.mb_top, 0, M_COPYALL, M_WAIT);
	error = rqp->sr_lerror = (m) ? SMB_TRAN_SEND(vcp, m, l) : ENOBUFS;
	if (error == 0) {
		if (rqp->sr_timo > 0)
			callout_reset(&rqp->sr_timo_ch, rqp->sr_timo,
				smb_iod_rqtimedout, rqp);

		if (rqp->sr_flags & SMBR_NOWAIT) {
			/* caller doesn't want to wait, flag as processed */
			smb_iod_rqprocessed(rqp, 0);
			return (0);
		}

#if 0
		iod->iod_lastrqsent = ts;
#endif
		rqp->sr_flags |= SMBR_SENT;
		rqp->sr_state = SMBRQ_SENT;
		return 0;
	}
	/*
	 * Check for fatal errors
	 */
	if (vcp && SMB_TRAN_FATAL(vcp, error)) {
		/*
		 * No further attempts should be made
		 */
		return ENOTCONN;
	}
	if (smb_rq_intr(rqp))
		smb_iod_rqprocessed(rqp, EINTR);
	return 0;
}

/*
 * Process incoming packets
 */
static void
smb_iod_recvall(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;
	struct lwp *l = iod->iod_l;
	struct smb_rq *rqp;
	struct mbuf *m;
	u_char *hp;
	u_short mid;
	int error;

	switch (iod->iod_state) {
	case SMBIOD_ST_NOTCONN:
	case SMBIOD_ST_DEAD:
	case SMBIOD_ST_RECONNECT:
		return;
	default:
		break;
	}

	for (;;) {
		m = NULL;
		error = SMB_TRAN_RECV(vcp, &m, l);
		if (error == EWOULDBLOCK)
			break;
		if (SMB_TRAN_FATAL(vcp, error)) {
			smb_iod_dead(iod);
			break;
		}
		if (error)
			break;
		KASSERT(m != NULL);

		m = m_pullup(m, SMB_HDRLEN);
		if (m == NULL)
			continue;	/* wait for a good packet */
		/*
		 * Now we got an entire and possibly invalid SMB packet.
		 * Be careful while parsing it.
		 */
		m_dumpm(m);
		hp = mtod(m, u_char*);
		if (memcmp(hp, SMB_SIGNATURE, SMB_SIGLEN) != 0) {
			m_freem(m);
			continue;
		}
		mid = SMB_HDRMID(hp);
		SMBSDEBUG(("mid %04x\n", (u_int)mid));
		SMB_IOD_RQLOCK(iod);
		SIMPLEQ_FOREACH(rqp, &iod->iod_rqlist, sr_link) {
			if (rqp->sr_mid != mid)
				continue;
			SMBRQ_SLOCK(rqp);
			if (rqp->sr_rp.md_top == NULL) {
				md_initm(&rqp->sr_rp, m);
			} else {
				if (rqp->sr_flags & SMBR_MULTIPACKET) {
					md_append_record(&rqp->sr_rp, m);
				} else {
					SMBRQ_SUNLOCK(rqp);
					SMBIODEBUG(("duplicate response %d (ignored)\n", mid));
					break;
				}
			}
			SMBRQ_SUNLOCK(rqp);
			smb_iod_rqprocessed(rqp, 0);
			break;
		}
		SMB_IOD_RQUNLOCK(iod);
		if (rqp == NULL) {
			SMBIODEBUG(("drop resp with mid %d\n", (u_int)mid));
/*			smb_printrqlist(vcp);*/
			m_freem(m);
		}
	}
	/*
	 * check for interrupts
	 */
	SMB_IOD_RQLOCK(iod);
	SIMPLEQ_FOREACH(rqp, &iod->iod_rqlist, sr_link) {
		if (smb_proc_intr(rqp->sr_cred->scr_l)) {
			smb_iod_rqprocessed(rqp, EINTR);
		}
	}
	SMB_IOD_RQUNLOCK(iod);
}

int
smb_iod_request(struct smbiod *iod, int event, void *ident)
{
	struct smbiod_event *evp;
	int error;

	SMBIODEBUG(("\n"));
	evp = smb_zmalloc(sizeof(*evp), M_SMBIOD, M_WAITOK);
	evp->ev_type = event;
	evp->ev_ident = ident;
	SMB_IOD_EVLOCK(iod);
	SIMPLEQ_INSERT_TAIL(&iod->iod_evlist, evp, ev_link);
	if ((event & SMBIOD_EV_SYNC) == 0) {
		SMB_IOD_EVUNLOCK(iod);
		smb_iod_wakeup(iod);
		return 0;
	}
	smb_iod_wakeup(iod);
	mtsleep(evp, PWAIT | PNORELOCK, "smbevw", 0, SMB_IOD_EVLOCKPTR(iod));
	error = evp->ev_error;
	free(evp, M_SMBIOD);
	return error;
}

/*
 * Place request in the queue.
 * Request from smbiod have a high priority.
 */
int
smb_iod_addrq(struct smb_rq *rqp)
{
	struct smb_vc *vcp = rqp->sr_vc;
	struct smbiod *iod = vcp->vc_iod;
	int error;

	SMBIODEBUG(("\n"));
	if (rqp->sr_cred->scr_l == iod->iod_l) {
		rqp->sr_flags |= SMBR_INTERNAL;
		SMB_IOD_RQLOCK(iod);
		SIMPLEQ_INSERT_HEAD(&iod->iod_rqlist, rqp, sr_link);
		SMB_IOD_RQUNLOCK(iod);
		for (;;) {
			if (smb_iod_sendrq(iod, rqp) != 0) {
				smb_iod_dead(iod);
				break;
			}
			/*
			 * we don't need to lock state field here
			 */
			if (rqp->sr_state != SMBRQ_NOTSENT)
				break;
			tsleep(&iod->iod_flags, PWAIT, "smbsndw", hz);
		}
		if (rqp->sr_lerror)
			smb_iod_removerq(rqp);
		return rqp->sr_lerror;
	}

	switch (iod->iod_state) {
	case SMBIOD_ST_NOTCONN:
		return ENOTCONN;
	case SMBIOD_ST_DEAD:
		error = smb_iod_request(iod, SMBIOD_EV_CONNECT | SMBIOD_EV_SYNC, NULL);
		if (error)
			return error;
		/*
		 * Return error to force the caller reissue the request
		 * using new connection state.
		 */
		return EXDEV;
	default:
		break;
	}

	SMB_IOD_RQLOCK(iod);
	for (;;) {
#ifdef DIAGNOSTIC
		if (vcp->vc_maxmux == 0)
			panic("%s: vc maxmum == 0", __func__);
#endif
		if (iod->iod_muxcnt < vcp->vc_maxmux)
			break;
		iod->iod_muxwant++;
		/* XXX use interruptible sleep? */
		mtsleep(&iod->iod_muxwant, PWAIT, "smbmux",
			0, SMB_IOD_RQLOCKPTR(iod));
	}
	iod->iod_muxcnt++;
	SIMPLEQ_INSERT_TAIL(&iod->iod_rqlist, rqp, sr_link);
	SMB_IOD_RQUNLOCK(iod);
	smb_iod_wakeup(iod);
	return 0;
}

int
smb_iod_removerq(struct smb_rq *rqp)
{
	struct smb_vc *vcp = rqp->sr_vc;
	struct smbiod *iod = vcp->vc_iod;

	SMBIODEBUG(("\n"));
	if (rqp->sr_flags & SMBR_INTERNAL) {
		SMB_IOD_RQLOCK(iod);
		SIMPLEQ_REMOVE(&iod->iod_rqlist, rqp, smb_rq, sr_link);
		SMB_IOD_RQUNLOCK(iod);
		return 0;
	}
	SMB_IOD_RQLOCK(iod);
	while (rqp->sr_flags & SMBR_XLOCK) {
		rqp->sr_flags |= SMBR_XLOCKWANT;
		mtsleep(rqp, PWAIT, "smbxrm", 0, SMB_IOD_RQLOCKPTR(iod));
	}
	SIMPLEQ_REMOVE(&iod->iod_rqlist, rqp, smb_rq, sr_link);
	iod->iod_muxcnt--;
	if (iod->iod_muxwant) {
		iod->iod_muxwant--;
		wakeup(&iod->iod_muxwant);
	}
	SMB_IOD_RQUNLOCK(iod);
	return 0;
}

int
smb_iod_waitrq(struct smb_rq *rqp)
{
	struct smbiod *iod = rqp->sr_vc->vc_iod;
	int error;

	SMBIODEBUG(("\n"));
	if (rqp->sr_flags & SMBR_INTERNAL) {
		for (;;) {
			smb_iod_sendall(iod);
			smb_iod_recvall(iod);
			if (rqp->sr_rpgen != rqp->sr_rplast)
				break;
			tsleep(&iod->iod_flags, PWAIT, "smbirq", hz);
		}
		smb_iod_removerq(rqp);
		return rqp->sr_lerror;

	}
	SMBRQ_SLOCK(rqp);
	if (rqp->sr_rpgen == rqp->sr_rplast) {
		/* XXX interruptible sleep? */
		mtsleep(&rqp->sr_state, PWAIT, "smbwrq", 0,
			SMBRQ_SLOCKPTR(rqp));
	}
	rqp->sr_rplast++;
	SMBRQ_SUNLOCK(rqp);
	error = rqp->sr_lerror;
	if (rqp->sr_flags & SMBR_MULTIPACKET) {
		/*
		 * If request should stay in the list, then reinsert it
		 * at the end of queue so other waiters have chance to concur
		 */
		SMB_IOD_RQLOCK(iod);
		SIMPLEQ_REMOVE(&iod->iod_rqlist, rqp, smb_rq, sr_link);
		SIMPLEQ_INSERT_TAIL(&iod->iod_rqlist, rqp, sr_link);
		SMB_IOD_RQUNLOCK(iod);
	} else
		smb_iod_removerq(rqp);
	return error;
}


static bool
smb_iod_sendall(struct smbiod *iod)
{
	struct smb_rq *rqp;
	int herror;
	bool sentany = false;

	herror = 0;
	/*
	 * Loop through the list of requests and send them if possible
	 */
	SMB_IOD_RQLOCK(iod);
	SIMPLEQ_FOREACH(rqp, &iod->iod_rqlist, sr_link) {
		if (__predict_false(rqp->sr_state == SMBRQ_NOTSENT)) {
			rqp->sr_flags |= SMBR_XLOCK;
			SMB_IOD_RQUNLOCK(iod);
			herror = smb_iod_sendrq(iod, rqp);
			SMB_IOD_RQLOCK(iod);
			rqp->sr_flags &= ~SMBR_XLOCK;
			if (rqp->sr_flags & SMBR_XLOCKWANT) {
				rqp->sr_flags &= ~SMBR_XLOCKWANT;
				wakeup(rqp);
			}

			if (__predict_false(herror != 0))
				break;
			sentany = true;
		}
	}
	SMB_IOD_RQUNLOCK(iod);
	if (herror == ENOTCONN)
		smb_iod_dead(iod);

	return sentany;
}

/*
 * "main" function for smbiod daemon
 */
static inline void
smb_iod_main(struct smbiod *iod)
{
#if 0
	struct smb_vc *vcp = iod->iod_vc;
	struct timespec tsnow;
#endif
	struct smbiod_event *evp;

	SMBIODEBUG(("\n"));

	/*
	 * Check all interesting events
	 */
	for (;;) {
		SMB_IOD_EVLOCK(iod);
		evp = SIMPLEQ_FIRST(&iod->iod_evlist);
		if (evp == NULL) {
			SMB_IOD_EVUNLOCK(iod);
			break;
		}
		SIMPLEQ_REMOVE_HEAD(&iod->iod_evlist, ev_link);
		evp->ev_type |= SMBIOD_EV_PROCESSING;
		SMB_IOD_EVUNLOCK(iod);
		switch (evp->ev_type & SMBIOD_EV_MASK) {
		case SMBIOD_EV_CONNECT:
			iod->iod_state = SMBIOD_ST_RECONNECT;
			evp->ev_error = smb_iod_connect(iod);
			break;
		case SMBIOD_EV_DISCONNECT:
			evp->ev_error = smb_iod_disconnect(iod);
			break;
		case SMBIOD_EV_TREECONNECT:
			evp->ev_error = smb_iod_treeconnect(iod, evp->ev_ident);
			break;
		case SMBIOD_EV_SHUTDOWN:
			iod->iod_flags |= SMBIOD_SHUTDOWN;
			break;
		case SMBIOD_EV_NEWRQ:
			break;
		}
		if (evp->ev_type & SMBIOD_EV_SYNC) {
			SMB_IOD_EVLOCK(iod);
			wakeup(evp);
			SMB_IOD_EVUNLOCK(iod);
		} else
			free(evp, M_SMBIOD);
	}
#if 0
	if (iod->iod_state == SMBIOD_ST_VCACTIVE) {
		getnanotime(&tsnow);
		timespecsub(&tsnow, &iod->iod_pingtimo);
		if (timespeccmp(&tsnow, &iod->iod_lastrqsent, >)) {
			smb_smb_echo(vcp, &iod->iod_scred);
		}
	}
#endif

	/*
	 * Do a send/receive cycle once and then as many times
	 * afterwards as we can send out new data.  This is to make
	 * sure we got all data sent which might have ended up in the
	 * queue during the receive phase (which might block releasing
	 * the kernel lock).
	 */
	smb_iod_sendall(iod);
	smb_iod_recvall(iod);
	while (smb_iod_sendall(iod)) {
		smb_iod_recvall(iod);
	}
}

void
smb_iod_thread(void *arg)
{
	struct smbiod *iod = arg;
	int s;

	/*
	 * Here we assume that the thread structure will be the same
	 * for an entire kthread (kproc, to be more precise) life.
	 */
	KASSERT(iod->iod_l == curlwp);
	smb_makescred(&iod->iod_scred, iod->iod_l, NULL);
	s = splnet();
	while ((iod->iod_flags & SMBIOD_SHUTDOWN) == 0) {
		smb_iod_main(iod);
		if (iod->iod_flags & SMBIOD_SHUTDOWN)
			break;
		SMBIODEBUG(("going to sleep\n"));
		/*
		 * technically wakeup every hz is unnecessary, but keep
		 * this here until smb has been made mpsafe.
		 */
		tsleep(&iod->iod_flags, PSOCK, "smbidle", hz);
	}
	splx(s);
	kthread_exit(0);
}

int
smb_iod_create(struct smb_vc *vcp)
{
	struct smbiod *iod;
	int error;

	iod = smb_zmalloc(sizeof(*iod), M_SMBIOD, M_WAITOK);
	iod->iod_id = smb_iod_next++;
	iod->iod_state = SMBIOD_ST_NOTCONN;
	iod->iod_vc = vcp;
#if 0
	iod->iod_pingtimo.tv_sec = SMBIOD_PING_TIMO;
	microtime(&iod->iod_lastrqsent);
#endif
	vcp->vc_iod = iod;
	smb_sl_init(&iod->iod_rqlock, "smbrql");
	SIMPLEQ_INIT(&iod->iod_rqlist);
	smb_sl_init(&iod->iod_evlock, "smbevl");
	SIMPLEQ_INIT(&iod->iod_evlist);
	error = kthread_create(PRI_NONE, 0, NULL, smb_iod_thread, iod,
	   &iod->iod_l, "smbiod%d", iod->iod_id);
	if (error) {
		SMBIODEBUG(("can't start smbiod: %d", error));
		free(iod, M_SMBIOD);
		return error;
	}
	return 0;
}

int
smb_iod_destroy(struct smbiod *iod)
{
	smb_iod_request(iod, SMBIOD_EV_SHUTDOWN | SMBIOD_EV_SYNC, NULL);
	smb_sl_destroy(&iod->iod_rqlock);
	smb_sl_destroy(&iod->iod_evlock);
	free(iod, M_SMBIOD);
	return 0;
}

int
smb_iod_init(void)
{
	return 0;
}

int
smb_iod_done(void)
{
	return 0;
}
