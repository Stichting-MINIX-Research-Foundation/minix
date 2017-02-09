/*	$NetBSD: essvar.h,v 1.27 2014/08/16 13:01:33 nakayama Exp $	*/
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
** @(#) $RCSfile: essvar.h,v $ $Revision: 1.27 $ (SHARK) $Date: 2014/08/16 13:01:33 $
**
**++
**
**  essvar.h
**
**  FACILITY:
**
**	DIGITAL Network Appliance Reference Design (DNARD)
**
**  MODULE DESCRIPTION:
**
**      This module contains the structure definitions and function
**      prototypes for the ESS Technologies 1887/888 sound chip
**      driver.
**
**  AUTHORS:
**
**	Blair Fidler	Software Engineering Australia
**			Gold Coast, Australia.
**
**  CREATION DATE:
**
**	May 12, 1997.
**
**  MODIFICATION HISTORY:
**
**--
*/

#include <sys/callout.h>

#define ESS_DAC_PLAY_VOL	0
#define ESS_MIC_PLAY_VOL	1
#define ESS_LINE_PLAY_VOL	2
#define ESS_SYNTH_PLAY_VOL	3
#define ESS_CD_PLAY_VOL		4
#define ESS_AUXB_PLAY_VOL	5
#define ESS_INPUT_CLASS		6

#define ESS_MASTER_VOL		7
#define ESS_PCSPEAKER_VOL	8
#define ESS_OUTPUT_CLASS	9

#define ESS_RECORD_MONITOR	10
#define ESS_MONITOR_CLASS	11

#define ESS_RECORD_VOL		12
#define	ESS_RECORD_SOURCE	13
#define ESS_RECORD_CLASS	14

#define ESS_1788_NDEVS		15

#define ESS_SPATIALIZER		15
#define ESS_SPATIALIZER_ENABLE	16

#define ESS_18X9_NDEVS		17

#define ESS_DAC_REC_VOL		15
#define ESS_MIC_REC_VOL		16
#define ESS_LINE_REC_VOL	17
#define ESS_SYNTH_REC_VOL	18
#define ESS_CD_REC_VOL		19
#define ESS_AUXB_REC_VOL	20
#define ESS_MIC_PREAMP		21

#define ESS_1888_NDEVS		22
#define ESS_MAX_NDEVS		22

struct ess_audio_channel
{
	int	drq;			/* DMA channel */
	bus_size_t maxsize;		/* max size for DMA channel */
#define IS16BITDRQ(drq) ((drq) >= 4)
	int	irq;			/* IRQ line for this DMA channel */
	int	ist;
	void	*ih;			/* interrupt vectoring */
	u_long	nintr;			/* number of interrupts taken */
	void	(*intr)(void*);		/* ISR for DMA complete */
	void	*arg;			/* arg for intr() */

	/* Status information */
	int	active;			/* boolean: channel in use? */

	/* Polling state */
	int	polled;			/* 1 if channel is polled */
	int	dmapos;			/* last DMA pointer */
	int	buffersize;		/* size of DMA buffer */
	/* (The following is only needed due to the stupid block interface.) */
	int	dmacount;		/* leftover partial block */
	int	blksize;		/* current block size */
};

struct ess_softc
{
	device_t sc_dev;		/* base device */
	isa_chipset_tag_t sc_ic;
	bus_space_tag_t sc_iot;		/* tag */
	bus_space_handle_t sc_ioh;	/* handle */
	kmutex_t sc_lock;
	kmutex_t sc_intr_lock;

	callout_t sc_poll1_ch;		/* audio1 poll */
	callout_t sc_poll2_ch;		/* audio2 poll */

	int	sc_iobase;		/* I/O port base address */

	u_short	sc_open;		/* reference count of open calls */

	int ndevs;
	u_char	gain[ESS_MAX_NDEVS][2];	/* kept in input levels */
#define ESS_LEFT 0
#define ESS_RIGHT 1

	u_int	out_port;		/* output port */
	u_int	in_mask;		/* input ports */
	u_int	in_port;		/* XXX needed for MI interface */

	u_int	spkr_state;		/* non-null is on */

	struct ess_audio_channel sc_audio1; /* audio channel for record */
	struct ess_audio_channel sc_audio2; /* audio channel for playback */

	u_int	sc_model;
#define ESS_UNSUPPORTED 0
#define	ESS_688		1
#define	ESS_1688	2
#define ESS_1788	3
#define ESS_1868	4
#define ESS_1869	5
#define ESS_1878	6
#define ESS_1879	7
#define ESS_888		8
#define ESS_1887	9
#define ESS_1888	10

	u_int	sc_version;		/* Legacy ES688/ES1688 ID */

	u_int	sc_spatializer;		/* spatializer enable */

	/* game port on es1888 */
	bus_space_tag_t sc_joy_iot;
	bus_space_handle_t sc_joy_ioh;
};

int	essmatch(struct ess_softc *);
void	essattach(struct ess_softc *, int);
int	ess_config_addr(struct ess_softc *);

