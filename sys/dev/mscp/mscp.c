/*	$NetBSD: mscp.c,v 1.36 2012/10/27 17:18:26 chs Exp $	*/

/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 *	@(#)mscp.c	7.5 (Berkeley) 12/16/90
 */

/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)mscp.c	7.5 (Berkeley) 12/16/90
 */

/*
 * MSCP generic driver routines
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mscp.c,v 1.36 2012/10/27 17:18:26 chs Exp $");

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/bufq.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <sys/bus.h>

#include <dev/mscp/mscp.h>
#include <dev/mscp/mscpreg.h>
#include <dev/mscp/mscpvar.h>

#define PCMD	PSWP		/* priority for command packet waits */

/*
 * Get a command packet.  Second argument is true iff we are
 * to wait if necessary.  Return NULL if none are available and
 * we cannot wait.
 */
struct mscp *
mscp_getcp(struct mscp_softc *mi, int canwait)
{
#define mri	(&mi->mi_cmd)
	struct mscp *mp;
	int i;
	int s = spluba();

again:
	/*
	 * Ensure that we have some command credits, and
	 * that the next command packet is free.
	 */
	if (mi->mi_credits <= MSCP_MINCREDITS) {
		if (!canwait) {
			splx(s);
			return (NULL);
		}
		mi->mi_wantcredits = 1;
		(void) tsleep(&mi->mi_wantcredits, PCMD, "mscpwcrd", 0);
		goto again;
	}
	i = mri->mri_next;
	if (mri->mri_desc[i] & MSCP_OWN) {
		if (!canwait) {
			splx(s);
			return (NULL);
		}
		mi->mi_wantcmd = 1;
		(void) tsleep(&mi->mi_wantcmd, PCMD, "mscpwcmd", 0);
		goto again;
	}
	mi->mi_credits--;
	mri->mri_desc[i] &= ~MSCP_INT;
	mri->mri_next = (mri->mri_next + 1) % mri->mri_size;
	splx(s);
	mp = &mri->mri_ring[i];

	/*
	 * Initialise some often-zero fields.
	 * ARE THE LAST TWO NECESSARY IN GENERAL?  IT SURE WOULD BE
	 * NICE IF DEC SOLD DOCUMENTATION FOR THEIR OWN CONTROLLERS.
	 */
	mp->mscp_msglen = MSCP_MSGLEN;
	mp->mscp_flags = 0;
	mp->mscp_modifier = 0;
	mp->mscp_seq.seq_bytecount = 0;
	mp->mscp_seq.seq_buffer = 0;
	mp->mscp_seq.seq_mapbase = 0;
/*???*/ mp->mscp_sccc.sccc_errlgfl = 0;
/*???*/ mp->mscp_sccc.sccc_copyspd = 0;
	return (mp);
#undef	mri
}

#ifdef AVOID_EMULEX_BUG
int	mscp_aeb_xor = 0x8000bb80;
#endif

/*
 * Handle a response ring transition.
 */
void
mscp_dorsp(struct mscp_softc *mi)
{
	device_t drive;
	struct mscp_device *me = mi->mi_me;
	struct mscp_ctlr *mc = mi->mi_mc;
	struct buf *bp;
	struct mscp *mp;
	struct mscp_xi *mxi;
	int nextrsp;
	int st, error;
	extern struct mscp mscp_cold_reply;
	extern int mscp_cold_unit;

	nextrsp = mi->mi_rsp.mri_next;
loop:
	if (mi->mi_rsp.mri_desc[nextrsp] & MSCP_OWN) {
		/*
		 * No more responses.  Remember the next expected
		 * response index.  Check to see if we have some
		 * credits back, and wake up sleepers if so.
		 */
		mi->mi_rsp.mri_next = nextrsp;
		if (mi->mi_wantcredits && mi->mi_credits > MSCP_MINCREDITS) {
			mi->mi_wantcredits = 0;
			wakeup((void *) &mi->mi_wantcredits);
		}
		return;
	}

	mp = &mi->mi_rsp.mri_ring[nextrsp];
	mi->mi_credits += MSCP_CREDITS(mp->mscp_msgtc);
	/*
	 * Controllers are allowed to interrupt as any drive, so we
	 * must check the command before checking for a drive.
	 */
	if (mp->mscp_opcode == (M_OP_SETCTLRC | M_OP_END)) {
		if ((mp->mscp_status & M_ST_MASK) == M_ST_SUCCESS) {
			mi->mi_flags |= MSC_READY;
		} else {
			printf("%s: SETCTLRC failed: %d ",
			    device_xname(mi->mi_dev), mp->mscp_status);
			mscp_printevent(mp);
		}
		goto done;
	}

	/*
	 * Found a response.  Update credit information.  If there is
	 * nothing else to do, jump to `done' to get the next response.
	 */
	if (mp->mscp_unit >= mi->mi_driveno) { /* Must expand drive table */
		int tmpno = (mp->mscp_unit + 32) & ~31;
		device_t *tmp = (device_t *)
		    malloc(tmpno * sizeof(tmp[0]), M_DEVBUF, M_NOWAIT|M_ZERO);
		/* XXX tmp should be checked for NULL */
		if (mi->mi_driveno) {
			memcpy(tmp, mi->mi_dp, mi->mi_driveno * sizeof(tmp[0]));
			free(mi->mi_dp, M_DEVBUF);
		}
		mi->mi_driveno = tmpno;
		mi->mi_dp = tmp;
	}

	drive = mi->mi_dp[mp->mscp_unit];

	switch (MSCP_MSGTYPE(mp->mscp_msgtc)) {

	case MSCPT_SEQ:
		break;

	case MSCPT_DATAGRAM:
		(*me->me_dgram)(drive, mp, mi);
		goto done;

	case MSCPT_CREDITS:
		goto done;

	case MSCPT_MAINTENANCE:
	default:
		printf("%s: unit %d: unknown message type 0x%x ignored\n",
			device_xname(mi->mi_dev), mp->mscp_unit,
			MSCP_MSGTYPE(mp->mscp_msgtc));
		goto done;
	}

	/*
	 * Handle individual responses.
	 */
	st = mp->mscp_status & M_ST_MASK;
	error = 0;
	switch (mp->mscp_opcode) {

	case M_OP_END:
		/*
		 * The controller presents a bogus END packet when
		 * a read/write command is given with an illegal
		 * block number.  This is contrary to the MSCP
		 * specification (ENDs are to be given only for
		 * invalid commands), but that is the way of it.
		 */
		if (st == M_ST_INVALCMD && mp->mscp_cmdref != 0) {
			printf("%s: bad lbn (%d)?\n", device_xname(drive),
				(int)mp->mscp_seq.seq_lbn);
			error = EIO;
			goto rwend;
		}
		goto unknown;

	case M_OP_ONLINE | M_OP_END:
		/*
		 * Finished an ON LINE request.	 Call the driver to
		 * find out whether it succeeded.  If so, mark it on
		 * line.
		 */
		(*me->me_online)(drive, mp);
		break;

	case M_OP_GETUNITST | M_OP_END:
		/*
		 * Got unit status.  If we are autoconfiguring, save
		 * the mscp struct so that mscp_attach know what to do.
		 * If the drive isn't configured, call config_found()
		 * to set it up, otherwise it's just a "normal" unit
		 * status.
		 */
		if (cold) {
			memcpy(&mscp_cold_reply, mp, sizeof(struct mscp));
			/* Detect that we've reached the end of all units */
			if (mp->mscp_unit < mscp_cold_unit)
				break;
		}

		if (mp->mscp_status == (M_ST_OFFLINE|M_OFFLINE_UNKNOWN))
			break;

		if (drive == 0) {
			struct mscp_work *mw;

			mutex_spin_enter(&mi->mi_mtx);

			mw = SLIST_FIRST(&mi->mi_freelist);
			if (mw == NULL) {
				aprint_error_dev(mi->mi_dev,
				    "couldn't attach drive (no free items)\n");
				mutex_spin_exit(&mi->mi_mtx);
			} else {
				SLIST_REMOVE_HEAD(&mi->mi_freelist, mw_list);
				mutex_spin_exit(&mi->mi_mtx);

				mw->mw_mi = mi;
				mw->mw_mp = *mp;
				workqueue_enqueue(mi->mi_wq,
				    (struct work *)mw, NULL);
			}
		} else
			/* Hack to avoid complaints */
			if (!(((mp->mscp_event & M_ST_MASK) == M_ST_AVAILABLE)
			    && cold))
				(*me->me_gotstatus)(drive, mp);
		break;

	case M_OP_AVAILATTN:
		/*
		 * The drive went offline and we did not notice.
		 * Mark it off line now, to force an on line request
		 * next, so we can make sure it is still the same
		 * drive.
		 *
		 * IF THE UDA DRIVER HAS A COMMAND AWAITING UNIBUS
		 * RESOURCES, THAT COMMAND MAY GO OUT BEFORE THE ON
		 * LINE.  IS IT WORTH FIXING??
		 */
#ifdef notyet
		(*md->md_offline)(ui, mp);
#endif
		break;

	case M_OP_POS | M_OP_END:
	case M_OP_WRITM | M_OP_END:
	case M_OP_AVAILABLE | M_OP_END:
		/*
		 * A non-data transfer operation completed.
		 */
		(*me->me_cmddone)(drive, mp);
		break;

	case M_OP_READ | M_OP_END:
	case M_OP_WRITE | M_OP_END:
		/*
		 * A transfer finished.	 Get the buffer, and release its
		 * map registers via ubadone().	 If the command finished
		 * with an off line or available status, the drive went
		 * off line (the idiot controller does not tell us until
		 * it comes back *on* line, or until we try to use it).
		 */
rwend:
#ifdef DIAGNOSTIC
		if (mp->mscp_cmdref >= NCMD) {
			/*
			 * No buffer means there is a bug somewhere!
			 */
			printf("%s: io done, but bad xfer number?\n",
			    device_xname(drive));
			mscp_hexdump(mp);
			break;
		}
#endif

		if (mp->mscp_cmdref == -1) {
			(*me->me_cmddone)(drive, mp);
			break;
		}
		mxi = &mi->mi_xi[mp->mscp_cmdref];
		if (mxi->mxi_inuse == 0)
			panic("mxi not inuse");
		bp = mxi->mxi_bp;
		/*
		 * Mark any error-due-to-bad-LBN (via `goto rwend').
		 * WHAT STATUS WILL THESE HAVE?	 IT SURE WOULD BE NICE
		 * IF DEC SOLD DOCUMENTATION FOR THEIR OWN CONTROLLERS.
		 */
		bp->b_error = error;
		if (st == M_ST_OFFLINE || st == M_ST_AVAILABLE) {
#ifdef notyet
			(*md->md_offline)(ui, mp);
#endif
		}

		/*
		 * If the transfer failed, give the driver a crack
		 * at fixing things up.
		 */
		if (st != M_ST_SUCCESS) {
			switch ((*me->me_ioerr)(drive, mp, bp)) {

			case MSCP_DONE:		/* fixed */
				break;

			case MSCP_RESTARTED:	/* still working on it */
				goto out;

			case MSCP_FAILED:	/* no luck */
				/* XXX must move to ra.c */
				mscp_printevent(mp);
				break;
			}
		}

		/*
		 * Set the residual count and mark the transfer as
		 * done.  If the I/O wait queue is now empty, release
		 * the shared BDP, if any.
		 */
		bp->b_resid = bp->b_bcount - mp->mscp_seq.seq_bytecount;
		bus_dmamap_unload(mi->mi_dmat, mxi->mxi_dmam);

		(*mc->mc_ctlrdone)(device_parent(mi->mi_dev));
		(*me->me_iodone)(drive, bp);
out:
		mxi->mxi_inuse = 0;
		mi->mi_mxiuse |= (1 << mp->mscp_cmdref);
		break;

	case M_OP_REPLACE | M_OP_END:
		/*
		 * A replace operation finished.  Just let the driver
		 * handle it (if it does replaces).
		 */
		if (me->me_replace == NULL)
			printf("%s: bogus REPLACE end\n", device_xname(drive));
		else
			(*me->me_replace)(drive, mp);
		break;

	default:
		/*
		 * If it is not one of the above, we cannot handle it.
		 * (And we should not have received it, for that matter.)
		 */
unknown:
		printf("%s: unknown opcode 0x%x status 0x%x ignored\n",
			device_xname(drive), mp->mscp_opcode, mp->mscp_status);
#ifdef DIAGNOSTIC
		mscp_hexdump(mp);
#endif
		break;
	}

	/*
	 * If the drive needs to be put back in the controller queue,
	 * do that now.	 (`bp' below ought to be `dp', but they are all
	 * struct buf *.)  Note that b_active was cleared in the driver;
	 * we presume that there is something to be done, hence reassert it.
	 */
#ifdef notyet /* XXX */
	if (ui->ui_flags & UNIT_REQUEUE) {
		...
	}
#endif
done:
	/*
	 * Give back the response packet, and take a look at the next.
	 */
	mp->mscp_msglen = MSCP_MSGLEN;
	mi->mi_rsp.mri_desc[nextrsp] |= MSCP_OWN;
	nextrsp = (nextrsp + 1) % mi->mi_rsp.mri_size;
	goto loop;
}

/*
 * Requeue outstanding transfers, e.g., after bus reset.
 * Also requeue any drives that have on line or unit status
 * info pending.
 */
void
mscp_requeue(struct mscp_softc *mi)
{
	panic("mscp_requeue");
}

void
mscp_worker(struct work *wk, void *dummy)
{
	struct mscp_softc *mi;
	struct mscp_work *mw;
	struct	drive_attach_args da;

	mw = (struct mscp_work *)wk;
	mi = mw->mw_mi;

	da.da_mp = &mw->mw_mp;
	da.da_typ = mi->mi_type;

	config_found(mi->mi_dev, (void *)&da, mscp_print);

	mutex_spin_enter(&mi->mi_mtx);
	SLIST_INSERT_HEAD(&mw->mw_mi->mi_freelist, mw, mw_list);
	mutex_spin_exit(&mi->mi_mtx);
}
