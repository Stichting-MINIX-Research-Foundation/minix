/*	$NetBSD: if_ne_pcmcia.c,v 1.160 2013/10/17 21:06:47 christos Exp $	*/

/*
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ne_pcmcia.c,v 1.160 2013/10/17 21:06:47 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/select.h>
#include <sys/device.h>
#include <sys/socket.h>

#include <net/if_types.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/ic/dp8390reg.h>
#include <dev/ic/dp8390var.h>

#include <dev/ic/ne2000reg.h>
#include <dev/ic/ne2000var.h>

#include <dev/ic/rtl80x9reg.h>
#include <dev/ic/rtl80x9var.h>

#include <dev/ic/dl10019var.h>

#include <dev/ic/ax88190reg.h>
#include <dev/ic/ax88190var.h>

int	ne_pcmcia_match(device_t, cfdata_t , void *);
int	ne_pcmcia_validate_config(struct pcmcia_config_entry *);
void	ne_pcmcia_attach(device_t, device_t, void *);
int	ne_pcmcia_detach(device_t, int);

int	ne_pcmcia_enable(struct dp8390_softc *);
void	ne_pcmcia_disable(struct dp8390_softc *);

struct ne_pcmcia_softc {
	struct ne2000_softc sc_ne2000;		/* real "ne2000" softc */

	void *sc_ih;				/* interrupt handle */

	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	int sc_state;
#define	NE_PCMCIA_ATTACHED	3
};

u_int8_t *ne_pcmcia_get_enaddr(struct ne_pcmcia_softc *, int,
	    u_int8_t [ETHER_ADDR_LEN]);
u_int8_t *ne_pcmcia_dl10019_get_enaddr(struct ne_pcmcia_softc *,
	    u_int8_t [ETHER_ADDR_LEN]);

CFATTACH_DECL_NEW(ne_pcmcia, sizeof(struct ne_pcmcia_softc),
    ne_pcmcia_match, ne_pcmcia_attach, ne_pcmcia_detach, dp8390_activate);

static const struct ne2000dev {
    int32_t manufacturer;
    int32_t product;
    const char *cis_info[4];
    int function;
    int enet_maddr;
    unsigned char enet_vendor[3];
    int flags;
#define	NE2000DVF_DL10019	0x0001		/* chip is D-Link DL10019 */
#define	NE2000DVF_AX88190	0x0002		/* chip is ASIX AX88190 */
} ne2000devs[] = {
    { PCMCIA_VENDOR_EDIMAX, PCMCIA_PRODUCT_EDIMAX_EP4000A,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0xa0, 0x0c }, 0 },

    { PCMCIA_VENDOR_EDIMAX, PCMCIA_PRODUCT_EDIMAX_EP4101,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0x90, 0xcc }, NE2000DVF_AX88190 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_SYNERGY21_S21810,
      0, -1, { 0x00, 0x48, 0x54 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_AMBICOM_AMB8002T,
      0, -1, { 0x00, 0x10, 0x7a }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_AMBICOM_AMB8110,
      0, -1, { 0x00, 0x10, 0x7a }, NE2000DVF_AX88190 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_PREMAX_PE200,
      0, 0x07f0, { 0x00, 0x20, 0xe0 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_PREMAX_PE200,
      0, -1, { 0x00, 0x20, 0xe0 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_PLANET_SMARTCOM2000,
      0, 0xff0, { 0x00, 0x00, 0xe8 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_DLINK_DE660,
      0, -1, { 0x00, 0x80, 0xc8 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_DLINK_DE660PLUS,
      0, -1, { 0x00, 0x80, 0xc8 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_RPTI_EP400,
      0, 0x110, { 0x00, 0x40, 0x95 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_RPTI_EP401,
      0, -1, { 0x00, 0x40, 0x95 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_ACCTON_EN2212,
      0, 0x0ff0, { 0x00, 0x00, 0xe8 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_ACCTON_EN2216,
      0, -1, { 0x00, 0x00, 0xe8 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_SVEC_COMBOCARD,
      0, -1, { 0x00, 0xe0, 0x98 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_SVEC_LANCARD,
      0, 0x7f0, { 0x00, 0xc0, 0x6c }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_EPSON_EEN10B,
      PCMCIA_CIS_EPSON_EEN10B,
      0, 0xff0, { 0x00, 0x00, 0x48 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_TAMARACK_ETHERNET,
      0, -1, { 0x00, 0x00, 0x00 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_CNET_NE2000,
      0, -1, { 0x00, 0x80, 0xad }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_GENIUS_ME3000II,
      0, -1, { 0x00, 0x40, 0x95 }, 0 },


    /*
     * You have to add new entries which contains
     * PCMCIA_VENDOR_INVALID and/or PCMCIA_PRODUCT_INVALID
     * in front of this comment.
     *
     * There are cards which use a generic vendor and product id but needs
     * a different handling depending on the cis_info, so ne2000_match
     * needs a table where the exceptions comes first and then the normal
     * product and vendor entries.
     */

    { PCMCIA_VENDOR_IBM, PCMCIA_PRODUCT_IBM_INFOMOVER,
      PCMCIA_CIS_INVALID,
      0, 0x0ff0, { 0xff, 0xff, 0xff }, 0 },

    { PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_ECARD_1,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0x80, 0xc8 }, 0 },

    { PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_COMBO_ECARD,
      PCMCIA_CIS_INVALID,
      0, -1, { 0xff, 0xff, 0xff }, NE2000DVF_DL10019 },

    { PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_COMBO_ECARD,
      PCMCIA_CIS_INVALID,
      0, -1, { 0xff, 0xff, 0xff }, NE2000DVF_AX88190 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_LANTECH_FASTNETTX,
      0, -1, { 0x00, 0x04, 0x1c }, NE2000DVF_AX88190 },

    { PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_ETHERFAST,
      PCMCIA_CIS_INVALID,
      0, -1, { 0xff, 0xff, 0xff }, NE2000DVF_DL10019 },

    { PCMCIA_VENDOR_NETGEAR, PCMCIA_PRODUCT_NETGEAR_FA410TXC,
      PCMCIA_CIS_INVALID,
      0, -1, { 0xff, 0xff, 0xff }, NE2000DVF_DL10019 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_DLINK_DFE670TXD,
      0, -1, { 0xff, 0xff, 0xff }, NE2000DVF_DL10019 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_MELCO_LPC2_TX,
      0, -1, { 0x00, 0x40, 0x26 }, NE2000DVF_DL10019 },

    { PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_COMBO_ECARD,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0x80, 0xc8 }, 0 },

    { PCMCIA_VENDOR_LINKSYS, PCMCIA_PRODUCT_LINKSYS_TRUST_COMBO_ECARD,
      PCMCIA_CIS_INVALID,
      0, 0x0120, { 0x20, 0x04, 0x49 }, 0 },

    /* Although the comments above say to put VENDOR/PRODUCT INVALID IDs
       above this list, we need to keep this one below the ECARD_1, or else
       both will match the same more-generic entry rather than the more
       specific one above with proper vendor and product IDs. */
    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_LINKSYS_ECARD_2,
      0, -1, { 0x00, 0x80, 0xc8 }, 0 },

    /*
     * D-Link DE-650 has many minor versions:
     *
     *   CIS information          Manufacturer Product  Note
     * 1 "D-Link, DE-650"             INVALID  INVALID  white card
     * 2 "D-Link, DE-650, Ver 01.00"  INVALID  INVALID  became bare metal
     * 3 "D-Link, DE-650, Ver 01.00"   0x149    0x265   minor change in look
     * 4 "D-Link, DE-650, Ver 01.00"   0x149    0x265   collision LED added
     *
     * While the 1st and the 2nd types should use the "D-Link DE-650" entry,
     * the 3rd and the 4th types should use the "Linksys EtherCard" entry.
     * Therefore, this enty must be below the LINKSYS_ECARD_1.  --itohy
     */
    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_DLINK_DE650,
      0, 0x0040, { 0x00, 0x80, 0xc8 }, 0 },

    /*
     * IO-DATA PCLA/TE and later version of PCLA/T has valid
     * vendor/product ID and it is possible to read MAC address
     * using standard I/O ports.  It also read from CIS offset 0x01c0.
     * On the other hand, earlier version of PCLA/T doesn't have valid
     * vendor/product ID and MAC address must be read from CIS offset
     * 0x0ff0 (i.e., usual ne2000 way to read it doesn't work).
     * And CIS information of earlier and later version of PCLA/T are
     * same except fourth element.  So, for now, we place the entry for
     * PCLA/TE (and later version of PCLA/T) followed by entry
     * for the earlier version of PCLA/T (or, modify to match all CIS
     * information and have three or more individual entries).
     */
    { PCMCIA_VENDOR_IODATA, PCMCIA_PRODUCT_IODATA_PCLATE,
      PCMCIA_CIS_INVALID,
      0, -1, { 0xff, 0xff, 0xff }, 0 },

    { PCMCIA_VENDOR_IODATA3, PCMCIA_PRODUCT_IODATA3_PCETTXR,
      PCMCIA_CIS_IODATA3_PCETTXR,
      0, -1, { 0x00, 0xa0, 0xb0 }, NE2000DVF_DL10019 },

    /*
     * This entry should be placed after above PCLA-TE entry.
     * See above comments for detail.
     */
    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_IODATA_PCLAT,
      0, 0x0ff0, { 0x00, 0xa0, 0xb0 }, 0 },

    { PCMCIA_VENDOR_DAYNA, PCMCIA_PRODUCT_DAYNA_COMMUNICARD_E_1,
      PCMCIA_CIS_INVALID,
      0, 0x0110, { 0x00, 0x80, 0x19 }, 0 },

    { PCMCIA_VENDOR_DAYNA, PCMCIA_PRODUCT_DAYNA_COMMUNICARD_E_2,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0x80, 0x19 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_COREGA_ETHER_CF_TD,
      0, -1, { 0x00, 0x00, 0xf4 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_COREGA_ETHER_PCC_T,
      0, -1, { 0x00, 0x00, 0xf4 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_COREGA_ETHER_PCC_TD,
      0, -1, { 0x00, 0x00, 0xf4 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_COREGA_ETHER_PCC_TL,
      0, -1, { 0x00, 0x00, 0xf4 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_COREGA_ETHER_II_PCC_T,
      0, -1, { 0x00, 0x00, 0xf4 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_COREGA_ETHER_II_PCC_TD,
      0, -1, { 0x00, 0x00, 0xf4 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_COREGA_FAST_ETHER_PCC_TX,
      0, -1, { 0x00, 0x00, 0xf4 }, NE2000DVF_DL10019 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_COREGA_FETHER_PCC_TXF,
      0, -1, { 0x00, 0x90, 0x99 }, NE2000DVF_DL10019 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_COREGA_FETHER_PCC_TXD,
      0, -1, { 0x00, 0x90, 0x99 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_COREGA_FETHER_II_PCC_TXD,
      0, -1, { 0x00, 0x90, 0x99 }, NE2000DVF_AX88190 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_COREGA_LAPCCTXD,
      0, -1, { 0x00, 0x90, 0x99 }, 0 },

    { PCMCIA_VENDOR_COMPEX, PCMCIA_PRODUCT_COMPEX_LINKPORT_ENET_B,
      PCMCIA_CIS_INVALID,
      0, 0x01c0, { 0xff, 0xff, 0xff }, 0 },

    { PCMCIA_VENDOR_SMC, PCMCIA_PRODUCT_SMC_EZCARD,
      PCMCIA_CIS_INVALID,
      0, 0x01c0, { 0x00, 0xe0, 0x29 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_SMC_8041,
      0, -1, { 0x00, 0x04, 0xe2 }, 0 },

    { PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_EA_ETHER,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0xc0, 0x1b }, 0 },

    { PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_LP_ETHER_CF,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0xc0, 0x1b }, 0 },

    { PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_LP_ETH_10_100_CF,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0xe0, 0x98 }, NE2000DVF_DL10019 },

    { PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_LP_ETHER,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0xc0, 0x1b }, 0 },

    { PCMCIA_VENDOR_KINGSTON, PCMCIA_PRODUCT_KINGSTON_KNE2,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0xc0, 0xf0 }, 0 },

    { PCMCIA_VENDOR_XIRCOM, PCMCIA_PRODUCT_XIRCOM_CFE_10,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0x10, 0xa4 }, 0 },

    { PCMCIA_VENDOR_MELCO, PCMCIA_PRODUCT_MELCO_LPC3_TX,
      PCMCIA_CIS_INVALID,
      0, -1, { 0xff, 0xff, 0xff }, NE2000DVF_AX88190 },

    { PCMCIA_VENDOR_BUFFALO, PCMCIA_PRODUCT_BUFFALO_LPC_CF_CLT,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0x07, 0x40 }, 0 },

    { PCMCIA_VENDOR_BUFFALO, PCMCIA_PRODUCT_BUFFALO_LPC3_CLT,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0x07, 0x40 }, 0 },

    { PCMCIA_VENDOR_BUFFALO, PCMCIA_PRODUCT_BUFFALO_LPC4_CLX,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0x40, 0xfa }, NE2000DVF_AX88190 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_BILLIONTON_LNT10TN,
      0, -1, { 0x00, 0x00, 0x00 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_BILLIONTON_CFLT10N,
      0, -1, { 0x00, 0x00, 0x00 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_NDC_ND5100_E,
      0, -1, { 0x00, 0x80, 0xc6 }, 0 },

    { PCMCIA_VENDOR_TELECOMDEVICE, PCMCIA_PRODUCT_TELECOMDEVICE_TCD_HPC100,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0x40, 0x26 }, NE2000DVF_AX88190 },

    { PCMCIA_VENDOR_MACNICA, PCMCIA_PRODUCT_MACNICA_ME1_JEIDA,
      PCMCIA_CIS_INVALID,
      0, 0x00b8, { 0x08, 0x00, 0x42 }, 0 },

    { PCMCIA_VENDOR_NETGEAR, PCMCIA_PRODUCT_NETGEAR_FA411,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0x40, 0xf4 }, 0 },

    { PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
      PCMCIA_CIS_DYNALINK_L10C,
      0, -1, { 0x00, 0x00, 0x00 }, 0 },

    { PCMCIA_VENDOR_ALLIEDTELESIS, PCMCIA_PRODUCT_ALLIEDTELESIS_LA_PCM,
      PCMCIA_CIS_INVALID,
      0, 0x0ff0, { 0x00, 0x00, 0xf4 }, 0 },

    { PCMCIA_VENDOR_NEXTCOM, PCMCIA_PRODUCT_NEXTCOM_NEXTHAWK,
      PCMCIA_CIS_INVALID,
      0, -1, { 0x00, 0x40, 0xb4 }, 0 },

    { PCMCIA_VENDOR_BELKIN, PCMCIA_PRODUCT_BELKIN_F5D5020,
      PCMCIA_CIS_BELKIN_F5D5020,
      0, -1, { 0x00, 0x30, 0xbd } , NE2000DVF_AX88190 },

#if 0
    /* the rest of these are stolen from the linux pcnet pcmcia device
       driver.  Since I don't know the manfid or cis info strings for
       any of them, they're not compiled in until I do. */
    { "APEX MultiCard",
      0x0000, 0x0000, NULL, NULL, 0,
      0x03f4, { 0x00, 0x20, 0xe5 }, 0 },
    { "ASANTE FriendlyNet",
      0x0000, 0x0000, NULL, NULL, 0,
      0x4910, { 0x00, 0x00, 0x94 }, 0 },
    { "Danpex EN-6200P2",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0110, { 0x00, 0x40, 0xc7 }, 0 },
    { "DataTrek NetCard",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x20, 0xe8 }, 0 },
    { "EP-210 Ethernet",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0110, { 0x00, 0x40, 0x33 }, 0 },
    { "ELECOM Laneed LD-CDWA",
      0x0000, 0x0000, NULL, NULL, 0,
      0x00b8, { 0x08, 0x00, 0x42 }, 0 },
    { "Grey Cell GCS2220",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0000, { 0x00, 0x47, 0x43 }, 0 },
    { "Hypertec Ethernet",
      0x0000, 0x0000, NULL, NULL, 0,
      0x01c0, { 0x00, 0x40, 0x4c }, 0 },
    { "IBM CCAE",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x08, 0x00, 0x5a }, 0 },
    { "IBM CCAE",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x04, 0xac }, 0 },
    { "IBM CCAE",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x06, 0x29 }, 0 },
    { "IBM FME",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0374, { 0x00, 0x04, 0xac }, 0 },
    { "IBM FME",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0374, { 0x08, 0x00, 0x5a }, 0 },
    { "Katron PE-520",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0110, { 0x00, 0x40, 0xf6 }, 0 },
    { "Kingston KNE-PCM/x",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0xc0, 0xf0 }, 0 },
    { "Kingston KNE-PCM/x",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0xe2, 0x0c, 0x0f }, 0 },
    { "Longshine LCS-8534",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0000, { 0x08, 0x00, 0x00 }, 0 },
    { "Maxtech PCN2000",
      0x0000, 0x0000, NULL, NULL, 0,
      0x5000, { 0x00, 0x00, 0xe8 }, 0 },
    { "NDC Instant-Link",
      0x0000, 0x0000, NULL, NULL, 0,
      0x003a, { 0x00, 0x80, 0xc6 }, 0 },
    { "NE2000 Compatible",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0xa0, 0x0c }, 0 },
    { "Network General Sniffer",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x00, 0x65 }, 0 },
    { "Panasonic VEL211",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x80, 0x45 }, 0 },
    { "SCM Ethernet",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0ff0, { 0x00, 0x20, 0xcb }, 0 },
    { "Volktek NPL-402CT",
      0x0000, 0x0000, NULL, NULL, 0,
      0x0060, { 0x00, 0x40, 0x05 }, 0 },
#endif
};

#define	NE2000_NDEVS	(sizeof(ne2000devs) / sizeof(ne2000devs[0]))

static const struct ne2000dev *
ne2000_match(struct pcmcia_card *card, int fct, int n)
{
	size_t i;

	/*
	 * See if it matches by manufacturer & product.
	 */
	if (card->manufacturer == ne2000devs[n].manufacturer &&
	    card->manufacturer != PCMCIA_VENDOR_INVALID &&
	    card->product == ne2000devs[n].product &&
	    card->product != PCMCIA_PRODUCT_INVALID)
		goto match;

	/*
	 * Otherwise, try to match by CIS strings.
	 */
	for (i = 0; i < 2; i++)
		if (card->cis1_info[i] == NULL ||
		    ne2000devs[n].cis_info[i] == NULL ||
		    strcmp(card->cis1_info[i], ne2000devs[n].cis_info[i]) != 0)
			return (NULL);

match:
	/*
	 * Finally, see if function number matches.
	 */
	return (fct == ne2000devs[n].function ? &ne2000devs[n] : NULL);
}


int
ne_pcmcia_match(device_t parent, cfdata_t match, void *aux)
{
	struct pcmcia_attach_args *pa = aux;
	int i;

	for (i = 0; i < NE2000_NDEVS; i++) {
		if (ne2000_match(pa->card, pa->pf->number, i))
			return (1);
	}

	return (0);
}

int
ne_pcmcia_validate_config(struct pcmcia_config_entry *cfe)
{
	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_iospace < 1 || cfe->num_iospace > 2)
		return (EINVAL);
	/* Some cards have a memory space, but we don't use it. */
	cfe->num_memspace = 0;
	return (0);
}

void
ne_pcmcia_attach(device_t parent, device_t self, void *aux)
{
	struct ne_pcmcia_softc *psc = device_private(self);
	struct ne2000_softc *nsc = &psc->sc_ne2000;
	struct dp8390_softc *dsc = &nsc->sc_dp8390;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	const struct ne2000dev *ne_dev;
	int i;
	u_int8_t myea[6], *enaddr;
	int error;

	aprint_naive("\n");

	dsc->sc_dev = self;
	psc->sc_pf = pa->pf;

	error = pcmcia_function_configure(pa->pf, ne_pcmcia_validate_config);
	if (error) {
		aprint_error_dev(self, "configure failed, error=%d\n", error);
		return;
	}

	cfe = pa->pf->cfe;
	dsc->sc_regt = cfe->iospace[0].handle.iot;
	dsc->sc_regh = cfe->iospace[0].handle.ioh;

	if (cfe->num_iospace == 1) {
		nsc->sc_asict = dsc->sc_regt;
		if (bus_space_subregion(dsc->sc_regt, dsc->sc_regh,
		    NE2000_ASIC_OFFSET, NE2000_ASIC_NPORTS, &nsc->sc_asich)) {
			aprint_error_dev(self,
			    "can't get subregion for asic\n");
			goto fail;
		}
	} else {
		nsc->sc_asict = cfe->iospace[1].handle.iot;
		nsc->sc_asich = cfe->iospace[1].handle.ioh;
	}

	error = ne_pcmcia_enable(dsc);
	if (error)
		goto fail;

	/* Set up power management hooks. */
	dsc->sc_enable = ne_pcmcia_enable;
	dsc->sc_disable = ne_pcmcia_disable;

	/*
	 * Read the station address from the board.
	 */
	i = 0;
again:
	enaddr = NULL;			/* Ask ASIC by default */
	for (; i < NE2000_NDEVS; i++) {
		ne_dev = ne2000_match(pa->card, pa->pf->number, i);
		if (ne_dev != NULL) {
			if (ne_dev->enet_maddr >= 0) {
				enaddr = ne_pcmcia_get_enaddr(psc,
				    ne_dev->enet_maddr, myea);
				if (enaddr == NULL)
					continue;
			}
			goto found;
		}
	}
	aprint_error_dev(self, "can't match ethernet vendor code\n");
	if (enaddr != NULL)
		aprint_error_dev(self,
		    "ethernet vendor code %02x:%02x:%02x\n",
		    enaddr[0], enaddr[1], enaddr[2]);
	goto fail2;

found:
	if ((ne_dev->flags & NE2000DVF_DL10019) != 0) {
		u_int8_t type;

		enaddr = ne_pcmcia_dl10019_get_enaddr(psc, myea);
		if (enaddr == NULL) {
			++i;
			goto again;
		}

		dsc->sc_mediachange = dl10019_mediachange;
		dsc->sc_mediastatus = dl10019_mediastatus;
		dsc->init_card = dl10019_init_card;
		dsc->stop_card = dl10019_stop_card;
		dsc->sc_media_init = dl10019_media_init;
		dsc->sc_media_fini = dl10019_media_fini;

		/* Determine if this is a DL10019 or a DL10022. */
		type = bus_space_read_1(nsc->sc_asict, nsc->sc_asich, 0x0f);
		if (type == 0x91 || type == 0x99) {
			nsc->sc_type = NE2000_TYPE_DL10022;
		} else {
			nsc->sc_type = NE2000_TYPE_DL10019;
		}
	}

	if ((ne_dev->flags & NE2000DVF_AX88190) != 0) {
		u_int8_t test;

		/* XXX This is highly bogus. */
		if ((pa->pf->ccr_mask & (1 << PCMCIA_CCR_IOBASE0)) == 0) {
			++i;
			goto again;
		}

		dsc->sc_mediachange = ax88190_mediachange;
		dsc->sc_mediastatus = ax88190_mediastatus;
		dsc->init_card = ax88190_init_card;
		dsc->stop_card = ax88190_stop_card;
		dsc->sc_media_init = ax88190_media_init;
		dsc->sc_media_fini = ax88190_media_fini;

		test = bus_space_read_1(nsc->sc_asict, nsc->sc_asich, 0x05);
		if (test != 0) {
			nsc->sc_type = NE2000_TYPE_AX88790;
		} else {
			nsc->sc_type = NE2000_TYPE_AX88190;
		}
	}

	if (enaddr != NULL &&
	    ne_dev->enet_vendor[0] != 0xff) {
		/*
		 * Make sure this is what we expect.
		 */
		if (enaddr[0] != ne_dev->enet_vendor[0] ||
		    enaddr[1] != ne_dev->enet_vendor[1] ||
		    enaddr[2] != ne_dev->enet_vendor[2]) {
			++i;
			goto again;
		}
	}

	/*
	 * Check for a Realtek 8019.
	 */
	if (nsc->sc_type == NE2000_TYPE_UNKNOWN) {
		bus_space_write_1(dsc->sc_regt, dsc->sc_regh, ED_P0_CR,
		    ED_CR_PAGE_0 | ED_CR_STP);
		if (bus_space_read_1(dsc->sc_regt, dsc->sc_regh,
		    NERTL_RTL0_8019ID0) == RTL0_8019ID0 &&
		    bus_space_read_1(dsc->sc_regt, dsc->sc_regh,
		    NERTL_RTL0_8019ID1) == RTL0_8019ID1) {
			dsc->sc_mediachange = rtl80x9_mediachange;
			dsc->sc_mediastatus = rtl80x9_mediastatus;
			dsc->init_card = rtl80x9_init_card;
			dsc->sc_media_init = rtl80x9_media_init;
		}
	}

	if (ne2000_attach(nsc, enaddr))
		goto fail2;

	if (!pmf_device_register(self, ne2000_suspend, ne2000_resume)) {
		aprint_error_dev(self, "cannot set power mgmt handler\n");
	}
	/* pmf(9) power hooks */
	if (pmf_device_register(self, ne2000_suspend, ne2000_resume)) {
#if 0 /* XXX: notyet: if_stop is NULL! */
		pmf_class_network_register(self, &dsc->sc_ec.ec_if);
#endif
	} else
		aprint_error_dev(self, "unable to establish power handler\n");

	psc->sc_state = NE_PCMCIA_ATTACHED;
	ne_pcmcia_disable(dsc);
	return;

fail2:
	ne_pcmcia_disable(dsc);
fail:
	pcmcia_function_unconfigure(pa->pf);
}

int
ne_pcmcia_detach(device_t self, int flags)
{
	struct ne_pcmcia_softc *psc = device_private(self);
	struct pcmcia_function *pf = psc->sc_pf;
	int error;

	if (psc->sc_state != NE_PCMCIA_ATTACHED)
		return (0);

	pmf_device_deregister(self);
	error = ne2000_detach(&psc->sc_ne2000, flags);
	if (error)
		return (error);

	pcmcia_function_unconfigure(pf);

	return (0);
}

int
ne_pcmcia_enable(struct dp8390_softc *dsc)
{
	struct ne_pcmcia_softc *psc = (struct ne_pcmcia_softc *)dsc;
	int error;

	/* set up the interrupt */
	psc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET, dp8390_intr,
	    dsc);
	if (!psc->sc_ih)
		return (EIO);

	error = pcmcia_function_enable(psc->sc_pf);
	if (error) {
		pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
		psc->sc_ih = 0;
	}

	return (error);
}

void
ne_pcmcia_disable(struct dp8390_softc *dsc)
{
	struct ne_pcmcia_softc *psc = (struct ne_pcmcia_softc *)dsc;

	pcmcia_function_disable(psc->sc_pf);
	pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
	psc->sc_ih = 0;
}

u_int8_t *
ne_pcmcia_get_enaddr(struct ne_pcmcia_softc *psc, int maddr,
   u_int8_t myea[ETHER_ADDR_LEN])
{
	struct ne2000_softc *nsc = &psc->sc_ne2000;
	struct dp8390_softc *dsc = &nsc->sc_dp8390;
	struct pcmcia_mem_handle pcmh;
	bus_size_t offset;
	u_int8_t *enaddr = NULL;
	int j, mwindow;

	if (maddr < 0)
		return (NULL);

	if (pcmcia_mem_alloc(psc->sc_pf, ETHER_ADDR_LEN * 2, &pcmh)) {
		aprint_error_dev(dsc->sc_dev,
		    "can't alloc mem for enet addr\n");
		goto fail_1;
	}
	if (pcmcia_mem_map(psc->sc_pf, PCMCIA_MEM_ATTR, maddr,
	    ETHER_ADDR_LEN * 2, &pcmh, &offset, &mwindow)) {
		aprint_error_dev(dsc->sc_dev, "can't map mem for enet addr\n");
		goto fail_2;
	}
	for (j = 0; j < ETHER_ADDR_LEN; j++)
		myea[j] = bus_space_read_1(pcmh.memt, pcmh.memh,
		    offset + (j * 2));
	enaddr = myea;

	pcmcia_mem_unmap(psc->sc_pf, mwindow);
 fail_2:
	pcmcia_mem_free(psc->sc_pf, &pcmh);
 fail_1:
	return (enaddr);
}

u_int8_t *
ne_pcmcia_dl10019_get_enaddr(struct ne_pcmcia_softc *psc,
    u_int8_t myea[ETHER_ADDR_LEN])
{
	struct ne2000_softc *nsc = &psc->sc_ne2000;
	u_int8_t sum;
	int j;

#define PAR0	0x04
	for (j = 0, sum = 0; j < 8; j++)
		sum += bus_space_read_1(nsc->sc_asict, nsc->sc_asich,
		    PAR0 + j);
	if (sum != 0xff)
		return (NULL);
	for (j = 0; j < ETHER_ADDR_LEN; j++)
		myea[j] = bus_space_read_1(nsc->sc_asict,
		    nsc->sc_asich, PAR0 + j);
#undef PAR0
	return (myea);
}
