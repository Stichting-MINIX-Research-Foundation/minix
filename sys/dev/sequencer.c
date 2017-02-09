/*	$NetBSD: sequencer.c,v 1.64 2015/08/20 14:40:17 christos Exp $	*/

/*
 * Copyright (c) 1998, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@NetBSD.org) and by Andrew Doran.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Locking:
 *
 * o sc_lock: provides atomic access to all data structures.  Taken from
 *   both process and soft interrupt context.
 *
 * o sc_dvlock: serializes operations on /dev/sequencer.  Taken from
 *   process context.  Dropped while waiting for data in sequencerread()
 *   to allow concurrent reads/writes while no data available.
 *
 * o sc_isopen: we allow only one concurrent open, only to prevent user
 *   and/or application error.
 *
 * o MIDI softc locks.  These can be spinlocks and there can be many of
 *   them, because we can open many MIDI devices.  We take these only in two
 *   places: when enabling redirection from the MIDI device and when
 *   disabling it (open/close).  midiseq_in() is called by the MIDI driver
 *   with its own lock held when passing data into this module.  To avoid
 *   lock order and context problems, we package the received message as a
 *   sequencer_pcqitem_t and put onto a producer-consumer queue.  A soft
 *   interrupt is scheduled to dequeue and decode the message later where we
 *   can safely acquire the sequencer device's sc_lock.  PCQ is lockless for
 *   multiple producer, single consumer settings like this one.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: sequencer.c,v 1.64 2015/08/20 14:40:17 christos Exp $");

#include "sequencer.h"

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/conf.h>
#include <sys/audioio.h>
#include <sys/midiio.h>
#include <sys/device.h>
#include <sys/intr.h>
#include <sys/atomic.h>
#include <sys/pcq.h>
#include <sys/vnode.h>
#include <sys/kauth.h>

#include <dev/midi_if.h>
#include <dev/midivar.h>
#include <dev/sequencervar.h>

#include "ioconf.h"

#define ADDTIMEVAL(a, b) ( \
	(a)->tv_sec += (b)->tv_sec, \
	(a)->tv_usec += (b)->tv_usec, \
	(a)->tv_usec > 1000000 ? ((a)->tv_sec++, (a)->tv_usec -= 1000000) : 0\
	)

#define SUBTIMEVAL(a, b) ( \
	(a)->tv_sec -= (b)->tv_sec, \
	(a)->tv_usec -= (b)->tv_usec, \
	(a)->tv_usec < 0 ? ((a)->tv_sec--, (a)->tv_usec += 1000000) : 0\
	)

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (sequencerdebug) printf x
#define DPRINTFN(n,x)	if (sequencerdebug >= (n)) printf x
int	sequencerdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define SEQ_NOTE_MAX 128
#define SEQ_NOTE_XXX 255

#define RECALC_USPERDIV(t) \
((t)->usperdiv = 60*1000000L/((t)->tempo_beatpermin*(t)->timebase_divperbeat))

typedef union sequencer_pcqitem {
	void	*qi_ptr;
	char	qi_msg[4];
} sequencer_pcqitem_t;

static void seq_reset(struct sequencer_softc *);
static int seq_do_command(struct sequencer_softc *, seq_event_t *);
static int seq_do_chnvoice(struct sequencer_softc *, seq_event_t *);
static int seq_do_chncommon(struct sequencer_softc *, seq_event_t *);
static void seq_timer_waitabs(struct sequencer_softc *, uint32_t);
static int seq_do_timing(struct sequencer_softc *, seq_event_t *);
static int seq_do_local(struct sequencer_softc *, seq_event_t *);
static int seq_do_sysex(struct sequencer_softc *, seq_event_t *);
static int seq_do_fullsize(struct sequencer_softc *, seq_event_t *, struct uio *);
static int seq_input_event(struct sequencer_softc *, seq_event_t *);
static int seq_drain(struct sequencer_softc *);
static void seq_startoutput(struct sequencer_softc *);
static void seq_timeout(void *);
static int seq_to_new(seq_event_t *, struct uio *);
static void seq_softintr(void *);

static int midiseq_out(struct midi_dev *, u_char *, u_int, int);
static struct midi_dev *midiseq_open(int, int);
static void midiseq_close(struct midi_dev *);
static void midiseq_reset(struct midi_dev *);
static int midiseq_noteon(struct midi_dev *, int, int, seq_event_t *);
static int midiseq_noteoff(struct midi_dev *, int, int, seq_event_t *);
static int midiseq_keypressure(struct midi_dev *, int, int, seq_event_t *);
static int midiseq_pgmchange(struct midi_dev *, int, seq_event_t *);
static int midiseq_chnpressure(struct midi_dev *, int, seq_event_t *);
static int midiseq_ctlchange(struct midi_dev *, int, seq_event_t *);
static int midiseq_pitchbend(struct midi_dev *, int, seq_event_t *);
static int midiseq_loadpatch(struct midi_dev *, struct sysex_info *, struct uio *);
void midiseq_in(struct midi_dev *, u_char *, int);

static dev_type_open(sequenceropen);
static dev_type_close(sequencerclose);
static dev_type_read(sequencerread);
static dev_type_write(sequencerwrite);
static dev_type_ioctl(sequencerioctl);
static dev_type_poll(sequencerpoll);
static dev_type_kqfilter(sequencerkqfilter);

const struct cdevsw sequencer_cdevsw = {
	.d_open = sequenceropen,
	.d_close = sequencerclose,
	.d_read = sequencerread,
	.d_write = sequencerwrite,
	.d_ioctl = sequencerioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = sequencerpoll,
	.d_mmap = nommap,
	.d_kqfilter = sequencerkqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER | D_MPSAFE
};
static LIST_HEAD(, sequencer_softc) sequencers = LIST_HEAD_INITIALIZER(sequencers);
static kmutex_t sequencer_lock;

static void
sequencerdestroy(struct sequencer_softc *sc)
{
	callout_halt(&sc->sc_callout, &sc->lock);
	callout_destroy(&sc->sc_callout);
	softint_disestablish(sc->sih);
	cv_destroy(&sc->rchan);
	cv_destroy(&sc->wchan);
	cv_destroy(&sc->lchan);
	if (sc->pcq)
		pcq_destroy(sc->pcq);
	kmem_free(sc, sizeof(*sc));
}

static struct sequencer_softc *
sequencercreate(int unit)
{
	struct sequencer_softc *sc = kmem_zalloc(sizeof(*sc), KM_SLEEP);
	if (sc == NULL) {
#ifdef DIAGNOSTIC
		printf("%s: out of memory\n", __func__);
#endif
		return NULL;
	}
	sc->sc_unit = unit;
	callout_init(&sc->sc_callout, CALLOUT_MPSAFE);
	sc->sih = softint_establish(SOFTINT_NET | SOFTINT_MPSAFE,
	    seq_softintr, sc);
	mutex_init(&sc->lock, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&sc->rchan, "midiseqr");
	cv_init(&sc->wchan, "midiseqw");
	cv_init(&sc->lchan, "midiseql");
	sc->pcq = pcq_create(SEQ_MAXQ, KM_SLEEP);
	if (sc->pcq == NULL) {
		sequencerdestroy(sc);
		return NULL;
	}
	return sc;
}


static struct sequencer_softc *
sequencerget(int unit)
{
	struct sequencer_softc *sc;
	if (unit < 0) {
#ifdef DIAGNOSTIC
		panic("%s: unit %d!", __func__, unit);
#endif
		return NULL;
	}
	mutex_enter(&sequencer_lock);
	LIST_FOREACH(sc, &sequencers, sc_link) {
		if (sc->sc_unit == unit) {
			mutex_exit(&sequencer_lock);
			return sc;
		}
	}
	mutex_exit(&sequencer_lock);
	if ((sc = sequencercreate(unit)) == NULL)
		return NULL;
	mutex_enter(&sequencer_lock);
	LIST_INSERT_HEAD(&sequencers, sc, sc_link);
	mutex_exit(&sequencer_lock);
	return sc;
}

#ifdef notyet
static void 
sequencerput(struct sequencer_softc *sc)
{
	mutex_enter(&sequencer_lock);
	LIST_REMOVE(sc, sc_link);
	mutex_exit(&sequencer_lock);
	sequencerdestroy(sc);
}
#endif

void
sequencerattach(int n)
{
	mutex_init(&sequencer_lock, MUTEX_DEFAULT, IPL_NONE);
}

/*
 * Release reference to device acquired with sequencer_enter().
 */
static void
sequencer_exit(struct sequencer_softc *sc)
{

	sc->dvlock--;
	cv_broadcast(&sc->lchan);
	mutex_exit(&sc->lock);
}

/*
 * Look up sequencer device and acquire locks for device access.
 */
static int
sequencer_enter(dev_t dev, struct sequencer_softc **scp)
{
	struct sequencer_softc *sc;

	/* First, find the device and take sc_lock. */
	if ((sc = sequencerget(SEQUENCERUNIT(dev))) == NULL)
		return ENXIO;
	mutex_enter(&sc->lock);
	while (sc->dvlock) {
		cv_wait(&sc->lchan, &sc->lock);
	}
	sc->dvlock++;
	if (sc->dying) {
		sequencer_exit(sc);
		return EIO;
	}
	*scp = sc;
	return 0;
}

static int
sequenceropen(dev_t dev, int flags, int ifmt, struct lwp *l)
{
	struct sequencer_softc *sc;
	struct midi_dev *md;
	struct midi_softc *msc;
	int error, unit, mdno;

	DPRINTF(("sequenceropen\n"));

	if ((error = sequencer_enter(dev, &sc)) != 0)
		return error;
	if (sc->isopen != 0) {
		sequencer_exit(sc);
		return EBUSY;
	}

	if (SEQ_IS_OLD(SEQUENCERUNIT(dev)))
		sc->mode = SEQ_OLD;
	else
		sc->mode = SEQ_NEW;
	sc->isopen++;
	sc->flags = flags & (FREAD|FWRITE);
	sc->pbus = 0;
	sc->async = 0;
	sc->input_stamp = ~0;

	sc->nmidi = 0;
	sc->ndevs = midi_unit_count();
	sc->timer.timebase_divperbeat = 100;
	sc->timer.tempo_beatpermin = 60;
	RECALC_USPERDIV(&sc->timer);
	sc->timer.divs_lastevent = sc->timer.divs_lastchange = 0;
	microtime(&sc->timer.reftime);

	SEQ_QINIT(&sc->inq);
	SEQ_QINIT(&sc->outq);
	sc->lowat = SEQ_MAXQ / 2;

	if (sc->ndevs > 0) {
		mutex_exit(&sc->lock);
		sc->devs = kmem_alloc(sc->ndevs * sizeof(struct midi_dev *),
		    KM_SLEEP);
		for (unit = 0; unit < sc->ndevs; unit++) {
			md = midiseq_open(unit, flags);
			if (md) {
				sc->devs[sc->nmidi++] = md;
				md->seq = sc;
				md->doingsysex = 0;
				DPRINTF(("%s: midi unit %d opened as seq %p\n",
				    __func__, unit, md));
			} else {
				DPRINTF(("%s: midi unit %d not opened as seq\n",
				    __func__, unit));
			}
		}
		mutex_enter(&sc->lock);
	} else {
		sc->devs = NULL;
	}

	/* Only now redirect input from MIDI devices. */
	for (mdno = 0; mdno < sc->nmidi; mdno++) {
		extern struct cfdriver midi_cd;

		msc = device_lookup_private(&midi_cd, sc->devs[mdno]->unit);
		if (msc) {
			mutex_enter(msc->lock);
			msc->seqopen = 1;
			mutex_exit(msc->lock);
		}
	}

	seq_reset(sc);
	sequencer_exit(sc);

	DPRINTF(("%s: mode=%d, nmidi=%d\n", __func__, sc->mode, sc->nmidi));
	return 0;
}

static int
seq_drain(struct sequencer_softc *sc)
{
	int error;

	KASSERT(mutex_owned(&sc->lock));

	DPRINTFN(3, ("seq_drain: %p, len=%d\n", sc, SEQ_QLEN(&sc->outq)));
	seq_startoutput(sc);
	error = 0;
	while (!SEQ_QEMPTY(&sc->outq) && !error)
		error = cv_timedwait_sig(&sc->wchan, &sc->lock, 60*hz);
	return (error);
}

static void
seq_timeout(void *addr)
{
	struct sequencer_softc *sc = addr;
	proc_t *p;
	pid_t pid;

	DPRINTFN(4, ("seq_timeout: %p\n", sc));

	mutex_enter(&sc->lock);
	if (sc->timeout == 0) {
		mutex_spin_exit(&sc->lock);
		return;
	}
	sc->timeout = 0;
	seq_startoutput(sc);
	if (SEQ_QLEN(&sc->outq) >= sc->lowat) {
		mutex_exit(&sc->lock);
		return;
	}
	cv_broadcast(&sc->wchan);
	selnotify(&sc->wsel, 0, NOTE_SUBMIT);
	if ((pid = sc->async) != 0) {
		mutex_enter(proc_lock);
		if ((p = proc_find(pid)) != NULL)
			psignal(p, SIGIO);
		mutex_exit(proc_lock);
	}
	mutex_exit(&sc->lock);
}

static void
seq_startoutput(struct sequencer_softc *sc)
{
	struct sequencer_queue *q = &sc->outq;
	seq_event_t cmd;

	KASSERT(mutex_owned(&sc->lock));

	if (sc->timeout)
		return;
	DPRINTFN(4, ("seq_startoutput: %p, len=%d\n", sc, SEQ_QLEN(q)));
	while (!SEQ_QEMPTY(q) && !sc->timeout) {
		SEQ_QGET(q, cmd);
		seq_do_command(sc, &cmd);
	}
}

static int
sequencerclose(dev_t dev, int flags, int ifmt, struct lwp *l)
{
	struct sequencer_softc *sc;
	struct midi_softc *msc;
	int unit, error;

	DPRINTF(("%s: %"PRIx64"\n", __func__, dev));

	if ((error = sequencer_enter(dev, &sc)) != 0)
		return error;
	seq_drain(sc);
	if (sc->timeout) {
		callout_halt(&sc->sc_callout, &sc->lock);
		sc->timeout = 0;
	}
	/* Bin input from MIDI devices. */
	for (unit = 0; unit < sc->nmidi; unit++) {
		extern struct cfdriver midi_cd;

		msc = device_lookup_private(&midi_cd, unit);
		if (msc) {
			mutex_enter(msc->lock);
			msc->seqopen = 0;
			mutex_exit(msc->lock);
		}
	}
	mutex_exit(&sc->lock);

	for (unit = 0; unit < sc->nmidi; unit++)
		if (sc->devs[unit] != NULL)
			midiseq_close(sc->devs[unit]);
	if (sc->devs != NULL) {
		KASSERT(sc->ndevs > 0);
		kmem_free(sc->devs, sc->ndevs * sizeof(struct midi_dev *));
		sc->devs = NULL;
	}

	mutex_enter(&sc->lock);
	sc->isopen = 0;
	sequencer_exit(sc);

	DPRINTF(("%s: %"PRIx64" done\n", __func__, dev));

	return (0);
}

static int
seq_input_event(struct sequencer_softc *sc, seq_event_t *cmd)
{
	struct sequencer_queue *q;

	KASSERT(mutex_owned(&sc->lock));

	DPRINTFN(2, ("seq_input_event: %02x %02x %02x %02x %02x "
	    "%02x %02x %02x\n", cmd->tag,
	    cmd->unknown.byte[0], cmd->unknown.byte[1],
	    cmd->unknown.byte[2], cmd->unknown.byte[3],
	    cmd->unknown.byte[4], cmd->unknown.byte[5],
	    cmd->unknown.byte[6]));
	q = &sc->inq;
	if (SEQ_QFULL(q))
		return (ENOMEM);
	SEQ_QPUT(q, *cmd);
	cv_broadcast(&sc->rchan);
	selnotify(&sc->rsel, 0, NOTE_SUBMIT);
	if (sc->async != 0) {
		proc_t *p;

		mutex_enter(proc_lock);
		if ((p = proc_find(sc->async)) != NULL)
			psignal(p, SIGIO);
		mutex_exit(proc_lock);
	}
	return 0;
}

static void
seq_softintr(void *addr)
{
	struct sequencer_softc *sc;
	struct timeval now;
	seq_event_t ev;
	int status, chan, unit;
	sequencer_pcqitem_t qi;
	u_long t;

	sc = addr;

	mutex_enter(&sc->lock);

	qi.qi_ptr = pcq_get(sc->pcq);
	if (qi.qi_ptr == NULL) {
		mutex_exit(&sc->lock);
		return;
	}
	KASSERT((qi.qi_msg[3] & 0x80) != 0);
	unit = qi.qi_msg[3] & ~0x80;
	status = MIDI_GET_STATUS(qi.qi_msg[0]);
	chan = MIDI_GET_CHAN(qi.qi_msg[0]);
	switch (status) {
	case MIDI_NOTEON: /* midi(4) always canonicalizes hidden note-off */
		ev = SEQ_MK_CHN(NOTEON, .device=unit, .channel=chan,
		    .key=qi.qi_msg[1], .velocity=qi.qi_msg[2]);
		break;
	case MIDI_NOTEOFF:
		ev = SEQ_MK_CHN(NOTEOFF, .device=unit, .channel=chan,
		    .key=qi.qi_msg[1], .velocity=qi.qi_msg[2]);
		break;
	case MIDI_KEY_PRESSURE:
		ev = SEQ_MK_CHN(KEY_PRESSURE, .device=unit, .channel=chan,
		    .key=qi.qi_msg[1], .pressure=qi.qi_msg[2]);
		break;
	case MIDI_CTL_CHANGE: /* XXX not correct for MSB */
		ev = SEQ_MK_CHN(CTL_CHANGE, .device=unit, .channel=chan,
		    .controller=qi.qi_msg[1], .value=qi.qi_msg[2]);
		break;
	case MIDI_PGM_CHANGE:
		ev = SEQ_MK_CHN(PGM_CHANGE, .device=unit, .channel=chan,
		    .program=qi.qi_msg[1]);
		break;
	case MIDI_CHN_PRESSURE:
		ev = SEQ_MK_CHN(CHN_PRESSURE, .device=unit, .channel=chan,
		    .pressure=qi.qi_msg[1]);
		break;
	case MIDI_PITCH_BEND:
		ev = SEQ_MK_CHN(PITCH_BEND, .device=unit, .channel=chan,
		    .value=(qi.qi_msg[1] & 0x7f) | ((qi.qi_msg[2] & 0x7f) << 7));
		break;
	default: /* this is now the point where MIDI_ACKs disappear */
		mutex_exit(&sc->lock);
		return;
	}
	microtime(&now);
	if (!sc->timer.running)
		now = sc->timer.stoptime;
	SUBTIMEVAL(&now, &sc->timer.reftime);
	t = now.tv_sec * 1000000 + now.tv_usec;
	t /= sc->timer.usperdiv;
	t += sc->timer.divs_lastchange;
	if (t != sc->input_stamp) {
		seq_input_event(sc, &SEQ_MK_TIMING(WAIT_ABS, .divisions=t));
		sc->input_stamp = t; /* XXX what happens if timer is reset? */
	}
	seq_input_event(sc, &ev);
	mutex_exit(&sc->lock);
}

static int
sequencerread(dev_t dev, struct uio *uio, int ioflag)
{
	struct sequencer_softc *sc;
	struct sequencer_queue *q;
	seq_event_t ev;
	int error;

	DPRINTFN(2, ("sequencerread: %"PRIx64", count=%d, ioflag=%x\n",
	   dev, (int)uio->uio_resid, ioflag));

	if ((error = sequencer_enter(dev, &sc)) != 0)
		return error;
	q = &sc->inq;

	if (sc->mode == SEQ_OLD) {
		sequencer_exit(sc);
		DPRINTFN(-1,("sequencerread: old read\n"));
		return EINVAL; /* XXX unimplemented */
	}
	while (SEQ_QEMPTY(q)) {
		if (ioflag & IO_NDELAY) {
			error = EWOULDBLOCK;
			break;
		}
		/* Drop lock to allow concurrent read/write. */
		KASSERT(sc->dvlock != 0);
		sc->dvlock--;
		error = cv_wait_sig(&sc->rchan, &sc->lock);
		while (sc->dvlock != 0) {
			cv_wait(&sc->lchan, &sc->lock);
		}
		sc->dvlock++;
		if (error) {
			break;
		}
	}
	while (uio->uio_resid >= sizeof(ev) && !error && !SEQ_QEMPTY(q)) {
		SEQ_QGET(q, ev);
		mutex_exit(&sc->lock);
		error = uiomove(&ev, sizeof(ev), uio);
		mutex_enter(&sc->lock);
	}
	sequencer_exit(sc);
	return error;
}

static int
sequencerwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct sequencer_softc *sc;
	struct sequencer_queue *q;
	int error;
	seq_event_t cmdbuf;
	int size;
	
	DPRINTFN(2, ("sequencerwrite: %"PRIx64", count=%d\n", dev,
	    (int)uio->uio_resid));

	if ((error = sequencer_enter(dev, &sc)) != 0)
		return error;
	q = &sc->outq;

	size = sc->mode == SEQ_NEW ? sizeof cmdbuf : SEQOLD_CMDSIZE;
	while (uio->uio_resid >= size && error == 0) {
		mutex_exit(&sc->lock);
		error = uiomove(&cmdbuf, size, uio);
		if (error == 0) {
			if (sc->mode == SEQ_OLD && seq_to_new(&cmdbuf, uio)) {
				mutex_enter(&sc->lock);
				continue;
			}
			if (cmdbuf.tag == SEQ_FULLSIZE) {
				/* We do it like OSS does, asynchronously */
				error = seq_do_fullsize(sc, &cmdbuf, uio);
				if (error == 0) {
					mutex_enter(&sc->lock);
					continue;
				}
			}
		}
		mutex_enter(&sc->lock);
		if (error != 0) {
			break;
		}
		while (SEQ_QFULL(q)) {
			seq_startoutput(sc);
			if (SEQ_QFULL(q)) {
				if (ioflag & IO_NDELAY) {
					error = EWOULDBLOCK;
					break;
				}
				error = cv_wait_sig(&sc->wchan, &sc->lock);
				if (error) {
					 break;
				}
			}
		}
		if (error == 0) {
			SEQ_QPUT(q, cmdbuf);
		}
	}
	if (error == 0) {
		seq_startoutput(sc);
	} else {
		DPRINTFN(2, ("sequencerwrite: error=%d\n", error));
	}
	sequencer_exit(sc);
	return error;
}

static int
sequencerioctl(dev_t dev, u_long cmd, void *addr, int flag, struct lwp *l)
{
	struct sequencer_softc *sc;
	struct synth_info *si;
	struct midi_dev *md;
	int devno, error, t;
	struct timeval now;
	u_long tx;

	DPRINTFN(2, ("sequencerioctl: %"PRIx64" cmd=0x%08lx\n", dev, cmd));

	if ((error = sequencer_enter(dev, &sc)) != 0)
		return error;
	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer. */
		break;

	case FIOASYNC:
		if (*(int *)addr) {
			if (sc->async != 0)
				return EBUSY;
			sc->async = curproc->p_pid;
			DPRINTF(("%s: FIOASYNC %d\n", __func__,
			    sc->async));
		} else {
			sc->async = 0;
		}
		break;

	case SEQUENCER_RESET:
		seq_reset(sc);
		break;

	case SEQUENCER_PANIC:
		seq_reset(sc);
		/* Do more?  OSS doesn't */
		break;

	case SEQUENCER_SYNC:
		if (sc->flags != FREAD)
			seq_drain(sc);
		break;

	case SEQUENCER_INFO:
		si = (struct synth_info*)addr;
		devno = si->device;
		if (devno < 0 || devno >= sc->nmidi) {
			error = EINVAL;
			break;
		}
		md = sc->devs[devno];
		strncpy(si->name, md->name, sizeof si->name);
		si->synth_type = SYNTH_TYPE_MIDI;
		si->synth_subtype = md->subtype;
		si->nr_voices = md->nr_voices;
		si->instr_bank_size = md->instr_bank_size;
		si->capabilities = md->capabilities;
		break;

	case SEQUENCER_NRSYNTHS:
		*(int *)addr = sc->nmidi;
		break;

	case SEQUENCER_NRMIDIS:
		*(int *)addr = sc->nmidi;
		break;

	case SEQUENCER_OUTOFBAND:
		DPRINTFN(3, ("sequencer_ioctl: OOB=%02x %02x %02x %02x %02x %02x %02x %02x\n",
		    *(u_char *)addr, *((u_char *)addr+1),
		    *((u_char *)addr+2), *((u_char *)addr+3),
		    *((u_char *)addr+4), *((u_char *)addr+5),
		    *((u_char *)addr+6), *((u_char *)addr+7)));
		if ((sc->flags & FWRITE) == 0) {
			error = EBADF;
		} else {
			error = seq_do_command(sc, (seq_event_t *)addr);
		}
		break;

	case SEQUENCER_TMR_TIMEBASE:
		t = *(int *)addr;
		if (t < 1)
			t = 1;
		if (t > 10000)
			t = 10000;
		*(int *)addr = t;
		sc->timer.timebase_divperbeat = t;
		sc->timer.divs_lastchange = sc->timer.divs_lastevent;
		microtime(&sc->timer.reftime);
		RECALC_USPERDIV(&sc->timer);
		break;

	case SEQUENCER_TMR_START:
		error = seq_do_timing(sc, &SEQ_MK_TIMING(START));
		break;

	case SEQUENCER_TMR_STOP:
		error = seq_do_timing(sc, &SEQ_MK_TIMING(STOP));
		break;

	case SEQUENCER_TMR_CONTINUE:
		error = seq_do_timing(sc, &SEQ_MK_TIMING(CONTINUE));
		break;

	case SEQUENCER_TMR_TEMPO:
		error = seq_do_timing(sc,
		    &SEQ_MK_TIMING(TEMPO, .bpm=*(int *)addr));
		RECALC_USPERDIV(&sc->timer);
		if (error == 0)
			*(int *)addr = sc->timer.tempo_beatpermin;
		break;

	case SEQUENCER_TMR_SOURCE:
		*(int *)addr = SEQUENCER_TMR_INTERNAL;
		break;

	case SEQUENCER_TMR_METRONOME:
		/* noop */
		break;

	case SEQUENCER_THRESHOLD:
		t = SEQ_MAXQ - *(int *)addr / sizeof (seq_event_rec);
		if (t < 1)
			t = 1;
		if (t > SEQ_MAXQ)
			t = SEQ_MAXQ;
		sc->lowat = t;
		break;

	case SEQUENCER_CTRLRATE:
		*(int *)addr = (sc->timer.tempo_beatpermin
		    *sc->timer.timebase_divperbeat + 30) / 60;
		break;

	case SEQUENCER_GETTIME:
		microtime(&now);
		SUBTIMEVAL(&now, &sc->timer.reftime);
		tx = now.tv_sec * 1000000 + now.tv_usec;
		tx /= sc->timer.usperdiv;
		tx += sc->timer.divs_lastchange;
		*(int *)addr = tx;
		break;

	default:
		DPRINTFN(-1,("sequencer_ioctl: unimpl %08lx\n", cmd));
		error = EINVAL;
		break;
	}
	sequencer_exit(sc);

	return error;
}

static int
sequencerpoll(dev_t dev, int events, struct lwp *l)
{
	struct sequencer_softc *sc;
	int revents = 0;
	if ((sc = sequencerget(SEQUENCERUNIT(dev))) == NULL)
		return ENXIO;

	DPRINTF(("%s: %p events=0x%x\n", __func__, sc, events));

	mutex_enter(&sc->lock);
	if (events & (POLLIN | POLLRDNORM))
		if ((sc->flags&FREAD) && !SEQ_QEMPTY(&sc->inq))
			revents |= events & (POLLIN | POLLRDNORM);

	if (events & (POLLOUT | POLLWRNORM))
		if ((sc->flags&FWRITE) && SEQ_QLEN(&sc->outq) < sc->lowat)
			revents |= events & (POLLOUT | POLLWRNORM);

	if (revents == 0) {
		if ((sc->flags&FREAD) && (events & (POLLIN | POLLRDNORM)))
			selrecord(l, &sc->rsel);

		if ((sc->flags&FWRITE) && (events & (POLLOUT | POLLWRNORM)))
			selrecord(l, &sc->wsel);
	}
	mutex_exit(&sc->lock);

	return revents;
}

static void
filt_sequencerrdetach(struct knote *kn)
{
	struct sequencer_softc *sc = kn->kn_hook;

	mutex_enter(&sc->lock);
	SLIST_REMOVE(&sc->rsel.sel_klist, kn, knote, kn_selnext);
	mutex_exit(&sc->lock);
}

static int
filt_sequencerread(struct knote *kn, long hint)
{
	struct sequencer_softc *sc = kn->kn_hook;
	int rv;

	if (hint != NOTE_SUBMIT) {
		mutex_enter(&sc->lock);
	}
	if (SEQ_QEMPTY(&sc->inq)) {
		rv = 0;
	} else {
		kn->kn_data = sizeof(seq_event_rec);
		rv = 1;
	}
	if (hint != NOTE_SUBMIT) {
		mutex_exit(&sc->lock);
	}
	return rv;
}

static const struct filterops sequencerread_filtops =
	{ 1, NULL, filt_sequencerrdetach, filt_sequencerread };

static void
filt_sequencerwdetach(struct knote *kn)
{
	struct sequencer_softc *sc = kn->kn_hook;

	mutex_enter(&sc->lock);
	SLIST_REMOVE(&sc->wsel.sel_klist, kn, knote, kn_selnext);
	mutex_exit(&sc->lock);
}

static int
filt_sequencerwrite(struct knote *kn, long hint)
{
	struct sequencer_softc *sc = kn->kn_hook;
	int rv;

	if (hint != NOTE_SUBMIT) {
		mutex_enter(&sc->lock);
	}
	if (SEQ_QLEN(&sc->outq) >= sc->lowat) {
		rv = 0;
	} else {
		kn->kn_data = sizeof(seq_event_rec);
		rv = 1;
	}
	if (hint != NOTE_SUBMIT) {
		mutex_exit(&sc->lock);
	}
	return rv;
}

static const struct filterops sequencerwrite_filtops =
	{ 1, NULL, filt_sequencerwdetach, filt_sequencerwrite };

static int
sequencerkqfilter(dev_t dev, struct knote *kn)
{
	struct sequencer_softc *sc;
	struct klist *klist;
	if ((sc = sequencerget(SEQUENCERUNIT(dev))) == NULL)
		return ENXIO;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &sc->rsel.sel_klist;
		kn->kn_fop = &sequencerread_filtops;
		break;

	case EVFILT_WRITE:
		klist = &sc->wsel.sel_klist;
		kn->kn_fop = &sequencerwrite_filtops;
		break;

	default:
		return (EINVAL);
	}

	kn->kn_hook = sc;

	mutex_enter(&sc->lock);
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	mutex_exit(&sc->lock);

	return (0);
}

static void
seq_reset(struct sequencer_softc *sc)
{
	int i, chn;
	struct midi_dev *md;

	KASSERT(mutex_owned(&sc->lock));

	if (!(sc->flags & FWRITE))
	        return;
	for (i = 0; i < sc->nmidi; i++) {
		md = sc->devs[i];
		midiseq_reset(md);
		for (chn = 0; chn < MAXCHAN; chn++) {
			midiseq_ctlchange(md, chn, &SEQ_MK_CHN(CTL_CHANGE,
			    .controller=MIDI_CTRL_NOTES_OFF));
			midiseq_ctlchange(md, chn, &SEQ_MK_CHN(CTL_CHANGE,
			    .controller=MIDI_CTRL_RESET));
			midiseq_pitchbend(md, chn, &SEQ_MK_CHN(PITCH_BEND,
			    .value=MIDI_BEND_NEUTRAL));
		}
	}
}

static int
seq_do_command(struct sequencer_softc *sc, seq_event_t *b)
{
	int dev;

	KASSERT(mutex_owned(&sc->lock));

	DPRINTFN(4, ("seq_do_command: %p cmd=0x%02x\n", sc, b->timing.op));

	switch(b->tag) {
	case SEQ_LOCAL:
		return seq_do_local(sc, b);
	case SEQ_TIMING:
		return seq_do_timing(sc, b);
	case SEQ_CHN_VOICE:
		return seq_do_chnvoice(sc, b);
	case SEQ_CHN_COMMON:
		return seq_do_chncommon(sc, b);
	case SEQ_SYSEX:
		return seq_do_sysex(sc, b);
	/* COMPAT */
	case SEQOLD_MIDIPUTC:
		dev = b->putc.device;
		if (dev < 0 || dev >= sc->nmidi)
			return (ENXIO);
		return midiseq_out(sc->devs[dev], &b->putc.byte, 1, 0);
	default:
		DPRINTFN(-1,("seq_do_command: unimpl command %02x\n", b->tag));
		return (EINVAL);
	}
}

static int
seq_do_chnvoice(struct sequencer_softc *sc, seq_event_t *b)
{
	int dev;
	int error;
	struct midi_dev *md;

	KASSERT(mutex_owned(&sc->lock));

	dev = b->voice.device;
	if (dev < 0 || dev >= sc->nmidi ||
	    b->voice.channel > 15 ||
	    b->voice.key >= SEQ_NOTE_MAX)
		return ENXIO;
	md = sc->devs[dev];
	switch(b->voice.op) {
	case MIDI_NOTEON: /* no need to special-case hidden noteoff here */
		error = midiseq_noteon(md, b->voice.channel, b->voice.key, b);
		break;
	case MIDI_NOTEOFF:
		error = midiseq_noteoff(md, b->voice.channel, b->voice.key, b);
		break;
	case MIDI_KEY_PRESSURE:
		error = midiseq_keypressure(md,
		    b->voice.channel, b->voice.key, b);
		break;
	default:
		DPRINTFN(-1,("seq_do_chnvoice: unimpl command %02x\n",
			b->voice.op));
		error = EINVAL;
		break;
	}
	return error;
}

static int
seq_do_chncommon(struct sequencer_softc *sc, seq_event_t *b)
{
	int dev;
	int error;
	struct midi_dev *md;

	KASSERT(mutex_owned(&sc->lock));

	dev = b->common.device;
	if (dev < 0 || dev >= sc->nmidi ||
	    b->common.channel > 15)
		return ENXIO;
	md = sc->devs[dev];
	DPRINTFN(2,("seq_do_chncommon: %02x\n", b->common.op));

	error = 0;
	switch(b->common.op) {
	case MIDI_PGM_CHANGE:
		error = midiseq_pgmchange(md, b->common.channel, b);
		break;
	case MIDI_CTL_CHANGE:
		error = midiseq_ctlchange(md, b->common.channel, b);
		break;
	case MIDI_PITCH_BEND:
		error = midiseq_pitchbend(md, b->common.channel, b);
		break;
	case MIDI_CHN_PRESSURE:
		error = midiseq_chnpressure(md, b->common.channel, b);
		break;
	default:
		DPRINTFN(-1,("seq_do_chncommon: unimpl command %02x\n",
			b->common.op));
		error = EINVAL;
		break;
	}
	return error;
}

static int
seq_do_local(struct sequencer_softc *sc, seq_event_t *b)
{

	KASSERT(mutex_owned(&sc->lock));

	return (EINVAL);
}

static int
seq_do_sysex(struct sequencer_softc *sc, seq_event_t *b)
{
	int dev, i;
	struct midi_dev *md;
	uint8_t *bf = b->sysex.buffer;

	KASSERT(mutex_owned(&sc->lock));

	dev = b->sysex.device;
	if (dev < 0 || dev >= sc->nmidi)
		return (ENXIO);
	DPRINTF(("%s: dev=%d\n", __func__, dev));
	md = sc->devs[dev];

	if (!md->doingsysex) {
		midiseq_out(md, (uint8_t[]){MIDI_SYSEX_START}, 1, 0);
		md->doingsysex = 1;
	}

	for (i = 0; i < 6 && bf[i] != 0xff; i++)
		;
	midiseq_out(md, bf, i, 0);
	if (i < 6 || (i > 0 && bf[i-1] == MIDI_SYSEX_END))
		md->doingsysex = 0;
	return 0;
}

static void
seq_timer_waitabs(struct sequencer_softc *sc, uint32_t divs)
{
	struct timeval when;
	long long usec;
	struct syn_timer *t;
	int ticks;

	KASSERT(mutex_owned(&sc->lock));

	t = &sc->timer;
	t->divs_lastevent = divs;
	divs -= t->divs_lastchange;
	usec = (long long)divs * (long long)t->usperdiv; /* convert to usec */
	when.tv_sec = usec / 1000000;
	when.tv_usec = usec % 1000000;
	DPRINTFN(4, ("seq_timer_waitabs: adjdivs=%d, sleep when=%"PRId64".%06"PRId64,
	             divs, when.tv_sec, (uint64_t)when.tv_usec));
	ADDTIMEVAL(&when, &t->reftime); /* abstime for end */
	ticks = tvhzto(&when);
	DPRINTFN(4, (" when+start=%"PRId64".%06"PRId64", tick=%d\n",
		     when.tv_sec, (uint64_t)when.tv_usec, ticks));
	if (ticks > 0) {
#ifdef DIAGNOSTIC
		if (ticks > 20 * hz) {
			/* Waiting more than 20s */
			printf("seq_timer_waitabs: funny ticks=%d, "
			       "usec=%lld\n", ticks, usec);
		}
#endif
		sc->timeout = 1;
		callout_reset(&sc->sc_callout, ticks,
		    seq_timeout, sc);
	}
#ifdef SEQUENCER_DEBUG
	else if (tick < 0)
		DPRINTF(("%s: ticks = %d\n", __func__, ticks));
#endif
}

static int
seq_do_timing(struct sequencer_softc *sc, seq_event_t *b)
{
	struct syn_timer *t = &sc->timer;
	struct timeval when;
	int error;

	KASSERT(mutex_owned(&sc->lock));

	error = 0;
	switch(b->timing.op) {
	case TMR_WAIT_REL:
		seq_timer_waitabs(sc,
		    b->t_WAIT_REL.divisions + t->divs_lastevent);
		break;
	case TMR_WAIT_ABS:
		seq_timer_waitabs(sc, b->t_WAIT_ABS.divisions);
		break;
	case TMR_START:
		microtime(&t->reftime);
		t->divs_lastevent = t->divs_lastchange = 0;
		t->running = 1;
		break;
	case TMR_STOP:
		microtime(&t->stoptime);
		t->running = 0;
		break;
	case TMR_CONTINUE:
		if (t->running)
			break;
		microtime(&when);
		SUBTIMEVAL(&when, &t->stoptime);
		ADDTIMEVAL(&t->reftime, &when);
		t->running = 1;
		break;
	case TMR_TEMPO:
		/* bpm is unambiguously MIDI clocks per minute / 24 */
		/* (24 MIDI clocks are usually but not always a quarter note) */
		if (b->t_TEMPO.bpm < 8) /* where are these limits specified? */
			t->tempo_beatpermin = 8;
		else if (b->t_TEMPO.bpm > 360) /* ? */
			t->tempo_beatpermin = 360;
		else
			t->tempo_beatpermin = b->t_TEMPO.bpm;
		t->divs_lastchange = t->divs_lastevent;
		microtime(&t->reftime);
		RECALC_USPERDIV(t);
		break;
	case TMR_ECHO:
		error = seq_input_event(sc, b);
		break;
	case TMR_RESET:
		t->divs_lastevent = t->divs_lastchange = 0;
		microtime(&t->reftime);
		break;
	case TMR_SPP:
	case TMR_TIMESIG:
		DPRINTF(("%s: unimplemented %02x\n", __func__, b->timing.op));
		error = EINVAL; /* not quite accurate... */
		break;
	default:
		DPRINTF(("%s: unknown %02x\n", __func__, b->timing.op));
		error = EINVAL;
		break;
	}
	return (error);
}

static int
seq_do_fullsize(struct sequencer_softc *sc, seq_event_t *b, struct uio *uio)
{
	struct sysex_info sysex;
	u_int dev;

#ifdef DIAGNOSTIC
	if (sizeof(seq_event_rec) != SEQ_SYSEX_HDRSIZE) {
		printf("seq_do_fullsize: sysex size ??\n");
		return EINVAL;
	}
#endif
	memcpy(&sysex, b, sizeof sysex);
	dev = sysex.device_no;
	if (/* dev < 0 || */ dev >= sc->nmidi)
		return (ENXIO);
	DPRINTFN(2, ("seq_do_fullsize: fmt=%04x, dev=%d, len=%d\n",
		     sysex.key, dev, sysex.len));
	return (midiseq_loadpatch(sc->devs[dev], &sysex, uio));
}

/*
 * Convert an old sequencer event to a new one.
 * NOTE: on entry, *ev may contain valid data only in the first 4 bytes.
 * That may be true even on exit (!) in the case of SEQOLD_MIDIPUTC; the
 * caller will only look at the first bytes in that case anyway. Ugly? Sure.
 */
static int
seq_to_new(seq_event_t *ev, struct uio *uio)
{
	int cmd, chan, note, parm;
	uint32_t tmp_delay;
	int error;
	uint8_t *bfp;

	cmd = ev->tag;
	bfp = ev->unknown.byte;
	chan = *bfp++;
	note = *bfp++;
	parm = *bfp++;
	DPRINTFN(3, ("seq_to_new: 0x%02x %d %d %d\n", cmd, chan, note, parm));

	if (cmd >= 0x80) {
		/* Fill the event record */
		if (uio->uio_resid >= sizeof *ev - SEQOLD_CMDSIZE) {
			error = uiomove(bfp, sizeof *ev - SEQOLD_CMDSIZE, uio);
			if (error)
				return error;
		} else
			return EINVAL;
	}

	switch(cmd) {
	case SEQOLD_NOTEOFF:
		/*
		 * What's with the SEQ_NOTE_XXX?  In OSS this seems to have
		 * been undocumented magic for messing with the overall volume
		 * of a 'voice', equated precariously with 'channel' and
		 * pretty much unimplementable except by directly frobbing a
		 * synth chip. For us, who treat everything as interfaced over
		 * MIDI, this will just be unceremoniously discarded as
		 * invalid in midiseq_noteoff, making the whole event an
		 * elaborate no-op, and that doesn't seem to be any different
		 * from what happens on linux with a MIDI-interfaced device,
		 * by the way. The moral is ... use the new /dev/music API, ok?
		 */
		*ev = SEQ_MK_CHN(NOTEOFF, .device=0, .channel=chan,
		    .key=SEQ_NOTE_XXX, .velocity=parm);
		break;
	case SEQOLD_NOTEON:
		*ev = SEQ_MK_CHN(NOTEON,
		    .device=0, .channel=chan, .key=note, .velocity=parm);
		break;
	case SEQOLD_WAIT:
		/*
		 * This event cannot even /exist/ on non-littleendian machines,
		 * and so help me, that's exactly the way OSS defined it.
		 * Also, the OSS programmer's guide states (p. 74, v1.11)
		 * that seqold time units are system clock ticks, unlike
		 * the new 'divisions' which are determined by timebase. In
		 * that case we would need to do scaling here - but no such
		 * behavior is visible in linux either--which also treats this
		 * value, surprisingly, as an absolute, not relative, time.
		 * My guess is that this event has gone unused so long that
		 * nobody could agree we got it wrong no matter what we do.
		 */
		tmp_delay = *(uint32_t *)ev >> 8;
		*ev = SEQ_MK_TIMING(WAIT_ABS, .divisions=tmp_delay);
		break;
	case SEQOLD_SYNCTIMER:
		/*
		 * The TMR_RESET event is not defined in any OSS materials
		 * I can find; it may have been invented here just to provide
		 * an accurate _to_new translation of this event.
		 */
		*ev = SEQ_MK_TIMING(RESET);
		break;
	case SEQOLD_PGMCHANGE:
		*ev = SEQ_MK_CHN(PGM_CHANGE,
		    .device=0, .channel=chan, .program=note);
		break;
	case SEQOLD_MIDIPUTC:
		break;		/* interpret in normal mode */
	case SEQOLD_ECHO:
	case SEQOLD_PRIVATE:
	case SEQOLD_EXTENDED:
	default:
		DPRINTF(("%s: not impl 0x%02x\n", __func__, cmd));
		return EINVAL;
	/* In case new-style events show up */
	case SEQ_TIMING:
	case SEQ_CHN_VOICE:
	case SEQ_CHN_COMMON:
	case SEQ_FULLSIZE:
		break;
	}
	return 0;
}

/**********************************************/

void
midiseq_in(struct midi_dev *md, u_char *msg, int len)
{
	struct sequencer_softc *sc;
	sequencer_pcqitem_t qi;

	DPRINTFN(2, ("midiseq_in: %p %02x %02x %02x\n",
		     md, msg[0], msg[1], msg[2]));

	sc = md->seq;

	qi.qi_msg[0] = msg[0];
	qi.qi_msg[1] = msg[1];
	qi.qi_msg[2] = msg[2];
	qi.qi_msg[3] = md->unit | 0x80;	/* ensure non-zero value of qi_ptr */
	pcq_put(sc->pcq, qi.qi_ptr);
	softint_schedule(sc->sih);
}

static struct midi_dev *
midiseq_open(int unit, int flags)
{
	extern struct cfdriver midi_cd;
	int error;
	struct midi_dev *md;
	struct midi_softc *sc;
	struct midi_info mi;
	int major;
	dev_t dev;
	vnode_t *vp;
	int oflags;
	
	major = devsw_name2chr("midi", NULL, 0);
	dev = makedev(major, unit);

	DPRINTFN(2, ("midiseq_open: %d %d\n", unit, flags));

	error = cdevvp(dev, &vp);
	if (error)
		return NULL;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_OPEN(vp, flags, kauth_cred_get());
	VOP_UNLOCK(vp);
	if (error) {
		vrele(vp);
		return NULL;
	}

	/* Only after we have acquired reference via VOP_OPEN(). */
	midi_getinfo(dev, &mi);
	oflags = flags;
	if ((mi.props & MIDI_PROP_CAN_INPUT) == 0)
	        flags &= ~FREAD;
	if ((flags & (FREAD|FWRITE)) == 0) {
		VOP_CLOSE(vp, oflags, kauth_cred_get());
		vrele(vp);
	        return NULL;
	}

	sc = device_lookup_private(&midi_cd, unit);
	md = kmem_zalloc(sizeof(*md), KM_SLEEP);
	md->unit = unit;
	md->name = mi.name;
	md->subtype = 0;
	md->nr_voices = 128;	/* XXX */
	md->instr_bank_size = 128; /* XXX */
	md->vp = vp;
	if (mi.props & MIDI_PROP_CAN_INPUT)
		md->capabilities |= SYNTH_CAP_INPUT;
	sc->seq_md = md;
	return (md);
}

static void
midiseq_close(struct midi_dev *md)
{
	DPRINTFN(2, ("midiseq_close: %d\n", md->unit));
	(void)vn_close(md->vp, 0, kauth_cred_get());
	kmem_free(md, sizeof(*md));
}

static void
midiseq_reset(struct midi_dev *md)
{
	/* XXX send GM reset? */
	DPRINTFN(3, ("midiseq_reset: %d\n", md->unit));
}

static int
midiseq_out(struct midi_dev *md, u_char *bf, u_int cc, int chk)
{
	DPRINTFN(5, ("midiseq_out: md=%p, unit=%d, bf[0]=0x%02x, cc=%d\n",
		     md, md->unit, bf[0], cc));

	/* midi(4) does running status compression where appropriate. */
	return midi_writebytes(md->unit, bf, cc);
}

/*
 * If the writing process hands us a hidden note-off in a note-on event,
 * we will simply write it that way; no need to special case it here,
 * as midi(4) will always canonicalize or compress as appropriate anyway.
 */
static int
midiseq_noteon(struct midi_dev *md, int chan, int key, seq_event_t *ev)
{
	return midiseq_out(md, (uint8_t[]){
	    MIDI_NOTEON | chan, key, ev->c_NOTEON.velocity & 0x7f}, 3, 1);
}

static int
midiseq_noteoff(struct midi_dev *md, int chan, int key, seq_event_t *ev)
{
	return midiseq_out(md, (uint8_t[]){
	    MIDI_NOTEOFF | chan, key, ev->c_NOTEOFF.velocity & 0x7f}, 3, 1);
}

static int
midiseq_keypressure(struct midi_dev *md, int chan, int key, seq_event_t *ev)
{
	return midiseq_out(md, (uint8_t[]){
	    MIDI_KEY_PRESSURE | chan, key,
	    ev->c_KEY_PRESSURE.pressure & 0x7f}, 3, 1);
}

static int
midiseq_pgmchange(struct midi_dev *md, int chan, seq_event_t *ev)
{
	if (ev->c_PGM_CHANGE.program > 127)
		return EINVAL;
	return midiseq_out(md, (uint8_t[]){
	    MIDI_PGM_CHANGE | chan, ev->c_PGM_CHANGE.program}, 2, 1);
}

static int
midiseq_chnpressure(struct midi_dev *md, int chan, seq_event_t *ev)
{
	if (ev->c_CHN_PRESSURE.pressure > 127)
		return EINVAL;
	return midiseq_out(md, (uint8_t[]){
	    MIDI_CHN_PRESSURE | chan, ev->c_CHN_PRESSURE.pressure}, 2, 1);
}

static int
midiseq_ctlchange(struct midi_dev *md, int chan, seq_event_t *ev)
{
	if (ev->c_CTL_CHANGE.controller > 127)
		return EINVAL;
	return midiseq_out( md, (uint8_t[]){
	    MIDI_CTL_CHANGE | chan, ev->c_CTL_CHANGE.controller,
	    ev->c_CTL_CHANGE.value & 0x7f /* XXX this is SO wrong */
	    }, 3, 1);
}

static int
midiseq_pitchbend(struct midi_dev *md, int chan, seq_event_t *ev)
{
	return midiseq_out(md, (uint8_t[]){
	    MIDI_PITCH_BEND | chan,
	    ev->c_PITCH_BEND.value & 0x7f,
	    (ev->c_PITCH_BEND.value >> 7) & 0x7f}, 3, 1);
}

static int
midiseq_loadpatch(struct midi_dev *md,
                  struct sysex_info *sysex, struct uio *uio)
{
	struct sequencer_softc *sc;
	u_char c, bf[128];
	int i, cc, error;

	if (sysex->key != SEQ_SYSEX_PATCH) {
		DPRINTFN(-1,("midiseq_loadpatch: bad patch key 0x%04x\n",
			     sysex->key));
		return (EINVAL);
	}
	if (uio->uio_resid < sysex->len)
		/* adjust length, should be an error */
		sysex->len = uio->uio_resid;

	DPRINTFN(2, ("midiseq_loadpatch: len=%d\n", sysex->len));
	if (sysex->len == 0)
		return EINVAL;
	error = uiomove(&c, 1, uio);
	if (error)
		return error;
	if (c != MIDI_SYSEX_START)		/* must start like this */
		return EINVAL;
	sc = md->seq;
	mutex_enter(&sc->lock);
	error = midiseq_out(md, &c, 1, 0);
	mutex_exit(&sc->lock);
	if (error)
		return error;
	--sysex->len;
	while (sysex->len > 0) {
		cc = sysex->len;
		if (cc > sizeof bf)
			cc = sizeof bf;
		error = uiomove(bf, cc, uio);
		if (error)
			break;
		for(i = 0; i < cc && !MIDI_IS_STATUS(bf[i]); i++)
			;
		/*
		 * XXX midi(4)'s buffer might not accommodate this, and the
		 * function will not block us (though in this case we have
		 * a process and could in principle block).
		 */
		mutex_enter(&sc->lock);
		error = midiseq_out(md, bf, i, 0);
		mutex_exit(&sc->lock);
		if (error)
			break;
		sysex->len -= i;
		if (i != cc)
			break;
	}
	/*
	 * Any leftover data in uio is rubbish;
	 * the SYSEX should be one write ending in SYSEX_END.
	 */
	uio->uio_resid = 0;
	c = MIDI_SYSEX_END;
	mutex_enter(&sc->lock);
	error = midiseq_out(md, &c, 1, 0);
	mutex_exit(&sc->lock);
	return error;
}

#include "midi.h"
#if NMIDI == 0
static dev_type_open(midiopen);
static dev_type_close(midiclose);

const struct cdevsw midi_cdevsw = {
	.d_open = midiopen,
	.d_close = midiclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER | D_MPSAFE
};

/*
 * If someone has a sequencer, but no midi devices there will
 * be unresolved references, so we provide little stubs.
 */

int
midi_unit_count(void)
{
	return (0);
}

static int
midiopen(dev_t dev, int flags, int ifmt, struct lwp *l)
{
	return (ENXIO);
}

struct cfdriver midi_cd;

void
midi_getinfo(dev_t dev, struct midi_info *mi)
{
        mi->name = "Dummy MIDI device";
	mi->props = 0;
}

static int
midiclose(dev_t dev, int flags, int ifmt, struct lwp *l)
{
	return (ENXIO);
}

int
midi_writebytes(int unit, u_char *bf, int cc)
{
	return (ENXIO);
}
#endif /* NMIDI == 0 */
