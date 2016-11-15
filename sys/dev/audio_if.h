/*	$NetBSD: audio_if.h,v 1.70 2014/11/18 01:50:12 jmcneill Exp $	*/

/*
 * Copyright (c) 1994 Havard Eidnes.
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
 */

#ifndef _SYS_DEV_AUDIO_IF_H_
#define _SYS_DEV_AUDIO_IF_H_

#include <sys/types.h>
#include <sys/audioio.h>
#include <sys/mutex.h>

/* check we have an audio(4) configured into kernel */
#if defined(_KERNEL_OPT)
#include "audio.h"

#if (NAUDIO == 0) && (NMIDI == 0) && (NMIDIBUS == 0)
#error "No 'audio* at audiobus?' or 'midi* at midibus?' or similar configured"
#endif

#endif /* _KERNEL_OPT */

/*
 * Interfaces for hardware drivers and MI audio.
 */

struct audio_softc;

/**
 * audio stream format
 */
typedef struct audio_params {
	u_int	sample_rate;	/* sample rate */
	u_int	encoding;	/* e.g. mu-law, linear, etc */
	u_int	precision;	/* bits/subframe */
	u_int	validbits;	/* valid bits in a subframe */
	u_int	channels;	/* mono(1), stereo(2) */
} audio_params_t;

/* The default audio mode: 8 kHz mono mu-law */
extern const struct audio_params audio_default;

/**
 * audio stream buffer
 */
typedef struct audio_stream {
	size_t bufsize;		/* allocated memory */
	uint8_t *start;		/* start of buffer area */
	uint8_t *end;		/* end of valid buffer area */
	uint8_t *inp;		/* address to be written next */
	const uint8_t *outp;	/* address to be read next */
	int used;		/* valid data size in this stream */
	audio_params_t param;	/* represents this stream */
	bool loop;
} audio_stream_t;

static __inline int
audio_stream_get_space(const audio_stream_t *s)
{
	if (s)
		return (s->end - s->start) - s->used;
	return 0;
}

static __inline int
audio_stream_get_used(const audio_stream_t *s)
{
	return s ? s->used : 0;
}

static __inline uint8_t *
audio_stream_add_inp(audio_stream_t *s, uint8_t *v, int diff)
{
	s->used += diff;
	v += diff;
	if (v >= s->end)
		v -= s->end - s->start;
	return v;
}

static __inline const uint8_t *
audio_stream_add_outp(audio_stream_t *s, const uint8_t *v, int diff)
{
	s->used -= diff;
	v += diff;
	if (v >= s->end)
		v -= s->end - s->start;
	return v;
}

/**
 * an interface to fill a audio stream buffer
 */
typedef struct stream_fetcher {
	int (*fetch_to)(struct audio_softc *, struct stream_fetcher *,
            audio_stream_t *, int);
} stream_fetcher_t;

/**
 * audio stream filter.
 * This must be an extension of stream_fetcher_t.
 */
typedef struct stream_filter {
/* public: */
	stream_fetcher_t base;
	void (*dtor)(struct stream_filter *);
	void (*set_fetcher)(struct stream_filter *, stream_fetcher_t *);
	void (*set_inputbuffer)(struct stream_filter *, audio_stream_t *);
/* private: */
	stream_fetcher_t *prev;
	audio_stream_t *src;
} stream_filter_t;

/**
 * factory method for stream_filter_t
 */
typedef stream_filter_t *stream_filter_factory_t(struct audio_softc *,
	const audio_params_t *, const audio_params_t *);

/**
 * filter pipeline request
 *
 * filters[0] is the first filter for playing or the last filter for recording.
 * The audio_params_t instance for the hardware is filters[0].param.
 */
#ifndef AUDIO_MAX_FILTERS
# define AUDIO_MAX_FILTERS	8
#endif
typedef struct stream_filter_list {
	void (*append)(struct stream_filter_list *, stream_filter_factory_t,
		       const audio_params_t *);
	void (*prepend)(struct stream_filter_list *, stream_filter_factory_t,
			const audio_params_t *);
	void (*set)(struct stream_filter_list *, int, stream_filter_factory_t,
		    const audio_params_t *);
	int req_size;
	struct stream_filter_req {
		stream_filter_factory_t *factory;
		audio_params_t param; /* from-param for recording,
					 to-param for playing */
	} filters[AUDIO_MAX_FILTERS];
} stream_filter_list_t;

struct audio_hw_if {
	int	(*open)(void *, int);	/* open hardware */
	void	(*close)(void *);	/* close hardware */
	int	(*drain)(void *);	/* Optional: drain buffers */

	/* Encoding. */
	/* XXX should we have separate in/out? */
	int	(*query_encoding)(void *, audio_encoding_t *);

	/* Set the audio encoding parameters (record and play).
	 * Return 0 on success, or an error code if the
	 * requested parameters are impossible.
	 * The values in the params struct may be changed (e.g. rounding
	 * to the nearest sample rate.)
	 */
	int	(*set_params)(void *, int, int, audio_params_t *,
		    audio_params_t *, stream_filter_list_t *,
		    stream_filter_list_t *);

	/* Hardware may have some say in the blocksize to choose */
	int	(*round_blocksize)(void *, int, int, const audio_params_t *);

	/*
	 * Changing settings may require taking device out of "data mode",
	 * which can be quite expensive.  Also, audiosetinfo() may
	 * change several settings in quick succession.  To avoid
	 * having to take the device in/out of "data mode", we provide
	 * this function which indicates completion of settings
	 * adjustment.
	 */
	int	(*commit_settings)(void *);

	/* Start input/output routines. These usually control DMA. */
	int	(*init_output)(void *, void *, int);
	int	(*init_input)(void *, void *, int);
	int	(*start_output)(void *, void *, int,
				    void (*)(void *), void *);
	int	(*start_input)(void *, void *, int,
				   void (*)(void *), void *);
	int	(*halt_output)(void *);
	int	(*halt_input)(void *);

	int	(*speaker_ctl)(void *, int);
#define SPKR_ON		1
#define SPKR_OFF	0

	int	(*getdev)(void *, struct audio_device *);
	int	(*setfd)(void *, int);

	/* Mixer (in/out ports) */
	int	(*set_port)(void *, mixer_ctrl_t *);
	int	(*get_port)(void *, mixer_ctrl_t *);

	int	(*query_devinfo)(void *, mixer_devinfo_t *);

	/* Allocate/free memory for the ring buffer. Usually malloc/free. */
	void	*(*allocm)(void *, int, size_t);
	void	(*freem)(void *, void *, size_t);
	size_t	(*round_buffersize)(void *, int, size_t);
	paddr_t	(*mappage)(void *, void *, off_t, int);

	int	(*get_props)(void *); /* device properties */

	int	(*trigger_output)(void *, void *, void *, int,
		    void (*)(void *), void *, const audio_params_t *);
	int	(*trigger_input)(void *, void *, void *, int,
		    void (*)(void *), void *, const audio_params_t *);
	int	(*dev_ioctl)(void *, u_long, void *, int, struct lwp *);
	void	(*get_locks)(void *, kmutex_t **, kmutex_t **);
};

struct audio_attach_args {
	int	type;
	const void *hwif;	/* either audio_hw_if * or midi_hw_if * */
	void	*hdl;
};
#define	AUDIODEV_TYPE_AUDIO	0
#define	AUDIODEV_TYPE_MIDI	1
#define AUDIODEV_TYPE_OPL	2
#define AUDIODEV_TYPE_MPU	3
#define AUDIODEV_TYPE_AUX	4

/* Attach the MI driver(s) to the MD driver. */
device_t audio_attach_mi(const struct audio_hw_if *, void *, device_t);
int	audioprint(void *, const char *);

/* Get the hw device from an audio softc */
device_t audio_get_device(struct audio_softc *);

/* Device identity flags */
#define SOUND_DEVICE		0
#define AUDIO_DEVICE		0x80
#define AUDIOCTL_DEVICE		0xc0
#define MIXER_DEVICE		0x10

#define AUDIOUNIT(x)		(minor(x)&0x0f)
#define AUDIODEV(x)		(minor(x)&0xf0)

#define ISDEVSOUND(x)		(AUDIODEV((x)) == SOUND_DEVICE)
#define ISDEVAUDIO(x)		(AUDIODEV((x)) == AUDIO_DEVICE)
#define ISDEVAUDIOCTL(x)	(AUDIODEV((x)) == AUDIOCTL_DEVICE)
#define ISDEVMIXER(x)		(AUDIODEV((x)) == MIXER_DEVICE)

/*
 * USB Audio specification defines 12 channels:
 *	L R C LFE Ls Rs Lc Rc S Sl Sr T
 */
#define AUDIO_MAX_CHANNELS	12

#endif /* _SYS_DEV_AUDIO_IF_H_ */

