/*	$NetBSD: twevar.h,v 1.30 2012/10/27 17:18:35 chs Exp $	*/

/*-
 * Copyright (c) 2000, 2001, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

#ifndef _PCI_TWEVAR_H_
#define	_PCI_TWEVAR_H_

#define	TWE_MAX_QUEUECNT	129

/* Callbacks from controller to array. */
struct twe_callbacks {
	void	(*tcb_openings)(device_t, int);
};

/* Per-array drive information. */
struct twe_drive {
	uint32_t		td_size;
	uint8_t			td_type;
	uint8_t			td_stripe;

	device_t td_dev;
	const struct twe_callbacks *td_callbacks;
};

/* Per-controller state. */
struct twe_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_dma_tag_t		sc_dmat;
	bus_dmamap_t		sc_dmamap;
	void			*sc_ih;
	void *			sc_cmds;
	bus_addr_t		sc_cmds_paddr;
	int			sc_nccbs;
	struct twe_ccb		*sc_ccbs;
	SIMPLEQ_HEAD(, twe_ccb)	sc_ccb_queue;
	SLIST_HEAD(, twe_ccb)	sc_ccb_freelist;
	int			sc_flags;
	int			sc_openings;
	int			sc_nunits;
	struct twe_drive	sc_units[TWE_MAX_UNITS];

	/* Asynchronous event notification queue for management tools. */
#define	TWE_AEN_Q_LENGTH	256
	uint16_t		sc_aen_queue[TWE_AEN_Q_LENGTH];
	int			sc_aen_head;
	int			sc_aen_tail;
};
#define	TWEF_OPEN	0x01	/* control device is opened */
#define	TWEF_AENQ_WAIT	0x02	/* someone waiting for AENs */
#define	TWEF_AEN	0x04	/* AEN fetch in progress */
#define	TWEF_WAIT_CCB	0x08	/* someone waiting for a CCB */

/* Optional per-command context. */
struct twe_context {
	void	(*tx_handler)(struct twe_ccb *, int);
	void 	*tx_context;
	device_t tx_dv;
};

/* Command control block. */
struct twe_ccb {
	union {
		SIMPLEQ_ENTRY(twe_ccb) simpleq;
		SLIST_ENTRY(twe_ccb) slist;
	} ccb_chain;
	struct twe_cmd	*ccb_cmd;
	int		ccb_cmdid;
	int		ccb_flags;
	void		*ccb_data;
	int		ccb_datasize;
	vaddr_t		ccb_abuf;
	bus_dmamap_t	ccb_dmamap_xfer;
	struct twe_context ccb_tx;
};
#define	TWE_CCB_DATA_IN		0x01	/* Map describes inbound xfer */
#define	TWE_CCB_DATA_OUT	0x02	/* Map describes outbound xfer */
#define	TWE_CCB_COMPLETE	0x04	/* Command completed */
#define	TWE_CCB_ACTIVE		0x08	/* Command active */
#define	TWE_CCB_AEN		0x10	/* For AEN retrieval */
#define	TWE_CCB_ALLOCED		0x20	/* CCB allocated */

struct twe_attach_args {
	int		twea_unit;
};

struct twe_ccb *twe_ccb_alloc(struct twe_softc *, int);
struct twe_ccb *twe_ccb_alloc_wait(struct twe_softc *, int);
void	twe_ccb_enqueue(struct twe_softc *sc, struct twe_ccb *ccb);
void	twe_ccb_free(struct twe_softc *sc, struct twe_ccb *);
int	twe_ccb_map(struct twe_softc *, struct twe_ccb *);
int	twe_ccb_poll(struct twe_softc *, struct twe_ccb *, int);
int	twe_ccb_submit(struct twe_softc *, struct twe_ccb *);
void	twe_ccb_unmap(struct twe_softc *, struct twe_ccb *);

void	twe_ccb_wait_handler(struct twe_ccb *, int);

int	twe_param_get(struct twe_softc *, int, int, size_t,
	    void (*)(struct twe_ccb *, int), struct twe_param **);
int	twe_param_get_1(struct twe_softc *, int, int, uint8_t *);
int	twe_param_get_2(struct twe_softc *, int, int, uint16_t *);
int	twe_param_get_4(struct twe_softc *, int, int, uint32_t *);

void	twe_register_callbacks(struct twe_softc *, int,
	    const struct twe_callbacks *);

static __inline size_t twe_get_maxsegs(void) {
	size_t max_segs = ((MAXPHYS + PAGE_SIZE - 1) / PAGE_SIZE) + 1;
#ifdef TWE_SG_SIZE
	if (TWE_SG_SIZE < max_segs)
	    max_segs = TWE_SG_SIZE;
#endif
	return max_segs;
}

static __inline size_t twe_get_maxxfer(size_t maxsegs) {
	return (maxsegs - 1) * PAGE_SIZE;
}

/*
 * Structures used to convert numeric codes to strings.
 */
struct twe_code_table {
	uint32_t	code;
	const char	*string;
};
extern const struct twe_code_table twe_table_status[];
extern const struct twe_code_table twe_table_unitstate[];
extern const struct twe_code_table twe_table_unittype[];
extern const struct twe_code_table twe_table_stripedepth[];
extern const struct twe_code_table twe_table_aen[];

const char *twe_describe_code(const struct twe_code_table *, uint32_t);

#endif	/* !_PCI_TWEVAR_H_ */
