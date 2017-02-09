/*	$NetBSD: audiovar.h,v 1.46 2011/11/23 23:07:31 jmcneill Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by TAMURA Kent
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
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 *	From: Header: audiovar.h,v 1.3 93/07/18 14:07:25 mccanne Exp  (LBL)
 */
#ifndef _SYS_DEV_AUDIOVAR_H_
#define _SYS_DEV_AUDIOVAR_H_

#include <sys/condvar.h>

#include <dev/audio_if.h>

/*
 * Initial/default block duration is both configurable and patchable.
 */
#ifndef AUDIO_BLK_MS
#define AUDIO_BLK_MS	50	/* 50 ms */
#endif

#ifndef AU_RING_SIZE
#define AU_RING_SIZE	65536
#endif

#define AUMINBUF	512
#define AUMINBLK	32
#define AUMINNOBLK	3
struct audio_ringbuffer {
	audio_stream_t s;
	int	blksize;	/* I/O block size (bytes) */
	int	maxblks;	/* no of blocks in ring */
	int	usedlow;	/* start writer when used falls below this */
	int	usedhigh;	/* stop writer when used goes above this */
	u_long	stamp;		/* bytes transferred */
	u_long	stamp_last;	/* old value of bytes transferred */
	u_long	fstamp;		/* bytes transferred from/to the buffer near to userland */
	u_long	drops;		/* missed samples from over/underrun */
	u_long	pdrops;		/* paused samples */
	bool pause;		/* transfer is paused */
	bool copying;		/* data is being copied */
	bool needfill;		/* buffer needs filling when copying is done */
	bool mmapped;		/* device is mmap()-ed */
};

#define AUDIO_N_PORTS 4

struct au_mixer_ports {
	int	index;		/* index of port-selector mixerctl */
	int	master;		/* index of master mixerctl */
	int	nports;		/* number of selectable ports */
	bool	isenum;		/* selector is enum type */
	u_int	allports;	/* all aumasks or'd */
	u_int	aumask[AUDIO_N_PORTS];	/* exposed value of "ports" */
	u_int	misel [AUDIO_N_PORTS];	/* ord of port, for selector */
	u_int	miport[AUDIO_N_PORTS];	/* index of port's mixerctl */
	bool	isdual;		/* has working mixerout */
	int	mixerout;	/* ord of mixerout, for dual case */
	int	cur_port;	/* the port that gain actually controls when
				   mixerout is selected, for dual case */
};

/*
 * Software state, per audio device.
 */
struct audio_softc {
	device_t	dev;
	void		*hw_hdl;	/* Hardware driver handle */
	const struct audio_hw_if *hw_if; /* Hardware interface */
	device_t	sc_dev;		/* Hardware device struct */
	u_char		sc_open;	/* single use device */
#define AUOPEN_READ	0x01
#define AUOPEN_WRITE	0x02
	u_char		sc_mode;	/* bitmask for RECORD/PLAY */

	struct	selinfo sc_wsel; /* write selector */
	struct	selinfo sc_rsel; /* read selector */
	pid_t		sc_async_audio;	/* process who wants audio SIGIO */
	void		*sc_sih_rd;
	void		*sc_sih_wr;
	struct	mixer_asyncs {
		struct mixer_asyncs *next;
		pid_t	pid;
	} *sc_async_mixer;  /* processes who want mixer SIGIO */

	/* Locks and sleep channels for reading, writing and draining. */
	kmutex_t	*sc_intr_lock;
	kmutex_t	*sc_lock;
	kcondvar_t	sc_rchan;
	kcondvar_t	sc_wchan;
	kcondvar_t	sc_lchan;
	int		sc_dvlock;
	bool		sc_dying;

	bool		sc_blkset;	/* Blocksize has been set */

	uint8_t		*sc_sil_start;	/* start of silence in buffer */
	int		sc_sil_count;	/* # of silence bytes */

	bool		sc_rbus;	/* input DMA in progress */
	bool		sc_pbus;	/* output DMA in progress */

	/**
	 *  userland
	 *	|  write(2) & uiomove(9)
	 *  sc_pstreams[0]	<sc_pparams> == sc_pustream;
	 *      |  sc_pfilters[0]
	 *  sc_pstreams[1]	<list_t::filters[n-1].param>
	 *      :
	 *  sc_pstreams[n-1]	<list_t::filters[1].param>
	 *      |  sc_pfilters[n-1]
	 *    sc_pr		<list_t::filters[0].param>
	 *      |
	 *  hardware
	 */
	audio_params_t		sc_pparams;	/* play encoding parameters */
	audio_stream_t		*sc_pustream;	/* the first buffer */
	int			sc_npfilters;	/* number of filters */
	audio_stream_t		sc_pstreams[AUDIO_MAX_FILTERS];
	stream_filter_t		*sc_pfilters[AUDIO_MAX_FILTERS];
	struct audio_ringbuffer	sc_pr;		/* Play ring */

	/**
	 *  hardware
	 *	|
	 *    sc_rr		<list_t::filters[0].param>
	 *	|  sc_rfilters[0]
	 *  sc_rstreams[0]	<list_t::filters[1].param>
	 *      |  sc_rfilters[1]
	 *  sc_rstreams[1]	<list_t::filters[2].param>
	 *      :
	 *	|  sc_rfilters[n-1]
	 *  sc_rstreams[n-1]	<sc_rparams> == sc_rustream
	 *      |  uiomove(9) & read(2)
	 *  userland
	 */
	struct audio_ringbuffer	sc_rr;		/* Record ring */
	int			sc_nrfilters;	/* number of filters */
	stream_filter_t		*sc_rfilters[AUDIO_MAX_FILTERS];
	audio_stream_t		sc_rstreams[AUDIO_MAX_FILTERS];
	audio_stream_t		*sc_rustream;	/* the last buffer */
	audio_params_t		sc_rparams;	/* record encoding parameters */

	int		sc_eof;		/* EOF, i.e. zero sized write, counter */
	u_long		sc_wstamp;	/* # of bytes read with read(2) */
	u_long		sc_playdrop;

	int		sc_full_duplex;	/* device in full duplex mode */

	struct	au_mixer_ports sc_inports, sc_outports;
	int		sc_monitor_port;

#ifdef AUDIO_INTR_TIME
	u_long	sc_pfirstintr;	/* first time we saw a play interrupt */
	int	sc_pnintr;	/* number of interrupts */
	u_long	sc_plastintr;	/* last time we saw a play interrupt */
	long	sc_pblktime;	/* nominal time between interrupts */
	u_long	sc_rfirstintr;	/* first time we saw a rec interrupt */
	int	sc_rnintr;	/* number of interrupts */
	u_long	sc_rlastintr;	/* last time we saw a rec interrupt */
	long	sc_rblktime;	/* nominal time between interrupts */
#endif

	u_int	sc_lastgain;
	struct audio_info sc_lastinfo;
	bool	sc_lastinfovalid;

	mixer_ctrl_t	*sc_mixer_state;
	int		sc_nmixer_states;
};

#endif /* _SYS_DEV_AUDIOVAR_H_ */
