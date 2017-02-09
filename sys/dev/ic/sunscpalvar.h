/*	$NetBSD: sunscpalvar.h,v 1.9 2008/07/06 13:29:50 tsutsui Exp $	*/

/*
 * Copyright (c) 2001 Matthew Fredette
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
 * module and the machine-independent sunscpal.c module.
 */

/*
 * The sun2 and sparc use real bus_space.
 */
#if defined(sun2) || defined(__sparc__)
# define SUNSCPAL_USE_BUS_SPACE
#endif
/*
 * The sun2 and sparc use real bus_dma.
 */
#if defined(sun2) || defined(__sparc__)
#define SUNSCPAL_USE_BUS_DMA
#endif

/*
 * Handy read/write macros
 */
#ifdef SUNSCPAL_USE_BUS_SPACE
# include <sys/bus.h>
/* bus_space() variety */
#define SUNSCPAL_READ_1(sc, reg)	bus_space_read_1(sc->sunscpal_regt, \
						sc->sunscpal_regh, sc->reg)
#define SUNSCPAL_WRITE_1(sc, reg, val)	bus_space_write_1(sc->sunscpal_regt, \
						sc->sunscpal_regh, sc->reg, val)
#define SUNSCPAL_READ_2(sc, reg)	bus_space_read_2(sc->sunscpal_regt, \
						sc->sunscpal_regh, sc->reg)
#define SUNSCPAL_WRITE_2(sc, reg, val)	bus_space_write_2(sc->sunscpal_regt, \
						sc->sunscpal_regh, sc->reg, val)
#else
/* legacy memory-mapped variety */
# define SUNSCPAL_READ_1(sc, reg)	(*sc->reg)
# define SUNSCPAL_WRITE_1(sc, reg, val)	do { *(sc->reg) = val; } while(0)
# define SUNSCPAL_READ_2 SUNSCPAL_READ_1
# define SUNSCPAL_WRITE_2 SUNSCPAL_WRITE_1
#endif

#define SUNSCPAL_CLR_INTR(sc)	do { SUNSCPAL_READ_2(sc, sunscpal_dma_count); SUNSCPAL_READ_2(sc, sunscpal_icr); } while(0)
#define	SUNSCPAL_BUSY(sc)	(SUNSCPAL_READ_2(sc, sunscpal_icr) & SUNSCPAL_ICR_BUSY)

/* These are NOT artibtrary, but map to bits in sunscpal_icr */
#define SUNSCPAL_PHASE_DATA_IN	(SUNSCPAL_ICR_INPUT_OUTPUT)
#define SUNSCPAL_PHASE_DATA_OUT	(0)
#define SUNSCPAL_PHASE_COMMAND	(SUNSCPAL_ICR_COMMAND_DATA)
#define SUNSCPAL_PHASE_STATUS	(SUNSCPAL_ICR_COMMAND_DATA | \
				 SUNSCPAL_ICR_INPUT_OUTPUT)
#define SUNSCPAL_PHASE_MSG_IN	(SUNSCPAL_ICR_MESSAGE | \
				 SUNSCPAL_ICR_COMMAND_DATA | \
				 SUNSCPAL_ICR_INPUT_OUTPUT)
#define SUNSCPAL_PHASE_MSG_OUT	(SUNSCPAL_ICR_MESSAGE | \
				 SUNSCPAL_ICR_COMMAND_DATA)
#define SUNSCPAL_PHASE_UNSPEC1	(SUNSCPAL_ICR_MESSAGE | \
				 SUNSCPAL_ICR_INPUT_OUTPUT)
#define SUNSCPAL_PHASE_UNSPEC2	(SUNSCPAL_ICR_MESSAGE)
#define SUNSCPAL_BYTE_READ(sc, phase)	(((phase) & SUNSCPAL_ICR_COMMAND_DATA) ? \
					 SUNSCPAL_READ_1(sc, sunscpal_cmd_stat) : \
					 SUNSCPAL_READ_1(sc, sunscpal_data))
#define SUNSCPAL_BYTE_WRITE(sc, phase, b)	do { \
							if ((phase) & SUNSCPAL_ICR_COMMAND_DATA) { \
								SUNSCPAL_WRITE_1(sc, sunscpal_cmd_stat, b); \
							} else { \
								SUNSCPAL_WRITE_1(sc, sunscpal_data, b); \
							} \
						} while(0)
/*
 * A mask and a macro for getting the current bus phase.
 */
#define SUNSCPAL_ICR_PHASE_MASK	(SUNSCPAL_ICR_MESSAGE | \
				 SUNSCPAL_ICR_COMMAND_DATA | \
				 SUNSCPAL_ICR_INPUT_OUTPUT)
#define SUNSCPAL_BUS_PHASE(icr)	((icr) & SUNSCPAL_ICR_PHASE_MASK)

/*
 * This illegal phase is used to prevent the PAL from having
 * a phase-match condition when we don't want one, such as
 * when setting up the DMA engine or whatever...
 */
#define SUNSCPAL_PHASE_INVALID	SUNSCPAL_PHASE_UNSPEC1

/*
 * Transfers lager than 65535 bytes need to be split-up.
 * (The DMA count register is only 16 bits.)
 * Make the size an integer multiple of the page size
 * to avoid buf/cluster remap problems.  (paranoid?)
 */
#define	SUNSCPAL_MAX_DMA_LEN 0xE000

#ifdef SUNSCPAL_USE_BUS_DMA
/*
 * This structure is used to keep track of mapped DMA requests.
 */
struct sunscpal_dma_handle {
	int		dh_flags;
#define	SUNSCDH_BUSY	0x01		/* This DH is in use */
	uint8_t *	dh_mapaddr;	/* Original data pointer */
	int		dh_maplen;	/* Original data length */
	bus_dmamap_t	dh_dmamap;
#define	dh_dvma dh_dmamap->dm_segs[0].ds_addr /* VA of buffer in DVMA space */
};
typedef struct sunscpal_dma_handle *sunscpal_dma_handle_t;
#else
typedef void *sunscpal_dma_handle_t;
#endif

/* Per-request state.  This is required in order to support reselection. */
struct sunscpal_req {
	struct		scsipi_xfer *sr_xs;	/* Pointer to xfer struct, NULL=unused */
	int		sr_target, sr_lun;	/* For fast access */
	sunscpal_dma_handle_t sr_dma_hand;	/* Current DMA handle */
	uint8_t		*sr_dataptr;		/* Saved data pointer */
	int		sr_datalen;
	int		sr_flags;		/* Internal error code */
#define	SR_IMMED			1	/* Immediate command */
#define	SR_OVERDUE			2	/* Timeout while not current */
#define	SR_ERROR			4	/* Error occurred */
	int		sr_status;		/* Status code from last cmd */
};
#define	SUNSCPAL_OPENINGS	16		/* How many commands we can enqueue. */


struct sunscpal_softc {
	device_t		sc_dev;
	struct scsipi_adapter	sc_adapter;
	struct scsipi_channel	sc_channel;

#ifdef SUNSCPAL_USE_BUS_SPACE
	/* Pointers to bus_space */
	bus_space_tag_t 	sunscpal_regt;
	bus_space_handle_t 	sunscpal_regh;

	/* Pointers to PAL registers.  */
	bus_size_t	sunscpal_data;
	bus_size_t	sunscpal_cmd_stat;
	bus_size_t	sunscpal_icr;
	bus_size_t	sunscpal_dma_addr_h;
	bus_size_t	sunscpal_dma_addr_l;
	bus_size_t	sunscpal_dma_count;
	bus_size_t	sunscpal_intvec;
#else
	/* Pointers to PAL registers.  See sunscpalreg.h */
	volatile uint8_t	*sunscpal_data;
	volatile uint8_t	*sunscpal_cmd_stat;
	volatile uint16_t	*sunscpal_icr;
	volatile uint16_t	*sunscpal_dma_addr_h;
	volatile uint16_t	*sunscpal_dma_addr_l;
	volatile uint16_t	*sunscpal_dma_count;
	volatile uint8_t	*sunscpal_intvec;
#endif

	/* Pointers to DMA-related structures */
#ifdef	SUNSCPAL_USE_BUS_DMA
	bus_dma_tag_t	sunscpal_dmat;
#endif
	sunscpal_dma_handle_t	sc_dma_handles;

	/* Functions set from MD code */
#ifndef	SUNSCPAL_USE_BUS_DMA
	void		(*sc_dma_alloc)(struct sunscpal_softc *);
	void		(*sc_dma_free)(struct sunscpal_softc *);

	void		(*sc_dma_setup)(struct sunscpal_softc *);
#endif

	void		(*sc_intr_on)(struct sunscpal_softc *);
	void		(*sc_intr_off)(struct sunscpal_softc *);

	int		sc_flags;	/* Misc. flags and capabilities */
#define	SUNSCPAL_DISABLE_DMA	1	/* Do not use DMA. */

	/* Set bits in this to disable parity for some target. */
	int		sc_parity_disable;

	int 	sc_min_dma_len;	/* Smaller than this is done with PIO */

	/* Begin MI shared data */

	int		sc_state;
#define	SUNSCPAL_IDLE		   0	/* Ready for new work. */
#define SUNSCPAL_WORKING 	0x01	/* Some command is in progress. */
#define	SUNSCPAL_ABORTING	0x02	/* Bailing out */
#define SUNSCPAL_DOINGDMA	0x04	/* The FIFO data path is active! */
#define SUNSCPAL_DROP_MSGIN	0x10	/* Discard all msgs (parity err detected) */

	/* The request that has the bus now. */
	struct		sunscpal_req *sc_current;

	/* Active data pointer for current SCSI command. */
	uint8_t		*sc_dataptr;
	int		sc_datalen;
	int		sc_reqlen;

	/* Begin MI private data */

	/* The number of operations in progress on the bus */
	volatile int	sc_ncmds;

	/* Ring buffer of pending/active requests */
	struct		sunscpal_req sc_ring[SUNSCPAL_OPENINGS];
	int		sc_rr;		/* Round-robin scan pointer */

	/* Active requests, by target/LUN */
	struct		sunscpal_req *sc_matrix[8][8];

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
#define SUNSCPAL_MAX_MSG_LEN 8
	uint8_t	sc_omess[SUNSCPAL_MAX_MSG_LEN];
	uint8_t	*sc_omp;		/* Outgoing message pointer */
	uint8_t	sc_imess[SUNSCPAL_MAX_MSG_LEN];
	uint8_t	*sc_imp;		/* Incoming message pointer */
	int	sc_rev;			/* Chip revision */
#define SUNSCPAL_VARIANT_501_1006	0
#define SUNSCPAL_VARIANT_501_1045	1

};

void	sunscpal_attach(struct sunscpal_softc *, int);
int	sunscpal_detach(struct sunscpal_softc *, int);
int 	sunscpal_intr(void *);
void 	sunscpal_scsipi_request(struct scsipi_channel *,
		scsipi_adapter_req_t, void *);
int 	sunscpal_pio_in(struct sunscpal_softc *, int, int, uint8_t *);
int 	sunscpal_pio_out(struct sunscpal_softc *, int, int, uint8_t *);
void	sunscpal_init(struct sunscpal_softc *);

/* Options for no-parity, DMA, and interrupts. */
#define SUNSCPAL_OPT_NO_PARITY_CHK  0xff
#define SUNSCPAL_OPT_FORCE_POLLING 0x100
#define SUNSCPAL_OPT_DISABLE_DMA   0x200

#ifdef	SUNSCPAL_DEBUG
struct sunscpal_softc *sunscpal_debug_sc;
void sunscpal_trace(char *msg, long val);
#define	SUNSCPAL_TRACE(msg, val) sunscpal_trace(msg, val)
#else	/* SUNSCPAL_DEBUG */
#define	SUNSCPAL_TRACE(msg, val)	/* nada */
#endif	/* SUNSCPAL_DEBUG */
