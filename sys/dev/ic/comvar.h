/*	$NetBSD: comvar.h,v 1.81 2015/05/03 17:22:54 jmcneill Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_multiprocessor.h"
#include "opt_lockdebug.h"
#include "opt_com.h"
#include "opt_kgdb.h"

#ifdef RND_COM
#include <sys/rndsource.h>
#endif

#include <sys/callout.h>
#include <sys/timepps.h>
#include <sys/mutex.h>
#include <sys/device.h>

#include <dev/ic/comreg.h>	/* for COM_NPORTS */

struct com_regs;

int comcnattach(bus_space_tag_t, bus_addr_t, int, int, int, tcflag_t);
int comcnattach1(struct com_regs *, int, int, int, tcflag_t);

#ifdef KGDB
int com_kgdb_attach(bus_space_tag_t, bus_addr_t, int, int, int, tcflag_t);
int com_kgdb_attach1(struct com_regs *, int, int, int, tcflag_t);
#endif

int com_is_console(bus_space_tag_t, bus_addr_t, bus_space_handle_t *);

/* Hardware flag masks */
#define	COM_HW_NOIEN	0x01
#define	COM_HW_FIFO	0x02
		/*	0x04	free for use */
#define	COM_HW_FLOW	0x08
#define	COM_HW_DEV_OK	0x20
#define	COM_HW_CONSOLE	0x40
#define	COM_HW_KGDB	0x80
#define	COM_HW_TXFIFO_DISABLE	0x100
#define	COM_HW_NO_TXPRELOAD	0x200
#define	COM_HW_AFE	0x400

/* Buffer size for character buffer */
#ifndef COM_RING_SIZE
#define	COM_RING_SIZE	2048
#endif

#ifdef	COM_REGMAP
#define	COM_REG_RXDATA		0
#define	COM_REG_TXDATA		1
#define	COM_REG_DLBL		2
#define	COM_REG_DLBH		3
#define	COM_REG_IER		4
#define	COM_REG_IIR		5
#define	COM_REG_FIFO		6
#define	COM_REG_TCR		6
#define	COM_REG_EFR		7
#define	COM_REG_TLR		7
#define	COM_REG_LCR		8
#define	COM_REG_MDR1		8
#define	COM_REG_MCR		9
#define	COM_REG_LSR		10
#define	COM_REG_MSR		11
#ifdef	COM_16750
#define	COM_REG_USR		31
#endif

struct com_regs {
	bus_space_tag_t		cr_iot;
	bus_space_handle_t	cr_ioh;
	bus_addr_t		cr_iobase;
	bus_size_t		cr_nports;
#ifdef COM_16750
	bus_size_t		cr_map[32];
#else
	bus_size_t		cr_map[16];
#endif
};

#ifdef COM_16750
extern const bus_size_t com_std_map[32];
#else
extern const bus_size_t com_std_map[16];
#endif

#define	COM_INIT_REGS(regs, tag, hdl, addr)				\
	do {								\
		regs.cr_iot = tag;					\
		regs.cr_ioh = hdl;					\
		regs.cr_iobase = addr;					\
		regs.cr_nports = COM_NPORTS;				\
		memcpy(regs.cr_map, com_std_map, sizeof (regs.cr_map));	\
	} while (0)

#else
#define	COM_REG_RXDATA		com_data
#define	COM_REG_TXDATA		com_data
#define	COM_REG_DLBL		com_dlbl
#define	COM_REG_DLBH		com_dlbh
#define	COM_REG_IER		com_ier
#define	COM_REG_IIR		com_iir
#define	COM_REG_FIFO		com_fifo
#define	COM_REG_EFR		com_efr
#define	COM_REG_LCR		com_lctl
#define	COM_REG_MCR		com_mcr
#define	COM_REG_LSR		com_lsr
#define	COM_REG_MSR		com_msr
#define	COM_REG_TCR		com_msr
#define	COM_REG_TLR		com_scratch
#define	COM_REG_MDR1		8
#ifdef	COM_16750
#define COM_REG_USR		com_usr
#endif

struct com_regs {
	bus_space_tag_t		cr_iot;
	bus_space_handle_t	cr_ioh;
	bus_addr_t		cr_iobase;
	bus_size_t		cr_nports;
};

#define	COM_INIT_REGS(regs, tag, hdl, addr)		\
	do {						\
		regs.cr_iot = tag;			\
		regs.cr_ioh = hdl;			\
		regs.cr_iobase = addr;			\
		regs.cr_nports = COM_NPORTS;		\
	} while (0)

#endif

struct comcons_info {
	struct com_regs regs;
	int rate;
	int frequency;
	int type;
	tcflag_t cflag;
};

struct com_softc {
	device_t sc_dev;
	void *sc_si;
	struct tty *sc_tty;

	struct callout sc_diag_callout;

	int sc_frequency;

	struct com_regs sc_regs;
	bus_space_handle_t sc_hayespioh;


	u_int sc_overflows,
	      sc_floods,
	      sc_errors;

	int sc_hwflags,
	    sc_swflags;
	u_int sc_fifolen;

	u_int sc_r_hiwat,
	      sc_r_lowat;
	u_char *volatile sc_rbget,
	       *volatile sc_rbput;
 	volatile u_int sc_rbavail;
	u_char *sc_rbuf,
	       *sc_ebuf;

 	u_char *sc_tba;
 	u_int sc_tbc,
	      sc_heldtbc;

	volatile u_char sc_rx_flags,
#define	RX_TTY_BLOCKED		0x01
#define	RX_TTY_OVERFLOWED	0x02
#define	RX_IBUF_BLOCKED		0x04
#define	RX_IBUF_OVERFLOWED	0x08
#define	RX_ANY_BLOCK		0x0f
			sc_tx_busy,
			sc_tx_done,
			sc_tx_stopped,
			sc_st_check,
			sc_rx_ready;

	volatile u_char sc_heldchange;
	volatile u_char sc_msr, sc_msr_delta, sc_msr_mask, sc_mcr,
	    sc_mcr_active, sc_lcr, sc_ier, sc_fifo, sc_dlbl, sc_dlbh, sc_efr;
	u_char sc_mcr_dtr, sc_mcr_rts, sc_msr_cts, sc_msr_dcd;

#ifdef COM_HAYESP
	u_char sc_prescaler;
#endif

	/*
	 * There are a great many almost-ns16550-compatible UARTs out
	 * there, which have minor differences.  The type field here
	 * lets us distinguish between them.
	 */
	int sc_type;
#define	COM_TYPE_NORMAL		0	/* normal 16x50 */
#define	COM_TYPE_HAYESP		1	/* Hayes ESP modem */
#define	COM_TYPE_PXA2x0		2	/* Intel PXA2x0 processor built-in */
#define	COM_TYPE_AU1x00		3	/* AMD/Alchemy Au1x000 proc. built-in */
#define	COM_TYPE_OMAP		4	/* TI OMAP processor built-in */
#define	COM_TYPE_16550_NOERS	5	/* like a 16550, no ERS */
#define	COM_TYPE_INGENIC	6	/* JZ4780 built-in */
#define	COM_TYPE_TEGRA		7	/* NVIDIA Tegra built-in */

	/* power management hooks */
	int (*enable)(struct com_softc *);
	void (*disable)(struct com_softc *);
	int enabled;

	/* XXXX: vendor workaround functions */
	int (*sc_vendor_workaround)(struct com_softc *);

	struct pps_state sc_pps_state;	/* pps state */

#ifdef RND_COM
	krndsource_t  rnd_source;
#endif
	kmutex_t		sc_lock;
};

int comprobe1(bus_space_tag_t, bus_space_handle_t);
int comintr(void *);
void com_attach_subr(struct com_softc *);
int com_probe_subr(struct com_regs *);
int com_detach(device_t, int);
bool com_resume(device_t, const pmf_qual_t *);
bool com_cleanup(device_t, int);
bool com_suspend(device_t, const pmf_qual_t *);

#ifndef IPL_SERIAL
#define	IPL_SERIAL	IPL_TTY
#define	splserial()	spltty()
#endif
