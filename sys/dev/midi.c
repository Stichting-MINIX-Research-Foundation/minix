/*	$NetBSD: midi.c,v 1.83 2014/12/30 07:28:34 mrg Exp $	*/

/*
 * Copyright (c) 1998, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@NetBSD.org), (MIDI FST and Active
 * Sense handling) Chapman Flack (chap@NetBSD.org), and Andrew Doran.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: midi.c,v 1.83 2014/12/30 07:28:34 mrg Exp $");

#include "midi.h"
#include "sequencer.h"

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/conf.h>
#include <sys/audioio.h>
#include <sys/midiio.h>
#include <sys/device.h>
#include <sys/intr.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/midivar.h>

#if NMIDI > 0

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (mididebug) printf x
#define DPRINTFN(n,x)	if (mididebug >= (n)) printf x
int	mididebug = 0;
/*
 *      1: detected protocol errors and buffer overflows
 *      2: probe, attach, detach
 *      3: open, close
 *      4: data received except realtime
 *      5: ioctl
 *      6: read, write, poll
 *      7: data transmitted
 *      8: uiomoves, synchronization
 *      9: realtime data received
 */
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

static	struct midi_softc *hwif_softc = NULL;
static	kmutex_t hwif_softc_lock;

static void	midi_in(void *, int);
static void	midi_out(void *);
static int	midi_poll_out(struct midi_softc *);
static int	midi_intr_out(struct midi_softc *);
static int 	midi_msg_out(struct midi_softc *, u_char **, u_char **,
			     u_char **, u_char **);
static int	midi_start_output(struct midi_softc *);
static void	midi_initbuf(struct midi_buffer *);
static void	midi_xmt_asense(void *);
static void	midi_rcv_asense(void *);
static void	midi_softint(void *);

static int	midiprobe(device_t, cfdata_t, void *);
static void	midiattach(device_t, device_t, void *);
int		mididetach(device_t, int);
static int	midiactivate(device_t, enum devact);

static dev_type_open(midiopen);
static dev_type_close(midiclose);
static dev_type_read(midiread);
static dev_type_write(midiwrite);
static dev_type_ioctl(midiioctl);
static dev_type_poll(midipoll);
static dev_type_kqfilter(midikqfilter);

const struct cdevsw midi_cdevsw = {
	.d_open = midiopen,
	.d_close = midiclose,
	.d_read = midiread,
	.d_write = midiwrite,
	.d_ioctl = midiioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = midipoll,
	.d_mmap = nommap,
	.d_kqfilter = midikqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER | D_MPSAFE
};

CFATTACH_DECL_NEW(midi, sizeof(struct midi_softc),
    midiprobe, midiattach, mididetach, midiactivate);

#define MIDI_XMT_ASENSE_PERIOD mstohz(275)
#define MIDI_RCV_ASENSE_PERIOD mstohz(300)

extern struct cfdriver midi_cd;

static int
midiprobe(device_t parent, cfdata_t match, void *aux)
{
	struct audio_attach_args *sa;

	sa = aux;

	DPRINTFN(2,("midiprobe: type=%d sa=%p hw=%p\n", sa->type, sa,
	    sa->hwif));

	return sa->type == AUDIODEV_TYPE_MIDI;
}

static void
midiattach(device_t parent, device_t self, void *aux)
{
	struct midi_softc *sc = device_private(self);
	struct audio_attach_args *sa = aux;
	const struct midi_hw_if *hwp;
	void *hdlp;

	hwp = sa->hwif;
	hdlp = sa->hdl;

	aprint_naive("\n");

	DPRINTFN(2, ("MIDI attach\n"));

#ifdef DIAGNOSTIC
	if (hwp == 0 ||
	    hwp->open == 0 ||
	    hwp->close == 0 ||
	    hwp->output == 0 ||
	    hwp->getinfo == 0) {
		printf("midi: missing method\n");
		return;
	}
#endif

	sc->dev = self;
	sc->hw_if = hwp;
	sc->hw_hdl = hdlp;
	midi_attach(sc);
}

static int
midiactivate(device_t self, enum devact act)
{
	struct midi_softc *sc = device_private(self);

	switch (act) {
	case DVACT_DEACTIVATE:
		mutex_enter(sc->lock);
		sc->dying = 1;
		mutex_exit(sc->lock);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

int
mididetach(device_t self, int flags)
{
	struct midi_softc *sc = device_private(self);
	int maj, mn;

	DPRINTFN(2,("%s: sc=%p flags=%d\n", __func__, sc, flags));

	pmf_device_deregister(self);

	mutex_enter(sc->lock);
	sc->dying = 1;

	if (--sc->refcnt >= 0) {
		/* Wake anything? */
		(void)cv_timedwait(&sc->detach_cv, sc->lock, hz * 60);
	}
	cv_broadcast(&sc->wchan);
	cv_broadcast(&sc->rchan);
	mutex_exit(sc->lock);

	/* locate the major number */
	maj = cdevsw_lookup_major(&midi_cdevsw);

	/*
	 * Nuke the vnodes for any open instances (calls close).
	 * Will wait until any activity on the device nodes has ceased.
	 *
	 * XXXAD NOT YET.
	 *
	 * XXXAD NEED TO PREVENT NEW REFERENCES THROUGH AUDIO_ENTER().
	 */
	mn = device_unit(self);
	vdevgone(maj, mn, mn, VCHR);
	
	if (!(sc->props & MIDI_PROP_NO_OUTPUT)) {
		evcnt_detach(&sc->xmt.bytesDiscarded);
		evcnt_detach(&sc->xmt.incompleteMessages);
	}
	if (sc->props & MIDI_PROP_CAN_INPUT) {
		evcnt_detach(&sc->rcv.bytesDiscarded);
		evcnt_detach(&sc->rcv.incompleteMessages);
	}

	if (sc->sih != NULL) {
		softint_disestablish(sc->sih);
		sc->sih = NULL;
	}

	mutex_enter(sc->lock);
	callout_halt(&sc->xmt_asense_co, sc->lock);
	callout_halt(&sc->rcv_asense_co, sc->lock);
	mutex_exit(sc->lock);

	callout_destroy(&sc->xmt_asense_co);
	callout_destroy(&sc->rcv_asense_co);

	cv_destroy(&sc->wchan);
	cv_destroy(&sc->rchan);
	cv_destroy(&sc->detach_cv);

	return (0);
}

void
midi_attach(struct midi_softc *sc)
{
	struct midi_info mi;
	kmutex_t *dummy;
	static int first = 1;

	if (first) {
		mutex_init(&hwif_softc_lock, MUTEX_DEFAULT, IPL_NONE);
		first = 0;
	}

	sc->hw_if->get_locks(sc->hw_hdl, &sc->lock, &dummy);

	callout_init(&sc->xmt_asense_co, CALLOUT_MPSAFE);
	callout_init(&sc->rcv_asense_co, CALLOUT_MPSAFE);
	callout_setfunc(&sc->xmt_asense_co, midi_xmt_asense, sc);
	callout_setfunc(&sc->rcv_asense_co, midi_rcv_asense, sc);

	sc->sih = softint_establish(SOFTINT_CLOCK | SOFTINT_MPSAFE,
	    midi_softint, sc);

	cv_init(&sc->rchan, "midird");
	cv_init(&sc->wchan, "midiwr");
	cv_init(&sc->detach_cv, "mididet");

	sc->dying = 0;
	sc->isopen = 0;
	sc->refcnt = 0;

	mutex_enter(&hwif_softc_lock);
	mutex_enter(sc->lock);
	hwif_softc = sc;
	sc->hw_if->getinfo(sc->hw_hdl, &mi);
	hwif_softc = NULL;
	mutex_exit(sc->lock);
	mutex_exit(&hwif_softc_lock);

	sc->props = mi.props;

	if (!(sc->props & MIDI_PROP_NO_OUTPUT)) {
		evcnt_attach_dynamic(&sc->xmt.bytesDiscarded,
			EVCNT_TYPE_MISC, NULL,
			device_xname(sc->dev), "xmt bytes discarded");
		evcnt_attach_dynamic(&sc->xmt.incompleteMessages,
			EVCNT_TYPE_MISC, NULL,
			device_xname(sc->dev), "xmt incomplete msgs");
	}
	if (sc->props & MIDI_PROP_CAN_INPUT) {
		evcnt_attach_dynamic(&sc->rcv.bytesDiscarded,
			EVCNT_TYPE_MISC, NULL,
			device_xname(sc->dev), "rcv bytes discarded");
		evcnt_attach_dynamic(&sc->rcv.incompleteMessages,
			EVCNT_TYPE_MISC, NULL,
			device_xname(sc->dev), "rcv incomplete msgs");
	}
	
	aprint_naive("\n");
	aprint_normal(": %s\n", mi.name);

	if (!pmf_device_register(sc->dev, NULL, NULL))
		aprint_error_dev(sc->dev, "couldn't establish power handler\n"); 
}

void
midi_register_hw_if_ext(struct midi_hw_if_ext *exthw)
{
	if (hwif_softc != NULL) /* ignore calls resulting from non-init */
		hwif_softc->hw_if_ext = exthw; /* uses of getinfo */
}

int
midi_unit_count(void)
{
	int i;
	for ( i = 0; i < midi_cd.cd_ndevs; ++i)
	        if (NULL == device_lookup(&midi_cd, i))
		        break;
        return i;
}

static void
midi_initbuf(struct midi_buffer *mb)
{
	mb->idx_producerp = mb->idx_consumerp = mb->idx;
	mb->buf_producerp = mb->buf_consumerp = mb->buf;
}

#define PACK_MB_IDX(cat,len) (((cat)<<4)|(len))
#define MB_IDX_CAT(idx) ((idx)>>4)
#define MB_IDX_LEN(idx) ((idx)&0xf)

static char const midi_cats[] = "\0\0\0\0\0\0\0\0\2\2\2\2\1\1\2\3";
#define MIDI_CAT(d) (midi_cats[((d)>>4)&15])
#define FST_RETURN(offp,endp,ret) \
	return (s->pos=s->msg+(offp)), (s->end=s->msg+(endp)), (ret)

enum fst_ret { FST_CHN, FST_CHV, FST_COM, FST_SYX, FST_RT, FST_MORE, FST_ERR,
               FST_HUH, FST_SXP };
enum fst_form { FST_CANON, FST_COMPR, FST_VCOMP };
static struct {
	int off;
	enum fst_ret tag;
} const midi_forms[] = {
	[FST_CANON] = { .off=0, .tag=FST_CHN },
	[FST_COMPR] = { .off=1, .tag=FST_CHN },
	[FST_VCOMP] = { .off=0, .tag=FST_CHV }
};
#define FST_CRETURN(endp) \
	FST_RETURN(midi_forms[form].off,endp,midi_forms[form].tag)

/*
 * A MIDI finite state transducer suitable for receiving or transmitting. It
 * will accept correct MIDI input that uses, doesn't use, or sometimes uses the
 * 'running status' compression technique, and transduce it to fully expanded
 * (form=FST_CANON) or fully compressed (form=FST_COMPR or FST_VCOMP) form.
 *
 * Returns FST_MORE if a complete message has not been parsed yet (SysEx
 * messages are the exception), FST_ERR or FST_HUH if the input does not
 * conform to the protocol, or FST_CHN (channel messages), FST_COM (System
 * Common messages), FST_RT (System Real-Time messages), or FST_SYX (System
 * Exclusive) to broadly categorize the message parsed. s->pos and s->end
 * locate the parsed message; while (s->pos<s->end) putchar(*(s->pos++));
 * would output it.
 *
 * FST_HUH means the character c wasn't valid in the original state, but the
 * state has now been reset to START and the caller should try again passing
 * the same c. FST_ERR means c isn't valid in the start state; the caller
 * should kiss it goodbye and continue to try successive characters from the
 * input until something other than FST_ERR or FST_HUH is returned, at which
 * point things are resynchronized.
 *
 * A FST_SYX return means that between pos and end are from 1 to 3
 * bytes of a system exclusive message. A SysEx message will be delivered in
 * one or more chunks of that form, where the first begins with 0xf0 and the
 * last (which is the only one that might have length < 3) ends with 0xf7.
 *
 * Messages corrupted by a protocol error are discarded and won't be seen at
 * all; again SysEx is the exception, as one or more chunks of it may already
 * have been parsed.
 *
 * For FST_CHN messages, s->msg[0] always contains the status byte even if
 * FST_COMPR form was requested (pos then points to msg[1]). That way, the
 * caller can always identify the exact message if there is a need to do so.
 * For all other message types except FST_SYX, the status byte is at *pos
 * (which may not necessarily be msg[0]!). There is only one SysEx status
 * byte, so the return value FST_SYX is sufficient to identify it.
 *
 * To simplify some use cases, compression can also be requested with
 * form=FST_VCOMP. In this form a compressible channel message is indicated
 * by returning a classification of FST_CHV instead of FST_CHN, and pos points
 * to the status byte rather than being advanced past it. If the caller in this
 * case saves the bytes from pos to end, it will have saved the entire message,
 * and can act on the FST_CHV tag to drop the first byte later. In this form,
 * unlike FST_CANON, hidden note-off (i.e. note-on with velocity 0) may occur.
 *
 * Two obscure points in the MIDI protocol complicate things further, both to
 * do with the EndSysEx code, 0xf7. First, this code is permitted (and
 * meaningless) outside of a System Exclusive message, anywhere a status byte
 * could appear. Second, it is allowed to be absent at the end of a System
 * Exclusive message (!) - any status byte at all (non-realtime) is allowed to
 * terminate the message. Both require accomodation in the interface to
 * midi_fst's caller. A stray 0xf7 should be ignored BUT should count as a
 * message received for purposes of Active Sense timeout; the case is
 * represented by a return of FST_COM with a length of zero (pos == end). A
 * status byte other than 0xf7 during a system exclusive message will cause an
 * FST_SXP (sysex plus) return; the bytes from pos to end are the end of the
 * system exclusive message, and after handling those the caller should call
 * midi_fst again with the same input byte.
 *
 * midi(4) will never produce either such form of rubbish.
 */
static enum fst_ret
midi_fst(struct midi_state *s, u_char c, enum fst_form form)
{
	int syxpos = 0;

	if (c >= 0xf8) { /* All realtime messages bypass state machine */
	        if (c == 0xf9 || c == 0xfd) {
			DPRINTF( ("midi_fst: s=%p c=0x%02x undefined\n", 
			    s, c));
			s->bytesDiscarded.ev_count++;
			return FST_ERR;
		}
		DPRINTFN(9, ("midi_fst: s=%p System Real-Time data=0x%02x\n", 
		    s, c));
		s->msg[2] = c;
		FST_RETURN(2,3,FST_RT);
	}

	DPRINTFN(4, ("midi_fst: s=%p data=0x%02x state=%d\n", 
	    s, c, s->state));

        switch (s->state | MIDI_CAT(c)) { /* break ==> return FST_MORE */
	case MIDI_IN_START  | MIDI_CAT_COMMON:
	case MIDI_IN_RUN1_1 | MIDI_CAT_COMMON:
	case MIDI_IN_RUN2_2 | MIDI_CAT_COMMON:
	case MIDI_IN_RXX2_2 | MIDI_CAT_COMMON:
	        s->msg[0] = c;
	        switch ( c) {
		case 0xf0: s->state = MIDI_IN_SYX1_3; break;
		case 0xf1: s->state = MIDI_IN_COM0_1; break;
		case 0xf2: s->state = MIDI_IN_COM0_2; break;
		case 0xf3: s->state = MIDI_IN_COM0_1; break;
		case 0xf6: s->state = MIDI_IN_START;  FST_RETURN(0,1,FST_COM);
		case 0xf7: s->state = MIDI_IN_START;  FST_RETURN(0,0,FST_COM);
		default: goto protocol_violation;
		}
		break;
	
	case MIDI_IN_RUN1_1 | MIDI_CAT_STATUS1:
		if (c == s->msg[0]) {
			s->state = MIDI_IN_RNX0_1;
			break;
		}
		/* FALLTHROUGH */
	case MIDI_IN_RUN2_2 | MIDI_CAT_STATUS1:
	case MIDI_IN_RXX2_2 | MIDI_CAT_STATUS1:
	case MIDI_IN_START  | MIDI_CAT_STATUS1:
	        s->state = MIDI_IN_RUN0_1;
	        s->msg[0] = c;
		break;
	
	case MIDI_IN_RUN2_2 | MIDI_CAT_STATUS2:
	case MIDI_IN_RXX2_2 | MIDI_CAT_STATUS2:
		if (c == s->msg[0]) {
			s->state = MIDI_IN_RNX0_2;
			break;
		}
		if ((c ^ s->msg[0]) == 0x10 && (c & 0xe0) == 0x80) {
			s->state = MIDI_IN_RXX0_2;
			s->msg[0] = c;
			break;
		}
		/* FALLTHROUGH */
	case MIDI_IN_RUN1_1 | MIDI_CAT_STATUS2:
	case MIDI_IN_START  | MIDI_CAT_STATUS2:
	        s->state = MIDI_IN_RUN0_2;
	        s->msg[0] = c;
		break;

        case MIDI_IN_COM0_1 | MIDI_CAT_DATA:
		s->state = MIDI_IN_START;
	        s->msg[1] = c;
		FST_RETURN(0,2,FST_COM);

        case MIDI_IN_COM0_2 | MIDI_CAT_DATA:
	        s->state = MIDI_IN_COM1_2;
	        s->msg[1] = c;
		break;

        case MIDI_IN_COM1_2 | MIDI_CAT_DATA:
		s->state = MIDI_IN_START;
	        s->msg[2] = c;
		FST_RETURN(0,3,FST_COM);

        case MIDI_IN_RUN0_1 | MIDI_CAT_DATA:
		s->state = MIDI_IN_RUN1_1;
	        s->msg[1] = c;
		FST_RETURN(0,2,FST_CHN);

        case MIDI_IN_RUN1_1 | MIDI_CAT_DATA:
        case MIDI_IN_RNX0_1 | MIDI_CAT_DATA:
		s->state = MIDI_IN_RUN1_1;
	        s->msg[1] = c;
		FST_CRETURN(2);

        case MIDI_IN_RUN0_2 | MIDI_CAT_DATA:
	        s->state = MIDI_IN_RUN1_2;
	        s->msg[1] = c;
		break;

        case MIDI_IN_RUN1_2 | MIDI_CAT_DATA:
		if (FST_CANON == form && 0 == c && (s->msg[0]&0xf0) == 0x90) {
			s->state = MIDI_IN_RXX2_2;
			s->msg[0] ^= 0x10;
			s->msg[2] = 64;
		} else {
			s->state = MIDI_IN_RUN2_2;
	        	s->msg[2] = c;
		}
		FST_RETURN(0,3,FST_CHN);

        case MIDI_IN_RUN2_2 | MIDI_CAT_DATA:
	        s->state = MIDI_IN_RNX1_2;
	        s->msg[1] = c;
		break;

        case MIDI_IN_RXX2_2 | MIDI_CAT_DATA:
	        s->state = MIDI_IN_RXX1_2;
		s->msg[0] ^= 0x10;
	        s->msg[1] = c;
		break;

        case MIDI_IN_RNX0_2 | MIDI_CAT_DATA:
	        s->state = MIDI_IN_RNY1_2;
	        s->msg[1] = c;
		break;

        case MIDI_IN_RXX0_2 | MIDI_CAT_DATA:
	        s->state = MIDI_IN_RXY1_2;
	        s->msg[1] = c;
		break;

        case MIDI_IN_RNX1_2 | MIDI_CAT_DATA:
        case MIDI_IN_RNY1_2 | MIDI_CAT_DATA:
		if (FST_CANON == form && 0 == c && (s->msg[0]&0xf0) == 0x90) {
			s->state = MIDI_IN_RXX2_2;
			s->msg[0] ^= 0x10;
			s->msg[2] = 64;
			FST_RETURN(0,3,FST_CHN);
		}
		s->state = MIDI_IN_RUN2_2;
	        s->msg[2] = c;
		FST_CRETURN(3);

        case MIDI_IN_RXX1_2 | MIDI_CAT_DATA:
        case MIDI_IN_RXY1_2 | MIDI_CAT_DATA:
		if (( 0 == c && (s->msg[0]&0xf0) == 0x90)
		  || (64 == c && (s->msg[0]&0xf0) == 0x80
		      && FST_CANON != form)) {
			s->state = MIDI_IN_RXX2_2;
			s->msg[0] ^= 0x10;
			s->msg[2] = 64 - c;
			FST_CRETURN(3);
		}
		s->state = MIDI_IN_RUN2_2;
	        s->msg[2] = c;
		FST_RETURN(0,3,FST_CHN);

        case MIDI_IN_SYX1_3 | MIDI_CAT_DATA:
		s->state = MIDI_IN_SYX2_3;
	        s->msg[1] = c;
		break;

        case MIDI_IN_SYX2_3 | MIDI_CAT_DATA:
		s->state = MIDI_IN_SYX0_3;
	        s->msg[2] = c;
		FST_RETURN(0,3,FST_SYX);

        case MIDI_IN_SYX0_3 | MIDI_CAT_DATA:
		s->state = MIDI_IN_SYX1_3;
	        s->msg[0] = c;
		break;

        case MIDI_IN_SYX2_3 | MIDI_CAT_COMMON:
        case MIDI_IN_SYX2_3 | MIDI_CAT_STATUS1:
        case MIDI_IN_SYX2_3 | MIDI_CAT_STATUS2:
		++ syxpos;
		/* FALLTHROUGH */
        case MIDI_IN_SYX1_3 | MIDI_CAT_COMMON:
        case MIDI_IN_SYX1_3 | MIDI_CAT_STATUS1:
        case MIDI_IN_SYX1_3 | MIDI_CAT_STATUS2:
		++ syxpos;
		/* FALLTHROUGH */
        case MIDI_IN_SYX0_3 | MIDI_CAT_COMMON:
        case MIDI_IN_SYX0_3 | MIDI_CAT_STATUS1:
        case MIDI_IN_SYX0_3 | MIDI_CAT_STATUS2:
		s->state = MIDI_IN_START;
	        if (c == 0xf7) {
			s->msg[syxpos] = c;
		        FST_RETURN(0,1+syxpos,FST_SYX);
		}
		s->msg[syxpos] = 0xf7;
		FST_RETURN(0,1+syxpos,FST_SXP);

        default:
protocol_violation:
                DPRINTF(("midi_fst: unexpected %#02x in state %u\n",
		    c, s->state));
		switch ( s->state) {
		case MIDI_IN_RUN1_1: /* can only get here by seeing an */
		case MIDI_IN_RUN2_2: /* INVALID System Common message */
		case MIDI_IN_RXX2_2:
		        s->state = MIDI_IN_START;
			/* FALLTHROUGH */
		case MIDI_IN_START:
			s->bytesDiscarded.ev_count++;
			return FST_ERR;
		case MIDI_IN_COM1_2:
		case MIDI_IN_RUN1_2:
		case MIDI_IN_RNY1_2:
		case MIDI_IN_RXY1_2:
			s->bytesDiscarded.ev_count++;
			/* FALLTHROUGH */
		case MIDI_IN_COM0_1:
		case MIDI_IN_RUN0_1:
		case MIDI_IN_RNX0_1:
		case MIDI_IN_COM0_2:
		case MIDI_IN_RUN0_2:
		case MIDI_IN_RNX0_2:
		case MIDI_IN_RXX0_2:
		case MIDI_IN_RNX1_2:
		case MIDI_IN_RXX1_2:
			s->bytesDiscarded.ev_count++;
		        s->incompleteMessages.ev_count++;
			break;
		default:
		        DPRINTF(("midi_fst: mishandled %#02x(%u) in state %u?!\n",
			    c, MIDI_CAT(c), s->state));
			break;
		}
		s->state = MIDI_IN_START;
		return FST_HUH;
	}
	return FST_MORE;
}

static void
midi_softint(void *cookie)
{
	struct midi_softc *sc;
	proc_t *p;
	pid_t pid;

	sc = cookie;

	mutex_enter(proc_lock);
	pid = sc->async;
	if (pid != 0 && (p = proc_find(pid)) != NULL)
		psignal(p, SIGIO);
	mutex_exit(proc_lock);
}

static void
midi_in(void *addr, int data)
{
	struct midi_softc *sc;
	struct midi_buffer *mb;
	int i, count;
	enum fst_ret got;
	MIDI_BUF_DECLARE(idx);
	MIDI_BUF_DECLARE(buf);

	sc = addr;
	mb = &sc->inbuf;

	KASSERT(mutex_owned(sc->lock));

	if (!sc->isopen)
		return;

	if ((sc->flags & FREAD) == 0)
		return;		/* discard data if not reading */
	
sxp_again:
	do {
		got = midi_fst(&sc->rcv, data, FST_CANON);
	} while (got == FST_HUH);
	
	switch (got) {
	case FST_MORE:
	case FST_ERR:
		return;
	case FST_CHN:
	case FST_COM:
	case FST_RT:
#if NSEQUENCER > 0
		if (sc->seqopen) {
			extern void midiseq_in(struct midi_dev *,u_char *,int);
			count = sc->rcv.end - sc->rcv.pos;
			midiseq_in(sc->seq_md, sc->rcv.pos, count);
			return;
		}
#endif
        	/*
		 * Pass Active Sense to the sequencer if it's open, but not to
		 * a raw reader. (Really should do something intelligent with
		 * it then, though....)
		 */
		if (got == FST_RT && MIDI_ACK == sc->rcv.pos[0]) {
			if (!sc->rcv_expect_asense) {
				sc->rcv_expect_asense = 1;
				callout_schedule(&sc->rcv_asense_co,
				    MIDI_RCV_ASENSE_PERIOD);
			}
			sc->rcv_quiescent = 0;
			sc->rcv_eof = 0;
			return;
		}
		/* FALLTHROUGH */
	/*
	 * Ultimately SysEx msgs should be offered to the sequencer also; the
	 * sequencer API addresses them - but maybe our sequencer can't handle
	 * them yet, so offer only to raw reader. (Which means, ultimately,
	 * discard them if the sequencer's open, as it's not doing reads!)
	 * -> When SysEx support is added to the sequencer, be sure to handle
	 *    FST_SXP there too.
	 */
	case FST_SYX:
	case FST_SXP:
		count = sc->rcv.end - sc->rcv.pos;
		sc->rcv_quiescent = 0;
		sc->rcv_eof = 0;
		if (0 == count)
			break;
		MIDI_BUF_PRODUCER_INIT(mb,idx);
		MIDI_BUF_PRODUCER_INIT(mb,buf);
		if (count > buf_lim - buf_cur
		     || 1 > idx_lim - idx_cur) {
			sc->rcv.bytesDiscarded.ev_count += count;
			DPRINTF(("midi_in: buffer full, discard data=0x%02x\n", 
				 sc->rcv.pos[0]));
			return;
		}
		for (i = 0; i < count; i++) {
			*buf_cur++ = sc->rcv.pos[i];
			MIDI_BUF_WRAP(buf);
		}
		*idx_cur++ = PACK_MB_IDX(got,count);
		MIDI_BUF_WRAP(idx);
		MIDI_BUF_PRODUCER_WBACK(mb,buf);
		MIDI_BUF_PRODUCER_WBACK(mb,idx);
		cv_broadcast(&sc->rchan);
		selnotify(&sc->rsel, 0, NOTE_SUBMIT);
		if (sc->async != 0)
			softint_schedule(sc->sih);
		break;
	default: /* don't #ifdef this away, gcc will say FST_HUH not handled */
		printf("midi_in: midi_fst returned %d?!\n", got);
	}
	if (FST_SXP == got)
		goto sxp_again;
}

static void
midi_out(void *addr)
{
	struct midi_softc *sc = addr;

	KASSERT(mutex_owned(sc->lock));

	if (!sc->isopen)
		return;
	DPRINTFN(8, ("midi_out: %p\n", sc));
	midi_intr_out(sc);
}

static int
midiopen(dev_t dev, int flags, int ifmt, struct lwp *l)
{
	struct midi_softc *sc;
	const struct midi_hw_if *hw;
	int error;

	sc = device_lookup_private(&midi_cd, MIDIUNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	DPRINTFN(3,("midiopen %p\n", sc));

	mutex_enter(sc->lock);
	if (sc->dying) {
		mutex_exit(sc->lock);
		return (EIO);
	}
	hw = sc->hw_if;
	if (hw == NULL) {
		mutex_exit(sc->lock);
		return ENXIO;
	}
	if (sc->isopen) {
		mutex_exit(sc->lock);
		return EBUSY;
	}

	/* put both state machines into known states */
	sc->rcv.state = MIDI_IN_START;
	sc->rcv.pos = sc->rcv.msg;
	sc->rcv.end = sc->rcv.msg;
	sc->xmt.state = MIDI_IN_START;
	sc->xmt.pos = sc->xmt.msg;
	sc->xmt.end = sc->xmt.msg;
	
	/* copy error counters so an ioctl (TBA) can give since-open stats */
	sc->rcv.atOpen.bytesDiscarded  = sc->rcv.bytesDiscarded.ev_count;
	sc->rcv.atQuery.bytesDiscarded = sc->rcv.bytesDiscarded.ev_count;
	
	sc->xmt.atOpen.bytesDiscarded  = sc->xmt.bytesDiscarded.ev_count;
	sc->xmt.atQuery.bytesDiscarded = sc->xmt.bytesDiscarded.ev_count;
	
	/* and the buffers */
	midi_initbuf(&sc->outbuf);
	midi_initbuf(&sc->inbuf);
	
	/* and the receive flags */
	sc->rcv_expect_asense = 0;
	sc->rcv_quiescent = 0;
	sc->rcv_eof = 0;
	sc->isopen++;
	sc->flags = flags;
	sc->pbus = 0;
	sc->async = 0;

#ifdef MIDI_SAVE
	if (midicnt != 0) {
		midisave.cnt = midicnt;
		midicnt = 0;
	}
#endif

	error = hw->open(sc->hw_hdl, flags, midi_in, midi_out, sc);
	if (error) {
		mutex_exit(sc->lock);
		return error;
	}

	mutex_exit(sc->lock);
	return 0;
}

static int
midiclose(dev_t dev, int flags, int ifmt, struct lwp *l)
{
	struct midi_softc *sc;
	const struct midi_hw_if *hw;

	sc = device_lookup_private(&midi_cd, MIDIUNIT(dev));
	hw = sc->hw_if;

	DPRINTFN(3,("midiclose %p\n", sc));

	mutex_enter(sc->lock);
	/* midi_start_output(sc); anything buffered => pbus already set! */
	while (sc->pbus) {
		if (sc->dying)
			break;
		DPRINTFN(8,("midiclose sleep ...\n"));
		cv_wait(&sc->wchan, sc->lock);
	}
	sc->isopen = 0;
	callout_halt(&sc->xmt_asense_co, sc->lock);
	callout_halt(&sc->rcv_asense_co, sc->lock);
	hw->close(sc->hw_hdl);
	sc->seqopen = 0;
	sc->seq_md = 0;
	mutex_exit(sc->lock);

	return 0;
}

static int
midiread(dev_t dev, struct uio *uio, int ioflag)
{
	struct midi_softc *sc;
	struct midi_buffer *mb;
	int appetite, error, first;
	MIDI_BUF_DECLARE(idx);
	MIDI_BUF_DECLARE(buf);

	sc = device_lookup_private(&midi_cd, MIDIUNIT(dev));
	mb = &sc->inbuf;
	first = 1;

	DPRINTFN(6,("midiread: %p, count=%lu\n", sc,
	    (unsigned long)uio->uio_resid));

	mutex_enter(sc->lock);
	if (sc->dying) {
		mutex_exit(sc->lock);
		return EIO;
	}
	if ((sc->props & MIDI_PROP_CAN_INPUT) == 0) {
		mutex_exit(sc->lock);
		return ENXIO;
	}
	MIDI_BUF_CONSUMER_INIT(mb,idx);
	MIDI_BUF_CONSUMER_INIT(mb,buf);
	error = 0;
	for (;;) {
		/*
		 * If the used portion of idx wraps around the end, just take
		 * the first part on this iteration, and we'll get the rest on
		 * the next.
		 */
		if (idx_lim > idx_end)
			idx_lim = idx_end;
		/*
		 * Count bytes through the last complete message that will
		 * fit in the requested read.
		 */
		for (appetite = uio->uio_resid; idx_cur < idx_lim; ++idx_cur) {
			if (appetite < MB_IDX_LEN(*idx_cur))
				break;
			appetite -= MB_IDX_LEN(*idx_cur);
		}
		appetite = uio->uio_resid - appetite;

		/*
		 * Only if the read is too small to hold even the first
		 * complete message will we return a partial one (updating idx
		 * to reflect the remaining length of the message).
		 */
		if (appetite == 0 && idx_cur < idx_lim) {
			if (!first)
				break;
			appetite = uio->uio_resid;
			*idx_cur = PACK_MB_IDX(MB_IDX_CAT(*idx_cur),
			    MB_IDX_LEN(*idx_cur) - appetite);
		}
		KASSERT(buf_cur + appetite <= buf_lim);
		
		/* move the bytes */
		if (appetite > 0) {		
			first = 0;  /* we know we won't return empty-handed */
			/* do two uiomoves if data wrap around end of buf */
			if (buf_cur + appetite > buf_end) {
				DPRINTFN(8,
					("midiread: uiomove cc=%td (prewrap)\n",
					buf_end - buf_cur));
				mutex_exit(sc->lock);
				error = uiomove(buf_cur, buf_end - buf_cur, uio);
				mutex_enter(sc->lock);
				if (error)
					break;
				if (sc->dying) {
					error = EIO;
					break;
				}
				appetite -= buf_end - buf_cur;
				buf_cur = mb->buf;
			}
			DPRINTFN(8, ("midiread: uiomove cc=%d\n", appetite));
			mutex_exit(sc->lock);
			error = uiomove(buf_cur, appetite, uio);
			mutex_enter(sc->lock);
			if (error)
				break;
			if (sc->dying) {
				error = EIO;
				break;
			}
			buf_cur += appetite;
		}
		
		MIDI_BUF_WRAP(idx);
		MIDI_BUF_WRAP(buf);
		MIDI_BUF_CONSUMER_WBACK(mb,idx);
		MIDI_BUF_CONSUMER_WBACK(mb,buf);
		if (0 == uio->uio_resid) /* if read satisfied, we're done */
			break;
		MIDI_BUF_CONSUMER_REFRESH(mb,idx);
		if (idx_cur == idx_lim) { /* need to wait for data? */
			if (!first || sc->rcv_eof) /* never block reader if */
				break;            /* any data already in hand */
			if (ioflag & IO_NDELAY) {
				error = EWOULDBLOCK;
				break;
			}
			error = cv_wait_sig(&sc->rchan, sc->lock);
			if (error)
				break;
			MIDI_BUF_CONSUMER_REFRESH(mb,idx); /* what'd we get? */
		}
		MIDI_BUF_CONSUMER_REFRESH(mb,buf);
		if (sc->dying) {
			error = EIO;
			break;
		}
	}
	mutex_exit(sc->lock);

	return error;
}

static void
midi_rcv_asense(void *arg)
{
	struct midi_softc *sc;

	sc = arg;

	mutex_enter(sc->lock);
	if (sc->dying || !sc->isopen) {
		mutex_exit(sc->lock);
		return;
	}
	if (sc->rcv_quiescent) {
		sc->rcv_eof = 1;
		sc->rcv_quiescent = 0;
		sc->rcv_expect_asense = 0;
		cv_broadcast(&sc->rchan);
		selnotify(&sc->rsel, 0, NOTE_SUBMIT);
		if (sc->async)
			softint_schedule(sc->sih);
		mutex_exit(sc->lock);
		return;
	}	
	sc->rcv_quiescent = 1;
	callout_schedule(&sc->rcv_asense_co, MIDI_RCV_ASENSE_PERIOD);
	mutex_exit(sc->lock);
}

static void
midi_xmt_asense(void *arg)
{
	struct midi_softc *sc;
	int error, armed;

	sc = arg;
	
	mutex_enter(sc->lock);
	if (sc->pbus || sc->dying || !sc->isopen) {
		mutex_exit(sc->lock);
		return;
	}
	sc->pbus = 1;
	if (sc->props & MIDI_PROP_OUT_INTR) {
		error = sc->hw_if->output(sc->hw_hdl, MIDI_ACK);
		armed = (error == 0);
	} else {
		error = sc->hw_if->output(sc->hw_hdl, MIDI_ACK);
		armed = 0;
	}
	if (!armed) {
		sc->pbus = 0;
		callout_schedule(&sc->xmt_asense_co, MIDI_XMT_ASENSE_PERIOD);
	}
	mutex_exit(sc->lock);
}

/*
 * The way this function was hacked up to plug into poll_out and intr_out
 * after they were written won't win it any beauty contests, but it'll work
 * (code in haste, refactor at leisure).
 */
static int
midi_msg_out(struct midi_softc *sc, u_char **idx, u_char **idxl, u_char **buf,
	     u_char **bufl)
{
	MIDI_BUF_DECLARE(idx);
	MIDI_BUF_DECLARE(buf);
	MIDI_BUF_EXTENT_INIT(&sc->outbuf,idx);
	MIDI_BUF_EXTENT_INIT(&sc->outbuf,buf);
	int length;
	int error;
	u_char contig[3];
	u_char *cp;
	u_char *ep;

	KASSERT(mutex_owned(sc->lock));

	idx_cur = *idx;
	idx_lim = *idxl;
	buf_cur = *buf;
	buf_lim = *bufl;
	
	length = MB_IDX_LEN(*idx_cur);
	
	for ( cp = contig, ep = cp + length; cp < ep;) {
		*cp++ = *buf_cur++;
		MIDI_BUF_WRAP(buf);
	}
	cp = contig;

	switch ( MB_IDX_CAT(*idx_cur)) {
	case FST_CHV: /* chnmsg to be compressed (for device that wants it) */
		++ cp;
		-- length;
		/* FALLTHROUGH */
	case FST_CHN:
		error = sc->hw_if_ext->channel(sc->hw_hdl,
		    MIDI_GET_STATUS(contig[0]), MIDI_GET_CHAN(contig[0]),
		    cp, length);
		break;
	case FST_COM:
		error = sc->hw_if_ext->common(sc->hw_hdl,
		    MIDI_GET_STATUS(contig[0]), cp, length);
		break;
	case FST_SYX:
	case FST_SXP:
		error = sc->hw_if_ext->sysex(sc->hw_hdl, cp, length);
		break;
	case FST_RT:
		error = sc->hw_if->output(sc->hw_hdl, *cp);
		break;
	default:
		error = EIO;
	}
	
	if (!error) {
		++ idx_cur;
		MIDI_BUF_WRAP(idx);
		*idx  = idx_cur;
		*idxl = idx_lim;
		*buf  = buf_cur;
		*bufl = buf_lim;
	}
	
	return error;
}

/*
 * midi_poll_out is intended for the midi hw (the vast majority of MIDI UARTs
 * on sound cards, apparently) that _do not have transmit-ready interrupts_.
 * Every call to hw_if->output for one of these may busy-wait to output the
 * byte; at the standard midi data rate that'll be 320us per byte. The
 * technique of writing only MIDI_MAX_WRITE bytes in a row and then waiting
 * for MIDI_WAIT does not reduce the total time spent busy-waiting, and it
 * adds arbitrary delays in transmission (and, since MIDI_WAIT is roughly the
 * same as the time to send MIDI_MAX_WRITE bytes, it effectively halves the
 * data rate). Here, a somewhat bolder approach is taken. Since midi traffic
 * is bursty but time-sensitive--most of the time there will be none at all,
 * but when there is it should go out ASAP--the strategy is to just get it
 * over with, and empty the buffer in one go. The effect this can have on
 * the rest of the system will be limited by the size of the buffer and the
 * sparseness of the traffic. But some precautions are in order. Interrupts
 * should all be unmasked when this is called, and midiwrite should not fill
 * the buffer more than once (when MIDI_PROP_CAN_INTR is false) without a
 * yield() so some other process can get scheduled. If the write is nonblocking,
 * midiwrite should return a short count rather than yield.
 *
 * Someday when there is fine-grained MP support, this should be reworked to
 * run in a callout so the writing process really could proceed concurrently.
 * But obviously where performance is a concern, interrupt-driven hardware
 * such as USB midi or (apparently) clcs will always be preferable. And it
 * seems (kern/32651) that many of the devices currently working in poll mode
 * may really have tx interrupt capability and want only implementation; that
 * ought to happen.
 */
static int
midi_poll_out(struct midi_softc *sc)
{
	struct midi_buffer *mb = &sc->outbuf;
	int error;
	int msglen;
	MIDI_BUF_DECLARE(idx);
	MIDI_BUF_DECLARE(buf);

	KASSERT(mutex_owned(sc->lock));

	error = 0;
	MIDI_BUF_CONSUMER_INIT(mb,idx);
	MIDI_BUF_CONSUMER_INIT(mb,buf);

	for (;;) {
		while (idx_cur != idx_lim) {
			if (sc->hw_if_ext) {
				error = midi_msg_out(sc, &idx_cur, &idx_lim,
				    &buf_cur, &buf_lim);
				if (error != 0) {
					break;
				}
				continue;
			}
			/* or, lacking hw_if_ext ... */
			msglen = MB_IDX_LEN(*idx_cur);
			DPRINTFN(7,("midi_poll_out: %p <- %#02x\n",
			    sc->hw_hdl, *buf_cur));
			error = sc->hw_if->output(sc->hw_hdl, *buf_cur);
			if (error) {
				break;
			}
			buf_cur++;
			MIDI_BUF_WRAP(buf);
			msglen--;
			if (msglen) {
				*idx_cur = PACK_MB_IDX(MB_IDX_CAT(*idx_cur),
				    msglen);
			} else {
				idx_cur++;
				MIDI_BUF_WRAP(idx);
			}
		}
		if (error != 0) {
			break;
		}
		KASSERT(buf_cur == buf_lim);
		MIDI_BUF_CONSUMER_WBACK(mb,idx);
		MIDI_BUF_CONSUMER_WBACK(mb,buf);
		MIDI_BUF_CONSUMER_REFRESH(mb,idx); /* any more to transmit? */
		MIDI_BUF_CONSUMER_REFRESH(mb,buf);
		if (idx_lim == idx_cur)
			break;
	}

	if (error != 0) {
		DPRINTF(("midi_poll_output error %d\n", error));
		MIDI_BUF_CONSUMER_WBACK(mb,idx);
		MIDI_BUF_CONSUMER_WBACK(mb,buf);
	}
	sc->pbus = 0;
	callout_schedule(&sc->xmt_asense_co, MIDI_XMT_ASENSE_PERIOD);
	return error;
}

/*
 * The interrupt flavor acquires spl and lock once and releases at the end,
 * as it expects to write only one byte or message. The interface convention
 * is that if hw_if->output returns 0, it has initiated transmission and the
 * completion interrupt WILL be forthcoming; if it has not returned 0, NO
 * interrupt will be forthcoming, and if it returns EINPROGRESS it wants
 * another byte right away.
 */
static int
midi_intr_out(struct midi_softc *sc)
{
	struct midi_buffer *mb;
	int error, msglen;
	MIDI_BUF_DECLARE(idx);
	MIDI_BUF_DECLARE(buf);
	int armed = 0;

	KASSERT(mutex_owned(sc->lock));

	error = 0;
	mb = &sc->outbuf;

	MIDI_BUF_CONSUMER_INIT(mb,idx);
	MIDI_BUF_CONSUMER_INIT(mb,buf);
	
	while (idx_cur != idx_lim) {
		if (sc->hw_if_ext) {
			error = midi_msg_out(sc, &idx_cur, &idx_lim,
			    &buf_cur, &buf_lim);
			if (!error ) /* no EINPROGRESS from extended hw_if */
				armed = 1;
			break;
		}
		/* or, lacking hw_if_ext ... */
		msglen = MB_IDX_LEN(*idx_cur);
		error = sc->hw_if->output(sc->hw_hdl, *buf_cur);
		if (error &&  error != EINPROGRESS)
			break;
		++buf_cur;
		MIDI_BUF_WRAP(buf);
		--msglen;
		if (msglen)
			*idx_cur = PACK_MB_IDX(MB_IDX_CAT(*idx_cur),msglen);
		else {
			++idx_cur;
			MIDI_BUF_WRAP(idx);
		}
		if (!error) {
			armed = 1;
			break;
		}
	}
	MIDI_BUF_CONSUMER_WBACK(mb,idx);
	MIDI_BUF_CONSUMER_WBACK(mb,buf);
	if (!armed) {
		sc->pbus = 0;
		callout_schedule(&sc->xmt_asense_co, MIDI_XMT_ASENSE_PERIOD);
	}
	cv_broadcast(&sc->wchan);
	selnotify(&sc->wsel, 0, NOTE_SUBMIT);
	if (sc->async) {
		softint_schedule(sc->sih);
	}
	if (error) {
		DPRINTF(("midi_intr_output error %d\n", error));
	}
	return error;
}

static int
midi_start_output(struct midi_softc *sc)
{

	KASSERT(mutex_owned(sc->lock));

	if (sc->dying)
		return EIO;
	if (sc->props & MIDI_PROP_OUT_INTR)
		return midi_intr_out(sc);
	return midi_poll_out(sc);
}

static int
real_writebytes(struct midi_softc *sc, u_char *ibuf, int cc)
{
	u_char *iend;
	struct midi_buffer *mb;
	int arming, count, got;
	enum fst_form form;
	MIDI_BUF_DECLARE(idx);
	MIDI_BUF_DECLARE(buf);
	int error;

	KASSERT(mutex_owned(sc->lock));

	if (sc->dying || !sc->isopen)
		return EIO;

	sc->refcnt++;

	iend = ibuf + cc;
	mb = &sc->outbuf;
	arming = 0;
	
	/*
	 * If the hardware uses the extended hw_if, pass it canonicalized
	 * messages (or compressed ones if it specifically requests, using
	 * VCOMP form so the bottom half can still pass the op and chan along);
	 * if it does not, send it compressed messages (using COMPR form as
	 * there is no need to preserve the status for the bottom half).
	 */
	if (NULL == sc->hw_if_ext)
		form = FST_COMPR;
	else if (sc->hw_if_ext->compress)
		form = FST_VCOMP;
	else
		form = FST_CANON;

	MIDI_BUF_PRODUCER_INIT(mb,idx);
	MIDI_BUF_PRODUCER_INIT(mb,buf);
	
	while (ibuf < iend) {
		got = midi_fst(&sc->xmt, *ibuf, form);
		++ibuf;
		switch ( got) {
		case FST_MORE:
			continue;
		case FST_ERR:
		case FST_HUH:
			error = EPROTO;
			goto out;
		case FST_CHN:
		case FST_CHV: /* only occurs in VCOMP form */
		case FST_COM:
		case FST_RT:
		case FST_SYX:
		case FST_SXP:
			break; /* go add to buffer */
#if defined(AUDIO_DEBUG) || defined(DIAGNOSTIC)
		default:
			printf("midi_wr: midi_fst returned %d?!\n", got);
#endif
		}
		count = sc->xmt.end - sc->xmt.pos;
		if (0 == count ) /* can happen with stray 0xf7; see midi_fst */
			continue;
		/*
		 * return EWOULDBLOCK if the data passed will not fit in
		 * the buffer; the caller should have taken steps to avoid that.
		 * If got==FST_SXP we lose the new status byte, but we're losing
		 * anyway, so c'est la vie.
		 */
		if (idx_cur == idx_lim || count > buf_lim - buf_cur) {
			MIDI_BUF_PRODUCER_REFRESH(mb,idx); /* get the most */
			MIDI_BUF_PRODUCER_REFRESH(mb,buf); /*  current facts */
			if (idx_cur == idx_lim || count > buf_lim - buf_cur) {
				error = EWOULDBLOCK; /* caller's problem */
				goto out;
			}
		}
		*idx_cur++ = PACK_MB_IDX(got,count);
		MIDI_BUF_WRAP(idx);
		while (count) {
			*buf_cur++ = *(sc->xmt.pos)++;
			MIDI_BUF_WRAP(buf);
			-- count;
		}
		if (FST_SXP == got)
			-- ibuf; /* again with same status byte */
	}
	MIDI_BUF_PRODUCER_WBACK(mb,buf);
	MIDI_BUF_PRODUCER_WBACK(mb,idx);
	/*
	 * If the output transfer is not already busy, and there is a message
	 * buffered, mark it busy, stop the Active Sense callout (what if we're
	 * too late and it's expired already? No big deal, an extra Active Sense
	 * never hurt anybody) and start the output transfer once we're out of
	 * the critical section (pbus==1 will stop anyone else doing the same).
	 */
	MIDI_BUF_CONSUMER_INIT(mb,idx); /* check what consumer's got to read */
	if (!sc->pbus && idx_cur < idx_lim) {
		sc->pbus = 1;
		callout_stop(&sc->xmt_asense_co);
		arming = 1;
	}

	error = arming ? midi_start_output(sc) : 0;

out:
	if (--sc->refcnt < 0)
		cv_broadcast(&sc->detach_cv);

	return error;
}

static int
midiwrite(dev_t dev, struct uio *uio, int ioflag)
{
	struct midi_softc *sc;
	struct midi_buffer *mb;
	int error;
	u_char inp[256];
	MIDI_BUF_DECLARE(idx);
	MIDI_BUF_DECLARE(buf);
	size_t idxspace;
	size_t bufspace;
	size_t xfrcount;
	int pollout = 0;

	(void)buf_end; (void)idx_end;
	sc = device_lookup_private(&midi_cd, MIDIUNIT(dev));

	DPRINTFN(6,("midiwrite: %p, unit=%d, count=%lu\n", sc, (int)minor(dev),
	    (unsigned long)uio->uio_resid));

	mutex_enter(sc->lock);
	if (sc->dying) {
		mutex_exit(sc->lock);
		return EIO;
	}

	sc->refcnt++;

	mb = &sc->outbuf;
	error = 0;
	while (uio->uio_resid > 0 && !error) {
		/*
		 * block if necessary for the minimum buffer space to guarantee
		 * we can write something.
		 */
		MIDI_BUF_PRODUCER_INIT(mb,idx); /* init can't go above loop; */
		MIDI_BUF_PRODUCER_INIT(mb,buf); /* real_writebytes moves cur */
		for (;;) {
			idxspace = MIDI_BUF_PRODUCER_REFRESH(mb,idx) - idx_cur;
			bufspace = MIDI_BUF_PRODUCER_REFRESH(mb,buf) - buf_cur;
			if (idxspace >= 1 && bufspace >= 3 && !pollout)
				break;
			DPRINTFN(8,("midi_write: sleep idx=%zd buf=%zd\n", 
				 idxspace, bufspace));
			if (ioflag & IO_NDELAY) {
				/*
				 * If some amount has already been transferred,
				 * the common syscall code will automagically
				 * convert this to success with a short count.
				 */
				error = EWOULDBLOCK;
				goto out;
			}
			if (pollout) {
				mutex_exit(sc->lock);
				yield(); /* see midi_poll_output */
				mutex_enter(sc->lock);
				pollout = 0;
			} else
				error = cv_wait_sig(&sc->wchan, sc->lock);
			if (sc->dying)
				error = EIO;
			if (error) {
				/*
				 * Similarly, the common code will handle
				 * EINTR and ERESTART properly here, changing to
				 * a short count if something transferred.
				 */
				goto out;
			}
		}

		/*
		 * The number of bytes we can safely extract from the uio
		 * depends on the available idx and buf space. Worst case,
		 * every byte is a message so 1 idx is required per byte.
		 * Worst case, the first byte completes a 3-byte msg in prior
		 * state, and every subsequent byte is a Program Change or
		 * Channel Pressure msg with running status and expands to 2
		 * bytes, so the buf space reqd is 3+2(n-1) or 2n+1. So limit
		 * the transfer to the min of idxspace and (bufspace-1)>>1.
		 */
		xfrcount = (bufspace - 1) >> 1;
		if (xfrcount > idxspace)
			xfrcount = idxspace;
		if (xfrcount > sizeof inp)
			xfrcount = sizeof inp;
		if (xfrcount > uio->uio_resid)
			xfrcount = uio->uio_resid;

		mutex_exit(sc->lock);
		error = uiomove(inp, xfrcount, uio);
		mutex_enter(sc->lock);
#ifdef MIDI_DEBUG
		if (error)
		        printf("midi_write:(1) uiomove failed %d; "
			       "xfrcount=%zu inp=%p\n",
			       error, xfrcount, inp);
#endif
		if (error)
			break;
		
		/*
		 * The number of bytes we extracted being calculated to
		 * definitely fit in the buffer even with canonicalization,
		 * there is no excuse for real_writebytes to return EWOULDBLOCK.
		 */
		error = real_writebytes(sc, inp, xfrcount);
		KASSERT(error != EWOULDBLOCK);
		if (error)
			break;

		/*
		 * If this is a polling device and we just sent a buffer, let's
		 * not send another without giving some other process a chance.
		 */
		if ((sc->props & MIDI_PROP_OUT_INTR) == 0)
			pollout = 1;
		DPRINTFN(8,("midiwrite: uio_resid now %zu, props=%d\n",
		    uio->uio_resid, sc->props));
	}

out:
	if (--sc->refcnt < 0)
		cv_broadcast(&sc->detach_cv);

	mutex_exit(sc->lock);
	return error;
}

/*
 * This write routine is only called from sequencer code and expects
 * a write that is smaller than the MIDI buffer.
 */
int
midi_writebytes(int unit, u_char *bf, int cc)
{
	struct midi_softc *sc =
	    device_lookup_private(&midi_cd, unit);
	int error;

	if (!sc)
		return EIO;

	DPRINTFN(7, ("midi_writebytes: %p, unit=%d, cc=%d %#02x %#02x %#02x\n",
                    sc, unit, cc, bf[0], bf[1], bf[2]));

	mutex_enter(sc->lock);
	if (sc->dying)
		error = EIO;
	else
		error = real_writebytes(sc, bf, cc);
	mutex_exit(sc->lock);

	return error;
}

static int
midiioctl(dev_t dev, u_long cmd, void *addr, int flag, struct lwp *l)
{
	struct midi_softc *sc;
	const struct midi_hw_if *hw;
	int error;
	MIDI_BUF_DECLARE(buf);

	(void)buf_end;
	sc = device_lookup_private(&midi_cd, MIDIUNIT(dev));

	mutex_enter(sc->lock);
	if (sc->dying) {
		mutex_exit(sc->lock);
		return EIO;
	}
	hw = sc->hw_if;
	error = 0;

	sc->refcnt++;

	DPRINTFN(5,("midiioctl: %p cmd=0x%08lx\n", sc, cmd));

	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper layer. */
		break;
	
	case FIONREAD:
		/*
		 * This code relies on the current implementation of midi_in
		 * always updating buf and idx together in a critical section,
		 * so buf always ends at a message boundary. Document this
		 * ioctl as always returning a value such that the last message
		 * included is complete (SysEx the only exception), and then
		 * make sure the implementation doesn't regress.  NB that
		 * means if this ioctl returns n and the proc then issues a
		 * read of n, n bytes will be read, but if the proc issues a
		 * read of m < n, fewer than m bytes may be read to ensure the
		 * read ends at a message boundary.
		 */
		MIDI_BUF_CONSUMER_INIT(&sc->inbuf,buf);
		*(int *)addr = buf_lim - buf_cur;
		break;

	case FIOASYNC:
		mutex_exit(sc->lock);
		mutex_enter(proc_lock);
		if (*(int *)addr) {
			if (sc->async) {
				error = EBUSY;
			} else {
				sc->async = curproc->p_pid;
			}
			DPRINTFN(5,("midi_ioctl: FIOASYNC %d\n",
			    curproc->p_pid));
		} else {
			sc->async = 0;
		}
		mutex_exit(proc_lock);
		mutex_enter(sc->lock);
		break;

#if 0
	case MIDI_PRETIME:
		/* XXX OSS
		 * This should set up a read timeout, but that's
		 * why we have poll(), so there's nothing yet. */
		error = EINVAL;
		break;
#endif

#ifdef MIDI_SAVE
	case MIDI_GETSAVE:
		mutex_exit(sc->lock);
		error = copyout(&midisave, *(void **)addr, sizeof midisave);
		mutex_enter(sc->lock);
  		break;
#endif

	default:
		if (hw->ioctl != NULL) {
			error = hw->ioctl(sc->hw_hdl, cmd, addr, flag, l);
		} else {
			error = EINVAL;
		}
		break;
	}

	if (--sc->refcnt < 0)
		cv_broadcast(&sc->detach_cv);
	mutex_exit(sc->lock);
	return error;
}

static int
midipoll(dev_t dev, int events, struct lwp *l)
{
	struct midi_softc *sc;
	int revents;
	MIDI_BUF_DECLARE(idx);
	MIDI_BUF_DECLARE(buf);

	(void)buf_end; (void)idx_end;
	sc = device_lookup_private(&midi_cd, MIDIUNIT(dev));
	revents = 0;

	DPRINTFN(6,("midipoll: %p events=0x%x\n", sc, events));

	mutex_enter(sc->lock);
	if (sc->dying) {
		mutex_exit(sc->lock);
		return POLLHUP;
	}

	sc->refcnt++;

	if ((events & (POLLIN | POLLRDNORM)) != 0) {
		MIDI_BUF_CONSUMER_INIT(&sc->inbuf, idx);
		if (idx_cur < idx_lim)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(l, &sc->rsel);
	}
	if ((events & (POLLOUT | POLLWRNORM)) != 0) {
		MIDI_BUF_PRODUCER_INIT(&sc->outbuf, idx);
		MIDI_BUF_PRODUCER_INIT(&sc->outbuf, buf);
		if (idx_lim - idx_cur >= 1 && buf_lim - buf_cur >= 3)
			revents |= events & (POLLOUT | POLLWRNORM);
		else
			selrecord(l, &sc->wsel);
	}

	if (--sc->refcnt < 0)
		cv_broadcast(&sc->detach_cv);

	mutex_exit(sc->lock);

	return revents;
}

static void
filt_midirdetach(struct knote *kn)
{
	struct midi_softc *sc = kn->kn_hook;

	mutex_enter(sc->lock);
	SLIST_REMOVE(&sc->rsel.sel_klist, kn, knote, kn_selnext);
	mutex_exit(sc->lock);
}

static int
filt_midiread(struct knote *kn, long hint)
{
	struct midi_softc *sc = kn->kn_hook;
	MIDI_BUF_DECLARE(buf);

	(void)buf_end;
	if (hint != NOTE_SUBMIT)
		mutex_enter(sc->lock);
	MIDI_BUF_CONSUMER_INIT(&sc->inbuf,buf);
	kn->kn_data = buf_lim - buf_cur;
	if (hint != NOTE_SUBMIT)
		mutex_exit(sc->lock);
	return (kn->kn_data > 0);
}

static const struct filterops midiread_filtops =
	{ 1, NULL, filt_midirdetach, filt_midiread };

static void
filt_midiwdetach(struct knote *kn)
{
	struct midi_softc *sc = kn->kn_hook;

	mutex_enter(sc->lock);
	SLIST_REMOVE(&sc->wsel.sel_klist, kn, knote, kn_selnext);
	mutex_exit(sc->lock);
}

static int
filt_midiwrite(struct knote *kn, long hint)
{
	struct midi_softc *sc = kn->kn_hook;
	MIDI_BUF_DECLARE(idx);
	MIDI_BUF_DECLARE(buf);

	mutex_exit(sc->lock);
	sc->refcnt++;
	mutex_enter(sc->lock);

	(void)idx_end; (void)buf_end;
	if (hint != NOTE_SUBMIT)
		mutex_enter(sc->lock);
	MIDI_BUF_PRODUCER_INIT(&sc->outbuf,idx);
	MIDI_BUF_PRODUCER_INIT(&sc->outbuf,buf);
	kn->kn_data = ((buf_lim - buf_cur)-1)>>1;
	if (kn->kn_data > idx_lim - idx_cur)
		kn->kn_data = idx_lim - idx_cur;
	if (hint != NOTE_SUBMIT)
		mutex_exit(sc->lock);

	// XXXMRG -- move this up, avoid the relock?
	mutex_enter(sc->lock);
	if (--sc->refcnt < 0)
		cv_broadcast(&sc->detach_cv);
	mutex_exit(sc->lock);

	return (kn->kn_data > 0);
}

static const struct filterops midiwrite_filtops =
	{ 1, NULL, filt_midiwdetach, filt_midiwrite };

int
midikqfilter(dev_t dev, struct knote *kn)
{
	struct midi_softc *sc =
	    device_lookup_private(&midi_cd, MIDIUNIT(dev));
	struct klist *klist;

	mutex_exit(sc->lock);
	sc->refcnt++;
	mutex_enter(sc->lock);

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &sc->rsel.sel_klist;
		kn->kn_fop = &midiread_filtops;
		break;

	case EVFILT_WRITE:
		klist = &sc->wsel.sel_klist;
		kn->kn_fop = &midiwrite_filtops;
		break;

	default:
		return (EINVAL);
	}

	kn->kn_hook = sc;

	mutex_enter(sc->lock);
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	if (--sc->refcnt < 0)
		cv_broadcast(&sc->detach_cv);
	mutex_exit(sc->lock);

	return (0);
}

void
midi_getinfo(dev_t dev, struct midi_info *mi)
{
	struct midi_softc *sc;

	sc = device_lookup_private(&midi_cd, MIDIUNIT(dev));
	if (sc == NULL)
		return;
	mutex_enter(sc->lock);
	sc->hw_if->getinfo(sc->hw_hdl, mi);
	mutex_exit(sc->lock);
}

#elif NMIDIBUS > 0 /* but NMIDI == 0 */

void
midi_register_hw_if_ext(struct midi_hw_if_ext *exthw)
{

	/* nothing */
}

#endif /* NMIDI > 0 */

#if NMIDI > 0 || NMIDIBUS > 0

device_t
midi_attach_mi(const struct midi_hw_if *mhwp, void *hdlp, device_t dev)
{
	struct audio_attach_args arg;

	if (mhwp == NULL) {
		panic("midi_attach_mi: NULL\n");
		return (0);
	}

	arg.type = AUDIODEV_TYPE_MIDI;
	arg.hwif = mhwp;
	arg.hdl = hdlp;
	return (config_found(dev, &arg, audioprint));
}

#endif /* NMIDI > 0 || NMIDIBUS > 0 */
