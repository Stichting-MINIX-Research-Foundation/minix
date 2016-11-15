/* $NetBSD: dtvvar.h,v 1.6 2011/08/09 01:42:24 jmcneill Exp $ */

/*-
 * Copyright (c) 2011 Jared D. McNeill <jmcneill@invisible.ca>
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
 *        This product includes software developed by Jared D. McNeill.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#ifndef _DEV_DTV_DTVVAR_H
#define _DEV_DTV_DTVVAR_H

#include <dev/dtv/dtvif.h>
#include <dev/dtv/dtv_scatter.h>

#define	DTV_DEFAULT_BLOCKSIZE	(32 * PAGE_SIZE)
#define	DTV_DEFAULT_BUFSIZE	(32 * DTV_DEFAULT_BLOCKSIZE)

#define	TS_PKTLEN		188
#define	TS_HAS_SYNC(_tspkt)	((_tspkt)[0] == 0x47)
#define	TS_HAS_PUSI(_tspkt)	((_tspkt)[1] & 0x40)
#define	TS_HAS_AF(_tspkt)	((_tspkt)[3] & 0x20)
#define	TS_HAS_PAYLOAD(_tspkt)	((_tspkt)[3] & 0x10)
#define	TS_PID(_tspkt)		((((_tspkt)[1] & 0x1f) << 8) | (_tspkt)[2])
#define	TS_SECTION_MAXLEN	4096

struct dtv_buffer {
	uint32_t	db_offset;
	uint32_t	db_bytesused;
	size_t		db_length;
	SIMPLEQ_ENTRY(dtv_buffer) db_entries;
};

SIMPLEQ_HEAD(dtv_sample_queue, dtv_buffer);

struct dtv_stream {
	unsigned int		ds_nbufs;
	struct dtv_buffer	**ds_buf;
	struct dtv_scatter_buf	ds_data;
	struct dtv_sample_queue	ds_ingress, ds_egress;
	kmutex_t		ds_ingress_lock, ds_egress_lock;
	kcondvar_t		ds_sample_cv;
	struct selinfo		ds_sel;
	uint32_t		ds_bytesread;
};

typedef enum {
	DTV_DEMUX_MODE_NONE,
	DTV_DEMUX_MODE_SECTION,
	DTV_DEMUX_MODE_PES,
} dtv_demux_mode_t;

struct dtv_ts_section {
	uint8_t			sec_buf[TS_SECTION_MAXLEN];
	uint16_t		sec_bytesused;
	uint16_t		sec_length;
};

struct dtv_demux {
	struct dtv_softc	*dd_sc;
	struct selinfo		dd_sel;
	kmutex_t		dd_lock;
	kcondvar_t		dd_section_cv;

	bool			dd_running;

	dtv_demux_mode_t	dd_mode;
	struct {
		struct dmx_sct_filter_params	params;
		struct dtv_ts_section		section[16];
		unsigned int			rp, wp;
		unsigned int			nsections;
		bool				overflow;
	} dd_secfilt;

	TAILQ_ENTRY(dtv_demux)	dd_entries;
};

struct dtv_ts {
	uint8_t			ts_pidfilter[0x2000];
	kmutex_t		ts_lock;
};

struct dtv_softc {
	device_t	sc_dev;
	const struct dtv_hw_if *sc_hw;
	void		*sc_priv;

	bool		sc_dying;

	unsigned int	sc_open;
	kmutex_t	sc_lock;

	size_t		sc_bufsize;
	bool		sc_bufsize_chg;

	struct dtv_stream sc_stream;
	struct dtv_ts	sc_ts;

	TAILQ_HEAD(, dtv_demux) sc_demux_list;
	kmutex_t	sc_demux_lock;
	int		sc_demux_runcnt;
};

#define	dtv_device_get_devinfo(sc, info)	\
	((sc)->sc_hw->get_devinfo((sc)->sc_priv, (info)))
#define	dtv_device_open(sc, flags)		\
	((sc)->sc_hw->open((sc)->sc_priv, (flags)))
#define	dtv_device_close(sc)			\
	((sc)->sc_hw->close((sc)->sc_priv))
#define	dtv_device_set_tuner(sc, params)	\
	((sc)->sc_hw->set_tuner((sc)->sc_priv, (params)))
#define	dtv_device_get_status(sc)		\
	((sc)->sc_hw->get_status((sc)->sc_priv))
#define	dtv_device_get_signal_strength(sc)	\
	((sc)->sc_hw->get_signal_strength((sc)->sc_priv))
#define	dtv_device_get_snr(sc)			\
	((sc)->sc_hw->get_snr((sc)->sc_priv))
#define	dtv_device_start_transfer(sc)	\
	((sc)->sc_hw->start_transfer((sc)->sc_priv, dtv_buffer_submit, (sc)))
#define	dtv_device_stop_transfer(sc)		\
	((sc)->sc_hw->stop_transfer((sc)->sc_priv))

int	dtv_frontend_ioctl(struct dtv_softc *, u_long, void *, int);

int	dtv_demux_open(struct dtv_softc *, int, int, lwp_t *);
void	dtv_demux_write(struct dtv_softc *, const uint8_t *, size_t);

int	dtv_buffer_realloc(struct dtv_softc *, size_t);
int	dtv_buffer_setup(struct dtv_softc *);
int	dtv_buffer_destroy(struct dtv_softc *);
int	dtv_buffer_read(struct dtv_softc *, struct uio *, int);
int	dtv_buffer_poll(struct dtv_softc *, int, lwp_t *);
void	dtv_buffer_submit(void *, const struct dtv_payload *);

void	dtv_common_close(struct dtv_softc *);

#endif /* !_DEV_DTV_DTVVAR_H */
