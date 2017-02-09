/*	$NetBSD: if_otus.c,v 1.25 2013/10/17 21:07:37 christos Exp $	*/
/*	$OpenBSD: if_otus.c,v 1.18 2010/08/27 17:08:00 jsg Exp $	*/

/*-
 * Copyright (c) 2009 Damien Bergamini <damien.bergamini@free.fr>
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

/*-
 * Driver for Atheros AR9001U chipset.
 * http://www.atheros.com/pt/bulletins/AR9001USBBulletin.pdf
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_otus.c,v 1.25 2013/10/17 21:07:37 christos Exp $");

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/intr.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/firmload.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/if_otusreg.h>
#include <dev/usb/if_otusvar.h>

#ifdef OTUS_DEBUG

#define	DBG_INIT	__BIT(0)
#define	DBG_FN		__BIT(1)
#define	DBG_TX		__BIT(2)
#define	DBG_RX		__BIT(3)
#define	DBG_STM		__BIT(4)
#define	DBG_CHAN	__BIT(5)
#define	DBG_REG		__BIT(6)
#define	DBG_CMD		__BIT(7)
#define	DBG_ALL		0xffffffffU
#define DBG_NO_SC	(struct otus_softc *)NULL

unsigned int otus_debug = 0;
#define DPRINTFN(n, s, ...) do { \
	if (otus_debug & (n)) { \
		if ((s) != NULL) \
			printf("%s: ", device_xname((s)->sc_dev)); \
		else \
			printf("otus0: "); \
		printf("%s: ", __func__); \
		printf(__VA_ARGS__); \
	} \
} while (0)

#else	/* ! OTUS_DEBUG */

#define DPRINTFN(n, ...) \
	do { } while (0)

#endif	/* OTUS_DEBUG */

Static int	otus_match(device_t, cfdata_t, void *);
Static void	otus_attach(device_t, device_t, void *);
Static int	otus_detach(device_t, int);
Static int	otus_activate(device_t, devact_t);
Static void	otus_attachhook(device_t);
Static void	otus_get_chanlist(struct otus_softc *);
Static int	otus_load_firmware(struct otus_softc *, const char *,
		    uint32_t);
Static int	otus_open_pipes(struct otus_softc *);
Static void	otus_close_pipes(struct otus_softc *);
Static int	otus_alloc_tx_cmd(struct otus_softc *);
Static void	otus_free_tx_cmd(struct otus_softc *);
Static int	otus_alloc_tx_data_list(struct otus_softc *);
Static void	otus_free_tx_data_list(struct otus_softc *);
Static int	otus_alloc_rx_data_list(struct otus_softc *);
Static void	otus_free_rx_data_list(struct otus_softc *);
Static void	otus_next_scan(void *);
Static void	otus_task(void *);
Static void	otus_do_async(struct otus_softc *,
		    void (*)(struct otus_softc *, void *), void *, int);
Static int	otus_newstate(struct ieee80211com *, enum ieee80211_state,
		    int);
Static void	otus_newstate_cb(struct otus_softc *, void *);
Static int	otus_cmd(struct otus_softc *, uint8_t, const void *, int,
		    void *);
Static void	otus_write(struct otus_softc *, uint32_t, uint32_t);
Static int	otus_write_barrier(struct otus_softc *);
Static struct	ieee80211_node *otus_node_alloc(struct ieee80211_node_table *);
Static int	otus_media_change(struct ifnet *);
Static int	otus_read_eeprom(struct otus_softc *);
Static void	otus_newassoc(struct ieee80211_node *, int);
Static void	otus_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void	otus_cmd_rxeof(struct otus_softc *, uint8_t *, int);
Static void	otus_sub_rxeof(struct otus_softc *, uint8_t *, int);
Static void	otus_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void	otus_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static int	otus_tx(struct otus_softc *, struct mbuf *,
		    struct ieee80211_node *, struct otus_tx_data *);
Static void	otus_start(struct ifnet *);
Static void	otus_watchdog(struct ifnet *);
Static int	otus_ioctl(struct ifnet *, u_long, void *);
Static int	otus_set_multi(struct otus_softc *);
#ifdef HAVE_EDCA
Static void	otus_updateedca(struct ieee80211com *);
Static void	otus_updateedca_cb(struct otus_softc *, void *);
#endif
Static void	otus_updateedca_cb_locked(struct otus_softc *);
Static void	otus_updateslot(struct ifnet *);
Static void	otus_updateslot_cb(struct otus_softc *, void *);
Static void	otus_updateslot_cb_locked(struct otus_softc *);
Static int	otus_init_mac(struct otus_softc *);
Static uint32_t	otus_phy_get_def(struct otus_softc *, uint32_t);
Static int	otus_set_board_values(struct otus_softc *,
		    struct ieee80211_channel *);
Static int	otus_program_phy(struct otus_softc *,
		    struct ieee80211_channel *);
Static int	otus_set_rf_bank4(struct otus_softc *,
		    struct ieee80211_channel *);
Static void	otus_get_delta_slope(uint32_t, uint32_t *, uint32_t *);
Static int	otus_set_chan(struct otus_softc *, struct ieee80211_channel *,
		    int);
#ifdef notyet
Static int	otus_set_key(struct ieee80211com *, struct ieee80211_node *,
		    struct ieee80211_key *);
Static void	otus_set_key_cb(struct otus_softc *, void *);
Static void	otus_delete_key(struct ieee80211com *, struct ieee80211_node *,
		    struct ieee80211_key *);
Static void	otus_delete_key_cb(struct otus_softc *, void *);
#endif /* notyet */
Static void	otus_calib_to(void *);
Static int	otus_set_bssid(struct otus_softc *, const uint8_t *);
Static int	otus_set_macaddr(struct otus_softc *, const uint8_t *);
#ifdef notyet
Static void	otus_led_newstate_type1(struct otus_softc *);
Static void	otus_led_newstate_type2(struct otus_softc *);
#endif /* notyet */
Static void	otus_led_newstate_type3(struct otus_softc *);
Static int	otus_init(struct ifnet *);
Static void	otus_stop(struct ifnet *);
Static void	otus_wait_async(struct otus_softc *);

/* List of supported channels. */
static const uint8_t ar_chans[] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124,
	128, 132, 136, 140, 149, 153, 157, 161, 165, 34, 38, 42, 46
};

/*
 * This data is automatically generated from the "otus.ini" file.
 * It is stored in a different way though, to reduce kernel's .rodata
 * section overhead (5.1KB instead of 8.5KB).
 */

/* NB: apply AR_PHY(). */
static const uint16_t ar5416_phy_regs[] = {
	0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x008,
	0x009, 0x00a, 0x00b, 0x00c, 0x00d, 0x00e, 0x00f, 0x010, 0x011,
	0x012, 0x013, 0x014, 0x015, 0x016, 0x017, 0x018, 0x01a, 0x01b,
	0x040, 0x041, 0x042, 0x043, 0x045, 0x046, 0x047, 0x048, 0x049,
	0x04a, 0x04b, 0x04d, 0x04e, 0x04f, 0x051, 0x052, 0x053, 0x055,
	0x056, 0x058, 0x059, 0x05c, 0x05d, 0x05e, 0x05f, 0x060, 0x061,
	0x062, 0x063, 0x064, 0x065, 0x066, 0x067, 0x068, 0x069, 0x06a,
	0x06b, 0x06c, 0x06d, 0x070, 0x071, 0x072, 0x073, 0x074, 0x075,
	0x076, 0x077, 0x078, 0x079, 0x07a, 0x07b, 0x07c, 0x07f, 0x080,
	0x081, 0x082, 0x083, 0x084, 0x085, 0x086, 0x087, 0x088, 0x089,
	0x08a, 0x08b, 0x08c, 0x08d, 0x08e, 0x08f, 0x090, 0x091, 0x092,
	0x093, 0x094, 0x095, 0x096, 0x097, 0x098, 0x099, 0x09a, 0x09b,
	0x09c, 0x09d, 0x09e, 0x09f, 0x0a0, 0x0a1, 0x0a2, 0x0a3, 0x0a4,
	0x0a5, 0x0a6, 0x0a7, 0x0a8, 0x0a9, 0x0aa, 0x0ab, 0x0ac, 0x0ad,
	0x0ae, 0x0af, 0x0b0, 0x0b1, 0x0b2, 0x0b3, 0x0b4, 0x0b5, 0x0b6,
	0x0b7, 0x0b8, 0x0b9, 0x0ba, 0x0bb, 0x0bc, 0x0bd, 0x0be, 0x0bf,
	0x0c0, 0x0c1, 0x0c2, 0x0c3, 0x0c4, 0x0c5, 0x0c6, 0x0c7, 0x0c8,
	0x0c9, 0x0ca, 0x0cb, 0x0cc, 0x0cd, 0x0ce, 0x0cf, 0x0d0, 0x0d1,
	0x0d2, 0x0d3, 0x0d4, 0x0d5, 0x0d6, 0x0d7, 0x0d8, 0x0d9, 0x0da,
	0x0db, 0x0dc, 0x0dd, 0x0de, 0x0df, 0x0e0, 0x0e1, 0x0e2, 0x0e3,
	0x0e4, 0x0e5, 0x0e6, 0x0e7, 0x0e8, 0x0e9, 0x0ea, 0x0eb, 0x0ec,
	0x0ed, 0x0ee, 0x0ef, 0x0f0, 0x0f1, 0x0f2, 0x0f3, 0x0f4, 0x0f5,
	0x0f6, 0x0f7, 0x0f8, 0x0f9, 0x0fa, 0x0fb, 0x0fc, 0x0fd, 0x0fe,
	0x0ff, 0x100, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108, 0x109,
	0x10a, 0x10b, 0x10c, 0x10d, 0x10e, 0x10f, 0x13c, 0x13d, 0x13e,
	0x13f, 0x280, 0x281, 0x282, 0x283, 0x284, 0x285, 0x286, 0x287,
	0x288, 0x289, 0x28a, 0x28b, 0x28c, 0x28d, 0x28e, 0x28f, 0x290,
	0x291, 0x292, 0x293, 0x294, 0x295, 0x296, 0x297, 0x298, 0x299,
	0x29a, 0x29b, 0x29d, 0x29e, 0x29f, 0x2c0, 0x2c1, 0x2c2, 0x2c3,
	0x2c4, 0x2c5, 0x2c6, 0x2c7, 0x2c8, 0x2c9, 0x2ca, 0x2cb, 0x2cc,
	0x2cd, 0x2ce, 0x2cf, 0x2d0, 0x2d1, 0x2d2, 0x2d3, 0x2d4, 0x2d5,
	0x2d6, 0x2e2, 0x2e3, 0x2e4, 0x2e5, 0x2e6, 0x2e7, 0x2e8, 0x2e9,
	0x2ea, 0x2eb, 0x2ec, 0x2ed, 0x2ee, 0x2ef, 0x2f0, 0x2f1, 0x2f2,
	0x2f3, 0x2f4, 0x2f5, 0x2f6, 0x2f7, 0x2f8, 0x412, 0x448, 0x458,
	0x683, 0x69b, 0x812, 0x848, 0x858, 0xa83, 0xa9b, 0xc19, 0xc57,
	0xc5a, 0xc6f, 0xe9c, 0xed7, 0xed8, 0xed9, 0xeda, 0xedb, 0xedc,
	0xedd, 0xede, 0xedf, 0xee0, 0xee1
};

static const uint32_t ar5416_phy_vals_5ghz_20mhz[] = {
	0x00000007, 0x00000300, 0x00000000, 0xad848e19, 0x7d14e000,
	0x9c0a9f6b, 0x00000090, 0x00000000, 0x02020200, 0x00000e0e,
	0x0a020001, 0x0000a000, 0x00000000, 0x00000e0e, 0x00000007,
	0x00200400, 0x206a002e, 0x1372161e, 0x001a6a65, 0x1284233c,
	0x6c48b4e4, 0x00000859, 0x7ec80d2e, 0x31395c5e, 0x0004dd10,
	0x409a4190, 0x050cb081, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x000007d0, 0x00000118, 0x10000fff, 0x0510081c,
	0xd0058a15, 0x00000001, 0x00000004, 0x3f3f3f3f, 0x3f3f3f3f,
	0x0000007f, 0xdfb81020, 0x9280b212, 0x00020028, 0x5d50e188,
	0x00081fff, 0x00009b40, 0x00001120, 0x190fb515, 0x00000000,
	0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000007, 0x001fff00, 0x006f00c4, 0x03051000,
	0x00000820, 0x038919be, 0x06336f77, 0x60f6532c, 0x08f186c8,
	0x00046384, 0x00000000, 0x00000000, 0x00000000, 0x00000200,
	0x64646464, 0x3c787878, 0x000000aa, 0x00000000, 0x00001042,
	0x00000000, 0x00000040, 0x00000080, 0x000001a1, 0x000001e1,
	0x00000021, 0x00000061, 0x00000168, 0x000001a8, 0x000001e8,
	0x00000028, 0x00000068, 0x00000189, 0x000001c9, 0x00000009,
	0x00000049, 0x00000089, 0x00000170, 0x000001b0, 0x000001f0,
	0x00000030, 0x00000070, 0x00000191, 0x000001d1, 0x00000011,
	0x00000051, 0x00000091, 0x000001b8, 0x000001f8, 0x00000038,
	0x00000078, 0x00000199, 0x000001d9, 0x00000019, 0x00000059,
	0x00000099, 0x000000d9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x00000000,
	0x00000001, 0x00000002, 0x00000003, 0x00000004, 0x00000005,
	0x00000008, 0x00000009, 0x0000000a, 0x0000000b, 0x0000000c,
	0x0000000d, 0x00000010, 0x00000011, 0x00000012, 0x00000013,
	0x00000014, 0x00000015, 0x00000018, 0x00000019, 0x0000001a,
	0x0000001b, 0x0000001c, 0x0000001d, 0x00000020, 0x00000021,
	0x00000022, 0x00000023, 0x00000024, 0x00000025, 0x00000028,
	0x00000029, 0x0000002a, 0x0000002b, 0x0000002c, 0x0000002d,
	0x00000030, 0x00000031, 0x00000032, 0x00000033, 0x00000034,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000010, 0x0000001a, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000008, 0x00000440, 0xd6be4788, 0x012e8160,
	0x40806333, 0x00106c10, 0x009c4060, 0x1883800a, 0x018830c6,
	0x00000400, 0x000009b5, 0x00000000, 0x00000108, 0x3f3f3f3f,
	0x3f3f3f3f, 0x13c889af, 0x38490a20, 0x00007bb6, 0x0fff3ffc,
	0x00000001, 0x0000a000, 0x00000000, 0x0cc75380, 0x0f0f0f01,
	0xdfa91f01, 0x00418a11, 0x00000000, 0x09249126, 0x0a1a9caa,
	0x1ce739ce, 0x051701ce, 0x18010000, 0x30032602, 0x48073e06,
	0x560b4c0a, 0x641a600f, 0x7a4f6e1b, 0x8c5b7e5a, 0x9d0f96cf,
	0xb51fa69f, 0xcb3fbd07, 0x0000d7bf, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x3fffffff, 0x3fffffff, 0x3fffffff, 0x0003ffff, 0x79a8aa1f,
	0x08000000, 0x3f3f3f3f, 0x3f3f3f3f, 0x1ce739ce, 0x000001ce,
	0x00000007, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x3f3f3f3f, 0x3f3f3f3f, 0x3f3f3f3f,
	0x00000000, 0x1ce739ce, 0x000000c0, 0x00180a65, 0x0510001c,
	0x00009b40, 0x012e8160, 0x09249126, 0x00180a65, 0x0510001c,
	0x00009b40, 0x012e8160, 0x09249126, 0x0001c600, 0x004b6a8e,
	0x000003ce, 0x00181400, 0x00820820, 0x066c420f, 0x0f282207,
	0x17601685, 0x1f801104, 0x37a00c03, 0x3fc40883, 0x57c00803,
	0x5fd80682, 0x7fe00482, 0x7f3c7bba, 0xf3307ff0
};

#ifdef notyet
static const uint32_t ar5416_phy_vals_5ghz_40mhz[] = {
	0x00000007, 0x000003c4, 0x00000000, 0xad848e19, 0x7d14e000,
	0x9c0a9f6b, 0x00000090, 0x00000000, 0x02020200, 0x00000e0e,
	0x0a020001, 0x0000a000, 0x00000000, 0x00000e0e, 0x00000007,
	0x00200400, 0x206a002e, 0x13721c1e, 0x001a6a65, 0x1284233c,
	0x6c48b4e4, 0x00000859, 0x7ec80d2e, 0x31395c5e, 0x0004dd10,
	0x409a4190, 0x050cb081, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x000007d0, 0x00000230, 0x10000fff, 0x0510081c,
	0xd0058a15, 0x00000001, 0x00000004, 0x3f3f3f3f, 0x3f3f3f3f,
	0x0000007f, 0xdfb81020, 0x9280b212, 0x00020028, 0x5d50e188,
	0x00081fff, 0x00009b40, 0x00001120, 0x190fb515, 0x00000000,
	0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000007, 0x001fff00, 0x006f00c4, 0x03051000,
	0x00000820, 0x038919be, 0x06336f77, 0x60f6532c, 0x08f186c8,
	0x00046384, 0x00000000, 0x00000000, 0x00000000, 0x00000200,
	0x64646464, 0x3c787878, 0x000000aa, 0x00000000, 0x00001042,
	0x00000000, 0x00000040, 0x00000080, 0x000001a1, 0x000001e1,
	0x00000021, 0x00000061, 0x00000168, 0x000001a8, 0x000001e8,
	0x00000028, 0x00000068, 0x00000189, 0x000001c9, 0x00000009,
	0x00000049, 0x00000089, 0x00000170, 0x000001b0, 0x000001f0,
	0x00000030, 0x00000070, 0x00000191, 0x000001d1, 0x00000011,
	0x00000051, 0x00000091, 0x000001b8, 0x000001f8, 0x00000038,
	0x00000078, 0x00000199, 0x000001d9, 0x00000019, 0x00000059,
	0x00000099, 0x000000d9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x00000000,
	0x00000001, 0x00000002, 0x00000003, 0x00000004, 0x00000005,
	0x00000008, 0x00000009, 0x0000000a, 0x0000000b, 0x0000000c,
	0x0000000d, 0x00000010, 0x00000011, 0x00000012, 0x00000013,
	0x00000014, 0x00000015, 0x00000018, 0x00000019, 0x0000001a,
	0x0000001b, 0x0000001c, 0x0000001d, 0x00000020, 0x00000021,
	0x00000022, 0x00000023, 0x00000024, 0x00000025, 0x00000028,
	0x00000029, 0x0000002a, 0x0000002b, 0x0000002c, 0x0000002d,
	0x00000030, 0x00000031, 0x00000032, 0x00000033, 0x00000034,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000010, 0x0000001a, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000008, 0x00000440, 0xd6be4788, 0x012e8160,
	0x40806333, 0x00106c10, 0x009c4060, 0x1883800a, 0x018830c6,
	0x00000400, 0x000009b5, 0x00000000, 0x00000210, 0x3f3f3f3f,
	0x3f3f3f3f, 0x13c889af, 0x38490a20, 0x00007bb6, 0x0fff3ffc,
	0x00000001, 0x0000a000, 0x00000000, 0x0cc75380, 0x0f0f0f01,
	0xdfa91f01, 0x00418a11, 0x00000000, 0x09249126, 0x0a1a9caa,
	0x1ce739ce, 0x051701ce, 0x18010000, 0x30032602, 0x48073e06,
	0x560b4c0a, 0x641a600f, 0x7a4f6e1b, 0x8c5b7e5a, 0x9d0f96cf,
	0xb51fa69f, 0xcb3fbcbf, 0x0000d7bf, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x3fffffff, 0x3fffffff, 0x3fffffff, 0x0003ffff, 0x79a8aa1f,
	0x08000000, 0x3f3f3f3f, 0x3f3f3f3f, 0x1ce739ce, 0x000001ce,
	0x00000007, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x3f3f3f3f, 0x3f3f3f3f, 0x3f3f3f3f,
	0x00000000, 0x1ce739ce, 0x000000c0, 0x00180a65, 0x0510001c,
	0x00009b40, 0x012e8160, 0x09249126, 0x00180a65, 0x0510001c,
	0x00009b40, 0x012e8160, 0x09249126, 0x0001c600, 0x004b6a8e,
	0x000003ce, 0x00181400, 0x00820820, 0x066c420f, 0x0f282207,
	0x17601685, 0x1f801104, 0x37a00c03, 0x3fc40883, 0x57c00803,
	0x5fd80682, 0x7fe00482, 0x7f3c7bba, 0xf3307ff0
};
#endif

#ifdef notyet
static const uint32_t ar5416_phy_vals_2ghz_40mhz[] = {
	0x00000007, 0x000003c4, 0x00000000, 0xad848e19, 0x7d14e000,
	0x9c0a9f6b, 0x00000090, 0x00000000, 0x02020200, 0x00000e0e,
	0x0a020001, 0x0000a000, 0x00000000, 0x00000e0e, 0x00000007,
	0x00200400, 0x206a002e, 0x13721c24, 0x00197a68, 0x1284233c,
	0x6c48b0e4, 0x00000859, 0x7ec80d2e, 0x31395c5e, 0x0004dd20,
	0x409a4190, 0x050cb081, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000898, 0x00000268, 0x10000fff, 0x0510001c,
	0xd0058a15, 0x00000001, 0x00000004, 0x3f3f3f3f, 0x3f3f3f3f,
	0x0000007f, 0xdfb81020, 0x9280b212, 0x00020028, 0x5d50e188,
	0x00081fff, 0x00009b40, 0x00001120, 0x190fb515, 0x00000000,
	0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000007, 0x001fff00, 0x006f00c4, 0x03051000,
	0x00000820, 0x038919be, 0x06336f77, 0x60f6532c, 0x08f186c8,
	0x00046384, 0x00000000, 0x00000000, 0x00000000, 0x00000200,
	0x64646464, 0x3c787878, 0x000000aa, 0x00000000, 0x00001042,
	0x00000000, 0x00000040, 0x00000080, 0x00000141, 0x00000181,
	0x000001c1, 0x00000001, 0x00000041, 0x000001a8, 0x000001e8,
	0x00000028, 0x00000068, 0x000000a8, 0x00000169, 0x000001a9,
	0x000001e9, 0x00000029, 0x00000069, 0x00000190, 0x000001d0,
	0x00000010, 0x00000050, 0x00000090, 0x00000151, 0x00000191,
	0x000001d1, 0x00000011, 0x00000051, 0x00000198, 0x000001d8,
	0x00000018, 0x00000058, 0x00000098, 0x00000159, 0x00000199,
	0x000001d9, 0x00000019, 0x00000059, 0x00000099, 0x000000d9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x00000000,
	0x00000001, 0x00000002, 0x00000003, 0x00000004, 0x00000005,
	0x00000008, 0x00000009, 0x0000000a, 0x0000000b, 0x0000000c,
	0x0000000d, 0x00000010, 0x00000011, 0x00000012, 0x00000013,
	0x00000014, 0x00000015, 0x00000018, 0x00000019, 0x0000001a,
	0x0000001b, 0x0000001c, 0x0000001d, 0x00000020, 0x00000021,
	0x00000022, 0x00000023, 0x00000024, 0x00000025, 0x00000028,
	0x00000029, 0x0000002a, 0x0000002b, 0x0000002c, 0x0000002d,
	0x00000030, 0x00000031, 0x00000032, 0x00000033, 0x00000034,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000010, 0x0000001a, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x0000000e, 0x00000440, 0xd03e4788, 0x012a8160,
	0x40806333, 0x00106c10, 0x009c4060, 0x1883800a, 0x018830c6,
	0x00000400, 0x000009b5, 0x00000000, 0x00000210, 0x3f3f3f3f,
	0x3f3f3f3f, 0x13c889af, 0x38490a20, 0x00007bb6, 0x0fff3ffc,
	0x00000001, 0x0000a000, 0x00000000, 0x0cc75380, 0x0f0f0f01,
	0xdfa91f01, 0x00418a11, 0x00000000, 0x09249126, 0x0a1a7caa,
	0x1ce739ce, 0x051701ce, 0x18010000, 0x2e032402, 0x4a0a3c06,
	0x621a540b, 0x764f6c1b, 0x845b7a5a, 0x950f8ccf, 0xa5cf9b4f,
	0xbddfaf1f, 0xd1ffc93f, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x3fffffff, 0x3fffffff, 0x3fffffff, 0x0003ffff, 0x79a8aa1f,
	0x08000000, 0x3f3f3f3f, 0x3f3f3f3f, 0x1ce739ce, 0x000001ce,
	0x00000007, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x3f3f3f3f, 0x3f3f3f3f, 0x3f3f3f3f,
	0x00000000, 0x1ce739ce, 0x000000c0, 0x00180a68, 0x0510001c,
	0x00009b40, 0x012a8160, 0x09249126, 0x00180a68, 0x0510001c,
	0x00009b40, 0x012a8160, 0x09249126, 0x0001c600, 0x004b6a8e,
	0x000003ce, 0x00181400, 0x00820820, 0x066c420f, 0x0f282207,
	0x17601685, 0x1f801104, 0x37a00c03, 0x3fc40883, 0x57c00803,
	0x5fd80682, 0x7fe00482, 0x7f3c7bba, 0xf3307ff0
};
#endif

static const uint32_t ar5416_phy_vals_2ghz_20mhz[] = {
	0x00000007, 0x00000300, 0x00000000, 0xad848e19, 0x7d14e000,
	0x9c0a9f6b, 0x00000090, 0x00000000, 0x02020200, 0x00000e0e,
	0x0a020001, 0x0000a000, 0x00000000, 0x00000e0e, 0x00000007,
	0x00200400, 0x206a002e, 0x137216a4, 0x00197a68, 0x1284233c,
	0x6c48b0e4, 0x00000859, 0x7ec80d2e, 0x31395c5e, 0x0004dd20,
	0x409a4190, 0x050cb081, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000898, 0x00000134, 0x10000fff, 0x0510001c,
	0xd0058a15, 0x00000001, 0x00000004, 0x3f3f3f3f, 0x3f3f3f3f,
	0x0000007f, 0xdfb81020, 0x9280b212, 0x00020028, 0x5d50e188,
	0x00081fff, 0x00009b40, 0x00001120, 0x190fb515, 0x00000000,
	0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000007, 0x001fff00, 0x006f00c4, 0x03051000,
	0x00000820, 0x038919be, 0x06336f77, 0x60f6532c, 0x08f186c8,
	0x00046384, 0x00000000, 0x00000000, 0x00000000, 0x00000200,
	0x64646464, 0x3c787878, 0x000000aa, 0x00000000, 0x00001042,
	0x00000000, 0x00000040, 0x00000080, 0x00000141, 0x00000181,
	0x000001c1, 0x00000001, 0x00000041, 0x000001a8, 0x000001e8,
	0x00000028, 0x00000068, 0x000000a8, 0x00000169, 0x000001a9,
	0x000001e9, 0x00000029, 0x00000069, 0x00000190, 0x000001d0,
	0x00000010, 0x00000050, 0x00000090, 0x00000151, 0x00000191,
	0x000001d1, 0x00000011, 0x00000051, 0x00000198, 0x000001d8,
	0x00000018, 0x00000058, 0x00000098, 0x00000159, 0x00000199,
	0x000001d9, 0x00000019, 0x00000059, 0x00000099, 0x000000d9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9,
	0x000000f9, 0x000000f9, 0x000000f9, 0x000000f9, 0x00000000,
	0x00000001, 0x00000002, 0x00000003, 0x00000004, 0x00000005,
	0x00000008, 0x00000009, 0x0000000a, 0x0000000b, 0x0000000c,
	0x0000000d, 0x00000010, 0x00000011, 0x00000012, 0x00000013,
	0x00000014, 0x00000015, 0x00000018, 0x00000019, 0x0000001a,
	0x0000001b, 0x0000001c, 0x0000001d, 0x00000020, 0x00000021,
	0x00000022, 0x00000023, 0x00000024, 0x00000025, 0x00000028,
	0x00000029, 0x0000002a, 0x0000002b, 0x0000002c, 0x0000002d,
	0x00000030, 0x00000031, 0x00000032, 0x00000033, 0x00000034,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000035, 0x00000035, 0x00000035, 0x00000035,
	0x00000035, 0x00000010, 0x0000001a, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x0000000e, 0x00000440, 0xd03e4788, 0x012a8160,
	0x40806333, 0x00106c10, 0x009c4060, 0x1883800a, 0x018830c6,
	0x00000400, 0x000009b5, 0x00000000, 0x00000108, 0x3f3f3f3f,
	0x3f3f3f3f, 0x13c889af, 0x38490a20, 0x00007bb6, 0x0fff3ffc,
	0x00000001, 0x0000a000, 0x00000000, 0x0cc75380, 0x0f0f0f01,
	0xdfa91f01, 0x00418a11, 0x00000000, 0x09249126, 0x0a1a7caa,
	0x1ce739ce, 0x051701ce, 0x18010000, 0x2e032402, 0x4a0a3c06,
	0x621a540b, 0x764f6c1b, 0x845b7a5a, 0x950f8ccf, 0xa5cf9b4f,
	0xbddfaf1f, 0xd1ffc93f, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x3fffffff, 0x3fffffff, 0x3fffffff, 0x0003ffff, 0x79a8aa1f,
	0x08000000, 0x3f3f3f3f, 0x3f3f3f3f, 0x1ce739ce, 0x000001ce,
	0x00000007, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x3f3f3f3f, 0x3f3f3f3f, 0x3f3f3f3f,
	0x00000000, 0x1ce739ce, 0x000000c0, 0x00180a68, 0x0510001c,
	0x00009b40, 0x012a8160, 0x09249126, 0x00180a68, 0x0510001c,
	0x00009b40, 0x012a8160, 0x09249126, 0x0001c600, 0x004b6a8e,
	0x000003ce, 0x00181400, 0x00820820, 0x066c420f, 0x0f282207,
	0x17601685, 0x1f801104, 0x37a00c03, 0x3fc40883, 0x57c00803,
	0x5fd80682, 0x7fe00482, 0x7f3c7bba, 0xf3307ff0
};

/* NB: apply AR_PHY(). */
static const uint8_t ar5416_banks_regs[] = {
	0x2c, 0x38, 0x2c, 0x3b, 0x2c, 0x38, 0x3c, 0x2c, 0x3a, 0x2c, 0x39,
	0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
	0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
	0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
	0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
	0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x38, 0x2c, 0x2c,
	0x2c, 0x3c
};

static const uint32_t ar5416_banks_vals_5ghz[] = {
	0x1e5795e5, 0x02008020, 0x02108421, 0x00000008, 0x0e73ff17,
	0x00000420, 0x01400018, 0x000001a1, 0x00000001, 0x00000013,
	0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00004000, 0x00006c00, 0x00002c00, 0x00004800,
	0x00004000, 0x00006000, 0x00001000, 0x00004000, 0x00007c00,
	0x00007c00, 0x00007c00, 0x00007c00, 0x00007c00, 0x00087c00,
	0x00007c00, 0x00005400, 0x00000c00, 0x00001800, 0x00007c00,
	0x00006c00, 0x00006c00, 0x00007c00, 0x00002c00, 0x00003c00,
	0x00003800, 0x00001c00, 0x00000800, 0x00000408, 0x00004c15,
	0x00004188, 0x0000201e, 0x00010408, 0x00000801, 0x00000c08,
	0x0000181e, 0x00001016, 0x00002800, 0x00004010, 0x0000081c,
	0x00000115, 0x00000015, 0x00000066, 0x0000001c, 0x00000000,
	0x00000004, 0x00000015, 0x0000001f, 0x00000000, 0x000000a0,
	0x00000000, 0x00000040, 0x0000001c
};

static const uint32_t ar5416_banks_vals_2ghz[] = {
	0x1e5795e5, 0x02008020, 0x02108421, 0x00000008, 0x0e73ff17,
	0x00000420, 0x01c00018, 0x000001a1, 0x00000001, 0x00000013,
	0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00004000, 0x00006c00, 0x00002c00, 0x00004800,
	0x00004000, 0x00006000, 0x00001000, 0x00004000, 0x00007c00,
	0x00007c00, 0x00007c00, 0x00007c00, 0x00007c00, 0x00087c00,
	0x00007c00, 0x00005400, 0x00000c00, 0x00001800, 0x00007c00,
	0x00006c00, 0x00006c00, 0x00007c00, 0x00002c00, 0x00003c00,
	0x00003800, 0x00001c00, 0x00000800, 0x00000408, 0x00004c15,
	0x00004188, 0x0000201e, 0x00010408, 0x00000801, 0x00000c08,
	0x0000181e, 0x00001016, 0x00002800, 0x00004010, 0x0000081c,
	0x00000115, 0x00000015, 0x00000066, 0x0000001c, 0x00000000,
	0x00000004, 0x00000015, 0x0000001f, 0x00000400, 0x000000a0,
	0x00000000, 0x00000040, 0x0000001c
};

static const struct usb_devno otus_devs[] = {
	{ USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_WN7512 },
	{ USB_VENDOR_ATHEROS2,		USB_PRODUCT_ATHEROS2_3CRUSBN275 },
	{ USB_VENDOR_ATHEROS2,		USB_PRODUCT_ATHEROS2_TG121N },
	{ USB_VENDOR_ATHEROS2,		USB_PRODUCT_ATHEROS2_AR9170 },
	{ USB_VENDOR_ATHEROS2,		USB_PRODUCT_ATHEROS2_WN612 },
	{ USB_VENDOR_ATHEROS2,		USB_PRODUCT_ATHEROS2_WN821NV2 },
	{ USB_VENDOR_AVM,		USB_PRODUCT_AVM_FRITZWLAN },
	{ USB_VENDOR_CACE,		USB_PRODUCT_CACE_AIRPCAPNX },
	{ USB_VENDOR_DLINK2,		USB_PRODUCT_DLINK2_DWA130D1 },
	{ USB_VENDOR_DLINK2,		USB_PRODUCT_DLINK2_DWA160A1 },
	{ USB_VENDOR_DLINK2,		USB_PRODUCT_DLINK2_DWA160A2 },
	{ USB_VENDOR_IODATA,		USB_PRODUCT_IODATA_WNGDNUS2 },
	{ USB_VENDOR_NEC,		USB_PRODUCT_NEC_WL300NUG },
	{ USB_VENDOR_NETGEAR,		USB_PRODUCT_NETGEAR_WN111V2 },
	{ USB_VENDOR_NETGEAR,		USB_PRODUCT_NETGEAR_WNA1000 },
	{ USB_VENDOR_NETGEAR,		USB_PRODUCT_NETGEAR_WNDA3100 },
	{ USB_VENDOR_PLANEX2,		USB_PRODUCT_PLANEX2_GW_US300 },
	{ USB_VENDOR_WISTRONNEWEB,	USB_PRODUCT_WISTRONNEWEB_O8494 },
	{ USB_VENDOR_WISTRONNEWEB,	USB_PRODUCT_WISTRONNEWEB_WNC0600 },
	{ USB_VENDOR_ZCOM,		USB_PRODUCT_ZCOM_UB81 },
	{ USB_VENDOR_ZCOM,		USB_PRODUCT_ZCOM_UB82 },
	{ USB_VENDOR_ZYDAS,		USB_PRODUCT_ZYDAS_ZD1221 },
	{ USB_VENDOR_ZYXEL,		USB_PRODUCT_ZYXEL_NWD271N }
};

CFATTACH_DECL_NEW(otus, sizeof(struct otus_softc), otus_match, otus_attach,
    otus_detach, otus_activate);

Static int
otus_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa;

	uaa = aux;

	DPRINTFN(DBG_FN, DBG_NO_SC,
	    "otus_match: vendor=0x%x product=0x%x revision=0x%x\n",
		    uaa->vendor, uaa->product, uaa->release);

	return usb_lookup(otus_devs, uaa->vendor, uaa->product) != NULL ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

Static void
otus_attach(device_t parent, device_t self, void *aux)
{
	struct otus_softc *sc;
	struct usb_attach_arg *uaa;
	char *devinfop;
	int error;

	sc = device_private(self);

	DPRINTFN(DBG_FN, sc, "\n");

	sc->sc_dev = self;
	uaa = aux;
	sc->sc_udev = uaa->device;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(sc->sc_udev, 0);
	aprint_normal_dev(sc->sc_dev, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	mutex_init(&sc->sc_cmd_mtx,   MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_task_mtx,  MUTEX_DEFAULT, IPL_NET);
	mutex_init(&sc->sc_tx_mtx,    MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_write_mtx, MUTEX_DEFAULT, IPL_NONE);

	usb_init_task(&sc->sc_task, otus_task, sc, 0);

	callout_init(&sc->sc_scan_to, 0);
	callout_setfunc(&sc->sc_scan_to, otus_next_scan, sc);
	callout_init(&sc->sc_calib_to, 0);
	callout_setfunc(&sc->sc_calib_to, otus_calib_to, sc);

	sc->sc_amrr.amrr_min_success_threshold =  1;
	sc->sc_amrr.amrr_max_success_threshold = 10;

	if (usbd_set_config_no(sc->sc_udev, 1, 0) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not set configuration no\n");
		return;
	}

	/* Get the first interface handle. */
	error = usbd_device2interface_handle(sc->sc_udev, 0, &sc->sc_iface);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not get interface handle\n");
		return;
	}

	if ((error = otus_open_pipes(sc)) != 0) {
		aprint_error_dev(sc->sc_dev, "could not open pipes\n");
		return;
	}

	/*
	 * We need the firmware loaded from file system to complete the attach.
	 */
	config_mountroot(self, otus_attachhook);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, sc->sc_dev);
}

Static void
otus_wait_async(struct otus_softc *sc)
{

	DPRINTFN(DBG_FN, sc, "\n");

	while (sc->sc_cmdq.queued > 0)
		tsleep(&sc->sc_cmdq, 0, "sc_cmdq", 0);
}

Static int
otus_detach(device_t self, int flags)
{
	struct otus_softc *sc;
	struct ifnet *ifp;
	int s;

	sc = device_private(self);

	DPRINTFN(DBG_FN, sc, "\n");

	s = splusb();

	sc->sc_dying = 1;

	ifp = sc->sc_ic.ic_ifp;
	if (ifp != NULL)	/* Failed to attach properly */
		otus_stop(ifp);

	usb_rem_task(sc->sc_udev, &sc->sc_task);
	callout_destroy(&sc->sc_scan_to);
	callout_destroy(&sc->sc_calib_to);

	if (ifp && ifp->if_flags != 0) { /* if_attach() has been called. */
		ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
		bpf_detach(ifp);
		ieee80211_ifdetach(&sc->sc_ic);
		if_detach(ifp);
	}
	otus_close_pipes(sc);
	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, sc->sc_dev);

	mutex_destroy(&sc->sc_write_mtx);
	mutex_destroy(&sc->sc_tx_mtx);
	mutex_destroy(&sc->sc_task_mtx);
	mutex_destroy(&sc->sc_cmd_mtx);
	return 0;
}

Static int
otus_activate(device_t self, devact_t act)
{
	struct otus_softc *sc;

	sc = device_private(self);

	DPRINTFN(DBG_FN, sc, "%d\n", act);

	switch (act) {
	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		if_deactivate(sc->sc_ic.ic_ifp);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

Static void
otus_attachhook(device_t arg)
{
	struct otus_softc *sc;
	struct ieee80211com *ic;
	struct ifnet *ifp;
	usb_device_request_t req;
	uint32_t in, out;
	int error;

	sc = device_private(arg);

	DPRINTFN(DBG_FN, sc, "\n");

	ic = &sc->sc_ic;
	ifp = &sc->sc_if;

	error = otus_load_firmware(sc, "otus-init", AR_FW_INIT_ADDR);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not load init firmware\n");
		return;
	}
	usbd_delay_ms(sc->sc_udev, 1000);

	error = otus_load_firmware(sc, "otus-main", AR_FW_MAIN_ADDR);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not load main firmware\n");
		return;
	}

	/* Tell device that firmware transfer is complete. */
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AR_FW_DOWNLOAD_COMPLETE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	if (usbd_do_request(sc->sc_udev, &req, NULL) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "firmware initialization failed\n");
		return;
	}

	/* Send an ECHO command to check that everything is settled. */
	in = 0xbadc0ffe;
	if (otus_cmd(sc, AR_CMD_ECHO, &in, sizeof(in), &out) != 0) {
		aprint_error_dev(sc->sc_dev, "echo command failed\n");
		return;
	}
	if (in != out) {
		aprint_error_dev(sc->sc_dev,
		    "echo reply mismatch: 0x%08x!=0x%08x\n", in, out);
		return;
	}

	/* Read entire EEPROM. */
	if (otus_read_eeprom(sc) != 0) {
		aprint_error_dev(sc->sc_dev, "could not read EEPROM\n");
		return;
	}

	sc->sc_txmask = sc->sc_eeprom.baseEepHeader.txMask;
	sc->sc_rxmask = sc->sc_eeprom.baseEepHeader.rxMask;
	sc->sc_capflags = sc->sc_eeprom.baseEepHeader.opCapFlags;
	IEEE80211_ADDR_COPY(ic->ic_myaddr, sc->sc_eeprom.baseEepHeader.macAddr);
	sc->sc_led_newstate = otus_led_newstate_type3;	/* XXX */

	aprint_normal_dev(sc->sc_dev,
	    "MAC/BBP AR9170, RF AR%X, MIMO %dT%dR, address %s\n",
	    (sc->sc_capflags & AR5416_OPFLAGS_11A) ?
	        0x9104 : ((sc->sc_txmask == 0x5) ? 0x9102 : 0x9101),
	    (sc->sc_txmask == 0x5) ? 2 : 1, (sc->sc_rxmask == 0x5) ? 2 : 1,
	    ether_sprintf(ic->ic_myaddr));

	/*
	 * Setup the 802.11 device.
	 */
	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* Set device capabilities. */
	ic->ic_caps =
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_WPA;		/* 802.11i */

	if (sc->sc_eeprom.baseEepHeader.opCapFlags & AR5416_OPFLAGS_11G) {
		/* Set supported .11b and .11g rates. */
		ic->ic_sup_rates[IEEE80211_MODE_11B] =
		    ieee80211_std_rateset_11b;
		ic->ic_sup_rates[IEEE80211_MODE_11G] =
		    ieee80211_std_rateset_11g;
	}
	if (sc->sc_eeprom.baseEepHeader.opCapFlags & AR5416_OPFLAGS_11A) {
		/* Set supported .11a rates. */
		ic->ic_sup_rates[IEEE80211_MODE_11A] =
		    ieee80211_std_rateset_11a;
	}

	/* Build the list of supported channels. */
	otus_get_chanlist(sc);

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init  = otus_init;
	ifp->if_ioctl = otus_ioctl;
	ifp->if_start = otus_start;
	ifp->if_watchdog = otus_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);

	if_attach(ifp);

	ieee80211_ifattach(ic);

	ic->ic_node_alloc = otus_node_alloc;
	ic->ic_newassoc   = otus_newassoc;
	ic->ic_updateslot = otus_updateslot;
#ifdef HAVE_EDCA
	ic->ic_updateedca = otus_updateedca;
#endif /* HAVE_EDCA */
#ifdef notyet
	ic->ic_set_key = otus_set_key;
	ic->ic_delete_key = otus_delete_key;
#endif /* notyet */

	/* Override state transition machine. */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = otus_newstate;
	ieee80211_media_init(ic, otus_media_change, ieee80211_media_status);

	bpf_attach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN,
	    &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof(sc->sc_rxtapu);
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(OTUS_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof(sc->sc_txtapu);
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(OTUS_TX_RADIOTAP_PRESENT);

	ieee80211_announce(ic);
}

Static void
otus_get_chanlist(struct otus_softc *sc)
{
	struct ieee80211com *ic;
	uint8_t chan;
	int i;

#ifdef OTUS_DEBUG
	/* XXX regulatory domain. */
	uint16_t domain = le16toh(sc->sc_eeprom.baseEepHeader.regDmn[0]);

	DPRINTFN(DBG_FN|DBG_INIT, sc, "regdomain=0x%04x\n", domain);
#endif

	ic = &sc->sc_ic;
	if (sc->sc_eeprom.baseEepHeader.opCapFlags & AR5416_OPFLAGS_11G) {
		for (i = 0; i < 14; i++) {
			chan = ar_chans[i];
			ic->ic_channels[chan].ic_freq =
			    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_2GHZ);
			ic->ic_channels[chan].ic_flags =
			    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
			    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
		}
	}
	if (sc->sc_eeprom.baseEepHeader.opCapFlags & AR5416_OPFLAGS_11A) {
		for (i = 14; i < __arraycount(ar_chans); i++) {
			chan = ar_chans[i];
			ic->ic_channels[chan].ic_freq =
			    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_5GHZ);
			ic->ic_channels[chan].ic_flags = IEEE80211_CHAN_A;
		}
	}
}

Static int
otus_load_firmware(struct otus_softc *sc, const char *name, uint32_t addr)
{
	usb_device_request_t req;
	firmware_handle_t fh;
	uint8_t *ptr;
	uint8_t *fw;
	size_t size;
	int mlen, error;

	DPRINTFN(DBG_FN, sc, "\n");

	if ((error = firmware_open("if_otus", name, &fh)) != 0)
		return error;

	size = firmware_get_size(fh);
	if ((fw = firmware_malloc(size)) == NULL) {
		firmware_close(fh);
		return ENOMEM;
	}
	if ((error = firmware_read(fh, 0, fw, size)) != 0)
		firmware_free(fw, size);
	firmware_close(fh);
	if (error)
		return error;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AR_FW_DOWNLOAD;
	USETW(req.wIndex, 0);

	ptr = fw;
	addr >>= 8;
	while (size > 0) {
		mlen = MIN(size, 4096);

		USETW(req.wValue, addr);
		USETW(req.wLength, mlen);
		if (usbd_do_request(sc->sc_udev, &req, ptr) != 0) {
			error = EIO;
			break;
		}
		addr += mlen >> 8;
		ptr  += mlen;
		size -= mlen;
	}
	free(fw, M_DEVBUF);
	return error;
}

Static int
otus_open_pipes(struct otus_softc *sc)
{
	usb_endpoint_descriptor_t *ed;
	int i, isize, error;

	DPRINTFN(DBG_FN, sc, "\n");

	error = usbd_open_pipe(sc->sc_iface, AR_EPT_BULK_RX_NO, 0,
	    &sc->sc_data_rx_pipe);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not open Rx bulk pipe\n");
		goto fail;
	}

	ed = usbd_get_endpoint_descriptor(sc->sc_iface, AR_EPT_INTR_RX_NO);
	if (ed == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "could not retrieve Rx intr pipe descriptor\n");
		goto fail;
	}
	isize = UGETW(ed->wMaxPacketSize);
	if (isize == 0) {
		aprint_error_dev(sc->sc_dev,
		    "invalid Rx intr pipe descriptor\n");
		goto fail;
	}
	sc->sc_ibuf = malloc(isize, M_USBDEV, M_NOWAIT);
	if (sc->sc_ibuf == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate Rx intr buffer\n");
		goto fail;
	}
	error = usbd_open_pipe_intr(sc->sc_iface, AR_EPT_INTR_RX_NO,
	    USBD_SHORT_XFER_OK, &sc->sc_cmd_rx_pipe, sc, sc->sc_ibuf, isize,
	    otus_intr, USBD_DEFAULT_INTERVAL);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not open Rx intr pipe\n");
		goto fail;
	}

	error = usbd_open_pipe(sc->sc_iface, AR_EPT_BULK_TX_NO, 0,
	    &sc->sc_data_tx_pipe);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not open Tx bulk pipe\n");
		goto fail;
	}

	error = usbd_open_pipe(sc->sc_iface, AR_EPT_INTR_TX_NO, 0,
	    &sc->sc_cmd_tx_pipe);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev, "could not open Tx intr pipe\n");
		goto fail;
	}

	if (otus_alloc_tx_cmd(sc) != 0) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate command xfer\n");
		goto fail;
	}

	if (otus_alloc_tx_data_list(sc) != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate Tx xfers\n");
		goto fail;
	}

	if (otus_alloc_rx_data_list(sc) != 0) {
		aprint_error_dev(sc->sc_dev, "could not allocate Rx xfers\n");
		goto fail;
	}

	for (i = 0; i < OTUS_RX_DATA_LIST_COUNT; i++) {
		struct otus_rx_data *data;

		data = &sc->sc_rx_data[i];
		usbd_setup_xfer(data->xfer, sc->sc_data_rx_pipe, data, data->buf,
		    OTUS_RXBUFSZ, USBD_SHORT_XFER_OK | USBD_NO_COPY,
		    USBD_NO_TIMEOUT, otus_rxeof);
		error = usbd_transfer(data->xfer);
		if (error != USBD_IN_PROGRESS && error != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not queue Rx xfer\n");
			goto fail;
		}
	}
	return 0;

 fail:	otus_close_pipes(sc);
	return error;
}

Static void
otus_close_pipes(struct otus_softc *sc)
{

	DPRINTFN(DBG_FN, sc, "\n");

	otus_free_tx_cmd(sc);
	otus_free_tx_data_list(sc);
	otus_free_rx_data_list(sc);

	if (sc->sc_data_rx_pipe != NULL)
		usbd_close_pipe(sc->sc_data_rx_pipe);
	if (sc->sc_cmd_rx_pipe != NULL) {
		usbd_abort_pipe(sc->sc_cmd_rx_pipe);
		usbd_close_pipe(sc->sc_cmd_rx_pipe);
	}
	if (sc->sc_ibuf != NULL)
		free(sc->sc_ibuf, M_USBDEV);
	if (sc->sc_data_tx_pipe != NULL)
		usbd_close_pipe(sc->sc_data_tx_pipe);
	if (sc->sc_cmd_tx_pipe != NULL)
		usbd_close_pipe(sc->sc_cmd_tx_pipe);
}

Static int
otus_alloc_tx_cmd(struct otus_softc *sc)
{
	struct otus_tx_cmd *cmd;

	DPRINTFN(DBG_FN, sc, "\n");

	cmd = &sc->sc_tx_cmd;
	cmd->xfer = usbd_alloc_xfer(sc->sc_udev);
	if (cmd->xfer == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate xfer\n");
		return ENOMEM;
	}
	cmd->buf = usbd_alloc_buffer(cmd->xfer, OTUS_MAX_TXCMDSZ);
	if (cmd->buf == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "could not allocate xfer buffer\n");
		usbd_free_xfer(cmd->xfer);
		return ENOMEM;
	}
	return 0;
}

Static void
otus_free_tx_cmd(struct otus_softc *sc)
{

	DPRINTFN(DBG_FN, sc, "\n");

	/* Make sure no transfers are pending. */
	usbd_abort_pipe(sc->sc_cmd_tx_pipe);

	mutex_enter(&sc->sc_cmd_mtx);
	if (sc->sc_tx_cmd.xfer != NULL)
		usbd_free_xfer(sc->sc_tx_cmd.xfer);
	sc->sc_tx_cmd.xfer = NULL;
	sc->sc_tx_cmd.buf  = NULL;
	mutex_exit(&sc->sc_cmd_mtx);
}

Static int
otus_alloc_tx_data_list(struct otus_softc *sc)
{
	struct otus_tx_data *data;
	int i, error;

	DPRINTFN(DBG_FN, sc, "\n");

	mutex_enter(&sc->sc_tx_mtx);
	error = 0;
	TAILQ_INIT(&sc->sc_tx_free_list);
	for (i = 0; i < OTUS_TX_DATA_LIST_COUNT; i++) {
		data = &sc->sc_tx_data[i];

		data->sc = sc;  /* Backpointer for callbacks. */

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "could not allocate xfer\n");
			error = ENOMEM;
			break;
		}
		data->buf = usbd_alloc_buffer(data->xfer, OTUS_TXBUFSZ);
		if (data->buf == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "could not allocate xfer buffer\n");
			error = ENOMEM;
			break;
		}
		/* Append this Tx buffer to our free list. */
		TAILQ_INSERT_TAIL(&sc->sc_tx_free_list, data, next);
	}
	if (error != 0)
		otus_free_tx_data_list(sc);
	mutex_exit(&sc->sc_tx_mtx);
	return error;
}

Static void
otus_free_tx_data_list(struct otus_softc *sc)
{
	int i;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Make sure no transfers are pending. */
	usbd_abort_pipe(sc->sc_data_tx_pipe);

	for (i = 0; i < OTUS_TX_DATA_LIST_COUNT; i++) {
		if (sc->sc_tx_data[i].xfer != NULL)
			usbd_free_xfer(sc->sc_tx_data[i].xfer);
	}
}

Static int
otus_alloc_rx_data_list(struct otus_softc *sc)
{
	struct otus_rx_data *data;
	int i, error;

	DPRINTFN(DBG_FN, sc, "\n");

	for (i = 0; i < OTUS_RX_DATA_LIST_COUNT; i++) {
		data = &sc->sc_rx_data[i];

		data->sc = sc;	/* Backpointer for callbacks. */

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "could not allocate xfer\n");
			error = ENOMEM;
			goto fail;
		}
		data->buf = usbd_alloc_buffer(data->xfer, OTUS_RXBUFSZ);
		if (data->buf == NULL) {
			aprint_error_dev(sc->sc_dev,
			    "could not allocate xfer buffer\n");
			error = ENOMEM;
			goto fail;
		}
	}
	return 0;

fail:	otus_free_rx_data_list(sc);
	return error;
}

Static void
otus_free_rx_data_list(struct otus_softc *sc)
{
	int i;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Make sure no transfers are pending. */
	usbd_abort_pipe(sc->sc_data_rx_pipe);

	for (i = 0; i < OTUS_RX_DATA_LIST_COUNT; i++)
		if (sc->sc_rx_data[i].xfer != NULL)
			usbd_free_xfer(sc->sc_rx_data[i].xfer);
}

Static void
otus_next_scan(void *arg)
{
	struct otus_softc *sc;

	sc = arg;

	DPRINTFN(DBG_FN, sc, "\n");

	if (sc->sc_dying)
		return;

	if (sc->sc_ic.ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(&sc->sc_ic);
}

Static void
otus_task(void *arg)
{
	struct otus_softc *sc;
	struct otus_host_cmd_ring *ring;
	struct otus_host_cmd *cmd;
	int s;

	sc = arg;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Process host commands. */
	s = splusb();
	mutex_spin_enter(&sc->sc_task_mtx);
	ring = &sc->sc_cmdq;
	while (ring->next != ring->cur) {
		cmd = &ring->cmd[ring->next];
		mutex_spin_exit(&sc->sc_task_mtx);
		splx(s);

		/* Callback. */
		DPRINTFN(DBG_CMD, sc, "cb=%p queued=%d\n", cmd->cb,
		    ring->queued);
		cmd->cb(sc, cmd->data);

		s = splusb();
		mutex_spin_enter(&sc->sc_task_mtx);
		ring->queued--;
		ring->next = (ring->next + 1) % OTUS_HOST_CMD_RING_COUNT;
	}
	mutex_spin_exit(&sc->sc_task_mtx);
	wakeup(ring);
	splx(s);
}

Static void
otus_do_async(struct otus_softc *sc, void (*cb)(struct otus_softc *, void *),
    void *arg, int len)
{
	struct otus_host_cmd_ring *ring;
	struct otus_host_cmd *cmd;
	int s;

	DPRINTFN(DBG_FN, sc, "cb=%p\n", cb);


	s = splusb();
	mutex_spin_enter(&sc->sc_task_mtx);
	ring = &sc->sc_cmdq;
	cmd = &ring->cmd[ring->cur];
	cmd->cb = cb;
	KASSERT(len <= sizeof(cmd->data));
	memcpy(cmd->data, arg, len);
	ring->cur = (ring->cur + 1) % OTUS_HOST_CMD_RING_COUNT;

	/* If there is no pending command already, schedule a task. */
	if (++ring->queued == 1) {
		mutex_spin_exit(&sc->sc_task_mtx);
		usb_add_task(sc->sc_udev, &sc->sc_task, USB_TASKQ_DRIVER);
	}
	else
		mutex_spin_exit(&sc->sc_task_mtx);
	wakeup(ring);
	splx(s);
}

Static int
otus_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct otus_softc *sc;
	struct otus_cmd_newstate cmd;

	sc = ic->ic_ifp->if_softc;

	DPRINTFN(DBG_FN|DBG_STM, sc, "nstate=%s(%d), arg=%d\n",
	    ieee80211_state_name[nstate], nstate, arg);

	/* Do it in a process context. */
	cmd.state = nstate;
	cmd.arg = arg;
	otus_do_async(sc, otus_newstate_cb, &cmd, sizeof(cmd));
	return 0;
}

Static void
otus_newstate_cb(struct otus_softc *sc, void *arg)
{
	struct otus_cmd_newstate *cmd;
	struct ieee80211com *ic;
	struct ieee80211_node *ni;
	enum ieee80211_state nstate;
	int s;

	cmd = arg;
	ic = &sc->sc_ic;
	ni = ic->ic_bss;
	nstate = cmd->state;

#ifdef OTUS_DEBUG
	enum ieee80211_state ostate = ostate = ic->ic_state;
	DPRINTFN(DBG_FN|DBG_STM, sc, "%s(%d)->%s(%d)\n",
	    ieee80211_state_name[ostate], ostate,
	    ieee80211_state_name[nstate], nstate);
#endif

	s = splnet();

	callout_halt(&sc->sc_scan_to, NULL);
	callout_halt(&sc->sc_calib_to, NULL);

	mutex_enter(&sc->sc_write_mtx);

	switch (nstate) {
	case IEEE80211_S_INIT:
		break;

	case IEEE80211_S_SCAN:
		otus_set_chan(sc, ic->ic_curchan, 0);
		if (!sc->sc_dying)
			callout_schedule(&sc->sc_scan_to, hz / 5);
		break;

	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		otus_set_chan(sc, ic->ic_curchan, 0);
		break;

	case IEEE80211_S_RUN:
		otus_set_chan(sc, ic->ic_curchan, 1);

		switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			otus_updateslot_cb_locked(sc);
			otus_set_bssid(sc, ni->ni_bssid);

			/* Fake a join to init the Tx rate. */
			otus_newassoc(ni, 1);

			/* Start calibration timer. */
			if (!sc->sc_dying)
				callout_schedule(&sc->sc_calib_to, hz);
			break;

		case IEEE80211_M_IBSS:
		case IEEE80211_M_AHDEMO:
		case IEEE80211_M_HOSTAP:
		case IEEE80211_M_MONITOR:
			break;
		}
		break;
	}
	(void)sc->sc_newstate(ic, nstate, cmd->arg);
	sc->sc_led_newstate(sc);
	mutex_exit(&sc->sc_write_mtx);

	splx(s);
}

Static int
otus_cmd(struct otus_softc *sc, uint8_t code, const void *idata, int ilen,
    void *odata)
{
	struct otus_tx_cmd *cmd;
	struct ar_cmd_hdr *hdr;
	int s, xferlen, error;

	DPRINTFN(DBG_FN, sc, "\n");

	cmd = &sc->sc_tx_cmd;

	mutex_enter(&sc->sc_cmd_mtx);

	/* Always bulk-out a multiple of 4 bytes. */
	xferlen = roundup2(sizeof(*hdr) + ilen, 4);

	hdr = (void *)cmd->buf;
	if (hdr == NULL) {	/* we may have been freed while detaching */
		mutex_exit(&sc->sc_cmd_mtx);
		DPRINTFN(DBG_CMD, sc, "tx_cmd freed with commands pending\n");
		return 0;
	}
	hdr->code  = code;
	hdr->len   = ilen;
	hdr->token = ++cmd->token;	/* Don't care about endianness. */
	KASSERT(sizeof(hdr) + ilen <= OTUS_MAX_TXCMDSZ);
	memcpy(cmd->buf + sizeof(hdr[0]), idata, ilen);

	DPRINTFN(DBG_CMD, sc, "sending command code=0x%02x len=%d token=%d\n",
	    code, ilen, hdr->token);

	s = splusb();
	cmd->odata = odata;
	cmd->done = 0;
	usbd_setup_xfer(cmd->xfer, sc->sc_cmd_tx_pipe, cmd, cmd->buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, OTUS_CMD_TIMEOUT, NULL);
	error = usbd_sync_transfer(cmd->xfer);
	if (error != 0) {
		splx(s);
		mutex_exit(&sc->sc_cmd_mtx);
#if defined(DIAGNOSTIC) || defined(OTUS_DEBUG)	/* XXX: kill some noise */
		aprint_error_dev(sc->sc_dev,
		    "could not send command 0x%x (error=%s)\n",
		    code, usbd_errstr(error));
#endif
		return EIO;
	}
	if (!cmd->done)
		error = tsleep(cmd, PCATCH, "otuscmd", hz);
	cmd->odata = NULL;	/* In case answer is received too late. */
	splx(s);
	mutex_exit(&sc->sc_cmd_mtx);
	if (error != 0) {
		aprint_error_dev(sc->sc_dev,
		    "timeout waiting for command 0x%02x reply\n", code);
	}
	return error;
}

Static void
otus_write(struct otus_softc *sc, uint32_t reg, uint32_t val)
{

	DPRINTFN(DBG_FN|DBG_REG, sc, "reg=0x%x, val=0x%x\n", reg, val);

	KASSERT(mutex_owned(&sc->sc_write_mtx));
	KASSERT(sc->sc_write_idx < __arraycount(sc->sc_write_buf));

	sc->sc_write_buf[sc->sc_write_idx].reg = htole32(reg);
	sc->sc_write_buf[sc->sc_write_idx].val = htole32(val);

	if (++sc->sc_write_idx >= __arraycount(sc->sc_write_buf))
		(void)otus_write_barrier(sc);
}

Static int
otus_write_barrier(struct otus_softc *sc)
{
	int error;

	DPRINTFN(DBG_FN, sc, "\n");

	KASSERT(mutex_owned(&sc->sc_write_mtx));
	KASSERT(sc->sc_write_idx <= __arraycount(sc->sc_write_buf));

	if (sc->sc_write_idx == 0)
		return 0;	/* Nothing to flush. */

	error = otus_cmd(sc, AR_CMD_WREG, sc->sc_write_buf,
	    sizeof(sc->sc_write_buf[0]) * sc->sc_write_idx, NULL);

	sc->sc_write_idx = 0;
	if (error)
		DPRINTFN(DBG_REG, sc, "error=%d\n", error);
	return error;
}

Static struct ieee80211_node *
otus_node_alloc(struct ieee80211_node_table *ntp)
{
	struct otus_node *on;

	DPRINTFN(DBG_FN, DBG_NO_SC, "\n");

	on = malloc(sizeof(*on), M_DEVBUF, M_NOWAIT | M_ZERO);
	return &on->ni;
}

Static int
otus_media_change(struct ifnet *ifp)
{
	struct otus_softc *sc;
	struct ieee80211com *ic;
	uint8_t rate, ridx;
	int error;

	sc = ifp->if_softc;

	DPRINTFN(DBG_FN, sc, "\n");

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	ic = &sc->sc_ic;
	if (ic->ic_fixed_rate != -1) {
		rate = ic->ic_sup_rates[ic->ic_curmode].
		    rs_rates[ic->ic_fixed_rate] & IEEE80211_RATE_VAL;
		for (ridx = 0; ridx <= OTUS_RIDX_MAX; ridx++)
			if (otus_rates[ridx].rate == rate)
				break;
		sc->sc_fixed_ridx = ridx;
	}

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		error = otus_init(ifp);

	return error;
}

Static int
otus_read_eeprom(struct otus_softc *sc)
{
	uint32_t regs[8], reg;
	uint8_t *eep;
	int i, j, error;

	DPRINTFN(DBG_FN, sc, "\n");

	KASSERT(sizeof(sc->sc_eeprom) % 32 == 0);

	/* Read EEPROM by blocks of 32 bytes. */
	eep = (uint8_t *)&sc->sc_eeprom;
	reg = AR_EEPROM_OFFSET;
	for (i = 0; i < sizeof(sc->sc_eeprom) / 32; i++) {
		for (j = 0; j < 8; j++, reg += 4)
			regs[j] = htole32(reg);
		error = otus_cmd(sc, AR_CMD_RREG, regs, sizeof(regs), eep);
		if (error != 0)
			break;
		eep += 32;
	}
	return error;
}

Static void
otus_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct ieee80211_rateset *rs;
	struct otus_softc *sc;
	struct otus_node *on;
	uint8_t rate;
	int ridx, i;

	sc = ni->ni_ic->ic_ifp->if_softc;

	DPRINTFN(DBG_FN, sc, "isnew=%d addr=%s\n",
	    isnew, ether_sprintf(ni->ni_macaddr));

	on = (void *)ni;
	ieee80211_amrr_node_init(&sc->sc_amrr, &on->amn);
	/* Start at lowest available bit-rate, AMRR will raise. */
	ni->ni_txrate = 0;
	rs = &ni->ni_rates;
	for (i = 0; i < rs->rs_nrates; i++) {
		rate = rs->rs_rates[i] & IEEE80211_RATE_VAL;
		/* Convert 802.11 rate to hardware rate index. */
		for (ridx = 0; ridx <= OTUS_RIDX_MAX; ridx++)
			if (otus_rates[ridx].rate == rate)
				break;
		on->ridx[i] = ridx;
		DPRINTFN(DBG_INIT, sc, "rate=0x%02x ridx=%d\n",
		    rs->rs_rates[i], on->ridx[i]);
	}
}

/* ARGSUSED */
Static void
otus_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
#if 0
	struct otus_softc *sc;
	int len;

	sc = priv;

	DPRINTFN(DBG_FN, sc, "\n");

	/*
	 * The Rx intr pipe is unused with current firmware.  Notifications
	 * and replies to commands are sent through the Rx bulk pipe instead
	 * (with a magic PLCP header.)
	 */
	if (__predict_false(status != USBD_NORMAL_COMPLETION)) {
		DPRINTFN(DBG_INTR, sc, "status=%d\n", status);
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_cmd_rx_pipe);
		return;
	}
	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	otus_cmd_rxeof(sc, sc->sc_ibuf, len);
#endif
}

Static void
otus_cmd_rxeof(struct otus_softc *sc, uint8_t *buf, int len)
{
	struct ieee80211com *ic;
	struct otus_tx_cmd *cmd;
	struct ar_cmd_hdr *hdr;
	int s;

	DPRINTFN(DBG_FN, sc, "\n");

	ic = &sc->sc_ic;

	if (__predict_false(len < sizeof(*hdr))) {
		DPRINTFN(DBG_RX, sc, "cmd too small %d\n", len);
		return;
	}
	hdr = (void *)buf;
	if (__predict_false(sizeof(*hdr) + hdr->len > len ||
	    sizeof(*hdr) + hdr->len > 64)) {
		DPRINTFN(DBG_RX, sc, "cmd too large %d\n", hdr->len);
		return;
	}

	if ((hdr->code & 0xc0) != 0xc0) {
		DPRINTFN(DBG_RX, sc, "received reply code=0x%02x len=%d token=%d\n",
		    hdr->code, hdr->len, hdr->token);
		cmd = &sc->sc_tx_cmd;
		if (__predict_false(hdr->token != cmd->token))
			return;
		/* Copy answer into caller's supplied buffer. */
		if (cmd->odata != NULL)
			memcpy(cmd->odata, &hdr[1], hdr->len);
		cmd->done = 1;
		wakeup(cmd);
		return;
	}

	/* Received unsolicited notification. */
	DPRINTFN(DBG_RX, sc, "received notification code=0x%02x len=%d\n",
	    hdr->code, hdr->len);
	switch (hdr->code & 0x3f) {
	case AR_EVT_BEACON:
		break;
	case AR_EVT_TX_COMP:
	{
		struct ar_evt_tx_comp *tx;
		struct ieee80211_node *ni;
		struct otus_node *on;

		tx = (void *)&hdr[1];

		DPRINTFN(DBG_RX, sc, "tx completed %s status=%d phy=0x%x\n",
		    ether_sprintf(tx->macaddr), le16toh(tx->status),
		    le32toh(tx->phy));
		s = splnet();
#ifdef notyet
#ifndef IEEE80211_STA_ONLY
		if (ic->ic_opmode != IEEE80211_M_STA) {
			ni = ieee80211_find_node(ic, tx->macaddr);
			if (__predict_false(ni == NULL)) {
				splx(s);
				break;
			}
		} else
#endif
#endif
			ni = ic->ic_bss;
		/* Update rate control statistics. */
		on = (void *)ni;
		/* NB: we do not set the TX_MAC_RATE_PROBING flag. */
		if (__predict_true(tx->status != 0))
			on->amn.amn_retrycnt++;
		splx(s);
		break;
	}
	case AR_EVT_TBTT:
		break;
	}
}

Static void
otus_sub_rxeof(struct otus_softc *sc, uint8_t *buf, int len)
{
	struct ieee80211com *ic;
	struct ifnet *ifp;
	struct ieee80211_node *ni;
	struct ar_rx_tail *tail;
	struct ieee80211_frame *wh;
	struct mbuf *m;
	uint8_t *plcp;
	int s, mlen, align;

	DPRINTFN(DBG_FN, sc, "\n");

	ic = &sc->sc_ic;
	ifp = ic->ic_ifp;

	if (__predict_false(len < AR_PLCP_HDR_LEN)) {
		DPRINTFN(DBG_RX, sc, "sub-xfer too short %d\n", len);
		return;
	}
	plcp = buf;

	/* All bits in the PLCP header are set to 1 for non-MPDU. */
	if (memcmp(plcp, AR_PLCP_HDR_INTR, AR_PLCP_HDR_LEN) == 0) {
		otus_cmd_rxeof(sc, plcp + AR_PLCP_HDR_LEN,
		    len - AR_PLCP_HDR_LEN);
		return;
	}

	/* Received MPDU. */
	if (__predict_false(len < AR_PLCP_HDR_LEN + sizeof(*tail))) {
		DPRINTFN(DBG_RX, sc, "MPDU too short %d\n", len);
		ifp->if_ierrors++;
		return;
	}
	tail = (void *)(plcp + len - sizeof(*tail));
	wh = (void *)(plcp + AR_PLCP_HDR_LEN);

	/* Discard error frames. */
	if (__predict_false((tail->error & sc->sc_rx_error_msk) != 0)) {
		DPRINTFN(DBG_RX, sc, "error frame 0x%02x\n", tail->error);
		if (tail->error & AR_RX_ERROR_FCS) {
			DPRINTFN(DBG_RX, sc, "bad FCS\n");
		} else if (tail->error & AR_RX_ERROR_MMIC) {
			/* Report Michael MIC failures to net80211. */
			ieee80211_notify_michael_failure(ic, wh, 0 /* XXX: keyix */);
		}
		ifp->if_ierrors++;
		return;
	}
	/* Compute MPDU's length. */
	mlen = len - AR_PLCP_HDR_LEN - sizeof(*tail);
	mlen -= IEEE80211_CRC_LEN;	/* strip 802.11 FCS */
	/* Make sure there's room for an 802.11 header. */
	/*
	 * XXX: This will drop most control packets.  Do we really
	 * want this in IEEE80211_M_MONITOR mode?
	 */
	if (__predict_false(mlen < sizeof(*wh))) {
		ifp->if_ierrors++;
		return;
	}

	/* Provide a 32-bit aligned protocol header to the stack. */
	align = (ieee80211_has_qos(wh) ^ ieee80211_has_addr4(wh)) ? 2 : 0;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (__predict_false(m == NULL)) {
		ifp->if_ierrors++;
		return;
	}
	if (align + mlen > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if (__predict_false(!(m->m_flags & M_EXT))) {
			ifp->if_ierrors++;
			m_freem(m);
			return;
		}
	}
	/* Finalize mbuf. */
	m->m_pkthdr.rcvif = ifp;
	m->m_data += align;
	memcpy(mtod(m, void *), wh, mlen);
	m->m_pkthdr.len = m->m_len = mlen;

	s = splnet();
	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct otus_rx_radiotap_header *tap;

		tap = &sc->sc_rxtap;
		tap->wr_flags = 0;
		tap->wr_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wr_antsignal = tail->rssi;
		tap->wr_rate = 2;	/* In case it can't be found below. */
		switch (tail->status & AR_RX_STATUS_MT_MASK) {
		case AR_RX_STATUS_MT_CCK:
			switch (plcp[0]) {
			case  10: tap->wr_rate =   2; break;
			case  20: tap->wr_rate =   4; break;
			case  55: tap->wr_rate =  11; break;
			case 110: tap->wr_rate =  22; break;
			}
			if (tail->status & AR_RX_STATUS_SHPREAMBLE)
				tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
			break;
		case AR_RX_STATUS_MT_OFDM:
			switch (plcp[0] & 0xf) {
			case 0xb: tap->wr_rate =  12; break;
			case 0xf: tap->wr_rate =  18; break;
			case 0xa: tap->wr_rate =  24; break;
			case 0xe: tap->wr_rate =  36; break;
			case 0x9: tap->wr_rate =  48; break;
			case 0xd: tap->wr_rate =  72; break;
			case 0x8: tap->wr_rate =  96; break;
			case 0xc: tap->wr_rate = 108; break;
			}
			break;
		}
		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
	}

	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);

	/* push the frame up to the 802.11 stack */
	ieee80211_input(ic, m, ni, tail->rssi, 0);

	/* Node is no longer needed. */
	ieee80211_free_node(ni);
	splx(s);
}

Static void
otus_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct otus_rx_data *data;
	struct otus_softc *sc;
	uint8_t *buf;
	struct ar_rx_head *head;
	uint16_t hlen;
	int len;

	data = priv;
	sc = data->sc;

	DPRINTFN(DBG_FN, sc, "\n");

	buf = data->buf;

	if (__predict_false(status != USBD_NORMAL_COMPLETION)) {
		DPRINTFN(DBG_RX, sc, "RX status=%d\n", status);
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_data_rx_pipe);
		else if (status != USBD_CANCELLED) {
			DPRINTFN(DBG_RX, sc,
			    "otus_rxeof: goto resubmit: status=%d\n", status);
			goto resubmit;
		}
		return;
	}
	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	while (len >= sizeof(*head)) {
		head = (void *)buf;
		if (__predict_false(head->tag != htole16(AR_RX_HEAD_TAG))) {
			DPRINTFN(DBG_RX, sc, "tag not valid 0x%x\n",
			    le16toh(head->tag));
			break;
		}
		hlen = le16toh(head->len);
		if (__predict_false(sizeof(*head) + hlen > len)) {
			DPRINTFN(DBG_RX, sc, "xfer too short %d/%d\n",
			    len, hlen);
			break;
		}
		/* Process sub-xfer. */
		otus_sub_rxeof(sc, (uint8_t *)&head[1], hlen);

		/* Next sub-xfer is aligned on a 32-bit boundary. */
		hlen = roundup2(sizeof(*head) + hlen, 4);
		buf += hlen;
		len -= hlen;
	}

 resubmit:
	usbd_setup_xfer(xfer, sc->sc_data_rx_pipe, data, data->buf, OTUS_RXBUFSZ,
	    USBD_SHORT_XFER_OK | USBD_NO_COPY, USBD_NO_TIMEOUT, otus_rxeof);
	(void)usbd_transfer(data->xfer);
}

Static void
otus_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct otus_tx_data *data;
	struct otus_softc *sc;
	struct ieee80211com *ic;
	struct ifnet *ifp;
	int s;

	data = priv;
	sc = data->sc;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Put this Tx buffer back to the free list. */
	mutex_enter(&sc->sc_tx_mtx);
	TAILQ_INSERT_TAIL(&sc->sc_tx_free_list, data, next);
	mutex_exit(&sc->sc_tx_mtx);

	ic = &sc->sc_ic;
	ifp = ic->ic_ifp;
	if (__predict_false(status != USBD_NORMAL_COMPLETION)) {
		DPRINTFN(DBG_TX, sc, "TX status=%d\n", status);
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->sc_data_tx_pipe);
		ifp->if_oerrors++;
		return;
	}
	ifp->if_opackets++;

	s = splnet();
	sc->sc_tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;	/* XXX: do after freeing Tx buffer? */
	otus_start(ifp);
	splx(s);
}

Static int
otus_tx(struct otus_softc *sc, struct mbuf *m, struct ieee80211_node *ni,
    struct otus_tx_data *data)
{
	struct ieee80211com *ic;
	struct otus_node *on;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	struct ar_tx_head *head;
	uint32_t phyctl;
	uint16_t macctl, qos;
	uint8_t qid;
	int error, ridx, hasqos, xferlen;

	DPRINTFN(DBG_FN, sc, "\n");

	ic = &sc->sc_ic;
	on = (void *)ni;

	wh = mtod(m, struct ieee80211_frame *);
	if ((wh->i_fc[1] & IEEE80211_FC1_PROTECTED)) {
		/* XXX: derived from upgt_tx_task() and ural_tx_data() */
		k = ieee80211_crypto_encap(ic, ni, m);
		if (k == NULL)
			return ENOBUFS;

		/* Packet header may have moved, reset our local pointer. */
		wh = mtod(m, struct ieee80211_frame *);
	}

#ifdef HAVE_EDCA
	if ((hasqos = ieee80211_has_qos(wh))) {
		qos = ieee80211_get_qos(wh);
		qid = ieee80211_up_to_ac(ic, qos & IEEE80211_QOS_TID);
	} else {
		qos = 0;
		qid = WME_AC_BE;
	}
#else
	hasqos = 0;
	qos = 0;
	qid = WME_AC_BE;
#endif

	/* Pickup a rate index. */
	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_DATA)
		ridx = (ic->ic_curmode == IEEE80211_MODE_11A) ?
		    OTUS_RIDX_OFDM6 : OTUS_RIDX_CCK1;
	else if (ic->ic_fixed_rate != -1)
		ridx = sc->sc_fixed_ridx;
	else
		ridx = on->ridx[ni->ni_txrate];

	phyctl = 0;
	macctl = AR_TX_MAC_BACKOFF | AR_TX_MAC_HW_DUR | AR_TX_MAC_QID(qid);

	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    (hasqos && ((qos & IEEE80211_QOS_ACKPOLICY_MASK) ==
	     IEEE80211_QOS_ACKPOLICY_NOACK)))
		macctl |= AR_TX_MAC_NOACK;

	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		if (m->m_pkthdr.len + IEEE80211_CRC_LEN >= ic->ic_rtsthreshold)
			macctl |= AR_TX_MAC_RTS;
		else if ((ic->ic_flags & IEEE80211_F_USEPROT) &&
		    ridx >= OTUS_RIDX_OFDM6) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				macctl |= AR_TX_MAC_CTS;
			else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
				macctl |= AR_TX_MAC_RTS;
		}
	}

	phyctl |= AR_TX_PHY_MCS(otus_rates[ridx].mcs);
	if (ridx >= OTUS_RIDX_OFDM6) {
		phyctl |= AR_TX_PHY_MT_OFDM;
		if (ridx <= OTUS_RIDX_OFDM24)
			phyctl |= AR_TX_PHY_ANTMSK(sc->sc_txmask);
		else
			phyctl |= AR_TX_PHY_ANTMSK(1);
	} else {	/* CCK */
		phyctl |= AR_TX_PHY_MT_CCK;
		phyctl |= AR_TX_PHY_ANTMSK(sc->sc_txmask);
	}

	/* Update rate control stats for frames that are ACK'ed. */
	if (!(macctl & AR_TX_MAC_NOACK))
		on->amn.amn_txcnt++;

	/* Fill Tx descriptor. */
	head = (void *)data->buf;
	head->len = htole16(m->m_pkthdr.len + IEEE80211_CRC_LEN);
	head->macctl = htole16(macctl);
	head->phyctl = htole32(phyctl);

	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct otus_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		if (wh->i_fc[1] & IEEE80211_FC1_WEP)
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;
		tap->wt_rate = otus_rates[ridx].rate;
		tap->wt_chan_freq = htole16(ic->ic_curchan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_curchan->ic_flags);

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m);
	}

	xferlen = sizeof(*head) + m->m_pkthdr.len;
	m_copydata(m, 0, m->m_pkthdr.len, (void *)&head[1]);

	DPRINTFN(DBG_TX, sc, "queued len=%d mac=0x%04x phy=0x%08x rate=%d\n",
	    head->len, head->macctl, head->phyctl, otus_rates[ridx].rate);

	usbd_setup_xfer(data->xfer, sc->sc_data_tx_pipe, data, data->buf, xferlen,
	    USBD_FORCE_SHORT_XFER | USBD_NO_COPY, OTUS_TX_TIMEOUT, otus_txeof);
	error = usbd_transfer(data->xfer);
	if (__predict_false(
		    error != USBD_NORMAL_COMPLETION &&
		    error != USBD_IN_PROGRESS)) {
		DPRINTFN(DBG_TX, sc, "transfer failed %d\n", error);
		return error;
	}
	return 0;
}

Static void
otus_start(struct ifnet *ifp)
{
	struct otus_softc *sc;
	struct ieee80211com *ic;
	struct otus_tx_data *data;
	struct ether_header *eh;
	struct ieee80211_node *ni;
	struct mbuf *m;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	sc = ifp->if_softc;
	ic = &sc->sc_ic;

	DPRINTFN(DBG_FN, sc, "\n");

	data = NULL;
	for (;;) {
		/*
		 * Grab a Tx buffer if we don't already have one.  If
		 * one isn't available, bail out.
		 * NB: We must obtain this Tx buffer _before_
		 * dequeueing anything as one may not be available
		 * later.  Both must be done inside a single lock.
		 */
		mutex_enter(&sc->sc_tx_mtx);
		if (data == NULL && !TAILQ_EMPTY(&sc->sc_tx_free_list)) {
			data = TAILQ_FIRST(&sc->sc_tx_free_list);
			TAILQ_REMOVE(&sc->sc_tx_free_list, data, next);
		}
		mutex_exit(&sc->sc_tx_mtx);

		if (data == NULL) {
			ifp->if_flags |= IFF_OACTIVE;
			DPRINTFN(DBG_TX, sc, "empty sc_tx_free_list\n");
			return;
		}

		/* Send pending management frames first. */
		IF_DEQUEUE(&ic->ic_mgtq, m);
		if (m != NULL) {
			ni = (void *)m->m_pkthdr.rcvif;
			m->m_pkthdr.rcvif = NULL;
			goto sendit;
		}

		if (ic->ic_state != IEEE80211_S_RUN)
			break;

		/* Encapsulate and send data frames. */
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		if (m->m_len < (int)sizeof(*eh) &&
		    (m = m_pullup(m, sizeof(*eh))) == NULL) {
			ifp->if_oerrors++;
			continue;
		}

		eh = mtod(m, struct ether_header *);
		ni = ieee80211_find_txnode(ic, eh->ether_dhost);
		if (ni == NULL) {
			m_freem(m);
			ifp->if_oerrors++;
			continue;
		}

		bpf_mtap(ifp, m);

		if ((m = ieee80211_encap(ic, m, ni)) == NULL) {
			/* original m was freed by ieee80211_encap() */
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}
 sendit:
		bpf_mtap3(ic->ic_rawbpf, m);

		if (otus_tx(sc, m, ni, data) != 0) {
			m_freem(m);
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}

		data = NULL;	/* we're finished with this data buffer */
		m_freem(m);
		ieee80211_free_node(ni);
		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}

	/*
	 * If here, we have a Tx buffer, but ran out of mbufs to
	 * transmit.  Put the Tx buffer back to the free list.
	 */
	mutex_enter(&sc->sc_tx_mtx);
	TAILQ_INSERT_TAIL(&sc->sc_tx_free_list, data, next);
	mutex_exit(&sc->sc_tx_mtx);
}

Static void
otus_watchdog(struct ifnet *ifp)
{
	struct otus_softc *sc;

	sc = ifp->if_softc;

	DPRINTFN(DBG_FN, sc, "\n");

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			aprint_error_dev(sc->sc_dev, "device timeout\n");
			/* otus_init(ifp); XXX needs a process context! */
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}
	ieee80211_watchdog(&sc->sc_ic);
}

Static int
otus_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct otus_softc *sc;
	struct ieee80211com *ic;
	int s, error = 0;

	sc = ifp->if_softc;

	DPRINTFN(DBG_FN, sc, "0x%lx\n", cmd);

	ic = &sc->sc_ic;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
#ifdef INET
		struct ifaddr *ifa = data;
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&ic->ic_ac, ifa);
#endif
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, cmd, data)) != 0)
			break;

		switch (ifp->if_flags & (IFF_UP | IFF_RUNNING)) {
		case IFF_UP | IFF_RUNNING:
			if (((ifp->if_flags ^ sc->sc_if_flags) &
				(IFF_ALLMULTI | IFF_PROMISC)) != 0)
				otus_set_multi(sc);
			break;
		case IFF_UP:
			otus_init(ifp);
			break;

		case IFF_RUNNING:
			otus_stop(ifp);
			break;
		case 0:
		default:
			break;
		}
		sc->sc_if_flags = ifp->if_flags;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((error = ether_ioctl(ifp, cmd, data)) == ENETRESET) {
			/* setup multicast filter, etc */
			/* XXX: ??? */
			error = 0;
		}
		break;

	case SIOCS80211CHANNEL:
		/*
		 * This allows for fast channel switching in monitor mode
		 * (used by kismet). In IBSS mode, we must explicitly reset
		 * the interface to generate a new beacon frame.
		 */
		error = ieee80211_ioctl(ic, cmd, data);

		DPRINTFN(DBG_CHAN, sc,
		    "ic_curchan=%d ic_ibss_chan=%d ic_des_chan=%d ni_chan=%d error=%d\n",
		    ieee80211_chan2ieee(ic, ic->ic_curchan),
		    ieee80211_chan2ieee(ic, ic->ic_ibss_chan),
		    ieee80211_chan2ieee(ic, ic->ic_des_chan),
		    ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan),
		    error);

		if (error == ENETRESET &&
		    ic->ic_opmode == IEEE80211_M_MONITOR) {
			if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
			    (IFF_UP | IFF_RUNNING)) {
				mutex_enter(&sc->sc_write_mtx);
				otus_set_chan(sc, ic->ic_curchan, 0);
				mutex_exit(&sc->sc_write_mtx);
			}
			error = 0;
		}
		break;

	default:
		error = ieee80211_ioctl(ic, cmd, data);
	}
	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			otus_init(ifp);
		error = 0;
	}
	splx(s);
	return error;
}

Static int
otus_set_multi(struct otus_softc *sc)
{
	struct ifnet *ifp;
	struct ether_multi *enm;
	struct ether_multistep step;
	uint32_t lo, hi;
	uint8_t bit;
	int error;

	DPRINTFN(DBG_FN, sc, "\n");

	ifp = sc->sc_ic.ic_ifp;
	if ((ifp->if_flags & (IFF_ALLMULTI | IFF_PROMISC)) != 0) {
		lo = hi = 0xffffffff;
		goto done;
	}
	lo = hi = 0;
	ETHER_FIRST_MULTI(step, &sc->sc_ec, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			ifp->if_flags |= IFF_ALLMULTI;
			lo = hi = 0xffffffff;
			goto done;
		}
		bit = enm->enm_addrlo[5] >> 2;
		if (bit < 32)
			lo |= 1 << bit;
		else
			hi |= 1 << (bit - 32);
		ETHER_NEXT_MULTI(step, enm);
	}
 done:
	mutex_enter(&sc->sc_write_mtx);
	hi |= 1 << 31;	/* Make sure the broadcast bit is set. */
	otus_write(sc, AR_MAC_REG_GROUP_HASH_TBL_L, lo);
	otus_write(sc, AR_MAC_REG_GROUP_HASH_TBL_H, hi);
	error = otus_write_barrier(sc);
	mutex_exit(&sc->sc_write_mtx);
	return error;
}

#ifdef HAVE_EDCA
Static void
otus_updateedca(struct ieee80211com *ic)
{

	DPRINTFN(DBG_FN, DBG_NO_SC, "\n");

	/* Do it in a process context. */
	otus_do_async(ic->ic_ifp->if_softc, otus_updateedca_cb, NULL, 0);
}

Static void
otus_updateedca_cb(struct otus_softc *sc, void *arg __used)
{

	DPRINTFN(DBG_FN, sc, "\n");

	mutex_enter(&sc->sc_write_mtx);
	otus_updateedca_cb_locked(sc);
	mutex_exit(&sc->sc_write_mtx);
}
#endif

Static void
otus_updateedca_cb_locked(struct otus_softc *sc)
{
#ifdef HAVE_EDCA
	struct ieee80211com *ic;
#endif
	const struct ieee80211_edca_ac_params *edca;
	int s;

	DPRINTFN(DBG_FN, sc, "\n");

	KASSERT(mutex_owned(&sc->sc_write_mtx));

	s = splnet();

#ifdef HAVE_EDCA
	ic = &sc->sc_ic;
	edca = (ic->ic_flags & IEEE80211_F_QOS) ?
	    ic->ic_edca_ac : otus_edca_def;
#else
	edca = otus_edca_def;
#endif /* HAVE_EDCA */

#define EXP2(val)	((1 << (val)) - 1)
#define AIFS(val)	((val) * 9 + 10)

	/* Set CWmin/CWmax values. */
	otus_write(sc, AR_MAC_REG_AC0_CW,
	    EXP2(edca[WME_AC_BE].ac_ecwmax) << 16 |
	    EXP2(edca[WME_AC_BE].ac_ecwmin));
	otus_write(sc, AR_MAC_REG_AC1_CW,
	    EXP2(edca[WME_AC_BK].ac_ecwmax) << 16 |
	    EXP2(edca[WME_AC_BK].ac_ecwmin));
	otus_write(sc, AR_MAC_REG_AC2_CW,
	    EXP2(edca[WME_AC_VI].ac_ecwmax) << 16 |
	    EXP2(edca[WME_AC_VI].ac_ecwmin));
	otus_write(sc, AR_MAC_REG_AC3_CW,
	    EXP2(edca[WME_AC_VO].ac_ecwmax) << 16 |
	    EXP2(edca[WME_AC_VO].ac_ecwmin));
	otus_write(sc, AR_MAC_REG_AC4_CW,		/* Special TXQ. */
	    EXP2(edca[WME_AC_VO].ac_ecwmax) << 16 |
	    EXP2(edca[WME_AC_VO].ac_ecwmin));

	/* Set AIFSN values. */
	otus_write(sc, AR_MAC_REG_AC1_AC0_AIFS,
	    AIFS(edca[WME_AC_VI].ac_aifsn) << 24 |
	    AIFS(edca[WME_AC_BK].ac_aifsn) << 12 |
	    AIFS(edca[WME_AC_BE].ac_aifsn));
	otus_write(sc, AR_MAC_REG_AC3_AC2_AIFS,
	    AIFS(edca[WME_AC_VO].ac_aifsn) << 16 |	/* Special TXQ. */
	    AIFS(edca[WME_AC_VO].ac_aifsn) <<  4 |
	    AIFS(edca[WME_AC_VI].ac_aifsn) >>  8);

	/* Set TXOP limit. */
	otus_write(sc, AR_MAC_REG_AC1_AC0_TXOP,
	    edca[WME_AC_BK].ac_txoplimit << 16 |
	    edca[WME_AC_BE].ac_txoplimit);
	otus_write(sc, AR_MAC_REG_AC3_AC2_TXOP,
	    edca[WME_AC_VO].ac_txoplimit << 16 |
	    edca[WME_AC_VI].ac_txoplimit);
#undef AIFS
#undef EXP2

	splx(s);

	(void)otus_write_barrier(sc);
}

Static void
otus_updateslot(struct ifnet *ifp)
{
	struct otus_softc *sc;

	sc = ifp->if_softc;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Do it in a process context. */
	otus_do_async(sc, otus_updateslot_cb, NULL, 0);
}

/* ARGSUSED */
Static void
otus_updateslot_cb(struct otus_softc *sc, void *arg)
{

	DPRINTFN(DBG_FN, sc, "\n");

	mutex_enter(&sc->sc_write_mtx);
	otus_updateslot_cb_locked(sc);
	mutex_exit(&sc->sc_write_mtx);
}

Static void
otus_updateslot_cb_locked(struct otus_softc *sc)
{
	uint32_t slottime;

	DPRINTFN(DBG_FN, sc, "\n");

	KASSERT(mutex_owned(&sc->sc_write_mtx));

	slottime = (sc->sc_ic.ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20;
	otus_write(sc, AR_MAC_REG_SLOT_TIME, slottime << 10);
	(void)otus_write_barrier(sc);
}

Static int
otus_init_mac(struct otus_softc *sc)
{
	int error;

	DPRINTFN(DBG_FN|DBG_INIT, sc, "\n");

	KASSERT(mutex_owned(&sc->sc_write_mtx));

	otus_write(sc, AR_MAC_REG_ACK_EXTENSION, 0x40);
	otus_write(sc, AR_MAC_REG_RETRY_MAX, 0);
	otus_write(sc, AR_MAC_REG_SNIFFER, AR_MAC_REG_SNIFFER_DEFAULTS);
	otus_write(sc, AR_MAC_REG_RX_THRESHOLD, 0xc1f80);
	otus_write(sc, AR_MAC_REG_RX_PE_DELAY, 0x70);
	otus_write(sc, AR_MAC_REG_EIFS_AND_SIFS, 0xa144000);
	otus_write(sc, AR_MAC_REG_SLOT_TIME, 9 << 10);

	/* CF-END mode */
	otus_write(sc, 0x1c3b2c, 0x19000000);

	/* NAV protects ACK only (in TXOP). */
	otus_write(sc, 0x1c3b38, 0x201);

	/* Set beacon PHY CTRL's TPC to 0x7, TA1=1 */
	/* OTUS set AM to 0x1 */
	otus_write(sc, AR_MAC_REG_BCN_HT1, 0x8000170);

	otus_write(sc, AR_MAC_REG_BACKOFF_PROTECT, 0x105);

	/* AGG test code*/
	/* Aggregation MAX number and timeout */
	otus_write(sc, AR_MAC_REG_AMPDU_FACTOR, 0x10000a);

	/* Filter any control frames, BAR is bit 24. */
	otus_write(sc, AR_MAC_REG_FRAMETYPE_FILTER, AR_MAC_REG_FTF_DEFAULTS);

	/* Enable deaggregator, response in sniffer mode */
	otus_write(sc, 0x1c3c40, 0x1 | 1 << 30);	/* XXX: was 0x1 */

	/* rate sets */
	otus_write(sc, AR_MAC_REG_BASIC_RATE, 0x150f);
	otus_write(sc, AR_MAC_REG_MANDATORY_RATE, 0x150f);
	otus_write(sc, AR_MAC_REG_RTS_CTS_RATE, 0x10b01bb);

	/* MIMO response control */
	otus_write(sc, 0x1c3694, 0x4003c1e);	/* bit 26~28  otus-AM */

	/* Switch MAC to OTUS interface. */
	otus_write(sc, 0x1c3600, 0x3);

	otus_write(sc, AR_MAC_REG_AMPDU_RX_THRESH, 0xffff);

	/* set PHY register read timeout (??) */
	otus_write(sc, AR_MAC_REG_MISC_680, 0xf00008);

	/* Disable Rx TimeOut, workaround for BB. */
	otus_write(sc, AR_MAC_REG_RX_TIMEOUT, 0x0);

	/* Set clock frequency to 88/80MHz. */
	otus_write(sc, AR_PWR_REG_CLOCK_SEL,
	    AR_PWR_CLK_AHB_80_88MHZ | AR_PWR_CLK_DAC_160_INV_DLY);

	/* Set WLAN DMA interrupt mode: generate intr per packet. */
	otus_write(sc, AR_MAC_REG_TXRX_MPI, 0x110011);

	otus_write(sc, AR_MAC_REG_FCS_SELECT, AR_MAC_FCS_FIFO_PROT);

	/* Disables the CF_END frame, undocumented register */
	otus_write(sc, AR_MAC_REG_TXOP_NOT_ENOUGH_INDICATION, 0x141e0f48);

	/* Disable HW decryption for now. */
	otus_write(sc, AR_MAC_REG_ENCRYPTION,
	    AR_MAC_REG_ENCRYPTION_DEFAULTS | AR_MAC_REG_ENCRYPTION_RX_SOFTWARE);

	/*
	 * XXX: should these be elsewhere?
	 */
	/* Enable LED0 and LED1. */
	otus_write(sc, AR_GPIO_REG_PORT_TYPE, 3);
	otus_write(sc, AR_GPIO_REG_DATA,
	    AR_GPIO_REG_DATA_LED0_ON | AR_GPIO_REG_DATA_LED1_ON);

	/* Set USB Rx stream mode maximum frame number to 2. */
	otus_write(sc, AR_USB_REG_MAX_AGG_UPLOAD, (1 << 2));

	/* Set USB Rx stream mode timeout to 10us. */
	otus_write(sc, AR_USB_REG_UPLOAD_TIME_CTL, 0x80);

	if ((error = otus_write_barrier(sc)) != 0)
		return error;

	/* Set default EDCA parameters. */
	otus_updateedca_cb_locked(sc);
	return 0;
}

/*
 * Return default value for PHY register based on current operating mode.
 */
Static uint32_t
otus_phy_get_def(struct otus_softc *sc, uint32_t reg)
{
	int i;

	DPRINTFN(DBG_FN, sc, "\n");

	for (i = 0; i < __arraycount(ar5416_phy_regs); i++)
		if (AR_PHY(ar5416_phy_regs[i]) == reg)
			return sc->sc_phy_vals[i];
	return 0;	/* Register not found. */
}

/*
 * Update PHY's programming based on vendor-specific data stored in EEPROM.
 * This is for FEM-type devices only.
 */
Static int
otus_set_board_values(struct otus_softc *sc, struct ieee80211_channel *c)
{
	const struct ModalEepHeader *eep;
	uint32_t tmp, offset;

	DPRINTFN(DBG_FN, sc, "\n");

	if (IEEE80211_IS_CHAN_5GHZ(c))
		eep = &sc->sc_eeprom.modalHeader[0];
	else
		eep = &sc->sc_eeprom.modalHeader[1];

	/* Offset of chain 2. */
	offset = 2 * 0x1000;

	tmp = le32toh(eep->antCtrlCommon);
	otus_write(sc, AR_PHY_SWITCH_COM, tmp);

	tmp = le32toh(eep->antCtrlChain[0]);
	otus_write(sc, AR_PHY_SWITCH_CHAIN_0, tmp);

	tmp = le32toh(eep->antCtrlChain[1]);
	otus_write(sc, AR_PHY_SWITCH_CHAIN_0 + offset, tmp);

	if (1 /* sc->sc_sco == AR_SCO_SCN */) {
		tmp = otus_phy_get_def(sc, AR_PHY_SETTLING);
		tmp &= ~(0x7f << 7);
		tmp |= (eep->switchSettling & 0x7f) << 7;
		otus_write(sc, AR_PHY_SETTLING, tmp);
	}

	tmp = otus_phy_get_def(sc, AR_PHY_DESIRED_SZ);
	tmp &= ~0xffff;
	tmp |= eep->pgaDesiredSize << 8 | eep->adcDesiredSize;
	otus_write(sc, AR_PHY_DESIRED_SZ, tmp);

	tmp = eep->txEndToXpaOff << 24 | eep->txEndToXpaOff << 16 |
	      eep->txFrameToXpaOn << 8 | eep->txFrameToXpaOn;
	otus_write(sc, AR_PHY_RF_CTL4, tmp);

	tmp = otus_phy_get_def(sc, AR_PHY_RF_CTL3);
	tmp &= ~(0xff << 16);
	tmp |= eep->txEndToRxOn << 16;
	otus_write(sc, AR_PHY_RF_CTL3, tmp);

	tmp = otus_phy_get_def(sc, AR_PHY_CCA);
	tmp &= ~(0x7f << 12);
	tmp |= (eep->thresh62 & 0x7f) << 12;
	otus_write(sc, AR_PHY_CCA, tmp);

	tmp = otus_phy_get_def(sc, AR_PHY_RXGAIN);
	tmp &= ~(0x3f << 12);
	tmp |= (eep->txRxAttenCh[0] & 0x3f) << 12;
	otus_write(sc, AR_PHY_RXGAIN, tmp);

	tmp = otus_phy_get_def(sc, AR_PHY_RXGAIN + offset);
	tmp &= ~(0x3f << 12);
	tmp |= (eep->txRxAttenCh[1] & 0x3f) << 12;
	otus_write(sc, AR_PHY_RXGAIN + offset, tmp);

	tmp = otus_phy_get_def(sc, AR_PHY_GAIN_2GHZ);
	tmp &= ~(0x3f << 18);
	tmp |= (eep->rxTxMarginCh[0] & 0x3f) << 18;
	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		tmp &= ~(0xf << 10);
		tmp |= (eep->bswMargin[0] & 0xf) << 10;
	}
	otus_write(sc, AR_PHY_GAIN_2GHZ, tmp);

	tmp = otus_phy_get_def(sc, AR_PHY_GAIN_2GHZ + offset);
	tmp &= ~(0x3f << 18);
	tmp |= (eep->rxTxMarginCh[1] & 0x3f) << 18;
	otus_write(sc, AR_PHY_GAIN_2GHZ + offset, tmp);

	tmp = otus_phy_get_def(sc, AR_PHY_TIMING_CTRL4);
	tmp &= ~(0x3f << 5 | 0x1f);
	tmp |= (eep->iqCalICh[0] & 0x3f) << 5 | (eep->iqCalQCh[0] & 0x1f);
	otus_write(sc, AR_PHY_TIMING_CTRL4, tmp);

	tmp = otus_phy_get_def(sc, AR_PHY_TIMING_CTRL4 + offset);
	tmp &= ~(0x3f << 5 | 0x1f);
	tmp |= (eep->iqCalICh[1] & 0x3f) << 5 | (eep->iqCalQCh[1] & 0x1f);
	otus_write(sc, AR_PHY_TIMING_CTRL4 + offset, tmp);

	tmp = otus_phy_get_def(sc, AR_PHY_TPCRG1);
	tmp &= ~(0xf << 16);
	tmp |= (eep->xpd & 0xf) << 16;
	otus_write(sc, AR_PHY_TPCRG1, tmp);

	return otus_write_barrier(sc);
}

Static int
otus_program_phy(struct otus_softc *sc, struct ieee80211_channel *c)
{
	const uint32_t *vals;
	int error, i;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Select PHY programming based on band and bandwidth. */
	if (IEEE80211_IS_CHAN_2GHZ(c))
		vals = ar5416_phy_vals_2ghz_20mhz;
	else
		vals = ar5416_phy_vals_5ghz_20mhz;
	for (i = 0; i < __arraycount(ar5416_phy_regs); i++)
		otus_write(sc, AR_PHY(ar5416_phy_regs[i]), vals[i]);
	sc->sc_phy_vals = vals;

	if (sc->sc_eeprom.baseEepHeader.deviceType == 0x80)	/* FEM */
		if ((error = otus_set_board_values(sc, c)) != 0)
			return error;

	/* Initial Tx power settings. */
	otus_write(sc, AR_PHY_POWER_TX_RATE_MAX, 0x7f);
	otus_write(sc, AR_PHY_POWER_TX_RATE1, 0x3f3f3f3f);
	otus_write(sc, AR_PHY_POWER_TX_RATE2, 0x3f3f3f3f);
	otus_write(sc, AR_PHY_POWER_TX_RATE3, 0x3f3f3f3f);
	otus_write(sc, AR_PHY_POWER_TX_RATE4, 0x3f3f3f3f);
	otus_write(sc, AR_PHY_POWER_TX_RATE5, 0x3f3f3f3f);
	otus_write(sc, AR_PHY_POWER_TX_RATE6, 0x3f3f3f3f);
	otus_write(sc, AR_PHY_POWER_TX_RATE7, 0x3f3f3f3f);
	otus_write(sc, AR_PHY_POWER_TX_RATE8, 0x3f3f3f3f);
	otus_write(sc, AR_PHY_POWER_TX_RATE9, 0x3f3f3f3f);

	if (IEEE80211_IS_CHAN_2GHZ(c))
		otus_write(sc, 0x1d4014, 0x5163);
	else
		otus_write(sc, 0x1d4014, 0x5143);

	return otus_write_barrier(sc);
}

static __inline uint8_t
otus_reverse_bits(uint8_t v)
{

	v = ((v >> 1) & 0x55) | ((v & 0x55) << 1);
	v = ((v >> 2) & 0x33) | ((v & 0x33) << 2);
	v = ((v >> 4) & 0x0f) | ((v & 0x0f) << 4);
	return v;
}

Static int
otus_set_rf_bank4(struct otus_softc *sc, struct ieee80211_channel *c)
{
	uint8_t chansel, d0, d1;
	uint16_t data;
	int error;

	DPRINTFN(DBG_FN, sc, "\n");

	d0 = 0;
	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		chansel = (c->ic_freq - 4800) / 5;
		if (chansel & 1)
			d0 |= AR_BANK4_AMODE_REFSEL(2);
		else
			d0 |= AR_BANK4_AMODE_REFSEL(1);
	} else {
		d0 |= AR_BANK4_AMODE_REFSEL(2);
		if (c->ic_freq == 2484) {	/* CH 14 */
			d0 |= AR_BANK4_BMODE_LF_SYNTH_FREQ;
			chansel = 10 + (c->ic_freq - 2274) / 5;
		} else
			chansel = 16 + (c->ic_freq - 2272) / 5;
		chansel <<= 2;
	}
	d0 |= AR_BANK4_ADDR(1) | AR_BANK4_CHUP;
	d1 = otus_reverse_bits(chansel);

	/* Write bits 0-4 of d0 and d1. */
	data = (d1 & 0x1f) << 5 | (d0 & 0x1f);
	otus_write(sc, AR_PHY(44), data);
	/* Write bits 5-7 of d0 and d1. */
	data = (d1 >> 5) << 5 | (d0 >> 5);
	otus_write(sc, AR_PHY(58), data);

	if ((error = otus_write_barrier(sc)) == 0)
		usbd_delay_ms(sc->sc_udev, 10);

	return error;
}

Static void
otus_get_delta_slope(uint32_t coeff, uint32_t *exponent, uint32_t *mantissa)
{
#define COEFF_SCALE_SHIFT	24
	uint32_t exp, man;

	DPRINTFN(DBG_FN, DBG_NO_SC, "\n");

	/* exponent = 14 - floor(log2(coeff)) */
	for (exp = 31; exp > 0; exp--)
		if (coeff & (1 << exp))
			break;
	KASSERT(exp != 0);
	exp = 14 - (exp - COEFF_SCALE_SHIFT);

	/* mantissa = floor(coeff * 2^exponent + 0.5) */
	man = coeff + (1 << (COEFF_SCALE_SHIFT - exp - 1));

	*mantissa = man >> (COEFF_SCALE_SHIFT - exp);
	*exponent = exp - 16;
#undef COEFF_SCALE_SHIFT
}

Static int
otus_set_chan(struct otus_softc *sc, struct ieee80211_channel *c, int assoc)
{
	struct ar_cmd_frequency cmd;
	struct ar_rsp_frequency rsp;
	const uint32_t *vals;
	uint32_t coeff, exp, man, tmp;
	uint8_t code;
	int error, i;

	DPRINTFN(DBG_FN, sc, "\n");


#ifdef OTUS_DEBUG
	struct ieee80211com *ic = &sc->sc_ic;
	int chan = ieee80211_chan2ieee(ic, c);

	DPRINTFN(DBG_CHAN, sc, "setting channel %d (%dMHz)\n",
	    chan, c->ic_freq);
#endif

	tmp = IEEE80211_IS_CHAN_2GHZ(c) ? 0x105 : 0x104;
	otus_write(sc, AR_MAC_REG_DYNAMIC_SIFS_ACK, tmp);
	if ((error = otus_write_barrier(sc)) != 0)
		return error;

	/* Disable BB Heavy Clip. */
	otus_write(sc, AR_PHY_HEAVY_CLIP_ENABLE, 0x200);
	if ((error = otus_write_barrier(sc)) != 0)
		return error;

	/* XXX Is that FREQ_START ? */
	error = otus_cmd(sc, AR_CMD_FREQ_STRAT, NULL, 0, NULL);
	if (error != 0)
		return error;

	/* Reprogram PHY and RF on channel band or bandwidth changes. */
	if (sc->sc_bb_reset || c->ic_flags != sc->sc_curchan->ic_flags) {
		DPRINTFN(DBG_CHAN, sc, "band switch\n");

		/* Cold/Warm reset BB/ADDA. */
		otus_write(sc, 0x1d4004, sc->sc_bb_reset ? 0x800 : 0x400);
		if ((error = otus_write_barrier(sc)) != 0)
			return error;

		otus_write(sc, 0x1d4004, 0);
		if ((error = otus_write_barrier(sc)) != 0)
			return error;
		sc->sc_bb_reset = 0;

		if ((error = otus_program_phy(sc, c)) != 0) {
			aprint_error_dev(sc->sc_dev,
			    "could not program PHY\n");
			return error;
		}

		/* Select RF programming based on band. */
		if (IEEE80211_IS_CHAN_5GHZ(c))
			vals = ar5416_banks_vals_5ghz;
		else
			vals = ar5416_banks_vals_2ghz;
		for (i = 0; i < __arraycount(ar5416_banks_regs); i++)
			otus_write(sc, AR_PHY(ar5416_banks_regs[i]), vals[i]);
		if ((error = otus_write_barrier(sc)) != 0) {
			aprint_error_dev(sc->sc_dev, "could not program RF\n");
			return error;
		}
		code = AR_CMD_RF_INIT;
	} else {
		code = AR_CMD_FREQUENCY;
	}

	if ((error = otus_set_rf_bank4(sc, c)) != 0)
		return error;

	tmp = (sc->sc_txmask == 0x5) ? 0x340 : 0x240;
	otus_write(sc, AR_PHY_TURBO, tmp);
	if ((error = otus_write_barrier(sc)) != 0)
		return error;

	/* Send firmware command to set channel. */
	cmd.freq = htole32((uint32_t)c->ic_freq * 1000);
	cmd.dynht2040 = htole32(0);
	cmd.htena = htole32(1);

	/* Set Delta Slope (exponent and mantissa). */
	coeff = (100 << 24) / c->ic_freq;
	otus_get_delta_slope(coeff, &exp, &man);
	cmd.dsc_exp = htole32(exp);
	cmd.dsc_man = htole32(man);
	DPRINTFN(DBG_CHAN, sc, "ds coeff=%u exp=%u man=%u\n",
	    coeff, exp, man);

	/* For Short GI, coeff is 9/10 that of normal coeff. */
	coeff = (9 * coeff) / 10;
	otus_get_delta_slope(coeff, &exp, &man);
	cmd.dsc_shgi_exp = htole32(exp);
	cmd.dsc_shgi_man = htole32(man);
	DPRINTFN(DBG_CHAN, sc, "ds shgi coeff=%u exp=%u man=%u\n",
	    coeff, exp, man);

	/* Set wait time for AGC and noise calibration (100 or 200ms). */
	cmd.check_loop_count = assoc ? htole32(2000) : htole32(1000);
	DPRINTFN(DBG_CHAN, sc, "%s\n",
	    code == AR_CMD_RF_INIT ? "RF_INIT" : "FREQUENCY");
	error = otus_cmd(sc, code, &cmd, sizeof(cmd), &rsp);
	if (error != 0)
		return error;

	if ((rsp.status & htole32(AR_CAL_ERR_AGC | AR_CAL_ERR_NF_VAL)) != 0) {
		DPRINTFN(DBG_CHAN, sc, "status=0x%x\n", le32toh(rsp.status));
		/* Force cold reset on next channel. */
		sc->sc_bb_reset = 1;
	}

#ifdef OTUS_DEBUG
	if (otus_debug & DBG_CHAN) {
		DPRINTFN(DBG_CHAN, sc, "calibration status=0x%x\n",
		    le32toh(rsp.status));
		for (i = 0; i < 2; i++) {	/* 2 Rx chains */
			/* Sign-extend 9-bit NF values. */
			DPRINTFN(DBG_CHAN, sc, "noisefloor chain %d=%d\n",
			    i, (((int32_t)le32toh(rsp.nf[i])) << 4) >> 23);
			DPRINTFN(DBG_CHAN, sc, "noisefloor ext chain %d=%d\n",
			    i, ((int32_t)le32toh(rsp.nf_ext[i])) >> 23);
		}
	}
#endif
	sc->sc_curchan = c;
	return 0;
}

#ifdef notyet
Static int
otus_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct otus_softc *sc;
	struct otus_cmd_key cmd;

	sc = ic->ic_ifp->if_softc;

	DPRINTFN(DBG_FN, sc, "\n");

	/* Defer setting of WEP keys until interface is brought up. */
	if ((ic->ic_ifp->if_flags & (IFF_UP | IFF_RUNNING)) !=
	    (IFF_UP | IFF_RUNNING))
		return 0;

	/* Do it in a process context. */
	cmd.key = *k;
	cmd.associd = (ni != NULL) ? ni->ni_associd : 0;
	otus_do_async(sc, otus_set_key_cb, &cmd, sizeof(cmd));
	return 0;
}

Static void
otus_set_key_cb(struct otus_softc *sc, void *arg)
{
	struct otus_cmd_key *cmd;
	struct ieee80211_key *k;
	struct ar_cmd_ekey key;
	uint16_t cipher;
	int error;

	DPRINTFN(DBG_FN, sc, "\n");

	cmd = arg;
	k = &cmd->key;

	memset(&key, 0, sizeof(key));
	if (k->k_flags & IEEE80211_KEY_GROUP) {
		key.uid = htole16(k->k_id);
		IEEE80211_ADDR_COPY(key.macaddr, sc->sc_ic.ic_myaddr);
		key.macaddr[0] |= 0x80;
	} else {
		key.uid = htole16(OTUS_UID(cmd->associd));
		IEEE80211_ADDR_COPY(key.macaddr, ni->ni_macaddr);
	}
	key.kix = htole16(0);
	/* Map net80211 cipher to hardware. */
	switch (k->k_cipher) {
	case IEEE80211_CIPHER_WEP40:
		cipher = AR_CIPHER_WEP64;
		break;
	case IEEE80211_CIPHER_WEP104:
		cipher = AR_CIPHER_WEP128;
		break;
	case IEEE80211_CIPHER_TKIP:
		cipher = AR_CIPHER_TKIP;
		break;
	case IEEE80211_CIPHER_CCMP:
		cipher = AR_CIPHER_AES;
		break;
	default:
		return;
	}
	key.cipher = htole16(cipher);
	memcpy(key.key, k->k_key, MIN(k->k_len, 16));
	error = otus_cmd(sc, AR_CMD_EKEY, &key, sizeof(key), NULL);
	if (error != 0 || k->k_cipher != IEEE80211_CIPHER_TKIP)
		return;

	/* TKIP: set Tx/Rx MIC Key. */
	key.kix = htole16(1);
	memcpy(key.key, k->k_key + 16, 16);
	(void)otus_cmd(sc, AR_CMD_EKEY, &key, sizeof(key), NULL);
}

Static void
otus_delete_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct otus_softc *sc;
	struct otus_cmd_key cmd;

	sc = ic->ic_ifp->if_softc;

	DPRINTFN(DBG_FN, sc, "\n");

	if (!(ic->ic_ifp->if_flags & IFF_RUNNING) ||
	    ic->ic_state != IEEE80211_S_RUN)
		return;	/* Nothing to do. */

	/* Do it in a process context. */
	cmd.key = *k;
	cmd.associd = (ni != NULL) ? ni->ni_associd : 0;
	otus_do_async(sc, otus_delete_key_cb, &cmd, sizeof(cmd));
}

Static void
otus_delete_key_cb(struct otus_softc *sc, void *arg)
{
	struct otus_cmd_key *cmd;
	struct ieee80211_key *k;
	uint32_t uid;

	DPRINTFN(DBG_FN, sc, "\n");

	cmd = arg;
	k = &cmd->key;
	if (k->k_flags & IEEE80211_KEY_GROUP)
		uid = htole32(k->k_id);
	else
		uid = htole32(OTUS_UID(cmd->associd));
	(void)otus_cmd(sc, AR_CMD_DKEY, &uid, sizeof(uid), NULL);
}
#endif /* notyet */

Static void
otus_calib_to(void *arg)
{
	struct otus_softc *sc;
	struct ieee80211com *ic;
	struct ieee80211_node *ni;
	struct otus_node *on;
	int s;

	sc = arg;

	DPRINTFN(DBG_FN, sc, "\n");

	if (sc->sc_dying)
		return;

	s = splnet();
	ic = &sc->sc_ic;
	ni = ic->ic_bss;
	on = (void *)ni;
	ieee80211_amrr_choose(&sc->sc_amrr, ni, &on->amn);
	splx(s);

	if (!sc->sc_dying)
		callout_schedule(&sc->sc_calib_to, hz);
}

Static int
otus_set_bssid(struct otus_softc *sc, const uint8_t *bssid)
{

	DPRINTFN(DBG_FN, sc, "\n");

	KASSERT(mutex_owned(&sc->sc_write_mtx));

	otus_write(sc, AR_MAC_REG_BSSID_L,
	    bssid[0] | bssid[1] << 8 | bssid[2] << 16 | bssid[3] << 24);
	otus_write(sc, AR_MAC_REG_BSSID_H,
	    bssid[4] | bssid[5] << 8);
	return otus_write_barrier(sc);
}

Static int
otus_set_macaddr(struct otus_softc *sc, const uint8_t *addr)
{

	DPRINTFN(DBG_FN, sc, "\n");

	KASSERT(mutex_owned(&sc->sc_write_mtx));

	otus_write(sc, AR_MAC_REG_MAC_ADDR_L,
	    addr[0] | addr[1] << 8 | addr[2] << 16 | addr[3] << 24);
	otus_write(sc, AR_MAC_REG_MAC_ADDR_H,
	    addr[4] | addr[5] << 8);
	return otus_write_barrier(sc);
}

#ifdef notyet
/* Default single-LED. */
Static void
otus_led_newstate_type1(struct otus_softc *sc)
{

	DPRINTFN(DBG_FN, sc, "\n");

	/* TBD */
}

/* NETGEAR, dual-LED. */
Static void
otus_led_newstate_type2(struct otus_softc *sc)
{

	DPRINTFN(DBG_FN, sc, "\n");

	/* TBD */
}
#endif /* notyet */

/*
 * NETGEAR, single-LED/3 colors (blue, red, purple.)
 */
Static void
otus_led_newstate_type3(struct otus_softc *sc)
{
	struct ieee80211com *ic;
	uint32_t led_state;

	DPRINTFN(DBG_FN, sc, "\n");

	ic = &sc->sc_ic;
	led_state = sc->sc_led_state;
	switch(ic->ic_state) {
	case IEEE80211_S_INIT:
		led_state = 0;
		break;
	case IEEE80211_S_SCAN:
		led_state ^= AR_GPIO_REG_DATA_LED0_ON | AR_GPIO_REG_DATA_LED1_ON;
		led_state &= ~(IEEE80211_IS_CHAN_2GHZ(sc->sc_curchan) ?
		    AR_GPIO_REG_DATA_LED1_ON : AR_GPIO_REG_DATA_LED0_ON);
		break;
	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		/* XXX: Turn both LEDs on for AUTH and ASSOC? */
		led_state = AR_GPIO_REG_DATA_LED0_ON | AR_GPIO_REG_DATA_LED1_ON;
		break;
	case IEEE80211_S_RUN:
		led_state = IEEE80211_IS_CHAN_2GHZ(sc->sc_curchan) ?
		    AR_GPIO_REG_DATA_LED0_ON : AR_GPIO_REG_DATA_LED1_ON;
		break;
	}
	if (led_state != sc->sc_led_state) {
		otus_write(sc, AR_GPIO_REG_DATA, led_state);
		if (otus_write_barrier(sc) == 0)
			sc->sc_led_state = led_state;
	}
}

Static int
otus_init(struct ifnet *ifp)
{
	struct otus_softc *sc;
	struct ieee80211com *ic;
	uint32_t filter, pm_mode, sniffer;
	int error;

	sc = ifp->if_softc;

	DPRINTFN(DBG_FN|DBG_INIT, sc, "\n");

	ic = &sc->sc_ic;

	mutex_enter(&sc->sc_write_mtx);

	/* Init host command ring. */
	mutex_spin_enter(&sc->sc_task_mtx);
	sc->sc_cmdq.cur = sc->sc_cmdq.next = sc->sc_cmdq.queued = 0;
	mutex_spin_exit(&sc->sc_task_mtx);

	if ((error = otus_init_mac(sc)) != 0) {
		mutex_exit(&sc->sc_write_mtx);
		aprint_error_dev(sc->sc_dev, "could not initialize MAC\n");
		return error;
	}

	IEEE80211_ADDR_COPY(ic->ic_myaddr, CLLADDR(ifp->if_sadl));
	(void)otus_set_macaddr(sc, ic->ic_myaddr);

	pm_mode = AR_MAC_REG_POWERMGT_DEFAULTS;
	sniffer = AR_MAC_REG_SNIFFER_DEFAULTS;
	filter = AR_MAC_REG_FTF_DEFAULTS;
	sc->sc_rx_error_msk = ~0;

	switch (ic->ic_opmode) {
#ifdef notyet
#ifndef IEEE80211_STA_ONLY
	case IEEE80211_M_HOSTAP:
		pm_mode |= AR_MAC_REG_POWERMGT_AP;
		break;
	case IEEE80211_M_IBSS:
		pm_mode |= AR_MAC_REG_POWERMGT_IBSS;	/* XXX: was 0x0 */
		break;
#endif
#endif
	case IEEE80211_M_STA:
		pm_mode |= AR_MAC_REG_POWERMGT_STA;
		break;
	case IEEE80211_M_MONITOR:
		sc->sc_rx_error_msk = ~AR_RX_ERROR_BAD_RA;
		filter = AR_MAC_REG_FTF_MONITOR;
		sniffer |= AR_MAC_REG_SNIFFER_ENABLE_PROMISC;
		break;
	default:
		aprint_error_dev(sc->sc_dev, "bad opmode: %d", ic->ic_opmode);
		return EOPNOTSUPP;	/* XXX: ??? */
	}
	otus_write(sc, AR_MAC_REG_POWERMANAGEMENT, pm_mode);
	otus_write(sc, AR_MAC_REG_FRAMETYPE_FILTER, filter);
	otus_write(sc, AR_MAC_REG_SNIFFER, sniffer);
	(void)otus_write_barrier(sc);

	sc->sc_bb_reset = 1;	/* Force cold reset. */
	if ((error = otus_set_chan(sc, ic->ic_curchan, 0)) != 0) {
		mutex_exit(&sc->sc_write_mtx);
		aprint_error_dev(sc->sc_dev, "could not set channel\n");
		return error;
	}

	/* Start Rx. */
	otus_write(sc, AR_MAC_REG_DMA, AR_MAC_REG_DMA_ENABLE);
	(void)otus_write_barrier(sc);
	mutex_exit(&sc->sc_write_mtx);

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);

	return 0;
}

Static void
otus_stop(struct ifnet *ifp)
{
	struct otus_softc *sc;
	struct ieee80211com *ic;
	int s;

	sc = ifp->if_softc;

	DPRINTFN(DBG_FN, sc, "\n");

	ic = &sc->sc_ic;

	sc->sc_tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	callout_halt(&sc->sc_scan_to, NULL);
	callout_halt(&sc->sc_calib_to, NULL);

	s = splusb();
	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
	otus_wait_async(sc);
	splx(s);

	/* Stop Rx. */
	mutex_enter(&sc->sc_write_mtx);
	otus_write(sc, AR_MAC_REG_DMA, AR_MAC_REG_DMA_OFF);
	(void)otus_write_barrier(sc);
	mutex_exit(&sc->sc_write_mtx);
}
