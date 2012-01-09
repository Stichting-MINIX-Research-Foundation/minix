/*	$NetBSD: 3c590.c,v 1.15 2008/12/14 18:46:33 christos Exp $	*/

/* stripped down from freebsd:sys/i386/netboot/3c509.c */


/**************************************************************************
NETBOOT -  BOOTP/TFTP Bootstrap Program

Author: Martin Renters.
  Date: Mar 22 1995

 This code is based heavily on David Greenman's if_ed.c driver and
  Andres Vega Garcia's if_ep.c driver.

 Copyright (C) 1993-1994, David Greenman, Martin Renters.
 Copyright (C) 1993-1995, Andres Vega Garcia.
 Copyright (C) 1995, Serge Babkin.
  This software may be used, modified, copied, distributed, and sold, in
  both source and binary form provided that the above copyright and these
  terms are retained. Under no circumstances are the authors responsible for
  the proper functioning of this software, nor do the authors assume any
  responsibility for damages incurred with its use.

3c509 support added by Serge Babkin (babkin@hq.icb.chel.su)

3c509.c,v 1.2 1995/05/30 07:58:52 rgrimes Exp

***************************************************************************/

#include <sys/types.h>
#include <machine/pio.h>

#include <lib/libsa/stand.h>

#include <libi386.h>
#include <pcivar.h>

#if defined(_STANDALONE) && !defined(SUPPORT_NO_NETBSD)
#include <lib/libkern/libkern.h>
#include <bootinfo.h>
#endif

#include "etherdrv.h"
#include "3c509.h"

#define EP_W3_INTERNAL_CONFIG	0x00	/* 32 bits */
#define EP_W3_RESET_OPTIONS	0x08	/* 16 bits */

unsigned ether_medium;
unsigned short eth_base;

extern void epreset(void);
extern int ep_get_e(int);

u_char eth_myaddr[6];

static struct mtabentry {
    int address_cfg; /* configured connector */
    int config_bit; /* connector present */
    char *name;
} mediatab[] = { /* indexed by media type - etherdrv.h */
    {3, 0x10, "BNC"},
    {0, 0x08, "UTP"},
    {1, 0x20, "AUI"},
    {6, 0x40, "MII"},
};

#if defined(_STANDALONE) && !defined(SUPPORT_NO_NETBSD)
static struct btinfo_netif bi_netif;
#endif

/**************************************************************************
ETH_PROBE - Look for an adapter
***************************************************************************/
int
EtherInit(unsigned char *myadr)
{
	/* common variables */
	int i, j;
	/* variables for 3C509 */
	u_short *p;
	struct mtabentry *m;

	/*********************************************************
			Search for 3Com 590 card
	***********************************************************/

	pcihdl_t hdl;
	int iobase;

	if (pcicheck() == -1) {
		printf("cannot access PCI\n");
		return 0;
	}

	if (pcifinddev(0x10b7, 0x5900, &hdl) &&
	    pcifinddev(0x10b7, 0x5950, &hdl) &&
	    pcifinddev(0x10b7, 0x9000, &hdl) &&
	    pcifinddev(0x10b7, 0x9001, &hdl) &&
	    pcifinddev(0x10b7, 0x9050, &hdl)) {
		printf("cannot find 3c59x / 3c90x\n");
		return 0;
	}

	if (pcicfgread(&hdl, 0x10, &iobase) || !(iobase & 1)) {
		printf("cannot map IO space\n");
		return 0;
	}
	eth_base = iobase & 0xfffffffc;

	/* test for presence of connectors */
	GO_WINDOW(3);
	i = inb(IS_BASE + EP_W3_RESET_OPTIONS);
	j = (inw(IS_BASE + EP_W3_INTERNAL_CONFIG + 2) >> 4) & 7;

	GO_WINDOW(0);

	for (ether_medium = 0, m = mediatab;
	     ether_medium < sizeof(mediatab) / sizeof(mediatab[0]);
	     ether_medium++, m++) {
		if (j == m->address_cfg) {
			if (!(i & m->config_bit)) {
				printf("%s not present\n", m->name);
				return 0;
			}
			printf("using %s\n", m->name);
			goto ok;
		}
	}
	printf("unknown connector\n");
	return 0;

 ok:
	/*
	 * Read the station address from the eeprom
	 */
	p = (u_short *) eth_myaddr;
	for (i = 0; i < 3; i++) {
		u_short help;
		GO_WINDOW(0);
		help = ep_get_e(i);
		p[i] = ((help & 0xff) << 8) | ((help & 0xff00) >> 8);
		GO_WINDOW(2);
		outw(BASE + EP_W2_ADDR_0 + (i * 2), help);
	}
	for (i = 0; i < 6; i++)
		myadr[i] = eth_myaddr[i];

	epreset();


#if defined(_STANDALONE) && !defined(SUPPORT_NO_NETBSD)
	strncpy(bi_netif.ifname, "ep", sizeof(bi_netif.ifname));
	bi_netif.bus = BI_BUS_PCI;
	bi_netif.addr.tag = hdl;

	BI_ADD(&bi_netif, BTINFO_NETIF, sizeof(bi_netif));
#endif

	return 1;
}
