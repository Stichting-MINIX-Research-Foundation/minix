/*	$NetBSD: ncr5380var.h,v 1.33 2012/07/28 00:43:23 matt Exp $	*/

/*
 * Copyright (c) 1995 David Jones, Gordon W. Ross
 * Copyright (c) 1994 Jarle Greipsland
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
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by
 *      David Jones and Gordon Ross
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file defines the interface between the machine-dependent
 * module and the machine-independent ncr5380sbc.c module.
 */

/*
 * Currently acorn26, amd64, alpha, i386, mips, news68k, sparc, sun2, and vax
 * use real bus space:
 *	acorn32: csa driver; easy to convert
 *	mac68k: sbc driver; easy to convert
 *	pc532: ncr driver; need bus.h first
 *	sun3: si driver; need bus.h first
 */
#if defined(acorn26) || \
    defined(__alpha__) || \
    defined(__amd64__) || \
    defined(__i386__) || \
    defined(__mips__) || \
    defined(news68k) || \
    defined(__sparc__) || \
    defined(sun2) || \
    defined(__vax__)
# define NCR5380_USE_BUS_SPACE
#endif

/*
 * Handy read/write macros
 */
#ifdef NCR5380_USE_BUS_SPACE
# include <sys/bus.h>
/* bus_space() variety */
# define NCR5380_READ(sc, reg)		bus_space_read_1(sc->sc_regt, \
					    sc->sc_regh, sc->reg)
# define NCR5380_WRITE(sc, reg, val)	bus_space_write_1(sc->sc_regt, \
					    sc->sc_regh, sc->reg, val)
#else
/* legacy memory-mapped variety */
# define NCR5380_READ(sc, reg)		(*sc->reg)
# define NCR5380_WRITE(sc, reg, val)	do { *(sc->reg) = val; } while (0)
#endif

#define SCI_CLR_INTR(sc)	NCR5380_READ(sc, sci_iack)
#define	SCI_BUSY(sc)		(NCR5380_READ(sc, sci_bus_csr) & SCI_BUS_BSY)

/* These are NOT artibtrary, but map to bits in sci_tcmd */
#define PHASE_DATA_OUT	0x0
#define PHASE_DATA_IN	0x1
#define PHASE_COMMAND	0x2
#define PHASE_STATUS	0x3
#define PHASE_UNSPEC1	0x4
#define PHASE_UNSPEC2	0x5
#define PHASE_MSG_OUT	0x6
#define PHASE_MSG_IN	0x7

/*
 * This illegal phase is used to prevent the 5380 from having
 * a phase-match condition when we don't want one, such as
 * when setting up the DMA engine or whatever...
 */
#define PHASE_INVALID	PHASE_UNSPEC1


/* Per-request state.  This is required in order to support reselection. */
struct sci_req {
	struct		scsipi_xfer *sr_xs;	/* Pointer to xfer struct, NULL=unused */
	int		sr_target, sr_lun;	/* For fast access */
	void		*sr_dma_hand;		/* Current DMA hnadle */
	uint8_t		*sr_dataptr;		/* Saved data pointer */
	int		sr_datalen;
	int		sr_flags;		/* Internal error code */
#define	SR_IMMED			1	/* Immediate command */
#define	SR_SENSE			2	/* We are getting sense */
#define	SR_OVERDUE			4	/* Timeout while not current */
#define	SR_ERROR			8	/* Error occurred */
	int		sr_status;		/* Status code from last cmd */
};
#define	SCI_OPENINGS	16		/* How many commands we can enqueue. */


struct ncr5380_softc {
	device_t		sc_dev;
	struct scsipi_adapter	sc_adapter;
	struct scsipi_channel	sc_channel;

#ifdef NCR5380_USE_BUS_SPACE
	/* Pointers to bus_space */
	bus_space_tag_t 	sc_regt;
	bus_space_handle_t 	sc_regh;

	/* Pointers to 5380 registers.  */
	bus_size_t	sci_r0;
	bus_size_t	sci_r1;
	bus_size_t	sci_r2;
	bus_size_t	sci_r3;
	bus_size_t	sci_r4;
	bus_size_t	sci_r5;
	bus_size_t	sci_r6;
	bus_size_t	sci_r7;
#else
	/* Pointers to 5380 registers.  See ncr5380reg.h */
	volatile uint8_t *sci_r0;
	volatile uint8_t *sci_r1;
	volatile uint8_t *sci_r2;
	volatile uint8_t *sci_r3;
	volatile uint8_t *sci_r4;
	volatile uint8_t *sci_r5;
	volatile uint8_t *sci_r6;
	volatile uint8_t *sci_r7;
#endif

	/* Functions set from MD code */
	int		(*sc_pio_out)(struct ncr5380_softc *,
					   int, int, uint8_t *);
	int		(*sc_pio_in)(struct ncr5380_softc *,
					  int, int, uint8_t *);
	void		(*sc_dma_alloc)(struct ncr5380_softc *);
	void		(*sc_dma_free)(struct ncr5380_softc *);

	void		(*sc_dma_setup)(struct ncr5380_softc *);
	void		(*sc_dma_start)(struct ncr5380_softc *);
	void		(*sc_dma_poll)(struct ncr5380_softc *);
	void		(*sc_dma_eop)(struct ncr5380_softc *);
	void		(*sc_dma_stop)(struct ncr5380_softc *);

	void		(*sc_intr_on)(struct ncr5380_softc *);
	void		(*sc_intr_off)(struct ncr5380_softc *);

	int		sc_flags;	/* Misc. flags and capabilities */
#define	NCR5380_FORCE_POLLING	1	/* Do not use interrupts. */

	/* Set bits in this to disable disconnect per-target. */
	int 	sc_no_disconnect;

	/* Set bits in this to disable parity for some target. */
	int		sc_parity_disable;

	int 	sc_min_dma_len;	/* Smaller than this is done with PIO */

	/* Begin MI shared data */

	int		sc_state;
#define	NCR_IDLE		   0	/* Ready for new work. */
#define NCR_WORKING 	0x01	/* Some command is in progress. */
#define	NCR_ABORTING	0x02	/* Bailing out */
#define NCR_DOINGDMA	0x04	/* The FIFO data path is active! */
#define NCR_DROP_MSGIN	0x10	/* Discard all msgs (parity err detected) */

	/* The request that has the bus now. */
	struct		sci_req *sc_current;

	/* Active data pointer for current SCSI command. */
	uint8_t		*sc_dataptr;
	int		sc_datalen;

	/* Begin MI private data */

	/* The number of operations in progress on the bus */
	volatile int	sc_ncmds;

	/* Ring buffer of pending/active requests */
	struct		sci_req sc_ring[SCI_OPENINGS];
	int		sc_rr;		/* Round-robin scan pointer */

	/* Active requests, by target/LUN */
	struct		sci_req *sc_matrix[8][8];

	/* Message stuff */
	int	sc_prevphase;

	u_int	sc_msgpriq;	/* Messages we want to send */
	u_int	sc_msgoutq;	/* Messages sent during last MESSAGE OUT */
	u_int	sc_msgout;	/* Message last transmitted */
#define SEND_DEV_RESET		0x01
#define SEND_PARITY_ERROR	0x02
#define SEND_ABORT		0x04
#define SEND_REJECT		0x08
#define SEND_INIT_DET_ERR	0x10
#define SEND_IDENTIFY  		0x20
#define SEND_SDTR		0x40
#define	SEND_WDTR		0x80
#define NCR_MAX_MSG_LEN 8
	uint8_t  sc_omess[NCR_MAX_MSG_LEN];
	uint8_t	*sc_omp;		/* Outgoing message pointer */
	uint8_t	sc_imess[NCR_MAX_MSG_LEN];
	uint8_t	*sc_imp;		/* Incoming message pointer */
	int	sc_rev;			/* Chip revision */
#define NCR_VARIANT_NCR5380	0
#define NCR_VARIANT_DP8490	1
#define NCR_VARIANT_NCR53C400	2
#define NCR_VARIANT_PAS16	3
#define NCR_VARIANT_CXD1180	4

};

void	ncr5380_attach(struct ncr5380_softc *);
int	ncr5380_detach(struct ncr5380_softc *, int);
int 	ncr5380_intr(void *);
void	ncr5380_scsipi_request(struct scsipi_channel *,
	    scsipi_adapter_req_t, void *);
int 	ncr5380_pio_in(struct ncr5380_softc *, int, int, uint8_t *);
int 	ncr5380_pio_out(struct ncr5380_softc *, int, int, uint8_t *);
void	ncr5380_init(struct ncr5380_softc *);

#ifdef	NCR5380_DEBUG
extern struct ncr5380_softc *ncr5380_debug_sc;
void ncr5380_trace(const char *msg, long val);
#define	NCR_TRACE(msg, val) ncr5380_trace(msg, val)
#else	/* NCR5380_DEBUG */
#define	NCR_TRACE(msg, val)	/* nada */
#endif	/* NCR5380_DEBUG */
