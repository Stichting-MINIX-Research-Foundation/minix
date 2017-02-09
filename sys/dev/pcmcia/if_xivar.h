/*	$NetBSD: if_xivar.h,v 1.9 2015/04/13 16:33:25 riastradh Exp $	*/

/*
 * Copyright (c) 2004 Charles M. Hannum.  All rights reserved.
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
 *      This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 */

#include <sys/rndsource.h>

struct xi_softc {
	device_t sc_dev;			/* Generic device info */
	struct ethercom sc_ethercom;		/* Ethernet common part */

	struct mii_data sc_mii;			/* MII media information */

	bus_space_tag_t sc_bst;			/* Bus cookie */
	bus_space_handle_t sc_bsh;		/* Bus I/O handle */

        /* Power management hooks and state. */
	int	(*sc_enable)(struct xi_softc *);
	void	(*sc_disable)(struct xi_softc *);
	int	sc_enabled;

	int		sc_chipset;		/* Chipset type */
#define	XI_CHIPSET_SCIPPER	0
#define	XI_CHIPSET_MOHAWK	1
#define	XI_CHIPSET_DINGO	2
	u_int8_t	sc_rev;			/* Chip revision */

	krndsource_t	sc_rnd_source;
};

void	xi_attach(struct xi_softc *, u_int8_t *);
int	xi_detach(device_t, int);
int	xi_activate(device_t, enum devact);
int	xi_intr(void *);
