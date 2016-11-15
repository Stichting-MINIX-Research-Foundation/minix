/*	$NetBSD: isv.c,v 1.7 2014/07/25 08:10:37 dholland Exp $ */

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by David Young.
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
__KERNEL_RCSID(0, "$NetBSD: isv.c,v 1.7 2014/07/25 08:10:37 dholland Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>

#include <uvm/uvm_extern.h>

#include <sys/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/isa/isvio.h>

#define	ISV_CONTROL	0x0		/* control: write-only */
#define	ISV_CONTROL_MODE_MASK		__BIT(0)
#define	ISV_CONTROL_MODE_CAPTURE	__SHIFTIN(0, ISV_CONTROL_MODE_MASK)
#define	ISV_CONTROL_MODE_READ		__SHIFTIN(1, ISV_CONTROL_MODE_MASK)
#define	ISV_CONTROL_COUNTER_MASK	__BIT(1)
#define	ISV_CONTROL_COUNTER_RESET	__SHIFTIN(1, ISV_CONTROL_COUNTER_MASK)
#define	ISV_CONTROL_COUNTER_AUTOINC	__SHIFTIN(0, ISV_CONTROL_COUNTER_MASK)

#define	ISV_DATA	ISV_CONTROL	/* data: read-only */

#define ISV_STATUS	0x2		/* status: read-only */
#define ISV_STATUS_VIDEO_MASK		__BIT(15)
#define ISV_STATUS_VIDEO_RETRACE	__SHIFTIN(0, ISV_STATUS_VIDEO_MASK)
#define ISV_STATUS_VIDEO_WRITE		__SHIFTIN(1, ISV_STATUS_VIDEO_MASK)

struct isv_regs {
	bus_space_tag_t		ir_bt;
	bus_space_handle_t	ir_bh;
};

enum isv_state {
	  ISV_S_CAPTURE0 = 0
	, ISV_S_CAPTURE1 = 1
	, ISV_S_CAPTURE2 = 2
	, ISV_S_RETRACE = 3
};

struct isv_softc {
	struct isv_regs	sc_ir;
	device_t	sc_dev;
	uint16_t	*sc_frame;
	int		sc_speed;
};

extern struct cfdriver isv_cd;

static dev_type_ioctl(isv_ioctl);
static dev_type_open(isv_open);
static dev_type_mmap(isv_mmap);

static int	isv_capture(struct isv_softc *);
static int 	isv_match(device_t, cfdata_t, void *);
static void 	isv_attach(device_t, device_t, void *);
static int 	isv_detach(device_t, int);
static uint16_t isv_read(struct isv_regs *, bus_size_t);
static void	isv_write(struct isv_regs *, bus_size_t, uint16_t);
static bool	isv_retrace(struct isv_regs *);
static int	isv_retrace_wait(struct isv_regs *, int *,
    const struct timeval *);
static int	isv_capture_wait(struct isv_regs *, int *,
    const struct timeval *);
static bool	isv_delta(int *, bool);
static int	isv_probe(struct isv_regs *);

CFATTACH_DECL_NEW(isv_isa, sizeof(struct isv_softc),
    isv_match, isv_attach, isv_detach, NULL);

const struct cdevsw isv_cdevsw = {
	.d_open = isv_open,
	.d_close = nullclose,
	.d_read = noread,
	.d_write = nowrite,
	.d_ioctl = isv_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = isv_mmap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER
};

static uint16_t
isv_read(struct isv_regs *ir, bus_size_t reg)
{
	return bus_space_read_2(ir->ir_bt, ir->ir_bh, reg);
}

static void
isv_write(struct isv_regs *ir, bus_size_t reg, uint16_t val)
{
	bus_space_write_2(ir->ir_bt, ir->ir_bh, reg, val);
}

static bool
isv_retrace(struct isv_regs *ir)
{
	uint16_t video;

	video = isv_read(ir, ISV_STATUS) & ISV_STATUS_VIDEO_MASK;
	return video == ISV_STATUS_VIDEO_RETRACE;
}

#define state_and_input(__state, __retrace)	\
	(((__state) << 1) | ((__retrace) ? 1 : 0))

static bool
isv_delta(int *state, bool retrace)
{
	bool transition = false;

	switch (state_and_input(*state, retrace)) {
	case state_and_input(ISV_S_CAPTURE0, false):
	case state_and_input(ISV_S_RETRACE, true):
		break;
	case state_and_input(ISV_S_CAPTURE2, true):
		transition = true;
		/*FALLTHROUGH*/
	case state_and_input(ISV_S_CAPTURE1, true):
	case state_and_input(ISV_S_CAPTURE0, true):
		(*state)++;
		break;
	case state_and_input(ISV_S_RETRACE, false):
		transition = true;
		/*FALLTHROUGH*/
	case state_and_input(ISV_S_CAPTURE2, false):
	case state_and_input(ISV_S_CAPTURE1, false):
		*state = ISV_S_CAPTURE0;
		break;
	}
	return transition;
}

static int
isv_probe(struct isv_regs *ir)
{
	int state, transitions;
	struct timeval end, now,
	    wait = {.tv_sec = 0, .tv_usec = 1000000 * 4 / 30};

	aprint_debug("%s: resetting\n", __func__);
	isv_write(ir, ISV_CONTROL,
	    ISV_CONTROL_MODE_CAPTURE|ISV_CONTROL_COUNTER_AUTOINC);

	aprint_debug("%s: waiting\n", __func__);

	microtime(&now);
	timeradd(&now, &wait, &end);

	state = transitions = 0;

	do {
		if (isv_delta(&state, isv_retrace(ir)))
			transitions++;

		if (state == ISV_S_CAPTURE0 || state == ISV_S_RETRACE)
			microtime(&now);
	} while (timercmp(&now, &end, <));

	aprint_debug("%s: %d transitions\n", __func__, transitions);

	return transitions >= 4 && transitions <= 10;
}

static int
isv_match(device_t parent, cfdata_t match, void *aux)
{
	struct isv_regs ir;
	struct isa_attach_args *ia = aux;
	int rv;

	/* Must supply an address */
	if (ia->ia_nio < 1 || ia->ia_io[0].ir_addr == ISA_UNKNOWN_PORT)
		return 0;

	ir.ir_bt = ia->ia_iot;

	if (bus_space_map(ir.ir_bt, ia->ia_io[0].ir_addr, 8, 0, &ir.ir_bh))
		return 0;

	rv = isv_probe(&ir);

	bus_space_unmap(ir.ir_bt, ir.ir_bh, 8);

	if (rv) {
		ia->ia_nio = 1;
		ia->ia_io[0].ir_size = 8;

		ia->ia_niomem = 0;
		ia->ia_nirq = 0;
		ia->ia_ndrq = 0;
	}

	return rv;
}


static void
isv_attach(device_t parent, device_t self, void *aux)
{
	struct isv_softc *sc = device_private(self);
	struct isv_regs *ir = &sc->sc_ir;
	struct isa_attach_args *ia = aux;

	ir->ir_bt = ia->ia_iot;

	if (bus_space_map(ir->ir_bt, ia->ia_io[0].ir_addr, 8, 0, &ir->ir_bh)) {
		aprint_error(": can't map i/o space\n");
		return;
	}

	/* Bus-independent attachment */
	sc->sc_dev = self;

	aprint_normal(": IDEC Supervision/16\n"); 

	/* TBD */
}

int
isv_open(dev_t dev, int flag, int devtype, lwp_t *l)
{
	vaddr_t va;
	struct isv_softc *sc = device_lookup_private(&isv_cd, minor(dev));

	if (sc == NULL)
		return ENXIO;

	if (sc->sc_frame != NULL)
		return 0;

	if ((va = uvm_km_alloc(kernel_map, ISV_WIDTH * ISV_LINES, PAGE_SIZE,
	    UVM_KMF_WIRED|UVM_KMF_ZERO|UVM_KMF_CANFAIL|UVM_KMF_WAITVA)) == 0)
		return ENOMEM;

	sc->sc_frame = (uint16_t *)(void *)va;
	return 0;
}

/* wait for retrace */
static int
isv_retrace_wait(struct isv_regs *ir, int *state, const struct timeval *end)
{
	struct timeval now;

	for (;;) {
		if (!isv_delta(state, isv_retrace(ir))) {
			microtime(&now);
			continue;
		}
		if (*state == ISV_S_RETRACE)
			break;
		if (*state != ISV_S_CAPTURE0)
			continue;

		microtime(&now);
		if (timercmp(&now, end, >=))
			return EIO;
	}
	return 0;
}

/* wait for capture mode */
static int
isv_capture_wait(struct isv_regs *ir, int *state, const struct timeval *end)
{
	struct timeval now;

	for (;;) {
		if (!isv_delta(state, isv_retrace(ir))) {
			microtime(&now);
			continue;
		}
		if (*state != ISV_S_RETRACE)
			break;

		microtime(&now);
		if (timercmp(&now, end, >=))
			return EIO;
	}
	return 0;
}


static int
isv_capture(struct isv_softc *sc)
{
	int speed;
	int rc, state = ISV_S_CAPTURE0;
	struct timeval diff, end, start, stop;
	static const struct timeval wait = {.tv_sec = 0, .tv_usec = 200000};
	struct isv_regs *ir = &sc->sc_ir;

	if (sc->sc_frame == NULL)
		return EAGAIN;

	microtime(&start);

	timeradd(&start, &wait, &end);

	speed = sc->sc_speed;
	sc->sc_speed = 0;

	if (speed < 1 && (rc = isv_retrace_wait(ir, &state, &end)) != 0)
		return rc;

	if (speed < 2 && (rc = isv_capture_wait(ir, &state, &end)) != 0)
		return rc;

	if ((rc = isv_retrace_wait(ir, &state, &end)) != 0)
		return rc;

	microtime(&stop);

	timersub(&stop, &start, &diff);

	aprint_debug_dev(sc->sc_dev, "%ssync in %" PRId64 ".%06d seconds\n",
	    (speed < 1) ? "" : ((speed < 2) ? "faster " : "fastest "),
	    diff.tv_sec, diff.tv_usec);

	microtime(&start);

	/* enter read mode, then toggle counter mode,
	 * autoinc -> reset -> autoinc, so that we start reading
	 * at the top of the frame.
	 */
	isv_write(ir, ISV_CONTROL,
	    ISV_CONTROL_MODE_READ|ISV_CONTROL_COUNTER_AUTOINC);
	isv_write(ir, ISV_CONTROL,
	    ISV_CONTROL_MODE_READ|ISV_CONTROL_COUNTER_RESET);
	isv_write(ir, ISV_CONTROL,
	    ISV_CONTROL_MODE_READ|ISV_CONTROL_COUNTER_AUTOINC);
	/* read one dummy word to prime the state machine on the
	 * image capture board
	 */
	isv_read(ir, ISV_DATA);
	bus_space_read_multi_stream_2(ir->ir_bt, ir->ir_bh, ISV_DATA,
	    sc->sc_frame, ISV_WIDTH * ISV_LINES / 2);

	/* restore to initial conditions */
	isv_write(ir, ISV_CONTROL,
	    ISV_CONTROL_MODE_CAPTURE|ISV_CONTROL_COUNTER_AUTOINC);

	microtime(&stop);

	timersub(&stop, &start, &diff);

	aprint_debug_dev(sc->sc_dev, "read in %" PRId64 ".%06d seconds\n",
		diff.tv_sec, diff.tv_usec);

	state = 0;

	if (isv_retrace_wait(ir, &state, &end) != 0)
		return 0;
	sc->sc_speed++;

	if (isv_capture_wait(ir, &state, &end) != 0)
		return 0;
	sc->sc_speed++;

	return 0;
}

int
isv_ioctl(dev_t dev, u_long cmd, void *data, int flag, lwp_t *l)
{
	struct isv_cmd ic;
	struct isv_softc *sc = device_lookup_private(&isv_cd, minor(dev));

	if (cmd != ISV_CMD)
		return ENOTTY;

	memcpy(&ic, data, sizeof(ic));

	if (ic.c_cmd != ISV_CMD_READ)
		return EINVAL;

	ic.c_frameno = 0;

	return isv_capture(sc);
}

paddr_t
isv_mmap(dev_t dev, off_t offset, int prot)
{
	struct isv_softc *sc = device_lookup_private(&isv_cd, minor(dev));
	paddr_t pa;

	if ((prot & ~(VM_PROT_READ)) != 0)
		return -1;

	if (sc->sc_frame == NULL)
		return -1;

	if (offset >= ISV_WIDTH * ISV_LINES)
		return -1;

	if (!pmap_extract(pmap_kernel(), (vaddr_t)&sc->sc_frame[offset/2], &pa))
		return -1;

	return atop(pa);
}

static int
isv_detach(device_t self, int flags)
{
	struct isv_softc *sc = device_private(self);
	struct isv_regs *ir = &sc->sc_ir;

	if (sc->sc_frame != NULL) {
		uvm_km_free(kernel_map, (vaddr_t)sc->sc_frame,
		    ISV_WIDTH * ISV_LINES, UVM_KMF_WIRED);
	}
	bus_space_unmap(ir->ir_bt, ir->ir_bh, 8);
	return 0;
}
