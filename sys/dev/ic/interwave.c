/*	$NetBSD: interwave.c,v 1.38 2013/11/08 03:12:17 christos Exp $	*/

/*
 * Copyright (c) 1997, 1999, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Kari Mettinen
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
__KERNEL_RCSID(0, "$NetBSD: interwave.c,v 1.38 2013/11/08 03:12:17 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/cpu.h>
#include <sys/intr.h>
#include <sys/audioio.h>

#include <machine/pio.h>

#include <dev/audio_if.h>
#include <dev/mulaw.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/interwavereg.h>
#include <dev/ic/interwavevar.h>


static void iwreset(struct iw_softc *, int);

static int iw_set_speed(struct iw_softc *, u_long, char);
static u_long iw_set_format(struct iw_softc *, u_long, int);
static void iw_mixer_line_level(struct iw_softc *, int, int, int);
static void iw_trigger_dma(struct iw_softc *, u_char);
static void iw_stop_dma(struct iw_softc *, u_char, u_char);
static void iw_dma_count(struct iw_softc *, u_short, int);
static int iwintr(void *);
static void iw_meminit(struct iw_softc *);
static void iw_mempoke(struct iw_softc *, u_long, u_char);
static u_char iw_mempeek(struct iw_softc *, u_long);

#ifdef USE_WAVETABLE
static void iw_set_voice_place(struct iw_softc *, u_char, u_long);
static void iw_voice_pan(struct iw_softc *, u_char, u_short, u_short);
static void iw_voice_freq(struct iw_softc *, u_char, u_long);
static void iw_set_loopmode(struct iw_softc *, u_char, u_char, u_char);
static void iw_set_voice_pos(struct iw_softc *, u_short, u_long, u_long);
static void iw_start_voice(struct iw_softc *, u_char);
static void iw_play_voice(struct iw_softc *, u_long, u_long, u_short);
static void iw_stop_voice(struct iw_softc *, u_char);
static void iw_move_voice_end(struct iw_softc *, u_short, u_long);
static void iw_initvoices(struct iw_softc *);
#endif

struct audio_device iw_device = {
	"Am78C201",
	"0.1",
	"guspnp"
};

#ifdef AUDIO_DEBUG
int iw_debug;
#define DPRINTF(p)       if (iw_debug) printf p
#else
#define DPRINTF(p)
#endif

static int      iw_cc = 1;
#ifdef DIAGNOSTIC
static int      outputs = 0;
static int      iw_ints = 0;
static int      inputs = 0;
static int      iw_inints = 0;
#endif

int
iwintr(void *arg)
{
	struct	iw_softc *sc;
	int	val;
	u_char	intrs;

	sc = arg;
	val = 0;
	intrs = 0;

	mutex_spin_enter(&sc->sc_intr_lock);

	IW_READ_DIRECT_1(6, sc->p2xr_h, intrs);	/* UISR */

	/* codec ints */

	/*
	 * The proper order to do this seems to be to read CSR3 to get the
	 * int cause and fifo over underrrun status, then deal with the ints
	 * (new DMA set up), and to clear ints by writing the respective bit
	 * to 0.
	 */

	/* read what ints happened */

	IW_READ_CODEC_1(CSR3I, intrs);

	/* clear them */

	IW_WRITE_DIRECT_1(2, sc->codec_index_h, 0x00);

	/* and process them */

	if (intrs & 0x20) {
#ifdef DIAGNOSTIC
		iw_inints++;
#endif
		if (sc->sc_recintr != 0)
			sc->sc_recintr(sc->sc_recarg);
		val = 1;
	}
	if (intrs & 0x10) {
#ifdef DIAGNOSTIC
		iw_ints++;
#endif
		if (sc->sc_playintr != 0)
			sc->sc_playintr(sc->sc_playarg);
		val = 1;
	}

	mutex_spin_exit(&sc->sc_intr_lock);

	return val;
}

void
iwattach(struct iw_softc *sc)
{
	int	got_irq;

	DPRINTF(("iwattach sc %p\n", sc));
	got_irq = 0;

	sc->cdatap = 1;		/* relative offsets in region */
	sc->csr1r = 2;
	sc->cxdr = 3;		/* CPDR or CRDR */

	sc->gmxr = 0;		/* sc->p3xr */
	sc->gmxdr = 1;		/* GMTDR or GMRDR */
	sc->svsr = 2;
	sc->igidxr = 3;
	sc->i16dp = 4;
	sc->i8dp = 5;
	sc->lmbdr = 7;

	sc->rec_precision = sc->play_precision = 8;
	sc->rec_channels = sc->play_channels = 1;
	sc->rec_encoding = sc->play_encoding = AUDIO_ENCODING_ULAW;
	sc->sc_irate = 8000;
	sc->sc_orate = 8000;

	sc->sc_fullduplex = 1;

	sc->sc_dma_flags = 0;

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	/*
	 * We can only use a few selected irqs, see if we got one from pnp
	 * code that suits us.
	 */

	if (sc->sc_irq > 0) {
		sc->sc_ih = isa_intr_establish(sc->sc_p2xr_ic,
		    sc->sc_irq, IST_EDGE, IPL_AUDIO, iwintr, sc);
		got_irq = 1;
	}
	if (!got_irq) {
		printf("\niwattach: couldn't get a suitable irq\n");
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}
	printf("\n");
	iwreset(sc, 0);
	iw_set_format(sc, AUDIO_ENCODING_ULAW, 0);
	iw_set_format(sc, AUDIO_ENCODING_ULAW, 1);
	printf("%s: interwave version %s\n",
	    device_xname(sc->sc_dev), iw_device.version);
	audio_attach_mi(sc->iw_hw_if, sc, sc->sc_dev);
}

int
iwopen(struct iw_softc *sc, int flags)
{

	DPRINTF(("iwopen: sc %p\n", sc));

#ifdef DIAGNOSTIC
	outputs = 0;
	iw_ints = 0;
	inputs = 0;
	iw_inints = 0;
#endif

	iwreset(sc, 1);

	return 0;
}

void
iwclose(void *addr)
{

	DPRINTF(("iwclose sc %p\n", addr));
#ifdef DIAGNOSTIC
	DPRINTF(("iwclose: outputs %d ints %d inputs %d in_ints %d\n",
		outputs, iw_ints, inputs, iw_inints));
#endif
}

#define RAM_STEP	64*1024

static void
iw_mempoke(struct iw_softc *sc, u_long addy, u_char val)
{

	IW_WRITE_GENERAL_2(LMALI, (u_short) addy);
	IW_WRITE_GENERAL_1(LMAHI, (u_char) (addy >> 16));

	/* Write byte to LMBDR */
	IW_WRITE_DIRECT_1(sc->p3xr + 7, sc->p3xr_h, val);
}

static u_char
iw_mempeek(struct iw_softc *sc, u_long addy)
{
	u_char	ret;

	IW_WRITE_GENERAL_2(LMALI, (u_short) addy);
	IW_WRITE_GENERAL_1(LMAHI, (u_char) (addy >> 16));

	IW_READ_DIRECT_1(sc->p3xr + 7, sc->p3xr_h, ret);
	return ret;		/* return byte from LMBDR */
}

static void
iw_meminit(struct iw_softc *sc)
{
	u_long	bank[4] = {0L, 0L, 0L, 0L};
	u_long	addr, base, cnt;
	u_char	i, ram /* ,memval=0 */ ;
	u_short	lmcfi;
	u_long	temppi;
	u_long	*lpbanks;

	addr = 0L;
	base = 0L;
	cnt = 0L;
	ram = 0;
	lpbanks = &temppi;

	IW_WRITE_GENERAL_1(LDMACI, 0x00);

	IW_READ_GENERAL_2(LMCFI, lmcfi);	/* 0x52 */
	lmcfi |= 0x0A0C;
	IW_WRITE_GENERAL_2(LMCFI, lmcfi);	/* max addr span */
	IW_WRITE_GENERAL_1(LMCI, 0x00);

	/* fifo addresses */

	IW_WRITE_GENERAL_2(LMRFAI, ((4 * 1024 * 1024) >> 8));
	IW_WRITE_GENERAL_2(LMPFAI, ((4 * 1024 * 1024 + 16 * 1024) >> 8));

	IW_WRITE_GENERAL_2(LMFSI, 0x000);

	IW_WRITE_GENERAL_2(LDICI, 0x0000);

	while (addr < (16 * 1024 * 1024)) {
		iw_mempoke(sc, addr, 0x00);
		addr += RAM_STEP;
	}

	printf("%s:", device_xname(sc->sc_dev));

	for (i = 0; i < 4; i++) {
		iw_mempoke(sc, base, 0xAA);	/* mark start of bank */
		iw_mempoke(sc, base + 1L, 0x55);
		if (iw_mempeek(sc, base) == 0xAA  &&
		    iw_mempeek(sc, base + 1L) == 0x55)
			ram = 1;
		if (ram) {
			while (cnt < (4 * 1024 * 1024)) {
				bank[i] += RAM_STEP;
				cnt += RAM_STEP;
				addr = base + cnt;
				if (iw_mempeek(sc, addr) == 0xAA)
					break;
			}
		}
		if (lpbanks != NULL) {
			*lpbanks = bank[i];
			lpbanks++;
		}
		bank[i] = bank[i] >> 10;
		printf("%s bank[%d]: %ldK", i ? "," : "", i, bank[i]);
		base += 4 * 1024 * 1024;
		cnt = 0L;
		ram = 0;
	}

	printf("\n");

	/*
	 * this is not really useful since GUS PnP supports memory
	 * configurations that aren't really supported by Interwave...beware
	 * of holes! Also, we don't use the memory for anything in this
	 * version of the driver.
	 *
	 * we've configured for 4M-4M-4M-4M
	 */
}

static void
iwreset(struct iw_softc *sc, int warm)
{
	u_char	reg, cmode, val, mixer_image;

	val = 0;
	mixer_image = 0;
	reg = 0;		/* XXX gcc -Wall */

	cmode = 0x6c;		/* enhanced codec mode (full duplex) */

	/* reset */

	IW_WRITE_GENERAL_1(URSTI, 0x00);
	delay(10);
	IW_WRITE_GENERAL_1(URSTI, 0x07);
	IW_WRITE_GENERAL_1(ICMPTI, 0x1f);	/* disable DSP and uici and
						 * udci writes */
	IW_WRITE_GENERAL_1(IDECI, 0x7f);	/* enable ints to ISA and
						 * codec access */
	IW_READ_GENERAL_1(IVERI, reg);
	IW_WRITE_GENERAL_1(IVERI, reg | 0x01);	/* hidden reg lock disable */
	IW_WRITE_GENERAL_1(UASBCI, 0x00);

	/* synth enhanced mode (default), 0 active voices, disable ints */

	IW_WRITE_GENERAL_1(SGMI_WR, 0x01);	/* enhanced mode, LFOs
						 * disabled */
	for (val = 0; val < 32; val++) {
		/* set each synth sound volume to 0 */
		IW_WRITE_DIRECT_1(sc->p3xr + 2, sc->p3xr_h, val);
		IW_WRITE_GENERAL_1(SVSI_WR, 0x00);
		IW_WRITE_GENERAL_2(SASLI_WR, 0x0000);
		IW_WRITE_GENERAL_2(SASHI_WR, 0x0000);
		IW_WRITE_GENERAL_2(SAELI_WR, 0x0000);
		IW_WRITE_GENERAL_2(SAEHI_WR, 0x0000);
		IW_WRITE_GENERAL_2(SFCI_WR, 0x0000);
		IW_WRITE_GENERAL_1(SACI_WR, 0x02);
		IW_WRITE_GENERAL_1(SVSI_WR, 0x00);
		IW_WRITE_GENERAL_1(SVEI_WR, 0x00);
		IW_WRITE_GENERAL_2(SVLI_WR, 0x0000);
		IW_WRITE_GENERAL_1(SVCI_WR, 0x02);
		IW_WRITE_GENERAL_1(SMSI_WR, 0x02);
	}

	IW_WRITE_GENERAL_1(SAVI_WR, 0x00);

	/* codec mode/init */

	/* first change mode to 1 */

	IW_WRITE_CODEC_1(CMODEI, 0x00);

	/* and mode 3 */

	IW_WRITE_CODEC_1(CMODEI, cmode);

	IW_READ_CODEC_1(CMODEI, reg);

	DPRINTF(("cmode %x\n", reg));

	sc->revision = ((reg & 0x80) >> 3) | (reg & 0x0f);

	IW_WRITE_DIRECT_1(sc->codec_index + 2, sc->p2xr_h, 0x00);

	IW_WRITE_CODEC_1(CFIG1I | IW_MCE, 0x00);	/* DMA 2 chan access */
	IW_WRITE_CODEC_1(CEXTI, 0x00);	/* disable ints for now */


	IW_WRITE_CODEC_1(CLPCTI, 0x00);	/* reset playback sample counters */
	IW_WRITE_CODEC_1(CUPCTI, 0x00);	/* always upper byte last */
	IW_WRITE_CODEC_1(CFIG2I, 0x80);	/* full voltage range, enable record
					 * and playback sample counters, and
					 * don't center output in case or
					 * FIFO underrun */
	IW_WRITE_CODEC_1(CFIG3I, 0xc0);	/* enable record/playback irq (still
					 * turned off from CEXTI), max DMA
					 * rate */
	IW_WRITE_CODEC_1(CSR3I, 0x00);	/* clear status 3 reg */


	IW_WRITE_CODEC_1(CLRCTI, 0x00);	/* reset record sample counters */
	IW_WRITE_CODEC_1(CURCTI, 0x00);	/* always upper byte last */


	IW_READ_GENERAL_1(IVERI, reg);

	sc->vers = reg >> 4;
	if (!warm)
		snprintf(iw_device.version, sizeof(iw_device.version), "%d.%d",
		    sc->vers, sc->revision);

	IW_WRITE_GENERAL_1(IDECI, 0x7f);	/* irqs and codec decode
						 * enable */


	/* ports */

	if (!warm) {
		iw_mixer_line_level(sc, IW_LINE_OUT, 255, 255);
		iw_mixer_line_level(sc, IW_LINE_IN, 0, 0);
		iw_mixer_line_level(sc, IW_AUX1, 0, 0);
		iw_mixer_line_level(sc, IW_AUX2, 200, 200); /* CD */
		sc->sc_dac.off = 0;
		iw_mixer_line_level(sc, IW_DAC, 200, 200);

		iw_mixer_line_level(sc, IW_MIC_IN, 0, 0);
		iw_mixer_line_level(sc, IW_REC, 0, 0);
		iw_mixer_line_level(sc, IW_LOOPBACK, 0, 0);
		iw_mixer_line_level(sc, IW_MONO_IN, 0, 0);

		/* mem stuff */
		iw_meminit(sc);

	}
	IW_WRITE_CODEC_1(CEXTI, 0x02);	/* codec int enable */

	/* clear _LDMACI */

	IW_WRITE_GENERAL_1(LDMACI, 0x00);

	/* enable mixer paths */
	mixer_image = 0x0c;
	IW_WRITE_DIRECT_1(sc->p2xr, sc->p2xr_h, mixer_image);
	/*
	 * enable output, line in. disable mic in bit 0 = 0 -> line in on
	 * (from codec?) bit 1 = 0 -> output on bit 2 = 1 -> mic in on bit 3
	 * = 1 -> irq&drq pin enable bit 4 = 1 -> channel interrupts to chan
	 * 1 bit 5 = 1 -> enable midi loop back bit 6 = 0 -> irq latches
	 * URCR[2:0] bit 6 = 1 -> DMA latches URCR[2:0]
	 */


	IW_READ_DIRECT_1(sc->p2xr, sc->p2xr_h, mixer_image);
#ifdef AUDIO_DEBUG
	if (!warm)
		DPRINTF(("mix image %x \n", mixer_image));
#endif
}

struct iw_codec_freq {
	u_long	freq;
	u_char	bits;
};

int
iw_set_speed(struct iw_softc *sc, u_long freq, char in)
{
	u_char	var, cfig3, reg;

	static struct iw_codec_freq iw_cf[17] = {
#define FREQ_1 24576000
#define FREQ_2 16934400
#define XTAL1 0
#define XTAL2 1
		{5510, 0x00 | XTAL2}, {6620, 0x0E | XTAL2},
		{8000, 0x00 | XTAL1}, {9600, 0x0E | XTAL1},
		{11025, 0x02 | XTAL2}, {16000, 0x02 | XTAL1},
		{18900, 0x04 | XTAL2}, {22050, 0x06 | XTAL2},
		{27420, 0x04 | XTAL1}, {32000, 0x06 | XTAL1},
		{33075, 0x0C | XTAL2}, {37800, 0x08 | XTAL2},
		{38400, 0x0A | XTAL1}, {44100, 0x0A | XTAL2},
		{44800, 0x08 | XTAL1}, {48000, 0x0C | XTAL1},
		{48000, 0x0C | XTAL1}	/* really a dummy for indexing later */
#undef XTAL1
#undef XTAL2
	};

	cfig3 = 0;		/* XXX gcc -Wall */

	/*
	 * if the frequency is between 3493 Hz and 32 kHz we can use a more
	 * accurate frequency than the ones listed above base on the formula
	 * FREQ/((16*(48+x))) where FREQ is either FREQ_1 (24576000Hz) or
	 * FREQ_2 (16934400Hz) and x is the value to be written to either
	 * CPVFI or CRVFI. To enable this option, bit 2 in CFIG3 needs to be
	 * set high
	 *
	 * NOT IMPLEMENTED!
	 *
	 * Note that if you have a 'bad' XTAL_1 (higher than 18.5 MHz), 44.8 kHz
	 * and 38.4 kHz modes will provide wrong frequencies to output.
	 */


	if (freq > 48000)
		freq = 48000;
	if (freq < 5510)
		freq = 5510;

	/* reset CFIG3[2] */

	IW_READ_CODEC_1(CFIG3I, cfig3);

	cfig3 |= 0xc0;		/* not full fifo treshhold */

	DPRINTF(("cfig3i = %x -> ", cfig3));

	cfig3 &= ~0x04;
	IW_WRITE_CODEC_1(CFIG3I, cfig3);
	IW_READ_CODEC_1(CFIG3I, cfig3);

	DPRINTF(("%x\n", cfig3));

	for (var = 0; var < 16; var++)	/* select closest frequency */
		if (freq <= iw_cf[var].freq)
			break;
	if (var != 16)
		if (abs(freq - iw_cf[var].freq) > abs(iw_cf[var + 1].freq - freq))
			var++;

	if (in)
		IW_WRITE_CODEC_1(CRDFI | IW_MCE, sc->recfmtbits | iw_cf[var].bits);
	else
		IW_WRITE_CODEC_1(CPDFI | IW_MCE, sc->playfmtbits | iw_cf[var].bits);
	freq = iw_cf[var].freq;
	DPRINTF(("setting %s frequency to %d bits %x \n",
	       in ? "in" : "out", (int) freq, iw_cf[var].bits));

	IW_READ_CODEC_1(CPDFI, reg);

	DPRINTF((" CPDFI %x ", reg));

	IW_READ_CODEC_1(CRDFI, reg);

	DPRINTF((" CRDFI %x ", reg));
	__USE(reg);

	return freq;
}

/* Encoding. */
int
iw_query_encoding(void *addr, audio_encoding_t *fp)
{
	/*
	 * LINEAR, ALAW, ULAW, ADPCM in HW, we'll use linear unsigned
	 * hardware mode for all 8-bit modes due to buggy (?) codec.
	 */

	/*
	 * except in wavetable synth. there we have only mu-law and 8 and 16
	 * bit linear data
	 */

	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 2:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 3:
		strcpy(fp->name, AudioEadpcm);
		fp->encoding = AUDIO_ENCODING_ADPCM;
		fp->precision = 8;	/* really 4 bit */
		fp->flags = 0;
		break;
	case 4:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 5:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	default:
		return EINVAL;
		/* NOTREACHED */
	}
	return 0;
}

u_long
iw_set_format(struct iw_softc *sc, u_long precision, int in)
{
	u_char	data;
	int	encoding, channels;

	encoding = in ? sc->rec_encoding : sc->play_encoding;
	channels = in ? sc->rec_channels : sc->play_channels;

	DPRINTF(("iw_set_format\n"));

	switch (encoding) {
	case AUDIO_ENCODING_ULAW:
		data = 0x00;
		break;

	case AUDIO_ENCODING_ALAW:
		data = 0x00;
		break;

	case AUDIO_ENCODING_SLINEAR_LE:
		if (precision == 16)
			data = 0x40;	/* little endian. 0xc0 is big endian */
		else
			data = 0x00;
		break;

	case AUDIO_ENCODING_SLINEAR_BE:
		if (precision == 16)
			data = 0xc0;
		else
			data = 0x00;
		break;

	case AUDIO_ENCODING_ADPCM:
		data = 0xa0;
		break;

	default:
		return -1;
	}

	if (channels == 2)
		data |= 0x10;	/* stereo */

	if (in) {
		/* in */
		sc->recfmtbits = data;
		/* This will zero the normal codec frequency,
		 * iw_set_speed should always be called afterwards.
		 */
		IW_WRITE_CODEC_1(CRDFI | IW_MCE, data);
	} else {
		/* out */
		sc->playfmtbits = data;
		IW_WRITE_CODEC_1(CPDFI | IW_MCE, data);
	}

	DPRINTF(("formatbits %s %x", in ? "in" : "out", data));

	return encoding;
}

int
iw_set_params(void *addr, int setmode, int usemode, audio_params_t *p,
    audio_params_t *q, stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
	audio_params_t phw, rhw;
	struct iw_softc *sc;
	stream_filter_factory_t *swcode;

	DPRINTF(("iw_setparams: code %u, prec %u, rate %u, chan %u\n",
	    p->encoding, p->precision, p->sample_rate, p->channels));
	sc = addr;
	swcode = NULL;
	phw = *p;
	rhw = *q;
	switch (p->encoding) {
	case AUDIO_ENCODING_ULAW:
		if (p->precision != 8)
			return EINVAL;
		phw.encoding = AUDIO_ENCODING_ULINEAR_LE;
		rhw.encoding = AUDIO_ENCODING_ULINEAR_LE;
		swcode = setmode & AUMODE_PLAY ? mulaw_to_linear8 : linear8_to_mulaw;
		break;
	case AUDIO_ENCODING_ALAW:
		if (p->precision != 8)
			return EINVAL;
		phw.encoding = AUDIO_ENCODING_ULINEAR_LE;
		rhw.encoding = AUDIO_ENCODING_ULINEAR_LE;
		swcode = setmode & AUMODE_PLAY ? alaw_to_linear8 : linear8_to_alaw;
		break;
	case AUDIO_ENCODING_ADPCM:
		if (p->precision != 8)
			return EINVAL;
		else
			break;

	case AUDIO_ENCODING_SLINEAR_LE:
	case AUDIO_ENCODING_SLINEAR_BE:
		if (p->precision != 8 && p->precision != 16)
			return EINVAL;
		else
			break;

	default:
		return EINVAL;

	}

	if (setmode & AUMODE_PLAY) {
		sc->play_channels = p->channels;
		sc->play_encoding = p->encoding;
		sc->play_precision = p->precision;
		iw_set_format(sc, p->precision, 0);
		q->sample_rate = p->sample_rate = sc->sc_orate =
			iw_set_speed(sc, p->sample_rate, 0);
		if (swcode != NULL) {
			phw.sample_rate = p->sample_rate;
			pfil->append(pfil, swcode, &phw);
		}
	} else {
#if 0
		q->channels = sc->rec_channels = p->channels;
		q->encoding = sc->rec_encoding = p->encoding;
		q->precision = sc->rec_precision = p->precision;
#endif
		sc->rec_channels = q->channels;
		sc->rec_encoding = q->encoding;
		sc->rec_precision = q->precision;

		iw_set_format(sc, p->precision, 1);
		q->sample_rate = sc->sc_irate =
			iw_set_speed(sc, q->sample_rate, 1);
		if (swcode != NULL) {
			rhw.sample_rate = q->sample_rate;
			rfil->append(rfil, swcode, &rhw);
		}
	}
	return 0;
}


int
iw_round_blocksize(void *addr, int blk, int mode,
    const audio_params_t *param)
{

	/* Round to a multiple of the biggest sample size. */
	return blk &= -4;
}

void
iw_mixer_line_level(struct iw_softc *sc, int line, int levl, int levr)
{
	u_char	gainl, gainr, attenl, attenr;

	switch (line) {
	case IW_REC:
		gainl = sc->sc_recsrcbits | (levl >> 4);
		gainr = sc->sc_recsrcbits | (levr >> 4);
		DPRINTF(("recording with %x", gainl));
		IW_WRITE_CODEC_1(CLICI, gainl);
		IW_WRITE_CODEC_1(CRICI, gainr);
		sc->sc_rec.voll = levl & 0xf0;
		sc->sc_rec.volr = levr & 0xf0;
		break;

	case IW_AUX1:

		gainl = (255 - levl) >> 3;
		gainr = (255 - levr) >> 3;

		/* mute if 0 level */
		if (levl == 0)
			gainl |= 0x80;
		if (levr == 0)
			gainr |= 0x80;

		IW_WRITE_CODEC_1(IW_LEFT_AUX1_PORT, gainl);
		IW_WRITE_CODEC_1(IW_RIGHT_AUX1_PORT, gainr);
		sc->sc_aux1.voll = levl & 0xf8;
		sc->sc_aux1.volr = levr & 0xf8;

		break;

	case IW_AUX2:

		gainl = (255 - levl) >> 3;
		gainr = (255 - levr) >> 3;

		/* mute if 0 level */
		if (levl == 0)
			gainl |= 0x80;
		if (levr == 0)
			gainr |= 0x80;

		IW_WRITE_CODEC_1(IW_LEFT_AUX2_PORT, gainl);
		IW_WRITE_CODEC_1(IW_RIGHT_AUX2_PORT, gainr);
		sc->sc_aux2.voll = levl & 0xf8;
		sc->sc_aux2.volr = levr & 0xf8;
		break;
	case IW_DAC:
		attenl = ((255 - levl) >> 2) | ((levl && !sc->sc_dac.off) ? 0 : 0x80);
		attenr = ((255 - levr) >> 2) | ((levr && !sc->sc_dac.off) ? 0 : 0x80);
		IW_WRITE_CODEC_1(CLDACI, attenl);
		IW_WRITE_CODEC_1(CRDACI, attenr);
		sc->sc_dac.voll = levl & 0xfc;
		sc->sc_dac.volr = levr & 0xfc;
		break;
	case IW_LOOPBACK:
		attenl = ((255 - levl) & 0xfc) | (levl ? 0x01 : 0);
		IW_WRITE_CODEC_1(CLCI, attenl);
		sc->sc_loopback.voll = levl & 0xfc;
		break;
	case IW_LINE_IN:
		gainl = (levl >> 3) | (levl ? 0 : 0x80);
		gainr = (levr >> 3) | (levr ? 0 : 0x80);
		IW_WRITE_CODEC_1(CLLICI, gainl);
		IW_WRITE_CODEC_1(CRLICI, gainr);
		sc->sc_linein.voll = levl & 0xf8;
		sc->sc_linein.volr = levr & 0xf8;
		break;
	case IW_MIC_IN:
		gainl = ((255 - levl) >> 3) | (levl ? 0 : 0x80);
		gainr = ((255 - levr) >> 3) | (levr ? 0 : 0x80);
		IW_WRITE_CODEC_1(CLMICI, gainl);
		IW_WRITE_CODEC_1(CRMICI, gainr);
		sc->sc_mic.voll = levl & 0xf8;
		sc->sc_mic.volr = levr & 0xf8;
		break;
	case IW_LINE_OUT:
		attenl = ((255 - levl) >> 3) | (levl ? 0 : 0x80);
		attenr = ((255 - levr) >> 3) | (levr ? 0 : 0x80);
		IW_WRITE_CODEC_1(CLOAI, attenl);
		IW_WRITE_CODEC_1(CROAI, attenr);
		sc->sc_lineout.voll = levl & 0xf8;
		sc->sc_lineout.volr = levr & 0xf8;
		break;
	case IW_MONO_IN:
		attenl = ((255 - levl) >> 4) | (levl ? 0 : 0xc0);	/* in/out mute */
		IW_WRITE_CODEC_1(CMONOI, attenl);
		sc->sc_monoin.voll = levl & 0xf0;
		break;
	}
}

int
iw_commit_settings(void *addr)
{

	return 0;
}

void
iw_trigger_dma(struct iw_softc *sc, u_char io)
{
	u_char	reg;

	IW_READ_CODEC_1(CSR3I, reg);
	IW_WRITE_CODEC_1(CSR3I, reg & ~(io == IW_DMA_PLAYBACK ? 0x10 : 0x20));

	IW_READ_CODEC_1(CFIG1I, reg);

	IW_WRITE_CODEC_1(CFIG1I, reg | io);

	/* let the counter run */
	IW_READ_CODEC_1(CFIG2I, reg);
	IW_WRITE_CODEC_1(CFIG2I, reg & ~(io << 4));
}

void
iw_stop_dma(struct iw_softc *sc, u_char io, u_char hard)
{
	u_char	reg;

	/* just stop the counter, no need to flush the fifo */
	IW_READ_CODEC_1(CFIG2I, reg);
	IW_WRITE_CODEC_1(CFIG2I, (reg | (io << 4)));

	if (hard) {
		/* unless we're closing the device */
		IW_READ_CODEC_1(CFIG1I, reg);
		IW_WRITE_CODEC_1(CFIG1I, reg & ~io);
	}
}

void
iw_dma_count(struct iw_softc *sc, u_short count, int io)
{

	if (io == IW_DMA_PLAYBACK) {
		IW_WRITE_CODEC_1(CLPCTI, (u_char) (count & 0x00ff));
		IW_WRITE_CODEC_1(CUPCTI, (u_char) ((count >> 8) & 0x00ff));
	} else {
		IW_WRITE_CODEC_1(CLRCTI, (u_char) (count & 0x00ff));
		IW_WRITE_CODEC_1(CURCTI, (u_char) ((count >> 8) & 0x00ff));
	}
}

int
iw_init_output(void *addr, void *sbuf, int cc)
{
	struct iw_softc *sc = (struct iw_softc *) addr;

	DPRINTF(("iw_init_output\n"));

	isa_dmastart(sc->sc_ic, sc->sc_playdrq, sbuf,
		     cc, NULL, DMAMODE_WRITE | DMAMODE_LOOP, BUS_DMA_NOWAIT);
	return 0;
}

int
iw_init_input(void *addr, void *sbuf, int cc)
{
	struct	iw_softc *sc;

	DPRINTF(("iw_init_input\n"));
	sc = (struct iw_softc *) addr;
	isa_dmastart(sc->sc_ic, sc->sc_recdrq, sbuf,
		     cc, NULL, DMAMODE_READ | DMAMODE_LOOP, BUS_DMA_NOWAIT);
	return 0;
}


int
iw_start_output(void *addr, void *p, int cc, void (*intr)(void *), void *arg)
{
	struct	iw_softc *sc;

#ifdef DIAGNOSTIC
	if (!intr) {
		printf("iw_start_output: no callback!\n");
		return 1;
	}
#endif
	sc = addr;
	sc->sc_playintr = intr;
	sc->sc_playarg = arg;
	sc->sc_dma_flags |= DMAMODE_WRITE;
	sc->sc_playdma_bp = p;

	isa_dmastart(sc->sc_ic, sc->sc_playdrq, sc->sc_playdma_bp,
	    cc, NULL, DMAMODE_WRITE, BUS_DMA_NOWAIT);


	if (sc->play_encoding == AUDIO_ENCODING_ADPCM)
		cc >>= 2;
	if (sc->play_precision == 16)
		cc >>= 1;

	if (sc->play_channels == 2 && sc->play_encoding != AUDIO_ENCODING_ADPCM)
		cc >>= 1;

	cc -= iw_cc;

	/* iw_dma_access(sc,1); */
	if (cc != sc->sc_playdma_cnt) {
		iw_dma_count(sc, (u_short) cc, IW_DMA_PLAYBACK);
		sc->sc_playdma_cnt = cc;

		iw_trigger_dma(sc, IW_DMA_PLAYBACK);
	}

#ifdef DIAGNOSTIC
	if (outputs != iw_ints)
		printf("iw_start_output: out %d, int %d\n", outputs, iw_ints);
	outputs++;
#endif

	return 0;
}


int
iw_start_input(void *addr, void *p, int cc, void (*intr)(void *), void *arg)
{
	struct	iw_softc *sc;

#ifdef DIAGNOSTIC
	if (!intr) {
		printf("iw_start_input: no callback!\n");
		return 1;
	}
#endif
	sc = addr;
	sc->sc_recintr = intr;
	sc->sc_recarg = arg;
	sc->sc_dma_flags |= DMAMODE_READ;
	sc->sc_recdma_bp = p;

	isa_dmastart(sc->sc_ic, sc->sc_recdrq, sc->sc_recdma_bp,
	    cc, NULL, DMAMODE_READ, BUS_DMA_NOWAIT);


	if (sc->rec_encoding == AUDIO_ENCODING_ADPCM)
		cc >>= 2;
	if (sc->rec_precision == 16)
		cc >>= 1;

	if (sc->rec_channels == 2 && sc->rec_encoding != AUDIO_ENCODING_ADPCM)
		cc >>= 1;

	cc -= iw_cc;

	/* iw_dma_access(sc,0); */
	if (sc->sc_recdma_cnt != cc) {
		iw_dma_count(sc, (u_short) cc, IW_DMA_RECORD);
		sc->sc_recdma_cnt = cc;
		/* iw_dma_ctrl(sc, IW_DMA_RECORD); */
		iw_trigger_dma(sc, IW_DMA_RECORD);
	}

#ifdef DIAGNOSTIC
	if ((inputs != iw_inints))
		printf("iw_start_input: in %d, inints %d\n", inputs, iw_inints);
	inputs++;
#endif

	return 0;
}


int
iw_halt_output(void *addr)
{
	struct	iw_softc *sc;

	sc = addr;
	iw_stop_dma(sc, IW_DMA_PLAYBACK, 0);
	return 0;
}


int
iw_halt_input(void *addr)
{
	struct	iw_softc *sc;

	sc = addr;
	iw_stop_dma(sc, IW_DMA_RECORD, 0);
	return 0;
}

int
iw_speaker_ctl(void *addr, int newstate)
{
	struct iw_softc *sc;
	u_char reg;

	sc = addr;
	if (newstate == SPKR_ON) {
		sc->sc_dac.off = 0;
		IW_READ_CODEC_1(CLDACI, reg);
		IW_WRITE_CODEC_1(CLDACI, reg & 0x7f);
		IW_READ_CODEC_1(CRDACI, reg);
		IW_WRITE_CODEC_1(CRDACI, reg & 0x7f);
	} else {
		/* SPKR_OFF */
		sc->sc_dac.off = 1;
		IW_READ_CODEC_1(CLDACI, reg);
		IW_WRITE_CODEC_1(CLDACI, reg | 0x80);
		IW_READ_CODEC_1(CRDACI, reg);
		IW_WRITE_CODEC_1(CRDACI, reg | 0x80);
	}
	return 0;
}

int
iw_getdev(void *addr, struct audio_device *retp)
{

	*retp = iw_device;
	return 0;
}

int
iw_setfd(void *addr, int flag)
{

	return 0;
}

/* Mixer (in/out ports) */
int
iw_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct iw_softc *sc;
	u_char vall, valr;
	int error;

	sc = addr;
	vall = 0;
	valr = 0;
	error = EINVAL;
	switch (cp->dev) {
	case IW_MIC_IN_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = 0;
			if (cp->un.value.num_channels == 1) {
				vall = valr = cp->un.value.level[0];
			} else {
				vall = cp->un.value.level[0];
				valr = cp->un.value.level[1];
			}
			sc->sc_mic.voll = vall;
			sc->sc_mic.volr = valr;
			iw_mixer_line_level(sc, IW_MIC_IN, vall, valr);
		}
		break;
	case IW_AUX1_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = 0;
			if (cp->un.value.num_channels == 1) {
				vall = valr = cp->un.value.level[0];
			} else {
				vall = cp->un.value.level[0];
				valr = cp->un.value.level[1];
			}
			sc->sc_aux1.voll = vall;
			sc->sc_aux1.volr = valr;
			iw_mixer_line_level(sc, IW_AUX1, vall, valr);
		}
		break;
	case IW_AUX2_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = 0;
			if (cp->un.value.num_channels == 1) {
				vall = valr = cp->un.value.level[0];
			} else {
				vall = cp->un.value.level[0];
				valr = cp->un.value.level[1];
			}
			sc->sc_aux2.voll = vall;
			sc->sc_aux2.volr = valr;
			iw_mixer_line_level(sc, IW_AUX2, vall, valr);
		}
		break;
	case IW_LINE_IN_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = 0;
			if (cp->un.value.num_channels == 1) {
				vall = valr = cp->un.value.level[0];
			} else {
				vall = cp->un.value.level[0];
				valr = cp->un.value.level[1];
			}
			sc->sc_linein.voll = vall;
			sc->sc_linein.volr = valr;
			iw_mixer_line_level(sc, IW_LINE_IN, vall, valr);
		}
		break;
	case IW_LINE_OUT_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = 0;
			if (cp->un.value.num_channels == 1) {
				vall = valr = cp->un.value.level[0];
			} else {
				vall = cp->un.value.level[0];
				valr = cp->un.value.level[1];
			}
			sc->sc_lineout.voll = vall;
			sc->sc_lineout.volr = valr;
			iw_mixer_line_level(sc, IW_LINE_OUT, vall, valr);
		}
		break;
	case IW_REC_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = 0;
			if (cp->un.value.num_channels == 1) {
				vall = valr = cp->un.value.level[0];
			} else {
				vall = cp->un.value.level[0];
				valr = cp->un.value.level[1];
			}
			sc->sc_rec.voll = vall;
			sc->sc_rec.volr = valr;
			iw_mixer_line_level(sc, IW_REC, vall, valr);
		}
		break;

	case IW_DAC_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = 0;
			if (cp->un.value.num_channels == 1) {
				vall = valr = cp->un.value.level[0];
			} else {
				vall = cp->un.value.level[0];
				valr = cp->un.value.level[1];
			}
			sc->sc_dac.voll = vall;
			sc->sc_dac.volr = valr;
			iw_mixer_line_level(sc, IW_DAC, vall, valr);
		}
		break;

	case IW_LOOPBACK_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = 0;
			if (cp->un.value.num_channels != 1) {
				return EINVAL;
			} else {
				valr = vall = cp->un.value.level[0];
			}
			sc->sc_loopback.voll = vall;
			sc->sc_loopback.volr = valr;
			iw_mixer_line_level(sc, IW_LOOPBACK, vall, valr);
		}
		break;

	case IW_MONO_IN_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			error = 0;
			if (cp->un.value.num_channels != 1) {
				return EINVAL;
			} else {
				valr = vall = cp->un.value.level[0];
			}
			sc->sc_monoin.voll = vall;
			sc->sc_monoin.volr = valr;
			iw_mixer_line_level(sc, IW_MONO_IN, vall, valr);
		}
		break;
	case IW_RECORD_SOURCE:
		error = 0;
		sc->sc_recsrcbits = cp->un.ord << 6;
		DPRINTF(("record source %d bits %x\n", cp->un.ord, sc->sc_recsrcbits));
		iw_mixer_line_level(sc, IW_REC, sc->sc_rec.voll, sc->sc_rec.volr);
		break;
	}

	return error;
}


int
iw_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct iw_softc *sc;
	int error;

	sc = addr;
	error = EINVAL;
	switch (cp->dev) {
	case IW_MIC_IN_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = 2;
			cp->un.value.level[0] = sc->sc_mic.voll;
			cp->un.value.level[1] = sc->sc_mic.volr;
			error = 0;
		}
		break;
	case IW_AUX1_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = 2;
			cp->un.value.level[0] = sc->sc_aux1.voll;
			cp->un.value.level[1] = sc->sc_aux1.volr;
			error = 0;
		}
		break;
	case IW_AUX2_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = 2;
			cp->un.value.level[0] = sc->sc_aux2.voll;
			cp->un.value.level[1] = sc->sc_aux2.volr;
			error = 0;
		}
		break;
	case IW_LINE_OUT_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = 2;
			cp->un.value.level[0] = sc->sc_lineout.voll;
			cp->un.value.level[1] = sc->sc_lineout.volr;
			error = 0;
		}
		break;
	case IW_LINE_IN_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = 2;
			cp->un.value.level[0] = sc->sc_linein.voll;
			cp->un.value.level[1] = sc->sc_linein.volr;
			error = 0;
		}
	case IW_REC_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = 2;
			cp->un.value.level[0] = sc->sc_rec.voll;
			cp->un.value.level[1] = sc->sc_rec.volr;
			error = 0;
		}
		break;

	case IW_DAC_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = 2;
			cp->un.value.level[0] = sc->sc_dac.voll;
			cp->un.value.level[1] = sc->sc_dac.volr;
			error = 0;
		}
		break;

	case IW_LOOPBACK_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = 1;
			cp->un.value.level[0] = sc->sc_loopback.voll;
			error = 0;
		}
		break;

	case IW_MONO_IN_LVL:
		if (cp->type == AUDIO_MIXER_VALUE) {
			cp->un.value.num_channels = 1;
			cp->un.value.level[0] = sc->sc_monoin.voll;
			error = 0;
		}
		break;
	case IW_RECORD_SOURCE:
		cp->un.ord = sc->sc_recsrcbits >> 6;
		error = 0;
		break;
	}

	return error;
}



int
iw_query_devinfo(void *addr, mixer_devinfo_t *dip)
{

	switch (dip->index) {
	case IW_MIC_IN_LVL:	/* Microphone */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = IW_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case IW_AUX1_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = IW_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNline);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case IW_AUX2_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = IW_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNcd);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case IW_LINE_OUT_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = IW_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNline);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case IW_DAC_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = IW_OUTPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNdac);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case IW_LINE_IN_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = IW_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNinput);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;
	case IW_MONO_IN_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = IW_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmono);
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;

	case IW_REC_LVL:	/* record level */
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = IW_RECORD_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNrecord);
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;

	case IW_LOOPBACK_LVL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = IW_RECORD_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "filter");
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		break;

	case IW_RECORD_SOURCE:
		dip->mixer_class = IW_RECORD_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNsource);
		dip->un.e.num_mem = 4;
		strcpy(dip->un.e.member[0].label.name, AudioNline);
		dip->un.e.member[0].ord = IW_LINE_IN_SRC;
		strcpy(dip->un.e.member[1].label.name, "aux1");
		dip->un.e.member[1].ord = IW_AUX1_SRC;
		strcpy(dip->un.e.member[2].label.name, AudioNmicrophone);
		dip->un.e.member[2].ord = IW_MIC_IN_SRC;
		strcpy(dip->un.e.member[3].label.name, AudioNmixerout);
		dip->un.e.member[3].ord = IW_MIX_OUT_SRC;
		break;
	case IW_INPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = IW_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCinputs);
		break;
	case IW_OUTPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = IW_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCoutputs);
		break;
	case IW_RECORD_CLASS:	/* record source class */
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = IW_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCrecord);
		return 0;
	default:
		return ENXIO;
	}
	return 0;
}


void *
iw_malloc(void *addr, int direction, size_t size)
{
	struct iw_softc *sc;
	int drq;

	sc = addr;
	if (direction == AUMODE_PLAY)
		drq = sc->sc_playdrq;
	else
		drq = sc->sc_recdrq;
	return isa_malloc(sc->sc_ic, drq, size, M_DEVBUF, M_WAITOK);
}

void
iw_free(void *addr, void *ptr, size_t size)
{

	isa_free(ptr, M_DEVBUF);
}

size_t
iw_round_buffersize(void *addr, int direction, size_t size)
{
	struct iw_softc *sc;
	bus_size_t maxsize;

	sc = addr;
	if (direction == AUMODE_PLAY)
		maxsize = sc->sc_play_maxsize;
	else
		maxsize = sc->sc_rec_maxsize;

	if (size > maxsize)
		size = maxsize;
	return size;
}

paddr_t
iw_mappage(void *addr, void *mem, off_t off, int prot)
{

	return isa_mappage(mem, off, prot);
}

int
iw_get_props(void *addr)
{
	struct iw_softc *sc;

	sc = addr;
	return AUDIO_PROP_MMAP |
		(sc->sc_fullduplex ? AUDIO_PROP_FULLDUPLEX : 0);
}

void
iw_get_locks(void *addr, kmutex_t **intr, kmutex_t **thread)
{
	struct iw_softc *sc;

	sc = addr;
	*intr = &sc->sc_intr_lock;
	*thread = &sc->sc_lock;
}
