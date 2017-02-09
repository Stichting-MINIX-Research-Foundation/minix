/* $NetBSD: zs_ioasic.c,v 1.40 2009/05/12 13:21:22 cegger Exp $ */

/*-
 * Copyright (c) 1996, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross, Ken Hornstein, and by Jason R. Thorpe of the
 * Numerical Aerospace Simulation Facility, NASA Ames Research Center.
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
 * Zilog Z8530 Dual UART driver (machine-dependent part).  This driver
 * handles Z8530 chips attached to the DECstation/Alpha IOASIC.  Modified
 * for NetBSD/alpha by Ken Hornstein and Jason R. Thorpe.  NetBSD/pmax
 * adaption by Mattias Drochner.  Merge work by Tohru Nishimura.
 *
 * Runs two serial lines per chip using slave drivers.
 * Plain tty/async lines use the zstty slave.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: zs_ioasic.c,v 1.40 2009/05/12 13:21:22 cegger Exp $");

#include "opt_ddb.h"
#include "opt_kgdb.h"
#include "zskbd.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/syslog.h>
#include <sys/intr.h>

#include <machine/autoconf.h>
#include <machine/z8530var.h>

#include <dev/cons.h>
#include <dev/ic/z8530reg.h>

#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicreg.h>
#include <dev/tc/ioasicvar.h>

#include <dev/tc/zs_ioasicvar.h>

#if defined(__alpha__) || defined(alpha)
#include <machine/rpb.h>
#endif
#if defined(pmax)
#include <pmax/pmax/pmaxtype.h>
#endif

/*
 * Helpers for console support.
 */
static void	zs_ioasic_cninit(tc_addr_t, tc_offset_t, int);
static int	zs_ioasic_cngetc(dev_t);
static void	zs_ioasic_cnputc(dev_t, int);
static void	zs_ioasic_cnpollc(dev_t, int);

struct consdev zs_ioasic_cons = {
	NULL, NULL, zs_ioasic_cngetc, zs_ioasic_cnputc,
	zs_ioasic_cnpollc, NULL, NULL, NULL, NODEV, CN_NORMAL,
};

static tc_offset_t zs_ioasic_console_offset;
static int zs_ioasic_console_channel;
static int zs_ioasic_console;
static struct zs_chanstate zs_ioasic_conschanstate_store;

static int	zs_ioasic_isconsole(tc_offset_t, int);
static void	zs_putc(struct zs_chanstate *, int);

/*
 * Some warts needed by z8530tty.c
 */
int zs_def_cflag = (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8;

/*
 * ZS chips are feeded a 7.372 MHz clock.
 */
#define	PCLK	(9600 * 768)	/* PCLK pin input clock rate */

/* The layout of this is hardware-dependent (padding, order). */
struct zshan {
#if defined(__alpha__) || defined(alpha)
	volatile u_int	zc_csr;		/* ctrl,status, and indirect access */
	u_int		zc_pad0;
	volatile u_int	zc_data;	/* data */
	u_int		sc_pad1;
#endif
#if defined(pmax)
	volatile uint16_t zc_csr;	/* ctrl,status, and indirect access */
	unsigned : 16;
	volatile uint16_t zc_data;	/* data */
	unsigned : 16;
#endif
};

struct zsdevice {
	/* Yes, they are backwards. */
	struct	zshan zs_chan_b;
	struct	zshan zs_chan_a;
};

static const u_char zs_ioasic_init_reg[16] = {
	0,	/* 0: CMD (reset, etc.) */
	0,	/* 1: No interrupts yet. */
	0xf0,	/* 2: IVECT */
	ZSWR3_RX_8 | ZSWR3_RX_ENABLE,
	ZSWR4_CLK_X16 | ZSWR4_ONESB,
	ZSWR5_TX_8 | ZSWR5_TX_ENABLE,
	0,	/* 6: TXSYNC/SYNCLO */
	0,	/* 7: RXSYNC/SYNCHI */
	0,	/* 8: alias for data port */
	ZSWR9_MASTER_IE | ZSWR9_VECTOR_INCL_STAT,
	0,	/*10: Misc. TX/RX control bits */
	ZSWR11_TXCLK_BAUD | ZSWR11_RXCLK_BAUD,
	22,	/*12: BAUDLO (default=9600) */
	0,	/*13: BAUDHI (default=9600) */
	ZSWR14_BAUD_ENA | ZSWR14_BAUD_FROM_PCLK,
	ZSWR15_BREAK_IE,
};

static struct zshan *
zs_ioasic_get_chan_addr(tc_addr_t zsaddr, int channel)
{
	struct zsdevice *addr;
	struct zshan *zc;

#if defined(__alpha__) || defined(alpha)
	addr = (struct zsdevice *)TC_DENSE_TO_SPARSE(zsaddr);
#endif
#if defined(pmax)
	addr = (struct zsdevice *)MIPS_PHYS_TO_KSEG1(zsaddr);
#endif

	if (channel == 0)
		zc = &addr->zs_chan_a;
	else
		zc = &addr->zs_chan_b;

	return (zc);
}


/****************************************************************
 * Autoconfig
 ****************************************************************/

/* Definition of the driver for autoconfig. */
static int	zs_ioasic_match(device_t, cfdata_t, void *);
static void	zs_ioasic_attach(device_t, device_t, void *);
static int	zs_ioasic_print(void *, const char *name);
static int	zs_ioasic_submatch(device_t, cfdata_t,
				   const int *, void *);

CFATTACH_DECL_NEW(zsc_ioasic, sizeof(struct zsc_softc),
    zs_ioasic_match, zs_ioasic_attach, NULL, NULL);

/* Interrupt handlers. */
static int	zs_ioasic_hardintr(void *);
static void	zs_ioasic_softintr(void *);

/*
 * Is the zs chip present?
 */
static int
zs_ioasic_match(device_t parent, cfdata_t cf, void *aux)
{
	struct ioasicdev_attach_args *d = aux;
	tc_addr_t zs_addr;

	/*
	 * Make sure that we're looking for the right kind of device.
	 */
	if (strncmp(d->iada_modname, "z8530   ", TC_ROM_LLEN) != 0 &&
	    strncmp(d->iada_modname, "scc", TC_ROM_LLEN) != 0)
		return (0);

	/*
	 * Find out the device address, and check it for validity.
	 */
	zs_addr = TC_DENSE_TO_SPARSE((tc_addr_t)d->iada_addr);
	if (tc_badaddr(zs_addr))
		return (0);

	return (1);
}

/*
 * Attach a found zs.
 */
static void
zs_ioasic_attach(device_t parent, device_t self, void *aux)
{
	struct zsc_softc *zs = device_private(self);
	struct zsc_attach_args zs_args;
	struct zs_chanstate *cs;
	struct ioasicdev_attach_args *d = aux;
	struct zshan *zc;
	int s, channel;
	u_long zflg;
	int locs[ZSCCF_NLOCS];

	zs->zsc_dev = self;
	aprint_normal("\n");

	/*
	 * Initialize software state for each channel.
	 */
	for (channel = 0; channel < 2; channel++) {
		zs_args.channel = channel;
		zs_args.hwflags = 0;

		if (zs_ioasic_isconsole(d->iada_offset, channel)) {
			cs = &zs_ioasic_conschanstate_store;
			zs_args.hwflags |= ZS_HWFLAG_CONSOLE;
		} else {
			cs = malloc(sizeof(struct zs_chanstate),
					M_DEVBUF, M_NOWAIT|M_ZERO);
			zs_lock_init(cs);
			zc = zs_ioasic_get_chan_addr(d->iada_addr, channel);
			cs->cs_reg_csr = (volatile void *)&zc->zc_csr;

			memcpy(cs->cs_creg, zs_ioasic_init_reg, 16);
			memcpy(cs->cs_preg, zs_ioasic_init_reg, 16);

			cs->cs_defcflag = zs_def_cflag;
			cs->cs_defspeed = 9600;		/* XXX */
			(void)zs_set_modes(cs, cs->cs_defcflag);
		}

		zs->zsc_cs[channel] = cs;
		zs->zsc_addroffset = d->iada_offset; /* cookie only */
		cs->cs_channel = channel;
		cs->cs_ops = &zsops_null;
		cs->cs_brg_clk = PCLK / 16;

		/*
		 * DCD and CTS interrupts are only meaningful on
		 * SCC 0/B, and RTS and DTR only on B of SCC 0 & 1.
		 *
		 * XXX This is sorta gross.
		 */
		if (d->iada_offset == 0x00100000 && channel == 1) {
			cs->cs_creg[15] |= ZSWR15_DCD_IE;
			cs->cs_preg[15] |= ZSWR15_DCD_IE;
			zflg = ZIP_FLAGS_DCDCTS;
		} else
			zflg = 0;
		if (channel == 1)
			zflg |= ZIP_FLAGS_DTRRTS;
		cs->cs_private = (void *)zflg;

		/*
		 * Clear the master interrupt enable.
		 * The INTENA is common to both channels,
		 * so just do it on the A channel.
		 */
		if (channel == 0) {
			zs_write_reg(cs, 9, 0);
		}

		/*
		 * Set up the flow/modem control channel pointer to
		 * deal with the weird wiring on the TC Alpha and
		 * DECstation.
		 */
		if (channel == 1)
			cs->cs_ctl_chan = zs->zsc_cs[0];
		else
			cs->cs_ctl_chan = NULL;

		locs[ZSCCF_CHANNEL] = channel;

		/*
		 * Look for a child driver for this channel.
		 * The child attach will setup the hardware.
		 */
		if (config_found_sm_loc(self, "zsc", locs, (void *)&zs_args,
				zs_ioasic_print, zs_ioasic_submatch) == NULL) {
			/* No sub-driver.  Just reset it. */
			uint8_t reset = (channel == 0) ?
			    ZSWR9_A_RESET : ZSWR9_B_RESET;
			s = splhigh();
			zs_write_reg(cs, 9, reset);
			splx(s);
		}
	}

	/*
	 * Set up the ioasic interrupt handler.
	 */
	ioasic_intr_establish(parent, d->iada_cookie, TC_IPL_TTY,
	    zs_ioasic_hardintr, zs);
	zs->zsc_sih = softint_establish(SOFTINT_SERIAL,
	    zs_ioasic_softintr, zs);
	if (zs->zsc_sih == NULL)
		panic("%s: unable to register softintr", __func__);

	/*
	 * Set the master interrupt enable and interrupt vector.  The
	 * Sun does this only on one channel.  The old Alpha SCC driver
	 * did it on both.  We'll do it on both.
	 */
	s = splhigh();
	/* interrupt vector */
	zs_write_reg(zs->zsc_cs[0], 2, zs_ioasic_init_reg[2]);
	zs_write_reg(zs->zsc_cs[1], 2, zs_ioasic_init_reg[2]);

	/* master interrupt control (enable) */
	zs_write_reg(zs->zsc_cs[0], 9, zs_ioasic_init_reg[9]);
	zs_write_reg(zs->zsc_cs[1], 9, zs_ioasic_init_reg[9]);
#if defined(__alpha__) || defined(alpha)
	/* ioasic interrupt enable */
	*(volatile u_int *)(ioasic_base + IOASIC_IMSK) |=
		    IOASIC_INTR_SCC_1 | IOASIC_INTR_SCC_0;
	tc_mb();
#endif
	splx(s);
}

static int
zs_ioasic_print(void *aux, const char *name)
{
	struct zsc_attach_args *args = aux;

	if (name != NULL)
		aprint_normal("%s:", name);

	if (args->channel != -1)
		aprint_normal(" channel %d", args->channel);

	return (UNCONF);
}

static int
zs_ioasic_submatch(device_t parent, cfdata_t cf, const int *locs, void *aux)
{
	struct zsc_softc *zs = device_private(parent);
	struct zsc_attach_args *pa = aux;
	const char *defname = "";

	if (cf->cf_loc[ZSCCF_CHANNEL] != ZSCCF_CHANNEL_DEFAULT &&
	    cf->cf_loc[ZSCCF_CHANNEL] != locs[ZSCCF_CHANNEL])
		return (0);

	if (cf->cf_loc[ZSCCF_CHANNEL] == ZSCCF_CHANNEL_DEFAULT) {
		if (pa->channel == 0) {
#if defined(pmax)
			if (systype == DS_MAXINE)
				return (0);
#endif
			if (zs->zsc_addroffset == 0x100000)
				defname = "vsms";
			else
				defname = "lkkbd";
		}
		else if (zs->zsc_addroffset == 0x100000)
			defname = "zstty";
#if defined(pmax)
		else if (systype == DS_MAXINE)
			return (0);
#endif
#if defined(__alpha__) || defined(alpha)
		else if (cputype == ST_DEC_3000_300)
			return (0);
#endif
		else
			defname = "zstty"; /* 3min/3max+, DEC3000/500 */

		if (strcmp(cf->cf_name, defname))
			return (0);
	}
	return (config_match(parent, cf, aux));
}

/*
 * Hardware interrupt handler.
 */
static int
zs_ioasic_hardintr(void *arg)
{
	struct zsc_softc *zsc = arg;

	/*
	 * Call the upper-level MI hardware interrupt handler.
	 */
	zsc_intr_hard(zsc);

	/*
	 * Check to see if we need to schedule any software-level
	 * processing interrupts.
	 */
	if (zsc->zsc_cs[0]->cs_softreq | zsc->zsc_cs[1]->cs_softreq)
		softint_schedule(zsc->zsc_sih);

	return (1);
}

/*
 * Software-level interrupt (character processing, lower priority).
 */
static void
zs_ioasic_softintr(void *arg)
{
	struct zsc_softc *zsc = arg;
	int s;

	s = spltty();
	(void)zsc_intr_soft(zsc);
	splx(s);
}

/*
 * MD functions for setting the baud rate and control modes.
 */
int
zs_set_speed(struct zs_chanstate *cs, int bps /*bits per second*/)
{
	int tconst, real_bps;

	if (bps == 0)
		return (0);

#ifdef DIAGNOSTIC
	if (cs->cs_brg_clk == 0)
		panic("zs_set_speed");
#endif

	tconst = BPS_TO_TCONST(cs->cs_brg_clk, bps);
	if (tconst < 0)
		return (EINVAL);

	/* Convert back to make sure we can do it. */
	real_bps = TCONST_TO_BPS(cs->cs_brg_clk, tconst);

	/* XXX - Allow some tolerance here? */
	if (real_bps != bps)
		return (EINVAL);

	cs->cs_preg[12] = tconst;
	cs->cs_preg[13] = tconst >> 8;

	/* Caller will stuff the pending registers. */
	return (0);
}

int
zs_set_modes(struct zs_chanstate *cs, int cflag)
{
	u_long privflags = (u_long)cs->cs_private;
	int s;

	/*
	 * Output hardware flow control on the chip is horrendous:
	 * if carrier detect drops, the receiver is disabled, and if
	 * CTS drops, the transmitter is stoped IN MID CHARACTER!
	 * Therefore, NEVER set the HFC bit, and instead use the
	 * status interrupt to detect CTS changes.
	 */
	s = splzs();
	if ((cflag & (CLOCAL | MDMBUF)) != 0)
		cs->cs_rr0_dcd = 0;
	else
		cs->cs_rr0_dcd = ZSRR0_DCD;
	if ((cflag & CRTSCTS) != 0) {
		cs->cs_wr5_dtr = ZSWR5_DTR;
		cs->cs_wr5_rts = ZSWR5_RTS;
		cs->cs_rr0_cts = ZSRR0_CTS;
	} else if ((cflag & CDTRCTS) != 0) {
		cs->cs_wr5_dtr = 0;
		cs->cs_wr5_rts = ZSWR5_DTR;
		cs->cs_rr0_cts = ZSRR0_CTS;
	} else if ((cflag & MDMBUF) != 0) {
		cs->cs_wr5_dtr = 0;
		cs->cs_wr5_rts = ZSWR5_DTR;
		cs->cs_rr0_cts = ZSRR0_DCD;
	} else {
		cs->cs_wr5_dtr = ZSWR5_DTR | ZSWR5_RTS;
		cs->cs_wr5_rts = 0;
		cs->cs_rr0_cts = 0;
	}

	if ((privflags & ZIP_FLAGS_DCDCTS) == 0) {
		cs->cs_rr0_dcd &= ~(ZSRR0_CTS|ZSRR0_DCD);
		cs->cs_rr0_cts &= ~(ZSRR0_CTS|ZSRR0_DCD);
	}
	if ((privflags & ZIP_FLAGS_DTRRTS) == 0) {
		cs->cs_wr5_dtr &= ~(ZSWR5_RTS|ZSWR5_DTR);
		cs->cs_wr5_rts &= ~(ZSWR5_RTS|ZSWR5_DTR);
	}
	splx(s);

	/* Caller will stuff the pending registers. */
	return (0);
}

/*
 * Functions to read and write individual registers in a channel.
 * The ZS chip requires a 1.6 uSec. recovery time between accesses,
 * and the Alpha TC hardware does NOT take care of this for you.
 * The delay is now handled inside the chip access functions.
 * These could be inlines, but with the delay, speed is moot.
 */
#if defined(pmax)
#undef	DELAY
#define	DELAY(x)
#endif

u_int
zs_read_reg(struct zs_chanstate *cs, u_int reg)
{
	volatile struct zshan *zc = (volatile void *)cs->cs_reg_csr;
	unsigned val;

	zc->zc_csr = reg << 8;
	tc_wmb();
	DELAY(5);
	val = (zc->zc_csr >> 8) & 0xff;
	/* tc_mb(); */
	DELAY(5);
	return (val);
}

void
zs_write_reg(struct zs_chanstate *cs, u_int reg, u_int val)
{
	volatile struct zshan *zc = (volatile void *)cs->cs_reg_csr;

	zc->zc_csr = reg << 8;
	tc_wmb();
	DELAY(5);
	zc->zc_csr = val << 8;
	tc_wmb();
	DELAY(5);
}

u_int
zs_read_csr(struct zs_chanstate *cs)
{
	volatile struct zshan *zc = (volatile void *)cs->cs_reg_csr;
	unsigned val;

	val = (zc->zc_csr >> 8) & 0xff;
	/* tc_mb(); */
	DELAY(5);
	return (val);
}

void
zs_write_csr(struct zs_chanstate *cs, u_int val)
{
	volatile struct zshan *zc = (volatile void *)cs->cs_reg_csr;

	zc->zc_csr = val << 8;
	tc_wmb();
	DELAY(5);
}

u_int
zs_read_data(struct zs_chanstate *cs)
{
	volatile struct zshan *zc = (volatile void *)cs->cs_reg_csr;
	unsigned val;

	val = (zc->zc_data) >> 8 & 0xff;
	/* tc_mb(); */
	DELAY(5);
	return (val);
}

void
zs_write_data(struct zs_chanstate *cs, u_int val)
{
	volatile struct zshan *zc = (volatile void *)cs->cs_reg_csr;

	zc->zc_data = val << 8;
	tc_wmb();
	DELAY(5);
}

/****************************************************************
 * Console support functions
 ****************************************************************/

/*
 * Handle user request to enter kernel debugger.
 */
void
zs_abort(struct zs_chanstate *cs)
{
	u_int rr0;

	/* Wait for end of break. */
	/* XXX - Limit the wait? */
	do {
		rr0 = zs_read_csr(cs);
	} while (rr0 & ZSRR0_BREAK);

#if defined(KGDB)
	zskgdb(cs);
#elif defined(DDB)
	Debugger();
#else
	printf("zs_abort: ignoring break on console\n");
#endif
}

/*
 * Polled input char.
 */
int
zs_getc(struct zs_chanstate *cs)
{
	int s, c;
	u_int rr0;

	s = splhigh();
	/* Wait for a character to arrive. */
	do {
		rr0 = zs_read_csr(cs);
	} while ((rr0 & ZSRR0_RX_READY) == 0);

	c = zs_read_data(cs);
	splx(s);

	/*
	 * This is used by the kd driver to read scan codes,
	 * so don't translate '\r' ==> '\n' here...
	 */
	return (c);
}

/*
 * Polled output char.
 */
static void
zs_putc(struct zs_chanstate *cs, int c)
{
	int s;
	u_int rr0;

	s = splhigh();
	/* Wait for transmitter to become ready. */
	do {
		rr0 = zs_read_csr(cs);
	} while ((rr0 & ZSRR0_TX_READY) == 0);

	zs_write_data(cs, c);

	/* Wait for the character to be transmitted. */
	do {
		rr0 = zs_read_csr(cs);
	} while ((rr0 & ZSRR0_TX_READY) == 0);
	splx(s);
}

/*****************************************************************/

/*
 * zs_ioasic_cninit --
 *	Initialize the serial channel for either a keyboard or
 *	a serial console.
 */
static void
zs_ioasic_cninit(tc_addr_t ioasic_addr, tc_offset_t zs_offset, int channel)
{
	struct zs_chanstate *cs;
	tc_addr_t zs_addr;
	struct zshan *zc;
	u_long zflg;

	/*
	 * Initialize the console finder helpers.
	 */
	zs_ioasic_console_offset = zs_offset;
	zs_ioasic_console_channel = channel;
	zs_ioasic_console = 1;

	/*
	 * Pointer to channel state.
	 */
	cs = &zs_ioasic_conschanstate_store;

	/*
	 * Compute the physical address of the chip, "map" it via
	 * K0SEG, and then get the address of the actual channel.
	 */
#if defined(__alpha__) || defined(alpha)
	zs_addr = ALPHA_PHYS_TO_K0SEG(ioasic_addr + zs_offset);
#endif
#if defined(pmax)
	zs_addr = MIPS_PHYS_TO_KSEG1(ioasic_addr + zs_offset);
#endif
	zc = zs_ioasic_get_chan_addr(zs_addr, channel);

	/* Setup temporary chanstate. */
	cs->cs_reg_csr = (volatile void *)&zc->zc_csr;

	cs->cs_channel = channel;
	cs->cs_ops = &zsops_null;
	cs->cs_brg_clk = PCLK / 16;

	/* Initialize the pending registers. */
	memcpy(cs->cs_preg, zs_ioasic_init_reg, 16);
	/* cs->cs_preg[5] |= (ZSWR5_DTR | ZSWR5_RTS); */

	/*
	 * DCD and CTS interrupts are only meaningful on
	 * SCC 0/B, and RTS and DTR only on B of SCC 0 & 1.
	 *
	 * XXX This is sorta gross.
	 */
	if (zs_offset == 0x00100000 && channel == 1)
		zflg = ZIP_FLAGS_DCDCTS;
	else
		zflg = 0;
	if (channel == 1)
		zflg |= ZIP_FLAGS_DTRRTS;
	cs->cs_private = (void *)zflg;

	/* Clear the master interrupt enable. */
	zs_write_reg(cs, 9, 0);

	/* Reset the whole SCC chip. */
	zs_write_reg(cs, 9, ZSWR9_HARD_RESET);

	/* Copy "pending" to "current" and H/W. */
	zs_loadchannelregs(cs);
}

/*
 * zs_ioasic_cnattach --
 *	Initialize and attach a serial console.
 */
void
zs_ioasic_cnattach(tc_addr_t ioasic_addr, tc_offset_t zs_offset, int channel)
{
	struct zs_chanstate *cs = &zs_ioasic_conschanstate_store;
	extern const struct cdevsw zstty_cdevsw;

	zs_ioasic_cninit(ioasic_addr, zs_offset, channel);
	zs_lock_init(cs);
	cs->cs_defspeed = 9600;
	cs->cs_defcflag = (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8;

	/* Point the console at the SCC. */
	cn_tab = &zs_ioasic_cons;
	cn_tab->cn_pri = CN_REMOTE;
	cn_tab->cn_dev = makedev(cdevsw_lookup_major(&zstty_cdevsw),
				 (zs_offset == 0x100000) ? 0 : 1);
}

/*
 * zs_ioasic_lk201_cnattach --
 *	Initialize and attach a keyboard.
 */
int
zs_ioasic_lk201_cnattach(tc_addr_t ioasic_addr, tc_offset_t zs_offset,
    int channel)
{
#if (NZSKBD > 0)
	struct zs_chanstate *cs = &zs_ioasic_conschanstate_store;

	zs_ioasic_cninit(ioasic_addr, zs_offset, channel);
	zs_lock_init(cs);
	cs->cs_defspeed = 4800;
	cs->cs_defcflag = (TTYDEF_CFLAG & ~(CSIZE | PARENB)) | CS8;
	return (zskbd_cnattach(cs));
#else
	return (ENXIO);
#endif
}

static int
zs_ioasic_isconsole(tc_offset_t offset, int channel)
{

	if (zs_ioasic_console &&
	    offset == zs_ioasic_console_offset &&
	    channel == zs_ioasic_console_channel)
		return (1);

	return (0);
}

/*
 * Polled console input putchar.
 */
static int
zs_ioasic_cngetc(dev_t dev)
{

	return (zs_getc(&zs_ioasic_conschanstate_store));
}

/*
 * Polled console output putchar.
 */
static void
zs_ioasic_cnputc(dev_t dev, int c)
{

	zs_putc(&zs_ioasic_conschanstate_store, c);
}

/*
 * Set polling/no polling on console.
 */
static void
zs_ioasic_cnpollc(dev_t dev, int onoff)
{

	/* XXX ??? */
}
