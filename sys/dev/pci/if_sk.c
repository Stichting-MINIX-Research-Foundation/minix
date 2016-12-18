/*	$NetBSD: if_sk.c,v 1.80 2015/04/13 16:33:25 riastradh Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
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

/*	$OpenBSD: if_sk.c,v 1.116 2006/06/22 23:06:03 brad Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: /c/ncvs/src/sys/pci/if_sk.c,v 1.20 2000/04/22 02:16:37 wpaul Exp $
 */

/*
 * Copyright (c) 2003 Nathan L. Binkert <binkertn@umich.edu>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * SysKonnect SK-NET gigabit ethernet driver for FreeBSD. Supports
 * the SK-984x series adapters, both single port and dual port.
 * References:
 * 	The XaQti XMAC II datasheet,
 * http://www.freebsd.org/~wpaul/SysKonnect/xmacii_datasheet_rev_c_9-29.pdf
 *	The SysKonnect GEnesis manual, http://www.syskonnect.com
 *
 * Note: XaQti has been acquired by Vitesse, and Vitesse does not have the
 * XMAC II datasheet online. I have put my copy at people.freebsd.org as a
 * convenience to others until Vitesse corrects this problem:
 *
 * http://people.freebsd.org/~wpaul/SysKonnect/xmacii_datasheet_rev_c_9-29.pdf
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Department of Electrical Engineering
 * Columbia University, New York City
 */

/*
 * The SysKonnect gigabit ethernet adapters consist of two main
 * components: the SysKonnect GEnesis controller chip and the XaQti Corp.
 * XMAC II gigabit ethernet MAC. The XMAC provides all of the MAC
 * components and a PHY while the GEnesis controller provides a PCI
 * interface with DMA support. Each card may have between 512K and
 * 2MB of SRAM on board depending on the configuration.
 *
 * The SysKonnect GEnesis controller can have either one or two XMAC
 * chips connected to it, allowing single or dual port NIC configurations.
 * SysKonnect has the distinction of being the only vendor on the market
 * with a dual port gigabit ethernet NIC. The GEnesis provides dual FIFOs,
 * dual DMA queues, packet/MAC/transmit arbiters and direct access to the
 * XMAC registers. This driver takes advantage of these features to allow
 * both XMACs to operate as independent interfaces.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_sk.c,v 1.80 2015/04/13 16:33:25 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/callout.h>
#include <sys/sysctl.h>
#include <sys/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <net/if_media.h>

#include <net/bpf.h>
#include <sys/rndsource.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/brgphyreg.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

/* #define SK_USEIOSPACE */

#include <dev/pci/if_skreg.h>
#include <dev/pci/if_skvar.h>

int skc_probe(device_t, cfdata_t, void *);
void skc_attach(device_t, device_t, void *aux);
int sk_probe(device_t, cfdata_t, void *);
void sk_attach(device_t, device_t, void *aux);
int skcprint(void *, const char *);
int sk_intr(void *);
void sk_intr_bcom(struct sk_if_softc *);
void sk_intr_xmac(struct sk_if_softc *);
void sk_intr_yukon(struct sk_if_softc *);
void sk_rxeof(struct sk_if_softc *);
void sk_txeof(struct sk_if_softc *);
int sk_encap(struct sk_if_softc *, struct mbuf *, u_int32_t *);
void sk_start(struct ifnet *);
int sk_ioctl(struct ifnet *, u_long, void *);
int sk_init(struct ifnet *);
void sk_init_xmac(struct sk_if_softc *);
void sk_init_yukon(struct sk_if_softc *);
void sk_stop(struct ifnet *, int);
void sk_watchdog(struct ifnet *);
void sk_shutdown(void *);
int sk_ifmedia_upd(struct ifnet *);
void sk_reset(struct sk_softc *);
int sk_newbuf(struct sk_if_softc *, int, struct mbuf *, bus_dmamap_t);
int sk_alloc_jumbo_mem(struct sk_if_softc *);
void sk_free_jumbo_mem(struct sk_if_softc *);
void *sk_jalloc(struct sk_if_softc *);
void sk_jfree(struct mbuf *, void *, size_t, void *);
int sk_init_rx_ring(struct sk_if_softc *);
int sk_init_tx_ring(struct sk_if_softc *);
u_int8_t sk_vpd_readbyte(struct sk_softc *, int);
void sk_vpd_read_res(struct sk_softc *,
					struct vpd_res *, int);
void sk_vpd_read(struct sk_softc *);

void sk_update_int_mod(struct sk_softc *);

int sk_xmac_miibus_readreg(device_t, int, int);
void sk_xmac_miibus_writereg(device_t, int, int, int);
void sk_xmac_miibus_statchg(struct ifnet *);

int sk_marv_miibus_readreg(device_t, int, int);
void sk_marv_miibus_writereg(device_t, int, int, int);
void sk_marv_miibus_statchg(struct ifnet *);

u_int32_t sk_xmac_hash(void *);
u_int32_t sk_yukon_hash(void *);
void sk_setfilt(struct sk_if_softc *, void *, int);
void sk_setmulti(struct sk_if_softc *);
void sk_tick(void *);

static bool skc_suspend(device_t, const pmf_qual_t *);
static bool skc_resume(device_t, const pmf_qual_t *);
static bool sk_resume(device_t dv, const pmf_qual_t *);

/* #define SK_DEBUG 2 */
#ifdef SK_DEBUG
#define DPRINTF(x)	if (skdebug) printf x
#define DPRINTFN(n,x)	if (skdebug >= (n)) printf x
int	skdebug = SK_DEBUG;

void sk_dump_txdesc(struct sk_tx_desc *, int);
void sk_dump_mbuf(struct mbuf *);
void sk_dump_bytes(const char *, int);
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

static int sk_sysctl_handler(SYSCTLFN_PROTO);
static int sk_root_num;

/* supported device vendors */
/* PCI_PRODUCT_DLINK_DGE560T_2 might belong in if_msk instead */
static const struct sk_product {
	pci_vendor_id_t		sk_vendor;
	pci_product_id_t	sk_product;
} sk_products[] = {
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3C940, },
	{ PCI_VENDOR_DLINK, PCI_PRODUCT_DLINK_DGE530T, },
	{ PCI_VENDOR_DLINK, PCI_PRODUCT_DLINK_DGE560T_2, },
	{ PCI_VENDOR_LINKSYS, PCI_PRODUCT_LINKSYS_EG1064, },
	{ PCI_VENDOR_SCHNEIDERKOCH, PCI_PRODUCT_SCHNEIDERKOCH_SKNET_GE, },
	{ PCI_VENDOR_SCHNEIDERKOCH, PCI_PRODUCT_SCHNEIDERKOCH_SK9821v2, },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_SKNET, },
	{ PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_BELKIN, },
	{ 0, 0, }
};

#define SK_LINKSYS_EG1032_SUBID	0x00151737

static inline u_int32_t
sk_win_read_4(struct sk_softc *sc, u_int32_t reg)
{
#ifdef SK_USEIOSPACE
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	return CSR_READ_4(sc, SK_WIN_BASE + SK_REG(reg));
#else
	return CSR_READ_4(sc, reg);
#endif
}

static inline u_int16_t
sk_win_read_2(struct sk_softc *sc, u_int32_t reg)
{
#ifdef SK_USEIOSPACE
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	return CSR_READ_2(sc, SK_WIN_BASE + SK_REG(reg));
#else
	return CSR_READ_2(sc, reg);
#endif
}

static inline u_int8_t
sk_win_read_1(struct sk_softc *sc, u_int32_t reg)
{
#ifdef SK_USEIOSPACE
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	return CSR_READ_1(sc, SK_WIN_BASE + SK_REG(reg));
#else
	return CSR_READ_1(sc, reg);
#endif
}

static inline void
sk_win_write_4(struct sk_softc *sc, u_int32_t reg, u_int32_t x)
{
#ifdef SK_USEIOSPACE
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	CSR_WRITE_4(sc, SK_WIN_BASE + SK_REG(reg), x);
#else
	CSR_WRITE_4(sc, reg, x);
#endif
}

static inline void
sk_win_write_2(struct sk_softc *sc, u_int32_t reg, u_int16_t x)
{
#ifdef SK_USEIOSPACE
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	CSR_WRITE_2(sc, SK_WIN_BASE + SK_REG(reg), x);
#else
	CSR_WRITE_2(sc, reg, x);
#endif
}

static inline void
sk_win_write_1(struct sk_softc *sc, u_int32_t reg, u_int8_t x)
{
#ifdef SK_USEIOSPACE
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	CSR_WRITE_1(sc, SK_WIN_BASE + SK_REG(reg), x);
#else
	CSR_WRITE_1(sc, reg, x);
#endif
}

/*
 * The VPD EEPROM contains Vital Product Data, as suggested in
 * the PCI 2.1 specification. The VPD data is separared into areas
 * denoted by resource IDs. The SysKonnect VPD contains an ID string
 * resource (the name of the adapter), a read-only area resource
 * containing various key/data fields and a read/write area which
 * can be used to store asset management information or log messages.
 * We read the ID string and read-only into buffers attached to
 * the controller softc structure for later use. At the moment,
 * we only use the ID string during sk_attach().
 */
u_int8_t
sk_vpd_readbyte(struct sk_softc *sc, int addr)
{
	int			i;

	sk_win_write_2(sc, SK_PCI_REG(SK_PCI_VPD_ADDR), addr);
	for (i = 0; i < SK_TIMEOUT; i++) {
		DELAY(1);
		if (sk_win_read_2(sc,
		    SK_PCI_REG(SK_PCI_VPD_ADDR)) & SK_VPD_FLAG)
			break;
	}

	if (i == SK_TIMEOUT)
		return 0;

	return sk_win_read_1(sc, SK_PCI_REG(SK_PCI_VPD_DATA));
}

void
sk_vpd_read_res(struct sk_softc *sc, struct vpd_res *res, int addr)
{
	int			i;
	u_int8_t		*ptr;

	ptr = (u_int8_t *)res;
	for (i = 0; i < sizeof(struct vpd_res); i++)
		ptr[i] = sk_vpd_readbyte(sc, i + addr);
}

void
sk_vpd_read(struct sk_softc *sc)
{
	int			pos = 0, i;
	struct vpd_res		res;

	if (sc->sk_vpd_prodname != NULL)
		free(sc->sk_vpd_prodname, M_DEVBUF);
	if (sc->sk_vpd_readonly != NULL)
		free(sc->sk_vpd_readonly, M_DEVBUF);
	sc->sk_vpd_prodname = NULL;
	sc->sk_vpd_readonly = NULL;

	sk_vpd_read_res(sc, &res, pos);

	if (res.vr_id != VPD_RES_ID) {
		aprint_error_dev(sc->sk_dev,
		    "bad VPD resource id: expected %x got %x\n",
		    VPD_RES_ID, res.vr_id);
		return;
	}

	pos += sizeof(res);
	sc->sk_vpd_prodname = malloc(res.vr_len + 1, M_DEVBUF, M_NOWAIT);
	if (sc->sk_vpd_prodname == NULL)
		panic("sk_vpd_read");
	for (i = 0; i < res.vr_len; i++)
		sc->sk_vpd_prodname[i] = sk_vpd_readbyte(sc, i + pos);
	sc->sk_vpd_prodname[i] = '\0';
	pos += i;

	sk_vpd_read_res(sc, &res, pos);

	if (res.vr_id != VPD_RES_READ) {
		aprint_error_dev(sc->sk_dev,
		    "bad VPD resource id: expected %x got %x\n",
		    VPD_RES_READ, res.vr_id);
		return;
	}

	pos += sizeof(res);
	sc->sk_vpd_readonly = malloc(res.vr_len, M_DEVBUF, M_NOWAIT);
	if (sc->sk_vpd_readonly == NULL)
		panic("sk_vpd_read");
	for (i = 0; i < res.vr_len ; i++)
		sc->sk_vpd_readonly[i] = sk_vpd_readbyte(sc, i + pos);
}

int
sk_xmac_miibus_readreg(device_t dev, int phy, int reg)
{
	struct sk_if_softc *sc_if = device_private(dev);
	int i;

	DPRINTFN(9, ("sk_xmac_miibus_readreg\n"));

	if (sc_if->sk_phytype == SK_PHYTYPE_XMAC && phy != 0)
		return 0;

	SK_XM_WRITE_2(sc_if, XM_PHY_ADDR, reg|(phy << 8));
	SK_XM_READ_2(sc_if, XM_PHY_DATA);
	if (sc_if->sk_phytype != SK_PHYTYPE_XMAC) {
		for (i = 0; i < SK_TIMEOUT; i++) {
			DELAY(1);
			if (SK_XM_READ_2(sc_if, XM_MMUCMD) &
			    XM_MMUCMD_PHYDATARDY)
				break;
		}

		if (i == SK_TIMEOUT) {
			aprint_error_dev(sc_if->sk_dev,
			    "phy failed to come ready\n");
			return 0;
		}
	}
	DELAY(1);
	return SK_XM_READ_2(sc_if, XM_PHY_DATA);
}

void
sk_xmac_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct sk_if_softc *sc_if = device_private(dev);
	int i;

	DPRINTFN(9, ("sk_xmac_miibus_writereg\n"));

	SK_XM_WRITE_2(sc_if, XM_PHY_ADDR, reg|(phy << 8));
	for (i = 0; i < SK_TIMEOUT; i++) {
		if (!(SK_XM_READ_2(sc_if, XM_MMUCMD) & XM_MMUCMD_PHYBUSY))
			break;
	}

	if (i == SK_TIMEOUT) {
		aprint_error_dev(sc_if->sk_dev, "phy failed to come ready\n");
		return;
	}

	SK_XM_WRITE_2(sc_if, XM_PHY_DATA, val);
	for (i = 0; i < SK_TIMEOUT; i++) {
		DELAY(1);
		if (!(SK_XM_READ_2(sc_if, XM_MMUCMD) & XM_MMUCMD_PHYBUSY))
			break;
	}

	if (i == SK_TIMEOUT)
		aprint_error_dev(sc_if->sk_dev, "phy write timed out\n");
}

void
sk_xmac_miibus_statchg(struct ifnet *ifp)
{
	struct sk_if_softc *sc_if = ifp->if_softc;
	struct mii_data *mii = &sc_if->sk_mii;

	DPRINTFN(9, ("sk_xmac_miibus_statchg\n"));

	/*
	 * If this is a GMII PHY, manually set the XMAC's
	 * duplex mode accordingly.
	 */
	if (sc_if->sk_phytype != SK_PHYTYPE_XMAC) {
		if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
			SK_XM_SETBIT_2(sc_if, XM_MMUCMD, XM_MMUCMD_GMIIFDX);
		else
			SK_XM_CLRBIT_2(sc_if, XM_MMUCMD, XM_MMUCMD_GMIIFDX);
	}
}

int
sk_marv_miibus_readreg(device_t dev, int phy, int reg)
{
	struct sk_if_softc *sc_if = device_private(dev);
	u_int16_t val;
	int i;

	if (phy != 0 ||
	    (sc_if->sk_phytype != SK_PHYTYPE_MARV_COPPER &&
	     sc_if->sk_phytype != SK_PHYTYPE_MARV_FIBER)) {
		DPRINTFN(9, ("sk_marv_miibus_readreg (skip) phy=%d, reg=%#x\n",
			     phy, reg));
		return 0;
	}

        SK_YU_WRITE_2(sc_if, YUKON_SMICR, YU_SMICR_PHYAD(phy) |
		      YU_SMICR_REGAD(reg) | YU_SMICR_OP_READ);

	for (i = 0; i < SK_TIMEOUT; i++) {
		DELAY(1);
		val = SK_YU_READ_2(sc_if, YUKON_SMICR);
		if (val & YU_SMICR_READ_VALID)
			break;
	}

	if (i == SK_TIMEOUT) {
		aprint_error_dev(sc_if->sk_dev, "phy failed to come ready\n");
		return 0;
	}

 	DPRINTFN(9, ("sk_marv_miibus_readreg: i=%d, timeout=%d\n", i,
		     SK_TIMEOUT));

        val = SK_YU_READ_2(sc_if, YUKON_SMIDR);

	DPRINTFN(9, ("sk_marv_miibus_readreg phy=%d, reg=%#x, val=%#x\n",
		     phy, reg, val));

	return val;
}

void
sk_marv_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct sk_if_softc *sc_if = device_private(dev);
	int i;

	DPRINTFN(9, ("sk_marv_miibus_writereg phy=%d reg=%#x val=%#x\n",
		     phy, reg, val));

	SK_YU_WRITE_2(sc_if, YUKON_SMIDR, val);
	SK_YU_WRITE_2(sc_if, YUKON_SMICR, YU_SMICR_PHYAD(phy) |
		      YU_SMICR_REGAD(reg) | YU_SMICR_OP_WRITE);

	for (i = 0; i < SK_TIMEOUT; i++) {
		DELAY(1);
		if (!(SK_YU_READ_2(sc_if, YUKON_SMICR) & YU_SMICR_BUSY))
			break;
	}

	if (i == SK_TIMEOUT)
		printf("%s: phy write timed out\n",
		    device_xname(sc_if->sk_dev));
}

void
sk_marv_miibus_statchg(struct ifnet *ifp)
{
	DPRINTFN(9, ("sk_marv_miibus_statchg: gpcr=%x\n",
		     SK_YU_READ_2(((struct sk_if_softc *)ifp->if_softc),
		     YUKON_GPCR)));
}

#define SK_HASH_BITS		6

u_int32_t
sk_xmac_hash(void *addr)
{
	u_int32_t		crc;

	crc = ether_crc32_le(addr,ETHER_ADDR_LEN);
	crc = ~crc & ((1<< SK_HASH_BITS) - 1);
	DPRINTFN(2,("multicast hash for %s is %x\n",ether_sprintf(addr),crc));
	return crc;
}

u_int32_t
sk_yukon_hash(void *addr)
{
	u_int32_t		crc;

	crc = ether_crc32_be(addr,ETHER_ADDR_LEN);
	crc &= ((1 << SK_HASH_BITS) - 1);
	DPRINTFN(2,("multicast hash for %s is %x\n",ether_sprintf(addr),crc));
	return crc;
}

void
sk_setfilt(struct sk_if_softc *sc_if, void *addrv, int slot)
{
	char *addr = addrv;
	int base = XM_RXFILT_ENTRY(slot);

	SK_XM_WRITE_2(sc_if, base, *(u_int16_t *)(&addr[0]));
	SK_XM_WRITE_2(sc_if, base + 2, *(u_int16_t *)(&addr[2]));
	SK_XM_WRITE_2(sc_if, base + 4, *(u_int16_t *)(&addr[4]));
}

void
sk_setmulti(struct sk_if_softc *sc_if)
{
	struct sk_softc *sc = sc_if->sk_softc;
	struct ifnet *ifp= &sc_if->sk_ethercom.ec_if;
	u_int32_t hashes[2] = { 0, 0 };
	int h = 0, i;
	struct ethercom *ec = &sc_if->sk_ethercom;
	struct ether_multi *enm;
	struct ether_multistep step;
	u_int8_t dummy[] = { 0, 0, 0, 0, 0 ,0 };

	/* First, zot all the existing filters. */
	switch (sc->sk_type) {
	case SK_GENESIS:
		for (i = 1; i < XM_RXFILT_MAX; i++)
			sk_setfilt(sc_if, (void *)&dummy, i);

		SK_XM_WRITE_4(sc_if, XM_MAR0, 0);
		SK_XM_WRITE_4(sc_if, XM_MAR2, 0);
		break;
	case SK_YUKON:
	case SK_YUKON_LITE:
	case SK_YUKON_LP:
		SK_YU_WRITE_2(sc_if, YUKON_MCAH1, 0);
		SK_YU_WRITE_2(sc_if, YUKON_MCAH2, 0);
		SK_YU_WRITE_2(sc_if, YUKON_MCAH3, 0);
		SK_YU_WRITE_2(sc_if, YUKON_MCAH4, 0);
		break;
	}

	/* Now program new ones. */
allmulti:
	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		hashes[0] = 0xFFFFFFFF;
		hashes[1] = 0xFFFFFFFF;
	} else {
		i = 1;
		/* First find the tail of the list. */
		ETHER_FIRST_MULTI(step, ec, enm);
		while (enm != NULL) {
			if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
				 ETHER_ADDR_LEN)) {
				ifp->if_flags |= IFF_ALLMULTI;
				goto allmulti;
			}
			DPRINTFN(2,("multicast address %s\n",
	    			ether_sprintf(enm->enm_addrlo)));
			/*
			 * Program the first XM_RXFILT_MAX multicast groups
			 * into the perfect filter. For all others,
			 * use the hash table.
			 */
			if (sc->sk_type == SK_GENESIS && i < XM_RXFILT_MAX) {
				sk_setfilt(sc_if, enm->enm_addrlo, i);
				i++;
			}
			else {
				switch (sc->sk_type) {
				case SK_GENESIS:
					h = sk_xmac_hash(enm->enm_addrlo);
					break;
				case SK_YUKON:
				case SK_YUKON_LITE:
				case SK_YUKON_LP:
					h = sk_yukon_hash(enm->enm_addrlo);
					break;
				}
				if (h < 32)
					hashes[0] |= (1 << h);
				else
					hashes[1] |= (1 << (h - 32));
			}

			ETHER_NEXT_MULTI(step, enm);
		}
	}

	switch (sc->sk_type) {
	case SK_GENESIS:
		SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_USE_HASH|
			       XM_MODE_RX_USE_PERFECT);
		SK_XM_WRITE_4(sc_if, XM_MAR0, hashes[0]);
		SK_XM_WRITE_4(sc_if, XM_MAR2, hashes[1]);
		break;
	case SK_YUKON:
	case SK_YUKON_LITE:
	case SK_YUKON_LP:
		SK_YU_WRITE_2(sc_if, YUKON_MCAH1, hashes[0] & 0xffff);
		SK_YU_WRITE_2(sc_if, YUKON_MCAH2, (hashes[0] >> 16) & 0xffff);
		SK_YU_WRITE_2(sc_if, YUKON_MCAH3, hashes[1] & 0xffff);
		SK_YU_WRITE_2(sc_if, YUKON_MCAH4, (hashes[1] >> 16) & 0xffff);
		break;
	}
}

int
sk_init_rx_ring(struct sk_if_softc *sc_if)
{
	struct sk_chain_data	*cd = &sc_if->sk_cdata;
	struct sk_ring_data	*rd = sc_if->sk_rdata;
	int			i;

	memset((char *)rd->sk_rx_ring, 0,
	    sizeof(struct sk_rx_desc) * SK_RX_RING_CNT);

	for (i = 0; i < SK_RX_RING_CNT; i++) {
		cd->sk_rx_chain[i].sk_desc = &rd->sk_rx_ring[i];
		if (i == (SK_RX_RING_CNT - 1)) {
			cd->sk_rx_chain[i].sk_next = &cd->sk_rx_chain[0];
			rd->sk_rx_ring[i].sk_next =
				htole32(SK_RX_RING_ADDR(sc_if, 0));
		} else {
			cd->sk_rx_chain[i].sk_next = &cd->sk_rx_chain[i + 1];
			rd->sk_rx_ring[i].sk_next =
				htole32(SK_RX_RING_ADDR(sc_if,i+1));
		}
	}

	for (i = 0; i < SK_RX_RING_CNT; i++) {
		if (sk_newbuf(sc_if, i, NULL,
		    sc_if->sk_cdata.sk_rx_jumbo_map) == ENOBUFS) {
			aprint_error_dev(sc_if->sk_dev,
			    "failed alloc of %dth mbuf\n", i);
			return ENOBUFS;
		}
	}
	sc_if->sk_cdata.sk_rx_prod = 0;
	sc_if->sk_cdata.sk_rx_cons = 0;

	return 0;
}

int
sk_init_tx_ring(struct sk_if_softc *sc_if)
{
	struct sk_chain_data	*cd = &sc_if->sk_cdata;
	struct sk_ring_data	*rd = sc_if->sk_rdata;
	int			i;

	memset(sc_if->sk_rdata->sk_tx_ring, 0,
	    sizeof(struct sk_tx_desc) * SK_TX_RING_CNT);

	for (i = 0; i < SK_TX_RING_CNT; i++) {
		cd->sk_tx_chain[i].sk_desc = &rd->sk_tx_ring[i];
		if (i == (SK_TX_RING_CNT - 1)) {
			cd->sk_tx_chain[i].sk_next = &cd->sk_tx_chain[0];
			rd->sk_tx_ring[i].sk_next =
				htole32(SK_TX_RING_ADDR(sc_if, 0));
		} else {
			cd->sk_tx_chain[i].sk_next = &cd->sk_tx_chain[i + 1];
			rd->sk_tx_ring[i].sk_next =
				htole32(SK_TX_RING_ADDR(sc_if,i+1));
		}
	}

	sc_if->sk_cdata.sk_tx_prod = 0;
	sc_if->sk_cdata.sk_tx_cons = 0;
	sc_if->sk_cdata.sk_tx_cnt = 0;

	SK_CDTXSYNC(sc_if, 0, SK_TX_RING_CNT,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	return 0;
}

int
sk_newbuf(struct sk_if_softc *sc_if, int i, struct mbuf *m,
	  bus_dmamap_t dmamap)
{
	struct mbuf		*m_new = NULL;
	struct sk_chain		*c;
	struct sk_rx_desc	*r;

	if (m == NULL) {
		void *buf = NULL;

		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			aprint_error_dev(sc_if->sk_dev,
			    "no memory for rx list -- packet dropped!\n");
			return ENOBUFS;
		}

		/* Allocate the jumbo buffer */
		buf = sk_jalloc(sc_if);
		if (buf == NULL) {
			m_freem(m_new);
			DPRINTFN(1, ("%s jumbo allocation failed -- packet "
			    "dropped!\n", sc_if->sk_ethercom.ec_if.if_xname));
			return ENOBUFS;
		}

		/* Attach the buffer to the mbuf */
		m_new->m_len = m_new->m_pkthdr.len = SK_JLEN;
		MEXTADD(m_new, buf, SK_JLEN, 0, sk_jfree, sc_if);

	} else {
		/*
	 	 * We're re-using a previously allocated mbuf;
		 * be sure to re-init pointers and lengths to
		 * default values.
		 */
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = SK_JLEN;
		m_new->m_data = m_new->m_ext.ext_buf;
	}
	m_adj(m_new, ETHER_ALIGN);

	c = &sc_if->sk_cdata.sk_rx_chain[i];
	r = c->sk_desc;
	c->sk_mbuf = m_new;
	r->sk_data_lo = htole32(dmamap->dm_segs[0].ds_addr +
	    (((vaddr_t)m_new->m_data
		- (vaddr_t)sc_if->sk_cdata.sk_jumbo_buf)));
	r->sk_ctl = htole32(SK_JLEN | SK_RXSTAT);

	SK_CDRXSYNC(sc_if, i, BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	return 0;
}

/*
 * Memory management for jumbo frames.
 */

int
sk_alloc_jumbo_mem(struct sk_if_softc *sc_if)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	char *ptr, *kva;
	bus_dma_segment_t	seg;
	int		i, rseg, state, error;
	struct sk_jpool_entry   *entry;

	state = error = 0;

	/* Grab a big chunk o' storage. */
	if (bus_dmamem_alloc(sc->sc_dmatag, SK_JMEM, PAGE_SIZE, 0,
			     &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sk_dev, "can't alloc rx buffers\n");
		return ENOBUFS;
	}

	state = 1;
	if (bus_dmamem_map(sc->sc_dmatag, &seg, rseg, SK_JMEM, (void **)&kva,
			   BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sk_dev,
		    "can't map dma buffers (%d bytes)\n",
		    SK_JMEM);
		error = ENOBUFS;
		goto out;
	}

	state = 2;
	if (bus_dmamap_create(sc->sc_dmatag, SK_JMEM, 1, SK_JMEM, 0,
	    BUS_DMA_NOWAIT, &sc_if->sk_cdata.sk_rx_jumbo_map)) {
		aprint_error_dev(sc->sk_dev, "can't create dma map\n");
		error = ENOBUFS;
		goto out;
	}

	state = 3;
	if (bus_dmamap_load(sc->sc_dmatag, sc_if->sk_cdata.sk_rx_jumbo_map,
			    kva, SK_JMEM, NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sk_dev, "can't load dma map\n");
		error = ENOBUFS;
		goto out;
	}

	state = 4;
	sc_if->sk_cdata.sk_jumbo_buf = (void *)kva;
	DPRINTFN(1,("sk_jumbo_buf = 0x%p\n", sc_if->sk_cdata.sk_jumbo_buf));

	LIST_INIT(&sc_if->sk_jfree_listhead);
	LIST_INIT(&sc_if->sk_jinuse_listhead);
	mutex_init(&sc_if->sk_jpool_mtx, MUTEX_DEFAULT, IPL_NET);

	/*
	 * Now divide it up into 9K pieces and save the addresses
	 * in an array.
	 */
	ptr = sc_if->sk_cdata.sk_jumbo_buf;
	for (i = 0; i < SK_JSLOTS; i++) {
		sc_if->sk_cdata.sk_jslots[i] = ptr;
		ptr += SK_JLEN;
		entry = malloc(sizeof(struct sk_jpool_entry),
		    M_DEVBUF, M_NOWAIT);
		if (entry == NULL) {
			aprint_error_dev(sc->sk_dev,
			    "no memory for jumbo buffer queue!\n");
			error = ENOBUFS;
			goto out;
		}
		entry->slot = i;
		if (i)
			LIST_INSERT_HEAD(&sc_if->sk_jfree_listhead,
				 entry, jpool_entries);
		else
			LIST_INSERT_HEAD(&sc_if->sk_jinuse_listhead,
				 entry, jpool_entries);
	}
out:
	if (error != 0) {
		switch (state) {
		case 4:
			bus_dmamap_unload(sc->sc_dmatag,
			    sc_if->sk_cdata.sk_rx_jumbo_map);
		case 3:
			bus_dmamap_destroy(sc->sc_dmatag,
			    sc_if->sk_cdata.sk_rx_jumbo_map);
		case 2:
			bus_dmamem_unmap(sc->sc_dmatag, kva, SK_JMEM);
		case 1:
			bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
			break;
		default:
			break;
		}
	}

	return error;
}

/*
 * Allocate a jumbo buffer.
 */
void *
sk_jalloc(struct sk_if_softc *sc_if)
{
	struct sk_jpool_entry   *entry;

	mutex_enter(&sc_if->sk_jpool_mtx);
	entry = LIST_FIRST(&sc_if->sk_jfree_listhead);

	if (entry == NULL) {
		mutex_exit(&sc_if->sk_jpool_mtx);
		return NULL;
	}

	LIST_REMOVE(entry, jpool_entries);
	LIST_INSERT_HEAD(&sc_if->sk_jinuse_listhead, entry, jpool_entries);
	mutex_exit(&sc_if->sk_jpool_mtx);
	return sc_if->sk_cdata.sk_jslots[entry->slot];
}

/*
 * Release a jumbo buffer.
 */
void
sk_jfree(struct mbuf *m, void *buf, size_t size, void *arg)
{
	struct sk_jpool_entry *entry;
	struct sk_if_softc *sc;
	int i;

	/* Extract the softc struct pointer. */
	sc = (struct sk_if_softc *)arg;

	if (sc == NULL)
		panic("sk_jfree: can't find softc pointer!");

	/* calculate the slot this buffer belongs to */

	i = ((vaddr_t)buf
	     - (vaddr_t)sc->sk_cdata.sk_jumbo_buf) / SK_JLEN;

	if ((i < 0) || (i >= SK_JSLOTS))
		panic("sk_jfree: asked to free buffer that we don't manage!");

	mutex_enter(&sc->sk_jpool_mtx);
	entry = LIST_FIRST(&sc->sk_jinuse_listhead);
	if (entry == NULL)
		panic("sk_jfree: buffer not in use!");
	entry->slot = i;
	LIST_REMOVE(entry, jpool_entries);
	LIST_INSERT_HEAD(&sc->sk_jfree_listhead, entry, jpool_entries);
	mutex_exit(&sc->sk_jpool_mtx);

	if (__predict_true(m != NULL))
		pool_cache_put(mb_cache, m);
}

/*
 * Set media options.
 */
int
sk_ifmedia_upd(struct ifnet *ifp)
{
	struct sk_if_softc *sc_if = ifp->if_softc;
	int rc;

	(void) sk_init(ifp);
	if ((rc = mii_mediachg(&sc_if->sk_mii)) == ENXIO)
		return 0;
	return rc;
}

int
sk_ioctl(struct ifnet *ifp, u_long command, void *data)
{
	struct sk_if_softc *sc_if = ifp->if_softc;
	struct sk_softc *sc = sc_if->sk_softc;
	int s, error = 0;

	/* DPRINTFN(2, ("sk_ioctl\n")); */

	s = splnet();

	switch (command) {

	case SIOCSIFFLAGS:
	        DPRINTFN(2, ("sk_ioctl IFFLAGS\n"));
		if ((error = ifioctl_common(ifp, command, data)) != 0)
			break;
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc_if->sk_if_flags & IFF_PROMISC)) {
				switch (sc->sk_type) {
				case SK_GENESIS:
					SK_XM_SETBIT_4(sc_if, XM_MODE,
					    XM_MODE_RX_PROMISC);
					break;
				case SK_YUKON:
				case SK_YUKON_LITE:
				case SK_YUKON_LP:
					SK_YU_CLRBIT_2(sc_if, YUKON_RCR,
					    YU_RCR_UFLEN | YU_RCR_MUFLEN);
					break;
				}
				sk_setmulti(sc_if);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc_if->sk_if_flags & IFF_PROMISC) {
				switch (sc->sk_type) {
				case SK_GENESIS:
					SK_XM_CLRBIT_4(sc_if, XM_MODE,
					    XM_MODE_RX_PROMISC);
					break;
				case SK_YUKON:
				case SK_YUKON_LITE:
				case SK_YUKON_LP:
					SK_YU_SETBIT_2(sc_if, YUKON_RCR,
					    YU_RCR_UFLEN | YU_RCR_MUFLEN);
					break;
				}

				sk_setmulti(sc_if);
			} else
				(void) sk_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				sk_stop(ifp,0);
		}
		sc_if->sk_if_flags = ifp->if_flags;
		error = 0;
		break;

	default:
	        DPRINTFN(2, ("sk_ioctl ETHER\n"));
		if ((error = ether_ioctl(ifp, command, data)) != ENETRESET)
			break;

		error = 0;

		if (command != SIOCADDMULTI && command != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING) {
			sk_setmulti(sc_if);
			DPRINTFN(2, ("sk_ioctl setmulti called\n"));
		}
		break;
	}

	splx(s);
	return error;
}

void
sk_update_int_mod(struct sk_softc *sc)
{
	u_int32_t imtimer_ticks;

	/*
         * Configure interrupt moderation. The moderation timer
	 * defers interrupts specified in the interrupt moderation
	 * timer mask based on the timeout specified in the interrupt
	 * moderation timer init register. Each bit in the timer
	 * register represents one tick, so to specify a timeout in
	 * microseconds, we have to multiply by the correct number of
	 * ticks-per-microsecond.
	 */
	switch (sc->sk_type) {
	case SK_GENESIS:
		imtimer_ticks = SK_IMTIMER_TICKS_GENESIS;
		break;
	case SK_YUKON_EC:
		imtimer_ticks = SK_IMTIMER_TICKS_YUKON_EC;
		break;
	default:
		imtimer_ticks = SK_IMTIMER_TICKS_YUKON;
	}
	aprint_verbose_dev(sc->sk_dev, "interrupt moderation is %d us\n",
	    sc->sk_int_mod);
        sk_win_write_4(sc, SK_IMTIMERINIT, SK_IM_USECS(sc->sk_int_mod));
        sk_win_write_4(sc, SK_IMMR, SK_ISR_TX1_S_EOF|SK_ISR_TX2_S_EOF|
	    SK_ISR_RX1_EOF|SK_ISR_RX2_EOF);
        sk_win_write_1(sc, SK_IMTIMERCTL, SK_IMCTL_START);
	sc->sk_int_mod_pending = 0;
}

/*
 * Lookup: Check the PCI vendor and device, and return a pointer to
 * The structure if the IDs match against our list.
 */

static const struct sk_product *
sk_lookup(const struct pci_attach_args *pa)
{
	const struct sk_product *psk;

	for ( psk = &sk_products[0]; psk->sk_vendor != 0; psk++ ) {
		if (PCI_VENDOR(pa->pa_id) == psk->sk_vendor &&
		    PCI_PRODUCT(pa->pa_id) == psk->sk_product)
			return psk;
	}
	return NULL;
}

/*
 * Probe for a SysKonnect GEnesis chip.
 */

int
skc_probe(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	const struct sk_product *psk;
	pcireg_t subid;

	subid = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

	/* special-case Linksys EG1032, since rev 3 uses re(4) */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_LINKSYS &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_LINKSYS_EG1032 &&
	    subid == SK_LINKSYS_EG1032_SUBID)
		return 1;

	if ((psk = sk_lookup(pa))) {
		return 1;
	}
	return 0;
}

/*
 * Force the GEnesis into reset, then bring it out of reset.
 */
void sk_reset(struct sk_softc *sc)
{
	DPRINTFN(2, ("sk_reset\n"));

	CSR_WRITE_2(sc, SK_CSR, SK_CSR_SW_RESET);
	CSR_WRITE_2(sc, SK_CSR, SK_CSR_MASTER_RESET);
	if (SK_YUKON_FAMILY(sc->sk_type))
		CSR_WRITE_2(sc, SK_LINK_CTRL, SK_LINK_RESET_SET);

	DELAY(1000);
	CSR_WRITE_2(sc, SK_CSR, SK_CSR_SW_UNRESET);
	DELAY(2);
	CSR_WRITE_2(sc, SK_CSR, SK_CSR_MASTER_UNRESET);
	if (SK_YUKON_FAMILY(sc->sk_type))
		CSR_WRITE_2(sc, SK_LINK_CTRL, SK_LINK_RESET_CLEAR);

	DPRINTFN(2, ("sk_reset: sk_csr=%x\n", CSR_READ_2(sc, SK_CSR)));
	DPRINTFN(2, ("sk_reset: sk_link_ctrl=%x\n",
		     CSR_READ_2(sc, SK_LINK_CTRL)));

	if (sc->sk_type == SK_GENESIS) {
		/* Configure packet arbiter */
		sk_win_write_2(sc, SK_PKTARB_CTL, SK_PKTARBCTL_UNRESET);
		sk_win_write_2(sc, SK_RXPA1_TINIT, SK_PKTARB_TIMEOUT);
		sk_win_write_2(sc, SK_TXPA1_TINIT, SK_PKTARB_TIMEOUT);
		sk_win_write_2(sc, SK_RXPA2_TINIT, SK_PKTARB_TIMEOUT);
		sk_win_write_2(sc, SK_TXPA2_TINIT, SK_PKTARB_TIMEOUT);
	}

	/* Enable RAM interface */
	sk_win_write_4(sc, SK_RAMCTL, SK_RAMCTL_UNRESET);

	sk_update_int_mod(sc);
}

int
sk_probe(device_t parent, cfdata_t match, void *aux)
{
	struct skc_attach_args *sa = aux;

	if (sa->skc_port != SK_PORT_A && sa->skc_port != SK_PORT_B)
		return 0;

	return 1;
}

/*
 * Each XMAC chip is attached as a separate logical IP interface.
 * Single port cards will have only one logical interface of course.
 */
void
sk_attach(device_t parent, device_t self, void *aux)
{
	struct sk_if_softc *sc_if = device_private(self);
	struct sk_softc *sc = device_private(parent);
	struct skc_attach_args *sa = aux;
	struct sk_txmap_entry	*entry;
	struct ifnet *ifp;
	bus_dma_segment_t seg;
	bus_dmamap_t dmamap;
	prop_data_t data;
	void *kva;
	int i, rseg;
	int mii_flags = 0;

	aprint_naive("\n");

	sc_if->sk_dev = self;
	sc_if->sk_port = sa->skc_port;
	sc_if->sk_softc = sc;
	sc->sk_if[sa->skc_port] = sc_if;

	if (sa->skc_port == SK_PORT_A)
		sc_if->sk_tx_bmu = SK_BMU_TXS_CSR0;
	if (sa->skc_port == SK_PORT_B)
		sc_if->sk_tx_bmu = SK_BMU_TXS_CSR1;

	DPRINTFN(2, ("begin sk_attach: port=%d\n", sc_if->sk_port));

	/*
	 * Get station address for this interface. Note that
	 * dual port cards actually come with three station
	 * addresses: one for each port, plus an extra. The
	 * extra one is used by the SysKonnect driver software
	 * as a 'virtual' station address for when both ports
	 * are operating in failover mode. Currently we don't
	 * use this extra address.
	 */
	data = prop_dictionary_get(device_properties(self), "mac-address");
	if (data != NULL) {
		/*
		 * Try to get the station address from device properties
		 * first, in case the ROM is missing.
		 */
		KASSERT(prop_object_type(data) == PROP_TYPE_DATA);
		KASSERT(prop_data_size(data) == ETHER_ADDR_LEN);
		memcpy(sc_if->sk_enaddr, prop_data_data_nocopy(data),
		    ETHER_ADDR_LEN);
	} else
		for (i = 0; i < ETHER_ADDR_LEN; i++)
			sc_if->sk_enaddr[i] = sk_win_read_1(sc,
			    SK_MAC0_0 + (sa->skc_port * 8) + i);

	aprint_normal(": Ethernet address %s\n",
	    ether_sprintf(sc_if->sk_enaddr));

	/*
	 * Set up RAM buffer addresses. The NIC will have a certain
	 * amount of SRAM on it, somewhere between 512K and 2MB. We
	 * need to divide this up a) between the transmitter and
 	 * receiver and b) between the two XMACs, if this is a
	 * dual port NIC. Our algorithm is to divide up the memory
	 * evenly so that everyone gets a fair share.
	 */
	if (sk_win_read_1(sc, SK_CONFIG) & SK_CONFIG_SINGLEMAC) {
		u_int32_t		chunk, val;

		chunk = sc->sk_ramsize / 2;
		val = sc->sk_rboff / sizeof(u_int64_t);
		sc_if->sk_rx_ramstart = val;
		val += (chunk / sizeof(u_int64_t));
		sc_if->sk_rx_ramend = val - 1;
		sc_if->sk_tx_ramstart = val;
		val += (chunk / sizeof(u_int64_t));
		sc_if->sk_tx_ramend = val - 1;
	} else {
		u_int32_t		chunk, val;

		chunk = sc->sk_ramsize / 4;
		val = (sc->sk_rboff + (chunk * 2 * sc_if->sk_port)) /
		    sizeof(u_int64_t);
		sc_if->sk_rx_ramstart = val;
		val += (chunk / sizeof(u_int64_t));
		sc_if->sk_rx_ramend = val - 1;
		sc_if->sk_tx_ramstart = val;
		val += (chunk / sizeof(u_int64_t));
		sc_if->sk_tx_ramend = val - 1;
	}

	DPRINTFN(2, ("sk_attach: rx_ramstart=%#x rx_ramend=%#x\n"
		     "           tx_ramstart=%#x tx_ramend=%#x\n",
		     sc_if->sk_rx_ramstart, sc_if->sk_rx_ramend,
		     sc_if->sk_tx_ramstart, sc_if->sk_tx_ramend));

	/* Read and save PHY type and set PHY address */
	sc_if->sk_phytype = sk_win_read_1(sc, SK_EPROM1) & 0xF;
	switch (sc_if->sk_phytype) {
	case SK_PHYTYPE_XMAC:
		sc_if->sk_phyaddr = SK_PHYADDR_XMAC;
		break;
	case SK_PHYTYPE_BCOM:
		sc_if->sk_phyaddr = SK_PHYADDR_BCOM;
		break;
	case SK_PHYTYPE_MARV_COPPER:
		sc_if->sk_phyaddr = SK_PHYADDR_MARV;
		break;
	default:
		aprint_error_dev(sc->sk_dev, "unsupported PHY type: %d\n",
		    sc_if->sk_phytype);
		return;
	}

	/* Allocate the descriptor queues. */
	if (bus_dmamem_alloc(sc->sc_dmatag, sizeof(struct sk_ring_data),
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->sk_dev, "can't alloc rx buffers\n");
		goto fail;
	}
	if (bus_dmamem_map(sc->sc_dmatag, &seg, rseg,
	    sizeof(struct sk_ring_data), &kva, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc_if->sk_dev,
		    "can't map dma buffers (%lu bytes)\n",
		    (u_long) sizeof(struct sk_ring_data));
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		goto fail;
	}
	if (bus_dmamap_create(sc->sc_dmatag, sizeof(struct sk_ring_data), 1,
	    sizeof(struct sk_ring_data), 0, BUS_DMA_NOWAIT,
            &sc_if->sk_ring_map)) {
		aprint_error_dev(sc_if->sk_dev, "can't create dma map\n");
		bus_dmamem_unmap(sc->sc_dmatag, kva,
		    sizeof(struct sk_ring_data));
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		goto fail;
	}
	if (bus_dmamap_load(sc->sc_dmatag, sc_if->sk_ring_map, kva,
	    sizeof(struct sk_ring_data), NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc_if->sk_dev, "can't load dma map\n");
		bus_dmamap_destroy(sc->sc_dmatag, sc_if->sk_ring_map);
		bus_dmamem_unmap(sc->sc_dmatag, kva,
		    sizeof(struct sk_ring_data));
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		goto fail;
	}

	for (i = 0; i < SK_RX_RING_CNT; i++)
		sc_if->sk_cdata.sk_rx_chain[i].sk_mbuf = NULL;

	SIMPLEQ_INIT(&sc_if->sk_txmap_head);
	for (i = 0; i < SK_TX_RING_CNT; i++) {
		sc_if->sk_cdata.sk_tx_chain[i].sk_mbuf = NULL;

		if (bus_dmamap_create(sc->sc_dmatag, SK_JLEN, SK_NTXSEG,
		    SK_JLEN, 0, BUS_DMA_NOWAIT, &dmamap)) {
			aprint_error_dev(sc_if->sk_dev,
			    "Can't create TX dmamap\n");
			bus_dmamap_unload(sc->sc_dmatag, sc_if->sk_ring_map);
			bus_dmamap_destroy(sc->sc_dmatag, sc_if->sk_ring_map);
			bus_dmamem_unmap(sc->sc_dmatag, kva,
			    sizeof(struct sk_ring_data));
			bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
			goto fail;
		}

		entry = malloc(sizeof(*entry), M_DEVBUF, M_NOWAIT);
		if (!entry) {
			aprint_error_dev(sc_if->sk_dev,
			    "Can't alloc txmap entry\n");
			bus_dmamap_destroy(sc->sc_dmatag, dmamap);
			bus_dmamap_unload(sc->sc_dmatag, sc_if->sk_ring_map);
			bus_dmamap_destroy(sc->sc_dmatag, sc_if->sk_ring_map);
			bus_dmamem_unmap(sc->sc_dmatag, kva,
			    sizeof(struct sk_ring_data));
			bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
			goto fail;
		}
		entry->dmamap = dmamap;
		SIMPLEQ_INSERT_HEAD(&sc_if->sk_txmap_head, entry, link);
	}

        sc_if->sk_rdata = (struct sk_ring_data *)kva;
	memset(sc_if->sk_rdata, 0, sizeof(struct sk_ring_data));

	ifp = &sc_if->sk_ethercom.ec_if;
	/* Try to allocate memory for jumbo buffers. */
	if (sk_alloc_jumbo_mem(sc_if)) {
		aprint_error("%s: jumbo buffer allocation failed\n", ifp->if_xname);
		goto fail;
	}
	sc_if->sk_ethercom.ec_capabilities = ETHERCAP_VLAN_MTU
		| ETHERCAP_JUMBO_MTU;

	ifp->if_softc = sc_if;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = sk_ioctl;
	ifp->if_start = sk_start;
	ifp->if_stop = sk_stop;
	ifp->if_init = sk_init;
	ifp->if_watchdog = sk_watchdog;
	ifp->if_capabilities = 0;
	IFQ_SET_MAXLEN(&ifp->if_snd, SK_TX_RING_CNT - 1);
	IFQ_SET_READY(&ifp->if_snd);
	strlcpy(ifp->if_xname, device_xname(sc_if->sk_dev), IFNAMSIZ);

	/*
	 * Do miibus setup.
	 */
	switch (sc->sk_type) {
	case SK_GENESIS:
		sk_init_xmac(sc_if);
		break;
	case SK_YUKON:
	case SK_YUKON_LITE:
	case SK_YUKON_LP:
		sk_init_yukon(sc_if);
		break;
	default:
		aprint_error_dev(sc->sk_dev, "unknown device type %d\n",
			sc->sk_type);
		goto fail;
	}

 	DPRINTFN(2, ("sk_attach: 1\n"));

	sc_if->sk_mii.mii_ifp = ifp;
	switch (sc->sk_type) {
	case SK_GENESIS:
		sc_if->sk_mii.mii_readreg = sk_xmac_miibus_readreg;
		sc_if->sk_mii.mii_writereg = sk_xmac_miibus_writereg;
		sc_if->sk_mii.mii_statchg = sk_xmac_miibus_statchg;
		break;
	case SK_YUKON:
	case SK_YUKON_LITE:
	case SK_YUKON_LP:
		sc_if->sk_mii.mii_readreg = sk_marv_miibus_readreg;
		sc_if->sk_mii.mii_writereg = sk_marv_miibus_writereg;
		sc_if->sk_mii.mii_statchg = sk_marv_miibus_statchg;
		mii_flags = MIIF_DOPAUSE;
		break;
	}

	sc_if->sk_ethercom.ec_mii = &sc_if->sk_mii;
	ifmedia_init(&sc_if->sk_mii.mii_media, 0,
	    sk_ifmedia_upd, ether_mediastatus);
	mii_attach(self, &sc_if->sk_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, mii_flags);
	if (LIST_EMPTY(&sc_if->sk_mii.mii_phys)) {
		aprint_error_dev(sc_if->sk_dev, "no PHY found!\n");
		ifmedia_add(&sc_if->sk_mii.mii_media, IFM_ETHER|IFM_MANUAL,
			    0, NULL);
		ifmedia_set(&sc_if->sk_mii.mii_media, IFM_ETHER|IFM_MANUAL);
	} else
		ifmedia_set(&sc_if->sk_mii.mii_media, IFM_ETHER|IFM_AUTO);

	callout_init(&sc_if->sk_tick_ch, 0);
	callout_reset(&sc_if->sk_tick_ch,hz,sk_tick,sc_if);

	DPRINTFN(2, ("sk_attach: 1\n"));

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);

	ether_ifattach(ifp, sc_if->sk_enaddr);

        rnd_attach_source(&sc->rnd_source, device_xname(sc->sk_dev),
            RND_TYPE_NET, RND_FLAG_DEFAULT);

	if (pmf_device_register(self, NULL, sk_resume))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	DPRINTFN(2, ("sk_attach: end\n"));

	return;

fail:
	sc->sk_if[sa->skc_port] = NULL;
}

int
skcprint(void *aux, const char *pnp)
{
	struct skc_attach_args *sa = aux;

	if (pnp)
		aprint_normal("sk port %c at %s",
		    (sa->skc_port == SK_PORT_A) ? 'A' : 'B', pnp);
	else
		aprint_normal(" port %c",
		    (sa->skc_port == SK_PORT_A) ? 'A' : 'B');
	return UNCONF;
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void
skc_attach(device_t parent, device_t self, void *aux)
{
	struct sk_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct skc_attach_args skca;
	pci_chipset_tag_t pc = pa->pa_pc;
#ifndef SK_USEIOSPACE
	pcireg_t memtype;
#endif
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_addr_t iobase;
	bus_size_t iosize;
	int rc, sk_nodenum;
	u_int32_t command;
	const char *revstr;
	const struct sysctlnode *node;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sk_dev = self;
	aprint_naive("\n");

	DPRINTFN(2, ("begin skc_attach\n"));

	/*
	 * Handle power management nonsense.
	 */
	command = pci_conf_read(pc, pa->pa_tag, SK_PCI_CAPID) & 0x000000FF;

	if (command == 0x01) {
		command = pci_conf_read(pc, pa->pa_tag, SK_PCI_PWRMGMTCTRL);
		if (command & SK_PSTATE_MASK) {
			u_int32_t		xiobase, membase, irq;

			/* Save important PCI config data. */
			xiobase = pci_conf_read(pc, pa->pa_tag, SK_PCI_LOIO);
			membase = pci_conf_read(pc, pa->pa_tag, SK_PCI_LOMEM);
			irq = pci_conf_read(pc, pa->pa_tag, SK_PCI_INTLINE);

			/* Reset the power state. */
			aprint_normal_dev(sc->sk_dev,
			    "chip is in D%d power mode -- setting to D0\n",
			    command & SK_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(pc, pa->pa_tag,
			    SK_PCI_PWRMGMTCTRL, command);

			/* Restore PCI config data. */
			pci_conf_write(pc, pa->pa_tag, SK_PCI_LOIO, xiobase);
			pci_conf_write(pc, pa->pa_tag, SK_PCI_LOMEM, membase);
			pci_conf_write(pc, pa->pa_tag, SK_PCI_INTLINE, irq);
		}
	}

	/*
	 * The firmware might have configured the interface to revert the
	 * byte order in all descriptors. Make that undone.
	 */
	command = pci_conf_read(pc, pa->pa_tag, SK_PCI_OURREG2);
	if (command & SK_REG2_REV_DESC)
		pci_conf_write(pc, pa->pa_tag, SK_PCI_OURREG2,
		    command & ~SK_REG2_REV_DESC);

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_IO_ENABLE |
	    PCI_COMMAND_MEM_ENABLE |
	    PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

#ifdef SK_USEIOSPACE
	if (!(command & PCI_COMMAND_IO_ENABLE)) {
		aprint_error(": failed to enable I/O ports!\n");
		return;
	}
	/*
	 * Map control/status registers.
	 */
	if (pci_mapreg_map(pa, SK_PCI_LOIO, PCI_MAPREG_TYPE_IO, 0,
			&sc->sk_btag, &sc->sk_bhandle,
			&iobase, &iosize)) {
		aprint_error(": can't find i/o space\n");
		return;
	}
#else
	if (!(command & PCI_COMMAND_MEM_ENABLE)) {
		aprint_error(": failed to enable memory mapping!\n");
		return;
	}
	memtype = pci_mapreg_type(pc, pa->pa_tag, SK_PCI_LOMEM);
	switch (memtype) {
        case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
        case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
                if (pci_mapreg_map(pa, SK_PCI_LOMEM,
				   memtype, 0, &sc->sk_btag, &sc->sk_bhandle,
				   &iobase, &iosize) == 0)
                        break;
        default:
                aprint_error_dev(sc->sk_dev, "can't find mem space\n");
                return;
	}

	DPRINTFN(2, ("skc_attach: iobase=%#" PRIxPADDR ", iosize=%zx\n",
	    iobase, iosize));
#endif
	sc->sc_dmatag = pa->pa_dmat;

	sc->sk_type = sk_win_read_1(sc, SK_CHIPVER);
	sc->sk_rev = (sk_win_read_1(sc, SK_CONFIG) >> 4);

	/* bail out here if chip is not recognized */
	if ( sc->sk_type != SK_GENESIS && ! SK_YUKON_FAMILY(sc->sk_type)) {
		aprint_error_dev(sc->sk_dev, "unknown chip type\n");
		goto fail;
	}
	if (SK_IS_YUKON2(sc)) {
		aprint_error_dev(sc->sk_dev,
		    "Does not support Yukon2--try msk(4).\n");
		goto fail;
	}
	DPRINTFN(2, ("skc_attach: allocate interrupt\n"));

	/* Allocate interrupt */
	if (pci_intr_map(pa, &ih)) {
		aprint_error(": couldn't map interrupt\n");
		goto fail;
	}

	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sk_intrhand = pci_intr_establish(pc, ih, IPL_NET, sk_intr, sc);
	if (sc->sk_intrhand == NULL) {
		aprint_error(": couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		goto fail;
	}
	aprint_normal(": %s\n", intrstr);

	/* Reset the adapter. */
	sk_reset(sc);

	/* Read and save vital product data from EEPROM. */
	sk_vpd_read(sc);

	if (sc->sk_type == SK_GENESIS) {
		u_int8_t val = sk_win_read_1(sc, SK_EPROM0);
		/* Read and save RAM size and RAMbuffer offset */
		switch (val) {
		case SK_RAMSIZE_512K_64:
			sc->sk_ramsize = 0x80000;
			sc->sk_rboff = SK_RBOFF_0;
			break;
		case SK_RAMSIZE_1024K_64:
			sc->sk_ramsize = 0x100000;
			sc->sk_rboff = SK_RBOFF_80000;
			break;
		case SK_RAMSIZE_1024K_128:
			sc->sk_ramsize = 0x100000;
			sc->sk_rboff = SK_RBOFF_0;
			break;
		case SK_RAMSIZE_2048K_128:
			sc->sk_ramsize = 0x200000;
			sc->sk_rboff = SK_RBOFF_0;
			break;
		default:
			aprint_error_dev(sc->sk_dev, "unknown ram size: %d\n",
			       val);
			goto fail_1;
			break;
		}

		DPRINTFN(2, ("skc_attach: ramsize=%d(%dk), rboff=%d\n",
			     sc->sk_ramsize, sc->sk_ramsize / 1024,
			     sc->sk_rboff));
	} else {
	  	u_int8_t val = sk_win_read_1(sc, SK_EPROM0);
		sc->sk_ramsize =  ( val == 0 ) ?  0x20000 : (( val * 4 )*1024);
		sc->sk_rboff = SK_RBOFF_0;

		DPRINTFN(2, ("skc_attach: ramsize=%dk (%d), rboff=%d\n",
			     sc->sk_ramsize / 1024, sc->sk_ramsize,
			     sc->sk_rboff));
	}

	/* Read and save physical media type */
	switch (sk_win_read_1(sc, SK_PMDTYPE)) {
	case SK_PMD_1000BASESX:
		sc->sk_pmd = IFM_1000_SX;
		break;
	case SK_PMD_1000BASELX:
		sc->sk_pmd = IFM_1000_LX;
		break;
	case SK_PMD_1000BASECX:
		sc->sk_pmd = IFM_1000_CX;
		break;
	case SK_PMD_1000BASETX:
	case SK_PMD_1000BASETX_ALT:
		sc->sk_pmd = IFM_1000_T;
		break;
	default:
		aprint_error_dev(sc->sk_dev, "unknown media type: 0x%x\n",
		    sk_win_read_1(sc, SK_PMDTYPE));
		goto fail_1;
	}

	/* determine whether to name it with vpd or just make it up */
	/* Marvell Yukon VPD's can freqently be bogus */

	switch (pa->pa_id) {
	case PCI_ID_CODE(PCI_VENDOR_SCHNEIDERKOCH,
			 PCI_PRODUCT_SCHNEIDERKOCH_SKNET_GE):
	case PCI_PRODUCT_SCHNEIDERKOCH_SK9821v2:
	case PCI_PRODUCT_3COM_3C940:
	case PCI_PRODUCT_DLINK_DGE530T:
	case PCI_PRODUCT_DLINK_DGE560T:
	case PCI_PRODUCT_DLINK_DGE560T_2:
	case PCI_PRODUCT_LINKSYS_EG1032:
	case PCI_PRODUCT_LINKSYS_EG1064:
	case PCI_ID_CODE(PCI_VENDOR_SCHNEIDERKOCH,
			 PCI_PRODUCT_SCHNEIDERKOCH_SK9821v2):
	case PCI_ID_CODE(PCI_VENDOR_3COM,PCI_PRODUCT_3COM_3C940):
	case PCI_ID_CODE(PCI_VENDOR_DLINK,PCI_PRODUCT_DLINK_DGE530T):
	case PCI_ID_CODE(PCI_VENDOR_DLINK,PCI_PRODUCT_DLINK_DGE560T):
	case PCI_ID_CODE(PCI_VENDOR_DLINK,PCI_PRODUCT_DLINK_DGE560T_2):
	case PCI_ID_CODE(PCI_VENDOR_LINKSYS,PCI_PRODUCT_LINKSYS_EG1032):
	case PCI_ID_CODE(PCI_VENDOR_LINKSYS,PCI_PRODUCT_LINKSYS_EG1064):
 		sc->sk_name = sc->sk_vpd_prodname;
 		break;
	case PCI_ID_CODE(PCI_VENDOR_MARVELL,PCI_PRODUCT_MARVELL_SKNET):
	/* whoops yukon vpd prodname bears no resemblance to reality */
		switch (sc->sk_type) {
		case SK_GENESIS:
			sc->sk_name = sc->sk_vpd_prodname;
			break;
		case SK_YUKON:
			sc->sk_name = "Marvell Yukon Gigabit Ethernet";
			break;
		case SK_YUKON_LITE:
			sc->sk_name = "Marvell Yukon Lite Gigabit Ethernet";
			break;
		case SK_YUKON_LP:
			sc->sk_name = "Marvell Yukon LP Gigabit Ethernet";
			break;
		default:
			sc->sk_name = "Marvell Yukon (Unknown) Gigabit Ethernet";
		}

	/* Yukon Lite Rev A0 needs special test, from sk98lin driver */

		if ( sc->sk_type == SK_YUKON ) {
			uint32_t flashaddr;
			uint8_t testbyte;

			flashaddr = sk_win_read_4(sc,SK_EP_ADDR);

			/* test Flash-Address Register */
			sk_win_write_1(sc,SK_EP_ADDR+3, 0xff);
			testbyte = sk_win_read_1(sc, SK_EP_ADDR+3);

			if (testbyte != 0) {
				/* this is yukon lite Rev. A0 */
				sc->sk_type = SK_YUKON_LITE;
				sc->sk_rev = SK_YUKON_LITE_REV_A0;
				/* restore Flash-Address Register */
				sk_win_write_4(sc,SK_EP_ADDR,flashaddr);
			}
		}
		break;
	case PCI_ID_CODE(PCI_VENDOR_MARVELL,PCI_PRODUCT_MARVELL_BELKIN):
		sc->sk_name = sc->sk_vpd_prodname;
		break;
 	default:
		sc->sk_name = "Unknown Marvell";
	}


	if ( sc->sk_type == SK_YUKON_LITE ) {
		switch (sc->sk_rev) {
		case SK_YUKON_LITE_REV_A0:
			revstr = "A0";
			break;
		case SK_YUKON_LITE_REV_A1:
			revstr = "A1";
			break;
		case SK_YUKON_LITE_REV_A3:
			revstr = "A3";
			break;
		default:
			revstr = "";
		}
	} else {
		revstr = "";
	}

	/* Announce the product name. */
	aprint_normal_dev(sc->sk_dev, "%s rev. %s(0x%x)\n",
			      sc->sk_name, revstr, sc->sk_rev);

	skca.skc_port = SK_PORT_A;
	(void)config_found(sc->sk_dev, &skca, skcprint);

	if (!(sk_win_read_1(sc, SK_CONFIG) & SK_CONFIG_SINGLEMAC)) {
		skca.skc_port = SK_PORT_B;
		(void)config_found(sc->sk_dev, &skca, skcprint);
	}

	/* Turn on the 'driver is loaded' LED. */
	CSR_WRITE_2(sc, SK_LED, SK_LED_GREEN_ON);

	/* skc sysctl setup */

	sc->sk_int_mod = SK_IM_DEFAULT;
	sc->sk_int_mod_pending = 0;

	if ((rc = sysctl_createv(&sc->sk_clog, 0, NULL, &node,
	    0, CTLTYPE_NODE, device_xname(sc->sk_dev),
	    SYSCTL_DESCR("skc per-controller controls"),
	    NULL, 0, NULL, 0, CTL_HW, sk_root_num, CTL_CREATE,
	    CTL_EOL)) != 0) {
		aprint_normal_dev(sc->sk_dev, "couldn't create sysctl node\n");
		goto fail_1;
	}

	sk_nodenum = node->sysctl_num;

	/* interrupt moderation time in usecs */
	if ((rc = sysctl_createv(&sc->sk_clog, 0, NULL, &node,
	    CTLFLAG_READWRITE,
	    CTLTYPE_INT, "int_mod",
	    SYSCTL_DESCR("sk interrupt moderation timer"),
	    sk_sysctl_handler, 0, (void *)sc,
	    0, CTL_HW, sk_root_num, sk_nodenum, CTL_CREATE,
	    CTL_EOL)) != 0) {
		aprint_normal_dev(sc->sk_dev, "couldn't create int_mod sysctl node\n");
		goto fail_1;
	}

	if (!pmf_device_register(self, skc_suspend, skc_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;

fail_1:
	pci_intr_disestablish(pc, sc->sk_intrhand);
fail:
	bus_space_unmap(sc->sk_btag, sc->sk_bhandle, iosize);
}

int
sk_encap(struct sk_if_softc *sc_if, struct mbuf *m_head, u_int32_t *txidx)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	struct sk_tx_desc	*f = NULL;
	u_int32_t		frag, cur, cnt = 0, sk_ctl;
	int			i;
	struct sk_txmap_entry	*entry;
	bus_dmamap_t		txmap;

	DPRINTFN(3, ("sk_encap\n"));

	entry = SIMPLEQ_FIRST(&sc_if->sk_txmap_head);
	if (entry == NULL) {
		DPRINTFN(3, ("sk_encap: no txmap available\n"));
		return ENOBUFS;
	}
	txmap = entry->dmamap;

	cur = frag = *txidx;

#ifdef SK_DEBUG
	if (skdebug >= 3)
		sk_dump_mbuf(m_head);
#endif

	/*
	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
	 * of fragments or hit the end of the mbuf chain.
	 */
	if (bus_dmamap_load_mbuf(sc->sc_dmatag, txmap, m_head,
	    BUS_DMA_NOWAIT)) {
		DPRINTFN(1, ("sk_encap: dmamap failed\n"));
		return ENOBUFS;
	}

	DPRINTFN(3, ("sk_encap: dm_nsegs=%d\n", txmap->dm_nsegs));

	/* Sync the DMA map. */
	bus_dmamap_sync(sc->sc_dmatag, txmap, 0, txmap->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	for (i = 0; i < txmap->dm_nsegs; i++) {
		if ((SK_TX_RING_CNT - (sc_if->sk_cdata.sk_tx_cnt + cnt)) < 2) {
			DPRINTFN(1, ("sk_encap: too few descriptors free\n"));
			return ENOBUFS;
		}
		f = &sc_if->sk_rdata->sk_tx_ring[frag];
		f->sk_data_lo = htole32(txmap->dm_segs[i].ds_addr);
		sk_ctl = txmap->dm_segs[i].ds_len | SK_OPCODE_DEFAULT;
		if (cnt == 0)
			sk_ctl |= SK_TXCTL_FIRSTFRAG;
		else
			sk_ctl |= SK_TXCTL_OWN;
		f->sk_ctl = htole32(sk_ctl);
		cur = frag;
		SK_INC(frag, SK_TX_RING_CNT);
		cnt++;
	}

	sc_if->sk_cdata.sk_tx_chain[cur].sk_mbuf = m_head;
	SIMPLEQ_REMOVE_HEAD(&sc_if->sk_txmap_head, link);

	sc_if->sk_cdata.sk_tx_map[cur] = entry;
	sc_if->sk_rdata->sk_tx_ring[cur].sk_ctl |=
		htole32(SK_TXCTL_LASTFRAG|SK_TXCTL_EOF_INTR);

	/* Sync descriptors before handing to chip */
	SK_CDTXSYNC(sc_if, *txidx, txmap->dm_nsegs,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	sc_if->sk_rdata->sk_tx_ring[*txidx].sk_ctl |=
		htole32(SK_TXCTL_OWN);

	/* Sync first descriptor to hand it off */
	SK_CDTXSYNC(sc_if, *txidx, 1, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	sc_if->sk_cdata.sk_tx_cnt += cnt;

#ifdef SK_DEBUG
	if (skdebug >= 3) {
		struct sk_tx_desc *desc;
		u_int32_t idx;
		for (idx = *txidx; idx != frag; SK_INC(idx, SK_TX_RING_CNT)) {
			desc = &sc_if->sk_rdata->sk_tx_ring[idx];
			sk_dump_txdesc(desc, idx);
		}
	}
#endif

	*txidx = frag;

	DPRINTFN(3, ("sk_encap: completed successfully\n"));

	return 0;
}

void
sk_start(struct ifnet *ifp)
{
        struct sk_if_softc	*sc_if = ifp->if_softc;
        struct sk_softc		*sc = sc_if->sk_softc;
        struct mbuf		*m_head = NULL;
        u_int32_t		idx = sc_if->sk_cdata.sk_tx_prod;
	int			pkts = 0;

	DPRINTFN(3, ("sk_start (idx %d, tx_chain[idx] %p)\n", idx,
		sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf));

	while (sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf == NULL) {
		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (sk_encap(sc_if, m_head, &idx)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/* now we are committed to transmit the packet */
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		pkts++;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		bpf_mtap(ifp, m_head);
	}
	if (pkts == 0)
		return;

	/* Transmit */
	if (idx != sc_if->sk_cdata.sk_tx_prod) {
		sc_if->sk_cdata.sk_tx_prod = idx;
		CSR_WRITE_4(sc, sc_if->sk_tx_bmu, SK_TXBMU_TX_START);

		/* Set a timeout in case the chip goes out to lunch. */
		ifp->if_timer = 5;
	}
}


void
sk_watchdog(struct ifnet *ifp)
{
	struct sk_if_softc *sc_if = ifp->if_softc;

	/*
	 * Reclaim first as there is a possibility of losing Tx completion
	 * interrupts.
	 */
	sk_txeof(sc_if);
	if (sc_if->sk_cdata.sk_tx_cnt != 0) {
		aprint_error_dev(sc_if->sk_dev, "watchdog timeout\n");

		ifp->if_oerrors++;

		sk_init(ifp);
	}
}

void
sk_shutdown(void *v)
{
	struct sk_if_softc	*sc_if = (struct sk_if_softc *)v;
	struct sk_softc		*sc = sc_if->sk_softc;
	struct ifnet 		*ifp = &sc_if->sk_ethercom.ec_if;

	DPRINTFN(2, ("sk_shutdown\n"));
	sk_stop(ifp,1);

	/* Turn off the 'driver is loaded' LED. */
	CSR_WRITE_2(sc, SK_LED, SK_LED_GREEN_OFF);

	/*
	 * Reset the GEnesis controller. Doing this should also
	 * assert the resets on the attached XMAC(s).
	 */
	sk_reset(sc);
}

void
sk_rxeof(struct sk_if_softc *sc_if)
{
	struct ifnet		*ifp = &sc_if->sk_ethercom.ec_if;
	struct mbuf		*m;
	struct sk_chain		*cur_rx;
	struct sk_rx_desc	*cur_desc;
	int			i, cur, total_len = 0;
	u_int32_t		rxstat, sk_ctl;
	bus_dmamap_t		dmamap;

	i = sc_if->sk_cdata.sk_rx_prod;

	DPRINTFN(3, ("sk_rxeof %d\n", i));

	for (;;) {
		cur = i;

		/* Sync the descriptor */
		SK_CDRXSYNC(sc_if, cur,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		sk_ctl = le32toh(sc_if->sk_rdata->sk_rx_ring[cur].sk_ctl);
		if (sk_ctl & SK_RXCTL_OWN) {
			/* Invalidate the descriptor -- it's not ready yet */
			SK_CDRXSYNC(sc_if, cur, BUS_DMASYNC_PREREAD);
			sc_if->sk_cdata.sk_rx_prod = i;
			break;
		}

		cur_rx = &sc_if->sk_cdata.sk_rx_chain[cur];
		cur_desc = &sc_if->sk_rdata->sk_rx_ring[cur];
		dmamap = sc_if->sk_cdata.sk_rx_jumbo_map;

		bus_dmamap_sync(sc_if->sk_softc->sc_dmatag, dmamap, 0,
		    dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		rxstat = le32toh(cur_desc->sk_xmac_rxstat);
		m = cur_rx->sk_mbuf;
		cur_rx->sk_mbuf = NULL;
		total_len = SK_RXBYTES(le32toh(cur_desc->sk_ctl));

		sc_if->sk_cdata.sk_rx_map[cur] = 0;

		SK_INC(i, SK_RX_RING_CNT);

		if (rxstat & XM_RXSTAT_ERRFRAME) {
			ifp->if_ierrors++;
			sk_newbuf(sc_if, cur, m, dmamap);
			continue;
		}

		/*
		 * Try to allocate a new jumbo buffer. If that
		 * fails, copy the packet to mbufs and put the
		 * jumbo buffer back in the ring so it can be
		 * re-used. If allocating mbufs fails, then we
		 * have to drop the packet.
		 */
		if (sk_newbuf(sc_if, cur, NULL, dmamap) == ENOBUFS) {
			struct mbuf		*m0;
			m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
			    total_len + ETHER_ALIGN, 0, ifp, NULL);
			sk_newbuf(sc_if, cur, m, dmamap);
			if (m0 == NULL) {
				aprint_error_dev(sc_if->sk_dev, "no receive "
				    "buffers available -- packet dropped!\n");
				ifp->if_ierrors++;
				continue;
			}
			m_adj(m0, ETHER_ALIGN);
			m = m0;
		} else {
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = m->m_len = total_len;
		}

		ifp->if_ipackets++;

		bpf_mtap(ifp, m);
		/* pass it on. */
		(*ifp->if_input)(ifp, m);
	}
}

void
sk_txeof(struct sk_if_softc *sc_if)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	struct sk_tx_desc	*cur_tx;
	struct ifnet		*ifp = &sc_if->sk_ethercom.ec_if;
	u_int32_t		idx, sk_ctl;
	struct sk_txmap_entry	*entry;

	DPRINTFN(3, ("sk_txeof\n"));

	/*
	 * Go through our tx ring and free mbufs for those
	 * frames that have been sent.
	 */
	idx = sc_if->sk_cdata.sk_tx_cons;
	while (idx != sc_if->sk_cdata.sk_tx_prod) {
		SK_CDTXSYNC(sc_if, idx, 1,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		cur_tx = &sc_if->sk_rdata->sk_tx_ring[idx];
		sk_ctl = le32toh(cur_tx->sk_ctl);
#ifdef SK_DEBUG
		if (skdebug >= 3)
			sk_dump_txdesc(cur_tx, idx);
#endif
		if (sk_ctl & SK_TXCTL_OWN) {
			SK_CDTXSYNC(sc_if, idx, 1, BUS_DMASYNC_PREREAD);
			break;
		}
		if (sk_ctl & SK_TXCTL_LASTFRAG)
			ifp->if_opackets++;
		if (sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf != NULL) {
			entry = sc_if->sk_cdata.sk_tx_map[idx];

			m_freem(sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf);
			sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf = NULL;

			bus_dmamap_sync(sc->sc_dmatag, entry->dmamap, 0,
			    entry->dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);

			bus_dmamap_unload(sc->sc_dmatag, entry->dmamap);
			SIMPLEQ_INSERT_TAIL(&sc_if->sk_txmap_head, entry,
					  link);
			sc_if->sk_cdata.sk_tx_map[idx] = NULL;
		}
		sc_if->sk_cdata.sk_tx_cnt--;
		SK_INC(idx, SK_TX_RING_CNT);
	}
	if (sc_if->sk_cdata.sk_tx_cnt == 0)
		ifp->if_timer = 0;
	else /* nudge chip to keep tx ring moving */
		CSR_WRITE_4(sc, sc_if->sk_tx_bmu, SK_TXBMU_TX_START);

	if (sc_if->sk_cdata.sk_tx_cnt < SK_TX_RING_CNT - 2)
		ifp->if_flags &= ~IFF_OACTIVE;

	sc_if->sk_cdata.sk_tx_cons = idx;
}

void
sk_tick(void *xsc_if)
{
	struct sk_if_softc *sc_if = xsc_if;
	struct mii_data *mii = &sc_if->sk_mii;
	struct ifnet *ifp = &sc_if->sk_ethercom.ec_if;
	int i;

	DPRINTFN(3, ("sk_tick\n"));

	if (!(ifp->if_flags & IFF_UP))
		return;

	if (sc_if->sk_phytype == SK_PHYTYPE_BCOM) {
		sk_intr_bcom(sc_if);
		return;
	}

	/*
	 * According to SysKonnect, the correct way to verify that
	 * the link has come back up is to poll bit 0 of the GPIO
	 * register three times. This pin has the signal from the
	 * link sync pin connected to it; if we read the same link
	 * state 3 times in a row, we know the link is up.
	 */
	for (i = 0; i < 3; i++) {
		if (SK_XM_READ_2(sc_if, XM_GPIO) & XM_GPIO_GP0_SET)
			break;
	}

	if (i != 3) {
		callout_reset(&sc_if->sk_tick_ch, hz, sk_tick, sc_if);
		return;
	}

	/* Turn the GP0 interrupt back on. */
	SK_XM_CLRBIT_2(sc_if, XM_IMR, XM_IMR_GP0_SET);
	SK_XM_READ_2(sc_if, XM_ISR);
	mii_tick(mii);
	if (ifp->if_link_state != LINK_STATE_UP)
		callout_reset(&sc_if->sk_tick_ch, hz, sk_tick, sc_if);
	else
		callout_stop(&sc_if->sk_tick_ch);
}

void
sk_intr_bcom(struct sk_if_softc *sc_if)
{
	struct mii_data *mii = &sc_if->sk_mii;
	struct ifnet *ifp = &sc_if->sk_ethercom.ec_if;
	int status;


	DPRINTFN(3, ("sk_intr_bcom\n"));

	SK_XM_CLRBIT_2(sc_if, XM_MMUCMD, XM_MMUCMD_TX_ENB|XM_MMUCMD_RX_ENB);

	/*
	 * Read the PHY interrupt register to make sure
	 * we clear any pending interrupts.
	 */
	status = sk_xmac_miibus_readreg(sc_if->sk_dev,
	    SK_PHYADDR_BCOM, BRGPHY_MII_ISR);

	if (!(ifp->if_flags & IFF_RUNNING)) {
		sk_init_xmac(sc_if);
		return;
	}

	if (status & (BRGPHY_ISR_LNK_CHG|BRGPHY_ISR_AN_PR)) {
		int lstat;
		lstat = sk_xmac_miibus_readreg(sc_if->sk_dev,
		    SK_PHYADDR_BCOM, BRGPHY_MII_AUXSTS);

		if (!(lstat & BRGPHY_AUXSTS_LINK) && sc_if->sk_link) {
			(void)mii_mediachg(mii);
			/* Turn off the link LED. */
			SK_IF_WRITE_1(sc_if, 0,
			    SK_LINKLED1_CTL, SK_LINKLED_OFF);
			sc_if->sk_link = 0;
		} else if (status & BRGPHY_ISR_LNK_CHG) {
			sk_xmac_miibus_writereg(sc_if->sk_dev,
			    SK_PHYADDR_BCOM, BRGPHY_MII_IMR, 0xFF00);
			mii_tick(mii);
			sc_if->sk_link = 1;
			/* Turn on the link LED. */
			SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL,
			    SK_LINKLED_ON|SK_LINKLED_LINKSYNC_OFF|
			    SK_LINKLED_BLINK_OFF);
			mii_pollstat(mii);
		} else {
			mii_tick(mii);
			callout_reset(&sc_if->sk_tick_ch, hz, sk_tick,sc_if);
		}
	}

	SK_XM_SETBIT_2(sc_if, XM_MMUCMD, XM_MMUCMD_TX_ENB|XM_MMUCMD_RX_ENB);
}

void
sk_intr_xmac(struct sk_if_softc	*sc_if)
{
	u_int16_t status = SK_XM_READ_2(sc_if, XM_ISR);

	DPRINTFN(3, ("sk_intr_xmac\n"));

	if (sc_if->sk_phytype == SK_PHYTYPE_XMAC) {
		if (status & XM_ISR_GP0_SET) {
			SK_XM_SETBIT_2(sc_if, XM_IMR, XM_IMR_GP0_SET);
			callout_reset(&sc_if->sk_tick_ch, hz, sk_tick, sc_if);
		}

		if (status & XM_ISR_AUTONEG_DONE) {
			callout_reset(&sc_if->sk_tick_ch, hz, sk_tick, sc_if);
		}
	}

	if (status & XM_IMR_TX_UNDERRUN)
		SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_FLUSH_TXFIFO);

	if (status & XM_IMR_RX_OVERRUN)
		SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_FLUSH_RXFIFO);
}

void
sk_intr_yukon(struct sk_if_softc *sc_if)
{
#ifdef SK_DEBUG
	int status;

	status = 
#endif
		SK_IF_READ_2(sc_if, 0, SK_GMAC_ISR);

	DPRINTFN(3, ("sk_intr_yukon status=%#x\n", status));
}

int
sk_intr(void *xsc)
{
	struct sk_softc		*sc = xsc;
	struct sk_if_softc	*sc_if0 = sc->sk_if[SK_PORT_A];
	struct sk_if_softc	*sc_if1 = sc->sk_if[SK_PORT_B];
	struct ifnet		*ifp0 = NULL, *ifp1 = NULL;
	u_int32_t		status;
	int			claimed = 0;

	if (sc_if0 != NULL)
		ifp0 = &sc_if0->sk_ethercom.ec_if;
	if (sc_if1 != NULL)
		ifp1 = &sc_if1->sk_ethercom.ec_if;

	for (;;) {
		status = CSR_READ_4(sc, SK_ISSR);
		DPRINTFN(3, ("sk_intr: status=%#x\n", status));

		if (!(status & sc->sk_intrmask))
			break;

		claimed = 1;

		/* Handle receive interrupts first. */
		if (sc_if0 && (status & SK_ISR_RX1_EOF)) {
			sk_rxeof(sc_if0);
			CSR_WRITE_4(sc, SK_BMU_RX_CSR0,
			    SK_RXBMU_CLR_IRQ_EOF|SK_RXBMU_RX_START);
		}
		if (sc_if1 && (status & SK_ISR_RX2_EOF)) {
			sk_rxeof(sc_if1);
			CSR_WRITE_4(sc, SK_BMU_RX_CSR1,
			    SK_RXBMU_CLR_IRQ_EOF|SK_RXBMU_RX_START);
		}

		/* Then transmit interrupts. */
		if (sc_if0 && (status & SK_ISR_TX1_S_EOF)) {
			sk_txeof(sc_if0);
			CSR_WRITE_4(sc, SK_BMU_TXS_CSR0,
			    SK_TXBMU_CLR_IRQ_EOF);
		}
		if (sc_if1 && (status & SK_ISR_TX2_S_EOF)) {
			sk_txeof(sc_if1);
			CSR_WRITE_4(sc, SK_BMU_TXS_CSR1,
			    SK_TXBMU_CLR_IRQ_EOF);
		}

		/* Then MAC interrupts. */
		if (sc_if0 && (status & SK_ISR_MAC1) &&
		    (ifp0->if_flags & IFF_RUNNING)) {
			if (sc->sk_type == SK_GENESIS)
				sk_intr_xmac(sc_if0);
			else
				sk_intr_yukon(sc_if0);
		}

		if (sc_if1 && (status & SK_ISR_MAC2) &&
		    (ifp1->if_flags & IFF_RUNNING)) {
			if (sc->sk_type == SK_GENESIS)
				sk_intr_xmac(sc_if1);
			else
				sk_intr_yukon(sc_if1);

		}

		if (status & SK_ISR_EXTERNAL_REG) {
			if (sc_if0 != NULL &&
			    sc_if0->sk_phytype == SK_PHYTYPE_BCOM)
				sk_intr_bcom(sc_if0);

			if (sc_if1 != NULL &&
			    sc_if1->sk_phytype == SK_PHYTYPE_BCOM)
				sk_intr_bcom(sc_if1);
		}
	}

	CSR_WRITE_4(sc, SK_IMR, sc->sk_intrmask);

	if (ifp0 != NULL && !IFQ_IS_EMPTY(&ifp0->if_snd))
		sk_start(ifp0);
	if (ifp1 != NULL && !IFQ_IS_EMPTY(&ifp1->if_snd))
		sk_start(ifp1);

	rnd_add_uint32(&sc->rnd_source, status);

	if (sc->sk_int_mod_pending)
		sk_update_int_mod(sc);

	return claimed;
}

void
sk_init_xmac(struct sk_if_softc	*sc_if)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	struct ifnet		*ifp = &sc_if->sk_ethercom.ec_if;
	static const struct sk_bcom_hack     bhack[] = {
	{ 0x18, 0x0c20 }, { 0x17, 0x0012 }, { 0x15, 0x1104 }, { 0x17, 0x0013 },
	{ 0x15, 0x0404 }, { 0x17, 0x8006 }, { 0x15, 0x0132 }, { 0x17, 0x8006 },
	{ 0x15, 0x0232 }, { 0x17, 0x800D }, { 0x15, 0x000F }, { 0x18, 0x0420 },
	{ 0, 0 } };

	DPRINTFN(1, ("sk_init_xmac\n"));

	/* Unreset the XMAC. */
	SK_IF_WRITE_2(sc_if, 0, SK_TXF1_MACCTL, SK_TXMACCTL_XMAC_UNRESET);
	DELAY(1000);

	/* Reset the XMAC's internal state. */
	SK_XM_SETBIT_2(sc_if, XM_GPIO, XM_GPIO_RESETMAC);

	/* Save the XMAC II revision */
	sc_if->sk_xmac_rev = XM_XMAC_REV(SK_XM_READ_4(sc_if, XM_DEVID));

	/*
	 * Perform additional initialization for external PHYs,
	 * namely for the 1000baseTX cards that use the XMAC's
	 * GMII mode.
	 */
	if (sc_if->sk_phytype == SK_PHYTYPE_BCOM) {
		int			i = 0;
		u_int32_t		val;

		/* Take PHY out of reset. */
		val = sk_win_read_4(sc, SK_GPIO);
		if (sc_if->sk_port == SK_PORT_A)
			val |= SK_GPIO_DIR0|SK_GPIO_DAT0;
		else
			val |= SK_GPIO_DIR2|SK_GPIO_DAT2;
		sk_win_write_4(sc, SK_GPIO, val);

		/* Enable GMII mode on the XMAC. */
		SK_XM_SETBIT_2(sc_if, XM_HWCFG, XM_HWCFG_GMIIMODE);

		sk_xmac_miibus_writereg(sc_if->sk_dev,
		    SK_PHYADDR_BCOM, MII_BMCR, BMCR_RESET);
		DELAY(10000);
		sk_xmac_miibus_writereg(sc_if->sk_dev,
		    SK_PHYADDR_BCOM, BRGPHY_MII_IMR, 0xFFF0);

		/*
		 * Early versions of the BCM5400 apparently have
		 * a bug that requires them to have their reserved
		 * registers initialized to some magic values. I don't
		 * know what the numbers do, I'm just the messenger.
		 */
		if (sk_xmac_miibus_readreg(sc_if->sk_dev,
		    SK_PHYADDR_BCOM, 0x03) == 0x6041) {
			while (bhack[i].reg) {
				sk_xmac_miibus_writereg(sc_if->sk_dev,
				    SK_PHYADDR_BCOM, bhack[i].reg,
				    bhack[i].val);
				i++;
			}
		}
	}

	/* Set station address */
	SK_XM_WRITE_2(sc_if, XM_PAR0,
		      *(u_int16_t *)(&sc_if->sk_enaddr[0]));
	SK_XM_WRITE_2(sc_if, XM_PAR1,
		      *(u_int16_t *)(&sc_if->sk_enaddr[2]));
	SK_XM_WRITE_2(sc_if, XM_PAR2,
		      *(u_int16_t *)(&sc_if->sk_enaddr[4]));
	SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_USE_STATION);

	if (ifp->if_flags & IFF_PROMISC)
		SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_PROMISC);
	else
		SK_XM_CLRBIT_4(sc_if, XM_MODE, XM_MODE_RX_PROMISC);

	if (ifp->if_flags & IFF_BROADCAST)
		SK_XM_CLRBIT_4(sc_if, XM_MODE, XM_MODE_RX_NOBROAD);
	else
		SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_NOBROAD);

	/* We don't need the FCS appended to the packet. */
	SK_XM_SETBIT_2(sc_if, XM_RXCMD, XM_RXCMD_STRIPFCS);

	/* We want short frames padded to 60 bytes. */
	SK_XM_SETBIT_2(sc_if, XM_TXCMD, XM_TXCMD_AUTOPAD);

	/*
	 * Enable the reception of all error frames. This is is
	 * a necessary evil due to the design of the XMAC. The
	 * XMAC's receive FIFO is only 8K in size, however jumbo
	 * frames can be up to 9000 bytes in length. When bad
	 * frame filtering is enabled, the XMAC's RX FIFO operates
	 * in 'store and forward' mode. For this to work, the
	 * entire frame has to fit into the FIFO, but that means
	 * that jumbo frames larger than 8192 bytes will be
	 * truncated. Disabling all bad frame filtering causes
	 * the RX FIFO to operate in streaming mode, in which
	 * case the XMAC will start transfering frames out of the
	 * RX FIFO as soon as the FIFO threshold is reached.
	 */
	SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_BADFRAMES|
	    XM_MODE_RX_GIANTS|XM_MODE_RX_RUNTS|XM_MODE_RX_CRCERRS|
	    XM_MODE_RX_INRANGELEN);

	if (ifp->if_mtu > (ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN))
		SK_XM_SETBIT_2(sc_if, XM_RXCMD, XM_RXCMD_BIGPKTOK);
	else
		SK_XM_CLRBIT_2(sc_if, XM_RXCMD, XM_RXCMD_BIGPKTOK);

	/*
	 * Bump up the transmit threshold. This helps hold off transmit
	 * underruns when we're blasting traffic from both ports at once.
	 */
	SK_XM_WRITE_2(sc_if, XM_TX_REQTHRESH, SK_XM_TX_FIFOTHRESH);

	/* Set multicast filter */
	sk_setmulti(sc_if);

	/* Clear and enable interrupts */
	SK_XM_READ_2(sc_if, XM_ISR);
	if (sc_if->sk_phytype == SK_PHYTYPE_XMAC)
		SK_XM_WRITE_2(sc_if, XM_IMR, XM_INTRS);
	else
		SK_XM_WRITE_2(sc_if, XM_IMR, 0xFFFF);

	/* Configure MAC arbiter */
	switch (sc_if->sk_xmac_rev) {
	case XM_XMAC_REV_B2:
		sk_win_write_1(sc, SK_RCINIT_RX1, SK_RCINIT_XMAC_B2);
		sk_win_write_1(sc, SK_RCINIT_TX1, SK_RCINIT_XMAC_B2);
		sk_win_write_1(sc, SK_RCINIT_RX2, SK_RCINIT_XMAC_B2);
		sk_win_write_1(sc, SK_RCINIT_TX2, SK_RCINIT_XMAC_B2);
		sk_win_write_1(sc, SK_MINIT_RX1, SK_MINIT_XMAC_B2);
		sk_win_write_1(sc, SK_MINIT_TX1, SK_MINIT_XMAC_B2);
		sk_win_write_1(sc, SK_MINIT_RX2, SK_MINIT_XMAC_B2);
		sk_win_write_1(sc, SK_MINIT_TX2, SK_MINIT_XMAC_B2);
		sk_win_write_1(sc, SK_RECOVERY_CTL, SK_RECOVERY_XMAC_B2);
		break;
	case XM_XMAC_REV_C1:
		sk_win_write_1(sc, SK_RCINIT_RX1, SK_RCINIT_XMAC_C1);
		sk_win_write_1(sc, SK_RCINIT_TX1, SK_RCINIT_XMAC_C1);
		sk_win_write_1(sc, SK_RCINIT_RX2, SK_RCINIT_XMAC_C1);
		sk_win_write_1(sc, SK_RCINIT_TX2, SK_RCINIT_XMAC_C1);
		sk_win_write_1(sc, SK_MINIT_RX1, SK_MINIT_XMAC_C1);
		sk_win_write_1(sc, SK_MINIT_TX1, SK_MINIT_XMAC_C1);
		sk_win_write_1(sc, SK_MINIT_RX2, SK_MINIT_XMAC_C1);
		sk_win_write_1(sc, SK_MINIT_TX2, SK_MINIT_XMAC_C1);
		sk_win_write_1(sc, SK_RECOVERY_CTL, SK_RECOVERY_XMAC_B2);
		break;
	default:
		break;
	}
	sk_win_write_2(sc, SK_MACARB_CTL,
	    SK_MACARBCTL_UNRESET|SK_MACARBCTL_FASTOE_OFF);

	sc_if->sk_link = 1;
}

void sk_init_yukon(struct sk_if_softc *sc_if)
{
	u_int32_t		/*mac, */phy;
	u_int16_t		reg;
	struct sk_softc		*sc;
	int			i;

	DPRINTFN(1, ("sk_init_yukon: start: sk_csr=%#x\n",
		     CSR_READ_4(sc_if->sk_softc, SK_CSR)));

	sc = sc_if->sk_softc;
	if (sc->sk_type == SK_YUKON_LITE &&
	    sc->sk_rev >= SK_YUKON_LITE_REV_A3) {
		/* Take PHY out of reset. */
		sk_win_write_4(sc, SK_GPIO,
			(sk_win_read_4(sc, SK_GPIO) | SK_GPIO_DIR9) & ~SK_GPIO_DAT9);
	}


	/* GMAC and GPHY Reset */
	SK_IF_WRITE_4(sc_if, 0, SK_GPHY_CTRL, SK_GPHY_RESET_SET);

	DPRINTFN(6, ("sk_init_yukon: 1\n"));

	SK_IF_WRITE_4(sc_if, 0, SK_GMAC_CTRL, SK_GMAC_RESET_SET);
	DELAY(1000);
	SK_IF_WRITE_4(sc_if, 0, SK_GMAC_CTRL, SK_GMAC_RESET_CLEAR);
	SK_IF_WRITE_4(sc_if, 0, SK_GMAC_CTRL, SK_GMAC_RESET_SET);
	DELAY(1000);


	DPRINTFN(6, ("sk_init_yukon: 2\n"));

	phy = SK_GPHY_INT_POL_HI | SK_GPHY_DIS_FC | SK_GPHY_DIS_SLEEP |
		SK_GPHY_ENA_XC | SK_GPHY_ANEG_ALL | SK_GPHY_ENA_PAUSE;

	switch (sc_if->sk_softc->sk_pmd) {
	case IFM_1000_SX:
	case IFM_1000_LX:
		phy |= SK_GPHY_FIBER;
		break;

	case IFM_1000_CX:
	case IFM_1000_T:
		phy |= SK_GPHY_COPPER;
		break;
	}

	DPRINTFN(3, ("sk_init_yukon: phy=%#x\n", phy));

	SK_IF_WRITE_4(sc_if, 0, SK_GPHY_CTRL, phy | SK_GPHY_RESET_SET);
	DELAY(1000);
	SK_IF_WRITE_4(sc_if, 0, SK_GPHY_CTRL, phy | SK_GPHY_RESET_CLEAR);
	SK_IF_WRITE_4(sc_if, 0, SK_GMAC_CTRL, SK_GMAC_LOOP_OFF |
		      SK_GMAC_PAUSE_ON | SK_GMAC_RESET_CLEAR);

	DPRINTFN(3, ("sk_init_yukon: gmac_ctrl=%#x\n",
		     SK_IF_READ_4(sc_if, 0, SK_GMAC_CTRL)));

	DPRINTFN(6, ("sk_init_yukon: 3\n"));

	/* unused read of the interrupt source register */
	DPRINTFN(6, ("sk_init_yukon: 4\n"));
	SK_IF_READ_2(sc_if, 0, SK_GMAC_ISR);

	DPRINTFN(6, ("sk_init_yukon: 4a\n"));
	reg = SK_YU_READ_2(sc_if, YUKON_PAR);
	DPRINTFN(6, ("sk_init_yukon: YUKON_PAR=%#x\n", reg));

	/* MIB Counter Clear Mode set */
        reg |= YU_PAR_MIB_CLR;
	DPRINTFN(6, ("sk_init_yukon: YUKON_PAR=%#x\n", reg));
	DPRINTFN(6, ("sk_init_yukon: 4b\n"));
	SK_YU_WRITE_2(sc_if, YUKON_PAR, reg);

	/* MIB Counter Clear Mode clear */
	DPRINTFN(6, ("sk_init_yukon: 5\n"));
        reg &= ~YU_PAR_MIB_CLR;
	SK_YU_WRITE_2(sc_if, YUKON_PAR, reg);

	/* receive control reg */
	DPRINTFN(6, ("sk_init_yukon: 7\n"));
	SK_YU_WRITE_2(sc_if, YUKON_RCR, YU_RCR_UFLEN | YU_RCR_MUFLEN |
		      YU_RCR_CRCR);

	/* transmit parameter register */
	DPRINTFN(6, ("sk_init_yukon: 8\n"));
	SK_YU_WRITE_2(sc_if, YUKON_TPR, YU_TPR_JAM_LEN(0x3) |
		      YU_TPR_JAM_IPG(0xb) | YU_TPR_JAM2DATA_IPG(0x1a) );

	/* serial mode register */
	DPRINTFN(6, ("sk_init_yukon: 9\n"));
	SK_YU_WRITE_2(sc_if, YUKON_SMR, YU_SMR_DATA_BLIND(0x1c) |
		      YU_SMR_MFL_VLAN | YU_SMR_MFL_JUMBO |
		      YU_SMR_IPG_DATA(0x1e));

	DPRINTFN(6, ("sk_init_yukon: 10\n"));
	/* Setup Yukon's address */
	for (i = 0; i < 3; i++) {
		/* Write Source Address 1 (unicast filter) */
		SK_YU_WRITE_2(sc_if, YUKON_SAL1 + i * 4,
			      sc_if->sk_enaddr[i * 2] |
			      sc_if->sk_enaddr[i * 2 + 1] << 8);
	}

	for (i = 0; i < 3; i++) {
		reg = sk_win_read_2(sc_if->sk_softc,
				    SK_MAC1_0 + i * 2 + sc_if->sk_port * 8);
		SK_YU_WRITE_2(sc_if, YUKON_SAL2 + i * 4, reg);
	}

	/* Set multicast filter */
	DPRINTFN(6, ("sk_init_yukon: 11\n"));
	sk_setmulti(sc_if);

	/* enable interrupt mask for counter overflows */
	DPRINTFN(6, ("sk_init_yukon: 12\n"));
	SK_YU_WRITE_2(sc_if, YUKON_TIMR, 0);
	SK_YU_WRITE_2(sc_if, YUKON_RIMR, 0);
	SK_YU_WRITE_2(sc_if, YUKON_TRIMR, 0);

	/* Configure RX MAC FIFO */
	SK_IF_WRITE_1(sc_if, 0, SK_RXMF1_CTRL_TEST, SK_RFCTL_RESET_CLEAR);
	SK_IF_WRITE_4(sc_if, 0, SK_RXMF1_CTRL_TEST, SK_RFCTL_OPERATION_ON);

	/* Configure TX MAC FIFO */
	SK_IF_WRITE_1(sc_if, 0, SK_TXMF1_CTRL_TEST, SK_TFCTL_RESET_CLEAR);
	SK_IF_WRITE_4(sc_if, 0, SK_TXMF1_CTRL_TEST, SK_TFCTL_OPERATION_ON);

	DPRINTFN(6, ("sk_init_yukon: end\n"));
}

/*
 * Note that to properly initialize any part of the GEnesis chip,
 * you first have to take it out of reset mode.
 */
int
sk_init(struct ifnet *ifp)
{
	struct sk_if_softc	*sc_if = ifp->if_softc;
	struct sk_softc		*sc = sc_if->sk_softc;
	struct mii_data		*mii = &sc_if->sk_mii;
	int			rc = 0, s;
	u_int32_t		imr, imtimer_ticks;

	DPRINTFN(1, ("sk_init\n"));

	s = splnet();

	if (ifp->if_flags & IFF_RUNNING) {
		splx(s);
		return 0;
	}

	/* Cancel pending I/O and free all RX/TX buffers. */
	sk_stop(ifp,0);

	if (sc->sk_type == SK_GENESIS) {
		/* Configure LINK_SYNC LED */
		SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL, SK_LINKLED_ON);
		SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL,
			      SK_LINKLED_LINKSYNC_ON);

		/* Configure RX LED */
		SK_IF_WRITE_1(sc_if, 0, SK_RXLED1_CTL,
			      SK_RXLEDCTL_COUNTER_START);

		/* Configure TX LED */
		SK_IF_WRITE_1(sc_if, 0, SK_TXLED1_CTL,
			      SK_TXLEDCTL_COUNTER_START);
	}

	/* Configure I2C registers */

	/* Configure XMAC(s) */
	switch (sc->sk_type) {
	case SK_GENESIS:
		sk_init_xmac(sc_if);
		break;
	case SK_YUKON:
	case SK_YUKON_LITE:
	case SK_YUKON_LP:
		sk_init_yukon(sc_if);
		break;
	}
	if ((rc = mii_mediachg(mii)) == ENXIO)
		rc = 0;
	else if (rc != 0)
		goto out;

	if (sc->sk_type == SK_GENESIS) {
		/* Configure MAC FIFOs */
		SK_IF_WRITE_4(sc_if, 0, SK_RXF1_CTL, SK_FIFO_UNRESET);
		SK_IF_WRITE_4(sc_if, 0, SK_RXF1_END, SK_FIFO_END);
		SK_IF_WRITE_4(sc_if, 0, SK_RXF1_CTL, SK_FIFO_ON);

		SK_IF_WRITE_4(sc_if, 0, SK_TXF1_CTL, SK_FIFO_UNRESET);
		SK_IF_WRITE_4(sc_if, 0, SK_TXF1_END, SK_FIFO_END);
		SK_IF_WRITE_4(sc_if, 0, SK_TXF1_CTL, SK_FIFO_ON);
	}

	/* Configure transmit arbiter(s) */
	SK_IF_WRITE_1(sc_if, 0, SK_TXAR1_COUNTERCTL,
	    SK_TXARCTL_ON|SK_TXARCTL_FSYNC_ON);

	/* Configure RAMbuffers */
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_CTLTST, SK_RBCTL_UNRESET);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_START, sc_if->sk_rx_ramstart);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_WR_PTR, sc_if->sk_rx_ramstart);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_RD_PTR, sc_if->sk_rx_ramstart);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_END, sc_if->sk_rx_ramend);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_CTLTST, SK_RBCTL_ON);

	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_CTLTST, SK_RBCTL_UNRESET);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_CTLTST, SK_RBCTL_STORENFWD_ON);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_START, sc_if->sk_tx_ramstart);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_WR_PTR, sc_if->sk_tx_ramstart);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_RD_PTR, sc_if->sk_tx_ramstart);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_END, sc_if->sk_tx_ramend);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_CTLTST, SK_RBCTL_ON);

	/* Configure BMUs */
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_BMU_CSR, SK_RXBMU_ONLINE);
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_CURADDR_LO,
	    SK_RX_RING_ADDR(sc_if, 0));
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_CURADDR_HI, 0);

	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_BMU_CSR, SK_TXBMU_ONLINE);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_CURADDR_LO,
            SK_TX_RING_ADDR(sc_if, 0));
	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_CURADDR_HI, 0);

	/* Init descriptors */
	if (sk_init_rx_ring(sc_if) == ENOBUFS) {
		aprint_error_dev(sc_if->sk_dev, "initialization failed: no "
		    "memory for rx buffers\n");
		sk_stop(ifp,0);
		splx(s);
		return ENOBUFS;
	}

	if (sk_init_tx_ring(sc_if) == ENOBUFS) {
		aprint_error_dev(sc_if->sk_dev, "initialization failed: no "
		    "memory for tx buffers\n");
		sk_stop(ifp,0);
		splx(s);
		return ENOBUFS;
	}

	/* Set interrupt moderation if changed via sysctl. */
	switch (sc->sk_type) {
	case SK_GENESIS:
		imtimer_ticks = SK_IMTIMER_TICKS_GENESIS;
		break;
	case SK_YUKON_EC:
		imtimer_ticks = SK_IMTIMER_TICKS_YUKON_EC;
		break;
	default:
		imtimer_ticks = SK_IMTIMER_TICKS_YUKON;
	}
	imr = sk_win_read_4(sc, SK_IMTIMERINIT);
	if (imr != SK_IM_USECS(sc->sk_int_mod)) {
		sk_win_write_4(sc, SK_IMTIMERINIT,
		    SK_IM_USECS(sc->sk_int_mod));
		aprint_verbose_dev(sc->sk_dev,
		    "interrupt moderation is %d us\n", sc->sk_int_mod);
	}

	/* Configure interrupt handling */
	CSR_READ_4(sc, SK_ISSR);
	if (sc_if->sk_port == SK_PORT_A)
		sc->sk_intrmask |= SK_INTRS1;
	else
		sc->sk_intrmask |= SK_INTRS2;

	sc->sk_intrmask |= SK_ISR_EXTERNAL_REG;

	CSR_WRITE_4(sc, SK_IMR, sc->sk_intrmask);

	/* Start BMUs. */
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_BMU_CSR, SK_RXBMU_RX_START);

	if (sc->sk_type == SK_GENESIS) {
		/* Enable XMACs TX and RX state machines */
		SK_XM_CLRBIT_2(sc_if, XM_MMUCMD, XM_MMUCMD_IGNPAUSE);
		SK_XM_SETBIT_2(sc_if, XM_MMUCMD,
			       XM_MMUCMD_TX_ENB|XM_MMUCMD_RX_ENB);
	}

	if (SK_YUKON_FAMILY(sc->sk_type)) {
		u_int16_t reg = SK_YU_READ_2(sc_if, YUKON_GPCR);
		reg |= YU_GPCR_TXEN | YU_GPCR_RXEN;
#if 0
		/* XXX disable 100Mbps and full duplex mode? */
		reg &= ~(YU_GPCR_SPEED | YU_GPCR_DPLX_EN);
#endif
		SK_YU_WRITE_2(sc_if, YUKON_GPCR, reg);
	}


	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	callout_reset(&sc_if->sk_tick_ch, hz, sk_tick, sc_if);

out:
	splx(s);
	return rc;
}

void
sk_stop(struct ifnet *ifp, int disable)
{
        struct sk_if_softc	*sc_if = ifp->if_softc;
	struct sk_softc		*sc = sc_if->sk_softc;
	int			i;

	DPRINTFN(1, ("sk_stop\n"));

	callout_stop(&sc_if->sk_tick_ch);

	if (sc_if->sk_phytype == SK_PHYTYPE_BCOM) {
		u_int32_t		val;

		/* Put PHY back into reset. */
		val = sk_win_read_4(sc, SK_GPIO);
		if (sc_if->sk_port == SK_PORT_A) {
			val |= SK_GPIO_DIR0;
			val &= ~SK_GPIO_DAT0;
		} else {
			val |= SK_GPIO_DIR2;
			val &= ~SK_GPIO_DAT2;
		}
		sk_win_write_4(sc, SK_GPIO, val);
	}

	/* Turn off various components of this interface. */
	SK_XM_SETBIT_2(sc_if, XM_GPIO, XM_GPIO_RESETMAC);
	switch (sc->sk_type) {
	case SK_GENESIS:
		SK_IF_WRITE_2(sc_if, 0, SK_TXF1_MACCTL,
			      SK_TXMACCTL_XMAC_RESET);
		SK_IF_WRITE_4(sc_if, 0, SK_RXF1_CTL, SK_FIFO_RESET);
		break;
	case SK_YUKON:
	case SK_YUKON_LITE:
	case SK_YUKON_LP:
		SK_IF_WRITE_1(sc_if,0, SK_RXMF1_CTRL_TEST, SK_RFCTL_RESET_SET);
		SK_IF_WRITE_1(sc_if,0, SK_TXMF1_CTRL_TEST, SK_TFCTL_RESET_SET);
		break;
	}
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_BMU_CSR, SK_RXBMU_OFFLINE);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_CTLTST, SK_RBCTL_RESET|SK_RBCTL_OFF);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_BMU_CSR, SK_TXBMU_OFFLINE);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_CTLTST, SK_RBCTL_RESET|SK_RBCTL_OFF);
	SK_IF_WRITE_1(sc_if, 0, SK_TXAR1_COUNTERCTL, SK_TXARCTL_OFF);
	SK_IF_WRITE_1(sc_if, 0, SK_RXLED1_CTL, SK_RXLEDCTL_COUNTER_STOP);
	SK_IF_WRITE_1(sc_if, 0, SK_TXLED1_CTL, SK_RXLEDCTL_COUNTER_STOP);
	SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL, SK_LINKLED_OFF);
	SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL, SK_LINKLED_LINKSYNC_OFF);

	/* Disable interrupts */
	if (sc_if->sk_port == SK_PORT_A)
		sc->sk_intrmask &= ~SK_INTRS1;
	else
		sc->sk_intrmask &= ~SK_INTRS2;
	CSR_WRITE_4(sc, SK_IMR, sc->sk_intrmask);

	SK_XM_READ_2(sc_if, XM_ISR);
	SK_XM_WRITE_2(sc_if, XM_IMR, 0xFFFF);

	/* Free RX and TX mbufs still in the queues. */
	for (i = 0; i < SK_RX_RING_CNT; i++) {
		if (sc_if->sk_cdata.sk_rx_chain[i].sk_mbuf != NULL) {
			m_freem(sc_if->sk_cdata.sk_rx_chain[i].sk_mbuf);
			sc_if->sk_cdata.sk_rx_chain[i].sk_mbuf = NULL;
		}
	}

	for (i = 0; i < SK_TX_RING_CNT; i++) {
		if (sc_if->sk_cdata.sk_tx_chain[i].sk_mbuf != NULL) {
			m_freem(sc_if->sk_cdata.sk_tx_chain[i].sk_mbuf);
			sc_if->sk_cdata.sk_tx_chain[i].sk_mbuf = NULL;
		}
	}

	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);
}

/* Power Management Framework */

static bool
skc_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct sk_softc *sc = device_private(dv);

	DPRINTFN(2, ("skc_suspend\n"));

	/* Turn off the driver is loaded LED */
	CSR_WRITE_2(sc, SK_LED, SK_LED_GREEN_OFF);

	return true;
}

static bool
skc_resume(device_t dv, const pmf_qual_t *qual)
{
	struct sk_softc *sc = device_private(dv);

	DPRINTFN(2, ("skc_resume\n"));

	sk_reset(sc);
	CSR_WRITE_2(sc, SK_LED, SK_LED_GREEN_ON);

	return true;
}

static bool
sk_resume(device_t dv, const pmf_qual_t *qual)
{
	struct sk_if_softc *sc_if = device_private(dv);

	sk_init_yukon(sc_if);
	return true;
}

CFATTACH_DECL_NEW(skc, sizeof(struct sk_softc),
    skc_probe, skc_attach, NULL, NULL);

CFATTACH_DECL_NEW(sk, sizeof(struct sk_if_softc),
    sk_probe, sk_attach, NULL, NULL);

#ifdef SK_DEBUG
void
sk_dump_txdesc(struct sk_tx_desc *desc, int idx)
{
#define DESC_PRINT(X)					\
	if (X)					\
		printf("txdesc[%d]." #X "=%#x\n",	\
		       idx, X);

	DESC_PRINT(le32toh(desc->sk_ctl));
	DESC_PRINT(le32toh(desc->sk_next));
	DESC_PRINT(le32toh(desc->sk_data_lo));
	DESC_PRINT(le32toh(desc->sk_data_hi));
	DESC_PRINT(le32toh(desc->sk_xmac_txstat));
	DESC_PRINT(le16toh(desc->sk_rsvd0));
	DESC_PRINT(le16toh(desc->sk_csum_startval));
	DESC_PRINT(le16toh(desc->sk_csum_startpos));
	DESC_PRINT(le16toh(desc->sk_csum_writepos));
	DESC_PRINT(le16toh(desc->sk_rsvd1));
#undef PRINT
}

void
sk_dump_bytes(const char *data, int len)
{
	int c, i, j;

	for (i = 0; i < len; i += 16) {
		printf("%08x  ", i);
		c = len - i;
		if (c > 16) c = 16;

		for (j = 0; j < c; j++) {
			printf("%02x ", data[i + j] & 0xff);
			if ((j & 0xf) == 7 && j > 0)
				printf(" ");
		}

		for (; j < 16; j++)
			printf("   ");
		printf("  ");

		for (j = 0; j < c; j++) {
			int ch = data[i + j] & 0xff;
			printf("%c", ' ' <= ch && ch <= '~' ? ch : ' ');
		}

		printf("\n");

		if (c < 16)
			break;
	}
}

void
sk_dump_mbuf(struct mbuf *m)
{
	int count = m->m_pkthdr.len;

	printf("m=%p, m->m_pkthdr.len=%d\n", m, m->m_pkthdr.len);

	while (count > 0 && m) {
		printf("m=%p, m->m_data=%p, m->m_len=%d\n",
		       m, m->m_data, m->m_len);
		sk_dump_bytes(mtod(m, char *), m->m_len);

		count -= m->m_len;
		m = m->m_next;
	}
}
#endif

static int
sk_sysctl_handler(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;
	struct sk_softc *sc;

	node = *rnode;
	sc = node.sysctl_data;
	t = sc->sk_int_mod;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

	if (t < SK_IM_MIN || t > SK_IM_MAX)
		return EINVAL;

	/* update the softc with sysctl-changed value, and mark
	   for hardware update */
	sc->sk_int_mod = t;
	sc->sk_int_mod_pending = 1;
	return 0;
}

/*
 * Set up sysctl(3) MIB, hw.sk.* - Individual controllers will be
 * set up in skc_attach()
 */
SYSCTL_SETUP(sysctl_sk, "sysctl sk subtree setup")
{
	int rc;
	const struct sysctlnode *node;

	if ((rc = sysctl_createv(clog, 0, NULL, &node,
	    0, CTLTYPE_NODE, "sk",
	    SYSCTL_DESCR("sk interface controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0) {
		goto err;
	}

	sk_root_num = node->sysctl_num;
	return;

err:
	aprint_error("%s: syctl_createv failed (rc = %d)\n", __func__, rc);
}
