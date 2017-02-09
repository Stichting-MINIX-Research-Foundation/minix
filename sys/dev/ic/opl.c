/*	$NetBSD: opl.c,v 1.40 2013/06/28 14:48:17 christos Exp $	*/

/*
 * Copyright (c) 1998, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@NetBSD.org), and by Andrew Doran.
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
 * The OPL3 (YMF262) manual can be found at
 * ftp://ftp.yamahayst.com/Fax_Back_Doc/sound/YMF262.PDF
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: opl.c,v 1.40 2013/06/28 14:48:17 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/select.h>
#include <sys/kmem.h>

#include <sys/cpu.h>
#include <sys/bus.h>

#include <sys/audioio.h>
#include <sys/midiio.h>
#include <dev/audio_if.h>

#include <dev/midi_if.h>
#include <dev/midivar.h>
#include <dev/midisynvar.h>

#include <dev/ic/oplreg.h>
#include <dev/ic/oplvar.h>

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (opldebug) printf x
#define DPRINTFN(n,x)	if (opldebug >= (n)) printf x
int	opldebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct real_voice {
	u_int8_t voice_num;
	u_int8_t voice_mode; /* 0=unavailable, 2=2 OP, 4=4 OP */
	u_int8_t iooffs; /* I/O port (left or right side) */
	u_int8_t op[4]; /* Operator offsets */
};

const struct opl_voice voicetab[] = {
/*       No    I/O offs	OP1	OP2	OP3   OP4	*/
/*	---------------------------------------------	*/
	{ 0,   OPL_L,	{0x00,	0x03,	0x08, 0x0b}, NULL, 0, },
	{ 1,   OPL_L,	{0x01,	0x04,	0x09, 0x0c}, NULL, 0, },
	{ 2,   OPL_L,	{0x02,	0x05,	0x0a, 0x0d}, NULL, 0, },

	{ 3,   OPL_L,	{0x08,	0x0b,	0x00, 0x00}, NULL, 0, },
	{ 4,   OPL_L,	{0x09,	0x0c,	0x00, 0x00}, NULL, 0, },
	{ 5,   OPL_L,	{0x0a,	0x0d,	0x00, 0x00}, NULL, 0, },

	{ 6,   OPL_L,	{0x10,	0x13,	0x00, 0x00}, NULL, 0, },
	{ 7,   OPL_L,	{0x11,	0x14,	0x00, 0x00}, NULL, 0, },
	{ 8,   OPL_L,	{0x12,	0x15,	0x00, 0x00}, NULL, 0, },

	{ 0,   OPL_R,	{0x00,	0x03,	0x08, 0x0b}, NULL, 0, },
	{ 1,   OPL_R,	{0x01,	0x04,	0x09, 0x0c}, NULL, 0, },
	{ 2,   OPL_R,	{0x02,	0x05,	0x0a, 0x0d}, NULL, 0, },
	{ 3,   OPL_R,	{0x08,	0x0b,	0x00, 0x00}, NULL, 0, },
	{ 4,   OPL_R,	{0x09,	0x0c,	0x00, 0x00}, NULL, 0, },
	{ 5,   OPL_R,	{0x0a,	0x0d,	0x00, 0x00}, NULL, 0, },

	{ 6,   OPL_R,	{0x10,	0x13,	0x00, 0x00}, NULL, 0, },
	{ 7,   OPL_R,	{0x11,	0x14,	0x00, 0x00}, NULL, 0, },
	{ 8,   OPL_R,	{0x12,	0x15,	0x00, 0x00}, NULL, 0, }
};

static void opl_command(struct opl_softc *, int, int, int);
void opl_reset(struct opl_softc *);
void opl_freq_to_fnum (int freq, int *block, int *fnum);

int oplsyn_open(midisyn *ms, int);
void oplsyn_close(midisyn *);
void oplsyn_reset(void *);
void oplsyn_attackv(midisyn *, uint_fast16_t, midipitch_t, int16_t);
static void oplsyn_repitchv(midisyn *, uint_fast16_t, midipitch_t);
static void oplsyn_relevelv(midisyn *, uint_fast16_t, int16_t);
static void oplsyn_setv(midisyn *, uint_fast16_t, midipitch_t, int16_t, int);
void oplsyn_releasev(midisyn *, uint_fast16_t, uint_fast8_t);
int oplsyn_ctlnotice(midisyn *, midictl_evt, uint_fast8_t, uint_fast16_t);
void oplsyn_programchange(midisyn *, uint_fast8_t, uint_fast8_t);
void oplsyn_loadpatch(midisyn *, struct sysex_info *, struct uio *);
static void oplsyn_panhandler(midisyn *, uint_fast8_t);

void opl_set_op_reg(struct opl_softc *, int, int, int, u_char);
void opl_set_ch_reg(struct opl_softc *, int, int, u_char);
void opl_load_patch(struct opl_softc *, int);
u_int32_t opl_get_block_fnum(midipitch_t mp);
int opl_calc_vol(int regbyte, int16_t level_cB);

struct midisyn_methods opl3_midi = {
	.open	   = oplsyn_open,
	.close	   = oplsyn_close,
	.attackv   = oplsyn_attackv,
	.repitchv  = oplsyn_repitchv,
	.relevelv  = oplsyn_relevelv,
	.releasev  = oplsyn_releasev,
	.pgmchg    = oplsyn_programchange,
	.ctlnotice = oplsyn_ctlnotice,
};

void
opl_attach(struct opl_softc *sc)
{
	int i;

	KASSERT(sc->dev != NULL);
	KASSERT(sc->lock != NULL);

	mutex_enter(sc->lock);
	i = opl_find(sc);
	mutex_exit(sc->lock);
	if (i == 0) {
		printf("\nopl: find failed\n");
		return;
	}

	mutex_enter(sc->lock);
	opl_reset(sc);
	mutex_exit(sc->lock);

	sc->syn.mets = &opl3_midi;
	size_t len = strlen(sc->syn.name);
	snprintf(sc->syn.name + len, sizeof(sc->syn.name) - len, "Yamaha OPL%d",
	    sc->model);
	sc->syn.data = sc;
	sc->syn.nvoice = sc->model == OPL_2 ? OPL2_NVOICE : OPL3_NVOICE;
	sc->syn.lock = sc->lock;
	midisyn_init(&sc->syn);

	/* Set up voice table */
	for (i = 0; i < OPL3_NVOICE; i++)
		sc->voices[i] = voicetab[i];

	aprint_normal(": model OPL%d", sc->model);

	/* Set up panpot */
	sc->panl = OPL_VOICE_TO_LEFT;
	sc->panr = OPL_VOICE_TO_RIGHT;
	if (sc->model == OPL_3 &&
	    device_cfdata(sc->dev)->cf_flags & OPL_FLAGS_SWAP_LR) {
		sc->panl = OPL_VOICE_TO_RIGHT;
		sc->panr = OPL_VOICE_TO_LEFT;
		aprint_normal(": LR swapped");
	}

	aprint_normal("\n");
	aprint_naive("\n");

	sc->sc_mididev =
	    midi_attach_mi(&midisyn_hw_if, &sc->syn, sc->dev);
}

int
opl_detach(struct opl_softc *sc, int flags)
{
	int rv = 0;

	if (sc->sc_mididev != NULL)
		rv = config_detach(sc->sc_mididev, flags);

	return(rv);
}

static void
opl_command(struct opl_softc *sc, int offs, int addr, int data)
{
	DPRINTFN(4, ("opl_command: sc=%p, offs=%d addr=0x%02x data=0x%02x\n",
		     sc, offs, addr, data));

	KASSERT(!sc->lock || mutex_owned(sc->lock));

	offs += sc->offs;
	bus_space_write_1(sc->iot, sc->ioh, OPL_ADDR+offs, addr);
	if (sc->model == OPL_2)
		delay(10);
	else
		delay(6);
	bus_space_write_1(sc->iot, sc->ioh, OPL_DATA+offs, data);
	if (sc->model == OPL_2)
		delay(30);
	else
		delay(6);
}

int
opl_match(bus_space_tag_t iot, bus_space_handle_t ioh, int offs)
{
	struct opl_softc *sc;
	int rv;

	sc = kmem_zalloc(sizeof(*sc), KM_SLEEP);
	sc->iot = iot;
	sc->ioh = ioh;
	sc->offs = offs;
	rv = opl_find(sc);
	kmem_free(sc, sizeof(*sc));
	return rv;
}

int
opl_find(struct opl_softc *sc)
{
	u_int8_t status1, status2;

	DPRINTFN(2,("opl_find: ioh=0x%x\n", (int)sc->ioh));
	sc->model = OPL_2;	/* worst case assumption */

	/* Reset timers 1 and 2 */
	opl_command(sc, OPL_L, OPL_TIMER_CONTROL,
		    OPL_TIMER1_MASK | OPL_TIMER2_MASK);
	/* Reset the IRQ of the FM chip */
	opl_command(sc, OPL_L, OPL_TIMER_CONTROL, OPL_IRQ_RESET);

	/* get status bits */
	status1 = bus_space_read_1(sc->iot,sc->ioh,OPL_STATUS+OPL_L+sc->offs);

	opl_command(sc, OPL_L, OPL_TIMER1, -2); /* wait 2 ticks */
	opl_command(sc, OPL_L, OPL_TIMER_CONTROL, /* start timer1 */
		    OPL_TIMER1_START | OPL_TIMER2_MASK);
	delay(1000);		/* wait for timer to expire */

	/* get status bits again */
	status2 = bus_space_read_1(sc->iot,sc->ioh,OPL_STATUS+OPL_L+sc->offs);

	opl_command(sc, OPL_L, OPL_TIMER_CONTROL,
		    OPL_TIMER1_MASK | OPL_TIMER2_MASK);
	opl_command(sc, OPL_L, OPL_TIMER_CONTROL, OPL_IRQ_RESET);

	DPRINTFN(2,("opl_find: %02x %02x\n", status1, status2));

	if ((status1 & OPL_STATUS_MASK) != 0 ||
	    (status2 & OPL_STATUS_MASK) != (OPL_STATUS_IRQ | OPL_STATUS_FT1))
		return (0);

	switch(status1) {
	case 0x00:
	case 0x0f:
		sc->model = OPL_3;
		break;
	case 0x06:
		sc->model = OPL_2;
		break;
	default:
		return (0);
	}

	DPRINTFN(2,("opl_find: OPL%d at 0x%x detected\n",
		    sc->model, (int)sc->ioh));
	return (1);
}

/*
 * idea: opl_command does a lot of busywaiting, and the driver typically sets
 *       a lot of registers each time a voice-attack happens. some kind of
 *       caching to remember what was last written to each register could save
 *       a lot of cpu. It would have to be smart enough not to interfere with
 *       any necessary sequences of register access expected by the hardware...
 */
void
opl_set_op_reg(struct opl_softc *sc, int base, int voice, int op, u_char value)
{
	struct opl_voice *v = &sc->voices[voice];

	KASSERT(mutex_owned(sc->lock));

	opl_command(sc, v->iooffs, base + v->op[op], value);
}

void
opl_set_ch_reg(struct opl_softc *sc, int base, int voice, u_char value)
{
	struct opl_voice *v = &sc->voices[voice];

	KASSERT(mutex_owned(sc->lock));

	opl_command(sc, v->iooffs, base + v->voiceno, value);
}


void
opl_load_patch(struct opl_softc *sc, int v)
{
	const struct opl_operators *p = sc->voices[v].patch;

	KASSERT(mutex_owned(sc->lock));

	opl_set_op_reg(sc, OPL_AM_VIB,          v, 0, p->ops[OO_CHARS+0]);
	opl_set_op_reg(sc, OPL_AM_VIB,          v, 1, p->ops[OO_CHARS+1]);
	opl_set_op_reg(sc, OPL_KSL_LEVEL,       v, 0, p->ops[OO_KSL_LEV+0]);
	opl_set_op_reg(sc, OPL_KSL_LEVEL,       v, 1, p->ops[OO_KSL_LEV+1]);
	opl_set_op_reg(sc, OPL_ATTACK_DECAY,    v, 0, p->ops[OO_ATT_DEC+0]);
	opl_set_op_reg(sc, OPL_ATTACK_DECAY,    v, 1, p->ops[OO_ATT_DEC+1]);
	opl_set_op_reg(sc, OPL_SUSTAIN_RELEASE, v, 0, p->ops[OO_SUS_REL+0]);
	opl_set_op_reg(sc, OPL_SUSTAIN_RELEASE, v, 1, p->ops[OO_SUS_REL+1]);
	opl_set_op_reg(sc, OPL_WAVE_SELECT,     v, 0, p->ops[OO_WAV_SEL+0]);
	opl_set_op_reg(sc, OPL_WAVE_SELECT,     v, 1, p->ops[OO_WAV_SEL+1]);
	opl_set_ch_reg(sc, OPL_FEEDBACK_CONNECTION, v, p->ops[OO_FB_CONN]);
}

uint32_t
opl_get_block_fnum(midipitch_t mp)
{
	midihz18_t hz18;
	uint32_t block;
	uint32_t f_num;
	
	/*
	 * We can get to about note 30 before needing to switch from block 0.
	 * Thereafter, switch block every octave; that will keep f_num in the
	 * upper end of its range, making the most bits available for
	 * resolution.
	 */
	block = ( mp - MIDIPITCH_FROM_KEY(19) ) / MIDIPITCH_OCTAVE;
	if ( block > 7 )	/* subtract wrapped */
		block = 0;
	/*
	 * Could subtract block*MIDIPITCH_OCTAVE here, or >>block later. Later.
	 */
	
	hz18 = MIDIPITCH_TO_HZ18(mp);
	hz18 >>= block;
	
	/*
	 * The formula in the manual is f_num = ((hz<<19)/fs)>>(block-1) (though
	 * block==0 implies >>-1 which is a C unspecified result). As we already
	 * have hz<<18 and I omitted the -1 when shifting above, what's left to
	 * do now is multiply by 4 and divide by fs, the sampling frequency of
	 * the chip. fs is the master clock frequency fM / 288, fM is 14.32 MHz
	 * so fs is a goofy number around 49.7kHz. The 5th convergent of the
	 * continued fraction matches 4/fs to 9+ significant figures. Doing the
	 * shift first (above) ensures there's room in hz18 to multiply by 9.
	 */
	
	f_num = (9 * hz18) / 111875; 
	return ((block << 10) | f_num);
}


void
opl_reset(struct opl_softc *sc)
{
	int i;

	KASSERT(mutex_owned(sc->lock));

	for (i = 1; i <= OPL_MAXREG; i++)
		opl_command(sc, OPL_L, OPL_KEYON_BLOCK + i, 0);

	opl_command(sc, OPL_L, OPL_TEST, OPL_ENABLE_WAVE_SELECT);
	opl_command(sc, OPL_L, OPL_PERCUSSION, 0);
	if (sc->model == OPL_3) {
		opl_command(sc, OPL_R, OPL_MODE, OPL3_ENABLE);
		opl_command(sc, OPL_R,OPL_CONNECTION_SELECT,OPL_NOCONNECTION);
	}

	for (i = 0; i < MIDI_MAX_CHANS; i++)
		sc->pan[i] = OPL_VOICE_TO_LEFT | OPL_VOICE_TO_RIGHT;
}

int
oplsyn_open(midisyn *ms, int flags)
{
	struct opl_softc *sc = ms->data;

	KASSERT(mutex_owned(sc->lock));

	DPRINTFN(2, ("oplsyn_open: %d\n", flags));

#ifndef AUDIO_NO_POWER_CTL
	if (sc->powerctl)
		sc->powerctl(sc->powerarg, 1);
#endif
	opl_reset(ms->data);
	if (sc->spkrctl)
		sc->spkrctl(sc->spkrarg, 1);
	return (0);
}

void
oplsyn_close(midisyn *ms)
{
	struct opl_softc *sc = ms->data;

	DPRINTFN(2, ("oplsyn_close:\n"));

	KASSERT(mutex_owned(sc->lock));

	/*opl_reset(ms->data);*/
	if (sc->spkrctl)
		sc->spkrctl(sc->spkrarg, 0);
#ifndef AUDIO_NO_POWER_CTL
	if (sc->powerctl)
		sc->powerctl(sc->powerarg, 0);
#endif
}

#if 0
void
oplsyn_getinfo(void *addr, struct synth_dev *sd)
{
	struct opl_softc *sc = addr;

	sd->name = sc->model == OPL_2 ? "Yamaha OPL2" : "Yamaha OPL3";
	sd->type = SYNTH_TYPE_FM;
	sd->subtype = sc->model == OPL_2 ? SYNTH_SUB_FM_TYPE_ADLIB
		: SYNTH_SUB_FM_TYPE_OPL3;
	sd->capabilities = 0;
}
#endif

void
oplsyn_reset(void *addr)
{
	struct opl_softc *sc = addr;

	KASSERT(mutex_owned(sc->lock));

	DPRINTFN(3, ("oplsyn_reset:\n"));
	opl_reset(sc);
}

int
opl_calc_vol(int regbyte, int16_t level_cB)
{
	int level = regbyte & OPL_TOTAL_LEVEL_MASK;
	
	/*
	 * level is a six-bit attenuation, from 0 (full output)
	 * to -48dB (but without the minus sign) in steps of .75 dB.
	 * We'll just add level_cB, after scaling it because it's
	 * in centibels instead and has the customary minus sign.
	 */

	level += ( -4 * level_cB ) / 30;

	if (level > OPL_TOTAL_LEVEL_MASK)
		level = OPL_TOTAL_LEVEL_MASK;
	if (level < 0)
		level = 0;

	return level & OPL_TOTAL_LEVEL_MASK;
}

#define OPLACT_ARTICULATE 1
#define OPLACT_PITCH      2
#define OPLACT_LEVEL      4

void
oplsyn_attackv(midisyn *ms,
               uint_fast16_t voice, midipitch_t mp, int16_t level_cB)
{
	oplsyn_setv(ms, voice, mp, level_cB,
		    OPLACT_ARTICULATE | OPLACT_PITCH | OPLACT_LEVEL);
}

static void
oplsyn_repitchv(midisyn *ms, uint_fast16_t voice, midipitch_t mp)
{
	oplsyn_setv(ms, voice, mp, 0, OPLACT_PITCH);
}

static void
oplsyn_relevelv(midisyn *ms, uint_fast16_t voice, int16_t level_cB)
{
	oplsyn_setv(ms, voice, 0, level_cB, OPLACT_LEVEL);
}

static void
oplsyn_setv(midisyn *ms,
            uint_fast16_t voice, midipitch_t mp, int16_t level_cB, int act)
{
	struct opl_softc *sc = ms->data;
	struct opl_voice *v;
	const struct opl_operators *p;
	u_int32_t block_fnum;
	int mult;
	int c_mult, m_mult;
	u_int32_t chan;
	u_int8_t chars0, chars1, ksl0, ksl1, fbc;
	u_int8_t r20m, r20c, r40m, r40c, rA0, rB0;
	u_int8_t vol0, vol1;

	KASSERT(mutex_owned(sc->lock));

	DPRINTFN(3, ("%s: %p %d %u %d\n", __func__, sc, voice,
		     mp, level_cB));

#ifdef DIAGNOSTIC
	if (voice >= sc->syn.nvoice) {
		printf("%s: bad voice %d\n", __func__, voice);
		return;
	}
#endif
	v = &sc->voices[voice];

	if ( act & OPLACT_ARTICULATE ) {
		/* Turn off old note */
		opl_set_op_reg(sc, OPL_KSL_LEVEL,   voice, 0, 0xff);
		opl_set_op_reg(sc, OPL_KSL_LEVEL,   voice, 1, 0xff);
		opl_set_ch_reg(sc, OPL_KEYON_BLOCK, voice,    0);

		chan = MS_GETCHAN(&ms->voices[voice]);
		p = &opl2_instrs[ms->pgms[chan]];
		v->patch = p;
		opl_load_patch(sc, voice);

		fbc = p->ops[OO_FB_CONN];
		if (sc->model == OPL_3) {
			fbc &= ~OPL_STEREO_BITS;
			fbc |= sc->pan[chan];
		}
		opl_set_ch_reg(sc, OPL_FEEDBACK_CONNECTION, voice, fbc);
	} else
		p = v->patch;

	if ( act & OPLACT_LEVEL ) {
		/* 2 voice */
		ksl0 = p->ops[OO_KSL_LEV+0];
		ksl1 = p->ops[OO_KSL_LEV+1];
		if (p->ops[OO_FB_CONN] & 0x01) {
			vol0 = opl_calc_vol(ksl0, level_cB);
			vol1 = opl_calc_vol(ksl1, level_cB);
		} else {
			vol0 = ksl0;
			vol1 = opl_calc_vol(ksl1, level_cB);
		}
		r40m = (ksl0 & OPL_KSL_MASK) | vol0;
		r40c = (ksl1 & OPL_KSL_MASK) | vol1;

		opl_set_op_reg(sc, OPL_KSL_LEVEL,   voice, 0, r40m);
		opl_set_op_reg(sc, OPL_KSL_LEVEL,   voice, 1, r40c);
	}
	
	if ( act & OPLACT_PITCH ) {
		mult = 1;
		if ( mp > MIDIPITCH_FROM_KEY(114) ) { /* out of mult 1 range */
			mult = 4;	/* will cover remaining MIDI range */
			mp -= 2*MIDIPITCH_OCTAVE;
		}

		block_fnum = opl_get_block_fnum(mp);

		chars0 = p->ops[OO_CHARS+0];
		chars1 = p->ops[OO_CHARS+1];
		m_mult = (chars0 & OPL_MULTIPLE_MASK) * mult;
		c_mult = (chars1 & OPL_MULTIPLE_MASK) * mult;

		if ( 4 == mult ) {
			if ( 0 == m_mult )  /* The OPL uses 0 to represent .5 */
				m_mult = 2; /* but of course 0*mult above did */
			if ( 0 == c_mult )  /* not DTRT */
				c_mult = 2;
		}

		if ((m_mult > 15) || (c_mult > 15)) {
			printf("%s: frequency out of range %u (mult %d)\n",
			       __func__, mp, mult);
			return;
		}
		r20m = (chars0 &~ OPL_MULTIPLE_MASK) | m_mult;
		r20c = (chars1 &~ OPL_MULTIPLE_MASK) | c_mult;

		rA0  = block_fnum & 0xFF;
		rB0  = (block_fnum >> 8) | OPL_KEYON_BIT;

		v->rB0 = rB0;

		opl_set_op_reg(sc, OPL_AM_VIB,      voice, 0, r20m);
		opl_set_op_reg(sc, OPL_AM_VIB,      voice, 1, r20c);

		opl_set_ch_reg(sc, OPL_FNUM_LOW,    voice,    rA0);
		opl_set_ch_reg(sc, OPL_KEYON_BLOCK, voice,    rB0);
	}
}

void
oplsyn_releasev(midisyn *ms, uint_fast16_t voice, uint_fast8_t vel)
{
	struct opl_softc *sc = ms->data;
	struct opl_voice *v;

	KASSERT(mutex_owned(sc->lock));

	DPRINTFN(1, ("%s: %p %d\n", __func__, sc, voice));

#ifdef DIAGNOSTIC
	if (voice >= sc->syn.nvoice) {
		printf("oplsyn_noteoff: bad voice %d\n", voice);
		return;
	}
#endif
	v = &sc->voices[voice];
	opl_set_ch_reg(sc, 0xB0, voice, v->rB0 & ~OPL_KEYON_BIT);
}

int
oplsyn_ctlnotice(midisyn *ms,
		 midictl_evt evt, uint_fast8_t chan, uint_fast16_t key)
{

	DPRINTFN(1, ("%s: %p %d\n", __func__, ms->data, chan));
	
	switch (evt) {
	case MIDICTL_RESET:
		oplsyn_panhandler(ms, chan);
		return 1;
	
	case MIDICTL_CTLR:
		switch (key) {
		case MIDI_CTRL_PAN_MSB:
			oplsyn_panhandler(ms, chan);
			return 1;
		}
		return 0;
	default:
		return 0;
	}
}

/* PROGRAM CHANGE midi event: */
void
oplsyn_programchange(midisyn *ms, uint_fast8_t chan, uint_fast8_t prog)
{
	/* sanity checks */
	if (chan >= MIDI_MAX_CHANS)
		return;

	ms->pgms[chan] = prog;
}

void
oplsyn_loadpatch(midisyn *ms, struct sysex_info *sysex,
    struct uio *uio)
{
#if 0
	struct opl_softc *sc = ms->data;
	struct sbi_instrument ins;

	DPRINTFN(1, ("oplsyn_loadpatch: %p\n", sc));

	memcpy(&ins, sysex, sizeof *sysex);
	if (uio->uio_resid >= sizeof ins - sizeof *sysex)
		return EINVAL;
	uiomove((char *)&ins + sizeof *sysex, sizeof ins - sizeof *sysex, uio);
	/* XXX */
#endif
}

static void
oplsyn_panhandler(midisyn *ms, uint_fast8_t chan)
{
	struct opl_softc *sc = ms->data;
	uint_fast16_t setting;
	
	setting = midictl_read(&ms->ctl, chan, MIDI_CTRL_PAN_MSB, 8192);
	setting >>= 7; /* we used to treat it as MSB only */
	sc->pan[chan] =
	    (setting <= OPL_MIDI_CENTER_MAX ? sc->panl : 0) |
	    (setting >= OPL_MIDI_CENTER_MIN ? sc->panr : 0);
}
