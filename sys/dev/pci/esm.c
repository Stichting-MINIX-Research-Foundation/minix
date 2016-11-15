/*      $NetBSD: esm.c,v 1.58 2014/03/29 19:28:24 christos Exp $      */

/*-
 * Copyright (c) 2002, 2003 Matt Fredette
 * All rights reserved.
 *
 * Copyright (c) 2000, 2001 Rene Hexel <rh@NetBSD.org>
 * All rights reserved.
 *
 * Copyright (c) 2000 Taku YAMAMOTO <taku@cent.saitama-u.ac.jp>
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
 * Taku Id: maestro.c,v 1.12 2000/09/06 03:32:34 taku Exp
 * FreeBSD: /c/ncvs/src/sys/dev/sound/pci/maestro.c,v 1.4 2000/12/18 01:36:35 cg Exp
 */

/*
 * TODO:
 *	- hardware volume support
 *	- fix 16-bit stereo recording, add 8-bit recording
 *	- MIDI support
 *	- joystick support
 *
 *
 * Credits:
 *
 * This code is based on the FreeBSD driver written by Taku YAMAMOTO
 *
 *
 * Original credits from the FreeBSD driver:
 *
 * Part of this code (especially in many magic numbers) was heavily inspired
 * by the Linux driver originally written by
 * Alan Cox <alan.cox@linux.org>, modified heavily by
 * Zach Brown <zab@zabbo.net>.
 *
 * busdma()-ize and buffer size reduction were suggested by
 * Cameron Grant <gandalf@vilnya.demon.co.uk>.
 * Also he showed me the way to use busdma() suite.
 *
 * Internal speaker problems on NEC VersaPro's and Dell Inspiron 7500
 * were looked at by
 * Munehiro Matsuda <haro@tk.kubota.co.jp>,
 * who brought patches based on the Linux driver with some simplification.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: esm.c,v 1.58 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/bus.h>
#include <sys/audioio.h>

#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/ic/ac97var.h>
#include <dev/ic/ac97reg.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/esmreg.h>
#include <dev/pci/esmvar.h>

#define	PCI_CBIO		0x10	/* Configuration Base I/O Address */

/* Debug */
#ifdef AUDIO_DEBUG
#define DPRINTF(l,x)	do { if (esm_debug & (l)) printf x; } while(0)
#define DUMPREG(x)	do { if (esm_debug & ESM_DEBUG_REG)	\
				 esm_dump_regs(x); } while(0)
int esm_debug = 0xfffc;
#define ESM_DEBUG_CODECIO	0x0001
#define ESM_DEBUG_IRQ		0x0002
#define ESM_DEBUG_DMA		0x0004
#define ESM_DEBUG_TIMER		0x0008
#define ESM_DEBUG_REG		0x0010
#define ESM_DEBUG_PARAM		0x0020
#define ESM_DEBUG_APU		0x0040
#define ESM_DEBUG_CODEC		0x0080
#define ESM_DEBUG_PCI		0x0100
#define ESM_DEBUG_RESUME	0x0200
#else
#define DPRINTF(x,y)	/* nothing */
#define DUMPREG(x)	/* nothing */
#endif

#ifdef DIAGNOSTIC
#define RANGE(n, l, h)	if ((n) < (l) || (n) >= (h))			\
		printf (#n "=%d out of range (%d, %d) in "		\
		__FILE__ ", line %d\n", (n), (l), (h), __LINE__)
#else
#define RANGE(x,y,z)	/* nothing */
#endif

#define inline inline

static inline void	ringbus_setdest(struct esm_softc *, int, int);

static inline uint16_t	wp_rdreg(struct esm_softc *, uint16_t);
static inline void	wp_wrreg(struct esm_softc *, uint16_t, uint16_t);
static inline uint16_t	wp_rdapu(struct esm_softc *, int, uint16_t);
static inline void	wp_wrapu(struct esm_softc *, int, uint16_t,
			    uint16_t);
static inline void	wp_settimer(struct esm_softc *, u_int);
static inline void	wp_starttimer(struct esm_softc *);
static inline void	wp_stoptimer(struct esm_softc *);

static inline void	wc_wrreg(struct esm_softc *, uint16_t, uint16_t);
static inline void	wc_wrchctl(struct esm_softc *, int, uint16_t);

static inline u_int	calc_timer_freq(struct esm_chinfo*);
static void		set_timer(struct esm_softc *);

static void		esmch_set_format(struct esm_chinfo *,
			    const audio_params_t *);
static void		esmch_combine_input(struct esm_softc *,
			    struct esm_chinfo *);

static bool		esm_suspend(device_t, const pmf_qual_t *);
static bool		esm_resume(device_t, const pmf_qual_t *);
static void		esm_childdet(device_t, device_t);
static int		esm_match(device_t, cfdata_t, void *);
static void		esm_attach(device_t, device_t, void *);
static int		esm_detach(device_t, int);
static int		esm_intr(void *);

static void		esm_freemem(struct esm_softc *, struct esm_dma *);
static int		esm_allocmem(struct esm_softc *, size_t, size_t,
			             struct esm_dma *);


CFATTACH_DECL2_NEW(esm, sizeof(struct esm_softc),
    esm_match, esm_attach, esm_detach, NULL, NULL, esm_childdet);

const struct audio_hw_if esm_hw_if = {
	NULL,				/* open */
	NULL,				/* close */
	NULL,				/* drain */
	esm_query_encoding,
	esm_set_params,
	esm_round_blocksize,
	NULL,				/* commit_settings */
	esm_init_output,
	esm_init_input,
	NULL,				/* start_output */
	NULL,				/* start_input */
	esm_halt_output,
	esm_halt_input,
	NULL,				/* speaker_ctl */
	esm_getdev,
	NULL,				/* getfd */
	esm_set_port,
	esm_get_port,
	esm_query_devinfo,
	esm_malloc,
	esm_free,
	esm_round_buffersize,
	esm_mappage,
	esm_get_props,
	esm_trigger_output,
	esm_trigger_input,
	NULL,
	esm_get_locks,
};

struct audio_device esm_device = {
	"ESS Maestro",
	"",
	"esm"
};

#define MAESTRO_NENCODINGS 8
static audio_encoding_t esm_encoding[MAESTRO_NENCODINGS] = {
	{ 0, AudioEulinear, AUDIO_ENCODING_ULINEAR, 8, 0 },
	{ 1, AudioEmulaw, AUDIO_ENCODING_ULAW, 8,
		AUDIO_ENCODINGFLAG_EMULATED },
	{ 2, AudioEalaw, AUDIO_ENCODING_ALAW, 8, AUDIO_ENCODINGFLAG_EMULATED },
	{ 3, AudioEslinear, AUDIO_ENCODING_SLINEAR, 8, 0 },
	{ 4, AudioEslinear_le, AUDIO_ENCODING_SLINEAR_LE, 16, 0 },
	{ 5, AudioEulinear_le, AUDIO_ENCODING_ULINEAR_LE, 16,
		AUDIO_ENCODINGFLAG_EMULATED },
	{ 6, AudioEslinear_be, AUDIO_ENCODING_SLINEAR_BE, 16,
		AUDIO_ENCODINGFLAG_EMULATED },
	{ 7, AudioEulinear_be, AUDIO_ENCODING_ULINEAR_BE, 16,
		AUDIO_ENCODINGFLAG_EMULATED },
};

#define ESM_NFORMATS	4
static const struct audio_format esm_formats[ESM_NFORMATS] = {
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 1, AUFMT_MONAURAL, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 2, AUFMT_STEREO, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 1, AUFMT_MONAURAL, 0, {4000, 48000}},
};

static const struct esm_quirks esm_quirks[] = {
	/* COMPAL 38W2 OEM Notebook, e.g. Dell INSPIRON 5000e */
	{ PCI_VENDOR_COMPAL, PCI_PRODUCT_COMPAL_38W2, ESM_QUIRKF_SWAPPEDCH },

	/* COMPAQ Armada M700 Notebook */
	{ PCI_VENDOR_COMPAQ, PCI_PRODUCT_COMPAQ_M700, ESM_QUIRKF_SWAPPEDCH },

	/* NEC Versa Pro LX VA26D */
	{ PCI_VENDOR_NEC, PCI_PRODUCT_NEC_VA26D, ESM_QUIRKF_GPIO },

	/* NEC Versa LX */
	{ PCI_VENDOR_NEC, PCI_PRODUCT_NEC_VERSALX, ESM_QUIRKF_GPIO },

	/* Toshiba Portege */
	{ PCI_VENDOR_TOSHIBA2, PCI_PRODUCT_TOSHIBA2_PORTEGE, ESM_QUIRKF_SWAPPEDCH }
};

enum esm_quirk_flags
esm_get_quirks(pcireg_t subid)
{
	int i;

	for (i = 0; i < __arraycount(esm_quirks); i++) {
		if (PCI_VENDOR(subid) == esm_quirks[i].eq_vendor &&
		    PCI_PRODUCT(subid) == esm_quirks[i].eq_product) {
			return esm_quirks[i].eq_quirks;
		}
	}

	return 0;
}


#ifdef AUDIO_DEBUG
struct esm_reg_info {
	int	offset;			/* register offset */
	int	width;			/* 1/2/4 bytes */
} dump_regs[] = {
	{ PORT_WAVCACHE_CTRL,		2 },
	{ PORT_HOSTINT_CTRL,		2 },
	{ PORT_HOSTINT_STAT,		2 },
	{ PORT_HWVOL_VOICE_SHADOW,	1 },
	{ PORT_HWVOL_VOICE,		1 },
	{ PORT_HWVOL_MASTER_SHADOW,	1 },
	{ PORT_HWVOL_MASTER,		1 },
	{ PORT_RINGBUS_CTRL,		4 },
	{ PORT_GPIO_DATA,		2 },
	{ PORT_GPIO_MASK,		2 },
	{ PORT_GPIO_DIR,		2 },
	{ PORT_ASSP_CTRL_A,		1 },
	{ PORT_ASSP_CTRL_B,		1 },
	{ PORT_ASSP_CTRL_C,		1 },
	{ PORT_ASSP_INT_STAT,		1 }
};

static void
esm_dump_regs(struct esm_softc *ess)
{
	int i;

	printf("%s registers:", device_xname(ess->sc_dev));
	for (i = 0; i < __arraycount(dump_regs); i++) {
		if (i % 5 == 0)
			printf("\n");
		printf("0x%2.2x: ", dump_regs[i].offset);
		switch(dump_regs[i].width) {
		case 4:
			printf("%8.8x, ", bus_space_read_4(ess->st, ess->sh,
			    dump_regs[i].offset));
			break;
		case 2:
			printf("%4.4x,     ", bus_space_read_2(ess->st, ess->sh,
			    dump_regs[i].offset));
			break;
		default:
			printf("%2.2x,       ",
			    bus_space_read_1(ess->st, ess->sh,
			    dump_regs[i].offset));
		}
	}
	printf("\n");
}
#endif


/* -----------------------------
 * Subsystems.
 */

/* Codec/Ringbus */

/* -------------------------------------------------------------------- */

int
esm_read_codec(void *sc, uint8_t regno, uint16_t *result)
{
	struct esm_softc *ess;
	unsigned t;

	ess = sc;
	/* We have to wait for a SAFE time to write addr/data */
	for (t = 0; t < 20; t++) {
		if ((bus_space_read_1(ess->st, ess->sh, PORT_CODEC_STAT)
		    & CODEC_STAT_MASK) != CODEC_STAT_PROGLESS)
			break;
		delay(2);	/* 20.8us / 13 */
	}
	if (t == 20)
		printf("%s: esm_read_codec() PROGLESS timed out.\n",
		    device_xname(ess->sc_dev));

	bus_space_write_1(ess->st, ess->sh, PORT_CODEC_CMD,
	    CODEC_CMD_READ | regno);
	delay(21);	/* AC97 cycle = 20.8usec */

	/* Wait for data retrieve */
	for (t = 0; t < 20; t++) {
		if ((bus_space_read_1(ess->st, ess->sh, PORT_CODEC_STAT)
		    & CODEC_STAT_MASK) == CODEC_STAT_RW_DONE)
			break;
		delay(2);	/* 20.8us / 13 */
	}
	if (t == 20)
		/* Timed out, but perform dummy read. */
		printf("%s: esm_read_codec() RW_DONE timed out.\n",
		    device_xname(ess->sc_dev));

	*result = bus_space_read_2(ess->st, ess->sh, PORT_CODEC_REG);

	return 0;
}

int
esm_write_codec(void *sc, uint8_t regno, uint16_t data)
{
	struct esm_softc *ess;
	unsigned t;

	ess = sc;
	/* We have to wait for a SAFE time to write addr/data */
	for (t = 0; t < 20; t++) {
		if ((bus_space_read_1(ess->st, ess->sh, PORT_CODEC_STAT)
		    & CODEC_STAT_MASK) != CODEC_STAT_PROGLESS)
			break;
		delay(2);	/* 20.8us / 13 */
	}
	if (t == 20) {
		/* Timed out. Abort writing. */
		printf("%s: esm_write_codec() PROGLESS timed out.\n",
		    device_xname(ess->sc_dev));
		return -1;
	}

	bus_space_write_2(ess->st, ess->sh, PORT_CODEC_REG, data);
	bus_space_write_1(ess->st, ess->sh, PORT_CODEC_CMD,
	    CODEC_CMD_WRITE | regno);

	return 0;
}

/* -------------------------------------------------------------------- */

static inline void
ringbus_setdest(struct esm_softc *ess, int src, int dest)
{
	uint32_t data;

	data = bus_space_read_4(ess->st, ess->sh, PORT_RINGBUS_CTRL);
	data &= ~(0xfU << src);
	data |= (0xfU & dest) << src;
	bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL, data);
}

/* Wave Processor */

static inline uint16_t
wp_rdreg(struct esm_softc *ess, uint16_t reg)
{

	bus_space_write_2(ess->st, ess->sh, PORT_DSP_INDEX, reg);
	return bus_space_read_2(ess->st, ess->sh, PORT_DSP_DATA);
}

static inline void
wp_wrreg(struct esm_softc *ess, uint16_t reg, uint16_t data)
{

	bus_space_write_2(ess->st, ess->sh, PORT_DSP_INDEX, reg);
	bus_space_write_2(ess->st, ess->sh, PORT_DSP_DATA, data);
}

static inline void
apu_setindex(struct esm_softc *ess, uint16_t reg)
{
	int t;

	wp_wrreg(ess, WPREG_CRAM_PTR, reg);
	/* Sometimes WP fails to set apu register index. */
	for (t = 0; t < 1000; t++) {
		if (bus_space_read_2(ess->st, ess->sh, PORT_DSP_DATA) == reg)
			break;
		bus_space_write_2(ess->st, ess->sh, PORT_DSP_DATA, reg);
	}
	if (t == 1000)
		printf("%s: apu_setindex() timed out.\n", device_xname(ess->sc_dev));
}

static inline uint16_t
wp_rdapu(struct esm_softc *ess, int ch, uint16_t reg)
{
	uint16_t ret;

	apu_setindex(ess, ((unsigned)ch << 4) + reg);
	ret = wp_rdreg(ess, WPREG_DATA_PORT);
	return ret;
}

static inline void
wp_wrapu(struct esm_softc *ess, int ch, uint16_t reg, uint16_t data)
{
	int t;

	DPRINTF(ESM_DEBUG_APU,
	    ("wp_wrapu(%p, ch=%d, reg=0x%x, data=0x%04x)\n",
	    ess, ch, reg, data));

	apu_setindex(ess, ((unsigned)ch << 4) + reg);
	wp_wrreg(ess, WPREG_DATA_PORT, data);
	for (t = 0; t < 1000; t++) {
		if (bus_space_read_2(ess->st, ess->sh, PORT_DSP_DATA) == data)
			break;
		bus_space_write_2(ess->st, ess->sh, PORT_DSP_DATA, data);
	}
	if (t == 1000)
		printf("%s: wp_wrapu() timed out.\n", device_xname(ess->sc_dev));
}

static inline void
wp_settimer(struct esm_softc *ess, u_int freq)
{
	u_int clock;
	u_int prescale, divide;

	clock = 48000 << 2;
	prescale = 0;
	divide = (freq != 0) ? (clock / freq) : ~0;
	RANGE(divide, WPTIMER_MINDIV, WPTIMER_MAXDIV);

	for (; divide > 32 << 1; divide >>= 1)
		prescale++;
	divide = (divide + 1) >> 1;

	for (; prescale < 7 && divide > 2 && !(divide & 1); divide >>= 1)
		prescale++;

	DPRINTF(ESM_DEBUG_TIMER,
	    ("wp_settimer(%p, %u): clock = %u, prescale = %u, divide = %u\n",
	    ess, freq, clock, prescale, divide));

	wp_wrreg(ess, WPREG_TIMER_ENABLE, 0);
	wp_wrreg(ess, WPREG_TIMER_FREQ,
	    (prescale << WP_TIMER_FREQ_PRESCALE_SHIFT) | (divide - 1));
	wp_wrreg(ess, WPREG_TIMER_ENABLE, 1);
}

static inline void
wp_starttimer(struct esm_softc *ess)
{

	wp_wrreg(ess, WPREG_TIMER_START, 1);
}

static inline void
wp_stoptimer(struct esm_softc *ess)
{

	wp_wrreg(ess, WPREG_TIMER_START, 0);
	bus_space_write_2(ess->st, ess->sh, PORT_INT_STAT, 1);
}

/* WaveCache */

static inline void
wc_wrreg(struct esm_softc *ess, uint16_t reg, uint16_t data)
{

	bus_space_write_2(ess->st, ess->sh, PORT_WAVCACHE_INDEX, reg);
	bus_space_write_2(ess->st, ess->sh, PORT_WAVCACHE_DATA, data);
}

static inline void
wc_wrchctl(struct esm_softc *ess, int ch, uint16_t data)
{

	wc_wrreg(ess, ch << 3, data);
}

/* -----------------------------
 * Controller.
 */

int
esm_attach_codec(void *sc, struct ac97_codec_if *codec_if)
{
	struct esm_softc *ess;

	ess = sc;
	ess->codec_if = codec_if;

	return 0;
}

int
esm_reset_codec(void *sc)
{

	return 0;
}


enum ac97_host_flags
esm_flags_codec(void *sc)
{
	struct esm_softc *ess;

	ess = sc;
	return ess->codec_flags;
}


void
esm_initcodec(struct esm_softc *ess)
{
	uint16_t data;

	DPRINTF(ESM_DEBUG_CODEC, ("esm_initcodec(%p)\n", ess));

	if (bus_space_read_4(ess->st, ess->sh, PORT_RINGBUS_CTRL)
	    & RINGBUS_CTRL_ACLINK_ENABLED) {
		bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL, 0);
		delay(104);	/* 20.8us * (4 + 1) */
	}
	/* XXX - 2nd codec should be looked at. */
	bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL,
	    RINGBUS_CTRL_AC97_SWRESET);
	delay(2);
	bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL,
	    RINGBUS_CTRL_ACLINK_ENABLED);
	delay(21);

	esm_read_codec(ess, 0, &data);
	if (bus_space_read_1(ess->st, ess->sh, PORT_CODEC_STAT)
	    & CODEC_STAT_MASK) {
		bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL, 0);
		delay(21);

		/* Try cold reset. */
		printf("%s: will perform cold reset.\n", device_xname(ess->sc_dev));
		data = bus_space_read_2(ess->st, ess->sh, PORT_GPIO_DIR);
		if (pci_conf_read(ess->pc, ess->tag, 0x58) & 1)
			data |= 0x10;
		data |= 0x009 &
		    ~bus_space_read_2(ess->st, ess->sh, PORT_GPIO_DATA);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_MASK, 0xff6);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_DIR,
		    data | 0x009);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_DATA, 0x000);
		delay(2);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_DATA, 0x001);
		delay(1);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_DATA, 0x009);
		delay(500000);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_DIR, data);
		delay(84);	/* 20.8us * 4 */
		bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL,
		    RINGBUS_CTRL_ACLINK_ENABLED);
		delay(21);
	}
}

void
esm_init(struct esm_softc *ess)
{

	/* Reset direct sound. */
	bus_space_write_2(ess->st, ess->sh, PORT_HOSTINT_CTRL,
	    HOSTINT_CTRL_DSOUND_RESET);
	delay(10000);
	bus_space_write_2(ess->st, ess->sh, PORT_HOSTINT_CTRL, 0);
	delay(10000);

	/* Enable direct sound interruption. */
	bus_space_write_2(ess->st, ess->sh, PORT_HOSTINT_CTRL,
	    HOSTINT_CTRL_DSOUND_INT_ENABLED);

	/* Setup Wave Processor. */

	/* Enable WaveCache */
	wp_wrreg(ess, WPREG_WAVE_ROMRAM,
	    WP_WAVE_VIRTUAL_ENABLED | WP_WAVE_DRAM_ENABLED);
	bus_space_write_2(ess->st, ess->sh, PORT_WAVCACHE_CTRL,
	    WAVCACHE_ENABLED | WAVCACHE_WTSIZE_4MB);

	/* Setup Codec/Ringbus. */
	esm_initcodec(ess);
	bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL,
	    RINGBUS_CTRL_RINGBUS_ENABLED | RINGBUS_CTRL_ACLINK_ENABLED);

	/* Undocumented registers from the Linux driver. */
	wp_wrreg(ess, 0x8, 0xB004);
	wp_wrreg(ess, 0x9, 0x001B);
	wp_wrreg(ess, 0xA, 0x8000);
	wp_wrreg(ess, 0xB, 0x3F37);
	wp_wrreg(ess, 0xD, 0x7632);

	wp_wrreg(ess, WPREG_BASE, 0x8598);	/* Parallel I/O */
	ringbus_setdest(ess, RINGBUS_SRC_ADC,
	    RINGBUS_DEST_STEREO | RINGBUS_DEST_DSOUND_IN);
	ringbus_setdest(ess, RINGBUS_SRC_DSOUND,
	    RINGBUS_DEST_STEREO | RINGBUS_DEST_DAC);

	/* Setup ASSP. Needed for Dell Inspiron 7500? */
	bus_space_write_1(ess->st, ess->sh, PORT_ASSP_CTRL_B, 0x00);
	bus_space_write_1(ess->st, ess->sh, PORT_ASSP_CTRL_A, 0x03);
	bus_space_write_1(ess->st, ess->sh, PORT_ASSP_CTRL_C, 0x00);

	/*
	 * Setup GPIO.
	 * There seems to be speciality with NEC systems.
	 */
	if (esm_get_quirks(ess->subid) & ESM_QUIRKF_GPIO) {
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_MASK,
		    0x9ff);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_DIR,
		    bus_space_read_2(ess->st, ess->sh, PORT_GPIO_DIR) |
			0x600);
		bus_space_write_2(ess->st, ess->sh, PORT_GPIO_DATA,
		    0x200);
	}

	DUMPREG(ess);
}

/* Channel controller. */

int
esm_init_output (void *sc, void *start, int size)
{
	struct esm_softc *ess;
	struct esm_dma *p;

	ess = sc;
	p = &ess->sc_dma;
	if ((char *)start != (char *)p->addr + MAESTRO_PLAYBUF_OFF) {
		printf("%s: esm_init_output: bad addr %p\n",
		    device_xname(ess->sc_dev), start);
		return EINVAL;
	}

	ess->pch.base = DMAADDR(p) + MAESTRO_PLAYBUF_OFF;

	DPRINTF(ESM_DEBUG_DMA, ("%s: pch.base = 0x%x\n",
		device_xname(ess->sc_dev), ess->pch.base));

	return 0;
}

int
esm_init_input (void *sc, void *start, int size)
{
	struct esm_softc *ess;
	struct esm_dma *p;

	ess = sc;
	p = &ess->sc_dma;
	if ((char *)start != (char *)p->addr + MAESTRO_RECBUF_OFF) {
		printf("%s: esm_init_input: bad addr %p\n",
		    device_xname(ess->sc_dev), start);
		return EINVAL;
	}

	switch (ess->rch.aputype) {
	case APUTYPE_16BITSTEREO:
		ess->rch.base = DMAADDR(p) + MAESTRO_RECBUF_L_OFF;
		break;
	default:
		ess->rch.base = DMAADDR(p) + MAESTRO_RECBUF_OFF;
		break;
	}

	DPRINTF(ESM_DEBUG_DMA, ("%s: rch.base = 0x%x\n",
		device_xname(ess->sc_dev), ess->rch.base));

	return 0;
}

int
esm_trigger_output(void *sc, void *start, void *end, int blksize,
    void (*intr)(void *), void *arg, const audio_params_t *param)
{
	size_t size;
	struct esm_softc *ess;
	struct esm_chinfo *ch;
	struct esm_dma *p;
	int pan, choffset;
	int i, nch;
	unsigned speed, offset, wpwa, dv;
	uint16_t apuch;

	DPRINTF(ESM_DEBUG_DMA,
	    ("esm_trigger_output(%p, %p, %p, 0x%x, %p, %p, %p)\n",
	    sc, start, end, blksize, intr, arg, param));
	ess = sc;
	ch = &ess->pch;
	pan = 0;
	nch = 1;
	speed = ch->sample_rate;
	apuch = ch->num << 1;

#ifdef DIAGNOSTIC
	if (ess->pactive) {
		printf("%s: esm_trigger_output: already running",
		    device_xname(ess->sc_dev));
		return EINVAL;
	}
#endif

	ess->sc_pintr = intr;
	ess->sc_parg = arg;
	p = &ess->sc_dma;
	if ((char *)start != (char *)p->addr + MAESTRO_PLAYBUF_OFF) {
		printf("%s: esm_trigger_output: bad addr %p\n",
		    device_xname(ess->sc_dev), start);
		return EINVAL;
	}

	ess->pch.blocksize = blksize;
	ess->pch.apublk = blksize >> 1;
	ess->pactive = 1;

	size = (size_t)(((char *)end - (char *)start) >> 1);
	choffset = MAESTRO_PLAYBUF_OFF;
	offset = choffset >> 1;
	wpwa = APU_USE_SYSMEM | ((offset >> 8) & APU_64KPAGE_MASK);

	DPRINTF(ESM_DEBUG_DMA,
	    ("choffs=0x%x, wpwa=0x%x, size=0x%lx words\n",
	    choffset, wpwa, (unsigned long int)size));

	switch (ch->aputype) {
	case APUTYPE_16BITSTEREO:
		ess->pch.apublk >>= 1;
		wpwa >>= 1;
		size >>= 1;
		offset >>= 1;
		/* FALLTHROUGH */
	case APUTYPE_8BITSTEREO:
		if (ess->codec_flags & AC97_HOST_SWAPPED_CHANNELS)
			pan = 8;
		else
			pan = -8;
		nch++;
		break;
	case APUTYPE_8BITLINEAR:
		ess->pch.apublk <<= 1;
		speed >>= 1;
		break;
	}

	ess->pch.apubase = offset;
	ess->pch.apubuf = size;
	ess->pch.nextirq = ess->pch.apublk;

	set_timer(ess);
	wp_starttimer(ess);

	dv = (((speed % 48000) << 16) + 24000) / 48000
	    + ((speed / 48000) << 16);

	for (i = nch-1; i >= 0; i--) {
		wp_wrapu(ess, apuch + i, APUREG_WAVESPACE, wpwa & 0xff00);
		wp_wrapu(ess, apuch + i, APUREG_CURPTR, offset);
		wp_wrapu(ess, apuch + i, APUREG_ENDPTR, offset + size);
		wp_wrapu(ess, apuch + i, APUREG_LOOPLEN, size - 1);
		wp_wrapu(ess, apuch + i, APUREG_AMPLITUDE, 0xe800);
		wp_wrapu(ess, apuch + i, APUREG_POSITION, 0x8f00
		    | (RADIUS_CENTERCIRCLE << APU_RADIUS_SHIFT)
		    | ((PAN_FRONT + pan) << APU_PAN_SHIFT));
		wp_wrapu(ess, apuch + i, APUREG_FREQ_LOBYTE, APU_plus6dB
		    | ((dv & 0xff) << APU_FREQ_LOBYTE_SHIFT));
		wp_wrapu(ess, apuch + i, APUREG_FREQ_HIWORD, dv >> 8);

		if (ch->aputype == APUTYPE_16BITSTEREO)
			wpwa |= APU_STEREO >> 1;
		pan = -pan;
	}

	wc_wrchctl(ess, apuch, ch->wcreg_tpl);
	if (nch > 1)
		wc_wrchctl(ess, apuch + 1, ch->wcreg_tpl);

	wp_wrapu(ess, apuch, APUREG_APUTYPE,
	    (ch->aputype << APU_APUTYPE_SHIFT) | APU_DMA_ENABLED | 0xf);
	if (ch->wcreg_tpl & WAVCACHE_CHCTL_STEREO)
		wp_wrapu(ess, apuch + 1, APUREG_APUTYPE,
		    (ch->aputype << APU_APUTYPE_SHIFT) | APU_DMA_ENABLED | 0xf);

	return 0;
}

int
esm_trigger_input(void *sc, void *start, void *end, int blksize,
    void (*intr)(void *), void *arg, const audio_params_t *param)
{
	size_t size;
	size_t mixsize;
	struct esm_softc *ess;
	struct esm_chinfo *ch;
	struct esm_dma *p;
	uint32_t chctl, choffset;
	uint32_t speed, offset, wpwa, dv;
	uint32_t mixoffset, mixdv;
	int i, nch;
	uint16_t apuch;
	uint16_t reg;

	DPRINTF(ESM_DEBUG_DMA,
	    ("esm_trigger_input(%p, %p, %p, 0x%x, %p, %p, %p)\n",
	    sc, start, end, blksize, intr, arg, param));
	ess = sc;
	ch = &ess->rch;
	nch = 1;
	speed = ch->sample_rate;
	apuch = ch->num << 1;

#ifdef DIAGNOSTIC
	if (ess->ractive) {
		printf("%s: esm_trigger_input: already running",
		    device_xname(ess->sc_dev));
		return EINVAL;
	}
#endif

	ess->sc_rintr = intr;
	ess->sc_rarg = arg;
	p = &ess->sc_dma;
	if ((char *)start != (char *)p->addr + MAESTRO_RECBUF_OFF) {
		printf("%s: esm_trigger_input: bad addr %p\n",
		    device_xname(ess->sc_dev), start);
		return EINVAL;
	}

	ess->rch.buffer = (void *)start;
	ess->rch.offset = 0;
	ess->rch.blocksize = blksize;
	ess->rch.bufsize = ((char *)end - (char *)start);
	ess->rch.apublk = blksize >> 1;
	ess->ractive = 1;

	size = (size_t)(((char *)end - (char *)start) >> 1);
	choffset = MAESTRO_RECBUF_OFF;
	switch (ch->aputype) {
	case APUTYPE_16BITSTEREO:
		size >>= 1;
		choffset = MAESTRO_RECBUF_L_OFF;
		ess->rch.apublk >>= 1;
		nch++;
		break;
	case APUTYPE_16BITLINEAR:
		break;
	default:
		ess->ractive = 0;
		return EINVAL;
	}

	mixsize = (MAESTRO_MIXBUF_SZ >> 1) >> 1;
	mixoffset = MAESTRO_MIXBUF_OFF;

	ess->rch.apubase = (choffset >> 1);
	ess->rch.apubuf = size;
	ess->rch.nextirq = ess->rch.apublk;

	set_timer(ess);
	wp_starttimer(ess);

	if (speed > 47999) speed = 47999;
	if (speed < 4000) speed = 4000;
	dv = (((speed % 48000) << 16) + 24000) / 48000
	    + ((speed / 48000) << 16);
	mixdv = 65536;	/* 48 kHz */

	for (i = 0; i < nch; i++) {

		/* Clear all rate conversion WP channel registers first. */
		for (reg = 0; reg < 15; reg++)
			wp_wrapu(ess, apuch + i, reg, 0);

		/* Program the WaveCache for the rate conversion WP channel. */
		chctl = (DMAADDR(p) + choffset - 0x10) &
		    WAVCACHE_CHCTL_ADDRTAG_MASK;
		wc_wrchctl(ess, apuch + i, chctl);

		/* Program the rate conversion WP channel. */
		wp_wrapu(ess, apuch + i, APUREG_FREQ_LOBYTE, APU_plus6dB
		    | ((dv & 0xff) << APU_FREQ_LOBYTE_SHIFT) | 0x08);
		wp_wrapu(ess, apuch + i, APUREG_FREQ_HIWORD, dv >> 8);
		offset = choffset >> 1;
		wpwa = APU_USE_SYSMEM | ((offset >> 8) & APU_64KPAGE_MASK);
		wp_wrapu(ess, apuch + i, APUREG_WAVESPACE, wpwa);
		wp_wrapu(ess, apuch + i, APUREG_CURPTR, offset);
		wp_wrapu(ess, apuch + i, APUREG_ENDPTR, offset + size);
		wp_wrapu(ess, apuch + i, APUREG_LOOPLEN, size - 1);
		wp_wrapu(ess, apuch + i, APUREG_EFFECTS_ENV, 0x00f0);
		wp_wrapu(ess, apuch + i, APUREG_AMPLITUDE, 0xe800);
		wp_wrapu(ess, apuch + i, APUREG_POSITION, 0x8f00
		    | (RADIUS_CENTERCIRCLE << APU_RADIUS_SHIFT)
		    | (PAN_FRONT << APU_PAN_SHIFT));
		wp_wrapu(ess, apuch + i, APUREG_ROUTE, apuch + 2 + i);

		DPRINTF(ESM_DEBUG_DMA,
		    ("choffs=0x%x, wpwa=0x%x, offset=0x%x words, size=0x%lx words\n",
		    choffset, wpwa, offset, (unsigned long int)size));

		/* Clear all mixer WP channel registers first. */
		for (reg = 0; reg < 15; reg++)
			wp_wrapu(ess, apuch + 2 + i, reg, 0);

		/* Program the WaveCache for the mixer WP channel. */
		chctl = (ess->rch.base + mixoffset - 0x10) &
		    WAVCACHE_CHCTL_ADDRTAG_MASK;
		wc_wrchctl(ess, apuch + 2 + i, chctl);

		/* Program the mixer WP channel. */
		wp_wrapu(ess, apuch + 2 + i, APUREG_FREQ_LOBYTE, APU_plus6dB
		    | ((mixdv & 0xff) << APU_FREQ_LOBYTE_SHIFT) | 0x08);
		wp_wrapu(ess, apuch + 2 + i, APUREG_FREQ_HIWORD, mixdv >> 8);
		offset = mixoffset >> 1;
		wpwa = APU_USE_SYSMEM | ((offset >> 8) & APU_64KPAGE_MASK);
		wp_wrapu(ess, apuch + 2 + i, APUREG_WAVESPACE, wpwa);
		wp_wrapu(ess, apuch + 2 + i, APUREG_CURPTR, offset);
		wp_wrapu(ess, apuch + 2 + i, APUREG_ENDPTR,
		    offset + mixsize);
		wp_wrapu(ess, apuch + 2 + i, APUREG_LOOPLEN, mixsize);
		wp_wrapu(ess, apuch + 2 + i, APUREG_EFFECTS_ENV, 0x00f0);
		wp_wrapu(ess, apuch + 2 + i, APUREG_AMPLITUDE, 0xe800);
		wp_wrapu(ess, apuch + 2 + i, APUREG_POSITION, 0x8f00
		    | (RADIUS_CENTERCIRCLE << APU_RADIUS_SHIFT)
		    | (PAN_FRONT << APU_PAN_SHIFT));
		wp_wrapu(ess, apuch + 2 + i, APUREG_ROUTE,
		    ROUTE_PARALLEL + i);

		DPRINTF(ESM_DEBUG_DMA,
		    ("mixoffs=0x%x, wpwa=0x%x, offset=0x%x words, size=0x%lx words\n",
		    mixoffset, wpwa, offset, (unsigned long int)mixsize));

		/* Assume we're going to loop to do the right channel. */
		choffset += MAESTRO_RECBUF_L_SZ;
		mixoffset += MAESTRO_MIXBUF_SZ >> 1;
	}

	wp_wrapu(ess, apuch, APUREG_APUTYPE,
	    (APUTYPE_RATECONV << APU_APUTYPE_SHIFT) |
	    APU_DMA_ENABLED | 0xf);
	if (nch > 1)
		wp_wrapu(ess, apuch + 1, APUREG_APUTYPE,
		    (APUTYPE_RATECONV << APU_APUTYPE_SHIFT) |
		    APU_DMA_ENABLED | 0xf);
	wp_wrapu(ess, apuch + 2, APUREG_APUTYPE,
	    (APUTYPE_INPUTMIXER << APU_APUTYPE_SHIFT) |
	    APU_DMA_ENABLED | 0xf);
	if (nch > 1)
		wp_wrapu(ess, apuch + 3, APUREG_APUTYPE,
		    (APUTYPE_RATECONV << APU_APUTYPE_SHIFT) |
		    APU_DMA_ENABLED | 0xf);

	return 0;
}

int
esm_halt_output(void *sc)
{
	struct esm_softc *ess;
	struct esm_chinfo *ch;

	DPRINTF(ESM_DEBUG_PARAM, ("esm_halt_output(%p)\n", sc));
	ess = sc;
	ch = &ess->pch;

	wp_wrapu(ess, (ch->num << 1), APUREG_APUTYPE,
	    APUTYPE_INACTIVE << APU_APUTYPE_SHIFT);
	wp_wrapu(ess, (ch->num << 1) + 1, APUREG_APUTYPE,
	    APUTYPE_INACTIVE << APU_APUTYPE_SHIFT);

	ess->pactive = 0;
	if (!ess->ractive)
		wp_stoptimer(ess);

	return 0;
}

int
esm_halt_input(void *sc)
{
	struct esm_softc *ess;
	struct esm_chinfo *ch;

	DPRINTF(ESM_DEBUG_PARAM, ("esm_halt_input(%p)\n", sc));
	ess = sc;
	ch = &ess->rch;

	wp_wrapu(ess, (ch->num << 1), APUREG_APUTYPE,
	    APUTYPE_INACTIVE << APU_APUTYPE_SHIFT);
	wp_wrapu(ess, (ch->num << 1) + 1, APUREG_APUTYPE,
	    APUTYPE_INACTIVE << APU_APUTYPE_SHIFT);
	wp_wrapu(ess, (ch->num << 1) + 2, APUREG_APUTYPE,
	    APUTYPE_INACTIVE << APU_APUTYPE_SHIFT);
	wp_wrapu(ess, (ch->num << 1) + 3, APUREG_APUTYPE,
	    APUTYPE_INACTIVE << APU_APUTYPE_SHIFT);

	ess->ractive = 0;
	if (!ess->pactive)
		wp_stoptimer(ess);

	return 0;
}

static inline u_int
calc_timer_freq(struct esm_chinfo *ch)
{
	u_int freq;

	freq = (ch->sample_rate + ch->apublk - 1) / ch->apublk;

	DPRINTF(ESM_DEBUG_TIMER,
	    ("calc_timer_freq(%p): rate = %u, blk = 0x%x (0x%x): freq = %u\n",
	    ch, ch->sample_rate, ch->apublk, ch->blocksize, freq));

	return freq;
}

static void
set_timer(struct esm_softc *ess)
{
	unsigned freq, freq2;

	freq = 0;
	if (ess->pactive)
		freq = calc_timer_freq(&ess->pch);

	if (ess->ractive) {
		freq2 = calc_timer_freq(&ess->rch);
		if (freq2 > freq)
			freq = freq2;
	}

	KASSERT(freq != 0);

	for (; freq < MAESTRO_MINFREQ; freq <<= 1)
		continue;

	if (freq > 0)
		wp_settimer(ess, freq);
}

static void
esmch_set_format(struct esm_chinfo *ch, const audio_params_t *p)
{
	uint16_t wcreg_tpl;
	uint16_t aputype;

	wcreg_tpl = (ch->base - 16) & WAVCACHE_CHCTL_ADDRTAG_MASK;
	aputype = APUTYPE_16BITLINEAR;
	if (p->channels == 2) {
		wcreg_tpl |= WAVCACHE_CHCTL_STEREO;
		aputype++;
	}
	if (p->precision == 8) {
		aputype += 2;
		switch (p->encoding) {
		case AUDIO_ENCODING_ULINEAR:
		case AUDIO_ENCODING_ULINEAR_BE:
		case AUDIO_ENCODING_ULINEAR_LE:
			wcreg_tpl |= WAVCACHE_CHCTL_U8;
			break;
		}
	}
	ch->wcreg_tpl = wcreg_tpl;
	ch->aputype = aputype;
	ch->sample_rate = p->sample_rate;

	DPRINTF(ESM_DEBUG_PARAM, ("esmch_set_format: "
	    "numch=%u, prec=%u, tpl=0x%x, aputype=%d, rate=%u\n",
	    p->channels, p->precision, wcreg_tpl, aputype, p->sample_rate));
}

/*
 * Since we can't record in true stereo, this function combines
 * the separately recorded left and right channels into the final
 * buffer for the upper layer.
 */
static void
esmch_combine_input(struct esm_softc *ess, struct esm_chinfo *ch)
{
	size_t offset, resid, count;
	uint32_t *dst32s;
	const uint32_t *left32s, *right32s;
	uint32_t left32, right32;

	/* The current offset into the upper layer buffer. */
	offset = ch->offset;

	/* The number of bytes left to combine. */
	resid = ch->blocksize;

	while (resid > 0) {

		/* The 32-bit words for the left channel. */
		left32s = (const uint32_t *)((char *)ess->sc_dma.addr +
		    MAESTRO_RECBUF_L_OFF + offset / 2);

		/* The 32-bit words for the right channel. */
		right32s = (const uint32_t *)((char *)ess->sc_dma.addr +
		    MAESTRO_RECBUF_R_OFF + offset / 2);

		/* The pointer to the 32-bit words we will write. */
		dst32s = (uint32_t *)((char *)ch->buffer + offset);

		/* Get the number of bytes we will combine now. */
		count = ch->bufsize - offset;
		if (count > resid)
			count = resid;
		resid -= count;
		offset += count;
		if (offset == ch->bufsize)
			offset = 0;

		/* Combine, writing two 32-bit words at a time. */
		KASSERT((count & (sizeof(uint32_t) * 2 - 1)) == 0);
		count /= (sizeof(uint32_t) * 2);
		while (count > 0) {
			left32 = *(left32s++);
			right32 = *(right32s++);
			/* XXX this endian handling is half-baked at best */
#if BYTE_ORDER == LITTLE_ENDIAN
			*(dst32s++) = (left32 & 0xFFFF) | (right32 << 16);
			*(dst32s++) = (left32 >> 16) | (right32 & 0xFFFF0000);
#else  /* BYTE_ORDER == BIG_ENDIAN */
			*(dst32s++) = (left32 & 0xFFFF0000) | (right32 >> 16);
			*(dst32s++) = (left32 << 16) | (right32 & 0xFFFF);
#endif /* BYTE_ORDER == BIG_ENDIAN */
			count--;
		}
	}

	/* Update the offset. */
	ch->offset = offset;
}

/*
 * Audio interface glue functions
 */

int
esm_getdev (void *sc, struct audio_device *adp)
{

	*adp = esm_device;
	return 0;
}

int
esm_round_blocksize(void *sc, int blk, int mode,
    const audio_params_t *param)
{

	DPRINTF(ESM_DEBUG_PARAM,
	    ("esm_round_blocksize(%p, 0x%x)", sc, blk));

	blk &= ~0x3f;		/* keep good alignment */

	DPRINTF(ESM_DEBUG_PARAM, (" = 0x%x\n", blk));

	return blk;
}

int
esm_query_encoding(void *sc, struct audio_encoding *fp)
{

	DPRINTF(ESM_DEBUG_PARAM,
	    ("esm_query_encoding(%p, %d)\n", sc, fp->index));

	if (fp->index < 0 || fp->index >= MAESTRO_NENCODINGS)
		return EINVAL;

	*fp = esm_encoding[fp->index];
	return 0;
}

int
esm_set_params(void *sc, int setmode, int usemode,
	audio_params_t *play, audio_params_t *rec,
	stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
	struct esm_softc *ess;
	audio_params_t *p;
	const audio_params_t *hw_play, *hw_rec;
	stream_filter_list_t *fil;
	int mode, i;

	DPRINTF(ESM_DEBUG_PARAM,
	    ("esm_set_params(%p, 0x%x, 0x%x, %p, %p)\n",
	    sc, setmode, usemode, play, rec));
	ess = sc;
	hw_play = NULL;
	hw_rec = NULL;
	for (mode = AUMODE_RECORD; mode != -1;
	     mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;

		p = mode == AUMODE_PLAY ? play : rec;

		if (p->sample_rate < 4000 || p->sample_rate > 48000 ||
		    (p->precision != 8 && p->precision != 16) ||
		    (p->channels != 1 && p->channels != 2))
			return EINVAL;

		fil = mode == AUMODE_PLAY ? pfil : rfil;
		i = auconv_set_converter(esm_formats, ESM_NFORMATS,
					 mode, p, FALSE, fil);
		if (i < 0)
			return EINVAL;
		if (fil->req_size > 0)
			p = &fil->filters[0].param;
		if (mode == AUMODE_PLAY)
			hw_play = p;
		else
			hw_rec = p;
	}

	if (hw_play)
		esmch_set_format(&ess->pch, hw_play);

	if (hw_rec)
		esmch_set_format(&ess->rch, hw_rec);

	return 0;
}

int
esm_set_port(void *sc, mixer_ctrl_t *cp)
{
	struct esm_softc *ess;

	ess = sc;
	return ess->codec_if->vtbl->mixer_set_port(ess->codec_if, cp);
}

int
esm_get_port(void *sc, mixer_ctrl_t *cp)
{
	struct esm_softc *ess;

	ess = sc;
	return ess->codec_if->vtbl->mixer_get_port(ess->codec_if, cp);
}

int
esm_query_devinfo(void *sc, mixer_devinfo_t *dip)
{
	struct esm_softc *ess;

	ess = sc;
	return ess->codec_if->vtbl->query_devinfo(ess->codec_if, dip);
}

void *
esm_malloc(void *sc, int direction, size_t size)
{
	struct esm_softc *ess;
	int off;

	DPRINTF(ESM_DEBUG_DMA,
	    ("esm_malloc(%p, %d, 0x%zd)", sc, direction, size));
	ess = sc;
	/*
	 * Each buffer can only be allocated once.
	 */
	if (ess->rings_alloced & direction) {
		DPRINTF(ESM_DEBUG_DMA, (" = 0 (ENOMEM)\n"));
		return 0;
	}

	/*
	 * Mark this buffer as allocated and return its
	 * kernel virtual address.
	 */
	ess->rings_alloced |= direction;
	off = (direction == AUMODE_PLAY ?
		MAESTRO_PLAYBUF_OFF : MAESTRO_RECBUF_OFF);
	DPRINTF(ESM_DEBUG_DMA, (" = %p (DMAADDR 0x%x)\n",
				(char *)ess->sc_dma.addr + off,
				(int)DMAADDR(&ess->sc_dma) + off));
	return (char *)ess->sc_dma.addr + off;
}

void
esm_free(void *sc, void *ptr, size_t size)
{
	struct esm_softc *ess;

	DPRINTF(ESM_DEBUG_DMA, ("esm_free(%p, %p, %zd)\n", sc, ptr, size));
	ess = sc;
	if ((char *)ptr == (char *)ess->sc_dma.addr + MAESTRO_PLAYBUF_OFF)
		ess->rings_alloced &= ~AUMODE_PLAY;
	else if ((char *)ptr == (char *)ess->sc_dma.addr + MAESTRO_RECBUF_OFF)
		ess->rings_alloced &= ~AUMODE_RECORD;
}

size_t
esm_round_buffersize(void *sc, int direction, size_t size)
{

	if (size > MAESTRO_PLAYBUF_SZ)
		size = MAESTRO_PLAYBUF_SZ;
	if (size > MAESTRO_RECBUF_SZ)
		size = MAESTRO_RECBUF_SZ;
	return size;
}

paddr_t
esm_mappage(void *sc, void *mem, off_t off, int prot)
{
	struct esm_softc *ess;

	DPRINTF(ESM_DEBUG_DMA,
	    ("esm_mappage(%p, %p, 0x%lx, 0x%x)\n",
	    sc, mem, (unsigned long)off, prot));
	ess = sc;
	if (off < 0)
		return -1;

	if ((char *)mem == (char *)ess->sc_dma.addr + MAESTRO_PLAYBUF_OFF)
		off += MAESTRO_PLAYBUF_OFF;
	else if ((char *)mem == (char *)ess->sc_dma.addr + MAESTRO_RECBUF_OFF)
		off += MAESTRO_RECBUF_OFF;
	else
		return -1;
	return bus_dmamem_mmap(ess->dmat, ess->sc_dma.segs, ess->sc_dma.nsegs,
	    off, prot, BUS_DMA_WAITOK);
}

int
esm_get_props(void *sc)
{

	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
}


/* -----------------------------
 * Bus space.
 */

static int
esm_intr(void *sc)
{
	struct esm_softc *ess;
	uint16_t status;
	uint16_t pos;
	int ret;

	ess = sc;
	ret = 0;

	mutex_spin_enter(&ess->sc_intr_lock);
	status = bus_space_read_1(ess->st, ess->sh, PORT_HOSTINT_STAT);
	if (!status) {
		mutex_spin_exit(&ess->sc_intr_lock);
		return 0;
	}

	/* Acknowledge all. */
	bus_space_write_2(ess->st, ess->sh, PORT_INT_STAT, 1);
	bus_space_write_1(ess->st, ess->sh, PORT_HOSTINT_STAT, 0);
#if 0	/* XXX - HWVOL */
	if (status & HOSTINT_STAT_HWVOL) {
		u_int delta;
		delta = bus_space_read_1(ess->st, ess->sh, PORT_HWVOL_MASTER)
		    - 0x88;
		if (delta & 0x11)
			mixer_set(device_get_softc(ess->dev),
			    SOUND_MIXER_VOLUME, 0);
		else {
			mixer_set(device_get_softc(ess->dev),
			    SOUND_MIXER_VOLUME,
			    mixer_get(device_get_softc(ess->dev),
				SOUND_MIXER_VOLUME)
			    + ((delta >> 5) & 0x7) - 4
			    + ((delta << 7) & 0x700) - 0x400);
		}
		bus_space_write_1(ess->st, ess->sh, PORT_HWVOL_MASTER, 0x88);
		ret++;
	}
#endif	/* XXX - HWVOL */

	if (ess->pactive) {
		pos = wp_rdapu(ess, ess->pch.num << 1, APUREG_CURPTR);

		DPRINTF(ESM_DEBUG_IRQ, (" %4.4x/%4.4x ", pos,
		    wp_rdapu(ess, (ess->pch.num<<1)+1, APUREG_CURPTR)));

		pos -= ess->pch.apubase;
		if (pos >= ess->pch.nextirq &&
		    pos - ess->pch.nextirq < ess->pch.apubuf / 2) {
			ess->pch.nextirq += ess->pch.apublk;

			if (ess->pch.nextirq >= ess->pch.apubuf)
				ess->pch.nextirq = 0;

			if (ess->sc_pintr) {
				DPRINTF(ESM_DEBUG_IRQ, ("P\n"));
				ess->sc_pintr(ess->sc_parg);
			}

		}
		ret++;
	}

	if (ess->ractive) {
		pos = wp_rdapu(ess, ess->rch.num << 1, APUREG_CURPTR);

		DPRINTF(ESM_DEBUG_IRQ, (" %4.4x/%4.4x ", pos,
		    wp_rdapu(ess, (ess->rch.num<<1)+1, APUREG_CURPTR)));

		pos -= ess->rch.apubase;
		if (pos >= ess->rch.nextirq &&
		    pos - ess->rch.nextirq < ess->rch.apubuf / 2) {
			ess->rch.nextirq += ess->rch.apublk;

			if (ess->rch.nextirq >= ess->rch.apubuf)
				ess->rch.nextirq = 0;

			if (ess->sc_rintr) {
				DPRINTF(ESM_DEBUG_IRQ, ("R\n"));
				switch(ess->rch.aputype) {
				case APUTYPE_16BITSTEREO:
					esmch_combine_input(ess, &ess->rch);
					break;
				}
				ess->sc_rintr(ess->sc_rarg);
			}

		}
		ret++;
	}
	mutex_spin_exit(&ess->sc_intr_lock);

	return ret;
}

static void
esm_freemem(struct esm_softc *sc, struct esm_dma *p)
{
	if (p->size == 0)
		return;

	bus_dmamem_free(sc->dmat, p->segs, p->nsegs);

	bus_dmamem_unmap(sc->dmat, p->addr, p->size);

	bus_dmamap_destroy(sc->dmat, p->map);

	bus_dmamap_unload(sc->dmat, p->map);

	p->size = 0;
}

static int
esm_allocmem(struct esm_softc *sc, size_t size, size_t align,
    struct esm_dma *p)
{
	int error;

	p->size = size;
	error = bus_dmamem_alloc(sc->dmat, p->size, align, 0,
				 p->segs, __arraycount(p->segs),
				 &p->nsegs, BUS_DMA_WAITOK);
	if (error)
		return error;

	error = bus_dmamem_map(sc->dmat, p->segs, p->nsegs, p->size,
			       &p->addr, BUS_DMA_WAITOK|BUS_DMA_COHERENT);
	if (error)
		goto free;

	error = bus_dmamap_create(sc->dmat, p->size, 1, p->size,
				  0, BUS_DMA_WAITOK, &p->map);
	if (error)
		goto unmap;

	error = bus_dmamap_load(sc->dmat, p->map, p->addr, p->size, NULL,
				BUS_DMA_WAITOK);
	if (error)
		goto destroy;

	return 0;

 destroy:
	bus_dmamap_destroy(sc->dmat, p->map);
 unmap:
	bus_dmamem_unmap(sc->dmat, p->addr, p->size);
 free:
	bus_dmamem_free(sc->dmat, p->segs, p->nsegs);

	p->size = 0;
	return error;
}

static int
esm_match(device_t dev, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = (struct pci_attach_args *)aux;
	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_ESSTECH:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ESSTECH_MAESTRO1:
		case PCI_PRODUCT_ESSTECH_MAESTRO2:
		case PCI_PRODUCT_ESSTECH_MAESTRO2E:
			return 1;
		}

	case PCI_VENDOR_ESSTECH2:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ESSTECH2_MAESTRO1:
			return 1;
		}
	}
	return 0;
}

static void
esm_attach(device_t parent, device_t self, void *aux)
{
	struct esm_softc *ess;
	struct pci_attach_args *pa;
	const char *intrstr;
	pci_chipset_tag_t pc;
	pcitag_t tag;
	pci_intr_handle_t ih;
	pcireg_t csr, data;
	uint16_t codec_data;
	uint16_t pcmbar;
	int error;
	char intrbuf[PCI_INTRSTR_LEN];

	ess = device_private(self);
	ess->sc_dev = self;
	pa = (struct pci_attach_args *)aux;
	pc = pa->pa_pc;
	tag = pa->pa_tag;

	pci_aprint_devinfo(pa, "Audio controller");

	mutex_init(&ess->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&ess->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	/* Enable the device. */
	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_IO_ENABLE);

	/* Map I/O register */
	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
	    &ess->st, &ess->sh, NULL, &ess->sz)) {
		aprint_error_dev(ess->sc_dev, "can't map i/o space\n");
		mutex_destroy(&ess->sc_lock);
		mutex_destroy(&ess->sc_intr_lock);
		return;
	}

	/* Initialize softc */
	ess->pch.num = 0;
	ess->rch.num = 1;
	ess->dmat = pa->pa_dmat;
	ess->tag = tag;
	ess->pc = pc;
	ess->subid = pci_conf_read(pc, tag, PCI_SUBSYS_ID_REG);

	DPRINTF(ESM_DEBUG_PCI,
	    ("%s: sub-system vendor 0x%4.4x, product 0x%4.4x\n",
	    device_xname(ess->sc_dev),
	    PCI_VENDOR(ess->subid), PCI_PRODUCT(ess->subid)));

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(ess->sc_dev, "can't map interrupt\n");
		mutex_destroy(&ess->sc_lock);
		mutex_destroy(&ess->sc_intr_lock);
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	ess->ih = pci_intr_establish(pc, ih, IPL_AUDIO, esm_intr, self);
	if (ess->ih == NULL) {
		aprint_error_dev(ess->sc_dev, "can't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		mutex_destroy(&ess->sc_lock);
		mutex_destroy(&ess->sc_intr_lock);
		return;
	}
	aprint_normal_dev(ess->sc_dev, "interrupting at %s\n",
	    intrstr);

	/*
	 * Setup PCI config registers
	 */

	/* power up chip */
	if ((error = pci_activate(pa->pa_pc, pa->pa_tag, self,
	    pci_activate_null)) && error != EOPNOTSUPP) {
		aprint_error_dev(ess->sc_dev, "cannot activate %d\n",
		    error);
		mutex_destroy(&ess->sc_lock);
		mutex_destroy(&ess->sc_intr_lock);
		return;
	}
	delay(100000);

	/* Disable all legacy emulations. */
	data = pci_conf_read(pc, tag, CONF_LEGACY);
	pci_conf_write(pc, tag, CONF_LEGACY, data | LEGACY_DISABLED);

	/* Disconnect from CHI. (Makes Dell inspiron 7500 work?)
	 * Enable posted write.
	 * Prefer PCI timing rather than that of ISA.
	 * Don't swap L/R. */
	data = pci_conf_read(pc, tag, CONF_MAESTRO);
	data |= MAESTRO_CHIBUS | MAESTRO_POSTEDWRITE | MAESTRO_DMA_PCITIMING;
	data &= ~MAESTRO_SWAP_LR;
	pci_conf_write(pc, tag, CONF_MAESTRO, data);

	/* initialize sound chip */
	esm_init(ess);

	esm_read_codec(ess, 0, &codec_data);
	if (codec_data == 0x80) {
		aprint_error_dev(ess->sc_dev, "PT101 codec detected!\n");
		mutex_destroy(&ess->sc_lock);
		mutex_destroy(&ess->sc_intr_lock);
		return;
	}

	/*
	 * Some cards and Notebooks appear to have left and right channels
	 * reversed.  Check if there is a corresponding quirk entry for
	 * the subsystem vendor and product and if so, set the appropriate
	 * codec flag.
	 */
	if (esm_get_quirks(ess->subid) & ESM_QUIRKF_SWAPPEDCH) {
		ess->codec_flags |= AC97_HOST_SWAPPED_CHANNELS;
	}
	ess->codec_flags |= AC97_HOST_DONT_READ;

	/* initialize AC97 host interface */
	ess->host_if.arg = self;
	ess->host_if.attach = esm_attach_codec;
	ess->host_if.read = esm_read_codec;
	ess->host_if.write = esm_write_codec;
	ess->host_if.reset = esm_reset_codec;
	ess->host_if.flags = esm_flags_codec;

	if (ac97_attach(&ess->host_if, self, &ess->sc_lock) != 0) {
		mutex_destroy(&ess->sc_lock);
		mutex_destroy(&ess->sc_intr_lock);
		return;
	}

	/* allocate our DMA region */
	if (esm_allocmem(ess, MAESTRO_DMA_SZ, MAESTRO_DMA_ALIGN,
		&ess->sc_dma)) {
		aprint_error_dev(ess->sc_dev, "couldn't allocate memory!\n");
		mutex_destroy(&ess->sc_lock);
		mutex_destroy(&ess->sc_intr_lock);
		return;
	}
	ess->rings_alloced = 0;

	/* set DMA base address */
	for (pcmbar = WAVCACHE_PCMBAR; pcmbar < WAVCACHE_PCMBAR + 4; pcmbar++)
		wc_wrreg(ess, pcmbar,
		    DMAADDR(&ess->sc_dma) >> WAVCACHE_BASEADDR_SHIFT);

	audio_attach_mi(&esm_hw_if, self, ess->sc_dev);

	if (!pmf_device_register(self, esm_suspend, esm_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static void
esm_childdet(device_t self, device_t child)
{
	/* we hold no child references, so do nothing */
}

static int
esm_detach(device_t self, int flags)
{
	int rc;
	struct esm_softc *ess = device_private(self);

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;
	pmf_device_deregister(self);

	/* free our DMA region */
	esm_freemem(ess, &ess->sc_dma);

	if (ess->codec_if != NULL) {
		mutex_enter(&ess->sc_lock);
		ess->codec_if->vtbl->detach(ess->codec_if);
		mutex_exit(&ess->sc_lock);
	}

	/* XXX Restore CONF_MAESTRO? */
	/* XXX Restore legacy emulations? */
	/* XXX Restore PCI config registers? */

	if (ess->ih != NULL)
		pci_intr_disestablish(ess->pc, ess->ih);

	bus_space_unmap(ess->st, ess->sh, ess->sz);
	mutex_destroy(&ess->sc_lock);
	mutex_destroy(&ess->sc_intr_lock);

	return 0;
}

static bool
esm_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct esm_softc *ess = device_private(dv);

	mutex_enter(&ess->sc_lock);
	mutex_spin_enter(&ess->sc_intr_lock);
	wp_stoptimer(ess);
	bus_space_write_2(ess->st, ess->sh, PORT_HOSTINT_CTRL, 0);
	esm_halt_output(ess);
	esm_halt_input(ess);
	mutex_spin_exit(&ess->sc_intr_lock);

	/* Power down everything except clock. */
	esm_write_codec(ess, AC97_REG_POWER, 0xdf00);
	delay(20);
	bus_space_write_4(ess->st, ess->sh, PORT_RINGBUS_CTRL, 0);
	delay(1);
	mutex_exit(&ess->sc_lock);

	return true;
}

static bool
esm_resume(device_t dv, const pmf_qual_t *qual)
{
	struct esm_softc *ess = device_private(dv);
	uint16_t pcmbar;

	delay(100000);

	mutex_enter(&ess->sc_lock);
	mutex_spin_enter(&ess->sc_intr_lock);
	esm_init(ess);

	/* set DMA base address */
	for (pcmbar = WAVCACHE_PCMBAR; pcmbar < WAVCACHE_PCMBAR + 4; pcmbar++)
		wc_wrreg(ess, pcmbar,
		    DMAADDR(&ess->sc_dma) >> WAVCACHE_BASEADDR_SHIFT);
	mutex_spin_exit(&ess->sc_intr_lock);
	ess->codec_if->vtbl->restore_ports(ess->codec_if);
	mutex_spin_enter(&ess->sc_intr_lock);
#if 0
	if (mixer_reinit(dev)) {
		printf("%s: unable to reinitialize the mixer\n",
		    device_xname(ess->sc_dev));
		return ENXIO;
	}
#endif

#if TODO
	if (ess->pactive)
		esm_start_output(ess);
	if (ess->ractive)
		esm_start_input(ess);
#endif
	if (ess->pactive || ess->ractive) {
		set_timer(ess);
		wp_starttimer(ess);
	}
	mutex_spin_exit(&ess->sc_intr_lock);
	mutex_exit(&ess->sc_lock);

	return true;
}

void
esm_get_locks(void *addr, kmutex_t **intr, kmutex_t **proc)
{
	struct esm_softc *esm;

	esm = addr;
	*intr = &esm->sc_intr_lock;
	*proc = &esm->sc_lock;
}
