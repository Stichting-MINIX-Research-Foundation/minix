/*	$NetBSD: midisynvar.h,v 1.14 2012/04/09 10:18:16 plunky Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@NetBSD.org).
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

#ifndef _SYS_DEV_MIDISYNVAR_H_
#define _SYS_DEV_MIDISYNVAR_H_

#include "midictl.h"

typedef struct midisyn midisyn;

/*
 * Important: any synth driver that uses midisyn must set up its methods
 * structure in a way that refers to members /by name/ and zeroes the rest
 * (as is the effect of C99 initializers with designators). This discipline
 * will allow the structure to evolve methods for newly implemented
 * functionality or to exploit capabilities of new drivers with a minimal
 * versioning burden.  Because midisyn is at present a very rudimentary and
 * partial implementation, change should be expected in this set of methods.
 * Do not hesitate to add one in the course of implementing new stuff, as
 * long as it will be generally useful and there is some reasonable fallback
 * for drivers without it.
 */
struct midisyn_methods {
	int  (*open)	(midisyn *, int /* flags */);
	void (*close)   (midisyn *);
	int  (*ioctl)   (midisyn *, u_long, void *, int, struct lwp *);
	/*
	 * allocv(midisyn *ms, uint_fast8_t chan, uint_fast8_t key);
	 * Allocate one of the devices actual voices (stealing one if
	 * necessary) to play note number 'key' (the MIDI note number, not
	 * a frequency) associated with MIDI channel chan. An implementation
	 * might want to choose a voice already last used on chan, to save
	 * shuffling of patches.
	 * One day a variant of this method will probably be needed, with an
	 * extra argument indicating whether a melodic or percussive voice is
	 * wanted.
	 */
	uint_fast16_t  (*allocv)  (midisyn *, uint_fast8_t, uint_fast8_t);
	/*
	 * attackv(midisyn *ms,
	 *         uint_fast16_t voice, midipitch_t mp, int16_t level_cB);
	 * Attack the voice 'voice' at pitch 'midipitch' with level 'level_cB'.
	 * The pitch is in MIDI Tuning units and accounts for all of the pitch
	 * adjustment controls that midisyn supports and that the driver has
	 * not reported handling internally. The level is in centibels of
	 * attenuation (0 == no attenuation, full output; reduced levels are
	 * negative) and again accounts for all level adjustments midisyn
	 * supports, except any the driver reports handling itself.
	 * The program used for the voice should be the current program of the
	 * voice's associated MIDI channel, and can be queried with MS_GETPGM.
	 */
	void (*attackv)  (midisyn *, uint_fast16_t, midipitch_t, int16_t);
	/*
	 * attackv_vel(midisyn *ms, uint_fast16_t voice,
	 *             midipitch_t mp, int16_t level_cB, uint_fast8_t vel);
	 * If the driver can do something useful with the voice's attack
	 * velocity, such as vary the attack envelope or timbre, it should
	 * provide this method. Velocity 64 represents the normal attack for
	 * the patch, lower values are softer, higher harder. IF the driver
	 * does not supply this method, midisyn will call the attackv method
	 * instead, and include the velocity in the calculation of level_cB.
	 */
	void (*attackv_vel) (midisyn *,
	                     uint_fast16_t, midipitch_t, int16_t, uint_fast8_t);
	/*
	 * releasev(midisyn *ms, uint_fast16_t voice, uint_fast8_t vel);
	 * Release the voice 'voice' with release velocity 'vel' where lower
	 * values mean slower decay, 64 refers to the normal decay envelope
	 * for the patch, and higher values mean decay faster.
	 */
	void (*releasev) (midisyn *, uint_fast16_t, uint_fast8_t);
	/*
	 * repitchv(midisyn *ms, uint_fast16_t voice, midipitch_t mp);
	 * A driver should provide this method if it is able to change the
	 * pitch of a sounding voice without rearticulating or glitching it.
	 * [not yet implemented in midisyn]
	 */
	void (*repitchv) (midisyn *, uint_fast16_t, midipitch_t);
	/*
	 * relevelv(midisyn *ms, uint_fast16_t voice, int16_t level_cB);
	 * A driver should provide this method if it is able to change the
	 * level of a sounding voice without rearticulating or glitching it.
	 * How the driver should adjust 'level_cB' to account for envelope
	 * decay since the initial level is not (quite, yet) specified; the
	 * driver may save the last level it got from midisyn, subtract from
	 * this one, and use the difference to adjust the current level out of
	 * the envelope generator. Or not.... [not yet implemented in midisyn]
	 */
	void (*relevelv) (midisyn *, uint_fast16_t, int16_t);
	/*
	 * pgmchg(midisyn *ms, uint_fast8_t chan, uint_fast8_t pgm);
	 * If the driver supports changing programs, it should do whatever it
	 * does AND be sure to store pgm in ms->pgms[chan].  (XXX?)
	 */
	void (*pgmchg)  (midisyn *, uint_fast8_t, uint_fast8_t);
	/*
	 * ctlnotice(midisyn *ms,
	 *           midictl_evt evt, uint_fast8_t chan, uint_fast16_t key);
	 * Reports any of the events on channel 'chan' that midictl (which see)
	 * manages. Return 0 for a particular event if it is not handled by the
	 * driver, nonzero if the driver handles it. Many events can be handled
	 * by midisyn if the driver ignores them, but will be left to the driver
	 * if it returns non-zero. For example, midisyn will usually combine
	 * the volume and expression controllers, plus a note's attack velocity,
	 * into a single 'level' parameter to attackv, but if the driver is
	 * responding to the volume controller on its own, midisyn will leave
	 * that controller out of the combined level computation.
	 *   The driver should respond to MIDICTL_RESET events specially. There
	 * are some items of state other than controllers that are specified to
	 * be reset; if the driver keeps such state, it should be reset. The
	 * driver should also midictl_read() all controllers it can respond to,
	 * which will ensure that midictl tracks them.
	 *   There are also things that RP-015 specifies do NOT get reset by
	 * the MIDICTL_RESET event. Don't reset those. :)
	 */
	int (*ctlnotice) (midisyn *, midictl_evt, uint_fast8_t, uint_fast16_t);
};

struct voice {
	u_int chan_note;	/* channel and note */
#define MS_CHANNOTE(chan, note) ((chan) * 256 + (note))
#define MS_GETCHAN(v) ((v)->chan_note >> 8)
	u_int seqno;		/* allocation index (increases with time) */
	int16_t velcB;		/* cB based on attack vel (relevel may need) */
	u_char inuse;
};

#define MIDI_MAX_CHANS 16

struct channelstate;

struct midisyn {
	/* Filled by synth driver */
	struct midisyn_methods *mets;
	char name[32];
	int nvoice;
	int flags;
	void *data;
	kmutex_t *lock;

	/* Set up by midisyn but available to synth driver for reading ctls */
	/*
	 * Note - there is currently no locking on this structure; if the synth
	 * driver interacts with midictl it should do so synchronously, when
	 * processing a call from midisyn, and not at other times such as upon
	 * an interrupt. (may revisit locking if problems crop up.)
	 */
	midictl ctl;
	
	/* Used by midisyn driver */
	struct voice *voices;
	u_int seqno;
	u_int16_t pgms[MIDI_MAX_CHANS]; /* ref'd if driver uses MS_GETPGM */
	struct channelstate *chnstate;
};

#define MS_GETPGM(ms, vno) ((ms)->pgms[MS_GETCHAN(&(ms)->voices[vno])])

extern const struct midi_hw_if midisyn_hw_if;

void	midisyn_init(midisyn *);

/*
 * Convert a 14-bit volume or expression controller value to centibels using
 * the General MIDI formula. The maximum controller value translates to 0 cB
 * (no attenuation), a half-range controller to -119 cB (level cut by 11.9 dB)
 * and a zero controller to INT16_MIN. If you are converting a 7-bit value
 * just shift it 7 bits left first.
 */
extern int16_t midisyn_vol2cB(uint_fast16_t);

#endif /* _SYS_DEV_MIDISYNVAR_H_ */
