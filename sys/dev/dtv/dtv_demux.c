/* $NetBSD: dtv_demux.c,v 1.6 2014/08/09 13:34:10 jmcneill Exp $ */

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

/*
 * This file contains support for the /dev/dvb/adapter<n>/demux0 device.
 *
 * The demux device is implemented as a cloning device. Each instance can
 * be in one of three modes: unconfigured (NONE), section filter (SECTION),
 * or PID filter (PES).
 *
 * An instance in section filter mode extracts PSI sections based on a
 * filter configured by the DMX_SET_FILTER ioctl. When an entire section is
 * received, it is made available to userspace via read method. Data is fed
 * into the section filter using the dtv_demux_write function.
 *
 * An instance in PID filter mode extracts TS packets that match the
 * specified PID filter configured by the DMX_SET_PES_FILTER, DMX_ADD_PID,
 * and DMX_REMOVE_PID ioctls. As this driver only implements the
 * DMX_OUT_TS_TAP output, these TS packets are made available to userspace
 * by calling read on the /dev/dvb/adapter<n>/dvr0 device.
 */ 

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dtv_demux.c,v 1.6 2014/08/09 13:34:10 jmcneill Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/select.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/poll.h>
#include <sys/vnode.h>
#include <sys/queue.h>

#include <dev/dtv/dtvvar.h>

static int	dtv_demux_read(struct file *, off_t *, struct uio *,
		    kauth_cred_t, int);
static int	dtv_demux_ioctl(struct file *, u_long, void *);
static int	dtv_demux_poll(struct file *, int);
static int	dtv_demux_close(struct file *);

static const struct fileops dtv_demux_fileops = {
	.fo_read = dtv_demux_read,
	.fo_write = fbadop_write,
	.fo_ioctl = dtv_demux_ioctl,
	.fo_fcntl = fnullop_fcntl,
	.fo_poll = dtv_demux_poll,
	.fo_stat = fbadop_stat,
	.fo_close = dtv_demux_close,
	.fo_kqfilter = fnullop_kqfilter,
	.fo_restart = fnullop_restart,
};

static uint32_t crc_table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
	0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
	0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
	0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
	0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
	0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
	0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
	0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
	0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
	0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
	0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
	0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
	0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
	0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
	0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
	0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
	0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
	0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
	0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
	0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
	0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
	0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
	0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
	0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
	0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
	0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
	0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
	0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
	0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
	0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
	0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
	0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
	0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
	0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
	0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
	0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
	0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
	0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
	0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
	0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
	0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
	0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
	0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

/* ISO/IEC 13818-1 Annex A "CRC Decoder Model" */
static uint32_t
dtv_demux_crc32(uint8_t *buf, int len)
{
	const uint32_t *crc_tab = crc_table;
	uint32_t CRC = 0xffffffff;
	int i;

	for (i = 0; i < len; i++)
	      	CRC = (CRC << 8) ^ crc_tab[((CRC >> 24) ^ *buf++) & 0xff];

	return CRC;
}

/*
 * Start running the demux.
 */
static int
dtv_demux_start(struct dtv_demux *demux)
{
	struct dtv_softc *sc = demux->dd_sc;
	int error = 0;
	bool dostart = false;

	/*
	 * If the demux is not running, mark it as running and update the
	 * global demux run counter.
	 */
	mutex_enter(&sc->sc_lock);
	KASSERT(sc->sc_demux_runcnt >= 0);
	if (demux->dd_running == false) {
		sc->sc_demux_runcnt++;
		demux->dd_running = true;
		/* If this is the first demux running, trigger device start */
		dostart = sc->sc_demux_runcnt == 1;
	}
	mutex_exit(&sc->sc_lock);

	if (dostart) {
		/* Setup receive buffers and trigger device start */
		error = dtv_buffer_setup(sc);
		if (error == 0)
			error = dtv_device_start_transfer(sc);
	}

	/*
	 * If something went wrong, restore the run counter and mark this
	 * demux instance as halted.
	 */
	if (error) {
		mutex_enter(&sc->sc_lock);
		sc->sc_demux_runcnt--;
		demux->dd_running = false;
		mutex_exit(&sc->sc_lock);
	}

	return error;
}

/*
 * Stop running the demux.
 */
static int
dtv_demux_stop(struct dtv_demux *demux)
{
	struct dtv_softc *sc = demux->dd_sc;
	int error = 0;
	bool dostop = false;

	/*
	 * If the demux is running, mark it as halted and update the
	 * global demux run counter.
	 */
	mutex_enter(&sc->sc_lock);
	if (demux->dd_running == true) {
		KASSERT(sc->sc_demux_runcnt > 0);
		demux->dd_running = false;
		sc->sc_demux_runcnt--;
		/* If this was the last demux running, trigger device stop */
		dostop = sc->sc_demux_runcnt == 0;
	}
	mutex_exit(&sc->sc_lock);

	if (dostop) {
		/* Trigger device stop */
		error = dtv_device_stop_transfer(sc);
	}

	/*
	 * If something went wrong, restore the run counter and mark this
	 * demux instance as running.
	 */
	if (error) {
		mutex_enter(&sc->sc_lock);
		sc->sc_demux_runcnt++;
		demux->dd_running = true;
		mutex_exit(&sc->sc_lock);
	}

	return error;
}

/*
 * Put the demux into PID filter mode and update the PID filter table.
 */
static int
dtv_demux_set_pidfilter(struct dtv_demux *demux, uint16_t pid, bool onoff)
{
	struct dtv_softc *sc = demux->dd_sc;

	/*
	 * TS PID is 13 bits; demux device uses special PID 0x2000 to mean
	 * "all PIDs". Verify that the requested PID is in range.
	 */
	if (pid > 0x2000)
		return EINVAL;

	/* Set demux mode */
	demux->dd_mode = DTV_DEMUX_MODE_PES;
	/*
	 * If requesting "all PIDs", set the on/off flag for all PIDs in
	 * the PID map, otherwise set the on/off flag for the requested
	 * PID.
	 */
	if (pid == 0x2000) {
		memset(sc->sc_ts.ts_pidfilter, onoff,
		    sizeof(sc->sc_ts.ts_pidfilter));
	} else {
		sc->sc_ts.ts_pidfilter[pid] = onoff;
	}

	return 0;
}

/*
 * Open a new instance of the demux cloning device.
 */
int
dtv_demux_open(struct dtv_softc *sc, int flags, int mode, lwp_t *l)
{
	struct file *fp;
	struct dtv_demux *demux;
	int error, fd;

	/* Allocate private storage */
	demux = kmem_zalloc(sizeof(*demux), KM_SLEEP);
	if (demux == NULL)
		return ENOMEM;
	demux->dd_sc = sc;
	/* Default operation mode is unconfigured */
	demux->dd_mode = DTV_DEMUX_MODE_NONE;
	selinit(&demux->dd_sel);
	mutex_init(&demux->dd_lock, MUTEX_DEFAULT, IPL_SCHED);
	cv_init(&demux->dd_section_cv, "dtvsec");

	error = fd_allocfile(&fp, &fd);
	if (error) {
		kmem_free(demux, sizeof(*demux));
		return error;
	}

	/* Add the demux to the list of demux instances */
	mutex_enter(&sc->sc_demux_lock);
	TAILQ_INSERT_TAIL(&sc->sc_demux_list, demux, dd_entries);
	mutex_exit(&sc->sc_demux_lock);

	return fd_clone(fp, fd, flags, &dtv_demux_fileops, demux);
}

/*
 * Close the instance of the demux cloning device.
 */
int
dtv_demux_close(struct file *fp)
{
	struct dtv_demux *demux = fp->f_data;
	struct dtv_softc *sc;
	int error;

	if (demux == NULL)
		return ENXIO;

	fp->f_data = NULL;

	sc = demux->dd_sc;

	/* If the demux is still running, stop it */
	if (demux->dd_running) {
		error = dtv_demux_stop(demux);
		if (error)
			return error;
	}

	/* Remove the demux from the list of demux instances */
	mutex_enter(&sc->sc_demux_lock);
	TAILQ_REMOVE(&sc->sc_demux_list, demux, dd_entries);
	mutex_exit(&sc->sc_demux_lock);

	mutex_destroy(&demux->dd_lock);
	cv_destroy(&demux->dd_section_cv);
	kmem_free(demux, sizeof(*demux));

	/* Update the global device open count */
	dtv_common_close(sc);

	return 0;
}

/*
 * Handle demux ioctl requests
 */
static int
dtv_demux_ioctl(struct file *fp, u_long cmd, void *data)
{
	struct dtv_demux *demux = fp->f_data;
	struct dmx_pes_filter_params *pesfilt;
	struct dmx_sct_filter_params *sctfilt;
	uint16_t pid;
	int error;

	if (demux == NULL)
		return ENXIO;

	switch (cmd) {
	case DMX_START:
		return dtv_demux_start(demux);
	case DMX_STOP:
		return dtv_demux_stop(demux);
	case DMX_SET_BUFFER_SIZE:
		/*
		 * The demux driver doesn't support configurable buffer sizes,
		 * but software relies on this command succeeding.
		 */
		return 0;
	case DMX_SET_FILTER:
		sctfilt = data;

		/* Verify that the requested PID is in range. */
		if (sctfilt->pid >= 0x2000)
			return EINVAL;

		/*
		 * Update section filter parameters, reset read/write ptrs,
		 * clear section count and overflow flag, and set the
		 * demux instance mode to section filter.
		 */
		demux->dd_secfilt.params = *sctfilt;
		demux->dd_secfilt.rp = demux->dd_secfilt.wp = 0;
		demux->dd_secfilt.nsections = 0;
		demux->dd_secfilt.overflow = false;
		demux->dd_mode = DTV_DEMUX_MODE_SECTION;

		/*
		 * If the DMX_IMMEDIATE_START flag is present in the request,
		 * start running the demux immediately (no need for a
		 * subsequent DMX_START ioctl).
		 */
		if (sctfilt->flags & DMX_IMMEDIATE_START) {
			error = dtv_demux_start(demux);
			if (error)
				return error;
		}

		return 0;
	case DMX_SET_PES_FILTER:
		pesfilt = data;

		/* The driver only supports input from the frontend */
		if (pesfilt->input != DMX_IN_FRONTEND)
			return EINVAL;
		/*
		 * The driver only supports output to the TS TAP in PID
		 * filter mode.
		 */
		if (pesfilt->output != DMX_OUT_TS_TAP)
			return EINVAL;

		/* Update PID filter table */
		error = dtv_demux_set_pidfilter(demux, pesfilt->pid, true);
		if (error)
			return error;

		/*
		 * If the DMX_IMMEDIATE_START flag is present in the request,
		 * start running the demux immediately (no need for a
		 * subsequent DMX_START ioctl).
		 */
		if (pesfilt->flags & DMX_IMMEDIATE_START) {
			error = dtv_demux_start(demux);
			if (error)
				return error;
		}
		return 0;
	case DMX_ADD_PID:
		pid = *(uint16_t *)data;
		return dtv_demux_set_pidfilter(demux, pid, true);
	case DMX_REMOVE_PID:
		pid = *(uint16_t *)data;
		return dtv_demux_set_pidfilter(demux, pid, false);
	default:
		return EINVAL;
	}
}

/*
 * Test for I/O readiness
 */
static int
dtv_demux_poll(struct file *fp, int events)
{
	struct dtv_demux *demux = fp->f_data;
	int revents = 0;

	if (demux == NULL)
		return POLLERR;

	/*
	 * If the demux instance is in section filter mode, wait for an
	 * entire section to become ready.
	 */
	mutex_enter(&demux->dd_lock);
	if (demux->dd_mode == DTV_DEMUX_MODE_SECTION &&
	    demux->dd_secfilt.nsections > 0) {
		revents |= POLLIN;
	} else {
		selrecord(curlwp, &demux->dd_sel);
	}
	mutex_exit(&demux->dd_lock);

	return revents;
}

/*
 * Read from the demux instance
 */
static int
dtv_demux_read(struct file *fp, off_t *offp, struct uio *uio,
    kauth_cred_t cred, int flags)
{
	struct dtv_demux *demux = fp->f_data;
	struct dtv_ts_section sec;
	int error;

	if (demux == NULL)
		return ENXIO;

	/* Only support read if the instance is in section filter mode */
	if (demux->dd_mode != DTV_DEMUX_MODE_SECTION)
		return EIO;

	/* Wait for a complete PSI section */
	mutex_enter(&demux->dd_lock);
	while (demux->dd_secfilt.nsections == 0) {
		if (flags & IO_NDELAY) {
			mutex_exit(&demux->dd_lock);
			/* No data available */
			return EWOULDBLOCK;
		}
		error = cv_wait_sig(&demux->dd_section_cv, &demux->dd_lock);
		if (error) {
			mutex_exit(&demux->dd_lock);
			return error;
		}
	}
	/* Copy the completed PSI section */
	sec = demux->dd_secfilt.section[demux->dd_secfilt.rp];
	/* Update read pointer */
	demux->dd_secfilt.rp++;
	if (demux->dd_secfilt.rp >= __arraycount(demux->dd_secfilt.section))
		demux->dd_secfilt.rp = 0;
	/* Update section count */
	demux->dd_secfilt.nsections--;
	mutex_exit(&demux->dd_lock);

	/*
	 * If the filter parameters specify the DMX_ONESHOT flag, stop
	 * the demux after one PSI section is received.
	 */
	if (demux->dd_secfilt.params.flags & DMX_ONESHOT)
		dtv_demux_stop(demux);

	/*
	 * Copy the PSI section to userspace. If the receiving buffer is
	 * too small, the rest of the payload will be discarded. Although
	 * this behaviour differs from the Linux implementation, in practice
	 * it should not be an issue as PSI sections have a max size of 4KB
	 * (and callers will generally provide a big enough buffer).
	 */
	return uiomove(sec.sec_buf, sec.sec_length, uio);
}

/*
 * Verify the CRC of a PSI section.
 */
static bool
dtv_demux_check_crc(struct dtv_demux *demux, struct dtv_ts_section *sec)
{
	uint32_t crc, sec_crc;

	/*
	 * If section_syntax_indicator is not set, the PSI section does
	 * not include a CRC field.
	 */
	if ((sec->sec_buf[1] & 0x80) == 0)
		return false;

	sec_crc = be32dec(&sec->sec_buf[sec->sec_length - 4]);
	crc = dtv_demux_crc32(&sec->sec_buf[0], sec->sec_length - 4);

	return crc == sec_crc;
}

/*
 * Process a single TS packet and extract PSI sections based on the
 * instance's section filter.
 */
static int
dtv_demux_process(struct dtv_demux *demux, const uint8_t *tspkt,
    size_t tspktlen)
{
	struct dtv_ts_section *sec;
	dmx_filter_t *dmxfilt = &demux->dd_secfilt.params.filter;
	const uint8_t *p;
	uint16_t section_length;
	int brem, avail;

	KASSERT(tspktlen == TS_PKTLEN);

	/* If the demux instance is not running, ignore the packet */
	if (demux->dd_running == false)
		return 0;

	/*
	 * If the demux instance is not in section filter mode, ignore
	 * the packet
	 */
	if (demux->dd_mode != DTV_DEMUX_MODE_SECTION)
		return 0;
	/*
	 * If the packet's TS PID does not match the section filter PID,
	 * ignore the packet
	 */
	if (TS_PID(tspkt) != demux->dd_secfilt.params.pid)
		return 0;
	/*
	 * If the TS packet does not contain a payload, ignore the packet
	 */
	if (TS_HAS_PAYLOAD(tspkt) == 0)
		return 0;

	mutex_enter(&demux->dd_lock);

	/* If the section buffer is full, set the overflow flag and return */
	if (demux->dd_secfilt.nsections ==
	    __arraycount(demux->dd_secfilt.section)) {
		demux->dd_secfilt.overflow = true;
		goto done;
	}
	sec = &demux->dd_secfilt.section[demux->dd_secfilt.wp];
	/* If we have no bytes in our buffer, wait for payload unit start */
	if (sec->sec_bytesused == 0 && TS_HAS_PUSI(tspkt) == 0)
		goto done;

	/* find payload start */
	p = tspkt + 4;
	if (TS_HAS_AF(tspkt)) {
		if (*p > 182)	/* AF length with payload is between 0-182 */
			goto done;
		p += (1 + *p);
	}
	if (TS_HAS_PUSI(tspkt)) {
		p += (1 + *p);
	}

	brem = tspktlen - (p - tspkt); 

	if (TS_HAS_PUSI(tspkt)) {
		if (brem < 16)
			goto done;

		section_length = ((p[1] & 0xf) << 8) | p[2];

		/* table_id filter */
		if (dmxfilt->mask[0]) {
			if ((p[0] & dmxfilt->mask[0]) != dmxfilt->filter[0])
				goto done;
		}
		/* table_id_ext filter */
		if (dmxfilt->mask[1] && dmxfilt->mask[2]) {
			/*
			 * table_id_ext is only valid if
			 * section_syntax_indicator is set
			 */
			if (section_length < 2 || (p[1] & 0x80) == 0)
				goto done;
			if ((p[3] & dmxfilt->mask[1]) != dmxfilt->filter[1])
				goto done;
			if ((p[4] & dmxfilt->mask[2]) != dmxfilt->filter[2])
				goto done;
		}

		sec->sec_length = section_length + 3;

		/* maximum section length is 4KB */
		if (sec->sec_length > sizeof(sec->sec_buf)) {
			sec->sec_bytesused = sec->sec_length = 0;
			goto done;
		}

	}

	/* If we have bytes pending and we see payload unit start, flush buf */
	if (sec->sec_bytesused > 0 && TS_HAS_PUSI(tspkt))
		sec->sec_bytesused = sec->sec_length = 0;

	/* Copy data into section buffer */
	avail = min(sec->sec_length - sec->sec_bytesused, brem);
	if (avail < 0)
		goto done;
	memcpy(&sec->sec_buf[sec->sec_bytesused], p, avail);
	sec->sec_bytesused += avail;

	/*
	 * If a complete section has been received, update section count
	 * and notify readers.
	 */
	if (sec->sec_bytesused == sec->sec_length) {
		/*
		 * If the DMX_CHECK_CRC flag was present in the DMX_SET_FILTER
		 * parameters, verify the PSI section checksum. If the
		 * checksum is invalid, discard the entire corrupt section.
		 */
		if ((demux->dd_secfilt.params.flags & DMX_CHECK_CRC) &&
		    dtv_demux_check_crc(demux, sec) == false) {
			/* discard section */
			sec->sec_bytesused = sec->sec_length = 0;
			goto done;
		}

		demux->dd_secfilt.wp++;
		if (demux->dd_secfilt.wp >=
		    __arraycount(demux->dd_secfilt.section))
			demux->dd_secfilt.wp = 0;
		demux->dd_secfilt.nsections++;
		cv_broadcast(&demux->dd_section_cv);
		selnotify(&demux->dd_sel, 0, 0);
	}

done:
	mutex_exit(&demux->dd_lock);
	return 0;
}

/*
 * Submit TS data to all demux instances
 */
void
dtv_demux_write(struct dtv_softc *sc, const uint8_t *tspkt, size_t tspktlen)
{
	struct dtv_demux *demux;

	mutex_enter(&sc->sc_demux_lock);
	TAILQ_FOREACH(demux, &sc->sc_demux_list, dd_entries) {
		dtv_demux_process(demux, tspkt, tspktlen);
	}
	mutex_exit(&sc->sc_demux_lock);
}
