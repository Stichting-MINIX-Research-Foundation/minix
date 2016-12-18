/*	$NetBSD: ess.c,v 1.82 2014/08/16 13:01:33 nakayama Exp $	*/

/*
 * Copyright 1997
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

/*
**++
**
**  ess.c
**
**  FACILITY:
**
**	DIGITAL Network Appliance Reference Design (DNARD)
**
**  MODULE DESCRIPTION:
**
**      This module contains the device driver for the ESS
**      Technologies 1888/1887/888 sound chip. The code in sbdsp.c was
**	used as a reference point when implementing this driver.
**
**  AUTHORS:
**
**	Blair Fidler	Software Engineering Australia
**			Gold Coast, Australia.
**
**  CREATION DATE:
**
**	March 10, 1997.
**
**  MODIFICATION HISTORY:
**
**	Heavily modified by Lennart Augustsson and Charles M. Hannum for
**	bus_dma, changes to audio interface, and many bug fixes.
**	ESS1788 support by Nathan J. Williams and Charles M. Hannum.
**--
*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ess.c,v 1.82 2014/08/16 13:01:33 nakayama Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/cpu.h>
#include <sys/intr.h>
#include <sys/bus.h>
#include <sys/audioio.h>
#include <sys/malloc.h>

#include <dev/audio_if.h>
#include <dev/auconv.h>
#include <dev/mulaw.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isa/essvar.h>
#include <dev/isa/essreg.h>

#include "joy_ess.h"

#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (essdebug) printf x
#define DPRINTFN(n,x)	if (essdebug>(n)) printf x
int	essdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#if 0
unsigned uuu;
#define EREAD1(t, h, a) (uuu=bus_space_read_1(t, h, a),printf("EREAD  %02x=%02x\n", ((int)h&0xfff)+a, uuu),uuu)
#define EWRITE1(t, h, a, d) (printf("EWRITE %02x=%02x\n", ((int)h & 0xfff)+a, d), bus_space_write_1(t, h, a, d))
#else
#define EREAD1(t, h, a) bus_space_read_1(t, h, a)
#define EWRITE1(t, h, a, d) bus_space_write_1(t, h, a, d)
#endif


int	ess_setup_sc(struct ess_softc *, int);

int	ess_open(void *, int);
void	ess_close(void *);
int	ess_getdev(void *, struct audio_device *);
int	ess_drain(void *);

int	ess_query_encoding(void *, struct audio_encoding *);

int	ess_set_params(void *, int, int, audio_params_t *,
	    audio_params_t *, stream_filter_list_t *, stream_filter_list_t *);

int	ess_round_blocksize(void *, int, int, const audio_params_t *);

int	ess_audio1_trigger_output(void *, void *, void *, int,
	    void (*)(void *), void *, const audio_params_t *);
int	ess_audio2_trigger_output(void *, void *, void *, int,
	    void (*)(void *), void *, const audio_params_t *);
int	ess_audio1_trigger_input(void *, void *, void *, int,
	    void (*)(void *), void *, const audio_params_t *);
int	ess_audio1_halt(void *);
int	ess_audio2_halt(void *);
int	ess_audio1_intr(void *);
int	ess_audio2_intr(void *);
void	ess_audio1_poll(void *);
void	ess_audio2_poll(void *);

int	ess_speaker_ctl(void *, int);

int	ess_getdev(void *, struct audio_device *);

int	ess_set_port(void *, mixer_ctrl_t *);
int	ess_get_port(void *, mixer_ctrl_t *);

void   *ess_malloc(void *, int, size_t);
void	ess_free(void *, void *, size_t);
size_t	ess_round_buffersize(void *, int, size_t);
paddr_t	ess_mappage(void *, void *, off_t, int);


int	ess_query_devinfo(void *, mixer_devinfo_t *);
int	ess_1788_get_props(void *);
int	ess_1888_get_props(void *);
void	ess_get_locks(void *, kmutex_t **, kmutex_t **);

void	ess_speaker_on(struct ess_softc *);
void	ess_speaker_off(struct ess_softc *);

void	ess_config_irq(struct ess_softc *);
void	ess_config_drq(struct ess_softc *);
void	ess_setup(struct ess_softc *);
int	ess_identify(struct ess_softc *);

int	ess_reset(struct ess_softc *);
void	ess_set_gain(struct ess_softc *, int, int);
int	ess_set_in_port(struct ess_softc *, int);
int	ess_set_in_ports(struct ess_softc *, int);
u_int	ess_srtotc(struct ess_softc *, u_int);
u_int	ess_srtofc(u_int);
u_char	ess_get_dsp_status(struct ess_softc *);
u_char	ess_dsp_read_ready(struct ess_softc *);
u_char	ess_dsp_write_ready(struct ess_softc *);
int	ess_rdsp(struct ess_softc *);
int	ess_wdsp(struct ess_softc *, u_char);
u_char	ess_read_x_reg(struct ess_softc *, u_char);
int	ess_write_x_reg(struct ess_softc *, u_char, u_char);
void	ess_clear_xreg_bits(struct ess_softc *, u_char, u_char);
void	ess_set_xreg_bits(struct ess_softc *, u_char, u_char);
u_char	ess_read_mix_reg(struct ess_softc *, u_char);
void	ess_write_mix_reg(struct ess_softc *, u_char, u_char);
void	ess_clear_mreg_bits(struct ess_softc *, u_char, u_char);
void	ess_set_mreg_bits(struct ess_softc *, u_char, u_char);
void	ess_read_multi_mix_reg(struct ess_softc *, u_char, u_int8_t *, bus_size_t);

static const char *essmodel[] = {
	"unsupported",

	"688",
	"1688",
	"1788",
	"1868",
	"1869",
	"1878",
	"1879",

	"888",
	"1887",
	"1888",
};

struct audio_device ess_device = {
	"ESS Technology",
	"x",
	"ess"
};

/*
 * Define our interface to the higher level audio driver.
 */

const struct audio_hw_if ess_1788_hw_if = {
	ess_open,
	ess_close,
	ess_drain,
	ess_query_encoding,
	ess_set_params,
	ess_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	ess_audio1_halt,
	ess_audio1_halt,
	ess_speaker_ctl,
	ess_getdev,
	NULL,
	ess_set_port,
	ess_get_port,
	ess_query_devinfo,
	ess_malloc,
	ess_free,
	ess_round_buffersize,
	ess_mappage,
	ess_1788_get_props,
	ess_audio1_trigger_output,
	ess_audio1_trigger_input,
	NULL,
	ess_get_locks,
};

const struct audio_hw_if ess_1888_hw_if = {
	ess_open,
	ess_close,
	ess_drain,
	ess_query_encoding,
	ess_set_params,
	ess_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	ess_audio2_halt,
	ess_audio1_halt,
	ess_speaker_ctl,
	ess_getdev,
	NULL,
	ess_set_port,
	ess_get_port,
	ess_query_devinfo,
	ess_malloc,
	ess_free,
	ess_round_buffersize,
	ess_mappage,
	ess_1888_get_props,
	ess_audio2_trigger_output,
	ess_audio1_trigger_input,
	NULL,
	ess_get_locks,
};

#define ESS_NFORMATS	8
static const struct audio_format ess_formats[ESS_NFORMATS] = {
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 0, {ESS_MINRATE, ESS_MAXRATE}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 1, AUFMT_MONAURAL, 0, {ESS_MINRATE, ESS_MAXRATE}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 0, {ESS_MINRATE, ESS_MAXRATE}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 16, 16,
	 1, AUFMT_MONAURAL, 0, {ESS_MINRATE, ESS_MAXRATE}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 2, AUFMT_STEREO, 0, {ESS_MINRATE, ESS_MAXRATE}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 1, AUFMT_MONAURAL, 0, {ESS_MINRATE, ESS_MAXRATE}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 8, 8,
	 2, AUFMT_STEREO, 0, {ESS_MINRATE, ESS_MAXRATE}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 8, 8,
	 1, AUFMT_MONAURAL, 0, {ESS_MINRATE, ESS_MAXRATE}},
};

#ifdef AUDIO_DEBUG
void ess_printsc(struct ess_softc *);
void ess_dump_mixer(struct ess_softc *);

void
ess_printsc(struct ess_softc *sc)
{
	int i;

	printf("iobase 0x%x outport %u inport %u speaker %s\n",
	       sc->sc_iobase, sc->out_port,
	       sc->in_port, sc->spkr_state ? "on" : "off");

	printf("audio1: DMA chan %d irq %d nintr %lu intr %p arg %p\n",
	       sc->sc_audio1.drq, sc->sc_audio1.irq, sc->sc_audio1.nintr,
	       sc->sc_audio1.intr, sc->sc_audio1.arg);

	if (!ESS_USE_AUDIO1(sc->sc_model)) {
		printf("audio2: DMA chan %d irq %d nintr %lu intr %p arg %p\n",
		       sc->sc_audio2.drq, sc->sc_audio2.irq, sc->sc_audio2.nintr,
		       sc->sc_audio2.intr, sc->sc_audio2.arg);
	}

	printf("gain:");
	for (i = 0; i < sc->ndevs; i++)
		printf(" %u,%u", sc->gain[i][ESS_LEFT], sc->gain[i][ESS_RIGHT]);
	printf("\n");
}

void
ess_dump_mixer(struct ess_softc *sc)
{

	printf("ESS_DAC_PLAY_VOL: mix reg 0x%02x=0x%02x\n",
	       0x7C, ess_read_mix_reg(sc, 0x7C));
	printf("ESS_MIC_PLAY_VOL: mix reg 0x%02x=0x%02x\n",
	       0x1A, ess_read_mix_reg(sc, 0x1A));
	printf("ESS_LINE_PLAY_VOL: mix reg 0x%02x=0x%02x\n",
	       0x3E, ess_read_mix_reg(sc, 0x3E));
	printf("ESS_SYNTH_PLAY_VOL: mix reg 0x%02x=0x%02x\n",
	       0x36, ess_read_mix_reg(sc, 0x36));
	printf("ESS_CD_PLAY_VOL: mix reg 0x%02x=0x%02x\n",
	       0x38, ess_read_mix_reg(sc, 0x38));
	printf("ESS_AUXB_PLAY_VOL: mix reg 0x%02x=0x%02x\n",
	       0x3A, ess_read_mix_reg(sc, 0x3A));
	printf("ESS_MASTER_VOL: mix reg 0x%02x=0x%02x\n",
	       0x32, ess_read_mix_reg(sc, 0x32));
	printf("ESS_PCSPEAKER_VOL: mix reg 0x%02x=0x%02x\n",
	       0x3C, ess_read_mix_reg(sc, 0x3C));
	printf("ESS_DAC_REC_VOL: mix reg 0x%02x=0x%02x\n",
	       0x69, ess_read_mix_reg(sc, 0x69));
	printf("ESS_MIC_REC_VOL: mix reg 0x%02x=0x%02x\n",
	       0x68, ess_read_mix_reg(sc, 0x68));
	printf("ESS_LINE_REC_VOL: mix reg 0x%02x=0x%02x\n",
	       0x6E, ess_read_mix_reg(sc, 0x6E));
	printf("ESS_SYNTH_REC_VOL: mix reg 0x%02x=0x%02x\n",
	       0x6B, ess_read_mix_reg(sc, 0x6B));
	printf("ESS_CD_REC_VOL: mix reg 0x%02x=0x%02x\n",
	       0x6A, ess_read_mix_reg(sc, 0x6A));
	printf("ESS_AUXB_REC_VOL: mix reg 0x%02x=0x%02x\n",
	       0x6C, ess_read_mix_reg(sc, 0x6C));
	printf("ESS_RECORD_VOL: x reg 0x%02x=0x%02x\n",
	       0xB4, ess_read_x_reg(sc, 0xB4));
	printf("Audio 1 play vol (unused): mix reg 0x%02x=0x%02x\n",
	       0x14, ess_read_mix_reg(sc, 0x14));

	printf("ESS_MIC_PREAMP: x reg 0x%02x=0x%02x\n",
	       ESS_XCMD_PREAMP_CTRL, ess_read_x_reg(sc, ESS_XCMD_PREAMP_CTRL));
	printf("ESS_RECORD_MONITOR: x reg 0x%02x=0x%02x\n",
	       ESS_XCMD_AUDIO_CTRL, ess_read_x_reg(sc, ESS_XCMD_AUDIO_CTRL));
	printf("Record source: mix reg 0x%02x=0x%02x, 0x%02x=0x%02x\n",
	       ESS_MREG_ADC_SOURCE, ess_read_mix_reg(sc, ESS_MREG_ADC_SOURCE),
	       ESS_MREG_AUDIO2_CTRL2, ess_read_mix_reg(sc, ESS_MREG_AUDIO2_CTRL2));
}

#endif

/*
 * Configure the ESS chip for the desired audio base address.
 */
int
ess_config_addr(struct ess_softc *sc)
{
	int iobase;
	bus_space_tag_t iot;
	/*
	 * Configure using the System Control Register method.  This
	 * method is used when the AMODE line is tied high, which is
	 * the case for the Shark, but not for the evaluation board.
	 */
	bus_space_handle_t scr_access_ioh;
	bus_space_handle_t scr_ioh;
	u_short scr_value;

	iobase = sc->sc_iobase;
	iot = sc->sc_iot;
	/*
	 * Set the SCR bit to enable audio.
	 */
	scr_value = ESS_SCR_AUDIO_ENABLE;

	/*
	 * Set the SCR bits necessary to select the specified audio
	 * base address.
	 */
	switch(iobase) {
	case 0x220:
		scr_value |= ESS_SCR_AUDIO_220;
		break;
	case 0x230:
		scr_value |= ESS_SCR_AUDIO_230;
		break;
	case 0x240:
		scr_value |= ESS_SCR_AUDIO_240;
		break;
	case 0x250:
		scr_value |= ESS_SCR_AUDIO_250;
		break;
	default:
		printf("ess: configured iobase 0x%x invalid\n", iobase);
		return 1;
		break;
	}

	/*
	 * Get a mapping for the System Control Register (SCR) access
	 * registers and the SCR data registers.
	 */
	if (bus_space_map(iot, ESS_SCR_ACCESS_BASE, ESS_SCR_ACCESS_PORTS,
			  0, &scr_access_ioh)) {
		printf("ess: can't map SCR access registers\n");
		return 1;
	}
	if (bus_space_map(iot, ESS_SCR_BASE, ESS_SCR_PORTS,
			  0, &scr_ioh)) {
		printf("ess: can't map SCR registers\n");
		bus_space_unmap(iot, scr_access_ioh, ESS_SCR_ACCESS_PORTS);
		return 1;
	}

	/* Unlock the SCR. */
	EWRITE1(iot, scr_access_ioh, ESS_SCR_UNLOCK, 0);

	/* Write the base address information into SCR[0]. */
	EWRITE1(iot, scr_ioh, ESS_SCR_INDEX, 0);
	EWRITE1(iot, scr_ioh, ESS_SCR_DATA, scr_value);

	/* Lock the SCR. */
	EWRITE1(iot, scr_access_ioh, ESS_SCR_LOCK, 0);

	/* Unmap the SCR access ports and the SCR data ports. */
	bus_space_unmap(iot, scr_access_ioh, ESS_SCR_ACCESS_PORTS);
	bus_space_unmap(iot, scr_ioh, ESS_SCR_PORTS);

	return 0;
}


/*
 * Configure the ESS chip for the desired IRQ and DMA channels.
 * ESS  ISA
 * --------
 * IRQA irq9
 * IRQB irq5
 * IRQC irq7
 * IRQD irq10
 * IRQE irq15
 *
 * DRQA drq0
 * DRQB drq1
 * DRQC drq3
 * DRQD drq5
 */
void
ess_config_irq(struct ess_softc *sc)
{
	int v;

	DPRINTFN(2,("ess_config_irq\n"));

	if (sc->sc_model == ESS_1887 &&
	    sc->sc_audio1.irq == sc->sc_audio2.irq &&
	    sc->sc_audio1.irq != -1) {
		/* Use new method, both interrupts are the same. */
		v = ESS_IS_SELECT_IRQ;	/* enable intrs */
		switch (sc->sc_audio1.irq) {
		case 5:
			v |= ESS_IS_INTRB;
			break;
		case 7:
			v |= ESS_IS_INTRC;
			break;
		case 9:
			v |= ESS_IS_INTRA;
			break;
		case 10:
			v |= ESS_IS_INTRD;
			break;
		case 15:
			v |= ESS_IS_INTRE;
			break;
#ifdef DIAGNOSTIC
		default:
			printf("ess_config_irq: configured irq %d not supported for Audio 1\n",
			       sc->sc_audio1.irq);
			return;
#endif
		}
		/* Set the IRQ */
		ess_write_mix_reg(sc, ESS_MREG_INTR_ST, v);
		return;
	}

	if (sc->sc_model == ESS_1887) {
		/* Tell the 1887 to use the old interrupt method. */
		ess_write_mix_reg(sc, ESS_MREG_INTR_ST, ESS_IS_ES1888);
	}

	if (sc->sc_audio1.polled) {
		/* Turn off Audio1 interrupts. */
		v = 0;
	} else {
		/* Configure Audio 1 for the appropriate IRQ line. */
		v = ESS_IRQ_CTRL_MASK | ESS_IRQ_CTRL_EXT; /* All intrs on */
		switch (sc->sc_audio1.irq) {
		case 5:
			v |= ESS_IRQ_CTRL_INTRB;
			break;
		case 7:
			v |= ESS_IRQ_CTRL_INTRC;
			break;
		case 9:
			v |= ESS_IRQ_CTRL_INTRA;
			break;
		case 10:
			v |= ESS_IRQ_CTRL_INTRD;
			break;
#ifdef DIAGNOSTIC
		default:
			printf("ess: configured irq %d not supported for Audio 1\n",
			       sc->sc_audio1.irq);
			return;
#endif
		}
	}
	ess_write_x_reg(sc, ESS_XCMD_IRQ_CTRL, v);

	if (ESS_USE_AUDIO1(sc->sc_model))
		return;

	if (sc->sc_audio2.polled) {
		/* Turn off Audio2 interrupts. */
		ess_clear_mreg_bits(sc, ESS_MREG_AUDIO2_CTRL2,
				    ESS_AUDIO2_CTRL2_IRQ2_ENABLE);
	} else {
		/* Audio2 is hardwired to INTRE in this mode. */
		ess_set_mreg_bits(sc, ESS_MREG_AUDIO2_CTRL2,
				  ESS_AUDIO2_CTRL2_IRQ2_ENABLE);
	}
}


void
ess_config_drq(struct ess_softc *sc)
{
	int v;

	DPRINTFN(2,("ess_config_drq\n"));

	/* Configure Audio 1 (record) for DMA on the appropriate channel. */
	v = ESS_DRQ_CTRL_PU | ESS_DRQ_CTRL_EXT;
	switch (sc->sc_audio1.drq) {
	case 0:
		v |= ESS_DRQ_CTRL_DRQA;
		break;
	case 1:
		v |= ESS_DRQ_CTRL_DRQB;
		break;
	case 3:
		v |= ESS_DRQ_CTRL_DRQC;
		break;
#ifdef DIAGNOSTIC
	default:
		printf("ess_config_drq: configured DMA chan %d not supported for Audio 1\n",
		       sc->sc_audio1.drq);
		return;
#endif
	}
	/* Set DRQ1 */
	ess_write_x_reg(sc, ESS_XCMD_DRQ_CTRL, v);

	if (ESS_USE_AUDIO1(sc->sc_model))
		return;

	/* Configure DRQ2 */
	v = ESS_AUDIO2_CTRL3_DRQ_PD;
	switch (sc->sc_audio2.drq) {
	case 0:
		v |= ESS_AUDIO2_CTRL3_DRQA;
		break;
	case 1:
		v |= ESS_AUDIO2_CTRL3_DRQB;
		break;
	case 3:
		v |= ESS_AUDIO2_CTRL3_DRQC;
		break;
	case 5:
		v |= ESS_AUDIO2_CTRL3_DRQD;
		break;
#ifdef DIAGNOSTIC
	default:
		printf("ess_config_drq: configured DMA chan %d not supported for Audio 2\n",
		       sc->sc_audio2.drq);
		return;
#endif
	}
	ess_write_mix_reg(sc, ESS_MREG_AUDIO2_CTRL3, v);
	/* Enable DMA 2 */
	ess_set_mreg_bits(sc, ESS_MREG_AUDIO2_CTRL2,
			  ESS_AUDIO2_CTRL2_DMA_ENABLE);
}

/*
 * Set up registers after a reset.
 */
void
ess_setup(struct ess_softc *sc)
{

	ess_config_irq(sc);
	ess_config_drq(sc);

	DPRINTFN(2,("ess_setup: done\n"));
}

/*
 * Determine the model of ESS chip we are talking to.  Currently we
 * only support ES1888, ES1887 and ES888.  The method of determining
 * the chip is based on the information on page 27 of the ES1887 data
 * sheet.
 *
 * This routine sets the values of sc->sc_model and sc->sc_version.
 */
int
ess_identify(struct ess_softc *sc)
{
	u_char reg1;
	u_char reg2;
	u_char reg3;
	u_int8_t ident[4];

	sc->sc_model = ESS_UNSUPPORTED;
	sc->sc_version = 0;

	memset(ident, 0, sizeof(ident));

	/*
	 * 1. Check legacy ID bytes.  These should be 0x68 0x8n, where
	 *    n >= 8 for an ES1887 or an ES888.  Other values indicate
	 *    earlier (unsupported) chips.
	 */
	ess_wdsp(sc, ESS_ACMD_LEGACY_ID);

	if ((reg1 = ess_rdsp(sc)) != 0x68) {
		printf("ess: First ID byte wrong (0x%02x)\n", reg1);
		return 1;
	}

	reg2 = ess_rdsp(sc);
	if (((reg2 & 0xf0) != 0x80) ||
	    ((reg2 & 0x0f) < 8)) {
		sc->sc_model = ESS_688;
		return 0;
	}

	/*
	 * Store the ID bytes as the version.
	 */
	sc->sc_version = (reg1 << 8) + reg2;


	/*
	 * 2. Verify we can change bit 2 in mixer register 0x64.  This
	 *    should be possible on all supported chips.
	 */
	reg1 = ess_read_mix_reg(sc, ESS_MREG_VOLUME_CTRL);
	reg2 = reg1 ^ 0x04;  /* toggle bit 2 */

	ess_write_mix_reg(sc, ESS_MREG_VOLUME_CTRL, reg2);

	if (ess_read_mix_reg(sc, ESS_MREG_VOLUME_CTRL) != reg2) {
		switch (sc->sc_version) {
		case 0x688b:
			sc->sc_model = ESS_1688;
			break;
		default:
			printf("ess: Hardware error (unable to toggle bit 2 of mixer register 0x64)\n");
			return 1;
		}
		return 0;
	}

	/*
	 * Restore the original value of mixer register 0x64.
	 */
	ess_write_mix_reg(sc, ESS_MREG_VOLUME_CTRL, reg1);


	/*
	 * 3. Verify we can change the value of mixer register
	 *    ESS_MREG_SAMPLE_RATE.
	 *    This is possible on the 1888/1887/888, but not on the 1788.
	 *    It is not necessary to restore the value of this mixer register.
	 */
	reg1 = ess_read_mix_reg(sc, ESS_MREG_SAMPLE_RATE);
	reg2 = reg1 ^ 0xff;  /* toggle all bits */

	ess_write_mix_reg(sc, ESS_MREG_SAMPLE_RATE, reg2);

	if (ess_read_mix_reg(sc, ESS_MREG_SAMPLE_RATE) != reg2) {
		/* If we got this far before failing, it's a 1788. */
		sc->sc_model = ESS_1788;

		/*
		 * Identify ESS model for ES18[67]8.
		 */
		ess_read_multi_mix_reg(sc, 0x40, ident, sizeof(ident));
		if(ident[0] == 0x18) {
			switch(ident[1]) {
			case 0x68:
				sc->sc_model = ESS_1868;
				break;
			case 0x78:
				sc->sc_model = ESS_1878;
				break;
			}
		}

		return 0;
	}

	/*
	 * 4. Determine if we can change bit 5 in mixer register 0x64.
	 *    This determines whether we have an ES1887:
	 *
	 *    - can change indicates ES1887
	 *    - can't change indicates ES1888 or ES888
	 */
	reg1 = ess_read_mix_reg(sc, ESS_MREG_VOLUME_CTRL);
	reg2 = reg1 ^ 0x20;  /* toggle bit 5 */

	ess_write_mix_reg(sc, ESS_MREG_VOLUME_CTRL, reg2);

	if (ess_read_mix_reg(sc, ESS_MREG_VOLUME_CTRL) == reg2) {
		sc->sc_model = ESS_1887;

		/*
		 * Restore the original value of mixer register 0x64.
		 */
		ess_write_mix_reg(sc, ESS_MREG_VOLUME_CTRL, reg1);

		/*
		 * Identify ESS model for ES18[67]9.
		 */
		ess_read_multi_mix_reg(sc, 0x40, ident, sizeof(ident));
		if(ident[0] == 0x18) {
			switch(ident[1]) {
			case 0x69:
				sc->sc_model = ESS_1869;
				break;
			case 0x79:
				sc->sc_model = ESS_1879;
				break;
			}
		}

		return 0;
	}

	/*
	 * 5. Determine if we can change the value of mixer
	 *    register 0x69 independently of mixer register
	 *    0x68. This determines which chip we have:
	 *
	 *    - can modify idependently indicates ES888
	 *    - register 0x69 is an alias of 0x68 indicates ES1888
	 */
	reg1 = ess_read_mix_reg(sc, 0x68);
	reg2 = ess_read_mix_reg(sc, 0x69);
	reg3 = reg2 ^ 0xff;  /* toggle all bits */

	/*
	 * Write different values to each register.
	 */
	ess_write_mix_reg(sc, 0x68, reg2);
	ess_write_mix_reg(sc, 0x69, reg3);

	if (ess_read_mix_reg(sc, 0x68) == reg2 &&
	    ess_read_mix_reg(sc, 0x69) == reg3)
		sc->sc_model = ESS_888;
	else
		sc->sc_model = ESS_1888;

	/*
	 * Restore the original value of the registers.
	 */
	ess_write_mix_reg(sc, 0x68, reg1);
	ess_write_mix_reg(sc, 0x69, reg2);

	return 0;
}


int
ess_setup_sc(struct ess_softc *sc, int doinit)
{

	/* Reset the chip. */
	if (ess_reset(sc) != 0) {
		DPRINTF(("ess_setup_sc: couldn't reset chip\n"));
		return 1;
	}

	/* Identify the ESS chip, and check that it is supported. */
	if (ess_identify(sc)) {
		DPRINTF(("ess_setup_sc: couldn't identify\n"));
		return 1;
	}

	return 0;
}

/*
 * Probe for the ESS hardware.
 */
int
essmatch(struct ess_softc *sc)
{
	if (!ESS_BASE_VALID(sc->sc_iobase)) {
		printf("ess: configured iobase 0x%x invalid\n", sc->sc_iobase);
		return 0;
	}

	if (ess_setup_sc(sc, 1))
		return 0;

	if (sc->sc_model == ESS_UNSUPPORTED) {
		DPRINTF(("ess: Unsupported model\n"));
		return 0;
	}

	/* Check that requested DMA channels are valid and different. */
	if (!ESS_DRQ1_VALID(sc->sc_audio1.drq)) {
		printf("ess: record drq %d invalid\n", sc->sc_audio1.drq);
		return 0;
	}
	if (!isa_drq_isfree(sc->sc_ic, sc->sc_audio1.drq))
		return 0;
	if (!ESS_USE_AUDIO1(sc->sc_model)) {
		if (!ESS_DRQ2_VALID(sc->sc_audio2.drq)) {
			printf("ess: play drq %d invalid\n", sc->sc_audio2.drq);
			return 0;
		}
		if (sc->sc_audio1.drq == sc->sc_audio2.drq) {
			printf("ess: play and record drq both %d\n",
			       sc->sc_audio1.drq);
			return 0;
		}
		if (!isa_drq_isfree(sc->sc_ic, sc->sc_audio2.drq))
			return 0;
	}

	/*
	 * The 1887 has an additional IRQ mode where both channels are mapped
	 * to the same IRQ.
	 */
	if (sc->sc_model == ESS_1887 &&
	    sc->sc_audio1.irq == sc->sc_audio2.irq &&
	    sc->sc_audio1.irq != -1 &&
	    ESS_IRQ12_VALID(sc->sc_audio1.irq))
		goto irq_not1888;

	/* Check that requested IRQ lines are valid and different. */
	if (sc->sc_audio1.irq != -1 &&
	    !ESS_IRQ1_VALID(sc->sc_audio1.irq)) {
		printf("ess: record irq %d invalid\n", sc->sc_audio1.irq);
		return 0;
	}
	if (!ESS_USE_AUDIO1(sc->sc_model)) {
		if (sc->sc_audio2.irq != -1 &&
		    !ESS_IRQ2_VALID(sc->sc_audio2.irq)) {
			printf("ess: play irq %d invalid\n", sc->sc_audio2.irq);
			return 0;
		}
		if (sc->sc_audio1.irq == sc->sc_audio2.irq &&
		    sc->sc_audio1.irq != -1) {
			printf("ess: play and record irq both %d\n",
			       sc->sc_audio1.irq);
			return 0;
		}
	}

irq_not1888:
	/* XXX should we check IRQs as well? */

	return 2; /* beat "sb" */
}


/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver.
 */
void
essattach(struct ess_softc *sc, int enablejoy)
{
	struct audio_attach_args arg;
	int i;
	u_int v;

	if (ess_setup_sc(sc, 0)) {
		printf(": setup failed\n");
		return;
	}

	aprint_normal("ESS Technology ES%s [version 0x%04x]\n",
	    essmodel[sc->sc_model], sc->sc_version);

	callout_init(&sc->sc_poll1_ch, CALLOUT_MPSAFE);
	callout_init(&sc->sc_poll2_ch, CALLOUT_MPSAFE);
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	sc->sc_audio1.polled = sc->sc_audio1.irq == -1;
	if (!sc->sc_audio1.polled) {
		sc->sc_audio1.ih = isa_intr_establish(sc->sc_ic,
		    sc->sc_audio1.irq, sc->sc_audio1.ist, IPL_AUDIO,
		    ess_audio1_intr, sc);
		aprint_normal_dev(sc->sc_dev,
		    "audio1 interrupting at irq %d\n", sc->sc_audio1.irq);
	} else
		aprint_normal_dev(sc->sc_dev, "audio1 polled\n");
	sc->sc_audio1.maxsize = isa_dmamaxsize(sc->sc_ic, sc->sc_audio1.drq);

	if (isa_drq_alloc(sc->sc_ic, sc->sc_audio1.drq) != 0) {
		aprint_error_dev(sc->sc_dev, "can't reserve drq %d\n",
		    sc->sc_audio1.drq);
		goto fail;
	}

	if (isa_dmamap_create(sc->sc_ic, sc->sc_audio1.drq,
	    sc->sc_audio1.maxsize, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW)) {
		aprint_error_dev(sc->sc_dev, "can't create map for drq %d\n",
		    sc->sc_audio1.drq);
		goto fail;
	}

	if (!ESS_USE_AUDIO1(sc->sc_model)) {
		sc->sc_audio2.polled = sc->sc_audio2.irq == -1;
		if (!sc->sc_audio2.polled) {
			sc->sc_audio2.ih = isa_intr_establish(sc->sc_ic,
			    sc->sc_audio2.irq, sc->sc_audio2.ist, IPL_AUDIO,
			    ess_audio2_intr, sc);
			aprint_normal_dev(sc->sc_dev,
			    "audio2 interrupting at irq %d\n",
			    sc->sc_audio2.irq);
		} else
			aprint_normal_dev(sc->sc_dev, "audio2 polled\n");
		sc->sc_audio2.maxsize = isa_dmamaxsize(sc->sc_ic,
		    sc->sc_audio2.drq);

		if (isa_drq_alloc(sc->sc_ic, sc->sc_audio2.drq) != 0) {
			aprint_error_dev(sc->sc_dev, "can't reserve drq %d\n",
			    sc->sc_audio2.drq);
			goto fail;
		}

		if (isa_dmamap_create(sc->sc_ic, sc->sc_audio2.drq,
		    sc->sc_audio2.maxsize, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW)) {
			aprint_error_dev(sc->sc_dev, "can't create map for drq %d\n",
			    sc->sc_audio2.drq);
			goto fail;
		}
	}

	/* Do a hardware reset on the mixer. */
	ess_write_mix_reg(sc, ESS_MIX_RESET, ESS_MIX_RESET);

	/*
	 * Set volume of Audio 1 to zero and disable Audio 1 DAC input
	 * to playback mixer, since playback is always through Audio 2.
	 */
	if (!ESS_USE_AUDIO1(sc->sc_model))
		ess_write_mix_reg(sc, ESS_MREG_VOLUME_VOICE, 0);
	ess_wdsp(sc, ESS_ACMD_DISABLE_SPKR);

	if (ESS_USE_AUDIO1(sc->sc_model)) {
		ess_write_mix_reg(sc, ESS_MREG_ADC_SOURCE, ESS_SOURCE_MIC);
		sc->in_port = ESS_SOURCE_MIC;
		if (ESS_IS_ES18X9(sc->sc_model)) {
			sc->ndevs = ESS_18X9_NDEVS;
			sc->sc_spatializer = 0;
			ess_set_mreg_bits(sc, ESS_MREG_MODE,
			    ESS_MODE_ASYNC_MODE | ESS_MODE_NEWREG);
			ess_set_mreg_bits(sc, ESS_MREG_SPATIAL_CTRL,
			    ESS_SPATIAL_CTRL_RESET);
			ess_clear_mreg_bits(sc, ESS_MREG_SPATIAL_CTRL,
			    ESS_SPATIAL_CTRL_ENABLE | ESS_SPATIAL_CTRL_MONO);
		} else
			sc->ndevs = ESS_1788_NDEVS;
	} else {
		/*
		 * Set hardware record source to use output of the record
		 * mixer. We do the selection of record source in software by
		 * setting the gain of the unused sources to zero. (See
		 * ess_set_in_ports.)
		 */
		ess_write_mix_reg(sc, ESS_MREG_ADC_SOURCE, ESS_SOURCE_MIXER);
		sc->in_mask = 1 << ESS_MIC_REC_VOL;
		sc->ndevs = ESS_1888_NDEVS;
		ess_clear_mreg_bits(sc, ESS_MREG_AUDIO2_CTRL2, 0x10);
		ess_set_mreg_bits(sc, ESS_MREG_AUDIO2_CTRL2, 0x08);
	}

	/*
	 * Set gain on each mixer device to a sensible value.
	 * Devices not normally used are turned off, and other devices
	 * are set to 50% volume.
	 */
	for (i = 0; i < sc->ndevs; i++) {
		if (ESS_IS_ES18X9(sc->sc_model)) {
			switch (i) {
			case ESS_SPATIALIZER:
			case ESS_SPATIALIZER_ENABLE:
				v = 0;
				goto skip;
			}
		}
		switch (i) {
		case ESS_MIC_PLAY_VOL:
		case ESS_LINE_PLAY_VOL:
		case ESS_CD_PLAY_VOL:
		case ESS_AUXB_PLAY_VOL:
		case ESS_DAC_REC_VOL:
		case ESS_LINE_REC_VOL:
		case ESS_SYNTH_REC_VOL:
		case ESS_CD_REC_VOL:
		case ESS_AUXB_REC_VOL:
			v = 0;
			break;
		default:
			v = ESS_4BIT_GAIN(AUDIO_MAX_GAIN / 2);
			break;
		}
skip:
		sc->gain[i][ESS_LEFT] = sc->gain[i][ESS_RIGHT] = v;
		ess_set_gain(sc, i, 1);
	}

	ess_setup(sc);

	/* Disable the speaker until the device is opened.  */
	ess_speaker_off(sc);
	sc->spkr_state = SPKR_OFF;

	snprintf(ess_device.name, sizeof(ess_device.name), "ES%s",
	    essmodel[sc->sc_model]);
	snprintf(ess_device.version, sizeof(ess_device.version), "0x%04x",
	    sc->sc_version);

	if (ESS_USE_AUDIO1(sc->sc_model))
		audio_attach_mi(&ess_1788_hw_if, sc, sc->sc_dev);
	else
		audio_attach_mi(&ess_1888_hw_if, sc, sc->sc_dev);

	arg.type = AUDIODEV_TYPE_OPL;
	arg.hwif = 0;
	arg.hdl = 0;
	(void)config_found(sc->sc_dev, &arg, audioprint);

#if NJOY_ESS > 0
	if (sc->sc_model == ESS_1888 && enablejoy) {
		unsigned char m40;

		m40 = ess_read_mix_reg(sc, 0x40);
		m40 |= 2;
		ess_write_mix_reg(sc, 0x40, m40);

		arg.type = AUDIODEV_TYPE_AUX;
		(void)config_found(sc->sc_dev, &arg, audioprint);
	}
#endif

#ifdef AUDIO_DEBUG
	if (essdebug > 0)
		ess_printsc(sc);
#endif

	return;

 fail:
	callout_destroy(&sc->sc_poll1_ch);
	callout_destroy(&sc->sc_poll2_ch);
	mutex_destroy(&sc->sc_lock);
	mutex_destroy(&sc->sc_intr_lock);
}

/*
 * Various routines to interface to higher level audio driver
 */

int
ess_open(void *addr, int flags)
{

	return 0;
}

void
ess_close(void *addr)
{
	struct ess_softc *sc;

	sc = addr;
	DPRINTF(("ess_close: sc=%p\n", sc));

	ess_speaker_off(sc);
	sc->spkr_state = SPKR_OFF;

	DPRINTF(("ess_close: closed\n"));
}

/*
 * Wait for FIFO to drain, and analog section to settle.
 * XXX should check FIFO empty bit.
 */
int
ess_drain(void *addr)
{
	struct ess_softc *sc;

	sc = addr;
	mutex_exit(&sc->sc_lock);
	kpause("essdr", FALSE, hz/20, &sc->sc_intr_lock); /* XXX */
	if (!mutex_tryenter(&sc->sc_lock)) {
		mutex_spin_exit(&sc->sc_intr_lock);
		mutex_enter(&sc->sc_lock);
		mutex_spin_enter(&sc->sc_intr_lock);
	}

	return 0;
}

/* XXX should use reference count */
int
ess_speaker_ctl(void *addr, int newstate)
{
	struct ess_softc *sc;

	sc = addr;
	if ((newstate == SPKR_ON) && (sc->spkr_state == SPKR_OFF)) {
		ess_speaker_on(sc);
		sc->spkr_state = SPKR_ON;
	}
	if ((newstate == SPKR_OFF) && (sc->spkr_state == SPKR_ON)) {
		ess_speaker_off(sc);
		sc->spkr_state = SPKR_OFF;
	}
	return 0;
}

int
ess_getdev(void *addr, struct audio_device *retp)
{

	*retp = ess_device;
	return 0;
}

int
ess_query_encoding(void *addr, struct audio_encoding *fp)
{
	/*struct ess_softc *sc = addr;*/

	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		return 0;
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 2:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 3:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = 0;
		return 0;
	case 4:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		return 0;
	case 5:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		return 0;
	case 6:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	default:
		return EINVAL;
	}
	return 0;
}

int
ess_set_params(
	void *addr,
	int setmode, int usemode,
	audio_params_t *play, audio_params_t *rec,
	stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
	struct ess_softc *sc;
	int rate;

	DPRINTF(("ess_set_params: set=%d use=%d\n", setmode, usemode));
	sc = addr;
	/*
	 * The ES1887 manual (page 39, `Full-Duplex DMA Mode') claims that in
	 * full-duplex operation the sample rates must be the same for both
	 * channels.  This appears to be false; the only bit in common is the
	 * clock source selection.  However, we'll be conservative here.
	 * - mycroft
	 */
	if (play->sample_rate != rec->sample_rate &&
	    usemode == (AUMODE_PLAY | AUMODE_RECORD)) {
		if (setmode == AUMODE_PLAY) {
			rec->sample_rate = play->sample_rate;
			setmode |= AUMODE_RECORD;
		} else if (setmode == AUMODE_RECORD) {
			play->sample_rate = rec->sample_rate;
			setmode |= AUMODE_PLAY;
		} else
			return EINVAL;
	}

	if (setmode & AUMODE_RECORD) {
		if (auconv_set_converter(ess_formats, ESS_NFORMATS,
					 AUMODE_RECORD, rec, FALSE, rfil) < 0)
			return EINVAL;
	}
	if (setmode & AUMODE_PLAY) {
		if (auconv_set_converter(ess_formats, ESS_NFORMATS,
					 AUMODE_PLAY, play, FALSE, pfil) < 0)
			return EINVAL;
	}

	if (usemode == AUMODE_RECORD)
		rate = rec->sample_rate;
	else
		rate = play->sample_rate;

	ess_write_x_reg(sc, ESS_XCMD_SAMPLE_RATE, ess_srtotc(sc, rate));
	ess_write_x_reg(sc, ESS_XCMD_FILTER_CLOCK, ess_srtofc(rate));

	if (!ESS_USE_AUDIO1(sc->sc_model)) {
		ess_write_mix_reg(sc, ESS_MREG_SAMPLE_RATE,
		    ess_srtotc(sc, rate));
		ess_write_mix_reg(sc, ESS_MREG_FILTER_CLOCK, ess_srtofc(rate));
	}

	return 0;
}

int
ess_audio1_trigger_output(
	void *addr,
	void *start, void *end,
	int blksize,
	void (*intr)(void *),
	void *arg,
	const audio_params_t *param)
{
	struct ess_softc *sc;
	u_int8_t reg;

	sc = addr;
	DPRINTFN(1, ("ess_audio1_trigger_output: sc=%p start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", addr, start, end, blksize, intr, arg));

	if (sc->sc_audio1.active)
		panic("ess_audio1_trigger_output: already running");

	sc->sc_audio1.active = 1;
	sc->sc_audio1.intr = intr;
	sc->sc_audio1.arg = arg;
	if (sc->sc_audio1.polled) {
		sc->sc_audio1.dmapos = 0;
		sc->sc_audio1.buffersize = (char *)end - (char *)start;
		sc->sc_audio1.dmacount = 0;
		sc->sc_audio1.blksize = blksize;
		callout_reset(&sc->sc_poll1_ch, hz / 30,
		    ess_audio1_poll, sc);
	}

	reg = ess_read_x_reg(sc, ESS_XCMD_AUDIO_CTRL);
	if (param->channels == 2) {
		reg &= ~ESS_AUDIO_CTRL_MONO;
		reg |= ESS_AUDIO_CTRL_STEREO;
	} else {
		reg |= ESS_AUDIO_CTRL_MONO;
		reg &= ~ESS_AUDIO_CTRL_STEREO;
	}
	ess_write_x_reg(sc, ESS_XCMD_AUDIO_CTRL, reg);

	reg = ess_read_x_reg(sc, ESS_XCMD_AUDIO1_CTRL1);
	if (param->precision == 16)
		reg |= ESS_AUDIO1_CTRL1_FIFO_SIZE;
	else
		reg &= ~ESS_AUDIO1_CTRL1_FIFO_SIZE;
	if (param->channels == 2)
		reg |= ESS_AUDIO1_CTRL1_FIFO_STEREO;
	else
		reg &= ~ESS_AUDIO1_CTRL1_FIFO_STEREO;
	if (param->encoding == AUDIO_ENCODING_SLINEAR_BE ||
	    param->encoding == AUDIO_ENCODING_SLINEAR_LE)
		reg |= ESS_AUDIO1_CTRL1_FIFO_SIGNED;
	else
		reg &= ~ESS_AUDIO1_CTRL1_FIFO_SIGNED;
	reg |= ESS_AUDIO1_CTRL1_FIFO_CONNECT;
	ess_write_x_reg(sc, ESS_XCMD_AUDIO1_CTRL1, reg);

	isa_dmastart(sc->sc_ic, sc->sc_audio1.drq, start,
		     (char *)end - (char *)start, NULL,
	    DMAMODE_WRITE | DMAMODE_LOOPDEMAND, BUS_DMA_NOWAIT);

	/* Program transfer count registers with 2's complement of count. */
	blksize = -blksize;
	ess_write_x_reg(sc, ESS_XCMD_XFER_COUNTLO, blksize);
	ess_write_x_reg(sc, ESS_XCMD_XFER_COUNTHI, blksize >> 8);

	/* Use 4 bytes per output DMA. */
	ess_set_xreg_bits(sc, ESS_XCMD_DEMAND_CTRL, ESS_DEMAND_CTRL_DEMAND_4);

	/* Start auto-init DMA */
	ess_wdsp(sc, ESS_ACMD_ENABLE_SPKR);
	reg = ess_read_x_reg(sc, ESS_XCMD_AUDIO1_CTRL2);
	reg &= ~(ESS_AUDIO1_CTRL2_DMA_READ | ESS_AUDIO1_CTRL2_ADC_ENABLE);
	reg |= ESS_AUDIO1_CTRL2_FIFO_ENABLE | ESS_AUDIO1_CTRL2_AUTO_INIT;
	ess_write_x_reg(sc, ESS_XCMD_AUDIO1_CTRL2, reg);

	return 0;
}

int
ess_audio2_trigger_output(
	void *addr,
	void *start, void *end,
	int blksize,
	void (*intr)(void *),
	void *arg,
	const audio_params_t *param)
{
	struct ess_softc *sc;
	u_int8_t reg;

	sc = addr;
	DPRINTFN(1, ("ess_audio2_trigger_output: sc=%p start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", addr, start, end, blksize, intr, arg));

	if (sc->sc_audio2.active)
		panic("ess_audio2_trigger_output: already running");

	sc->sc_audio2.active = 1;
	sc->sc_audio2.intr = intr;
	sc->sc_audio2.arg = arg;
	if (sc->sc_audio2.polled) {
		sc->sc_audio2.dmapos = 0;
		sc->sc_audio2.buffersize = (char *)end - (char *)start;
		sc->sc_audio2.dmacount = 0;
		sc->sc_audio2.blksize = blksize;
		callout_reset(&sc->sc_poll2_ch, hz / 30,
		    ess_audio2_poll, sc);
	}

	reg = ess_read_mix_reg(sc, ESS_MREG_AUDIO2_CTRL2);
	if (param->precision == 16)
		reg |= ESS_AUDIO2_CTRL2_FIFO_SIZE;
	else
		reg &= ~ESS_AUDIO2_CTRL2_FIFO_SIZE;
	if (param->channels == 2)
		reg |= ESS_AUDIO2_CTRL2_CHANNELS;
	else
		reg &= ~ESS_AUDIO2_CTRL2_CHANNELS;
	if (param->encoding == AUDIO_ENCODING_SLINEAR_BE ||
	    param->encoding == AUDIO_ENCODING_SLINEAR_LE)
		reg |= ESS_AUDIO2_CTRL2_FIFO_SIGNED;
	else
		reg &= ~ESS_AUDIO2_CTRL2_FIFO_SIGNED;
	ess_write_mix_reg(sc, ESS_MREG_AUDIO2_CTRL2, reg);

	isa_dmastart(sc->sc_ic, sc->sc_audio2.drq, start,
		     (char *)end - (char *)start, NULL,
	    DMAMODE_WRITE | DMAMODE_LOOPDEMAND, BUS_DMA_NOWAIT);

	if (IS16BITDRQ(sc->sc_audio2.drq))
		blksize >>= 1;	/* use word count for 16 bit DMA */
	/* Program transfer count registers with 2's complement of count. */
	blksize = -blksize;
	ess_write_mix_reg(sc, ESS_MREG_XFER_COUNTLO, blksize);
	ess_write_mix_reg(sc, ESS_MREG_XFER_COUNTHI, blksize >> 8);

	reg = ess_read_mix_reg(sc, ESS_MREG_AUDIO2_CTRL1);
	if (IS16BITDRQ(sc->sc_audio2.drq))
		reg |= ESS_AUDIO2_CTRL1_XFER_SIZE;
	else
		reg &= ~ESS_AUDIO2_CTRL1_XFER_SIZE;
	reg |= ESS_AUDIO2_CTRL1_DEMAND_8;
	reg |= ESS_AUDIO2_CTRL1_DAC_ENABLE | ESS_AUDIO2_CTRL1_FIFO_ENABLE |
	       ESS_AUDIO2_CTRL1_AUTO_INIT;
	ess_write_mix_reg(sc, ESS_MREG_AUDIO2_CTRL1, reg);

	return (0);
}

int
ess_audio1_trigger_input(
	void *addr,
	void *start, void *end,
	int blksize,
	void (*intr)(void *),
	void *arg,
	const audio_params_t *param)
{
	struct ess_softc *sc;
	u_int8_t reg;

	sc = addr;
	DPRINTFN(1, ("ess_audio1_trigger_input: sc=%p start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", addr, start, end, blksize, intr, arg));

	if (sc->sc_audio1.active)
		panic("ess_audio1_trigger_input: already running");

	sc->sc_audio1.active = 1;
	sc->sc_audio1.intr = intr;
	sc->sc_audio1.arg = arg;
	if (sc->sc_audio1.polled) {
		sc->sc_audio1.dmapos = 0;
		sc->sc_audio1.buffersize = (char *)end - (char *)start;
		sc->sc_audio1.dmacount = 0;
		sc->sc_audio1.blksize = blksize;
		callout_reset(&sc->sc_poll1_ch, hz / 30,
		    ess_audio1_poll, sc);
	}

	reg = ess_read_x_reg(sc, ESS_XCMD_AUDIO_CTRL);
	if (param->channels == 2) {
		reg &= ~ESS_AUDIO_CTRL_MONO;
		reg |= ESS_AUDIO_CTRL_STEREO;
	} else {
		reg |= ESS_AUDIO_CTRL_MONO;
		reg &= ~ESS_AUDIO_CTRL_STEREO;
	}
	ess_write_x_reg(sc, ESS_XCMD_AUDIO_CTRL, reg);

	reg = ess_read_x_reg(sc, ESS_XCMD_AUDIO1_CTRL1);
	if (param->precision == 16)
		reg |= ESS_AUDIO1_CTRL1_FIFO_SIZE;
	else
		reg &= ~ESS_AUDIO1_CTRL1_FIFO_SIZE;
	if (param->channels == 2)
		reg |= ESS_AUDIO1_CTRL1_FIFO_STEREO;
	else
		reg &= ~ESS_AUDIO1_CTRL1_FIFO_STEREO;
	if (param->encoding == AUDIO_ENCODING_SLINEAR_BE ||
	    param->encoding == AUDIO_ENCODING_SLINEAR_LE)
		reg |= ESS_AUDIO1_CTRL1_FIFO_SIGNED;
	else
		reg &= ~ESS_AUDIO1_CTRL1_FIFO_SIGNED;
	reg |= ESS_AUDIO1_CTRL1_FIFO_CONNECT;
	ess_write_x_reg(sc, ESS_XCMD_AUDIO1_CTRL1, reg);

	isa_dmastart(sc->sc_ic, sc->sc_audio1.drq, start,
		     (char *)end - (char *)start, NULL,
	    DMAMODE_READ | DMAMODE_LOOPDEMAND, BUS_DMA_NOWAIT);

	/* Program transfer count registers with 2's complement of count. */
	blksize = -blksize;
	ess_write_x_reg(sc, ESS_XCMD_XFER_COUNTLO, blksize);
	ess_write_x_reg(sc, ESS_XCMD_XFER_COUNTHI, blksize >> 8);

	/* Use 4 bytes per input DMA. */
	ess_set_xreg_bits(sc, ESS_XCMD_DEMAND_CTRL, ESS_DEMAND_CTRL_DEMAND_4);

	/* Start auto-init DMA */
	ess_wdsp(sc, ESS_ACMD_DISABLE_SPKR);
	reg = ess_read_x_reg(sc, ESS_XCMD_AUDIO1_CTRL2);
	reg |= ESS_AUDIO1_CTRL2_DMA_READ | ESS_AUDIO1_CTRL2_ADC_ENABLE;
	reg |= ESS_AUDIO1_CTRL2_FIFO_ENABLE | ESS_AUDIO1_CTRL2_AUTO_INIT;
	ess_write_x_reg(sc, ESS_XCMD_AUDIO1_CTRL2, reg);

	return 0;
}

int
ess_audio1_halt(void *addr)
{
	struct ess_softc *sc;

	sc = addr;
	DPRINTF(("ess_audio1_halt: sc=%p\n", sc));

	if (sc->sc_audio1.active) {
		ess_clear_xreg_bits(sc, ESS_XCMD_AUDIO1_CTRL2,
		    ESS_AUDIO1_CTRL2_FIFO_ENABLE);
		isa_dmaabort(sc->sc_ic, sc->sc_audio1.drq);
		if (sc->sc_audio1.polled)
			callout_stop(&sc->sc_poll1_ch);
		sc->sc_audio1.active = 0;
	}

	return 0;
}

int
ess_audio2_halt(void *addr)
{
	struct ess_softc *sc;

	sc = addr;
	DPRINTF(("ess_audio2_halt: sc=%p\n", sc));

	if (sc->sc_audio2.active) {
		ess_clear_mreg_bits(sc, ESS_MREG_AUDIO2_CTRL1,
		    ESS_AUDIO2_CTRL1_DAC_ENABLE |
		    ESS_AUDIO2_CTRL1_FIFO_ENABLE);
		isa_dmaabort(sc->sc_ic, sc->sc_audio2.drq);
		if (sc->sc_audio2.polled)
			callout_stop(&sc->sc_poll2_ch);
		sc->sc_audio2.active = 0;
	}

	return 0;
}

int
ess_audio1_intr(void *arg)
{
	struct ess_softc *sc;
	uint8_t reg;
	int rv;

	sc = arg;
	DPRINTFN(1,("ess_audio1_intr: intr=%p\n", sc->sc_audio1.intr));

	mutex_spin_enter(&sc->sc_intr_lock);

	/* Check and clear interrupt on Audio1. */
	reg = EREAD1(sc->sc_iot, sc->sc_ioh, ESS_DSP_RW_STATUS);
	if ((reg & ESS_DSP_READ_OFLOW) == 0) {
		mutex_spin_exit(&sc->sc_intr_lock);
		return 0;
	}
	reg = EREAD1(sc->sc_iot, sc->sc_ioh, ESS_CLEAR_INTR);

	sc->sc_audio1.nintr++;

	if (sc->sc_audio1.active) {
		(*sc->sc_audio1.intr)(sc->sc_audio1.arg);
		rv = 1;
	} else
		rv = 0;

	mutex_spin_exit(&sc->sc_intr_lock);

	return rv;
}

int
ess_audio2_intr(void *arg)
{
	struct ess_softc *sc;
	uint8_t reg;
	int rv;

	sc = arg;
	DPRINTFN(1,("ess_audio2_intr: intr=%p\n", sc->sc_audio2.intr));

	mutex_spin_enter(&sc->sc_intr_lock);

	/* Check and clear interrupt on Audio2. */
	reg = ess_read_mix_reg(sc, ESS_MREG_AUDIO2_CTRL2);
	if ((reg & ESS_AUDIO2_CTRL2_IRQ_LATCH) == 0) {
		mutex_spin_exit(&sc->sc_intr_lock);
		return 0;
	}
	reg &= ~ESS_AUDIO2_CTRL2_IRQ_LATCH;
	ess_write_mix_reg(sc, ESS_MREG_AUDIO2_CTRL2, reg);

	sc->sc_audio2.nintr++;

	if (sc->sc_audio2.active) {
		(*sc->sc_audio2.intr)(sc->sc_audio2.arg);
		rv = 1;
	} else
		rv = 0;

	mutex_spin_exit(&sc->sc_intr_lock);

	return rv;
}

void
ess_audio1_poll(void *addr)
{
	struct ess_softc *sc;
	int dmapos, dmacount;

	sc = addr;
	mutex_spin_enter(&sc->sc_intr_lock);

	if (!sc->sc_audio1.active) {
		mutex_spin_exit(&sc->sc_intr_lock);
		return;
	}

	sc->sc_audio1.nintr++;

	dmapos = isa_dmacount(sc->sc_ic, sc->sc_audio1.drq);
	dmacount = sc->sc_audio1.dmapos - dmapos;
	if (dmacount < 0)
		dmacount += sc->sc_audio1.buffersize;
	sc->sc_audio1.dmapos = dmapos;
#if 1
	dmacount += sc->sc_audio1.dmacount;
	while (dmacount > sc->sc_audio1.blksize) {
		dmacount -= sc->sc_audio1.blksize;
		(*sc->sc_audio1.intr)(sc->sc_audio1.arg);
	}
	sc->sc_audio1.dmacount = dmacount;
#else
	(*sc->sc_audio1.intr)(sc->sc_audio1.arg, dmacount);
#endif

	mutex_spin_exit(&sc->sc_intr_lock);
	callout_reset(&sc->sc_poll1_ch, hz / 30, ess_audio1_poll, sc);
}

void
ess_audio2_poll(void *addr)
{
	struct ess_softc *sc;
	int dmapos, dmacount;

	sc = addr;
	mutex_spin_enter(&sc->sc_intr_lock);

	if (!sc->sc_audio2.active) {
		mutex_spin_exit(&sc->sc_intr_lock);
		return;
	}

	sc->sc_audio2.nintr++;

	dmapos = isa_dmacount(sc->sc_ic, sc->sc_audio2.drq);
	dmacount = sc->sc_audio2.dmapos - dmapos;
	if (dmacount < 0)
		dmacount += sc->sc_audio2.buffersize;
	sc->sc_audio2.dmapos = dmapos;
#if 1
	dmacount += sc->sc_audio2.dmacount;
	while (dmacount > sc->sc_audio2.blksize) {
		dmacount -= sc->sc_audio2.blksize;
		(*sc->sc_audio2.intr)(sc->sc_audio2.arg);
	}
	sc->sc_audio2.dmacount = dmacount;
#else
	(*sc->sc_audio2.intr)(sc->sc_audio2.arg, dmacount);
#endif

	mutex_spin_exit(&sc->sc_intr_lock);
	callout_reset(&sc->sc_poll2_ch, hz / 30, ess_audio2_poll, sc);
}

int
ess_round_blocksize(void *addr, int blk, int mode,
    const audio_params_t *param)
{

	return blk & -8;	/* round for max DMA size */
}

int
ess_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct ess_softc *sc;
	int lgain, rgain;

	sc = addr;
	DPRINTFN(5,("ess_set_port: port=%d num_channels=%d\n",
		    cp->dev, cp->un.value.num_channels));

	switch (cp->dev) {
	/*
	 * The following mixer ports are all stereo. If we get a
	 * single-channel gain value passed in, then we duplicate it
	 * to both left and right channels.
	 */
	case ESS_MASTER_VOL:
	case ESS_DAC_PLAY_VOL:
	case ESS_MIC_PLAY_VOL:
	case ESS_LINE_PLAY_VOL:
	case ESS_SYNTH_PLAY_VOL:
	case ESS_CD_PLAY_VOL:
	case ESS_AUXB_PLAY_VOL:
	case ESS_RECORD_VOL:
		if (cp->type != AUDIO_MIXER_VALUE)
			return EINVAL;

		switch (cp->un.value.num_channels) {
		case 1:
			lgain = rgain = ESS_4BIT_GAIN(
			  cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
			break;
		case 2:
			lgain = ESS_4BIT_GAIN(
			  cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT]);
			rgain = ESS_4BIT_GAIN(
			  cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT]);
			break;
		default:
			return EINVAL;
		}

		sc->gain[cp->dev][ESS_LEFT]  = lgain;
		sc->gain[cp->dev][ESS_RIGHT] = rgain;
		ess_set_gain(sc, cp->dev, 1);
		return 0;

	/*
	 * The PC speaker port is mono. If we get a stereo gain value
	 * passed in, then we return EINVAL.
	 */
	case ESS_PCSPEAKER_VOL:
		if (cp->un.value.num_channels != 1)
			return EINVAL;

		sc->gain[cp->dev][ESS_LEFT] = sc->gain[cp->dev][ESS_RIGHT] =
		  ESS_3BIT_GAIN(cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
		ess_set_gain(sc, cp->dev, 1);
		return 0;

	case ESS_RECORD_SOURCE:
		if (ESS_USE_AUDIO1(sc->sc_model)) {
			if (cp->type == AUDIO_MIXER_ENUM)
				return ess_set_in_port(sc, cp->un.ord);
			else
				return EINVAL;
		} else {
			if (cp->type == AUDIO_MIXER_SET)
				return ess_set_in_ports(sc, cp->un.mask);
			else
				return EINVAL;
		}
		return 0;

	case ESS_RECORD_MONITOR:
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;

		if (cp->un.ord)
			/* Enable monitor */
			ess_set_xreg_bits(sc, ESS_XCMD_AUDIO_CTRL,
					  ESS_AUDIO_CTRL_MONITOR);
		else
			/* Disable monitor */
			ess_clear_xreg_bits(sc, ESS_XCMD_AUDIO_CTRL,
					    ESS_AUDIO_CTRL_MONITOR);
		return 0;
	}

	if (ESS_IS_ES18X9(sc->sc_model)) {

		switch (cp->dev) {
		case ESS_SPATIALIZER:
			if (cp->type != AUDIO_MIXER_VALUE ||
			    cp->un.value.num_channels != 1)
				return EINVAL;

			sc->gain[cp->dev][ESS_LEFT] =
				sc->gain[cp->dev][ESS_RIGHT] = ESS_6BIT_GAIN(
				    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
			ess_set_gain(sc, cp->dev, 1);
			return 0;

		case ESS_SPATIALIZER_ENABLE:
			if (cp->type != AUDIO_MIXER_ENUM)
				return EINVAL;

			sc->sc_spatializer = (cp->un.ord != 0);
			if (sc->sc_spatializer)
				ess_set_mreg_bits(sc, ESS_MREG_SPATIAL_CTRL,
				    ESS_SPATIAL_CTRL_ENABLE);
			else
				ess_clear_mreg_bits(sc, ESS_MREG_SPATIAL_CTRL,
				    ESS_SPATIAL_CTRL_ENABLE);
			return 0;
		}
	}

	if (ESS_USE_AUDIO1(sc->sc_model))
		return EINVAL;

	switch (cp->dev) {
	case ESS_DAC_REC_VOL:
	case ESS_MIC_REC_VOL:
	case ESS_LINE_REC_VOL:
	case ESS_SYNTH_REC_VOL:
	case ESS_CD_REC_VOL:
	case ESS_AUXB_REC_VOL:
		if (cp->type != AUDIO_MIXER_VALUE)
			return EINVAL;

		switch (cp->un.value.num_channels) {
		case 1:
			lgain = rgain = ESS_4BIT_GAIN(
			  cp->un.value.level[AUDIO_MIXER_LEVEL_MONO]);
			break;
		case 2:
			lgain = ESS_4BIT_GAIN(
			  cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT]);
			rgain = ESS_4BIT_GAIN(
			  cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT]);
			break;
		default:
			return EINVAL;
		}

		sc->gain[cp->dev][ESS_LEFT]  = lgain;
		sc->gain[cp->dev][ESS_RIGHT] = rgain;
		ess_set_gain(sc, cp->dev, 1);
		return 0;

	case ESS_MIC_PREAMP:
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;

		if (cp->un.ord)
			/* Enable microphone preamp */
			ess_set_xreg_bits(sc, ESS_XCMD_PREAMP_CTRL,
					  ESS_PREAMP_CTRL_ENABLE);
		else
			/* Disable microphone preamp */
			ess_clear_xreg_bits(sc, ESS_XCMD_PREAMP_CTRL,
					  ESS_PREAMP_CTRL_ENABLE);
		return 0;
	}

	return EINVAL;
}

int
ess_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct ess_softc *sc;

	sc = addr;
	DPRINTFN(5,("ess_get_port: port=%d\n", cp->dev));

	switch (cp->dev) {
	case ESS_MASTER_VOL:
	case ESS_DAC_PLAY_VOL:
	case ESS_MIC_PLAY_VOL:
	case ESS_LINE_PLAY_VOL:
	case ESS_SYNTH_PLAY_VOL:
	case ESS_CD_PLAY_VOL:
	case ESS_AUXB_PLAY_VOL:
	case ESS_RECORD_VOL:
		switch (cp->un.value.num_channels) {
		case 1:
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
				sc->gain[cp->dev][ESS_LEFT];
			break;
		case 2:
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
				sc->gain[cp->dev][ESS_LEFT];
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
				sc->gain[cp->dev][ESS_RIGHT];
			break;
		default:
			return EINVAL;
		}
		return 0;

	case ESS_PCSPEAKER_VOL:
		if (cp->un.value.num_channels != 1)
			return EINVAL;

		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
			sc->gain[cp->dev][ESS_LEFT];
		return 0;

	case ESS_RECORD_SOURCE:
		if (ESS_USE_AUDIO1(sc->sc_model))
			cp->un.ord = sc->in_port;
		else
			cp->un.mask = sc->in_mask;
		return 0;

	case ESS_RECORD_MONITOR:
		cp->un.ord = (ess_read_x_reg(sc, ESS_XCMD_AUDIO_CTRL) &
			      ESS_AUDIO_CTRL_MONITOR) ? 1 : 0;
		return 0;
	}

	if (ESS_IS_ES18X9(sc->sc_model)) {

		switch (cp->dev) {
		case ESS_SPATIALIZER:
			if (cp->un.value.num_channels != 1)
				return EINVAL;

			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
				sc->gain[cp->dev][ESS_LEFT];
			return 0;

		case ESS_SPATIALIZER_ENABLE:
			cp->un.ord = sc->sc_spatializer;
			return 0;
		}
	}

	if (ESS_USE_AUDIO1(sc->sc_model))
		return EINVAL;

	switch (cp->dev) {
	case ESS_DAC_REC_VOL:
	case ESS_MIC_REC_VOL:
	case ESS_LINE_REC_VOL:
	case ESS_SYNTH_REC_VOL:
	case ESS_CD_REC_VOL:
	case ESS_AUXB_REC_VOL:
		switch (cp->un.value.num_channels) {
		case 1:
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
				sc->gain[cp->dev][ESS_LEFT];
			break;
		case 2:
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
				sc->gain[cp->dev][ESS_LEFT];
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
				sc->gain[cp->dev][ESS_RIGHT];
			break;
		default:
			return EINVAL;
		}
		return 0;

	case ESS_MIC_PREAMP:
		cp->un.ord = (ess_read_x_reg(sc, ESS_XCMD_PREAMP_CTRL) &
			      ESS_PREAMP_CTRL_ENABLE) ? 1 : 0;
		return 0;
	}

	return EINVAL;
}

int
ess_query_devinfo(void *addr, mixer_devinfo_t *dip)
{
	struct ess_softc *sc;

	sc = addr;
	DPRINTFN(5,("ess_query_devinfo: model=%d index=%d\n",
		    sc->sc_model, dip->index));

	/*
	 * REVISIT: There are some slight differences between the
	 *          mixers on the different ESS chips, which can
	 *          be sorted out using the chip model rather than a
	 *          separate mixer model.
	 *          This is currently coded assuming an ES1887; we
	 *          need to work out which bits are not applicable to
	 *          the other models (1888 and 888).
	 */
	switch (dip->index) {
	case ESS_DAC_PLAY_VOL:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNdac);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_MIC_PLAY_VOL:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		if (ESS_USE_AUDIO1(sc->sc_model))
			dip->next = AUDIO_MIXER_LAST;
		else
			dip->next = ESS_MIC_PREAMP;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_LINE_PLAY_VOL:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNline);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_SYNTH_PLAY_VOL:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNfmsynth);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_CD_PLAY_VOL:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNcd);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_AUXB_PLAY_VOL:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "auxb");
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_INPUT_CLASS:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCinputs);
		dip->type = AUDIO_MIXER_CLASS;
		return 0;

	case ESS_MASTER_VOL:
		dip->mixer_class = ESS_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmaster);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_PCSPEAKER_VOL:
		dip->mixer_class = ESS_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "pc_speaker");
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 1;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_OUTPUT_CLASS:
		dip->mixer_class = ESS_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCoutputs);
		dip->type = AUDIO_MIXER_CLASS;
		return 0;

	case ESS_RECORD_VOL:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNrecord);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_RECORD_SOURCE:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNsource);
		if (ESS_USE_AUDIO1(sc->sc_model)) {
			/*
			 * The 1788 doesn't use the input mixer control that
			 * the 1888 uses, because it's a pain when you only
			 * have one mixer.
			 * Perhaps it could be emulated by keeping both sets of
			 * gain values, and doing a `context switch' of the
			 * mixer registers when shifting from playing to
			 * recording.
			 */
			dip->type = AUDIO_MIXER_ENUM;
			dip->un.e.num_mem = 4;
			strcpy(dip->un.e.member[0].label.name, AudioNmicrophone);
			dip->un.e.member[0].ord = ESS_SOURCE_MIC;
			strcpy(dip->un.e.member[1].label.name, AudioNline);
			dip->un.e.member[1].ord = ESS_SOURCE_LINE;
			strcpy(dip->un.e.member[2].label.name, AudioNcd);
			dip->un.e.member[2].ord = ESS_SOURCE_CD;
			strcpy(dip->un.e.member[3].label.name, AudioNmixerout);
			dip->un.e.member[3].ord = ESS_SOURCE_MIXER;
		} else {
			dip->type = AUDIO_MIXER_SET;
			dip->un.s.num_mem = 6;
			strcpy(dip->un.s.member[0].label.name, AudioNdac);
			dip->un.s.member[0].mask = 1 << ESS_DAC_REC_VOL;
			strcpy(dip->un.s.member[1].label.name, AudioNmicrophone);
			dip->un.s.member[1].mask = 1 << ESS_MIC_REC_VOL;
			strcpy(dip->un.s.member[2].label.name, AudioNline);
			dip->un.s.member[2].mask = 1 << ESS_LINE_REC_VOL;
			strcpy(dip->un.s.member[3].label.name, AudioNfmsynth);
			dip->un.s.member[3].mask = 1 << ESS_SYNTH_REC_VOL;
			strcpy(dip->un.s.member[4].label.name, AudioNcd);
			dip->un.s.member[4].mask = 1 << ESS_CD_REC_VOL;
			strcpy(dip->un.s.member[5].label.name, "auxb");
			dip->un.s.member[5].mask = 1 << ESS_AUXB_REC_VOL;
		}
		return 0;

	case ESS_RECORD_CLASS:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCrecord);
		dip->type = AUDIO_MIXER_CLASS;
		return 0;

	case ESS_RECORD_MONITOR:
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmute);
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = ESS_MONITOR_CLASS;
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNon);
		dip->un.e.member[1].ord = 1;
		return 0;

	case ESS_MONITOR_CLASS:
		dip->mixer_class = ESS_MONITOR_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCmonitor);
		dip->type = AUDIO_MIXER_CLASS;
		return 0;
	}

	if (ESS_IS_ES18X9(sc->sc_model)) {

		switch (dip->index) {
		case ESS_SPATIALIZER:
			dip->mixer_class = ESS_OUTPUT_CLASS;
			dip->prev = AUDIO_MIXER_LAST;
			dip->next = ESS_SPATIALIZER_ENABLE;
			strcpy(dip->label.name, AudioNspatial);
			dip->type = AUDIO_MIXER_VALUE;
			dip->un.v.num_channels = 1;
			strcpy(dip->un.v.units.name, "level");
			return 0;

		case ESS_SPATIALIZER_ENABLE:
			dip->mixer_class = ESS_OUTPUT_CLASS;
			dip->prev = ESS_SPATIALIZER;
			dip->next = AUDIO_MIXER_LAST;
			strcpy(dip->label.name, "enable");
			dip->type = AUDIO_MIXER_ENUM;
			dip->un.e.num_mem = 2;
			strcpy(dip->un.e.member[0].label.name, AudioNoff);
			dip->un.e.member[0].ord = 0;
			strcpy(dip->un.e.member[1].label.name, AudioNon);
			dip->un.e.member[1].ord = 1;
			return 0;
		}
	}

	if (ESS_USE_AUDIO1(sc->sc_model))
		return ENXIO;

	switch (dip->index) {
	case ESS_DAC_REC_VOL:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNdac);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_MIC_REC_VOL:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_LINE_REC_VOL:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNline);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_SYNTH_REC_VOL:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNfmsynth);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_CD_REC_VOL:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNcd);
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_AUXB_REC_VOL:
		dip->mixer_class = ESS_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, "auxb");
		dip->type = AUDIO_MIXER_VALUE;
		dip->un.v.num_channels = 2;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case ESS_MIC_PREAMP:
		dip->mixer_class = ESS_INPUT_CLASS;
		dip->prev = ESS_MIC_PLAY_VOL;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNpreamp);
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNon);
		dip->un.e.member[1].ord = 1;
		return 0;
	}

	return ENXIO;
}

void *
ess_malloc(void *addr, int direction, size_t size)
{
	struct ess_softc *sc;
	int drq;

	sc = addr;
	if ((!ESS_USE_AUDIO1(sc->sc_model)) && direction == AUMODE_PLAY)
		drq = sc->sc_audio2.drq;
	else
		drq = sc->sc_audio1.drq;
	return (isa_malloc(sc->sc_ic, drq, size, M_DEVBUF, M_WAITOK));
}

void
ess_free(void *addr, void *ptr, size_t size)
{

	isa_free(ptr, M_DEVBUF);
}

size_t
ess_round_buffersize(void *addr, int direction, size_t size)
{
	struct ess_softc *sc;
	bus_size_t maxsize;

	sc = addr;
	if ((!ESS_USE_AUDIO1(sc->sc_model)) && direction == AUMODE_PLAY)
		maxsize = sc->sc_audio2.maxsize;
	else
		maxsize = sc->sc_audio1.maxsize;

	if (size > maxsize)
		size = maxsize;
	return size;
}

paddr_t
ess_mappage(void *addr, void *mem, off_t off, int prot)
{

	return isa_mappage(mem, off, prot);
}

int
ess_1788_get_props(void *addr)
{

	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT;
}

int
ess_1888_get_props(void *addr)
{

	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
}

void
ess_get_locks(void *addr, kmutex_t **intr, kmutex_t **thread)
{
	struct ess_softc *sc;

	sc = addr;
	*intr = &sc->sc_intr_lock;
	*thread = &sc->sc_lock;
}


/* ============================================
 * Generic functions for ess, not used by audio h/w i/f
 * =============================================
 */

/*
 * Reset the chip.
 * Return non-zero if the chip isn't detected.
 */
int
ess_reset(struct ess_softc *sc)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	sc->sc_audio1.active = 0;
	sc->sc_audio2.active = 0;

	EWRITE1(iot, ioh, ESS_DSP_RESET, ESS_RESET_EXT);
	delay(10000);		/* XXX shouldn't delay so long */
	EWRITE1(iot, ioh, ESS_DSP_RESET, 0);
	if (ess_rdsp(sc) != ESS_MAGIC)
		return 1;

	/* Enable access to the ESS extension commands. */
	ess_wdsp(sc, ESS_ACMD_ENABLE_EXT);

	return 0;
}

void
ess_set_gain(struct ess_softc *sc, int port, int on)
{
	int gain, left, right;
	int mix;
	int src;
	int stereo;

	/*
	 * Most gain controls are found in the mixer registers and
	 * are stereo. Any that are not, must set mix and stereo as
	 * required.
	 */
	mix = 1;
	stereo = 1;

	if (ESS_IS_ES18X9(sc->sc_model)) {
		switch (port) {
		case ESS_SPATIALIZER:
			src = ESS_MREG_SPATIAL_LEVEL;
			stereo = -1;
			goto skip;
		case ESS_SPATIALIZER_ENABLE:
			return;
		}
	}
	switch (port) {
	case ESS_MASTER_VOL:
		src = ESS_MREG_VOLUME_MASTER;
		break;
	case ESS_DAC_PLAY_VOL:
		if (ESS_USE_AUDIO1(sc->sc_model))
			src = ESS_MREG_VOLUME_VOICE;
		else
			src = 0x7C;
		break;
	case ESS_MIC_PLAY_VOL:
		src = ESS_MREG_VOLUME_MIC;
		break;
	case ESS_LINE_PLAY_VOL:
		src = ESS_MREG_VOLUME_LINE;
		break;
	case ESS_SYNTH_PLAY_VOL:
		src = ESS_MREG_VOLUME_SYNTH;
		break;
	case ESS_CD_PLAY_VOL:
		src = ESS_MREG_VOLUME_CD;
		break;
	case ESS_AUXB_PLAY_VOL:
		src = ESS_MREG_VOLUME_AUXB;
		break;
	case ESS_PCSPEAKER_VOL:
		src = ESS_MREG_VOLUME_PCSPKR;
		stereo = 0;
		break;
	case ESS_DAC_REC_VOL:
		src = 0x69;
		break;
	case ESS_MIC_REC_VOL:
		src = 0x68;
		break;
	case ESS_LINE_REC_VOL:
		src = 0x6E;
		break;
	case ESS_SYNTH_REC_VOL:
		src = 0x6B;
		break;
	case ESS_CD_REC_VOL:
		src = 0x6A;
		break;
	case ESS_AUXB_REC_VOL:
		src = 0x6C;
		break;
	case ESS_RECORD_VOL:
		src = ESS_XCMD_VOLIN_CTRL;
		mix = 0;
		break;
	default:
		return;
	}
skip:

	/* 1788 doesn't have a separate recording mixer */
	if (ESS_USE_AUDIO1(sc->sc_model) && mix && src > 0x62)
		return;

	if (on) {
		left = sc->gain[port][ESS_LEFT];
		right = sc->gain[port][ESS_RIGHT];
	} else {
		left = right = 0;
	}

	if (stereo == -1)
		gain = ESS_SPATIAL_GAIN(left);
	else if (stereo)
		gain = ESS_STEREO_GAIN(left, right);
	else
		gain = ESS_MONO_GAIN(left);

	if (mix)
		ess_write_mix_reg(sc, src, gain);
	else
		ess_write_x_reg(sc, src, gain);
}

/* Set the input device on devices without an input mixer. */
int
ess_set_in_port(struct ess_softc *sc, int ord)
{
	mixer_devinfo_t di;
	int i;

	DPRINTF(("ess_set_in_port: ord=0x%x\n", ord));

	/*
	 * Get the device info for the record source control,
	 * including the list of available sources.
	 */
	di.index = ESS_RECORD_SOURCE;
	if (ess_query_devinfo(sc, &di))
		return EINVAL;

	/* See if the given ord value was anywhere in the list. */
	for (i = 0; i < di.un.e.num_mem; i++) {
		if (ord == di.un.e.member[i].ord)
			break;
	}
	if (i == di.un.e.num_mem)
		return EINVAL;

	ess_write_mix_reg(sc, ESS_MREG_ADC_SOURCE, ord);

	sc->in_port = ord;
	return 0;
}

/* Set the input device levels on input-mixer-enabled devices. */
int
ess_set_in_ports(struct ess_softc *sc, int mask)
{
	mixer_devinfo_t di;
	int i, port;

	DPRINTF(("ess_set_in_ports: mask=0x%x\n", mask));

	/*
	 * Get the device info for the record source control,
	 * including the list of available sources.
	 */
	di.index = ESS_RECORD_SOURCE;
	if (ess_query_devinfo(sc, &di))
		return EINVAL;

	/*
	 * Set or disable the record volume control for each of the
	 * possible sources.
	 */
	for (i = 0; i < di.un.s.num_mem; i++) {
		/*
		 * Calculate the source port number from its mask.
		 */
		port = ffs(di.un.s.member[i].mask);

		/*
		 * Set the source gain:
		 *	to the current value if source is enabled
		 *	to zero if source is disabled
		 */
		ess_set_gain(sc, port, mask & di.un.s.member[i].mask);
	}

	sc->in_mask = mask;
	return 0;
}

void
ess_speaker_on(struct ess_softc *sc)
{

	/* Unmute the DAC. */
	ess_set_gain(sc, ESS_DAC_PLAY_VOL, 1);
}

void
ess_speaker_off(struct ess_softc *sc)
{

	/* Mute the DAC. */
	ess_set_gain(sc, ESS_DAC_PLAY_VOL, 0);
}

/*
 * Calculate the time constant for the requested sampling rate.
 */
u_int
ess_srtotc(struct ess_softc *sc, u_int rate)
{
	u_int tc;

	/* The following formulae are from the ESS data sheet. */
	if (ESS_IS_ES18X9(sc->sc_model)) {
		if ((rate % 8000) != 0)
			tc = 128 - 793800L / rate;
		else
			tc = 256 - 768000L / rate;
	} else {
		if (rate <= 22050)
			tc = 128 - 397700L / rate;
		else
			tc = 256 - 795500L / rate;
	}

	return tc;
}


/*
 * Calculate the filter constant for the reuqested sampling rate.
 */
u_int
ess_srtofc(u_int rate)
{
	/*
	 * The following formula is derived from the information in
	 * the ES1887 data sheet, based on a roll-off frequency of
	 * 87%.
	 */
	return 256 - 200279L / rate;
}


/*
 * Return the status of the DSP.
 */
u_char
ess_get_dsp_status(struct ess_softc *sc)
{
	return EREAD1(sc->sc_iot, sc->sc_ioh, ESS_DSP_RW_STATUS);
}


/*
 * Return the read status of the DSP:	1 -> DSP ready for reading
 *					0 -> DSP not ready for reading
 */
u_char
ess_dsp_read_ready(struct ess_softc *sc)
{

	return (ess_get_dsp_status(sc) & ESS_DSP_READ_READY) ? 1 : 0;
}


/*
 * Return the write status of the DSP:	1 -> DSP ready for writing
 *					0 -> DSP not ready for writing
 */
u_char
ess_dsp_write_ready(struct ess_softc *sc)
{
	return (ess_get_dsp_status(sc) & ESS_DSP_WRITE_BUSY) ? 0 : 1;
}


/*
 * Read a byte from the DSP.
 */
int
ess_rdsp(struct ess_softc *sc)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int i;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	for (i = ESS_READ_TIMEOUT; i > 0; --i) {
		if (ess_dsp_read_ready(sc)) {
			i = EREAD1(iot, ioh, ESS_DSP_READ);
			DPRINTFN(8,("ess_rdsp() = 0x%02x\n", i));
			return i;
		} else
			delay(10);
	}

	DPRINTF(("ess_rdsp: timed out\n"));
	return -1;
}

/*
 * Write a byte to the DSP.
 */
int
ess_wdsp(struct ess_softc *sc, u_char v)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int i;

	DPRINTFN(8,("ess_wdsp(0x%02x)\n", v));

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	for (i = ESS_WRITE_TIMEOUT; i > 0; --i) {
		if (ess_dsp_write_ready(sc)) {
			EWRITE1(iot, ioh, ESS_DSP_WRITE, v);
			return 0;
		} else
			delay(10);
	}

	DPRINTF(("ess_wdsp(0x%02x): timed out\n", v));
	return -1;
}

/*
 * Write a value to one of the ESS extended registers.
 */
int
ess_write_x_reg(struct ess_softc *sc, u_char reg, u_char val)
{
	int error;

	DPRINTFN(2,("ess_write_x_reg: %02x=%02x\n", reg, val));
	if ((error = ess_wdsp(sc, reg)) == 0)
		error = ess_wdsp(sc, val);

	return error;
}

/*
 * Read the value of one of the ESS extended registers.
 */
u_char
ess_read_x_reg(struct ess_softc *sc, u_char reg)
{
	int error;
	int val;

	if ((error = ess_wdsp(sc, 0xC0)) == 0)
		error = ess_wdsp(sc, reg);
	if (error) {
		DPRINTF(("Error reading extended register 0x%02x\n", reg));
	}
/* REVISIT: what if an error is returned above? */
	val = ess_rdsp(sc);
	DPRINTFN(2,("ess_read_x_reg: %02x=%02x\n", reg, val));
	return val;
}

void
ess_clear_xreg_bits(struct ess_softc *sc, u_char reg, u_char mask)
{
	if (ess_write_x_reg(sc, reg, ess_read_x_reg(sc, reg) & ~mask) == -1) {
		DPRINTF(("Error clearing bits in extended register 0x%02x\n",
			 reg));
	}
}

void
ess_set_xreg_bits(struct ess_softc *sc, u_char reg, u_char mask)
{
	if (ess_write_x_reg(sc, reg, ess_read_x_reg(sc, reg) | mask) == -1) {
		DPRINTF(("Error setting bits in extended register 0x%02x\n",
			 reg));
	}
}


/*
 * Write a value to one of the ESS mixer registers.
 */
void
ess_write_mix_reg(struct ess_softc *sc, u_char reg, u_char val)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	DPRINTFN(2,("ess_write_mix_reg: %x=%x\n", reg, val));

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	EWRITE1(iot, ioh, ESS_MIX_REG_SELECT, reg);
	EWRITE1(iot, ioh, ESS_MIX_REG_DATA, val);
}

/*
 * Read the value of one of the ESS mixer registers.
 */
u_char
ess_read_mix_reg(struct ess_softc *sc, u_char reg)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_char val;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	EWRITE1(iot, ioh, ESS_MIX_REG_SELECT, reg);
	val = EREAD1(iot, ioh, ESS_MIX_REG_DATA);

	DPRINTFN(2,("ess_read_mix_reg: %x=%x\n", reg, val));
	return val;
}

void
ess_clear_mreg_bits(struct ess_softc *sc, u_char reg, u_char mask)
{

	ess_write_mix_reg(sc, reg, ess_read_mix_reg(sc, reg) & ~mask);
}

void
ess_set_mreg_bits(struct ess_softc *sc, u_char reg, u_char mask)
{

	ess_write_mix_reg(sc, reg, ess_read_mix_reg(sc, reg) | mask);
}

void
ess_read_multi_mix_reg(struct ess_softc *sc, u_char reg,
		       uint8_t *datap, bus_size_t count)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	EWRITE1(iot, ioh, ESS_MIX_REG_SELECT, reg);
	bus_space_read_multi_1(iot, ioh, ESS_MIX_REG_DATA, datap, count);
}
