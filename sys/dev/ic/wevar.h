/*	$NetBSD: wevar.h,v 1.5 2009/05/12 14:25:18 cegger Exp $	*/

/*
 * National Semiconductor DS8390 NIC register definitions.
 *
 * Copyright (C) 1993, David Greenman.  This software may be used, modified,
 * copied, distributed, and sold, in both source and binary form provided that
 * the above copyright and these terms are retained.  Under no circumstances is
 * the author responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its use.
 */

struct we_softc {
	struct dp8390_softc sc_dp8390;

	bus_space_tag_t sc_asict;	/* space tag for ASIC */
	bus_space_handle_t sc_asich;	/* space handle for ASIC */

	u_int8_t sc_laar_proto;
	u_int8_t sc_msr_proto;

	u_int8_t sc_type;		/* our type */

	u_int8_t sc_flags;		/* attachment flags */
#define WE_16BIT_ENABLE		1	/* are we 16 bit? */
#define WE_16BIT_NOTOGGLE	2	/* avoid explicit 16bit reset/enable? */

	int sc_iobase;			/* i/o address */
	int sc_maddr;			/* physical i/o mem addr */

	void (*sc_init_hook)(struct we_softc *);

	void *sc_ih;			/* interrupt handle */
};

int we_config(device_t self, struct we_softc *, const char *);
