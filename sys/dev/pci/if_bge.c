/*	$NetBSD: if_bge.c,v 1.293 2015/07/21 03:15:50 knakahara Exp $	*/

/*
 * Copyright (c) 2001 Wind River Systems
 * Copyright (c) 1997, 1998, 1999, 2001
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 * $FreeBSD: if_bge.c,v 1.13 2002/04/04 06:01:31 wpaul Exp $
 */

/*
 * Broadcom BCM570x family gigabit ethernet driver for NetBSD.
 *
 * NetBSD version by:
 *
 *	Frank van der Linden <fvdl@wasabisystems.com>
 *	Jason Thorpe <thorpej@wasabisystems.com>
 *	Jonathan Stone <jonathan@dsg.stanford.edu>
 *
 * Originally written for FreeBSD by Bill Paul <wpaul@windriver.com>
 * Senior Engineer, Wind River Systems
 */

/*
 * The Broadcom BCM5700 is based on technology originally developed by
 * Alteon Networks as part of the Tigon I and Tigon II gigabit ethernet
 * MAC chips. The BCM5700, sometimes referred to as the Tigon III, has
 * two on-board MIPS R4000 CPUs and can have as much as 16MB of external
 * SSRAM. The BCM5700 supports TCP, UDP and IP checksum offload, jumbo
 * frames, highly configurable RX filtering, and 16 RX and TX queues
 * (which, along with RX filter rules, can be used for QOS applications).
 * Other features, such as TCP segmentation, may be available as part
 * of value-added firmware updates. Unlike the Tigon I and Tigon II,
 * firmware images can be stored in hardware and need not be compiled
 * into the driver.
 *
 * The BCM5700 supports the PCI v2.2 and PCI-X v1.0 standards, and will
 * function in a 32-bit/64-bit 33/66MHz bus, or a 64-bit/133MHz bus.
 *
 * The BCM5701 is a single-chip solution incorporating both the BCM5700
 * MAC and a BCM5401 10/100/1000 PHY. Unlike the BCM5700, the BCM5701
 * does not support external SSRAM.
 *
 * Broadcom also produces a variation of the BCM5700 under the "Altima"
 * brand name, which is functionally similar but lacks PCI-X support.
 *
 * Without external SSRAM, you can only have at most 4 TX rings,
 * and the use of the mini RX ring is disabled. This seems to imply
 * that these features are simply not available on the BCM5701. As a
 * result, this driver does not implement any support for the mini RX
 * ring.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_bge.c,v 1.293 2015/07/21 03:15:50 knakahara Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#include <sys/rndsource.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

/* Headers for TCP Segmentation Offload (TSO) */
#include <netinet/in_systm.h>		/* n_time for <netinet/ip.h>... */
#include <netinet/in.h>			/* ip_{src,dst}, for <netinet/ip.h> */
#include <netinet/ip.h>			/* for struct ip */
#include <netinet/tcp.h>		/* for struct tcphdr */


#include <net/bpf.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/miidevs.h>
#include <dev/mii/brgphyreg.h>

#include <dev/pci/if_bgereg.h>
#include <dev/pci/if_bgevar.h>

#include <prop/proplib.h>

#define ETHER_MIN_NOPAD (ETHER_MIN_LEN - ETHER_CRC_LEN) /* i.e., 60 */


/*
 * Tunable thresholds for rx-side bge interrupt mitigation.
 */

/*
 * The pairs of values below were obtained from empirical measurement
 * on bcm5700 rev B2; they ar designed to give roughly 1 receive
 * interrupt for every N packets received, where N is, approximately,
 * the second value (rx_max_bds) in each pair.  The values are chosen
 * such that moving from one pair to the succeeding pair was observed
 * to roughly halve interrupt rate under sustained input packet load.
 * The values were empirically chosen to avoid overflowing internal
 * limits on the  bcm5700: increasing rx_ticks much beyond 600
 * results in internal wrapping and higher interrupt rates.
 * The limit of 46 frames was chosen to match NFS workloads.
 *
 * These values also work well on bcm5701, bcm5704C, and (less
 * tested) bcm5703.  On other chipsets, (including the Altima chip
 * family), the larger values may overflow internal chip limits,
 * leading to increasing interrupt rates rather than lower interrupt
 * rates.
 *
 * Applications using heavy interrupt mitigation (interrupting every
 * 32 or 46 frames) in both directions may need to increase the TCP
 * windowsize to above 131072 bytes (e.g., to 199608 bytes) to sustain
 * full link bandwidth, due to ACKs and window updates lingering
 * in the RX queue during the 30-to-40-frame interrupt-mitigation window.
 */
static const struct bge_load_rx_thresh {
	int rx_ticks;
	int rx_max_bds; }
bge_rx_threshes[] = {
	{ 16,   1 },	/* rx_max_bds = 1 disables interrupt mitigation */
	{ 32,   2 },
	{ 50,   4 },
	{ 100,  8 },
	{ 192, 16 },
	{ 416, 32 },
	{ 598, 46 }
};
#define NBGE_RX_THRESH (sizeof(bge_rx_threshes) / sizeof(bge_rx_threshes[0]))

/* XXX patchable; should be sysctl'able */
static int bge_auto_thresh = 1;
static int bge_rx_thresh_lvl;

static int bge_rxthresh_nodenum;

typedef int (*bge_eaddr_fcn_t)(struct bge_softc *, uint8_t[]);

static uint32_t bge_chipid(const struct pci_attach_args *);
#ifdef __HAVE_PCI_MSI_MSIX
static int bge_can_use_msi(struct bge_softc *);
#endif
static int bge_probe(device_t, cfdata_t, void *);
static void bge_attach(device_t, device_t, void *);
static int bge_detach(device_t, int);
static void bge_release_resources(struct bge_softc *);

static int bge_get_eaddr_fw(struct bge_softc *, uint8_t[]);
static int bge_get_eaddr_mem(struct bge_softc *, uint8_t[]);
static int bge_get_eaddr_nvram(struct bge_softc *, uint8_t[]);
static int bge_get_eaddr_eeprom(struct bge_softc *, uint8_t[]);
static int bge_get_eaddr(struct bge_softc *, uint8_t[]);

static void bge_txeof(struct bge_softc *);
static void bge_rxcsum(struct bge_softc *, struct bge_rx_bd *, struct mbuf *);
static void bge_rxeof(struct bge_softc *);

static void bge_asf_driver_up (struct bge_softc *);
static void bge_tick(void *);
static void bge_stats_update(struct bge_softc *);
static void bge_stats_update_regs(struct bge_softc *);
static int bge_encap(struct bge_softc *, struct mbuf *, uint32_t *);

static int bge_intr(void *);
static void bge_start(struct ifnet *);
static int bge_ifflags_cb(struct ethercom *);
static int bge_ioctl(struct ifnet *, u_long, void *);
static int bge_init(struct ifnet *);
static void bge_stop(struct ifnet *, int);
static void bge_watchdog(struct ifnet *);
static int bge_ifmedia_upd(struct ifnet *);
static void bge_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static uint8_t bge_nvram_getbyte(struct bge_softc *, int, uint8_t *);
static int bge_read_nvram(struct bge_softc *, uint8_t *, int, int);

static uint8_t bge_eeprom_getbyte(struct bge_softc *, int, uint8_t *);
static int bge_read_eeprom(struct bge_softc *, void *, int, int);
static void bge_setmulti(struct bge_softc *);

static void bge_handle_events(struct bge_softc *);
static int bge_alloc_jumbo_mem(struct bge_softc *);
#if 0 /* XXX */
static void bge_free_jumbo_mem(struct bge_softc *);
#endif
static void *bge_jalloc(struct bge_softc *);
static void bge_jfree(struct mbuf *, void *, size_t, void *);
static int bge_newbuf_std(struct bge_softc *, int, struct mbuf *,
			       bus_dmamap_t);
static int bge_newbuf_jumbo(struct bge_softc *, int, struct mbuf *);
static int bge_init_rx_ring_std(struct bge_softc *);
static void bge_free_rx_ring_std(struct bge_softc *);
static int bge_init_rx_ring_jumbo(struct bge_softc *);
static void bge_free_rx_ring_jumbo(struct bge_softc *);
static void bge_free_tx_ring(struct bge_softc *);
static int bge_init_tx_ring(struct bge_softc *);

static int bge_chipinit(struct bge_softc *);
static int bge_blockinit(struct bge_softc *);
static int bge_phy_addr(struct bge_softc *);
static uint32_t bge_readmem_ind(struct bge_softc *, int);
static void bge_writemem_ind(struct bge_softc *, int, int);
static void bge_writembx(struct bge_softc *, int, int);
static void bge_writembx_flush(struct bge_softc *, int, int);
static void bge_writemem_direct(struct bge_softc *, int, int);
static void bge_writereg_ind(struct bge_softc *, int, int);
static void bge_set_max_readrq(struct bge_softc *);

static int bge_miibus_readreg(device_t, int, int);
static void bge_miibus_writereg(device_t, int, int, int);
static void bge_miibus_statchg(struct ifnet *);

#define BGE_RESET_SHUTDOWN	0
#define	BGE_RESET_START		1
#define	BGE_RESET_SUSPEND	2
static void bge_sig_post_reset(struct bge_softc *, int);
static void bge_sig_legacy(struct bge_softc *, int);
static void bge_sig_pre_reset(struct bge_softc *, int);
static void bge_wait_for_event_ack(struct bge_softc *);
static void bge_stop_fw(struct bge_softc *);
static int bge_reset(struct bge_softc *);
static void bge_link_upd(struct bge_softc *);
static void bge_sysctl_init(struct bge_softc *);
static int bge_sysctl_verify(SYSCTLFN_PROTO);

static void bge_ape_lock_init(struct bge_softc *);
static void bge_ape_read_fw_ver(struct bge_softc *);
static int bge_ape_lock(struct bge_softc *, int);
static void bge_ape_unlock(struct bge_softc *, int);
static void bge_ape_send_event(struct bge_softc *, uint32_t);
static void bge_ape_driver_state_change(struct bge_softc *, int);

#ifdef BGE_DEBUG
#define DPRINTF(x)	if (bgedebug) printf x
#define DPRINTFN(n,x)	if (bgedebug >= (n)) printf x
#define BGE_TSO_PRINTF(x)  do { if (bge_tso_debug) printf x ;} while (0)
int	bgedebug = 0;
int	bge_tso_debug = 0;
void		bge_debug_info(struct bge_softc *);
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#define BGE_TSO_PRINTF(x)
#endif

#ifdef BGE_EVENT_COUNTERS
#define	BGE_EVCNT_INCR(ev)	(ev).ev_count++
#define	BGE_EVCNT_ADD(ev, val)	(ev).ev_count += (val)
#define	BGE_EVCNT_UPD(ev, val)	(ev).ev_count = (val)
#else
#define	BGE_EVCNT_INCR(ev)	/* nothing */
#define	BGE_EVCNT_ADD(ev, val)	/* nothing */
#define	BGE_EVCNT_UPD(ev, val)	/* nothing */
#endif

static const struct bge_product {
	pci_vendor_id_t		bp_vendor;
	pci_product_id_t	bp_product;
	const char		*bp_name;
} bge_products[] = {
	/*
	 * The BCM5700 documentation seems to indicate that the hardware
	 * still has the Alteon vendor ID burned into it, though it
	 * should always be overridden by the value in the EEPROM.  We'll
	 * check for it anyway.
	 */
	{ PCI_VENDOR_ALTEON,
	  PCI_PRODUCT_ALTEON_BCM5700,
	  "Broadcom BCM5700 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_ALTEON,
	  PCI_PRODUCT_ALTEON_BCM5701,
	  "Broadcom BCM5701 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_ALTIMA,
	  PCI_PRODUCT_ALTIMA_AC1000,
	  "Altima AC1000 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_ALTIMA,
	  PCI_PRODUCT_ALTIMA_AC1001,
	  "Altima AC1001 Gigabit Ethernet",
	   },
	{ PCI_VENDOR_ALTIMA,
	  PCI_PRODUCT_ALTIMA_AC1003,
	  "Altima AC1003 Gigabit Ethernet",
	   },
	{ PCI_VENDOR_ALTIMA,
	  PCI_PRODUCT_ALTIMA_AC9100,
	  "Altima AC9100 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_APPLE,
	  PCI_PRODUCT_APPLE_BCM5701,
	  "APPLE BCM5701 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5700,
	  "Broadcom BCM5700 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5701,
	  "Broadcom BCM5701 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5702,
	  "Broadcom BCM5702 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5702X,
	  "Broadcom BCM5702X Gigabit Ethernet" },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5703,
	  "Broadcom BCM5703 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5703X,
	  "Broadcom BCM5703X Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5703_ALT,
	  "Broadcom BCM5703 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5704C,
	  "Broadcom BCM5704C Dual Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5704S,
	  "Broadcom BCM5704S Dual Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5705,
	  "Broadcom BCM5705 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5705F,
	  "Broadcom BCM5705F Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5705K,
	  "Broadcom BCM5705K Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5705M,
	  "Broadcom BCM5705M Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5705M_ALT,
	  "Broadcom BCM5705M Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5714,
	  "Broadcom BCM5714 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5714S,
	  "Broadcom BCM5714S Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5715,
	  "Broadcom BCM5715 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5715S,
	  "Broadcom BCM5715S Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5717,
	  "Broadcom BCM5717 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5718,
	  "Broadcom BCM5718 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5719,
	  "Broadcom BCM5719 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5720,
	  "Broadcom BCM5720 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5721,
	  "Broadcom BCM5721 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5722,
	  "Broadcom BCM5722 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5723,
	  "Broadcom BCM5723 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5750,
	  "Broadcom BCM5750 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5751,
	  "Broadcom BCM5751 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5751F,
	  "Broadcom BCM5751F Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5751M,
	  "Broadcom BCM5751M Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5752,
	  "Broadcom BCM5752 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5752M,
	  "Broadcom BCM5752M Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5753,
	  "Broadcom BCM5753 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5753F,
	  "Broadcom BCM5753F Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5753M,
	  "Broadcom BCM5753M Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5754,
	  "Broadcom BCM5754 Gigabit Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5754M,
	  "Broadcom BCM5754M Gigabit Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5755,
	  "Broadcom BCM5755 Gigabit Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5755M,
	  "Broadcom BCM5755M Gigabit Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5756,
	  "Broadcom BCM5756 Gigabit Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5761,
	  "Broadcom BCM5761 Gigabit Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5761E,
	  "Broadcom BCM5761E Gigabit Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5761S,
	  "Broadcom BCM5761S Gigabit Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5761SE,
	  "Broadcom BCM5761SE Gigabit Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5764,
	  "Broadcom BCM5764 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5780,
	  "Broadcom BCM5780 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5780S,
	  "Broadcom BCM5780S Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5781,
	  "Broadcom BCM5781 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5782,
	  "Broadcom BCM5782 Gigabit Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5784M,
	  "BCM5784M NetLink 1000baseT Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5785F,
	  "BCM5785F NetLink 10/100 Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5785G,
	  "BCM5785G NetLink 1000baseT Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5786,
	  "Broadcom BCM5786 Gigabit Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5787,
	  "Broadcom BCM5787 Gigabit Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5787F,
	  "Broadcom BCM5787F 10/100 Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5787M,
	  "Broadcom BCM5787M Gigabit Ethernet",
	},
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5788,
	  "Broadcom BCM5788 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5789,
	  "Broadcom BCM5789 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5901,
	  "Broadcom BCM5901 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5901A2,
	  "Broadcom BCM5901A2 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5903M,
	  "Broadcom BCM5903M Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5906,
	  "Broadcom BCM5906 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM5906M,
	  "Broadcom BCM5906M Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM57760,
	  "Broadcom BCM57760 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM57761,
	  "Broadcom BCM57761 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM57762,
	  "Broadcom BCM57762 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM57765,
	  "Broadcom BCM57765 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM57766,
	  "Broadcom BCM57766 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM57780,
	  "Broadcom BCM57780 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM57781,
	  "Broadcom BCM57781 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM57782,
	  "Broadcom BCM57782 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM57785,
	  "Broadcom BCM57785 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM57786,
	  "Broadcom BCM57786 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM57788,
	  "Broadcom BCM57788 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM57790,
	  "Broadcom BCM57790 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM57791,
	  "Broadcom BCM57791 Fast Ethernet",
	  },
	{ PCI_VENDOR_BROADCOM,
	  PCI_PRODUCT_BROADCOM_BCM57795,
	  "Broadcom BCM57795 Fast Ethernet",
	  },
	{ PCI_VENDOR_SCHNEIDERKOCH,
	  PCI_PRODUCT_SCHNEIDERKOCH_SK_9DX1,
	  "SysKonnect SK-9Dx1 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_3COM,
	  PCI_PRODUCT_3COM_3C996,
	  "3Com 3c996 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_FUJITSU4,
	  PCI_PRODUCT_FUJITSU4_PW008GE4,
	  "Fujitsu PW008GE4 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_FUJITSU4,
	  PCI_PRODUCT_FUJITSU4_PW008GE5,
	  "Fujitsu PW008GE5 Gigabit Ethernet",
	  },
	{ PCI_VENDOR_FUJITSU4,
	  PCI_PRODUCT_FUJITSU4_PP250_450_LAN,
	  "Fujitsu Primepower 250/450 Gigabit Ethernet",
	  },
	{ 0,
	  0,
	  NULL },
};

#define BGE_IS_JUMBO_CAPABLE(sc)	((sc)->bge_flags & BGEF_JUMBO_CAPABLE)
#define BGE_IS_5700_FAMILY(sc)		((sc)->bge_flags & BGEF_5700_FAMILY)
#define BGE_IS_5705_PLUS(sc)		((sc)->bge_flags & BGEF_5705_PLUS)
#define BGE_IS_5714_FAMILY(sc)		((sc)->bge_flags & BGEF_5714_FAMILY)
#define BGE_IS_575X_PLUS(sc)		((sc)->bge_flags & BGEF_575X_PLUS)
#define BGE_IS_5755_PLUS(sc)		((sc)->bge_flags & BGEF_5755_PLUS)
#define BGE_IS_57765_FAMILY(sc)		((sc)->bge_flags & BGEF_57765_FAMILY)
#define BGE_IS_57765_PLUS(sc)		((sc)->bge_flags & BGEF_57765_PLUS)
#define BGE_IS_5717_PLUS(sc)		((sc)->bge_flags & BGEF_5717_PLUS)

static const struct bge_revision {
	uint32_t		br_chipid;
	const char		*br_name;
} bge_revisions[] = {
	{ BGE_CHIPID_BCM5700_A0, "BCM5700 A0" },
	{ BGE_CHIPID_BCM5700_A1, "BCM5700 A1" },
	{ BGE_CHIPID_BCM5700_B0, "BCM5700 B0" },
	{ BGE_CHIPID_BCM5700_B1, "BCM5700 B1" },
	{ BGE_CHIPID_BCM5700_B2, "BCM5700 B2" },
	{ BGE_CHIPID_BCM5700_B3, "BCM5700 B3" },
	{ BGE_CHIPID_BCM5700_ALTIMA, "BCM5700 Altima" },
	{ BGE_CHIPID_BCM5700_C0, "BCM5700 C0" },
	{ BGE_CHIPID_BCM5701_A0, "BCM5701 A0" },
	{ BGE_CHIPID_BCM5701_B0, "BCM5701 B0" },
	{ BGE_CHIPID_BCM5701_B2, "BCM5701 B2" },
	{ BGE_CHIPID_BCM5701_B5, "BCM5701 B5" },
	{ BGE_CHIPID_BCM5703_A0, "BCM5702/5703 A0" },
	{ BGE_CHIPID_BCM5703_A1, "BCM5702/5703 A1" },
	{ BGE_CHIPID_BCM5703_A2, "BCM5702/5703 A2" },
	{ BGE_CHIPID_BCM5703_A3, "BCM5702/5703 A3" },
	{ BGE_CHIPID_BCM5703_B0, "BCM5702/5703 B0" },
	{ BGE_CHIPID_BCM5704_A0, "BCM5704 A0" },
	{ BGE_CHIPID_BCM5704_A1, "BCM5704 A1" },
	{ BGE_CHIPID_BCM5704_A2, "BCM5704 A2" },
	{ BGE_CHIPID_BCM5704_A3, "BCM5704 A3" },
	{ BGE_CHIPID_BCM5704_B0, "BCM5704 B0" },
	{ BGE_CHIPID_BCM5705_A0, "BCM5705 A0" },
	{ BGE_CHIPID_BCM5705_A1, "BCM5705 A1" },
	{ BGE_CHIPID_BCM5705_A2, "BCM5705 A2" },
	{ BGE_CHIPID_BCM5705_A3, "BCM5705 A3" },
	{ BGE_CHIPID_BCM5750_A0, "BCM5750 A0" },
	{ BGE_CHIPID_BCM5750_A1, "BCM5750 A1" },
	{ BGE_CHIPID_BCM5750_A3, "BCM5750 A3" },
	{ BGE_CHIPID_BCM5750_B0, "BCM5750 B0" },
	{ BGE_CHIPID_BCM5750_B1, "BCM5750 B1" },
	{ BGE_CHIPID_BCM5750_C0, "BCM5750 C0" },
	{ BGE_CHIPID_BCM5750_C1, "BCM5750 C1" },
	{ BGE_CHIPID_BCM5750_C2, "BCM5750 C2" },
	{ BGE_CHIPID_BCM5752_A0, "BCM5752 A0" },
	{ BGE_CHIPID_BCM5752_A1, "BCM5752 A1" },
	{ BGE_CHIPID_BCM5752_A2, "BCM5752 A2" },
	{ BGE_CHIPID_BCM5714_A0, "BCM5714 A0" },
	{ BGE_CHIPID_BCM5714_B0, "BCM5714 B0" },
	{ BGE_CHIPID_BCM5714_B3, "BCM5714 B3" },
	{ BGE_CHIPID_BCM5715_A0, "BCM5715 A0" },
	{ BGE_CHIPID_BCM5715_A1, "BCM5715 A1" },
	{ BGE_CHIPID_BCM5715_A3, "BCM5715 A3" },
	{ BGE_CHIPID_BCM5717_A0, "BCM5717 A0" },
	{ BGE_CHIPID_BCM5717_B0, "BCM5717 B0" },
	{ BGE_CHIPID_BCM5719_A0, "BCM5719 A0" },
	{ BGE_CHIPID_BCM5720_A0, "BCM5720 A0" },
	{ BGE_CHIPID_BCM5755_A0, "BCM5755 A0" },
	{ BGE_CHIPID_BCM5755_A1, "BCM5755 A1" },
	{ BGE_CHIPID_BCM5755_A2, "BCM5755 A2" },
	{ BGE_CHIPID_BCM5755_C0, "BCM5755 C0" },
	{ BGE_CHIPID_BCM5761_A0, "BCM5761 A0" },
	{ BGE_CHIPID_BCM5761_A1, "BCM5761 A1" },
	{ BGE_CHIPID_BCM5784_A0, "BCM5784 A0" },
	{ BGE_CHIPID_BCM5784_A1, "BCM5784 A1" },
	{ BGE_CHIPID_BCM5784_B0, "BCM5784 B0" },
	/* 5754 and 5787 share the same ASIC ID */
	{ BGE_CHIPID_BCM5787_A0, "BCM5754/5787 A0" },
	{ BGE_CHIPID_BCM5787_A1, "BCM5754/5787 A1" },
	{ BGE_CHIPID_BCM5787_A2, "BCM5754/5787 A2" },
	{ BGE_CHIPID_BCM5906_A0, "BCM5906 A0" },
	{ BGE_CHIPID_BCM5906_A1, "BCM5906 A1" },
	{ BGE_CHIPID_BCM5906_A2, "BCM5906 A2" },
	{ BGE_CHIPID_BCM57765_A0, "BCM57765 A0" },
	{ BGE_CHIPID_BCM57765_B0, "BCM57765 B0" },
	{ BGE_CHIPID_BCM57780_A0, "BCM57780 A0" },
	{ BGE_CHIPID_BCM57780_A1, "BCM57780 A1" },

	{ 0, NULL }
};

/*
 * Some defaults for major revisions, so that newer steppings
 * that we don't know about have a shot at working.
 */
static const struct bge_revision bge_majorrevs[] = {
	{ BGE_ASICREV_BCM5700, "unknown BCM5700" },
	{ BGE_ASICREV_BCM5701, "unknown BCM5701" },
	{ BGE_ASICREV_BCM5703, "unknown BCM5703" },
	{ BGE_ASICREV_BCM5704, "unknown BCM5704" },
	{ BGE_ASICREV_BCM5705, "unknown BCM5705" },
	{ BGE_ASICREV_BCM5750, "unknown BCM5750" },
	{ BGE_ASICREV_BCM5714, "unknown BCM5714" },
	{ BGE_ASICREV_BCM5714_A0, "unknown BCM5714" },
	{ BGE_ASICREV_BCM5752, "unknown BCM5752" },
	{ BGE_ASICREV_BCM5780, "unknown BCM5780" },
	{ BGE_ASICREV_BCM5755, "unknown BCM5755" },
	{ BGE_ASICREV_BCM5761, "unknown BCM5761" },
	{ BGE_ASICREV_BCM5784, "unknown BCM5784" },
	{ BGE_ASICREV_BCM5785, "unknown BCM5785" },
	/* 5754 and 5787 share the same ASIC ID */
	{ BGE_ASICREV_BCM5787, "unknown BCM5754/5787" },
	{ BGE_ASICREV_BCM5906, "unknown BCM5906" },
	{ BGE_ASICREV_BCM57765, "unknown BCM57765" },
	{ BGE_ASICREV_BCM57766, "unknown BCM57766" },
	{ BGE_ASICREV_BCM57780, "unknown BCM57780" },
	{ BGE_ASICREV_BCM5717, "unknown BCM5717" },
	{ BGE_ASICREV_BCM5719, "unknown BCM5719" },
	{ BGE_ASICREV_BCM5720, "unknown BCM5720" },

	{ 0, NULL }
};

static int bge_allow_asf = 1;

CFATTACH_DECL3_NEW(bge, sizeof(struct bge_softc),
    bge_probe, bge_attach, bge_detach, NULL, NULL, NULL, DVF_DETACH_SHUTDOWN);

static uint32_t
bge_readmem_ind(struct bge_softc *sc, int off)
{
	pcireg_t val;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906 &&
	    off >= BGE_STATS_BLOCK && off < BGE_SEND_RING_1_TO_4)
		return 0;

	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_MEMWIN_BASEADDR, off);
	val = pci_conf_read(sc->sc_pc, sc->sc_pcitag, BGE_PCI_MEMWIN_DATA);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_MEMWIN_BASEADDR, 0);
	return val;
}

static void
bge_writemem_ind(struct bge_softc *sc, int off, int val)
{

	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_MEMWIN_BASEADDR, off);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_MEMWIN_DATA, val);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_MEMWIN_BASEADDR, 0);
}

/*
 * PCI Express only
 */
static void
bge_set_max_readrq(struct bge_softc *sc)
{
	pcireg_t val;

	val = pci_conf_read(sc->sc_pc, sc->sc_pcitag, sc->bge_pciecap
	    + PCIE_DCSR);
	val &= ~PCIE_DCSR_MAX_READ_REQ;
	switch (sc->bge_expmrq) {
	case 2048:
		val |= BGE_PCIE_DEVCTL_MAX_READRQ_2048;
		break;
	case 4096:
		val |= BGE_PCIE_DEVCTL_MAX_READRQ_4096;
		break;
	default:
		panic("incorrect expmrq value(%d)", sc->bge_expmrq);
		break;
	}
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, sc->bge_pciecap
	    + PCIE_DCSR, val);
}

#ifdef notdef
static uint32_t
bge_readreg_ind(struct bge_softc *sc, int off)
{
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_REG_BASEADDR, off);
	return (pci_conf_read(sc->sc_pc, sc->sc_pcitag, BGE_PCI_REG_DATA));
}
#endif

static void
bge_writereg_ind(struct bge_softc *sc, int off, int val)
{
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_REG_BASEADDR, off);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_REG_DATA, val);
}

static void
bge_writemem_direct(struct bge_softc *sc, int off, int val)
{
	CSR_WRITE_4(sc, off, val);
}

static void
bge_writembx(struct bge_softc *sc, int off, int val)
{
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906)
		off += BGE_LPMBX_IRQ0_HI - BGE_MBX_IRQ0_HI;

	CSR_WRITE_4(sc, off, val);
}

static void
bge_writembx_flush(struct bge_softc *sc, int off, int val)
{
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906)
		off += BGE_LPMBX_IRQ0_HI - BGE_MBX_IRQ0_HI;

	CSR_WRITE_4_FLUSH(sc, off, val);
}

/*
 * Clear all stale locks and select the lock for this driver instance.
 */
void
bge_ape_lock_init(struct bge_softc *sc)
{
	struct pci_attach_args *pa = &(sc->bge_pa);
	uint32_t bit, regbase;
	int i;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5761)
		regbase = BGE_APE_LOCK_GRANT;
	else
		regbase = BGE_APE_PER_LOCK_GRANT;

	/* Clear any stale locks. */
	for (i = BGE_APE_LOCK_PHY0; i <= BGE_APE_LOCK_GPIO; i++) {
		switch (i) {
		case BGE_APE_LOCK_PHY0:
		case BGE_APE_LOCK_PHY1:
		case BGE_APE_LOCK_PHY2:
		case BGE_APE_LOCK_PHY3:
			bit = BGE_APE_LOCK_GRANT_DRIVER0;
			break;
		default:
			if (pa->pa_function == 0)
				bit = BGE_APE_LOCK_GRANT_DRIVER0;
			else
				bit = (1 << pa->pa_function);
		}
		APE_WRITE_4(sc, regbase + 4 * i, bit);
	}

	/* Select the PHY lock based on the device's function number. */
	switch (pa->pa_function) {
	case 0:
		sc->bge_phy_ape_lock = BGE_APE_LOCK_PHY0;
		break;
	case 1:
		sc->bge_phy_ape_lock = BGE_APE_LOCK_PHY1;
		break;
	case 2:
		sc->bge_phy_ape_lock = BGE_APE_LOCK_PHY2;
		break;
	case 3:
		sc->bge_phy_ape_lock = BGE_APE_LOCK_PHY3;
		break;
	default:
		printf("%s: PHY lock not supported on function\n",
		    device_xname(sc->bge_dev));
		break;
	}
}

/*
 * Check for APE firmware, set flags, and print version info.
 */
void
bge_ape_read_fw_ver(struct bge_softc *sc)
{
	const char *fwtype;
	uint32_t apedata, features;

	/* Check for a valid APE signature in shared memory. */
	apedata = APE_READ_4(sc, BGE_APE_SEG_SIG);
	if (apedata != BGE_APE_SEG_SIG_MAGIC) {
		sc->bge_mfw_flags &= ~ BGE_MFW_ON_APE;
		return;
	}

	/* Check if APE firmware is running. */
	apedata = APE_READ_4(sc, BGE_APE_FW_STATUS);
	if ((apedata & BGE_APE_FW_STATUS_READY) == 0) {
		printf("%s: APE signature found but FW status not ready! "
		    "0x%08x\n", device_xname(sc->bge_dev), apedata);
		return;
	}

	sc->bge_mfw_flags |= BGE_MFW_ON_APE;

	/* Fetch the APE firwmare type and version. */
	apedata = APE_READ_4(sc, BGE_APE_FW_VERSION);
	features = APE_READ_4(sc, BGE_APE_FW_FEATURES);
	if ((features & BGE_APE_FW_FEATURE_NCSI) != 0) {
		sc->bge_mfw_flags |= BGE_MFW_TYPE_NCSI;
		fwtype = "NCSI";
	} else if ((features & BGE_APE_FW_FEATURE_DASH) != 0) {
		sc->bge_mfw_flags |= BGE_MFW_TYPE_DASH;
		fwtype = "DASH";
	} else
		fwtype = "UNKN";

	/* Print the APE firmware version. */
	aprint_normal_dev(sc->bge_dev, "APE firmware %s %d.%d.%d.%d\n", fwtype,
	    (apedata & BGE_APE_FW_VERSION_MAJMSK) >> BGE_APE_FW_VERSION_MAJSFT,
	    (apedata & BGE_APE_FW_VERSION_MINMSK) >> BGE_APE_FW_VERSION_MINSFT,
	    (apedata & BGE_APE_FW_VERSION_REVMSK) >> BGE_APE_FW_VERSION_REVSFT,
	    (apedata & BGE_APE_FW_VERSION_BLDMSK));
}

int
bge_ape_lock(struct bge_softc *sc, int locknum)
{
	struct pci_attach_args *pa = &(sc->bge_pa);
	uint32_t bit, gnt, req, status;
	int i, off;

	if ((sc->bge_mfw_flags & BGE_MFW_ON_APE) == 0)
		return (0);

	/* Lock request/grant registers have different bases. */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5761) {
		req = BGE_APE_LOCK_REQ;
		gnt = BGE_APE_LOCK_GRANT;
	} else {
		req = BGE_APE_PER_LOCK_REQ;
		gnt = BGE_APE_PER_LOCK_GRANT;
	}

	off = 4 * locknum;

	switch (locknum) {
	case BGE_APE_LOCK_GPIO:
		/* Lock required when using GPIO. */
		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5761)
			return (0);
		if (pa->pa_function == 0)
			bit = BGE_APE_LOCK_REQ_DRIVER0;
		else
			bit = (1 << pa->pa_function);
		break;
	case BGE_APE_LOCK_GRC:
		/* Lock required to reset the device. */
		if (pa->pa_function == 0)
			bit = BGE_APE_LOCK_REQ_DRIVER0;
		else
			bit = (1 << pa->pa_function);
		break;
	case BGE_APE_LOCK_MEM:
		/* Lock required when accessing certain APE memory. */
		if (pa->pa_function == 0)
			bit = BGE_APE_LOCK_REQ_DRIVER0;
		else
			bit = (1 << pa->pa_function);
		break;
	case BGE_APE_LOCK_PHY0:
	case BGE_APE_LOCK_PHY1:
	case BGE_APE_LOCK_PHY2:
	case BGE_APE_LOCK_PHY3:
		/* Lock required when accessing PHYs. */
		bit = BGE_APE_LOCK_REQ_DRIVER0;
		break;
	default:
		return (EINVAL);
	}

	/* Request a lock. */
	APE_WRITE_4_FLUSH(sc, req + off, bit);

	/* Wait up to 1 second to acquire lock. */
	for (i = 0; i < 20000; i++) {
		status = APE_READ_4(sc, gnt + off);
		if (status == bit)
			break;
		DELAY(50);
	}

	/* Handle any errors. */
	if (status != bit) {
		printf("%s: APE lock %d request failed! "
		    "request = 0x%04x[0x%04x], status = 0x%04x[0x%04x]\n",
		    device_xname(sc->bge_dev),
		    locknum, req + off, bit & 0xFFFF, gnt + off,
		    status & 0xFFFF);
		/* Revoke the lock request. */
		APE_WRITE_4(sc, gnt + off, bit);
		return (EBUSY);
	}

	return (0);
}

void
bge_ape_unlock(struct bge_softc *sc, int locknum)
{
	struct pci_attach_args *pa = &(sc->bge_pa);
	uint32_t bit, gnt;
	int off;

	if ((sc->bge_mfw_flags & BGE_MFW_ON_APE) == 0)
		return;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5761)
		gnt = BGE_APE_LOCK_GRANT;
	else
		gnt = BGE_APE_PER_LOCK_GRANT;

	off = 4 * locknum;

	switch (locknum) {
	case BGE_APE_LOCK_GPIO:
		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5761)
			return;
		if (pa->pa_function == 0)
			bit = BGE_APE_LOCK_GRANT_DRIVER0;
		else
			bit = (1 << pa->pa_function);
		break;
	case BGE_APE_LOCK_GRC:
		if (pa->pa_function == 0)
			bit = BGE_APE_LOCK_GRANT_DRIVER0;
		else
			bit = (1 << pa->pa_function);
		break;
	case BGE_APE_LOCK_MEM:
		if (pa->pa_function == 0)
			bit = BGE_APE_LOCK_GRANT_DRIVER0;
		else
			bit = (1 << pa->pa_function);
		break;
	case BGE_APE_LOCK_PHY0:
	case BGE_APE_LOCK_PHY1:
	case BGE_APE_LOCK_PHY2:
	case BGE_APE_LOCK_PHY3:
		bit = BGE_APE_LOCK_GRANT_DRIVER0;
		break;
	default:
		return;
	}

	/* Write and flush for consecutive bge_ape_lock() */
	APE_WRITE_4_FLUSH(sc, gnt + off, bit);
}

/*
 * Send an event to the APE firmware.
 */
void
bge_ape_send_event(struct bge_softc *sc, uint32_t event)
{
	uint32_t apedata;
	int i;

	/* NCSI does not support APE events. */
	if ((sc->bge_mfw_flags & BGE_MFW_ON_APE) == 0)
		return;

	/* Wait up to 1ms for APE to service previous event. */
	for (i = 10; i > 0; i--) {
		if (bge_ape_lock(sc, BGE_APE_LOCK_MEM) != 0)
			break;
		apedata = APE_READ_4(sc, BGE_APE_EVENT_STATUS);
		if ((apedata & BGE_APE_EVENT_STATUS_EVENT_PENDING) == 0) {
			APE_WRITE_4(sc, BGE_APE_EVENT_STATUS, event |
			    BGE_APE_EVENT_STATUS_EVENT_PENDING);
			bge_ape_unlock(sc, BGE_APE_LOCK_MEM);
			APE_WRITE_4(sc, BGE_APE_EVENT, BGE_APE_EVENT_1);
			break;
		}
		bge_ape_unlock(sc, BGE_APE_LOCK_MEM);
		DELAY(100);
	}
	if (i == 0) {
		printf("%s: APE event 0x%08x send timed out\n",
		    device_xname(sc->bge_dev), event);
	}
}

void
bge_ape_driver_state_change(struct bge_softc *sc, int kind)
{
	uint32_t apedata, event;

	if ((sc->bge_mfw_flags & BGE_MFW_ON_APE) == 0)
		return;

	switch (kind) {
	case BGE_RESET_START:
		/* If this is the first load, clear the load counter. */
		apedata = APE_READ_4(sc, BGE_APE_HOST_SEG_SIG);
		if (apedata != BGE_APE_HOST_SEG_SIG_MAGIC)
			APE_WRITE_4(sc, BGE_APE_HOST_INIT_COUNT, 0);
		else {
			apedata = APE_READ_4(sc, BGE_APE_HOST_INIT_COUNT);
			APE_WRITE_4(sc, BGE_APE_HOST_INIT_COUNT, ++apedata);
		}
		APE_WRITE_4(sc, BGE_APE_HOST_SEG_SIG,
		    BGE_APE_HOST_SEG_SIG_MAGIC);
		APE_WRITE_4(sc, BGE_APE_HOST_SEG_LEN,
		    BGE_APE_HOST_SEG_LEN_MAGIC);

		/* Add some version info if bge(4) supports it. */
		APE_WRITE_4(sc, BGE_APE_HOST_DRIVER_ID,
		    BGE_APE_HOST_DRIVER_ID_MAGIC(1, 0));
		APE_WRITE_4(sc, BGE_APE_HOST_BEHAVIOR,
		    BGE_APE_HOST_BEHAV_NO_PHYLOCK);
		APE_WRITE_4(sc, BGE_APE_HOST_HEARTBEAT_INT_MS,
		    BGE_APE_HOST_HEARTBEAT_INT_DISABLE);
		APE_WRITE_4(sc, BGE_APE_HOST_DRVR_STATE,
		    BGE_APE_HOST_DRVR_STATE_START);
		event = BGE_APE_EVENT_STATUS_STATE_START;
		break;
	case BGE_RESET_SHUTDOWN:
		APE_WRITE_4(sc, BGE_APE_HOST_DRVR_STATE,
		    BGE_APE_HOST_DRVR_STATE_UNLOAD);
		event = BGE_APE_EVENT_STATUS_STATE_UNLOAD;
		break;
	case BGE_RESET_SUSPEND:
		event = BGE_APE_EVENT_STATUS_STATE_SUSPEND;
		break;
	default:
		return;
	}

	bge_ape_send_event(sc, event | BGE_APE_EVENT_STATUS_DRIVER_EVNT |
	    BGE_APE_EVENT_STATUS_STATE_CHNGE);
}

static uint8_t
bge_nvram_getbyte(struct bge_softc *sc, int addr, uint8_t *dest)
{
	uint32_t access, byte = 0;
	int i;

	/* Lock. */
	CSR_WRITE_4(sc, BGE_NVRAM_SWARB, BGE_NVRAMSWARB_SET1);
	for (i = 0; i < 8000; i++) {
		if (CSR_READ_4(sc, BGE_NVRAM_SWARB) & BGE_NVRAMSWARB_GNT1)
			break;
		DELAY(20);
	}
	if (i == 8000)
		return 1;

	/* Enable access. */
	access = CSR_READ_4(sc, BGE_NVRAM_ACCESS);
	CSR_WRITE_4(sc, BGE_NVRAM_ACCESS, access | BGE_NVRAMACC_ENABLE);

	CSR_WRITE_4(sc, BGE_NVRAM_ADDR, addr & 0xfffffffc);
	CSR_WRITE_4(sc, BGE_NVRAM_CMD, BGE_NVRAM_READCMD);
	for (i = 0; i < BGE_TIMEOUT * 10; i++) {
		DELAY(10);
		if (CSR_READ_4(sc, BGE_NVRAM_CMD) & BGE_NVRAMCMD_DONE) {
			DELAY(10);
			break;
		}
	}

	if (i == BGE_TIMEOUT * 10) {
		aprint_error_dev(sc->bge_dev, "nvram read timed out\n");
		return 1;
	}

	/* Get result. */
	byte = CSR_READ_4(sc, BGE_NVRAM_RDDATA);

	*dest = (bswap32(byte) >> ((addr % 4) * 8)) & 0xFF;

	/* Disable access. */
	CSR_WRITE_4(sc, BGE_NVRAM_ACCESS, access);

	/* Unlock. */
	CSR_WRITE_4_FLUSH(sc, BGE_NVRAM_SWARB, BGE_NVRAMSWARB_CLR1);

	return 0;
}

/*
 * Read a sequence of bytes from NVRAM.
 */
static int
bge_read_nvram(struct bge_softc *sc, uint8_t *dest, int off, int cnt)
{
	int error = 0, i;
	uint8_t byte = 0;

	if (BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5906)
		return 1;

	for (i = 0; i < cnt; i++) {
		error = bge_nvram_getbyte(sc, off + i, &byte);
		if (error)
			break;
		*(dest + i) = byte;
	}

	return (error ? 1 : 0);
}

/*
 * Read a byte of data stored in the EEPROM at address 'addr.' The
 * BCM570x supports both the traditional bitbang interface and an
 * auto access interface for reading the EEPROM. We use the auto
 * access method.
 */
static uint8_t
bge_eeprom_getbyte(struct bge_softc *sc, int addr, uint8_t *dest)
{
	int i;
	uint32_t byte = 0;

	/*
	 * Enable use of auto EEPROM access so we can avoid
	 * having to use the bitbang method.
	 */
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_AUTO_EEPROM);

	/* Reset the EEPROM, load the clock period. */
	CSR_WRITE_4(sc, BGE_EE_ADDR,
	    BGE_EEADDR_RESET | BGE_EEHALFCLK(BGE_HALFCLK_384SCL));
	DELAY(20);

	/* Issue the read EEPROM command. */
	CSR_WRITE_4(sc, BGE_EE_ADDR, BGE_EE_READCMD | addr);

	/* Wait for completion */
	for (i = 0; i < BGE_TIMEOUT * 10; i++) {
		DELAY(10);
		if (CSR_READ_4(sc, BGE_EE_ADDR) & BGE_EEADDR_DONE)
			break;
	}

	if (i == BGE_TIMEOUT * 10) {
		aprint_error_dev(sc->bge_dev, "eeprom read timed out\n");
		return 1;
	}

	/* Get result. */
	byte = CSR_READ_4(sc, BGE_EE_DATA);

	*dest = (byte >> ((addr % 4) * 8)) & 0xFF;

	return 0;
}

/*
 * Read a sequence of bytes from the EEPROM.
 */
static int
bge_read_eeprom(struct bge_softc *sc, void *destv, int off, int cnt)
{
	int error = 0, i;
	uint8_t byte = 0;
	char *dest = destv;

	for (i = 0; i < cnt; i++) {
		error = bge_eeprom_getbyte(sc, off + i, &byte);
		if (error)
			break;
		*(dest + i) = byte;
	}

	return (error ? 1 : 0);
}

static int
bge_miibus_readreg(device_t dev, int phy, int reg)
{
	struct bge_softc *sc = device_private(dev);
	uint32_t val;
	uint32_t autopoll;
	int i;

	if (bge_ape_lock(sc, sc->bge_phy_ape_lock) != 0)
		return 0;

	/* Reading with autopolling on may trigger PCI errors */
	autopoll = CSR_READ_4(sc, BGE_MI_MODE);
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_STS_CLRBIT(sc, BGE_STS_AUTOPOLL);
		BGE_CLRBIT_FLUSH(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(80);
	}

	CSR_WRITE_4_FLUSH(sc, BGE_MI_COMM, BGE_MICMD_READ | BGE_MICOMM_BUSY |
	    BGE_MIPHY(phy) | BGE_MIREG(reg));

	for (i = 0; i < BGE_TIMEOUT; i++) {
		delay(10);
		val = CSR_READ_4(sc, BGE_MI_COMM);
		if (!(val & BGE_MICOMM_BUSY)) {
			DELAY(5);
			val = CSR_READ_4(sc, BGE_MI_COMM);
			break;
		}
	}

	if (i == BGE_TIMEOUT) {
		aprint_error_dev(sc->bge_dev, "PHY read timed out\n");
		val = 0;
		goto done;
	}

done:
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_STS_SETBIT(sc, BGE_STS_AUTOPOLL);
		BGE_SETBIT_FLUSH(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(80);
	}

	bge_ape_unlock(sc, sc->bge_phy_ape_lock);

	if (val & BGE_MICOMM_READFAIL)
		return 0;

	return (val & 0xFFFF);
}

static void
bge_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct bge_softc *sc = device_private(dev);
	uint32_t autopoll;
	int i;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906 &&
	    (reg == BRGPHY_MII_1000CTL || reg == BRGPHY_MII_AUXCTL))
		return;

	if (bge_ape_lock(sc, sc->bge_phy_ape_lock) != 0)
		return;

	/* Reading with autopolling on may trigger PCI errors */
	autopoll = CSR_READ_4(sc, BGE_MI_MODE);
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_STS_CLRBIT(sc, BGE_STS_AUTOPOLL);
		BGE_CLRBIT_FLUSH(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(80);
	}

	CSR_WRITE_4_FLUSH(sc, BGE_MI_COMM, BGE_MICMD_WRITE | BGE_MICOMM_BUSY |
	    BGE_MIPHY(phy) | BGE_MIREG(reg) | val);

	for (i = 0; i < BGE_TIMEOUT; i++) {
		delay(10);
		if (!(CSR_READ_4(sc, BGE_MI_COMM) & BGE_MICOMM_BUSY)) {
			delay(5);
			CSR_READ_4(sc, BGE_MI_COMM);
			break;
		}
	}

	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_STS_SETBIT(sc, BGE_STS_AUTOPOLL);
		BGE_SETBIT_FLUSH(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		delay(80);
	}

	bge_ape_unlock(sc, sc->bge_phy_ape_lock);

	if (i == BGE_TIMEOUT)
		aprint_error_dev(sc->bge_dev, "PHY read timed out\n");
}

static void
bge_miibus_statchg(struct ifnet *ifp)
{
	struct bge_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->bge_mii;
	uint32_t mac_mode, rx_mode, tx_mode;

	/*
	 * Get flow control negotiation result.
	 */
	if (IFM_SUBTYPE(mii->mii_media.ifm_cur->ifm_media) == IFM_AUTO &&
	    (mii->mii_media_active & IFM_ETH_FMASK) != sc->bge_flowflags)
		sc->bge_flowflags = mii->mii_media_active & IFM_ETH_FMASK;

	if (!BGE_STS_BIT(sc, BGE_STS_LINK) &&
	    mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE)
		BGE_STS_SETBIT(sc, BGE_STS_LINK);
	else if (BGE_STS_BIT(sc, BGE_STS_LINK) &&
	    (!(mii->mii_media_status & IFM_ACTIVE) ||
	    IFM_SUBTYPE(mii->mii_media_active) == IFM_NONE))
		BGE_STS_CLRBIT(sc, BGE_STS_LINK);

	if (!BGE_STS_BIT(sc, BGE_STS_LINK))
		return;

	/* Set the port mode (MII/GMII) to match the link speed. */
	mac_mode = CSR_READ_4(sc, BGE_MAC_MODE) &
	    ~(BGE_MACMODE_PORTMODE | BGE_MACMODE_HALF_DUPLEX);
	tx_mode = CSR_READ_4(sc, BGE_TX_MODE);
	rx_mode = CSR_READ_4(sc, BGE_RX_MODE);
	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T ||
	    IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_SX)
		mac_mode |= BGE_PORTMODE_GMII;
	else
		mac_mode |= BGE_PORTMODE_MII;

	tx_mode &= ~BGE_TXMODE_FLOWCTL_ENABLE;
	rx_mode &= ~BGE_RXMODE_FLOWCTL_ENABLE;
	if ((mii->mii_media_active & IFM_FDX) != 0) {
		if (sc->bge_flowflags & IFM_ETH_TXPAUSE)
			tx_mode |= BGE_TXMODE_FLOWCTL_ENABLE;
		if (sc->bge_flowflags & IFM_ETH_RXPAUSE)
			rx_mode |= BGE_RXMODE_FLOWCTL_ENABLE;
	} else
		mac_mode |= BGE_MACMODE_HALF_DUPLEX;

	CSR_WRITE_4_FLUSH(sc, BGE_MAC_MODE, mac_mode);
	DELAY(40);
	CSR_WRITE_4(sc, BGE_TX_MODE, tx_mode);
	CSR_WRITE_4(sc, BGE_RX_MODE, rx_mode);
}

/*
 * Update rx threshold levels to values in a particular slot
 * of the interrupt-mitigation table bge_rx_threshes.
 */
static void
bge_set_thresh(struct ifnet *ifp, int lvl)
{
	struct bge_softc *sc = ifp->if_softc;
	int s;

	/* For now, just save the new Rx-intr thresholds and record
	 * that a threshold update is pending.  Updating the hardware
	 * registers here (even at splhigh()) is observed to
	 * occasionaly cause glitches where Rx-interrupts are not
	 * honoured for up to 10 seconds. jonathan@NetBSD.org, 2003-04-05
	 */
	s = splnet();
	sc->bge_rx_coal_ticks = bge_rx_threshes[lvl].rx_ticks;
	sc->bge_rx_max_coal_bds = bge_rx_threshes[lvl].rx_max_bds;
	sc->bge_pending_rxintr_change = 1;
	splx(s);
}


/*
 * Update Rx thresholds of all bge devices
 */
static void
bge_update_all_threshes(int lvl)
{
	struct ifnet *ifp;
	const char * const namebuf = "bge";
	int namelen;

	if (lvl < 0)
		lvl = 0;
	else if (lvl >= NBGE_RX_THRESH)
		lvl = NBGE_RX_THRESH - 1;

	namelen = strlen(namebuf);
	/*
	 * Now search all the interfaces for this name/number
	 */
	IFNET_FOREACH(ifp) {
		if (strncmp(ifp->if_xname, namebuf, namelen) != 0)
		      continue;
		/* We got a match: update if doing auto-threshold-tuning */
		if (bge_auto_thresh)
			bge_set_thresh(ifp, lvl);
	}
}

/*
 * Handle events that have triggered interrupts.
 */
static void
bge_handle_events(struct bge_softc *sc)
{

	return;
}

/*
 * Memory management for jumbo frames.
 */

static int
bge_alloc_jumbo_mem(struct bge_softc *sc)
{
	char *ptr, *kva;
	bus_dma_segment_t	seg;
	int		i, rseg, state, error;
	struct bge_jpool_entry   *entry;

	state = error = 0;

	/* Grab a big chunk o' storage. */
	if (bus_dmamem_alloc(sc->bge_dmatag, BGE_JMEM, PAGE_SIZE, 0,
	     &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->bge_dev, "can't alloc rx buffers\n");
		return ENOBUFS;
	}

	state = 1;
	if (bus_dmamem_map(sc->bge_dmatag, &seg, rseg, BGE_JMEM, (void **)&kva,
	    BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->bge_dev,
		    "can't map DMA buffers (%d bytes)\n", (int)BGE_JMEM);
		error = ENOBUFS;
		goto out;
	}

	state = 2;
	if (bus_dmamap_create(sc->bge_dmatag, BGE_JMEM, 1, BGE_JMEM, 0,
	    BUS_DMA_NOWAIT, &sc->bge_cdata.bge_rx_jumbo_map)) {
		aprint_error_dev(sc->bge_dev, "can't create DMA map\n");
		error = ENOBUFS;
		goto out;
	}

	state = 3;
	if (bus_dmamap_load(sc->bge_dmatag, sc->bge_cdata.bge_rx_jumbo_map,
	    kva, BGE_JMEM, NULL, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->bge_dev, "can't load DMA map\n");
		error = ENOBUFS;
		goto out;
	}

	state = 4;
	sc->bge_cdata.bge_jumbo_buf = (void *)kva;
	DPRINTFN(1,("bge_jumbo_buf = %p\n", sc->bge_cdata.bge_jumbo_buf));

	SLIST_INIT(&sc->bge_jfree_listhead);
	SLIST_INIT(&sc->bge_jinuse_listhead);

	/*
	 * Now divide it up into 9K pieces and save the addresses
	 * in an array.
	 */
	ptr = sc->bge_cdata.bge_jumbo_buf;
	for (i = 0; i < BGE_JSLOTS; i++) {
		sc->bge_cdata.bge_jslots[i] = ptr;
		ptr += BGE_JLEN;
		entry = malloc(sizeof(struct bge_jpool_entry),
		    M_DEVBUF, M_NOWAIT);
		if (entry == NULL) {
			aprint_error_dev(sc->bge_dev,
			    "no memory for jumbo buffer queue!\n");
			error = ENOBUFS;
			goto out;
		}
		entry->slot = i;
		SLIST_INSERT_HEAD(&sc->bge_jfree_listhead,
				 entry, jpool_entries);
	}
out:
	if (error != 0) {
		switch (state) {
		case 4:
			bus_dmamap_unload(sc->bge_dmatag,
			    sc->bge_cdata.bge_rx_jumbo_map);
		case 3:
			bus_dmamap_destroy(sc->bge_dmatag,
			    sc->bge_cdata.bge_rx_jumbo_map);
		case 2:
			bus_dmamem_unmap(sc->bge_dmatag, kva, BGE_JMEM);
		case 1:
			bus_dmamem_free(sc->bge_dmatag, &seg, rseg);
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
static void *
bge_jalloc(struct bge_softc *sc)
{
	struct bge_jpool_entry   *entry;

	entry = SLIST_FIRST(&sc->bge_jfree_listhead);

	if (entry == NULL) {
		aprint_error_dev(sc->bge_dev, "no free jumbo buffers\n");
		return NULL;
	}

	SLIST_REMOVE_HEAD(&sc->bge_jfree_listhead, jpool_entries);
	SLIST_INSERT_HEAD(&sc->bge_jinuse_listhead, entry, jpool_entries);
	return (sc->bge_cdata.bge_jslots[entry->slot]);
}

/*
 * Release a jumbo buffer.
 */
static void
bge_jfree(struct mbuf *m, void *buf, size_t size, void *arg)
{
	struct bge_jpool_entry *entry;
	struct bge_softc *sc;
	int i, s;

	/* Extract the softc struct pointer. */
	sc = (struct bge_softc *)arg;

	if (sc == NULL)
		panic("bge_jfree: can't find softc pointer!");

	/* calculate the slot this buffer belongs to */

	i = ((char *)buf
	     - (char *)sc->bge_cdata.bge_jumbo_buf) / BGE_JLEN;

	if ((i < 0) || (i >= BGE_JSLOTS))
		panic("bge_jfree: asked to free buffer that we don't manage!");

	s = splvm();
	entry = SLIST_FIRST(&sc->bge_jinuse_listhead);
	if (entry == NULL)
		panic("bge_jfree: buffer not in use!");
	entry->slot = i;
	SLIST_REMOVE_HEAD(&sc->bge_jinuse_listhead, jpool_entries);
	SLIST_INSERT_HEAD(&sc->bge_jfree_listhead, entry, jpool_entries);

	if (__predict_true(m != NULL))
  		pool_cache_put(mb_cache, m);
	splx(s);
}


/*
 * Initialize a standard receive ring descriptor.
 */
static int
bge_newbuf_std(struct bge_softc *sc, int i, struct mbuf *m,
    bus_dmamap_t dmamap)
{
	struct mbuf		*m_new = NULL;
	struct bge_rx_bd	*r;
	int			error;

	if (dmamap == NULL) {
		error = bus_dmamap_create(sc->bge_dmatag, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_NOWAIT, &dmamap);
		if (error != 0)
			return error;
	}

	sc->bge_cdata.bge_rx_std_map[i] = dmamap;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return ENOBUFS;

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			return ENOBUFS;
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;

	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}
	if (!(sc->bge_flags & BGEF_RX_ALIGNBUG))
	    m_adj(m_new, ETHER_ALIGN);
	if (bus_dmamap_load_mbuf(sc->bge_dmatag, dmamap, m_new,
	    BUS_DMA_READ|BUS_DMA_NOWAIT)) {
		m_freem(m_new);
		return ENOBUFS;
	}
	bus_dmamap_sync(sc->bge_dmatag, dmamap, 0, dmamap->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	sc->bge_cdata.bge_rx_std_chain[i] = m_new;
	r = &sc->bge_rdata->bge_rx_std_ring[i];
	BGE_HOSTADDR(r->bge_addr, dmamap->dm_segs[0].ds_addr);
	r->bge_flags = BGE_RXBDFLAG_END;
	r->bge_len = m_new->m_len;
	r->bge_idx = i;

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offsetof(struct bge_ring_data, bge_rx_std_ring) +
		i * sizeof (struct bge_rx_bd),
	    sizeof (struct bge_rx_bd),
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	return 0;
}

/*
 * Initialize a jumbo receive ring descriptor. This allocates
 * a jumbo buffer from the pool managed internally by the driver.
 */
static int
bge_newbuf_jumbo(struct bge_softc *sc, int i, struct mbuf *m)
{
	struct mbuf *m_new = NULL;
	struct bge_rx_bd *r;
	void *buf = NULL;

	if (m == NULL) {

		/* Allocate the mbuf. */
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return ENOBUFS;

		/* Allocate the jumbo buffer */
		buf = bge_jalloc(sc);
		if (buf == NULL) {
			m_freem(m_new);
			aprint_error_dev(sc->bge_dev,
			    "jumbo allocation failed -- packet dropped!\n");
			return ENOBUFS;
		}

		/* Attach the buffer to the mbuf. */
		m_new->m_len = m_new->m_pkthdr.len = BGE_JUMBO_FRAMELEN;
		MEXTADD(m_new, buf, BGE_JUMBO_FRAMELEN, M_DEVBUF,
		    bge_jfree, sc);
		m_new->m_flags |= M_EXT_RW;
	} else {
		m_new = m;
		buf = m_new->m_data = m_new->m_ext.ext_buf;
		m_new->m_ext.ext_size = BGE_JUMBO_FRAMELEN;
	}
	if (!(sc->bge_flags & BGEF_RX_ALIGNBUG))
	    m_adj(m_new, ETHER_ALIGN);
	bus_dmamap_sync(sc->bge_dmatag, sc->bge_cdata.bge_rx_jumbo_map,
	    mtod(m_new, char *) - (char *)sc->bge_cdata.bge_jumbo_buf, BGE_JLEN,
	    BUS_DMASYNC_PREREAD);
	/* Set up the descriptor. */
	r = &sc->bge_rdata->bge_rx_jumbo_ring[i];
	sc->bge_cdata.bge_rx_jumbo_chain[i] = m_new;
	BGE_HOSTADDR(r->bge_addr, BGE_JUMBO_DMA_ADDR(sc, m_new));
	r->bge_flags = BGE_RXBDFLAG_END|BGE_RXBDFLAG_JUMBO_RING;
	r->bge_len = m_new->m_len;
	r->bge_idx = i;

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offsetof(struct bge_ring_data, bge_rx_jumbo_ring) +
		i * sizeof (struct bge_rx_bd),
	    sizeof (struct bge_rx_bd),
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	return 0;
}

/*
 * The standard receive ring has 512 entries in it. At 2K per mbuf cluster,
 * that's 1MB or memory, which is a lot. For now, we fill only the first
 * 256 ring entries and hope that our CPU is fast enough to keep up with
 * the NIC.
 */
static int
bge_init_rx_ring_std(struct bge_softc *sc)
{
	int i;

	if (sc->bge_flags & BGEF_RXRING_VALID)
		return 0;

	for (i = 0; i < BGE_SSLOTS; i++) {
		if (bge_newbuf_std(sc, i, NULL, 0) == ENOBUFS)
			return ENOBUFS;
	}

	sc->bge_std = i - 1;
	bge_writembx(sc, BGE_MBX_RX_STD_PROD_LO, sc->bge_std);

	sc->bge_flags |= BGEF_RXRING_VALID;

	return 0;
}

static void
bge_free_rx_ring_std(struct bge_softc *sc)
{
	int i;

	if (!(sc->bge_flags & BGEF_RXRING_VALID))
		return;

	for (i = 0; i < BGE_STD_RX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_rx_std_chain[i] != NULL) {
			m_freem(sc->bge_cdata.bge_rx_std_chain[i]);
			sc->bge_cdata.bge_rx_std_chain[i] = NULL;
			bus_dmamap_destroy(sc->bge_dmatag,
			    sc->bge_cdata.bge_rx_std_map[i]);
		}
		memset((char *)&sc->bge_rdata->bge_rx_std_ring[i], 0,
		    sizeof(struct bge_rx_bd));
	}

	sc->bge_flags &= ~BGEF_RXRING_VALID;
}

static int
bge_init_rx_ring_jumbo(struct bge_softc *sc)
{
	int i;
	volatile struct bge_rcb *rcb;

	if (sc->bge_flags & BGEF_JUMBO_RXRING_VALID)
		return 0;

	for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
		if (bge_newbuf_jumbo(sc, i, NULL) == ENOBUFS)
			return ENOBUFS;
	}

	sc->bge_jumbo = i - 1;
	sc->bge_flags |= BGEF_JUMBO_RXRING_VALID;

	rcb = &sc->bge_rdata->bge_info.bge_jumbo_rx_rcb;
	rcb->bge_maxlen_flags = 0;
	CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_MAXLEN_FLAGS, rcb->bge_maxlen_flags);

	bge_writembx(sc, BGE_MBX_RX_JUMBO_PROD_LO, sc->bge_jumbo);

	return 0;
}

static void
bge_free_rx_ring_jumbo(struct bge_softc *sc)
{
	int i;

	if (!(sc->bge_flags & BGEF_JUMBO_RXRING_VALID))
		return;

	for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_rx_jumbo_chain[i] != NULL) {
			m_freem(sc->bge_cdata.bge_rx_jumbo_chain[i]);
			sc->bge_cdata.bge_rx_jumbo_chain[i] = NULL;
		}
		memset((char *)&sc->bge_rdata->bge_rx_jumbo_ring[i], 0,
		    sizeof(struct bge_rx_bd));
	}

	sc->bge_flags &= ~BGEF_JUMBO_RXRING_VALID;
}

static void
bge_free_tx_ring(struct bge_softc *sc)
{
	int i;
	struct txdmamap_pool_entry *dma;

	if (!(sc->bge_flags & BGEF_TXRING_VALID))
		return;

	for (i = 0; i < BGE_TX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_tx_chain[i] != NULL) {
			m_freem(sc->bge_cdata.bge_tx_chain[i]);
			sc->bge_cdata.bge_tx_chain[i] = NULL;
			SLIST_INSERT_HEAD(&sc->txdma_list, sc->txdma[i],
					    link);
			sc->txdma[i] = 0;
		}
		memset((char *)&sc->bge_rdata->bge_tx_ring[i], 0,
		    sizeof(struct bge_tx_bd));
	}

	while ((dma = SLIST_FIRST(&sc->txdma_list))) {
		SLIST_REMOVE_HEAD(&sc->txdma_list, link);
		bus_dmamap_destroy(sc->bge_dmatag, dma->dmamap);
		free(dma, M_DEVBUF);
	}

	sc->bge_flags &= ~BGEF_TXRING_VALID;
}

static int
bge_init_tx_ring(struct bge_softc *sc)
{
	struct ifnet *ifp = &sc->ethercom.ec_if;
	int i;
	bus_dmamap_t dmamap;
	bus_size_t maxsegsz;
	struct txdmamap_pool_entry *dma;

	if (sc->bge_flags & BGEF_TXRING_VALID)
		return 0;

	sc->bge_txcnt = 0;
	sc->bge_tx_saved_considx = 0;

	/* Initialize transmit producer index for host-memory send ring. */
	sc->bge_tx_prodidx = 0;
	bge_writembx(sc, BGE_MBX_TX_HOST_PROD0_LO, sc->bge_tx_prodidx);
	/* 5700 b2 errata */
	if (BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5700_BX)
		bge_writembx(sc, BGE_MBX_TX_HOST_PROD0_LO, sc->bge_tx_prodidx);

	/* NIC-memory send ring not used; initialize to zero. */
	bge_writembx(sc, BGE_MBX_TX_NIC_PROD0_LO, 0);
	/* 5700 b2 errata */
	if (BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5700_BX)
		bge_writembx(sc, BGE_MBX_TX_NIC_PROD0_LO, 0);

	/* Limit DMA segment size for some chips */
	if ((BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM57766) &&
	    (ifp->if_mtu <= ETHERMTU))
		maxsegsz = 2048;
	else if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5719)
		maxsegsz = 4096;
	else
		maxsegsz = ETHER_MAX_LEN_JUMBO;
	SLIST_INIT(&sc->txdma_list);
	for (i = 0; i < BGE_TX_RING_CNT; i++) {
		if (bus_dmamap_create(sc->bge_dmatag, BGE_TXDMA_MAX,
		    BGE_NTXSEG, maxsegsz, 0, BUS_DMA_NOWAIT,
		    &dmamap))
			return ENOBUFS;
		if (dmamap == NULL)
			panic("dmamap NULL in bge_init_tx_ring");
		dma = malloc(sizeof(*dma), M_DEVBUF, M_NOWAIT);
		if (dma == NULL) {
			aprint_error_dev(sc->bge_dev,
			    "can't alloc txdmamap_pool_entry\n");
			bus_dmamap_destroy(sc->bge_dmatag, dmamap);
			return ENOMEM;
		}
		dma->dmamap = dmamap;
		SLIST_INSERT_HEAD(&sc->txdma_list, dma, link);
	}

	sc->bge_flags |= BGEF_TXRING_VALID;

	return 0;
}

static void
bge_setmulti(struct bge_softc *sc)
{
	struct ethercom		*ac = &sc->ethercom;
	struct ifnet		*ifp = &ac->ec_if;
	struct ether_multi	*enm;
	struct ether_multistep  step;
	uint32_t		hashes[4] = { 0, 0, 0, 0 };
	uint32_t		h;
	int			i;

	if (ifp->if_flags & IFF_PROMISC)
		goto allmulti;

	/* Now program new ones. */
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			goto allmulti;
		}

		h = ether_crc32_le(enm->enm_addrlo, ETHER_ADDR_LEN);

		/* Just want the 7 least-significant bits. */
		h &= 0x7f;

		hashes[(h & 0x60) >> 5] |= 1 << (h & 0x1F);
		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;
	goto setit;

 allmulti:
	ifp->if_flags |= IFF_ALLMULTI;
	hashes[0] = hashes[1] = hashes[2] = hashes[3] = 0xffffffff;

 setit:
	for (i = 0; i < 4; i++)
		CSR_WRITE_4(sc, BGE_MAR0 + (i * 4), hashes[i]);
}

static void
bge_sig_pre_reset(struct bge_softc *sc, int type)
{

	/*
	 * Some chips don't like this so only do this if ASF is enabled
	 */
	if (sc->bge_asf_mode)
		bge_writemem_ind(sc, BGE_SRAM_FW_MB, BGE_SRAM_FW_MB_MAGIC);

	if (sc->bge_asf_mode & ASF_NEW_HANDSHAKE) {
		switch (type) {
		case BGE_RESET_START:
			bge_writemem_ind(sc, BGE_SRAM_FW_DRV_STATE_MB,
			    BGE_FW_DRV_STATE_START);
			break;
		case BGE_RESET_SHUTDOWN:
			bge_writemem_ind(sc, BGE_SRAM_FW_DRV_STATE_MB,
			    BGE_FW_DRV_STATE_UNLOAD);
			break;
		case BGE_RESET_SUSPEND:
			bge_writemem_ind(sc, BGE_SRAM_FW_DRV_STATE_MB,
			    BGE_FW_DRV_STATE_SUSPEND);
			break;
		}
	}

	if (type == BGE_RESET_START || type == BGE_RESET_SUSPEND)
		bge_ape_driver_state_change(sc, type);
}

static void
bge_sig_post_reset(struct bge_softc *sc, int type)
{

	if (sc->bge_asf_mode & ASF_NEW_HANDSHAKE) {
		switch (type) {
		case BGE_RESET_START:
			bge_writemem_ind(sc, BGE_SRAM_FW_DRV_STATE_MB,
			    BGE_FW_DRV_STATE_START_DONE);
			/* START DONE */
			break;
		case BGE_RESET_SHUTDOWN:
			bge_writemem_ind(sc, BGE_SRAM_FW_DRV_STATE_MB,
			    BGE_FW_DRV_STATE_UNLOAD_DONE);
			break;
		}
	}

	if (type == BGE_RESET_SHUTDOWN)
		bge_ape_driver_state_change(sc, type);
}

static void
bge_sig_legacy(struct bge_softc *sc, int type)
{

	if (sc->bge_asf_mode) {
		switch (type) {
		case BGE_RESET_START:
			bge_writemem_ind(sc, BGE_SRAM_FW_DRV_STATE_MB,
			    BGE_FW_DRV_STATE_START);
			break;
		case BGE_RESET_SHUTDOWN:
			bge_writemem_ind(sc, BGE_SRAM_FW_DRV_STATE_MB,
			    BGE_FW_DRV_STATE_UNLOAD);
			break;
		}
	}
}

static void
bge_wait_for_event_ack(struct bge_softc *sc)
{
	int i;

	/* wait up to 2500usec */
	for (i = 0; i < 250; i++) {
		if (!(CSR_READ_4(sc, BGE_RX_CPU_EVENT) &
			BGE_RX_CPU_DRV_EVENT))
			break;
		DELAY(10);
	}
}

static void
bge_stop_fw(struct bge_softc *sc)
{

	if (sc->bge_asf_mode) {
		bge_wait_for_event_ack(sc);

		bge_writemem_ind(sc, BGE_SRAM_FW_CMD_MB, BGE_FW_CMD_PAUSE);
		CSR_WRITE_4_FLUSH(sc, BGE_RX_CPU_EVENT,
		    CSR_READ_4(sc, BGE_RX_CPU_EVENT) | BGE_RX_CPU_DRV_EVENT);

		bge_wait_for_event_ack(sc);
	}
}

static int
bge_poll_fw(struct bge_softc *sc)
{
	uint32_t val;
	int i;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906) {
		for (i = 0; i < BGE_TIMEOUT; i++) {
			val = CSR_READ_4(sc, BGE_VCPU_STATUS);
			if (val & BGE_VCPU_STATUS_INIT_DONE)
				break;
			DELAY(100);
		}
		if (i >= BGE_TIMEOUT) {
			aprint_error_dev(sc->bge_dev, "reset timed out\n");
			return -1;
		}
	} else {
		/*
		 * Poll the value location we just wrote until
		 * we see the 1's complement of the magic number.
		 * This indicates that the firmware initialization
		 * is complete.
		 * XXX 1000ms for Flash and 10000ms for SEEPROM.
		 */
		for (i = 0; i < BGE_TIMEOUT; i++) {
			val = bge_readmem_ind(sc, BGE_SRAM_FW_MB);
			if (val == ~BGE_SRAM_FW_MB_MAGIC)
				break;
			DELAY(10);
		}

		if ((i >= BGE_TIMEOUT)
		    && ((sc->bge_flags & BGEF_NO_EEPROM) == 0)) {
			aprint_error_dev(sc->bge_dev,
			    "firmware handshake timed out, val = %x\n", val);
			return -1;
		}
	}

	if (sc->bge_chipid == BGE_CHIPID_BCM57765_A0) {
		/* tg3 says we have to wait extra time */
		delay(10 * 1000);
	}

	return 0;
}

int
bge_phy_addr(struct bge_softc *sc)
{
	struct pci_attach_args *pa = &(sc->bge_pa);
	int phy_addr = 1;

	/*
	 * PHY address mapping for various devices.
	 *
	 *          | F0 Cu | F0 Sr | F1 Cu | F1 Sr |
	 * ---------+-------+-------+-------+-------+
	 * BCM57XX  |   1   |   X   |   X   |   X   |
	 * BCM5704  |   1   |   X   |   1   |   X   |
	 * BCM5717  |   1   |   8   |   2   |   9   |
	 * BCM5719  |   1   |   8   |   2   |   9   |
	 * BCM5720  |   1   |   8   |   2   |   9   |
	 *
	 *          | F2 Cu | F2 Sr | F3 Cu | F3 Sr |
	 * ---------+-------+-------+-------+-------+
	 * BCM57XX  |   X   |   X   |   X   |   X   |
	 * BCM5704  |   X   |   X   |   X   |   X   |
	 * BCM5717  |   X   |   X   |   X   |   X   |
	 * BCM5719  |   3   |   10  |   4   |   11  |
	 * BCM5720  |   X   |   X   |   X   |   X   |
	 *
	 * Other addresses may respond but they are not
	 * IEEE compliant PHYs and should be ignored.
	 */
	switch (BGE_ASICREV(sc->bge_chipid)) {
	case BGE_ASICREV_BCM5717:
	case BGE_ASICREV_BCM5719:
	case BGE_ASICREV_BCM5720:
		phy_addr = pa->pa_function;
		if (sc->bge_chipid != BGE_CHIPID_BCM5717_A0) {
			phy_addr += (CSR_READ_4(sc, BGE_SGDIG_STS) &
			    BGE_SGDIGSTS_IS_SERDES) ? 8 : 1;
		} else {
			phy_addr += (CSR_READ_4(sc, BGE_CPMU_PHY_STRAP) &
			    BGE_CPMU_PHY_STRAP_IS_SERDES) ? 8 : 1;
		}
	}

	return phy_addr;
}

/*
 * Do endian, PCI and DMA initialization. Also check the on-board ROM
 * self-test results.
 */
static int
bge_chipinit(struct bge_softc *sc)
{
	uint32_t dma_rw_ctl, misc_ctl, mode_ctl, reg;
	int i;

	/* Set endianness before we access any non-PCI registers. */
	misc_ctl = BGE_INIT;
	if (sc->bge_flags & BGEF_TAGGED_STATUS)
		misc_ctl |= BGE_PCIMISCCTL_TAGGED_STATUS;
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_MISC_CTL,
	    misc_ctl);

	/*
	 * Clear the MAC statistics block in the NIC's
	 * internal memory.
	 */
	for (i = BGE_STATS_BLOCK;
	    i < BGE_STATS_BLOCK_END + 1; i += sizeof(uint32_t))
		BGE_MEMWIN_WRITE(sc->sc_pc, sc->sc_pcitag, i, 0);

	for (i = BGE_STATUS_BLOCK;
	    i < BGE_STATUS_BLOCK_END + 1; i += sizeof(uint32_t))
		BGE_MEMWIN_WRITE(sc->sc_pc, sc->sc_pcitag, i, 0);

	/* 5717 workaround from tg3 */
	if (sc->bge_chipid == BGE_CHIPID_BCM5717_A0) {
		/* Save */
		mode_ctl = CSR_READ_4(sc, BGE_MODE_CTL);

		/* Temporary modify MODE_CTL to control TLP */
		reg = mode_ctl & ~BGE_MODECTL_PCIE_TLPADDRMASK;
		CSR_WRITE_4(sc, BGE_MODE_CTL, reg | BGE_MODECTL_PCIE_TLPADDR1);

		/* Control TLP */
		reg = CSR_READ_4(sc, BGE_TLP_CONTROL_REG +
		    BGE_TLP_PHYCTL1);
		CSR_WRITE_4(sc, BGE_TLP_CONTROL_REG + BGE_TLP_PHYCTL1,
		    reg | BGE_TLP_PHYCTL1_EN_L1PLLPD);

		/* Restore */
		CSR_WRITE_4(sc, BGE_MODE_CTL, mode_ctl);
	}
	
	if (BGE_IS_57765_FAMILY(sc)) {
		if (sc->bge_chipid == BGE_CHIPID_BCM57765_A0) {
			/* Save */
			mode_ctl = CSR_READ_4(sc, BGE_MODE_CTL);

			/* Temporary modify MODE_CTL to control TLP */
			reg = mode_ctl & ~BGE_MODECTL_PCIE_TLPADDRMASK;
			CSR_WRITE_4(sc, BGE_MODE_CTL,
			    reg | BGE_MODECTL_PCIE_TLPADDR1);
		
			/* Control TLP */
			reg = CSR_READ_4(sc, BGE_TLP_CONTROL_REG +
			    BGE_TLP_PHYCTL5);
			CSR_WRITE_4(sc, BGE_TLP_CONTROL_REG + BGE_TLP_PHYCTL5,
			    reg | BGE_TLP_PHYCTL5_DIS_L2CLKREQ);

			/* Restore */
			CSR_WRITE_4(sc, BGE_MODE_CTL, mode_ctl);
		}
		if (BGE_CHIPREV(sc->bge_chipid) != BGE_CHIPREV_57765_AX) {
			reg = CSR_READ_4(sc, BGE_CPMU_PADRNG_CTL);
			CSR_WRITE_4(sc, BGE_CPMU_PADRNG_CTL,
			    reg | BGE_CPMU_PADRNG_CTL_RDIV2);

			/* Save */
			mode_ctl = CSR_READ_4(sc, BGE_MODE_CTL);

			/* Temporary modify MODE_CTL to control TLP */
			reg = mode_ctl & ~BGE_MODECTL_PCIE_TLPADDRMASK;
			CSR_WRITE_4(sc, BGE_MODE_CTL,
			    reg | BGE_MODECTL_PCIE_TLPADDR0);

			/* Control TLP */
			reg = CSR_READ_4(sc, BGE_TLP_CONTROL_REG +
			    BGE_TLP_FTSMAX);
			reg &= ~BGE_TLP_FTSMAX_MSK;
			CSR_WRITE_4(sc, BGE_TLP_CONTROL_REG + BGE_TLP_FTSMAX,
			    reg | BGE_TLP_FTSMAX_VAL);

			/* Restore */
			CSR_WRITE_4(sc, BGE_MODE_CTL, mode_ctl);
		}

		reg = CSR_READ_4(sc, BGE_CPMU_LSPD_10MB_CLK);
		reg &= ~BGE_CPMU_LSPD_10MB_MACCLK_MASK;
		reg |= BGE_CPMU_LSPD_10MB_MACCLK_6_25;
		CSR_WRITE_4(sc, BGE_CPMU_LSPD_10MB_CLK, reg);
	}

	/* Set up the PCI DMA control register. */
	dma_rw_ctl = BGE_PCI_READ_CMD | BGE_PCI_WRITE_CMD;
	if (sc->bge_flags & BGEF_PCIE) {
		/* Read watermark not used, 128 bytes for write. */
		DPRINTFN(4, ("(%s: PCI-Express DMA setting)\n",
		    device_xname(sc->bge_dev)));
		if (sc->bge_mps >= 256)
			dma_rw_ctl |= BGE_PCIDMARWCTL_WR_WAT_SHIFT(7);
		else
			dma_rw_ctl |= BGE_PCIDMARWCTL_WR_WAT_SHIFT(3);
	} else if (sc->bge_flags & BGEF_PCIX) {
	  	DPRINTFN(4, ("(:%s: PCI-X DMA setting)\n",
		    device_xname(sc->bge_dev)));
		/* PCI-X bus */
		if (BGE_IS_5714_FAMILY(sc)) {
			/* 256 bytes for read and write. */
			dma_rw_ctl |= BGE_PCIDMARWCTL_RD_WAT_SHIFT(2) |
			    BGE_PCIDMARWCTL_WR_WAT_SHIFT(2);

			if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5780)
				dma_rw_ctl |= BGE_PCIDMARWCTL_ONEDMA_ATONCE_GLOBAL;
			else
				dma_rw_ctl |= BGE_PCIDMARWCTL_ONEDMA_ATONCE_LOCAL;
		} else if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5703) {
			/*
			 * In the BCM5703, the DMA read watermark should
			 * be set to less than or equal to the maximum
			 * memory read byte count of the PCI-X command
			 * register.
			 */
			dma_rw_ctl |= BGE_PCIDMARWCTL_RD_WAT_SHIFT(4) |
			    BGE_PCIDMARWCTL_WR_WAT_SHIFT(3);
		} else if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704) {
			/* 1536 bytes for read, 384 bytes for write. */
			dma_rw_ctl |= BGE_PCIDMARWCTL_RD_WAT_SHIFT(7) |
			    BGE_PCIDMARWCTL_WR_WAT_SHIFT(3);
		} else {
			/* 384 bytes for read and write. */
			dma_rw_ctl |= BGE_PCIDMARWCTL_RD_WAT_SHIFT(3) |
			    BGE_PCIDMARWCTL_WR_WAT_SHIFT(3) |
			    (0x0F);
		}

		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5703 ||
		    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704) {
			uint32_t tmp;

			/* Set ONEDMA_ATONCE for hardware workaround. */
			tmp = CSR_READ_4(sc, BGE_PCI_CLKCTL) & 0x1f;
			if (tmp == 6 || tmp == 7)
				dma_rw_ctl |=
				    BGE_PCIDMARWCTL_ONEDMA_ATONCE_GLOBAL;

			/* Set PCI-X DMA write workaround. */
			dma_rw_ctl |= BGE_PCIDMARWCTL_ASRT_ALL_BE;
		}
	} else {
		/* Conventional PCI bus: 256 bytes for read and write. */
	  	DPRINTFN(4, ("(%s: PCI 2.2 DMA setting)\n",
		    device_xname(sc->bge_dev)));
		dma_rw_ctl |= BGE_PCIDMARWCTL_RD_WAT_SHIFT(7) |
		    BGE_PCIDMARWCTL_WR_WAT_SHIFT(7);

		if (BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5705 &&
		    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5750)
			dma_rw_ctl |= 0x0F;
	}

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5701)
		dma_rw_ctl |= BGE_PCIDMARWCTL_USE_MRM |
		    BGE_PCIDMARWCTL_ASRT_ALL_BE;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5703 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704)
		dma_rw_ctl &= ~BGE_PCIDMARWCTL_MINDMA;

	if (BGE_IS_57765_PLUS(sc)) {
		dma_rw_ctl &= ~BGE_PCIDMARWCTL_DIS_CACHE_ALIGNMENT;
		if (sc->bge_chipid == BGE_CHIPID_BCM57765_A0)
			dma_rw_ctl &= ~BGE_PCIDMARWCTL_CRDRDR_RDMA_MRRS_MSK;

		/*
		 * Enable HW workaround for controllers that misinterpret
		 * a status tag update and leave interrupts permanently
		 * disabled.
		 */
		if (!BGE_IS_57765_FAMILY(sc) &&
		    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5717)
			dma_rw_ctl |= BGE_PCIDMARWCTL_TAGGED_STATUS_WA;
	}

	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_DMA_RW_CTL,
	    dma_rw_ctl);

	/*
	 * Set up general mode register.
	 */
	mode_ctl = BGE_DMA_SWAP_OPTIONS;
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5720) {
		/* Retain Host-2-BMC settings written by APE firmware. */
		mode_ctl |= CSR_READ_4(sc, BGE_MODE_CTL) &
		    (BGE_MODECTL_BYTESWAP_B2HRX_DATA |
		    BGE_MODECTL_WORDSWAP_B2HRX_DATA |
		    BGE_MODECTL_B2HRX_ENABLE | BGE_MODECTL_HTX2B_ENABLE);
	}
	mode_ctl |= BGE_MODECTL_MAC_ATTN_INTR | BGE_MODECTL_HOST_SEND_BDS |
	    BGE_MODECTL_TX_NO_PHDR_CSUM;

	/*
	 * BCM5701 B5 have a bug causing data corruption when using
	 * 64-bit DMA reads, which can be terminated early and then
	 * completed later as 32-bit accesses, in combination with
	 * certain bridges.
	 */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5701 &&
	    sc->bge_chipid == BGE_CHIPID_BCM5701_B5)
		mode_ctl |= BGE_MODECTL_FORCE_PCI32;

	/*
	 * Tell the firmware the driver is running
	 */
	if (sc->bge_asf_mode & ASF_STACKUP)
		mode_ctl |= BGE_MODECTL_STACKUP;

	CSR_WRITE_4(sc, BGE_MODE_CTL, mode_ctl);

	/*
	 * Disable memory write invalidate.  Apparently it is not supported
	 * properly by these devices.
	 */
	PCI_CLRBIT(sc->sc_pc, sc->sc_pcitag, PCI_COMMAND_STATUS_REG,
		   PCI_COMMAND_INVALIDATE_ENABLE);

#ifdef __brokenalpha__
	/*
	 * Must insure that we do not cross an 8K (bytes) boundary
	 * for DMA reads.  Our highest limit is 1K bytes.  This is a
	 * restriction on some ALPHA platforms with early revision
	 * 21174 PCI chipsets, such as the AlphaPC 164lx
	 */
	PCI_SETBIT(sc, BGE_PCI_DMA_RW_CTL, BGE_PCI_READ_BNDRY_1024, 4);
#endif

	/* Set the timer prescaler (always 66MHz) */
	CSR_WRITE_4(sc, BGE_MISC_CFG, BGE_32BITTIME_66MHZ);

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906) {
		DELAY(40);	/* XXX */

		/* Put PHY into ready state */
		BGE_CLRBIT_FLUSH(sc, BGE_MISC_CFG, BGE_MISCCFG_EPHY_IDDQ);
		DELAY(40);
	}

	return 0;
}

static int
bge_blockinit(struct bge_softc *sc)
{
	volatile struct bge_rcb	 *rcb;
	bus_size_t rcb_addr;
	struct ifnet *ifp = &sc->ethercom.ec_if;
	bge_hostaddr taddr;
	uint32_t	dmactl, mimode, val;
	int		i, limit;

	/*
	 * Initialize the memory window pointer register so that
	 * we can access the first 32K of internal NIC RAM. This will
	 * allow us to set up the TX send ring RCBs and the RX return
	 * ring RCBs, plus other things which live in NIC memory.
	 */
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_MEMWIN_BASEADDR, 0);

	if (!BGE_IS_5705_PLUS(sc)) {
		/* 57XX step 33 */
		/* Configure mbuf memory pool */
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_BASEADDR,
		    BGE_BUFFPOOL_1);

		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704)
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_LEN, 0x10000);
		else
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_LEN, 0x18000);

		/* 57XX step 34 */
		/* Configure DMA resource pool */
		CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_BASEADDR,
		    BGE_DMA_DESCRIPTORS);
		CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_LEN, 0x2000);
	}

	/* 5718 step 11, 57XX step 35 */
	/*
	 * Configure mbuf pool watermarks. New broadcom docs strongly
	 * recommend these.
	 */
	if (BGE_IS_5717_PLUS(sc)) {
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_READDMA_LOWAT, 0x0);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x2a);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_HIWAT, 0xa0);
	} else if (BGE_IS_5705_PLUS(sc)) {
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_READDMA_LOWAT, 0x0);

		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906) {
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x04);
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_HIWAT, 0x10);
		} else {
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x10);
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_HIWAT, 0x60);
		}
	} else {
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_READDMA_LOWAT, 0x50);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x20);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_HIWAT, 0x60);
	}

	/* 57XX step 36 */
	/* Configure DMA resource watermarks */
	CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_LOWAT, 5);
	CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_HIWAT, 10);

	/* 5718 step 13, 57XX step 38 */
	/* Enable buffer manager */
	val = BGE_BMANMODE_ENABLE | BGE_BMANMODE_ATTN;
	/*
	 * Change the arbitration algorithm of TXMBUF read request to
	 * round-robin instead of priority based for BCM5719.  When
	 * TXFIFO is almost empty, RDMA will hold its request until
	 * TXFIFO is not almost empty.
	 */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5719)
		val |= BGE_BMANMODE_NO_TX_UNDERRUN;
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5717 ||
		sc->bge_chipid == BGE_CHIPID_BCM5719_A0 ||
		sc->bge_chipid == BGE_CHIPID_BCM5720_A0)
		val |= BGE_BMANMODE_LOMBUF_ATTN;
	CSR_WRITE_4(sc, BGE_BMAN_MODE, val);

	/* 57XX step 39 */
	/* Poll for buffer manager start indication */
	for (i = 0; i < BGE_TIMEOUT * 2; i++) {
		DELAY(10);
		if (CSR_READ_4(sc, BGE_BMAN_MODE) & BGE_BMANMODE_ENABLE)
			break;
	}

	if (i == BGE_TIMEOUT * 2) {
		aprint_error_dev(sc->bge_dev,
		    "buffer manager failed to start\n");
		return ENXIO;
	}

	/* 57XX step 40 */
	/* Enable flow-through queues */
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0xFFFFFFFF);
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0);

	/* Wait until queue initialization is complete */
	for (i = 0; i < BGE_TIMEOUT * 2; i++) {
		if (CSR_READ_4(sc, BGE_FTQ_RESET) == 0)
			break;
		DELAY(10);
	}

	if (i == BGE_TIMEOUT * 2) {
		aprint_error_dev(sc->bge_dev,
		    "flow-through queue init failed\n");
		return ENXIO;
	}

	/*
	 * Summary of rings supported by the controller:
	 *
	 * Standard Receive Producer Ring
	 * - This ring is used to feed receive buffers for "standard"
	 *   sized frames (typically 1536 bytes) to the controller.
	 *
	 * Jumbo Receive Producer Ring
	 * - This ring is used to feed receive buffers for jumbo sized
	 *   frames (i.e. anything bigger than the "standard" frames)
	 *   to the controller.
	 *
	 * Mini Receive Producer Ring
	 * - This ring is used to feed receive buffers for "mini"
	 *   sized frames to the controller.
	 * - This feature required external memory for the controller
	 *   but was never used in a production system.  Should always
	 *   be disabled.
	 *
	 * Receive Return Ring
	 * - After the controller has placed an incoming frame into a
	 *   receive buffer that buffer is moved into a receive return
	 *   ring.  The driver is then responsible to passing the
	 *   buffer up to the stack.  Many versions of the controller
	 *   support multiple RR rings.
	 *
	 * Send Ring
	 * - This ring is used for outgoing frames.  Many versions of
	 *   the controller support multiple send rings.
	 */

	/* 5718 step 15, 57XX step 41 */
	/* Initialize the standard RX ring control block */
	rcb = &sc->bge_rdata->bge_info.bge_std_rx_rcb;
	BGE_HOSTADDR(rcb->bge_hostaddr, BGE_RING_DMA_ADDR(sc, bge_rx_std_ring));
	/* 5718 step 16 */
	if (BGE_IS_57765_PLUS(sc)) {
		/*
		 * Bits 31-16: Programmable ring size (2048, 1024, 512, .., 32)
		 * Bits 15-2 : Maximum RX frame size
		 * Bit 1     : 1 = Ring Disabled, 0 = Ring ENabled
		 * Bit 0     : Reserved
		 */
		rcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(512, BGE_MAX_FRAMELEN << 2);
	} else if (BGE_IS_5705_PLUS(sc)) {
		/*
		 * Bits 31-16: Programmable ring size (512, 256, 128, 64, 32)
		 * Bits 15-2 : Reserved (should be 0)
		 * Bit 1     : 1 = Ring Disabled, 0 = Ring Enabled
		 * Bit 0     : Reserved
		 */
		rcb->bge_maxlen_flags = BGE_RCB_MAXLEN_FLAGS(512, 0);
	} else {
		/*
		 * Ring size is always XXX entries
		 * Bits 31-16: Maximum RX frame size
		 * Bits 15-2 : Reserved (should be 0)
		 * Bit 1     : 1 = Ring Disabled, 0 = Ring Enabled
		 * Bit 0     : Reserved
		 */
		rcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(BGE_MAX_FRAMELEN, 0);
	}
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5717 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5719 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5720)
		rcb->bge_nicaddr = BGE_STD_RX_RINGS_5717;
	else
		rcb->bge_nicaddr = BGE_STD_RX_RINGS;
	/* Write the standard receive producer ring control block. */
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_HADDR_HI, rcb->bge_hostaddr.bge_addr_hi);
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_HADDR_LO, rcb->bge_hostaddr.bge_addr_lo);
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_MAXLEN_FLAGS, rcb->bge_maxlen_flags);
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_NICADDR, rcb->bge_nicaddr);

	/* Reset the standard receive producer ring producer index. */
	bge_writembx(sc, BGE_MBX_RX_STD_PROD_LO, 0);

	/* 57XX step 42 */
	/*
	 * Initialize the jumbo RX ring control block
	 * We set the 'ring disabled' bit in the flags
	 * field until we're actually ready to start
	 * using this ring (i.e. once we set the MTU
	 * high enough to require it).
	 */
	if (BGE_IS_JUMBO_CAPABLE(sc)) {
		rcb = &sc->bge_rdata->bge_info.bge_jumbo_rx_rcb;
		BGE_HOSTADDR(rcb->bge_hostaddr,
		    BGE_RING_DMA_ADDR(sc, bge_rx_jumbo_ring));
		rcb->bge_maxlen_flags = BGE_RCB_MAXLEN_FLAGS(0,
		    BGE_RCB_FLAG_USE_EXT_RX_BD | BGE_RCB_FLAG_RING_DISABLED);
		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5717 ||
		    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5719 ||
		    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5720)
			rcb->bge_nicaddr = BGE_JUMBO_RX_RINGS_5717;
		else
			rcb->bge_nicaddr = BGE_JUMBO_RX_RINGS;
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_HADDR_HI,
		    rcb->bge_hostaddr.bge_addr_hi);
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_HADDR_LO,
		    rcb->bge_hostaddr.bge_addr_lo);
		/* Program the jumbo receive producer ring RCB parameters. */
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_MAXLEN_FLAGS,
		    rcb->bge_maxlen_flags);
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_NICADDR, rcb->bge_nicaddr);
		/* Reset the jumbo receive producer ring producer index. */
		bge_writembx(sc, BGE_MBX_RX_JUMBO_PROD_LO, 0);
	}

	/* 57XX step 43 */
	/* Disable the mini receive producer ring RCB. */
	if (BGE_IS_5700_FAMILY(sc)) {
		/* Set up dummy disabled mini ring RCB */
		rcb = &sc->bge_rdata->bge_info.bge_mini_rx_rcb;
		rcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(0, BGE_RCB_FLAG_RING_DISABLED);
		CSR_WRITE_4(sc, BGE_RX_MINI_RCB_MAXLEN_FLAGS,
		    rcb->bge_maxlen_flags);
		/* Reset the mini receive producer ring producer index. */
		bge_writembx(sc, BGE_MBX_RX_MINI_PROD_LO, 0);

		bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
		    offsetof(struct bge_ring_data, bge_info),
		    sizeof (struct bge_gib),
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	}

	/* Choose de-pipeline mode for BCM5906 A0, A1 and A2. */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906) {
		if (sc->bge_chipid == BGE_CHIPID_BCM5906_A0 ||
		    sc->bge_chipid == BGE_CHIPID_BCM5906_A1 ||
		    sc->bge_chipid == BGE_CHIPID_BCM5906_A2)
			CSR_WRITE_4(sc, BGE_ISO_PKT_TX,
			    (CSR_READ_4(sc, BGE_ISO_PKT_TX) & ~3) | 2);
	}
	/* 5718 step 14, 57XX step 44 */
	/*
	 * The BD ring replenish thresholds control how often the
	 * hardware fetches new BD's from the producer rings in host
	 * memory.  Setting the value too low on a busy system can
	 * starve the hardware and recue the throughpout.
	 *
	 * Set the BD ring replenish thresholds. The recommended
	 * values are 1/8th the number of descriptors allocated to
	 * each ring, but since we try to avoid filling the entire
	 * ring we set these to the minimal value of 8.  This needs to
	 * be done on several of the supported chip revisions anyway,
	 * to work around HW bugs.
	 */
	CSR_WRITE_4(sc, BGE_RBDI_STD_REPL_THRESH, 8);
	if (BGE_IS_JUMBO_CAPABLE(sc))
		CSR_WRITE_4(sc, BGE_RBDI_JUMBO_REPL_THRESH, 8);

	/* 5718 step 18 */
	if (BGE_IS_5717_PLUS(sc)) {
		CSR_WRITE_4(sc, BGE_STD_REPL_LWM, 4);
		CSR_WRITE_4(sc, BGE_JUMBO_REPL_LWM, 4);
	}

	/* 57XX step 45 */
	/*
	 * Disable all send rings by setting the 'ring disabled' bit
	 * in the flags field of all the TX send ring control blocks,
	 * located in NIC memory.
	 */
	if (BGE_IS_5700_FAMILY(sc)) {
		/* 5700 to 5704 had 16 send rings. */
		limit = BGE_TX_RINGS_EXTSSRAM_MAX;
	} else if (BGE_IS_5717_PLUS(sc)) {
		limit = BGE_TX_RINGS_5717_MAX;
	} else if (BGE_IS_57765_FAMILY(sc)) {
		limit = BGE_TX_RINGS_57765_MAX;
	} else
		limit = 1;
	rcb_addr = BGE_MEMWIN_START + BGE_SEND_RING_RCB;
	for (i = 0; i < limit; i++) {
		RCB_WRITE_4(sc, rcb_addr, bge_maxlen_flags,
		    BGE_RCB_MAXLEN_FLAGS(0, BGE_RCB_FLAG_RING_DISABLED));
		RCB_WRITE_4(sc, rcb_addr, bge_nicaddr, 0);
		rcb_addr += sizeof(struct bge_rcb);
	}

	/* 57XX step 46 and 47 */
	/* Configure send ring RCB 0 (we use only the first ring) */
	rcb_addr = BGE_MEMWIN_START + BGE_SEND_RING_RCB;
	BGE_HOSTADDR(taddr, BGE_RING_DMA_ADDR(sc, bge_tx_ring));
	RCB_WRITE_4(sc, rcb_addr, bge_hostaddr.bge_addr_hi, taddr.bge_addr_hi);
	RCB_WRITE_4(sc, rcb_addr, bge_hostaddr.bge_addr_lo, taddr.bge_addr_lo);
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5717 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5719 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5720)
		RCB_WRITE_4(sc, rcb_addr, bge_nicaddr, BGE_SEND_RING_5717);
	else
		RCB_WRITE_4(sc, rcb_addr, bge_nicaddr,
		    BGE_NIC_TXRING_ADDR(0, BGE_TX_RING_CNT));
	RCB_WRITE_4(sc, rcb_addr, bge_maxlen_flags,
	    BGE_RCB_MAXLEN_FLAGS(BGE_TX_RING_CNT, 0));

	/* 57XX step 48 */
	/*
	 * Disable all receive return rings by setting the
	 * 'ring diabled' bit in the flags field of all the receive
	 * return ring control blocks, located in NIC memory.
	 */
	if (BGE_IS_5717_PLUS(sc)) {
		/* Should be 17, use 16 until we get an SRAM map. */
		limit = 16;
	} else if (BGE_IS_5700_FAMILY(sc))
		limit = BGE_RX_RINGS_MAX;
	else if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5755 ||
	    BGE_IS_57765_FAMILY(sc))
		limit = 4;
	else
		limit = 1;
	/* Disable all receive return rings */
	rcb_addr = BGE_MEMWIN_START + BGE_RX_RETURN_RING_RCB;
	for (i = 0; i < limit; i++) {
		RCB_WRITE_4(sc, rcb_addr, bge_hostaddr.bge_addr_hi, 0);
		RCB_WRITE_4(sc, rcb_addr, bge_hostaddr.bge_addr_lo, 0);
		RCB_WRITE_4(sc, rcb_addr, bge_maxlen_flags,
		    BGE_RCB_MAXLEN_FLAGS(sc->bge_return_ring_cnt,
			BGE_RCB_FLAG_RING_DISABLED));
		RCB_WRITE_4(sc, rcb_addr, bge_nicaddr, 0);
		bge_writembx(sc, BGE_MBX_RX_CONS0_LO +
		    (i * (sizeof(uint64_t))), 0);
		rcb_addr += sizeof(struct bge_rcb);
	}

	/* 57XX step 49 */
	/*
	 * Set up receive return ring 0.  Note that the NIC address
	 * for RX return rings is 0x0.  The return rings live entirely
	 * within the host, so the nicaddr field in the RCB isn't used.
	 */
	rcb_addr = BGE_MEMWIN_START + BGE_RX_RETURN_RING_RCB;
	BGE_HOSTADDR(taddr, BGE_RING_DMA_ADDR(sc, bge_rx_return_ring));
	RCB_WRITE_4(sc, rcb_addr, bge_hostaddr.bge_addr_hi, taddr.bge_addr_hi);
	RCB_WRITE_4(sc, rcb_addr, bge_hostaddr.bge_addr_lo, taddr.bge_addr_lo);
	RCB_WRITE_4(sc, rcb_addr, bge_nicaddr, 0x00000000);
	RCB_WRITE_4(sc, rcb_addr, bge_maxlen_flags,
	    BGE_RCB_MAXLEN_FLAGS(sc->bge_return_ring_cnt, 0));

	/* 5718 step 24, 57XX step 53 */
	/* Set random backoff seed for TX */
	CSR_WRITE_4(sc, BGE_TX_RANDOM_BACKOFF,
	    (CLLADDR(ifp->if_sadl)[0] + CLLADDR(ifp->if_sadl)[1] +
		CLLADDR(ifp->if_sadl)[2] + CLLADDR(ifp->if_sadl)[3] +
		CLLADDR(ifp->if_sadl)[4] + CLLADDR(ifp->if_sadl)[5]) &
	    BGE_TX_BACKOFF_SEED_MASK);

	/* 5718 step 26, 57XX step 55 */
	/* Set inter-packet gap */
	val = 0x2620;
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5720)
		val |= CSR_READ_4(sc, BGE_TX_LENGTHS) &
		    (BGE_TXLEN_JMB_FRM_LEN_MSK | BGE_TXLEN_CNT_DN_VAL_MSK);
	CSR_WRITE_4(sc, BGE_TX_LENGTHS, val);

	/* 5718 step 27, 57XX step 56 */
	/*
	 * Specify which ring to use for packets that don't match
	 * any RX rules.
	 */
	CSR_WRITE_4(sc, BGE_RX_RULES_CFG, 0x08);

	/* 5718 step 28, 57XX step 57 */
	/*
	 * Configure number of RX lists. One interrupt distribution
	 * list, sixteen active lists, one bad frames class.
	 */
	CSR_WRITE_4(sc, BGE_RXLP_CFG, 0x181);

	/* 5718 step 29, 57XX step 58 */
	/* Inialize RX list placement stats mask. */
	if (BGE_IS_575X_PLUS(sc)) {
		val = CSR_READ_4(sc, BGE_RXLP_STATS_ENABLE_MASK);
		val &= ~BGE_RXLPSTATCONTROL_DACK_FIX;
		CSR_WRITE_4(sc, BGE_RXLP_STATS_ENABLE_MASK, val);
	} else
		CSR_WRITE_4(sc, BGE_RXLP_STATS_ENABLE_MASK, 0x007FFFFF);

	/* 5718 step 30, 57XX step 59 */
	CSR_WRITE_4(sc, BGE_RXLP_STATS_CTL, 0x1);

	/* 5718 step 33, 57XX step 62 */
	/* Disable host coalescing until we get it set up */
	CSR_WRITE_4(sc, BGE_HCC_MODE, 0x00000000);

	/* 5718 step 34, 57XX step 63 */
	/* Poll to make sure it's shut down. */
	for (i = 0; i < BGE_TIMEOUT * 2; i++) {
		DELAY(10);
		if (!(CSR_READ_4(sc, BGE_HCC_MODE) & BGE_HCCMODE_ENABLE))
			break;
	}

	if (i == BGE_TIMEOUT * 2) {
		aprint_error_dev(sc->bge_dev,
		    "host coalescing engine failed to idle\n");
		return ENXIO;
	}

	/* 5718 step 35, 36, 37 */
	/* Set up host coalescing defaults */
	CSR_WRITE_4(sc, BGE_HCC_RX_COAL_TICKS, sc->bge_rx_coal_ticks);
	CSR_WRITE_4(sc, BGE_HCC_TX_COAL_TICKS, sc->bge_tx_coal_ticks);
	CSR_WRITE_4(sc, BGE_HCC_RX_MAX_COAL_BDS, sc->bge_rx_max_coal_bds);
	CSR_WRITE_4(sc, BGE_HCC_TX_MAX_COAL_BDS, sc->bge_tx_max_coal_bds);
	if (!(BGE_IS_5705_PLUS(sc))) {
		CSR_WRITE_4(sc, BGE_HCC_RX_COAL_TICKS_INT, 0);
		CSR_WRITE_4(sc, BGE_HCC_TX_COAL_TICKS_INT, 0);
	}
	CSR_WRITE_4(sc, BGE_HCC_RX_MAX_COAL_BDS_INT, 0);
	CSR_WRITE_4(sc, BGE_HCC_TX_MAX_COAL_BDS_INT, 0);

	/* Set up address of statistics block */
	if (BGE_IS_5700_FAMILY(sc)) {
		BGE_HOSTADDR(taddr, BGE_RING_DMA_ADDR(sc, bge_info.bge_stats));
		CSR_WRITE_4(sc, BGE_HCC_STATS_TICKS, sc->bge_stat_ticks);
		CSR_WRITE_4(sc, BGE_HCC_STATS_BASEADDR, BGE_STATS_BLOCK);
		CSR_WRITE_4(sc, BGE_HCC_STATS_ADDR_HI, taddr.bge_addr_hi);
		CSR_WRITE_4(sc, BGE_HCC_STATS_ADDR_LO, taddr.bge_addr_lo);
	}

	/* 5718 step 38 */
	/* Set up address of status block */
	BGE_HOSTADDR(taddr, BGE_RING_DMA_ADDR(sc, bge_status_block));
	CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_BASEADDR, BGE_STATUS_BLOCK);
	CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_ADDR_HI, taddr.bge_addr_hi);
	CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_ADDR_LO, taddr.bge_addr_lo);
	sc->bge_rdata->bge_status_block.bge_idx[0].bge_rx_prod_idx = 0;
	sc->bge_rdata->bge_status_block.bge_idx[0].bge_tx_cons_idx = 0;

	/* Set up status block size. */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700 &&
	    sc->bge_chipid != BGE_CHIPID_BCM5700_C0) {
		val = BGE_STATBLKSZ_FULL;
		bzero(&sc->bge_rdata->bge_status_block, BGE_STATUS_BLK_SZ);
	} else {
		val = BGE_STATBLKSZ_32BYTE;
		bzero(&sc->bge_rdata->bge_status_block, 32);
	}

	/* 5718 step 39, 57XX step 73 */
	/* Turn on host coalescing state machine */
	CSR_WRITE_4(sc, BGE_HCC_MODE, val | BGE_HCCMODE_ENABLE);

	/* 5718 step 40, 57XX step 74 */
	/* Turn on RX BD completion state machine and enable attentions */
	CSR_WRITE_4(sc, BGE_RBDC_MODE,
	    BGE_RBDCMODE_ENABLE | BGE_RBDCMODE_ATTN);

	/* 5718 step 41, 57XX step 75 */
	/* Turn on RX list placement state machine */
	CSR_WRITE_4(sc, BGE_RXLP_MODE, BGE_RXLPMODE_ENABLE);

	/* 57XX step 76 */
	/* Turn on RX list selector state machine. */
	if (!(BGE_IS_5705_PLUS(sc)))
		CSR_WRITE_4(sc, BGE_RXLS_MODE, BGE_RXLSMODE_ENABLE);

	val = BGE_MACMODE_TXDMA_ENB | BGE_MACMODE_RXDMA_ENB |
	    BGE_MACMODE_RX_STATS_CLEAR | BGE_MACMODE_TX_STATS_CLEAR |
	    BGE_MACMODE_RX_STATS_ENB | BGE_MACMODE_TX_STATS_ENB |
	    BGE_MACMODE_FRMHDR_DMA_ENB;

	if (sc->bge_flags & BGEF_FIBER_TBI)
		val |= BGE_PORTMODE_TBI;
	else if (sc->bge_flags & BGEF_FIBER_MII)
		val |= BGE_PORTMODE_GMII;
	else
		val |= BGE_PORTMODE_MII;

	/* 5718 step 42 and 43, 57XX step 77 and 78 */
	/* Allow APE to send/receive frames. */
	if ((sc->bge_mfw_flags & BGE_MFW_ON_APE) != 0)
		val |= BGE_MACMODE_APE_RX_EN | BGE_MACMODE_APE_TX_EN;

	/* Turn on DMA, clear stats */
	CSR_WRITE_4_FLUSH(sc, BGE_MAC_MODE, val);
	/* 5718 step 44 */
	DELAY(40);

	/* 5718 step 45, 57XX step 79 */
	/* Set misc. local control, enable interrupts on attentions */
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_INTR_ONATTN);
	if (BGE_IS_5717_PLUS(sc)) {
		CSR_READ_4(sc, BGE_MISC_LOCAL_CTL); /* Flush */
		/* 5718 step 46 */
		DELAY(100);
	}

	/* 57XX step 81 */
	/* Turn on DMA completion state machine */
	if (!(BGE_IS_5705_PLUS(sc)))
		CSR_WRITE_4(sc, BGE_DMAC_MODE, BGE_DMACMODE_ENABLE);

	/* 5718 step 47, 57XX step 82 */
	val = BGE_WDMAMODE_ENABLE | BGE_WDMAMODE_ALL_ATTNS;

	/* 5718 step 48 */
	/* Enable host coalescing bug fix. */
	if (BGE_IS_5755_PLUS(sc))
		val |= BGE_WDMAMODE_STATUS_TAG_FIX;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5785)
		val |= BGE_WDMAMODE_BURST_ALL_DATA;

	/* Turn on write DMA state machine */
	CSR_WRITE_4_FLUSH(sc, BGE_WDMA_MODE, val);
	/* 5718 step 49 */
	DELAY(40);

	val = BGE_RDMAMODE_ENABLE | BGE_RDMAMODE_ALL_ATTNS;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5717)
		val |= BGE_RDMAMODE_MULT_DMA_RD_DIS;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5784 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5785 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM57780)
		val |= BGE_RDMAMODE_BD_SBD_CRPT_ATTN |
		    BGE_RDMAMODE_MBUF_RBD_CRPT_ATTN |
		    BGE_RDMAMODE_MBUF_SBD_CRPT_ATTN;

	if (sc->bge_flags & BGEF_PCIE)
		val |= BGE_RDMAMODE_FIFO_LONG_BURST;
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM57766) {
		if (ifp->if_mtu <= ETHERMTU)
			val |= BGE_RDMAMODE_JMB_2K_MMRR;
	}
	if (sc->bge_flags & BGEF_TSO)
		val |= BGE_RDMAMODE_TSO4_ENABLE;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5720) {
		val |= CSR_READ_4(sc, BGE_RDMA_MODE) &
		    BGE_RDMAMODE_H2BNC_VLAN_DET;
		/*
		 * Allow multiple outstanding read requests from
		 * non-LSO read DMA engine.
		 */
		val &= ~BGE_RDMAMODE_MULT_DMA_RD_DIS;
	}

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5761 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5784 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5785 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM57780 ||
	    BGE_IS_57765_PLUS(sc)) {
		dmactl = CSR_READ_4(sc, BGE_RDMA_RSRVCTRL);
		/*
		 * Adjust tx margin to prevent TX data corruption and
		 * fix internal FIFO overflow.
		 */
		if (sc->bge_chipid == BGE_CHIPID_BCM5719_A0) {
			dmactl &= ~(BGE_RDMA_RSRVCTRL_FIFO_LWM_MASK |
			    BGE_RDMA_RSRVCTRL_FIFO_HWM_MASK |
			    BGE_RDMA_RSRVCTRL_TXMRGN_MASK);
			dmactl |= BGE_RDMA_RSRVCTRL_FIFO_LWM_1_5K |
			    BGE_RDMA_RSRVCTRL_FIFO_HWM_1_5K |
			    BGE_RDMA_RSRVCTRL_TXMRGN_320B;
		}
		/*
		 * Enable fix for read DMA FIFO overruns.
		 * The fix is to limit the number of RX BDs
		 * the hardware would fetch at a fime.
		 */
		CSR_WRITE_4(sc, BGE_RDMA_RSRVCTRL, dmactl |
		    BGE_RDMA_RSRVCTRL_FIFO_OFLW_FIX);
	}

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5719) {
		CSR_WRITE_4(sc, BGE_RDMA_LSO_CRPTEN_CTRL,
		    CSR_READ_4(sc, BGE_RDMA_LSO_CRPTEN_CTRL) |
		    BGE_RDMA_LSO_CRPTEN_CTRL_BLEN_BD_4K |
		    BGE_RDMA_LSO_CRPTEN_CTRL_BLEN_LSO_4K);
	} else if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5720) {
		/*
		 * Allow 4KB burst length reads for non-LSO frames.
		 * Enable 512B burst length reads for buffer descriptors.
		 */
		CSR_WRITE_4(sc, BGE_RDMA_LSO_CRPTEN_CTRL,
		    CSR_READ_4(sc, BGE_RDMA_LSO_CRPTEN_CTRL) |
		    BGE_RDMA_LSO_CRPTEN_CTRL_BLEN_BD_512 |
		    BGE_RDMA_LSO_CRPTEN_CTRL_BLEN_LSO_4K);
	}

	/* Turn on read DMA state machine */
	CSR_WRITE_4_FLUSH(sc, BGE_RDMA_MODE, val);
	/* 5718 step 52 */
	delay(40);

	/* 5718 step 56, 57XX step 84 */
	/* Turn on RX data completion state machine */
	CSR_WRITE_4(sc, BGE_RDC_MODE, BGE_RDCMODE_ENABLE);

	/* Turn on RX data and RX BD initiator state machine */
	CSR_WRITE_4(sc, BGE_RDBDI_MODE, BGE_RDBDIMODE_ENABLE);

	/* 57XX step 85 */
	/* Turn on Mbuf cluster free state machine */
	if (!BGE_IS_5705_PLUS(sc))
		CSR_WRITE_4(sc, BGE_MBCF_MODE, BGE_MBCFMODE_ENABLE);

	/* 5718 step 57, 57XX step 86 */
	/* Turn on send data completion state machine */
	val = BGE_SDCMODE_ENABLE;
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5761)
		val |= BGE_SDCMODE_CDELAY;
	CSR_WRITE_4(sc, BGE_SDC_MODE, val);

	/* 5718 step 58 */
	/* Turn on send BD completion state machine */
	CSR_WRITE_4(sc, BGE_SBDC_MODE, BGE_SBDCMODE_ENABLE);

	/* 57XX step 88 */
	/* Turn on RX BD initiator state machine */
	CSR_WRITE_4(sc, BGE_RBDI_MODE, BGE_RBDIMODE_ENABLE);

	/* 5718 step 60, 57XX step 90 */
	/* Turn on send data initiator state machine */
	if (sc->bge_flags & BGEF_TSO) {
		/* XXX: magic value from Linux driver */
		CSR_WRITE_4(sc, BGE_SDI_MODE, BGE_SDIMODE_ENABLE |
		    BGE_SDIMODE_HW_LSO_PRE_DMA);
	} else
		CSR_WRITE_4(sc, BGE_SDI_MODE, BGE_SDIMODE_ENABLE);

	/* 5718 step 61, 57XX step 91 */
	/* Turn on send BD initiator state machine */
	CSR_WRITE_4(sc, BGE_SBDI_MODE, BGE_SBDIMODE_ENABLE);

	/* 5718 step 62, 57XX step 92 */
	/* Turn on send BD selector state machine */
	CSR_WRITE_4(sc, BGE_SRS_MODE, BGE_SRSMODE_ENABLE);

	/* 5718 step 31, 57XX step 60 */
	CSR_WRITE_4(sc, BGE_SDI_STATS_ENABLE_MASK, 0x007FFFFF);
	/* 5718 step 32, 57XX step 61 */
	CSR_WRITE_4(sc, BGE_SDI_STATS_CTL,
	    BGE_SDISTATSCTL_ENABLE | BGE_SDISTATSCTL_FASTER);

	/* ack/clear link change events */
	CSR_WRITE_4(sc, BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED |
	    BGE_MACSTAT_CFG_CHANGED | BGE_MACSTAT_MI_COMPLETE |
	    BGE_MACSTAT_LINK_CHANGED);
	CSR_WRITE_4(sc, BGE_MI_STS, 0);

	/*
	 * Enable attention when the link has changed state for
	 * devices that use auto polling.
	 */
	if (sc->bge_flags & BGEF_FIBER_TBI) {
		CSR_WRITE_4(sc, BGE_MI_STS, BGE_MISTS_LINK);
	} else {
		if ((sc->bge_flags & BGEF_CPMU_PRESENT) != 0)
			mimode = BGE_MIMODE_500KHZ_CONST;
		else
			mimode = BGE_MIMODE_BASE;
		/* 5718 step 68. 5718 step 69 (optionally). */
		if (BGE_IS_5700_FAMILY(sc) ||
		    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5705) {
			mimode |= BGE_MIMODE_AUTOPOLL;
			BGE_STS_SETBIT(sc, BGE_STS_AUTOPOLL);
		}
		mimode |= BGE_MIMODE_PHYADDR(sc->bge_phy_addr);
		CSR_WRITE_4(sc, BGE_MI_MODE, mimode);
		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700)
			CSR_WRITE_4(sc, BGE_MAC_EVT_ENB,
			    BGE_EVTENB_MI_INTERRUPT);
	}

	/*
	 * Clear any pending link state attention.
	 * Otherwise some link state change events may be lost until attention
	 * is cleared by bge_intr() -> bge_link_upd() sequence.
	 * It's not necessary on newer BCM chips - perhaps enabling link
	 * state change attentions implies clearing pending attention.
	 */
	CSR_WRITE_4(sc, BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED |
	    BGE_MACSTAT_CFG_CHANGED | BGE_MACSTAT_MI_COMPLETE |
	    BGE_MACSTAT_LINK_CHANGED);

	/* Enable link state change attentions. */
	BGE_SETBIT(sc, BGE_MAC_EVT_ENB, BGE_EVTENB_LINK_CHANGED);

	return 0;
}

static const struct bge_revision *
bge_lookup_rev(uint32_t chipid)
{
	const struct bge_revision *br;

	for (br = bge_revisions; br->br_name != NULL; br++) {
		if (br->br_chipid == chipid)
			return br;
	}

	for (br = bge_majorrevs; br->br_name != NULL; br++) {
		if (br->br_chipid == BGE_ASICREV(chipid))
			return br;
	}

	return NULL;
}

static const struct bge_product *
bge_lookup(const struct pci_attach_args *pa)
{
	const struct bge_product *bp;

	for (bp = bge_products; bp->bp_name != NULL; bp++) {
		if (PCI_VENDOR(pa->pa_id) == bp->bp_vendor &&
		    PCI_PRODUCT(pa->pa_id) == bp->bp_product)
			return bp;
	}

	return NULL;
}

static uint32_t
bge_chipid(const struct pci_attach_args *pa)
{
	uint32_t id;

	id = pci_conf_read(pa->pa_pc, pa->pa_tag, BGE_PCI_MISC_CTL)
		>> BGE_PCIMISCCTL_ASICREV_SHIFT;

	if (BGE_ASICREV(id) == BGE_ASICREV_USE_PRODID_REG) {
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_BROADCOM_BCM5717:
		case PCI_PRODUCT_BROADCOM_BCM5718:
		case PCI_PRODUCT_BROADCOM_BCM5719:
		case PCI_PRODUCT_BROADCOM_BCM5720:
			id = pci_conf_read(pa->pa_pc, pa->pa_tag,
			    BGE_PCI_GEN2_PRODID_ASICREV);
			break;
		case PCI_PRODUCT_BROADCOM_BCM57761:
		case PCI_PRODUCT_BROADCOM_BCM57762:
		case PCI_PRODUCT_BROADCOM_BCM57765:
		case PCI_PRODUCT_BROADCOM_BCM57766:
		case PCI_PRODUCT_BROADCOM_BCM57781:
		case PCI_PRODUCT_BROADCOM_BCM57785:
		case PCI_PRODUCT_BROADCOM_BCM57791:
		case PCI_PRODUCT_BROADCOM_BCM57795:
			id = pci_conf_read(pa->pa_pc, pa->pa_tag,
			    BGE_PCI_GEN15_PRODID_ASICREV);
			break;
		default:
			id = pci_conf_read(pa->pa_pc, pa->pa_tag,
			    BGE_PCI_PRODID_ASICREV);
			break;
		}
	}

	return id;
}

#ifdef __HAVE_PCI_MSI_MSIX
/*
 * Return true if MSI can be used with this device.
 */
static int
bge_can_use_msi(struct bge_softc *sc)
{
	int can_use_msi = 0;

	switch (BGE_ASICREV(sc->bge_chipid)) {
	case BGE_ASICREV_BCM5714_A0:
	case BGE_ASICREV_BCM5714:
		/*
		 * Apparently, MSI doesn't work when these chips are
		 * configured in single-port mode.
		 */
		break;
	case BGE_ASICREV_BCM5750:
		if (BGE_CHIPREV(sc->bge_chipid) != BGE_CHIPREV_5750_AX &&
		    BGE_CHIPREV(sc->bge_chipid) != BGE_CHIPREV_5750_BX)
			can_use_msi = 1;
		break;
	default:
		if (BGE_IS_575X_PLUS(sc))
			can_use_msi = 1;
	}
	return (can_use_msi);
}
#endif

/*
 * Probe for a Broadcom chip. Check the PCI vendor and device IDs
 * against our list and return its name if we find a match. Note
 * that since the Broadcom controller contains VPD support, we
 * can get the device name string from the controller itself instead
 * of the compiled-in string. This is a little slow, but it guarantees
 * we'll always announce the right product name.
 */
static int
bge_probe(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (bge_lookup(pa) != NULL)
		return 1;

	return 0;
}

static void
bge_attach(device_t parent, device_t self, void *aux)
{
	struct bge_softc	*sc = device_private(self);
	struct pci_attach_args	*pa = aux;
	prop_dictionary_t dict;
	const struct bge_product *bp;
	const struct bge_revision *br;
	pci_chipset_tag_t	pc;
#ifndef __HAVE_PCI_MSI_MSIX
	pci_intr_handle_t	ih;
#else
	int counts[PCI_INTR_TYPE_SIZE];
	pci_intr_type_t intr_type, max_type;
#endif
	const char		*intrstr = NULL;
	uint32_t 		hwcfg, hwcfg2, hwcfg3, hwcfg4, hwcfg5;
	uint32_t		command;
	struct ifnet		*ifp;
	uint32_t		misccfg, mimode;
	void *			kva;
	u_char			eaddr[ETHER_ADDR_LEN];
	pcireg_t		memtype, subid, reg;
	bus_addr_t		memaddr;
	uint32_t		pm_ctl;
	bool			no_seeprom;
	int			capmask;
	int			mii_flags;
	int			map_flags;
	char intrbuf[PCI_INTRSTR_LEN];

	bp = bge_lookup(pa);
	KASSERT(bp != NULL);

	sc->sc_pc = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;
	sc->bge_dev = self;

	sc->bge_pa = *pa;
	pc = sc->sc_pc;
	subid = pci_conf_read(pc, sc->sc_pcitag, PCI_SUBSYS_ID_REG);

	aprint_naive(": Ethernet controller\n");
	aprint_normal(": %s\n", bp->bp_name);

	/*
	 * Map control/status registers.
	 */
	DPRINTFN(5, ("Map control/status regs\n"));
	command = pci_conf_read(pc, sc->sc_pcitag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, sc->sc_pcitag, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(pc, sc->sc_pcitag, PCI_COMMAND_STATUS_REG);

	if (!(command & PCI_COMMAND_MEM_ENABLE)) {
		aprint_error_dev(sc->bge_dev,
		    "failed to enable memory mapping!\n");
		return;
	}

	DPRINTFN(5, ("pci_mem_find\n"));
	memtype = pci_mapreg_type(sc->sc_pc, sc->sc_pcitag, BGE_PCI_BAR0);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
#if 0
		if (pci_mapreg_map(pa, BGE_PCI_BAR0,
		    memtype, 0, &sc->bge_btag, &sc->bge_bhandle,
		    &memaddr, &sc->bge_bsize) == 0)
			break;
#else
		/*
		 * Workaround for PCI prefetchable bit. Some BCM5717-5720 based
		 * system get NMI on boot (PR#48451). This problem might not be
		 * the driver's bug but our PCI common part's bug. Until we
		 * find a real reason, we ignore the prefetchable bit.
		 */
		if (pci_mapreg_info(pa->pa_pc, pa->pa_tag, BGE_PCI_BAR0,
		    memtype, &memaddr, &sc->bge_bsize, &map_flags) == 0) {
			map_flags &= ~BUS_SPACE_MAP_PREFETCHABLE;
			if (bus_space_map(pa->pa_memt, memaddr, sc->bge_bsize,
			    map_flags, &sc->bge_bhandle) == 0) {
				sc->bge_btag = pa->pa_memt;
				break;
			}
		}
#endif
	default:
		aprint_error_dev(sc->bge_dev, "can't find mem space\n");
		return;
	}

	/* Save various chip information. */
	sc->bge_chipid = bge_chipid(pa);
	sc->bge_phy_addr = bge_phy_addr(sc);

	if ((pci_get_capability(sc->sc_pc, sc->sc_pcitag, PCI_CAP_PCIEXPRESS,
	        &sc->bge_pciecap, NULL) != 0)
	    || (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5785)) {
		/* PCIe */
		sc->bge_flags |= BGEF_PCIE;
		/* Extract supported maximum payload size. */
		reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
		    sc->bge_pciecap + PCIE_DCAP);
		sc->bge_mps = 128 << (reg & PCIE_DCAP_MAX_PAYLOAD);
		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5719 ||
		    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5720)
			sc->bge_expmrq = 2048;
		else
			sc->bge_expmrq = 4096;
		bge_set_max_readrq(sc);
	} else if ((pci_conf_read(sc->sc_pc, sc->sc_pcitag, BGE_PCI_PCISTATE) &
		BGE_PCISTATE_PCI_BUSMODE) == 0) {
		/* PCI-X */
		sc->bge_flags |= BGEF_PCIX;
		if (pci_get_capability(pa->pa_pc, pa->pa_tag, PCI_CAP_PCIX,
			&sc->bge_pcixcap, NULL) == 0)
			aprint_error_dev(sc->bge_dev,
			    "unable to find PCIX capability\n");
	}

	if (BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5700_BX) {
		/*
		 * Kludge for 5700 Bx bug: a hardware bug (PCIX byte enable?)
		 * can clobber the chip's PCI config-space power control
		 * registers, leaving the card in D3 powersave state. We do
		 * not have memory-mapped registers in this state, so force
		 * device into D0 state before starting initialization.
		 */
		pm_ctl = pci_conf_read(pc, sc->sc_pcitag, BGE_PCI_PWRMGMT_CMD);
		pm_ctl &= ~(PCI_PWR_D0|PCI_PWR_D1|PCI_PWR_D2|PCI_PWR_D3);
		pm_ctl |= (1 << 8) | PCI_PWR_D0 ; /* D0 state */
		pci_conf_write(pc, sc->sc_pcitag, BGE_PCI_PWRMGMT_CMD, pm_ctl);
		DELAY(1000);	/* 27 usec is allegedly sufficent */
	}

	/* Save chipset family. */
	switch (BGE_ASICREV(sc->bge_chipid)) {
	case BGE_ASICREV_BCM5717:
	case BGE_ASICREV_BCM5719:
	case BGE_ASICREV_BCM5720:
		sc->bge_flags |= BGEF_5717_PLUS;
		/* FALLTHROUGH */
	case BGE_ASICREV_BCM57765:
	case BGE_ASICREV_BCM57766:
		if (!BGE_IS_5717_PLUS(sc))
			sc->bge_flags |= BGEF_57765_FAMILY;
		sc->bge_flags |= BGEF_57765_PLUS | BGEF_5755_PLUS |
		    BGEF_575X_PLUS | BGEF_5705_PLUS | BGEF_JUMBO_CAPABLE;
		/* Jumbo frame on BCM5719 A0 does not work. */
		if ((BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5719) &&
		    (sc->bge_chipid == BGE_CHIPID_BCM5719_A0))
			sc->bge_flags &= ~BGEF_JUMBO_CAPABLE;
		break;
	case BGE_ASICREV_BCM5755:
	case BGE_ASICREV_BCM5761:
	case BGE_ASICREV_BCM5784:
	case BGE_ASICREV_BCM5785:
	case BGE_ASICREV_BCM5787:
	case BGE_ASICREV_BCM57780:
		sc->bge_flags |= BGEF_5755_PLUS | BGEF_575X_PLUS | BGEF_5705_PLUS;
		break;
	case BGE_ASICREV_BCM5700:
	case BGE_ASICREV_BCM5701:
	case BGE_ASICREV_BCM5703:
	case BGE_ASICREV_BCM5704:
		sc->bge_flags |= BGEF_5700_FAMILY | BGEF_JUMBO_CAPABLE;
		break;
	case BGE_ASICREV_BCM5714_A0:
	case BGE_ASICREV_BCM5780:
	case BGE_ASICREV_BCM5714:
		sc->bge_flags |= BGEF_5714_FAMILY | BGEF_JUMBO_CAPABLE;
		/* FALLTHROUGH */
	case BGE_ASICREV_BCM5750:
	case BGE_ASICREV_BCM5752:
	case BGE_ASICREV_BCM5906:
		sc->bge_flags |= BGEF_575X_PLUS;
		/* FALLTHROUGH */
	case BGE_ASICREV_BCM5705:
		sc->bge_flags |= BGEF_5705_PLUS;
		break;
	}

	/* Identify chips with APE processor. */
	switch (BGE_ASICREV(sc->bge_chipid)) {
	case BGE_ASICREV_BCM5717:
	case BGE_ASICREV_BCM5719:
	case BGE_ASICREV_BCM5720:
	case BGE_ASICREV_BCM5761:
		sc->bge_flags |= BGEF_APE;
		break;
	}

	/*
	 * The 40bit DMA bug applies to the 5714/5715 controllers and is
	 * not actually a MAC controller bug but an issue with the embedded
	 * PCIe to PCI-X bridge in the device. Use 40bit DMA workaround.
	 */
	if (BGE_IS_5714_FAMILY(sc) && ((sc->bge_flags & BGEF_PCIX) != 0))
		sc->bge_flags |= BGEF_40BIT_BUG;

	/* Chips with APE need BAR2 access for APE registers/memory. */
	if ((sc->bge_flags & BGEF_APE) != 0) {
		memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, BGE_PCI_BAR2);
#if 0
		if (pci_mapreg_map(pa, BGE_PCI_BAR2, memtype, 0,
			&sc->bge_apetag, &sc->bge_apehandle, NULL,
			&sc->bge_apesize)) {
			aprint_error_dev(sc->bge_dev,
			    "couldn't map BAR2 memory\n");
			return;
		}
#else
		/*
		 * Workaround for PCI prefetchable bit. Some BCM5717-5720 based
		 * system get NMI on boot (PR#48451). This problem might not be
		 * the driver's bug but our PCI common part's bug. Until we
		 * find a real reason, we ignore the prefetchable bit.
		 */
		if (pci_mapreg_info(pa->pa_pc, pa->pa_tag, BGE_PCI_BAR2,
		    memtype, &memaddr, &sc->bge_apesize, &map_flags) != 0) {
			aprint_error_dev(sc->bge_dev,
			    "couldn't map BAR2 memory\n");
			return;
		}

		map_flags &= ~BUS_SPACE_MAP_PREFETCHABLE;
		if (bus_space_map(pa->pa_memt, memaddr,
		    sc->bge_apesize, map_flags, &sc->bge_apehandle) != 0) {
			aprint_error_dev(sc->bge_dev,
			    "couldn't map BAR2 memory\n");
			return;
		}
		sc->bge_apetag = pa->pa_memt;
#endif

		/* Enable APE register/memory access by host driver. */
		reg = pci_conf_read(pa->pa_pc, pa->pa_tag, BGE_PCI_PCISTATE);
		reg |= BGE_PCISTATE_ALLOW_APE_CTLSPC_WR |
		    BGE_PCISTATE_ALLOW_APE_SHMEM_WR |
		    BGE_PCISTATE_ALLOW_APE_PSPACE_WR;
		pci_conf_write(pa->pa_pc, pa->pa_tag, BGE_PCI_PCISTATE, reg);

		bge_ape_lock_init(sc);
		bge_ape_read_fw_ver(sc);
	}

	/* Identify the chips that use an CPMU. */
	if (BGE_IS_5717_PLUS(sc) ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5784 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5761 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5785 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM57780)
		sc->bge_flags |= BGEF_CPMU_PRESENT;

	/* Set MI_MODE */
	mimode = BGE_MIMODE_PHYADDR(sc->bge_phy_addr);
	if ((sc->bge_flags & BGEF_CPMU_PRESENT) != 0)
		mimode |= BGE_MIMODE_500KHZ_CONST;
	else
		mimode |= BGE_MIMODE_BASE;
	CSR_WRITE_4(sc, BGE_MI_MODE, mimode);

	/*
	 * When using the BCM5701 in PCI-X mode, data corruption has
	 * been observed in the first few bytes of some received packets.
	 * Aligning the packet buffer in memory eliminates the corruption.
	 * Unfortunately, this misaligns the packet payloads.  On platforms
	 * which do not support unaligned accesses, we will realign the
	 * payloads by copying the received packets.
	 */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5701 &&
	    sc->bge_flags & BGEF_PCIX)
		sc->bge_flags |= BGEF_RX_ALIGNBUG;

	if (BGE_IS_5700_FAMILY(sc))
		sc->bge_flags |= BGEF_JUMBO_CAPABLE;

	misccfg = CSR_READ_4(sc, BGE_MISC_CFG);
	misccfg &= BGE_MISCCFG_BOARD_ID_MASK;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5705 &&
	    (misccfg == BGE_MISCCFG_BOARD_ID_5788 ||
	     misccfg == BGE_MISCCFG_BOARD_ID_5788M))
		sc->bge_flags |= BGEF_IS_5788;

	/*
	 * Some controllers seem to require a special firmware to use
	 * TSO. But the firmware is not available to FreeBSD and Linux
	 * claims that the TSO performed by the firmware is slower than
	 * hardware based TSO. Moreover the firmware based TSO has one
	 * known bug which can't handle TSO if ethernet header + IP/TCP
	 * header is greater than 80 bytes. The workaround for the TSO
	 * bug exist but it seems it's too expensive than not using
	 * TSO at all. Some hardwares also have the TSO bug so limit
	 * the TSO to the controllers that are not affected TSO issues
	 * (e.g. 5755 or higher).
	 */
	if (BGE_IS_5755_PLUS(sc)) {
		/*
		 * BCM5754 and BCM5787 shares the same ASIC id so
		 * explicit device id check is required.
		 */
		if ((PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_BROADCOM_BCM5754) &&
		    (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_BROADCOM_BCM5754M))
			sc->bge_flags |= BGEF_TSO;
	}

	capmask = 0xffffffff; /* XXX BMSR_DEFCAPMASK */
	if ((BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5703 &&
	     (misccfg == 0x4000 || misccfg == 0x8000)) ||
	    (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5705 &&
	     PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BROADCOM &&
	     (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5901 ||
	      PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5901A2 ||
	      PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5705F)) ||
	    (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BROADCOM &&
	     (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5751F ||
	      PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5753F ||
	      PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5787F)) ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM57790 ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM57791 ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM57795 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906) {
		/* These chips are 10/100 only. */
		capmask &= ~BMSR_EXTSTAT;
		sc->bge_phy_flags |= BGEPHYF_NO_WIRESPEED;
	}

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700 ||
	    (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5705 &&
	     (sc->bge_chipid != BGE_CHIPID_BCM5705_A0 &&
		 sc->bge_chipid != BGE_CHIPID_BCM5705_A1)))
		sc->bge_phy_flags |= BGEPHYF_NO_WIRESPEED;

	/* Set various PHY bug flags. */
	if (sc->bge_chipid == BGE_CHIPID_BCM5701_A0 ||
	    sc->bge_chipid == BGE_CHIPID_BCM5701_B0)
		sc->bge_phy_flags |= BGEPHYF_CRC_BUG;
	if (BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5703_AX ||
	    BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5704_AX)
		sc->bge_phy_flags |= BGEPHYF_ADC_BUG;
	if (sc->bge_chipid == BGE_CHIPID_BCM5704_A0)
		sc->bge_phy_flags |= BGEPHYF_5704_A0_BUG;
	if ((BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700 ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5701) &&
	    PCI_VENDOR(subid) == PCI_VENDOR_DELL)
		sc->bge_phy_flags |= BGEPHYF_NO_3LED;
	if (BGE_IS_5705_PLUS(sc) &&
	    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5906 &&
	    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5785 &&
	    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM57780 &&
	    !BGE_IS_57765_PLUS(sc)) {
		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5755 ||
		    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5761 ||
		    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5784 ||
		    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5787) {
			if (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_BROADCOM_BCM5722 &&
			    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_BROADCOM_BCM5756)
				sc->bge_phy_flags |= BGEPHYF_JITTER_BUG;
			if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM5755M)
				sc->bge_phy_flags |= BGEPHYF_ADJUST_TRIM;
		} else
			sc->bge_phy_flags |= BGEPHYF_BER_BUG;
	}

	/*
	 * SEEPROM check.
	 * First check if firmware knows we do not have SEEPROM.
	 */
	if (prop_dictionary_get_bool(device_properties(self),
	     "without-seeprom", &no_seeprom) && no_seeprom)
	 	sc->bge_flags |= BGEF_NO_EEPROM;

	else if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906)
		sc->bge_flags |= BGEF_NO_EEPROM;

	/* Now check the 'ROM failed' bit on the RX CPU */
	else if (CSR_READ_4(sc, BGE_RXCPU_MODE) & BGE_RXCPUMODE_ROMFAIL)
		sc->bge_flags |= BGEF_NO_EEPROM;

	sc->bge_asf_mode = 0;
	/* No ASF if APE present. */
	if ((sc->bge_flags & BGEF_APE) == 0) {
		if (bge_allow_asf && (bge_readmem_ind(sc, BGE_SRAM_DATA_SIG) ==
			BGE_SRAM_DATA_SIG_MAGIC)) {
			if (bge_readmem_ind(sc, BGE_SRAM_DATA_CFG) &
			    BGE_HWCFG_ASF) {
				sc->bge_asf_mode |= ASF_ENABLE;
				sc->bge_asf_mode |= ASF_STACKUP;
				if (BGE_IS_575X_PLUS(sc))
					sc->bge_asf_mode |= ASF_NEW_HANDSHAKE;
			}
		}
	}

#ifdef __HAVE_PCI_MSI_MSIX
	counts[PCI_INTR_TYPE_MSI] = 1;
	counts[PCI_INTR_TYPE_INTX] = 1;
	/* Check MSI capability */
	if (bge_can_use_msi(sc) != 0) {
		max_type = PCI_INTR_TYPE_MSI;
		sc->bge_flags |= BGEF_MSI;
	} else
		max_type = PCI_INTR_TYPE_INTX;

alloc_retry:
	if (pci_intr_alloc(pa, &sc->bge_pihp, counts, max_type) != 0) {
		aprint_error_dev(sc->bge_dev, "couldn't alloc interrupt\n");
		return;
	}
#else	/* !__HAVE_PCI_MSI_MSIX */
	DPRINTFN(5, ("pci_intr_map\n"));
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->bge_dev, "couldn't map interrupt\n");
		return;
	}
#endif

	DPRINTFN(5, ("pci_intr_string\n"));
#ifdef __HAVE_PCI_MSI_MSIX
	intrstr = pci_intr_string(pc, sc->bge_pihp[0], intrbuf,
	    sizeof(intrbuf));
	DPRINTFN(5, ("pci_intr_establish\n"));
	sc->bge_intrhand = pci_intr_establish(pc, sc->bge_pihp[0], IPL_NET,
	    bge_intr, sc);
	if (sc->bge_intrhand == NULL) {
		intr_type = pci_intr_type(sc->bge_pihp[0]);
		aprint_error_dev(sc->bge_dev,"unable to establish %s\n",
		    (intr_type == PCI_INTR_TYPE_MSI) ? "MSI" : "INTx");
		pci_intr_release(pc, sc->bge_pihp, 1);
		switch (intr_type) {
		case PCI_INTR_TYPE_MSI:
			/* The next try is for INTx: Disable MSI */
			max_type = PCI_INTR_TYPE_INTX;
			counts[PCI_INTR_TYPE_INTX] = 1;
			sc->bge_flags &= ~BGEF_MSI;
			goto alloc_retry;
		case PCI_INTR_TYPE_INTX:
		default:
			/* See below */
			break;
		}
	}
#else	/* !__HAVE_PCI_MSI_MSIX */
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));

	DPRINTFN(5, ("pci_intr_establish\n"));
	sc->bge_intrhand = pci_intr_establish(pc, ih, IPL_NET, bge_intr, sc);
#endif

	if (sc->bge_intrhand == NULL) {
		aprint_error_dev(sc->bge_dev,
		    "couldn't establish interrupt%s%s\n",
		    intrstr ? " at " : "", intrstr ? intrstr : "");
		return;
	}
	aprint_normal_dev(sc->bge_dev, "interrupting at %s\n", intrstr);

	/*
	 * All controllers except BCM5700 supports tagged status but
	 * we use tagged status only for MSI case on BCM5717. Otherwise
	 * MSI on BCM5717 does not work.
	 */
	if (BGE_IS_5717_PLUS(sc) && sc->bge_flags & BGEF_MSI)
		sc->bge_flags |= BGEF_TAGGED_STATUS;

	/*
	 * Reset NVRAM before bge_reset(). It's required to acquire NVRAM
	 * lock in bge_reset().
	 */
	CSR_WRITE_4(sc, BGE_EE_ADDR,
	    BGE_EEADDR_RESET | BGE_EEHALFCLK(BGE_HALFCLK_384SCL));
	delay(1000);
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_AUTO_EEPROM);

	bge_stop_fw(sc);
	bge_sig_pre_reset(sc, BGE_RESET_START);
	if (bge_reset(sc))
		aprint_error_dev(sc->bge_dev, "chip reset failed\n");

	/*
	 * Read the hardware config word in the first 32k of NIC internal
	 * memory, or fall back to the config word in the EEPROM.
	 * Note: on some BCM5700 cards, this value appears to be unset.
	 */
	hwcfg = hwcfg2 = hwcfg3 = hwcfg4 = hwcfg5 = 0;
	if (bge_readmem_ind(sc, BGE_SRAM_DATA_SIG) ==
	    BGE_SRAM_DATA_SIG_MAGIC) {
		uint32_t tmp;

		hwcfg = bge_readmem_ind(sc, BGE_SRAM_DATA_CFG);
		tmp = bge_readmem_ind(sc, BGE_SRAM_DATA_VER) >>
		    BGE_SRAM_DATA_VER_SHIFT;
		if ((0 < tmp) && (tmp < 0x100))
			hwcfg2 = bge_readmem_ind(sc, BGE_SRAM_DATA_CFG_2);
		if (sc->bge_flags & BGEF_PCIE)
			hwcfg3 = bge_readmem_ind(sc, BGE_SRAM_DATA_CFG_3);
		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5785)
			hwcfg4 = bge_readmem_ind(sc, BGE_SRAM_DATA_CFG_4);
		if (BGE_IS_5717_PLUS(sc))
			hwcfg5 = bge_readmem_ind(sc, BGE_SRAM_DATA_CFG_5);
	} else if (!(sc->bge_flags & BGEF_NO_EEPROM)) {
		bge_read_eeprom(sc, (void *)&hwcfg,
		    BGE_EE_HWCFG_OFFSET, sizeof(hwcfg));
		hwcfg = be32toh(hwcfg);
	}
	aprint_normal_dev(sc->bge_dev,
	    "HW config %08x, %08x, %08x, %08x %08x\n",
	    hwcfg, hwcfg2, hwcfg3, hwcfg4, hwcfg5);

	bge_sig_legacy(sc, BGE_RESET_START);
	bge_sig_post_reset(sc, BGE_RESET_START);

	if (bge_chipinit(sc)) {
		aprint_error_dev(sc->bge_dev, "chip initialization failed\n");
		bge_release_resources(sc);
		return;
	}

	/*
	 * Get station address from the EEPROM.
	 */
	if (bge_get_eaddr(sc, eaddr)) {
		aprint_error_dev(sc->bge_dev,
		    "failed to read station address\n");
		bge_release_resources(sc);
		return;
	}

	br = bge_lookup_rev(sc->bge_chipid);

	if (br == NULL) {
		aprint_normal_dev(sc->bge_dev, "unknown ASIC (0x%x)",
		    sc->bge_chipid);
	} else {
		aprint_normal_dev(sc->bge_dev, "ASIC %s (0x%x)",
		    br->br_name, sc->bge_chipid);
	}
	aprint_normal(", Ethernet address %s\n", ether_sprintf(eaddr));

	/* Allocate the general information block and ring buffers. */
	if (pci_dma64_available(pa))
		sc->bge_dmatag = pa->pa_dmat64;
	else
		sc->bge_dmatag = pa->pa_dmat;

	/* 40bit DMA workaround */
	if (sizeof(bus_addr_t) > 4) {
		if ((sc->bge_flags & BGEF_40BIT_BUG) != 0) {
			bus_dma_tag_t olddmatag = sc->bge_dmatag; /* save */

			if (bus_dmatag_subregion(olddmatag, 0,
				(bus_addr_t)(1ULL << 40), &(sc->bge_dmatag),
				BUS_DMA_NOWAIT) != 0) {
				aprint_error_dev(self,
				    "WARNING: failed to restrict dma range,"
				    " falling back to parent bus dma range\n");
				sc->bge_dmatag = olddmatag;
			}
		}
	}
	DPRINTFN(5, ("bus_dmamem_alloc\n"));
	if (bus_dmamem_alloc(sc->bge_dmatag, sizeof(struct bge_ring_data),
			     PAGE_SIZE, 0, &sc->bge_ring_seg, 1,
		&sc->bge_ring_rseg, BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->bge_dev, "can't alloc rx buffers\n");
		return;
	}
	DPRINTFN(5, ("bus_dmamem_map\n"));
	if (bus_dmamem_map(sc->bge_dmatag, &sc->bge_ring_seg,
		sc->bge_ring_rseg, sizeof(struct bge_ring_data), &kva,
			   BUS_DMA_NOWAIT)) {
		aprint_error_dev(sc->bge_dev,
		    "can't map DMA buffers (%zu bytes)\n",
		    sizeof(struct bge_ring_data));
		bus_dmamem_free(sc->bge_dmatag, &sc->bge_ring_seg,
		    sc->bge_ring_rseg);
		return;
	}
	DPRINTFN(5, ("bus_dmamem_create\n"));
	if (bus_dmamap_create(sc->bge_dmatag, sizeof(struct bge_ring_data), 1,
	    sizeof(struct bge_ring_data), 0,
	    BUS_DMA_NOWAIT, &sc->bge_ring_map)) {
		aprint_error_dev(sc->bge_dev, "can't create DMA map\n");
		bus_dmamem_unmap(sc->bge_dmatag, kva,
				 sizeof(struct bge_ring_data));
		bus_dmamem_free(sc->bge_dmatag, &sc->bge_ring_seg,
		    sc->bge_ring_rseg);
		return;
	}
	DPRINTFN(5, ("bus_dmamem_load\n"));
	if (bus_dmamap_load(sc->bge_dmatag, sc->bge_ring_map, kva,
			    sizeof(struct bge_ring_data), NULL,
			    BUS_DMA_NOWAIT)) {
		bus_dmamap_destroy(sc->bge_dmatag, sc->bge_ring_map);
		bus_dmamem_unmap(sc->bge_dmatag, kva,
				 sizeof(struct bge_ring_data));
		bus_dmamem_free(sc->bge_dmatag, &sc->bge_ring_seg,
		    sc->bge_ring_rseg);
		return;
	}

	DPRINTFN(5, ("bzero\n"));
	sc->bge_rdata = (struct bge_ring_data *)kva;

	memset(sc->bge_rdata, 0, sizeof(struct bge_ring_data));

	/* Try to allocate memory for jumbo buffers. */
	if (BGE_IS_JUMBO_CAPABLE(sc)) {
		if (bge_alloc_jumbo_mem(sc)) {
			aprint_error_dev(sc->bge_dev,
			    "jumbo buffer allocation failed\n");
		} else
			sc->ethercom.ec_capabilities |= ETHERCAP_JUMBO_MTU;
	}

	/* Set default tuneable values. */
	sc->bge_stat_ticks = BGE_TICKS_PER_SEC;
	sc->bge_rx_coal_ticks = 150;
	sc->bge_rx_max_coal_bds = 64;
	sc->bge_tx_coal_ticks = 300;
	sc->bge_tx_max_coal_bds = 400;
	if (BGE_IS_5705_PLUS(sc)) {
		sc->bge_tx_coal_ticks = (12 * 5);
		sc->bge_tx_max_coal_bds = (12 * 5);
			aprint_verbose_dev(sc->bge_dev,
			    "setting short Tx thresholds\n");
	}

	if (BGE_IS_5717_PLUS(sc))
		sc->bge_return_ring_cnt = BGE_RETURN_RING_CNT;
	else if (BGE_IS_5705_PLUS(sc))
		sc->bge_return_ring_cnt = BGE_RETURN_RING_CNT_5705;
	else
		sc->bge_return_ring_cnt = BGE_RETURN_RING_CNT;

	/* Set up ifnet structure */
	ifp = &sc->ethercom.ec_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = bge_ioctl;
	ifp->if_stop = bge_stop;
	ifp->if_start = bge_start;
	ifp->if_init = bge_init;
	ifp->if_watchdog = bge_watchdog;
	IFQ_SET_MAXLEN(&ifp->if_snd, max(BGE_TX_RING_CNT - 1, IFQ_MAXLEN));
	IFQ_SET_READY(&ifp->if_snd);
	DPRINTFN(5, ("strcpy if_xname\n"));
	strcpy(ifp->if_xname, device_xname(sc->bge_dev));

	if (sc->bge_chipid != BGE_CHIPID_BCM5700_B0)
		sc->ethercom.ec_if.if_capabilities |=
		    IFCAP_CSUM_IPv4_Tx | IFCAP_CSUM_IPv4_Rx;
#if 1	/* XXX TCP/UDP checksum offload breaks with pf(4) */
		sc->ethercom.ec_if.if_capabilities |=
		    IFCAP_CSUM_TCPv4_Tx | IFCAP_CSUM_TCPv4_Rx |
		    IFCAP_CSUM_UDPv4_Tx | IFCAP_CSUM_UDPv4_Rx;
#endif
	sc->ethercom.ec_capabilities |=
	    ETHERCAP_VLAN_HWTAGGING | ETHERCAP_VLAN_MTU;

	if (sc->bge_flags & BGEF_TSO)
		sc->ethercom.ec_if.if_capabilities |= IFCAP_TSOv4;

	/*
	 * Do MII setup.
	 */
	DPRINTFN(5, ("mii setup\n"));
	sc->bge_mii.mii_ifp = ifp;
	sc->bge_mii.mii_readreg = bge_miibus_readreg;
	sc->bge_mii.mii_writereg = bge_miibus_writereg;
	sc->bge_mii.mii_statchg = bge_miibus_statchg;

	/*
	 * Figure out what sort of media we have by checking the hardware
	 * config word.  Note: on some BCM5700 cards, this value appears to be
	 * unset. If that's the case, we have to rely on identifying the NIC
	 * by its PCI subsystem ID, as we do below for the SysKonnect SK-9D41.
	 * The SysKonnect SK-9D41 is a 1000baseSX card.
	 */
	if (PCI_PRODUCT(pa->pa_id) == SK_SUBSYSID_9D41 ||
	    (hwcfg & BGE_HWCFG_MEDIA) == BGE_MEDIA_FIBER) {
		if (BGE_IS_5705_PLUS(sc)) {
			sc->bge_flags |= BGEF_FIBER_MII;
			sc->bge_phy_flags |= BGEPHYF_NO_WIRESPEED;
		} else
			sc->bge_flags |= BGEF_FIBER_TBI;
	}

	/* Set bge_phy_flags before prop_dictionary_set_uint32() */
	if (BGE_IS_JUMBO_CAPABLE(sc))
		sc->bge_phy_flags |= BGEPHYF_JUMBO_CAPABLE;

	/* set phyflags and chipid before mii_attach() */
	dict = device_properties(self);
	prop_dictionary_set_uint32(dict, "phyflags", sc->bge_phy_flags);
	prop_dictionary_set_uint32(dict, "chipid", sc->bge_chipid);

	if (sc->bge_flags & BGEF_FIBER_TBI) {
		ifmedia_init(&sc->bge_ifmedia, IFM_IMASK, bge_ifmedia_upd,
		    bge_ifmedia_sts);
		ifmedia_add(&sc->bge_ifmedia, IFM_ETHER |IFM_1000_SX, 0, NULL);
		ifmedia_add(&sc->bge_ifmedia, IFM_ETHER | IFM_1000_SX|IFM_FDX,
			    0, NULL);
		ifmedia_add(&sc->bge_ifmedia, IFM_ETHER | IFM_AUTO, 0, NULL);
		ifmedia_set(&sc->bge_ifmedia, IFM_ETHER | IFM_AUTO);
		/* Pretend the user requested this setting */
		sc->bge_ifmedia.ifm_media = sc->bge_ifmedia.ifm_cur->ifm_media;
	} else {
		/*
		 * Do transceiver setup and tell the firmware the
		 * driver is down so we can try to get access the
		 * probe if ASF is running.  Retry a couple of times
		 * if we get a conflict with the ASF firmware accessing
		 * the PHY.
		 */
		BGE_CLRBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);
		bge_asf_driver_up(sc);

		ifmedia_init(&sc->bge_mii.mii_media, 0, bge_ifmedia_upd,
			     bge_ifmedia_sts);
		mii_flags = MIIF_DOPAUSE;
		if (sc->bge_flags & BGEF_FIBER_MII)
			mii_flags |= MIIF_HAVEFIBER;
		mii_attach(sc->bge_dev, &sc->bge_mii, capmask, sc->bge_phy_addr,
		    MII_OFFSET_ANY, mii_flags);

		if (LIST_EMPTY(&sc->bge_mii.mii_phys)) {
			aprint_error_dev(sc->bge_dev, "no PHY found!\n");
			ifmedia_add(&sc->bge_mii.mii_media,
				    IFM_ETHER|IFM_MANUAL, 0, NULL);
			ifmedia_set(&sc->bge_mii.mii_media,
				    IFM_ETHER|IFM_MANUAL);
		} else
			ifmedia_set(&sc->bge_mii.mii_media,
				    IFM_ETHER|IFM_AUTO);

		/*
		 * Now tell the firmware we are going up after probing the PHY
		 */
		if (sc->bge_asf_mode & ASF_STACKUP)
			BGE_SETBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);
	}

	/*
	 * Call MI attach routine.
	 */
	DPRINTFN(5, ("if_attach\n"));
	if_attach(ifp);
	DPRINTFN(5, ("ether_ifattach\n"));
	ether_ifattach(ifp, eaddr);
	ether_set_ifflags_cb(&sc->ethercom, bge_ifflags_cb);
	rnd_attach_source(&sc->rnd_source, device_xname(sc->bge_dev),
		RND_TYPE_NET, RND_FLAG_DEFAULT);
#ifdef BGE_EVENT_COUNTERS
	/*
	 * Attach event counters.
	 */
	evcnt_attach_dynamic(&sc->bge_ev_intr, EVCNT_TYPE_INTR,
	    NULL, device_xname(sc->bge_dev), "intr");
	evcnt_attach_dynamic(&sc->bge_ev_tx_xoff, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->bge_dev), "tx_xoff");
	evcnt_attach_dynamic(&sc->bge_ev_tx_xon, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->bge_dev), "tx_xon");
	evcnt_attach_dynamic(&sc->bge_ev_rx_xoff, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->bge_dev), "rx_xoff");
	evcnt_attach_dynamic(&sc->bge_ev_rx_xon, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->bge_dev), "rx_xon");
	evcnt_attach_dynamic(&sc->bge_ev_rx_macctl, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->bge_dev), "rx_macctl");
	evcnt_attach_dynamic(&sc->bge_ev_xoffentered, EVCNT_TYPE_MISC,
	    NULL, device_xname(sc->bge_dev), "xoffentered");
#endif /* BGE_EVENT_COUNTERS */
	DPRINTFN(5, ("callout_init\n"));
	callout_init(&sc->bge_timeout, 0);

	if (pmf_device_register(self, NULL, NULL))
		pmf_class_network_register(self, ifp);
	else
		aprint_error_dev(self, "couldn't establish power handler\n");

	bge_sysctl_init(sc);

#ifdef BGE_DEBUG
	bge_debug_info(sc);
#endif
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static int
bge_detach(device_t self, int flags __unused)
{
	struct bge_softc *sc = device_private(self);
	struct ifnet *ifp = &sc->ethercom.ec_if;
	int s;

	s = splnet();
	/* Stop the interface. Callouts are stopped in it. */
	bge_stop(ifp, 1);
	splx(s);

	mii_detach(&sc->bge_mii, MII_PHY_ANY, MII_OFFSET_ANY);

	/* Delete all remaining media. */
	ifmedia_delete_instance(&sc->bge_mii.mii_media, IFM_INST_ANY);

	ether_ifdetach(ifp);
	if_detach(ifp);

	bge_release_resources(sc);

	return 0;
}

static void
bge_release_resources(struct bge_softc *sc)
{

	/* Disestablish the interrupt handler */
	if (sc->bge_intrhand != NULL) {
		pci_intr_disestablish(sc->sc_pc, sc->bge_intrhand);
#ifdef __HAVE_PCI_MSI_MSIX
		pci_intr_release(sc->sc_pc, sc->bge_pihp, 1);
#endif
		sc->bge_intrhand = NULL;
	}

	if (sc->bge_dmatag != NULL) {
		bus_dmamap_unload(sc->bge_dmatag, sc->bge_ring_map);
		bus_dmamap_destroy(sc->bge_dmatag, sc->bge_ring_map);
		bus_dmamem_unmap(sc->bge_dmatag, (void *)sc->bge_rdata,
		    sizeof(struct bge_ring_data));
		bus_dmamem_free(sc->bge_dmatag, &sc->bge_ring_seg, sc->bge_ring_rseg);
	}

	/* Unmap the device registers */
	if (sc->bge_bsize != 0) {
		bus_space_unmap(sc->bge_btag, sc->bge_bhandle, sc->bge_bsize);
		sc->bge_bsize = 0;
	}

	/* Unmap the APE registers */
	if (sc->bge_apesize != 0) {
		bus_space_unmap(sc->bge_apetag, sc->bge_apehandle,
		    sc->bge_apesize);
		sc->bge_apesize = 0;
	}
}

static int
bge_reset(struct bge_softc *sc)
{
	uint32_t cachesize, command;
	uint32_t reset, mac_mode, mac_mode_mask;
	pcireg_t devctl, reg;
	int i, val;
	void (*write_op)(struct bge_softc *, int, int);

	/* Make mask for BGE_MAC_MODE register. */
	mac_mode_mask = BGE_MACMODE_HALF_DUPLEX | BGE_MACMODE_PORTMODE;
	if ((sc->bge_mfw_flags & BGE_MFW_ON_APE) != 0)
		mac_mode_mask |= BGE_MACMODE_APE_RX_EN | BGE_MACMODE_APE_TX_EN;
	/* Keep mac_mode_mask's bits of BGE_MAC_MODE register into mac_mode */
	mac_mode = CSR_READ_4(sc, BGE_MAC_MODE) & mac_mode_mask;
	
	if (BGE_IS_575X_PLUS(sc) && !BGE_IS_5714_FAMILY(sc) &&
	    (BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5906)) {
	    	if (sc->bge_flags & BGEF_PCIE)
			write_op = bge_writemem_direct;
		else
			write_op = bge_writemem_ind;
	} else
		write_op = bge_writereg_ind;

	/* 57XX step 4 */
	/* Acquire the NVM lock */
	if ((sc->bge_flags & BGEF_NO_EEPROM) == 0 &&
	    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5700 &&
	    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5701) {
		CSR_WRITE_4(sc, BGE_NVRAM_SWARB, BGE_NVRAMSWARB_SET1);
		for (i = 0; i < 8000; i++) {
			if (CSR_READ_4(sc, BGE_NVRAM_SWARB) &
			    BGE_NVRAMSWARB_GNT1)
				break;
			DELAY(20);
		}
		if (i == 8000) {
			printf("%s: NVRAM lock timedout!\n",
			    device_xname(sc->bge_dev));
		}
	}

	/* Take APE lock when performing reset. */
	bge_ape_lock(sc, BGE_APE_LOCK_GRC);

	/* 57XX step 3 */
	/* Save some important PCI state. */
	cachesize = pci_conf_read(sc->sc_pc, sc->sc_pcitag, BGE_PCI_CACHESZ);
	/* 5718 reset step 3 */
	command = pci_conf_read(sc->sc_pc, sc->sc_pcitag, BGE_PCI_CMD);

	/* 5718 reset step 5, 57XX step 5b-5d */
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_MISC_CTL,
	    BGE_PCIMISCCTL_INDIRECT_ACCESS | BGE_PCIMISCCTL_MASK_PCI_INTR |
	    BGE_HIF_SWAP_OPTIONS | BGE_PCIMISCCTL_PCISTATE_RW);

	/* XXX ???: Disable fastboot on controllers that support it. */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5752 ||
	    BGE_IS_5755_PLUS(sc))
		CSR_WRITE_4(sc, BGE_FASTBOOT_PC, 0);

	/* 5718 reset step 2, 57XX step 6 */
	/*
	 * Write the magic number to SRAM at offset 0xB50.
	 * When firmware finishes its initialization it will
	 * write ~BGE_MAGIC_NUMBER to the same location.
	 */
	bge_writemem_ind(sc, BGE_SRAM_FW_MB, BGE_SRAM_FW_MB_MAGIC);

	/* 5718 reset step 6, 57XX step 7 */
	reset = BGE_MISCCFG_RESET_CORE_CLOCKS | BGE_32BITTIME_66MHZ;
	/*
	 * XXX: from FreeBSD/Linux; no documentation
	 */
	if (sc->bge_flags & BGEF_PCIE) {
		if ((BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5785) &&
		    !BGE_IS_57765_PLUS(sc) &&
		    (CSR_READ_4(sc, BGE_PHY_TEST_CTRL_REG) ==
			(BGE_PHY_PCIE_LTASS_MODE | BGE_PHY_PCIE_SCRAM_MODE))) {
			/* PCI Express 1.0 system */
			CSR_WRITE_4(sc, BGE_PHY_TEST_CTRL_REG,
			    BGE_PHY_PCIE_SCRAM_MODE);
		}
		if (sc->bge_chipid != BGE_CHIPID_BCM5750_A0) {
			/*
			 * Prevent PCI Express link training
			 * during global reset.
			 */
			CSR_WRITE_4(sc, BGE_MISC_CFG, 1 << 29);
			reset |= (1 << 29);
		}
	}

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906) {
		i = CSR_READ_4(sc, BGE_VCPU_STATUS);
		CSR_WRITE_4(sc, BGE_VCPU_STATUS,
		    i | BGE_VCPU_STATUS_DRV_RESET);
		i = CSR_READ_4(sc, BGE_VCPU_EXT_CTRL);
		CSR_WRITE_4(sc, BGE_VCPU_EXT_CTRL,
		    i & ~BGE_VCPU_EXT_CTRL_HALT_CPU);
	}

	/*
	 * Set GPHY Power Down Override to leave GPHY
	 * powered up in D0 uninitialized.
	 */
	if (BGE_IS_5705_PLUS(sc) &&
	    (sc->bge_flags & BGEF_CPMU_PRESENT) == 0)
		reset |= BGE_MISCCFG_GPHY_PD_OVERRIDE;

	/* Issue global reset */
	write_op(sc, BGE_MISC_CFG, reset);

	/* 5718 reset step 7, 57XX step 8 */
	if (sc->bge_flags & BGEF_PCIE)
		delay(100*1000); /* too big */
	else
		delay(1000);

	if (sc->bge_flags & BGEF_PCIE) {
		if (sc->bge_chipid == BGE_CHIPID_BCM5750_A0) {
			DELAY(500000);
			/* XXX: Magic Numbers */
			reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
			    BGE_PCI_UNKNOWN0);
			pci_conf_write(sc->sc_pc, sc->sc_pcitag,
			    BGE_PCI_UNKNOWN0,
			    reg | (1 << 15));
		}
		devctl = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
		    sc->bge_pciecap + PCIE_DCSR);
		/* Clear enable no snoop and disable relaxed ordering. */
		devctl &= ~(PCIE_DCSR_ENA_RELAX_ORD |
		    PCIE_DCSR_ENA_NO_SNOOP);

		/* Set PCIE max payload size to 128 for older PCIe devices */
		if ((sc->bge_flags & BGEF_CPMU_PRESENT) == 0)
			devctl &= ~(0x00e0);
		/* Clear device status register. Write 1b to clear */
		devctl |= PCIE_DCSR_URD | PCIE_DCSR_FED
		    | PCIE_DCSR_NFED | PCIE_DCSR_CED;
		pci_conf_write(sc->sc_pc, sc->sc_pcitag,
		    sc->bge_pciecap + PCIE_DCSR, devctl);
		bge_set_max_readrq(sc);
	}

	/* From Linux: dummy read to flush PCI posted writes */
	reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, BGE_PCI_CMD);

	/*
	 * Reset some of the PCI state that got zapped by reset
	 * To modify the PCISTATE register, BGE_PCIMISCCTL_PCISTATE_RW must be
	 * set, too.
	 */
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_MISC_CTL,
	    BGE_PCIMISCCTL_INDIRECT_ACCESS | BGE_PCIMISCCTL_MASK_PCI_INTR |
	    BGE_HIF_SWAP_OPTIONS | BGE_PCIMISCCTL_PCISTATE_RW);
	val = BGE_PCISTATE_ROM_ENABLE | BGE_PCISTATE_ROM_RETRY_ENABLE;
	if (sc->bge_chipid == BGE_CHIPID_BCM5704_A0 &&
	    (sc->bge_flags & BGEF_PCIX) != 0)
		val |= BGE_PCISTATE_RETRY_SAME_DMA;
	if ((sc->bge_mfw_flags & BGE_MFW_ON_APE) != 0)
		val |= BGE_PCISTATE_ALLOW_APE_CTLSPC_WR |
		    BGE_PCISTATE_ALLOW_APE_SHMEM_WR |
		    BGE_PCISTATE_ALLOW_APE_PSPACE_WR;
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_PCISTATE, val);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_CACHESZ, cachesize);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_CMD, command);

	/* 57xx step 11: disable PCI-X Relaxed Ordering. */
	if (sc->bge_flags & BGEF_PCIX) {
		reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, sc->bge_pcixcap
		    + PCIX_CMD);
		/* Set max memory read byte count to 2K */
		if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5703) {
			reg &= ~PCIX_CMD_BYTECNT_MASK;
			reg |= PCIX_CMD_BCNT_2048;
		} else if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704){
			/*
			 * For 5704, set max outstanding split transaction
			 * field to 0 (0 means it supports 1 request)
			 */
			reg &= ~(PCIX_CMD_SPLTRANS_MASK
			    | PCIX_CMD_BYTECNT_MASK);
			reg |= PCIX_CMD_BCNT_2048;
		}
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, sc->bge_pcixcap
		    + PCIX_CMD, reg & ~PCIX_CMD_RELAXED_ORDER);
	}

	/* 5718 reset step 10, 57XX step 12 */
	/* Enable memory arbiter. */
	if (BGE_IS_5714_FAMILY(sc)) {
		val = CSR_READ_4(sc, BGE_MARB_MODE);
		CSR_WRITE_4(sc, BGE_MARB_MODE, BGE_MARBMODE_ENABLE | val);
	} else
		CSR_WRITE_4(sc, BGE_MARB_MODE, BGE_MARBMODE_ENABLE);

	/* XXX 5721, 5751 and 5752 */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5750) {
		/* Step 19: */
		BGE_SETBIT(sc, BGE_TLP_CONTROL_REG, 1 << 29 | 1 << 25);
		/* Step 20: */
		BGE_SETBIT(sc, BGE_TLP_CONTROL_REG, BGE_TLP_DATA_FIFO_PROTECT);
	}

	/* 5718 reset step 12, 57XX step 15 and 16 */
	/* Fix up byte swapping */
	CSR_WRITE_4(sc, BGE_MODE_CTL, BGE_DMA_SWAP_OPTIONS);

	/* 5718 reset step 13, 57XX step 17 */
	/* Poll until the firmware initialization is complete */
	bge_poll_fw(sc);

	/* 57XX step 21 */
	if (BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5704_BX) {
		pcireg_t msidata;
	
		msidata = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
		    BGE_PCI_MSI_DATA);
		msidata |= ((1 << 13 | 1 << 12 | 1 << 10) << 16);
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, BGE_PCI_MSI_DATA,
		    msidata);
	}

	/* 57XX step 18 */
	/* Write mac mode. */
	val = CSR_READ_4(sc, BGE_MAC_MODE);
	/* Restore mac_mode_mask's bits using mac_mode */
	val = (val & ~mac_mode_mask) | mac_mode;
	CSR_WRITE_4_FLUSH(sc, BGE_MAC_MODE, val);
	DELAY(40);

	bge_ape_unlock(sc, BGE_APE_LOCK_GRC);

	/*
	 * The 5704 in TBI mode apparently needs some special
	 * adjustment to insure the SERDES drive level is set
	 * to 1.2V.
	 */
	if (sc->bge_flags & BGEF_FIBER_TBI &&
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704) {
		uint32_t serdescfg;

		serdescfg = CSR_READ_4(sc, BGE_SERDES_CFG);
		serdescfg = (serdescfg & ~0xFFF) | 0x880;
		CSR_WRITE_4(sc, BGE_SERDES_CFG, serdescfg);
	}

	if (sc->bge_flags & BGEF_PCIE &&
	    !BGE_IS_57765_PLUS(sc) &&
	    sc->bge_chipid != BGE_CHIPID_BCM5750_A0 &&
	    BGE_ASICREV(sc->bge_chipid) != BGE_ASICREV_BCM5785) {
		uint32_t v;

		/* Enable PCI Express bug fix */
		v = CSR_READ_4(sc, BGE_TLP_CONTROL_REG);
		CSR_WRITE_4(sc, BGE_TLP_CONTROL_REG,
		    v | BGE_TLP_DATA_FIFO_PROTECT);
	}

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5720)
		BGE_CLRBIT(sc, BGE_CPMU_CLCK_ORIDE,
		    CPMU_CLCK_ORIDE_MAC_ORIDE_EN);

	return 0;
}

/*
 * Frame reception handling. This is called if there's a frame
 * on the receive return list.
 *
 * Note: we have to be able to handle two possibilities here:
 * 1) the frame is from the jumbo receive ring
 * 2) the frame is from the standard receive ring
 */

static void
bge_rxeof(struct bge_softc *sc)
{
	struct ifnet *ifp;
	uint16_t rx_prod, rx_cons;
	int stdcnt = 0, jumbocnt = 0;
	bus_dmamap_t dmamap;
	bus_addr_t offset, toff;
	bus_size_t tlen;
	int tosync;

	rx_cons = sc->bge_rx_saved_considx;
	rx_prod = sc->bge_rdata->bge_status_block.bge_idx[0].bge_rx_prod_idx;

	/* Nothing to do */
	if (rx_cons == rx_prod)
		return;

	ifp = &sc->ethercom.ec_if;

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offsetof(struct bge_ring_data, bge_status_block),
	    sizeof (struct bge_status_block),
	    BUS_DMASYNC_POSTREAD);

	offset = offsetof(struct bge_ring_data, bge_rx_return_ring);
	tosync = rx_prod - rx_cons;

	if (tosync != 0)
		rnd_add_uint32(&sc->rnd_source, tosync);

	toff = offset + (rx_cons * sizeof (struct bge_rx_bd));

	if (tosync < 0) {
		tlen = (sc->bge_return_ring_cnt - rx_cons) *
		    sizeof (struct bge_rx_bd);
		bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
		    toff, tlen, BUS_DMASYNC_POSTREAD);
		tosync = -tosync;
	}

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offset, tosync * sizeof (struct bge_rx_bd),
	    BUS_DMASYNC_POSTREAD);

	while (rx_cons != rx_prod) {
		struct bge_rx_bd	*cur_rx;
		uint32_t		rxidx;
		struct mbuf		*m = NULL;

		cur_rx = &sc->bge_rdata->bge_rx_return_ring[rx_cons];

		rxidx = cur_rx->bge_idx;
		BGE_INC(rx_cons, sc->bge_return_ring_cnt);

		if (cur_rx->bge_flags & BGE_RXBDFLAG_JUMBO_RING) {
			BGE_INC(sc->bge_jumbo, BGE_JUMBO_RX_RING_CNT);
			m = sc->bge_cdata.bge_rx_jumbo_chain[rxidx];
			sc->bge_cdata.bge_rx_jumbo_chain[rxidx] = NULL;
			jumbocnt++;
			bus_dmamap_sync(sc->bge_dmatag,
			    sc->bge_cdata.bge_rx_jumbo_map,
			    mtod(m, char *) - (char *)sc->bge_cdata.bge_jumbo_buf,
			    BGE_JLEN, BUS_DMASYNC_POSTREAD);
			if (cur_rx->bge_flags & BGE_RXBDFLAG_ERROR) {
				ifp->if_ierrors++;
				bge_newbuf_jumbo(sc, sc->bge_jumbo, m);
				continue;
			}
			if (bge_newbuf_jumbo(sc, sc->bge_jumbo,
					     NULL)== ENOBUFS) {
				ifp->if_ierrors++;
				bge_newbuf_jumbo(sc, sc->bge_jumbo, m);
				continue;
			}
		} else {
			BGE_INC(sc->bge_std, BGE_STD_RX_RING_CNT);
			m = sc->bge_cdata.bge_rx_std_chain[rxidx];

			sc->bge_cdata.bge_rx_std_chain[rxidx] = NULL;
			stdcnt++;
			dmamap = sc->bge_cdata.bge_rx_std_map[rxidx];
			sc->bge_cdata.bge_rx_std_map[rxidx] = 0;
			if (dmamap == NULL) {
				ifp->if_ierrors++;
				bge_newbuf_std(sc, sc->bge_std, m, dmamap);
				continue;
			}
			bus_dmamap_sync(sc->bge_dmatag, dmamap, 0,
			    dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->bge_dmatag, dmamap);
			if (cur_rx->bge_flags & BGE_RXBDFLAG_ERROR) {
				ifp->if_ierrors++;
				bge_newbuf_std(sc, sc->bge_std, m, dmamap);
				continue;
			}
			if (bge_newbuf_std(sc, sc->bge_std,
			    NULL, dmamap) == ENOBUFS) {
				ifp->if_ierrors++;
				bge_newbuf_std(sc, sc->bge_std, m, dmamap);
				continue;
			}
		}

		ifp->if_ipackets++;
#ifndef __NO_STRICT_ALIGNMENT
		/*
		 * XXX: if the 5701 PCIX-Rx-DMA workaround is in effect,
		 * the Rx buffer has the layer-2 header unaligned.
		 * If our CPU requires alignment, re-align by copying.
		 */
		if (sc->bge_flags & BGEF_RX_ALIGNBUG) {
			memmove(mtod(m, char *) + ETHER_ALIGN, m->m_data,
				cur_rx->bge_len);
			m->m_data += ETHER_ALIGN;
		}
#endif

		m->m_pkthdr.len = m->m_len = cur_rx->bge_len - ETHER_CRC_LEN;
		m->m_pkthdr.rcvif = ifp;

		/*
		 * Handle BPF listeners. Let the BPF user see the packet.
		 */
		bpf_mtap(ifp, m);

		bge_rxcsum(sc, cur_rx, m);

		/*
		 * If we received a packet with a vlan tag, pass it
		 * to vlan_input() instead of ether_input().
		 */
		if (cur_rx->bge_flags & BGE_RXBDFLAG_VLAN_TAG) {
			VLAN_INPUT_TAG(ifp, m, cur_rx->bge_vlan_tag, continue);
		}

		(*ifp->if_input)(ifp, m);
	}

	sc->bge_rx_saved_considx = rx_cons;
	bge_writembx(sc, BGE_MBX_RX_CONS0_LO, sc->bge_rx_saved_considx);
	if (stdcnt)
		bge_writembx(sc, BGE_MBX_RX_STD_PROD_LO, sc->bge_std);
	if (jumbocnt)
		bge_writembx(sc, BGE_MBX_RX_JUMBO_PROD_LO, sc->bge_jumbo);
}

static void
bge_rxcsum(struct bge_softc *sc, struct bge_rx_bd *cur_rx, struct mbuf *m)
{

	if (BGE_IS_57765_PLUS(sc)) {
		if ((cur_rx->bge_flags & BGE_RXBDFLAG_IPV6) == 0) {
			if ((cur_rx->bge_flags & BGE_RXBDFLAG_IP_CSUM) != 0)
				m->m_pkthdr.csum_flags = M_CSUM_IPv4;
			if ((cur_rx->bge_error_flag &
				BGE_RXERRFLAG_IP_CSUM_NOK) != 0)
				m->m_pkthdr.csum_flags |= M_CSUM_IPv4_BAD;
			if (cur_rx->bge_flags & BGE_RXBDFLAG_TCP_UDP_CSUM) {
				m->m_pkthdr.csum_data =
				    cur_rx->bge_tcp_udp_csum;
				m->m_pkthdr.csum_flags |=
				    (M_CSUM_TCPv4|M_CSUM_UDPv4|
					M_CSUM_DATA);
			}
		}
	} else {
		if ((cur_rx->bge_flags & BGE_RXBDFLAG_IP_CSUM) != 0)
			m->m_pkthdr.csum_flags = M_CSUM_IPv4;
		if ((cur_rx->bge_ip_csum ^ 0xffff) != 0)
			m->m_pkthdr.csum_flags |= M_CSUM_IPv4_BAD;
		/*
		 * Rx transport checksum-offload may also
		 * have bugs with packets which, when transmitted,
		 * were `runts' requiring padding.
		 */
		if (cur_rx->bge_flags & BGE_RXBDFLAG_TCP_UDP_CSUM &&
		    (/* (sc->_bge_quirks & BGE_QUIRK_SHORT_CKSUM_BUG) == 0 ||*/
			    m->m_pkthdr.len >= ETHER_MIN_NOPAD)) {
			m->m_pkthdr.csum_data =
			    cur_rx->bge_tcp_udp_csum;
			m->m_pkthdr.csum_flags |=
			    (M_CSUM_TCPv4|M_CSUM_UDPv4|
				M_CSUM_DATA);
		}
	}
}

static void
bge_txeof(struct bge_softc *sc)
{
	struct bge_tx_bd *cur_tx = NULL;
	struct ifnet *ifp;
	struct txdmamap_pool_entry *dma;
	bus_addr_t offset, toff;
	bus_size_t tlen;
	int tosync;
	struct mbuf *m;

	ifp = &sc->ethercom.ec_if;

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offsetof(struct bge_ring_data, bge_status_block),
	    sizeof (struct bge_status_block),
	    BUS_DMASYNC_POSTREAD);

	offset = offsetof(struct bge_ring_data, bge_tx_ring);
	tosync = sc->bge_rdata->bge_status_block.bge_idx[0].bge_tx_cons_idx -
	    sc->bge_tx_saved_considx;

	if (tosync != 0)
		rnd_add_uint32(&sc->rnd_source, tosync);

	toff = offset + (sc->bge_tx_saved_considx * sizeof (struct bge_tx_bd));

	if (tosync < 0) {
		tlen = (BGE_TX_RING_CNT - sc->bge_tx_saved_considx) *
		    sizeof (struct bge_tx_bd);
		bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
		    toff, tlen, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		tosync = -tosync;
	}

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offset, tosync * sizeof (struct bge_tx_bd),
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	/*
	 * Go through our tx ring and free mbufs for those
	 * frames that have been sent.
	 */
	while (sc->bge_tx_saved_considx !=
	    sc->bge_rdata->bge_status_block.bge_idx[0].bge_tx_cons_idx) {
		uint32_t		idx = 0;

		idx = sc->bge_tx_saved_considx;
		cur_tx = &sc->bge_rdata->bge_tx_ring[idx];
		if (cur_tx->bge_flags & BGE_TXBDFLAG_END)
			ifp->if_opackets++;
		m = sc->bge_cdata.bge_tx_chain[idx];
		if (m != NULL) {
			sc->bge_cdata.bge_tx_chain[idx] = NULL;
			dma = sc->txdma[idx];
			bus_dmamap_sync(sc->bge_dmatag, dma->dmamap, 0,
			    dma->dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->bge_dmatag, dma->dmamap);
			SLIST_INSERT_HEAD(&sc->txdma_list, dma, link);
			sc->txdma[idx] = NULL;

			m_freem(m);
		}
		sc->bge_txcnt--;
		BGE_INC(sc->bge_tx_saved_considx, BGE_TX_RING_CNT);
		ifp->if_timer = 0;
	}

	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;
}

static int
bge_intr(void *xsc)
{
	struct bge_softc *sc;
	struct ifnet *ifp;
	uint32_t pcistate, statusword, statustag;
	uint32_t intrmask = BGE_PCISTATE_INTR_NOT_ACTIVE;

	sc = xsc;
	ifp = &sc->ethercom.ec_if;

	/* 5717 and newer chips have no BGE_PCISTATE_INTR_NOT_ACTIVE bit */
	if (BGE_IS_5717_PLUS(sc))
		intrmask = 0;

	/* It is possible for the interrupt to arrive before
	 * the status block is updated prior to the interrupt.
	 * Reading the PCI State register will confirm whether the
	 * interrupt is ours and will flush the status block.
	 */
	pcistate = CSR_READ_4(sc, BGE_PCI_PCISTATE);

	/* read status word from status block */
	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offsetof(struct bge_ring_data, bge_status_block),
	    sizeof (struct bge_status_block),
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	statusword = sc->bge_rdata->bge_status_block.bge_status;
	statustag = sc->bge_rdata->bge_status_block.bge_status_tag << 24;

	if (sc->bge_flags & BGEF_TAGGED_STATUS) {
		if (sc->bge_lasttag == statustag &&
		    (~pcistate & intrmask)) {
			return (0);
		}
		sc->bge_lasttag = statustag;
	} else {
		if (!(statusword & BGE_STATFLAG_UPDATED) &&
		    !(~pcistate & intrmask)) {
			return (0);
		}
		statustag = 0;
	}
	/* Ack interrupt and stop others from occurring. */
	bge_writembx_flush(sc, BGE_MBX_IRQ0_LO, 1);
	BGE_EVCNT_INCR(sc->bge_ev_intr);

	/* clear status word */
	sc->bge_rdata->bge_status_block.bge_status = 0;

	bus_dmamap_sync(sc->bge_dmatag, sc->bge_ring_map,
	    offsetof(struct bge_ring_data, bge_status_block),
	    sizeof (struct bge_status_block),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700 ||
	    statusword & BGE_STATFLAG_LINKSTATE_CHANGED ||
	    BGE_STS_BIT(sc, BGE_STS_LINK_EVT))
		bge_link_upd(sc);

	if (ifp->if_flags & IFF_RUNNING) {
		/* Check RX return ring producer/consumer */
		bge_rxeof(sc);

		/* Check TX ring producer/consumer */
		bge_txeof(sc);
	}

	if (sc->bge_pending_rxintr_change) {
		uint32_t rx_ticks = sc->bge_rx_coal_ticks;
		uint32_t rx_bds = sc->bge_rx_max_coal_bds;

		CSR_WRITE_4(sc, BGE_HCC_RX_COAL_TICKS, rx_ticks);
		DELAY(10);
		(void)CSR_READ_4(sc, BGE_HCC_RX_COAL_TICKS);

		CSR_WRITE_4(sc, BGE_HCC_RX_MAX_COAL_BDS, rx_bds);
		DELAY(10);
		(void)CSR_READ_4(sc, BGE_HCC_RX_MAX_COAL_BDS);

		sc->bge_pending_rxintr_change = 0;
	}
	bge_handle_events(sc);

	/* Re-enable interrupts. */
	bge_writembx_flush(sc, BGE_MBX_IRQ0_LO, statustag);

	if (ifp->if_flags & IFF_RUNNING && !IFQ_IS_EMPTY(&ifp->if_snd))
		bge_start(ifp);

	return 1;
}

static void
bge_asf_driver_up(struct bge_softc *sc)
{
	if (sc->bge_asf_mode & ASF_STACKUP) {
		/* Send ASF heartbeat aprox. every 2s */
		if (sc->bge_asf_count)
			sc->bge_asf_count --;
		else {
			sc->bge_asf_count = 2;

			bge_wait_for_event_ack(sc);

			bge_writemem_ind(sc, BGE_SRAM_FW_CMD_MB,
			    BGE_FW_CMD_DRV_ALIVE3);
			bge_writemem_ind(sc, BGE_SRAM_FW_CMD_LEN_MB, 4);
			bge_writemem_ind(sc, BGE_SRAM_FW_CMD_DATA_MB,
			    BGE_FW_HB_TIMEOUT_SEC);
			CSR_WRITE_4_FLUSH(sc, BGE_RX_CPU_EVENT,
			    CSR_READ_4(sc, BGE_RX_CPU_EVENT) |
			    BGE_RX_CPU_DRV_EVENT);
		}
	}
}

static void
bge_tick(void *xsc)
{
	struct bge_softc *sc = xsc;
	struct mii_data *mii = &sc->bge_mii;
	int s;

	s = splnet();

	if (BGE_IS_5705_PLUS(sc))
		bge_stats_update_regs(sc);
	else
		bge_stats_update(sc);

	if (sc->bge_flags & BGEF_FIBER_TBI) {
		/*
		 * Since in TBI mode auto-polling can't be used we should poll
		 * link status manually. Here we register pending link event
		 * and trigger interrupt.
		 */
		BGE_STS_SETBIT(sc, BGE_STS_LINK_EVT);
		BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_INTR_SET);
	} else {
		/*
		 * Do not touch PHY if we have link up. This could break
		 * IPMI/ASF mode or produce extra input errors.
		 * (extra input errors was reported for bcm5701 & bcm5704).
		 */
		if (!BGE_STS_BIT(sc, BGE_STS_LINK))
			mii_tick(mii);
	}

	bge_asf_driver_up(sc);

	if (!sc->bge_detaching)
		callout_reset(&sc->bge_timeout, hz, bge_tick, sc);

	splx(s);
}

static void
bge_stats_update_regs(struct bge_softc *sc)
{
	struct ifnet *ifp = &sc->ethercom.ec_if;

	ifp->if_collisions += CSR_READ_4(sc, BGE_MAC_STATS +
	    offsetof(struct bge_mac_stats_regs, etherStatsCollisions));

	ifp->if_ierrors += CSR_READ_4(sc, BGE_RXLP_LOCSTAT_IFIN_DROPS);
	ifp->if_ierrors += CSR_READ_4(sc, BGE_RXLP_LOCSTAT_IFIN_ERRORS);
	ifp->if_ierrors += CSR_READ_4(sc, BGE_RXLP_LOCSTAT_OUT_OF_BDS);
}

static void
bge_stats_update(struct bge_softc *sc)
{
	struct ifnet *ifp = &sc->ethercom.ec_if;
	bus_size_t stats = BGE_MEMWIN_START + BGE_STATS_BLOCK;

#define READ_STAT(sc, stats, stat) \
	  CSR_READ_4(sc, stats + offsetof(struct bge_stats, stat))

	ifp->if_collisions +=
	  (READ_STAT(sc, stats, dot3StatsSingleCollisionFrames.bge_addr_lo) +
	   READ_STAT(sc, stats, dot3StatsMultipleCollisionFrames.bge_addr_lo) +
	   READ_STAT(sc, stats, dot3StatsExcessiveCollisions.bge_addr_lo) +
	   READ_STAT(sc, stats, dot3StatsLateCollisions.bge_addr_lo)) -
	  ifp->if_collisions;

	BGE_EVCNT_UPD(sc->bge_ev_tx_xoff,
		      READ_STAT(sc, stats, outXoffSent.bge_addr_lo));
	BGE_EVCNT_UPD(sc->bge_ev_tx_xon,
		      READ_STAT(sc, stats, outXonSent.bge_addr_lo));
	BGE_EVCNT_UPD(sc->bge_ev_rx_xoff,
		      READ_STAT(sc, stats,
		      		xoffPauseFramesReceived.bge_addr_lo));
	BGE_EVCNT_UPD(sc->bge_ev_rx_xon,
		      READ_STAT(sc, stats, xonPauseFramesReceived.bge_addr_lo));
	BGE_EVCNT_UPD(sc->bge_ev_rx_macctl,
		      READ_STAT(sc, stats,
		      		macControlFramesReceived.bge_addr_lo));
	BGE_EVCNT_UPD(sc->bge_ev_xoffentered,
		      READ_STAT(sc, stats, xoffStateEntered.bge_addr_lo));

#undef READ_STAT

#ifdef notdef
	ifp->if_collisions +=
	   (sc->bge_rdata->bge_info.bge_stats.dot3StatsSingleCollisionFrames +
	   sc->bge_rdata->bge_info.bge_stats.dot3StatsMultipleCollisionFrames +
	   sc->bge_rdata->bge_info.bge_stats.dot3StatsExcessiveCollisions +
	   sc->bge_rdata->bge_info.bge_stats.dot3StatsLateCollisions) -
	   ifp->if_collisions;
#endif
}

/*
 * Pad outbound frame to ETHER_MIN_NOPAD for an unusual reason.
 * The bge hardware will pad out Tx runts to ETHER_MIN_NOPAD,
 * but when such padded frames employ the  bge IP/TCP checksum offload,
 * the hardware checksum assist gives incorrect results (possibly
 * from incorporating its own padding into the UDP/TCP checksum; who knows).
 * If we pad such runts with zeros, the onboard checksum comes out correct.
 */
static inline int
bge_cksum_pad(struct mbuf *pkt)
{
	struct mbuf *last = NULL;
	int padlen;

	padlen = ETHER_MIN_NOPAD - pkt->m_pkthdr.len;

	/* if there's only the packet-header and we can pad there, use it. */
	if (pkt->m_pkthdr.len == pkt->m_len &&
	    M_TRAILINGSPACE(pkt) >= padlen) {
		last = pkt;
	} else {
		/*
		 * Walk packet chain to find last mbuf. We will either
		 * pad there, or append a new mbuf and pad it
		 * (thus perhaps avoiding the bcm5700 dma-min bug).
		 */
		for (last = pkt; last->m_next != NULL; last = last->m_next) {
	      	       continue; /* do nothing */
		}

		/* `last' now points to last in chain. */
		if (M_TRAILINGSPACE(last) < padlen) {
			/* Allocate new empty mbuf, pad it. Compact later. */
			struct mbuf *n;
			MGET(n, M_DONTWAIT, MT_DATA);
			if (n == NULL)
				return ENOBUFS;
			n->m_len = 0;
			last->m_next = n;
			last = n;
		}
	}

	KDASSERT(!M_READONLY(last));
	KDASSERT(M_TRAILINGSPACE(last) >= padlen);

	/* Now zero the pad area, to avoid the bge cksum-assist bug */
	memset(mtod(last, char *) + last->m_len, 0, padlen);
	last->m_len += padlen;
	pkt->m_pkthdr.len += padlen;
	return 0;
}

/*
 * Compact outbound packets to avoid bug with DMA segments less than 8 bytes.
 */
static inline int
bge_compact_dma_runt(struct mbuf *pkt)
{
	struct mbuf	*m, *prev;
	int 		totlen;

	prev = NULL;
	totlen = 0;

	for (m = pkt; m != NULL; prev = m,m = m->m_next) {
		int mlen = m->m_len;
		int shortfall = 8 - mlen ;

		totlen += mlen;
		if (mlen == 0)
			continue;
		if (mlen >= 8)
			continue;

		/* If we get here, mbuf data is too small for DMA engine.
		 * Try to fix by shuffling data to prev or next in chain.
		 * If that fails, do a compacting deep-copy of the whole chain.
		 */

		/* Internal frag. If fits in prev, copy it there. */
		if (prev && M_TRAILINGSPACE(prev) >= m->m_len) {
		  	memcpy(prev->m_data + prev->m_len, m->m_data, mlen);
			prev->m_len += mlen;
			m->m_len = 0;
			/* XXX stitch chain */
			prev->m_next = m_free(m);
			m = prev;
			continue;
		}
		else if (m->m_next != NULL &&
			     M_TRAILINGSPACE(m) >= shortfall &&
			     m->m_next->m_len >= (8 + shortfall)) {
		    /* m is writable and have enough data in next, pull up. */

		  	memcpy(m->m_data + m->m_len, m->m_next->m_data,
			    shortfall);
			m->m_len += shortfall;
			m->m_next->m_len -= shortfall;
			m->m_next->m_data += shortfall;
		}
		else if (m->m_next == NULL || 1) {
		  	/* Got a runt at the very end of the packet.
			 * borrow data from the tail of the preceding mbuf and
			 * update its length in-place. (The original data is still
			 * valid, so we can do this even if prev is not writable.)
			 */

			/* if we'd make prev a runt, just move all of its data. */
			KASSERT(prev != NULL /*, ("runt but null PREV")*/);
			KASSERT(prev->m_len >= 8 /*, ("runt prev")*/);

			if ((prev->m_len - shortfall) < 8)
				shortfall = prev->m_len;

#ifdef notyet	/* just do the safe slow thing for now */
			if (!M_READONLY(m)) {
				if (M_LEADINGSPACE(m) < shorfall) {
					void *m_dat;
					m_dat = (m->m_flags & M_PKTHDR) ?
					  m->m_pktdat : m->dat;
					memmove(m_dat, mtod(m, void*), m->m_len);
					m->m_data = m_dat;
				    }
			} else
#endif	/* just do the safe slow thing */
			{
				struct mbuf * n = NULL;
				int newprevlen = prev->m_len - shortfall;

				MGET(n, M_NOWAIT, MT_DATA);
				if (n == NULL)
				   return ENOBUFS;
				KASSERT(m->m_len + shortfall < MLEN
					/*,
					  ("runt %d +prev %d too big\n", m->m_len, shortfall)*/);

				/* first copy the data we're stealing from prev */
				memcpy(n->m_data, prev->m_data + newprevlen,
				    shortfall);

				/* update prev->m_len accordingly */
				prev->m_len -= shortfall;

				/* copy data from runt m */
				memcpy(n->m_data + shortfall, m->m_data,
				    m->m_len);

				/* n holds what we stole from prev, plus m */
				n->m_len = shortfall + m->m_len;

				/* stitch n into chain and free m */
				n->m_next = m->m_next;
				prev->m_next = n;
				/* KASSERT(m->m_next == NULL); */
				m->m_next = NULL;
				m_free(m);
				m = n;	/* for continuing loop */
			}
		}
	}
	return 0;
}

/*
 * Encapsulate an mbuf chain in the tx ring by coupling the mbuf data
 * pointers to descriptors.
 */
static int
bge_encap(struct bge_softc *sc, struct mbuf *m_head, uint32_t *txidx)
{
	struct bge_tx_bd	*f = NULL;
	uint32_t		frag, cur;
	uint16_t		csum_flags = 0;
	uint16_t		txbd_tso_flags = 0;
	struct txdmamap_pool_entry *dma;
	bus_dmamap_t dmamap;
	int			i = 0;
	struct m_tag		*mtag;
	int			use_tso, maxsegsize, error;

	cur = frag = *txidx;

	if (m_head->m_pkthdr.csum_flags) {
		if (m_head->m_pkthdr.csum_flags & M_CSUM_IPv4)
			csum_flags |= BGE_TXBDFLAG_IP_CSUM;
		if (m_head->m_pkthdr.csum_flags & (M_CSUM_TCPv4|M_CSUM_UDPv4))
			csum_flags |= BGE_TXBDFLAG_TCP_UDP_CSUM;
	}

	/*
	 * If we were asked to do an outboard checksum, and the NIC
	 * has the bug where it sometimes adds in the Ethernet padding,
	 * explicitly pad with zeros so the cksum will be correct either way.
	 * (For now, do this for all chip versions, until newer
	 * are confirmed to not require the workaround.)
	 */
	if ((csum_flags & BGE_TXBDFLAG_TCP_UDP_CSUM) == 0 ||
#ifdef notyet
	    (sc->bge_quirks & BGE_QUIRK_SHORT_CKSUM_BUG) == 0 ||
#endif
	    m_head->m_pkthdr.len >= ETHER_MIN_NOPAD)
		goto check_dma_bug;

	if (bge_cksum_pad(m_head) != 0)
	    return ENOBUFS;

check_dma_bug:
	if (!(BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5700_BX))
		goto doit;

	/*
	 * bcm5700 Revision B silicon cannot handle DMA descriptors with
	 * less than eight bytes.  If we encounter a teeny mbuf
	 * at the end of a chain, we can pad.  Otherwise, copy.
	 */
	if (bge_compact_dma_runt(m_head) != 0)
		return ENOBUFS;

doit:
	dma = SLIST_FIRST(&sc->txdma_list);
	if (dma == NULL)
		return ENOBUFS;
	dmamap = dma->dmamap;

	/*
	 * Set up any necessary TSO state before we start packing...
	 */
	use_tso = (m_head->m_pkthdr.csum_flags & M_CSUM_TSOv4) != 0;
	if (!use_tso) {
		maxsegsize = 0;
	} else {	/* TSO setup */
		unsigned  mss;
		struct ether_header *eh;
		unsigned ip_tcp_hlen, iptcp_opt_words, tcp_seg_flags, offset;
		struct mbuf * m0 = m_head;
		struct ip *ip;
		struct tcphdr *th;
		int iphl, hlen;

		/*
		 * XXX It would be nice if the mbuf pkthdr had offset
		 * fields for the protocol headers.
		 */

		eh = mtod(m0, struct ether_header *);
		switch (htons(eh->ether_type)) {
		case ETHERTYPE_IP:
			offset = ETHER_HDR_LEN;
			break;

		case ETHERTYPE_VLAN:
			offset = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
			break;

		default:
			/*
			 * Don't support this protocol or encapsulation.
			 */
			return ENOBUFS;
		}

		/*
		 * TCP/IP headers are in the first mbuf; we can do
		 * this the easy way.
		 */
		iphl = M_CSUM_DATA_IPv4_IPHL(m0->m_pkthdr.csum_data);
		hlen = iphl + offset;
		if (__predict_false(m0->m_len <
				    (hlen + sizeof(struct tcphdr)))) {

			aprint_debug_dev(sc->bge_dev,
			    "TSO: hard case m0->m_len == %d < ip/tcp hlen %zd,"
			    "not handled yet\n",
			     m0->m_len, hlen+ sizeof(struct tcphdr));
#ifdef NOTYET
			/*
			 * XXX jonathan@NetBSD.org: untested.
			 * how to force  this branch to be taken?
			 */
			BGE_EVCNT_INCR(sc->bge_ev_txtsopain);

			m_copydata(m0, offset, sizeof(ip), &ip);
			m_copydata(m0, hlen, sizeof(th), &th);

			ip.ip_len = 0;

			m_copyback(m0, hlen + offsetof(struct ip, ip_len),
			    sizeof(ip.ip_len), &ip.ip_len);

			th.th_sum = in_cksum_phdr(ip.ip_src.s_addr,
			    ip.ip_dst.s_addr, htons(IPPROTO_TCP));

			m_copyback(m0, hlen + offsetof(struct tcphdr, th_sum),
			    sizeof(th.th_sum), &th.th_sum);

			hlen += th.th_off << 2;
			iptcp_opt_words	= hlen;
#else
			/*
			 * if_wm "hard" case not yet supported, can we not
			 * mandate it out of existence?
			 */
			(void) ip; (void)th; (void) ip_tcp_hlen;

			return ENOBUFS;
#endif
		} else {
			ip = (struct ip *) (mtod(m0, char *) + offset);
			th = (struct tcphdr *) (mtod(m0, char *) + hlen);
			ip_tcp_hlen = iphl +  (th->th_off << 2);

			/* Total IP/TCP options, in 32-bit words */
			iptcp_opt_words = (ip_tcp_hlen
					   - sizeof(struct tcphdr)
					   - sizeof(struct ip)) >> 2;
		}
		if (BGE_IS_575X_PLUS(sc)) {
			th->th_sum = 0;
			csum_flags &= ~(BGE_TXBDFLAG_TCP_UDP_CSUM);
		} else {
			/*
			 * XXX jonathan@NetBSD.org: 5705 untested.
			 * Requires TSO firmware patch for 5701/5703/5704.
			 */
			th->th_sum = in_cksum_phdr(ip->ip_src.s_addr,
			    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
		}

		mss = m_head->m_pkthdr.segsz;
		txbd_tso_flags |=
		    BGE_TXBDFLAG_CPU_PRE_DMA |
		    BGE_TXBDFLAG_CPU_POST_DMA;

		/*
		 * Our NIC TSO-assist assumes TSO has standard, optionless
		 * IPv4 and TCP headers, which total 40 bytes. By default,
		 * the NIC copies 40 bytes of IP/TCP header from the
		 * supplied header into the IP/TCP header portion of
		 * each post-TSO-segment. If the supplied packet has IP or
		 * TCP options, we need to tell the NIC to copy those extra
		 * bytes into each  post-TSO header, in addition to the normal
		 * 40-byte IP/TCP header (and to leave space accordingly).
		 * Unfortunately, the driver encoding of option length
		 * varies across different ASIC families.
		 */
		tcp_seg_flags = 0;
		if (iptcp_opt_words) {
			if (BGE_IS_5705_PLUS(sc)) {
				tcp_seg_flags =
					iptcp_opt_words << 11;
			} else {
				txbd_tso_flags |=
					iptcp_opt_words << 12;
			}
		}
		maxsegsize = mss | tcp_seg_flags;
		ip->ip_len = htons(mss + ip_tcp_hlen);

	}	/* TSO setup */

	/*
	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
	 * of fragments or hit the end of the mbuf chain.
	 */
	error = bus_dmamap_load_mbuf(sc->bge_dmatag, dmamap, m_head,
	    BUS_DMA_NOWAIT);
	if (error)
		return ENOBUFS;
	/*
	 * Sanity check: avoid coming within 16 descriptors
	 * of the end of the ring.
	 */
	if (dmamap->dm_nsegs > (BGE_TX_RING_CNT - sc->bge_txcnt - 16)) {
		BGE_TSO_PRINTF(("%s: "
		    " dmamap_load_mbuf too close to ring wrap\n",
		    device_xname(sc->bge_dev)));
		goto fail_unload;
	}

	mtag = sc->ethercom.ec_nvlans ?
	    m_tag_find(m_head, PACKET_TAG_VLAN, NULL) : NULL;


	/* Iterate over dmap-map fragments. */
	for (i = 0; i < dmamap->dm_nsegs; i++) {
		f = &sc->bge_rdata->bge_tx_ring[frag];
		if (sc->bge_cdata.bge_tx_chain[frag] != NULL)
			break;

		BGE_HOSTADDR(f->bge_addr, dmamap->dm_segs[i].ds_addr);
		f->bge_len = dmamap->dm_segs[i].ds_len;

		/*
		 * For 5751 and follow-ons, for TSO we must turn
		 * off checksum-assist flag in the tx-descr, and
		 * supply the ASIC-revision-specific encoding
		 * of TSO flags and segsize.
		 */
		if (use_tso) {
			if (BGE_IS_575X_PLUS(sc) || i == 0) {
				f->bge_rsvd = maxsegsize;
				f->bge_flags = csum_flags | txbd_tso_flags;
			} else {
				f->bge_rsvd = 0;
				f->bge_flags =
				  (csum_flags | txbd_tso_flags) & 0x0fff;
			}
		} else {
			f->bge_rsvd = 0;
			f->bge_flags = csum_flags;
		}

		if (mtag != NULL) {
			f->bge_flags |= BGE_TXBDFLAG_VLAN_TAG;
			f->bge_vlan_tag = VLAN_TAG_VALUE(mtag);
		} else {
			f->bge_vlan_tag = 0;
		}
		cur = frag;
		BGE_INC(frag, BGE_TX_RING_CNT);
	}

	if (i < dmamap->dm_nsegs) {
		BGE_TSO_PRINTF(("%s: reached %d < dm_nsegs %d\n",
		    device_xname(sc->bge_dev), i, dmamap->dm_nsegs));
		goto fail_unload;
	}

	bus_dmamap_sync(sc->bge_dmatag, dmamap, 0, dmamap->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	if (frag == sc->bge_tx_saved_considx) {
		BGE_TSO_PRINTF(("%s: frag %d = wrapped id %d?\n",
		    device_xname(sc->bge_dev), frag, sc->bge_tx_saved_considx));

		goto fail_unload;
	}

	sc->bge_rdata->bge_tx_ring[cur].bge_flags |= BGE_TXBDFLAG_END;
	sc->bge_cdata.bge_tx_chain[cur] = m_head;
	SLIST_REMOVE_HEAD(&sc->txdma_list, link);
	sc->txdma[cur] = dma;
	sc->bge_txcnt += dmamap->dm_nsegs;

	*txidx = frag;

	return 0;

fail_unload:
	bus_dmamap_unload(sc->bge_dmatag, dmamap);

	return ENOBUFS;
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit descriptors.
 */
static void
bge_start(struct ifnet *ifp)
{
	struct bge_softc *sc;
	struct mbuf *m_head = NULL;
	uint32_t prodidx;
	int pkts = 0;

	sc = ifp->if_softc;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	prodidx = sc->bge_tx_prodidx;

	while (sc->bge_cdata.bge_tx_chain[prodidx] == NULL) {
		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

#if 0
		/*
		 * XXX
		 * safety overkill.  If this is a fragmented packet chain
		 * with delayed TCP/UDP checksums, then only encapsulate
		 * it if we have enough descriptors to handle the entire
		 * chain at once.
		 * (paranoia -- may not actually be needed)
		 */
		if (m_head->m_flags & M_FIRSTFRAG &&
		    m_head->m_pkthdr.csum_flags & (CSUM_DELAY_DATA)) {
			if ((BGE_TX_RING_CNT - sc->bge_txcnt) <
			    M_CSUM_DATA_IPv4_OFFSET(m_head->m_pkthdr.csum_data) + 16) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
		}
#endif

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (bge_encap(sc, m_head, &prodidx)) {
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
	bge_writembx(sc, BGE_MBX_TX_HOST_PROD0_LO, prodidx);
	/* 5700 b2 errata */
	if (BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5700_BX)
		bge_writembx(sc, BGE_MBX_TX_HOST_PROD0_LO, prodidx);

	sc->bge_tx_prodidx = prodidx;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
}

static int
bge_init(struct ifnet *ifp)
{
	struct bge_softc *sc = ifp->if_softc;
	const uint16_t *m;
	uint32_t mode, reg;
	int s, error = 0;

	s = splnet();

	ifp = &sc->ethercom.ec_if;

	/* Cancel pending I/O and flush buffers. */
	bge_stop(ifp, 0);

	bge_stop_fw(sc);
	bge_sig_pre_reset(sc, BGE_RESET_START);
	bge_reset(sc);
	bge_sig_legacy(sc, BGE_RESET_START);

	if (BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5784_AX) {
		reg = CSR_READ_4(sc, BGE_CPMU_CTRL);
		reg &= ~(BGE_CPMU_CTRL_LINK_AWARE_MODE |
		    BGE_CPMU_CTRL_LINK_IDLE_MODE);
		CSR_WRITE_4(sc, BGE_CPMU_CTRL, reg);

		reg = CSR_READ_4(sc, BGE_CPMU_LSPD_10MB_CLK);
		reg &= ~BGE_CPMU_LSPD_10MB_CLK;
		reg |= BGE_CPMU_LSPD_10MB_MACCLK_6_25;
		CSR_WRITE_4(sc, BGE_CPMU_LSPD_10MB_CLK, reg);

		reg = CSR_READ_4(sc, BGE_CPMU_LNK_AWARE_PWRMD);
		reg &= ~BGE_CPMU_LNK_AWARE_MACCLK_MASK;
		reg |= BGE_CPMU_LNK_AWARE_MACCLK_6_25;
		CSR_WRITE_4(sc, BGE_CPMU_LNK_AWARE_PWRMD, reg);

		reg = CSR_READ_4(sc, BGE_CPMU_HST_ACC);
		reg &= ~BGE_CPMU_HST_ACC_MACCLK_MASK;
		reg |= BGE_CPMU_HST_ACC_MACCLK_6_25;
		CSR_WRITE_4(sc, BGE_CPMU_HST_ACC, reg);
	}

	bge_sig_post_reset(sc, BGE_RESET_START);

	bge_chipinit(sc);

	/*
	 * Init the various state machines, ring
	 * control blocks and firmware.
	 */
	error = bge_blockinit(sc);
	if (error != 0) {
		aprint_error_dev(sc->bge_dev, "initialization error %d\n",
		    error);
		splx(s);
		return error;
	}

	ifp = &sc->ethercom.ec_if;

	/* 5718 step 25, 57XX step 54 */
	/* Specify MTU. */
	CSR_WRITE_4(sc, BGE_RX_MTU, ifp->if_mtu +
	    ETHER_HDR_LEN + ETHER_CRC_LEN + ETHER_VLAN_ENCAP_LEN);

	/* 5718 step 23 */
	/* Load our MAC address. */
	m = (const uint16_t *)&(CLLADDR(ifp->if_sadl)[0]);
	CSR_WRITE_4(sc, BGE_MAC_ADDR1_LO, htons(m[0]));
	CSR_WRITE_4(sc, BGE_MAC_ADDR1_HI, (htons(m[1]) << 16) | htons(m[2]));

	/* Enable or disable promiscuous mode as needed. */
	if (ifp->if_flags & IFF_PROMISC)
		BGE_SETBIT(sc, BGE_RX_MODE, BGE_RXMODE_RX_PROMISC);
	else
		BGE_CLRBIT(sc, BGE_RX_MODE, BGE_RXMODE_RX_PROMISC);

	/* Program multicast filter. */
	bge_setmulti(sc);

	/* Init RX ring. */
	bge_init_rx_ring_std(sc);

	/*
	 * Workaround for a bug in 5705 ASIC rev A0. Poll the NIC's
	 * memory to insure that the chip has in fact read the first
	 * entry of the ring.
	 */
	if (sc->bge_chipid == BGE_CHIPID_BCM5705_A0) {
		uint32_t		v, i;
		for (i = 0; i < 10; i++) {
			DELAY(20);
			v = bge_readmem_ind(sc, BGE_STD_RX_RINGS + 8);
			if (v == (MCLBYTES - ETHER_ALIGN))
				break;
		}
		if (i == 10)
			aprint_error_dev(sc->bge_dev,
			    "5705 A0 chip failed to load RX ring\n");
	}

	/* Init jumbo RX ring. */
	if (ifp->if_mtu > (ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN))
		bge_init_rx_ring_jumbo(sc);

	/* Init our RX return ring index */
	sc->bge_rx_saved_considx = 0;

	/* Init TX ring. */
	bge_init_tx_ring(sc);

	/* 5718 step 63, 57XX step 94 */
	/* Enable TX MAC state machine lockup fix. */
	mode = CSR_READ_4(sc, BGE_TX_MODE);
	if (BGE_IS_5755_PLUS(sc) ||
	    BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906)
		mode |= BGE_TXMODE_MBUF_LOCKUP_FIX;
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5720) {
		mode &= ~(BGE_TXMODE_JMB_FRM_LEN | BGE_TXMODE_CNT_DN_MODE);
		mode |= CSR_READ_4(sc, BGE_TX_MODE) &
		    (BGE_TXMODE_JMB_FRM_LEN | BGE_TXMODE_CNT_DN_MODE);
	}

	/* Turn on transmitter */
	CSR_WRITE_4_FLUSH(sc, BGE_TX_MODE, mode | BGE_TXMODE_ENABLE);
	/* 5718 step 64 */
	DELAY(100);

	/* 5718 step 65, 57XX step 95 */
	/* Turn on receiver */
	mode = CSR_READ_4(sc, BGE_RX_MODE);
	if (BGE_IS_5755_PLUS(sc))
		mode |= BGE_RXMODE_IPV6_ENABLE;
	CSR_WRITE_4_FLUSH(sc, BGE_RX_MODE, mode | BGE_RXMODE_ENABLE);
	/* 5718 step 66 */
	DELAY(10);

	/* 5718 step 12, 57XX step 37 */
	/*
	 * XXX Doucments of 5718 series and 577xx say the recommended value
	 * is 1, but tg3 set 1 only on 57765 series.
	 */
	if (BGE_IS_57765_PLUS(sc))
		reg = 1;
	else
		reg = 2;
	CSR_WRITE_4_FLUSH(sc, BGE_MAX_RX_FRAME_LOWAT, reg);

	/* Tell firmware we're alive. */
	BGE_SETBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);

	/* Enable host interrupts. */
	BGE_SETBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_CLEAR_INTA);
	BGE_CLRBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_MASK_PCI_INTR);
	bge_writembx_flush(sc, BGE_MBX_IRQ0_LO, 0);

	if ((error = bge_ifmedia_upd(ifp)) != 0)
		goto out;

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	callout_reset(&sc->bge_timeout, hz, bge_tick, sc);

out:
	sc->bge_if_flags = ifp->if_flags;
	splx(s);

	return error;
}

/*
 * Set media options.
 */
static int
bge_ifmedia_upd(struct ifnet *ifp)
{
	struct bge_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->bge_mii;
	struct ifmedia *ifm = &sc->bge_ifmedia;
	int rc;

	/* If this is a 1000baseX NIC, enable the TBI port. */
	if (sc->bge_flags & BGEF_FIBER_TBI) {
		if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
			return EINVAL;
		switch (IFM_SUBTYPE(ifm->ifm_media)) {
		case IFM_AUTO:
			/*
			 * The BCM5704 ASIC appears to have a special
			 * mechanism for programming the autoneg
			 * advertisement registers in TBI mode.
			 */
			if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5704) {
				uint32_t sgdig;
				sgdig = CSR_READ_4(sc, BGE_SGDIG_STS);
				if (sgdig & BGE_SGDIGSTS_DONE) {
					CSR_WRITE_4(sc, BGE_TX_TBI_AUTONEG, 0);
					sgdig = CSR_READ_4(sc, BGE_SGDIG_CFG);
					sgdig |= BGE_SGDIGCFG_AUTO |
					    BGE_SGDIGCFG_PAUSE_CAP |
					    BGE_SGDIGCFG_ASYM_PAUSE;
					CSR_WRITE_4_FLUSH(sc, BGE_SGDIG_CFG,
					    sgdig | BGE_SGDIGCFG_SEND);
					DELAY(5);
					CSR_WRITE_4_FLUSH(sc, BGE_SGDIG_CFG,
					    sgdig);
				}
			}
			break;
		case IFM_1000_SX:
			if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX) {
				BGE_CLRBIT(sc, BGE_MAC_MODE,
				    BGE_MACMODE_HALF_DUPLEX);
			} else {
				BGE_SETBIT(sc, BGE_MAC_MODE,
				    BGE_MACMODE_HALF_DUPLEX);
			}
			DELAY(40);
			break;
		default:
			return EINVAL;
		}
		/* XXX 802.3x flow control for 1000BASE-SX */
		return 0;
	}

	if ((BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5784) &&
	    (BGE_CHIPREV(sc->bge_chipid) != BGE_CHIPREV_5784_AX)) {
		uint32_t reg;

		reg = CSR_READ_4(sc, BGE_CPMU_CTRL);
		if ((reg & BGE_CPMU_CTRL_GPHY_10MB_RXONLY) != 0) {
			reg &= ~BGE_CPMU_CTRL_GPHY_10MB_RXONLY;
			CSR_WRITE_4(sc, BGE_CPMU_CTRL, reg);
		}
	}

	BGE_STS_SETBIT(sc, BGE_STS_LINK_EVT);
	if ((rc = mii_mediachg(mii)) == ENXIO)
		return 0;

	if (BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5784_AX) {
		uint32_t reg;

		reg = CSR_READ_4(sc, BGE_CPMU_LSPD_1000MB_CLK);
		if ((reg & BGE_CPMU_LSPD_1000MB_MACCLK_MASK)
		    == (BGE_CPMU_LSPD_1000MB_MACCLK_12_5)) {
			reg &= ~BGE_CPMU_LSPD_1000MB_MACCLK_MASK;
			delay(40);
			CSR_WRITE_4(sc, BGE_CPMU_LSPD_1000MB_CLK, reg);
		}
	}

	/*
	 * Force an interrupt so that we will call bge_link_upd
	 * if needed and clear any pending link state attention.
	 * Without this we are not getting any further interrupts
	 * for link state changes and thus will not UP the link and
	 * not be able to send in bge_start. The only way to get
	 * things working was to receive a packet and get a RX intr.
	 */
	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700 ||
	    sc->bge_flags & BGEF_IS_5788)
		BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_INTR_SET);
	else
		BGE_SETBIT(sc, BGE_HCC_MODE, BGE_HCCMODE_COAL_NOW);

	return rc;
}

/*
 * Report current media status.
 */
static void
bge_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct bge_softc *sc = ifp->if_softc;
	struct mii_data *mii = &sc->bge_mii;

	if (sc->bge_flags & BGEF_FIBER_TBI) {
		ifmr->ifm_status = IFM_AVALID;
		ifmr->ifm_active = IFM_ETHER;
		if (CSR_READ_4(sc, BGE_MAC_STS) &
		    BGE_MACSTAT_TBI_PCS_SYNCHED)
			ifmr->ifm_status |= IFM_ACTIVE;
		ifmr->ifm_active |= IFM_1000_SX;
		if (CSR_READ_4(sc, BGE_MAC_MODE) & BGE_MACMODE_HALF_DUPLEX)
			ifmr->ifm_active |= IFM_HDX;
		else
			ifmr->ifm_active |= IFM_FDX;
		return;
	}

	mii_pollstat(mii);
	ifmr->ifm_status = mii->mii_media_status;
	ifmr->ifm_active = (mii->mii_media_active & ~IFM_ETH_FMASK) |
	    sc->bge_flowflags;
}

static int
bge_ifflags_cb(struct ethercom *ec)
{
	struct ifnet *ifp = &ec->ec_if;
	struct bge_softc *sc = ifp->if_softc;
	int change = ifp->if_flags ^ sc->bge_if_flags;

	if ((change & ~(IFF_CANTCHANGE|IFF_DEBUG)) != 0)
		return ENETRESET;
	else if ((change & (IFF_PROMISC | IFF_ALLMULTI)) == 0)
		return 0;

	if ((ifp->if_flags & IFF_PROMISC) == 0)
		BGE_CLRBIT(sc, BGE_RX_MODE, BGE_RXMODE_RX_PROMISC);
	else
		BGE_SETBIT(sc, BGE_RX_MODE, BGE_RXMODE_RX_PROMISC);

	bge_setmulti(sc);

	sc->bge_if_flags = ifp->if_flags;
	return 0;
}

static int
bge_ioctl(struct ifnet *ifp, u_long command, void *data)
{
	struct bge_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	int s, error = 0;
	struct mii_data *mii;

	s = splnet();

	switch (command) {
	case SIOCSIFMEDIA:
		/* XXX Flow control is not supported for 1000BASE-SX */
		if (sc->bge_flags & BGEF_FIBER_TBI) {
			ifr->ifr_media &= ~IFM_ETH_FMASK;
			sc->bge_flowflags = 0;
		}

		/* Flow control requires full-duplex mode. */
		if (IFM_SUBTYPE(ifr->ifr_media) == IFM_AUTO ||
		    (ifr->ifr_media & IFM_FDX) == 0) {
		    	ifr->ifr_media &= ~IFM_ETH_FMASK;
		}
		if (IFM_SUBTYPE(ifr->ifr_media) != IFM_AUTO) {
			if ((ifr->ifr_media & IFM_ETH_FMASK) == IFM_FLOW) {
				/* We can do both TXPAUSE and RXPAUSE. */
				ifr->ifr_media |=
				    IFM_ETH_TXPAUSE | IFM_ETH_RXPAUSE;
			}
			sc->bge_flowflags = ifr->ifr_media & IFM_ETH_FMASK;
		}
		/* FALLTHROUGH */
	case SIOCGIFMEDIA:
		if (sc->bge_flags & BGEF_FIBER_TBI) {
			error = ifmedia_ioctl(ifp, ifr, &sc->bge_ifmedia,
			    command);
		} else {
			mii = &sc->bge_mii;
			error = ifmedia_ioctl(ifp, ifr, &mii->mii_media,
			    command);
		}
		break;
	default:
		if ((error = ether_ioctl(ifp, command, data)) != ENETRESET)
			break;

		error = 0;

		if (command != SIOCADDMULTI && command != SIOCDELMULTI)
			;
		else if (ifp->if_flags & IFF_RUNNING)
			bge_setmulti(sc);
		break;
	}

	splx(s);

	return error;
}

static void
bge_watchdog(struct ifnet *ifp)
{
	struct bge_softc *sc;

	sc = ifp->if_softc;

	aprint_error_dev(sc->bge_dev, "watchdog timeout -- resetting\n");

	ifp->if_flags &= ~IFF_RUNNING;
	bge_init(ifp);

	ifp->if_oerrors++;
}

static void
bge_stop_block(struct bge_softc *sc, bus_addr_t reg, uint32_t bit)
{
	int i;

	BGE_CLRBIT_FLUSH(sc, reg, bit);

	for (i = 0; i < 1000; i++) {
		delay(100);
		if ((CSR_READ_4(sc, reg) & bit) == 0)
			return;
	}

	/*
	 * Doesn't print only when the register is BGE_SRS_MODE. It occurs
	 * on some environment (and once after boot?)
	 */
	if (reg != BGE_SRS_MODE)
		aprint_error_dev(sc->bge_dev,
		    "block failed to stop: reg 0x%lx, bit 0x%08x\n",
		    (u_long)reg, bit);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
bge_stop(struct ifnet *ifp, int disable)
{
	struct bge_softc *sc = ifp->if_softc;

	if (disable) {
		sc->bge_detaching = 1;
		callout_halt(&sc->bge_timeout, NULL);
	} else
		callout_stop(&sc->bge_timeout);

	/* Disable host interrupts. */
	BGE_SETBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_MASK_PCI_INTR);
	bge_writembx_flush(sc, BGE_MBX_IRQ0_LO, 1);

	/*
	 * Tell firmware we're shutting down.
	 */
	bge_stop_fw(sc);
	bge_sig_pre_reset(sc, BGE_RESET_SHUTDOWN);

	/*
	 * Disable all of the receiver blocks.
	 */
	bge_stop_block(sc, BGE_RX_MODE, BGE_RXMODE_ENABLE);
	bge_stop_block(sc, BGE_RBDI_MODE, BGE_RBDIMODE_ENABLE);
	bge_stop_block(sc, BGE_RXLP_MODE, BGE_RXLPMODE_ENABLE);
	if (BGE_IS_5700_FAMILY(sc))
		bge_stop_block(sc, BGE_RXLS_MODE, BGE_RXLSMODE_ENABLE);
	bge_stop_block(sc, BGE_RDBDI_MODE, BGE_RBDIMODE_ENABLE);
	bge_stop_block(sc, BGE_RDC_MODE, BGE_RDCMODE_ENABLE);
	bge_stop_block(sc, BGE_RBDC_MODE, BGE_RBDCMODE_ENABLE);

	/*
	 * Disable all of the transmit blocks.
	 */
	bge_stop_block(sc, BGE_SRS_MODE, BGE_SRSMODE_ENABLE);
	bge_stop_block(sc, BGE_SBDI_MODE, BGE_SBDIMODE_ENABLE);
	bge_stop_block(sc, BGE_SDI_MODE, BGE_SDIMODE_ENABLE);
	bge_stop_block(sc, BGE_RDMA_MODE, BGE_RDMAMODE_ENABLE);
	bge_stop_block(sc, BGE_SDC_MODE, BGE_SDCMODE_ENABLE);
	if (BGE_IS_5700_FAMILY(sc))
		bge_stop_block(sc, BGE_DMAC_MODE, BGE_DMACMODE_ENABLE);
	bge_stop_block(sc, BGE_SBDC_MODE, BGE_SBDCMODE_ENABLE);

	BGE_CLRBIT_FLUSH(sc, BGE_MAC_MODE, BGE_MACMODE_TXDMA_ENB);
	delay(40);

	bge_stop_block(sc, BGE_TX_MODE, BGE_TXMODE_ENABLE);

	/*
	 * Shut down all of the memory managers and related
	 * state machines.
	 */
	/* 5718 step 5a,5b */
	bge_stop_block(sc, BGE_HCC_MODE, BGE_HCCMODE_ENABLE);
	bge_stop_block(sc, BGE_WDMA_MODE, BGE_WDMAMODE_ENABLE);
	if (BGE_IS_5700_FAMILY(sc))
		bge_stop_block(sc, BGE_MBCF_MODE, BGE_MBCFMODE_ENABLE);

	/* 5718 step 5c,5d */
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0xFFFFFFFF);
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0);

	if (BGE_IS_5700_FAMILY(sc)) {
		bge_stop_block(sc, BGE_BMAN_MODE, BGE_BMANMODE_ENABLE);
		bge_stop_block(sc, BGE_MARB_MODE, BGE_MARBMODE_ENABLE);
	}

	bge_reset(sc);
	bge_sig_legacy(sc, BGE_RESET_SHUTDOWN);
	bge_sig_post_reset(sc, BGE_RESET_SHUTDOWN);

	/*
	 * Keep the ASF firmware running if up.
	 */
	if (sc->bge_asf_mode & ASF_STACKUP)
		BGE_SETBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);
	else
		BGE_CLRBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);

	/* Free the RX lists. */
	bge_free_rx_ring_std(sc);

	/* Free jumbo RX list. */
	if (BGE_IS_JUMBO_CAPABLE(sc))
		bge_free_rx_ring_jumbo(sc);

	/* Free TX buffers. */
	bge_free_tx_ring(sc);

	/*
	 * Isolate/power down the PHY.
	 */
	if (!(sc->bge_flags & BGEF_FIBER_TBI))
		mii_down(&sc->bge_mii);

	sc->bge_tx_saved_considx = BGE_TXCONS_UNSET;

	/* Clear MAC's link state (PHY may still have link UP). */
	BGE_STS_CLRBIT(sc, BGE_STS_LINK);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

static void
bge_link_upd(struct bge_softc *sc)
{
	struct ifnet *ifp = &sc->ethercom.ec_if;
	struct mii_data *mii = &sc->bge_mii;
	uint32_t status;
	int link;

	/* Clear 'pending link event' flag */
	BGE_STS_CLRBIT(sc, BGE_STS_LINK_EVT);

	/*
	 * Process link state changes.
	 * Grrr. The link status word in the status block does
	 * not work correctly on the BCM5700 rev AX and BX chips,
	 * according to all available information. Hence, we have
	 * to enable MII interrupts in order to properly obtain
	 * async link changes. Unfortunately, this also means that
	 * we have to read the MAC status register to detect link
	 * changes, thereby adding an additional register access to
	 * the interrupt handler.
	 */

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5700) {
		status = CSR_READ_4(sc, BGE_MAC_STS);
		if (status & BGE_MACSTAT_MI_INTERRUPT) {
			mii_pollstat(mii);

			if (!BGE_STS_BIT(sc, BGE_STS_LINK) &&
			    mii->mii_media_status & IFM_ACTIVE &&
			    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE)
				BGE_STS_SETBIT(sc, BGE_STS_LINK);
			else if (BGE_STS_BIT(sc, BGE_STS_LINK) &&
			    (!(mii->mii_media_status & IFM_ACTIVE) ||
			    IFM_SUBTYPE(mii->mii_media_active) == IFM_NONE))
				BGE_STS_CLRBIT(sc, BGE_STS_LINK);

			/* Clear the interrupt */
			CSR_WRITE_4(sc, BGE_MAC_EVT_ENB,
			    BGE_EVTENB_MI_INTERRUPT);
			bge_miibus_readreg(sc->bge_dev, sc->bge_phy_addr,
			    BRGPHY_MII_ISR);
			bge_miibus_writereg(sc->bge_dev, sc->bge_phy_addr,
			    BRGPHY_MII_IMR, BRGPHY_INTRS);
		}
		return;
	}

	if (sc->bge_flags & BGEF_FIBER_TBI) {
		status = CSR_READ_4(sc, BGE_MAC_STS);
		if (status & BGE_MACSTAT_TBI_PCS_SYNCHED) {
			if (!BGE_STS_BIT(sc, BGE_STS_LINK)) {
				BGE_STS_SETBIT(sc, BGE_STS_LINK);
				if (BGE_ASICREV(sc->bge_chipid)
				    == BGE_ASICREV_BCM5704) {
					BGE_CLRBIT(sc, BGE_MAC_MODE,
					    BGE_MACMODE_TBI_SEND_CFGS);
					DELAY(40);
				}
				CSR_WRITE_4(sc, BGE_MAC_STS, 0xFFFFFFFF);
				if_link_state_change(ifp, LINK_STATE_UP);
			}
		} else if (BGE_STS_BIT(sc, BGE_STS_LINK)) {
			BGE_STS_CLRBIT(sc, BGE_STS_LINK);
			if_link_state_change(ifp, LINK_STATE_DOWN);
		}
	} else if (BGE_STS_BIT(sc, BGE_STS_AUTOPOLL)) {
		/*
		 * Some broken BCM chips have BGE_STATFLAG_LINKSTATE_CHANGED
		 * bit in status word always set. Workaround this bug by
		 * reading PHY link status directly.
		 */
		link = (CSR_READ_4(sc, BGE_MI_STS) & BGE_MISTS_LINK)?
		    BGE_STS_LINK : 0;

		if (BGE_STS_BIT(sc, BGE_STS_LINK) != link) {
			mii_pollstat(mii);

			if (!BGE_STS_BIT(sc, BGE_STS_LINK) &&
			    mii->mii_media_status & IFM_ACTIVE &&
			    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE)
				BGE_STS_SETBIT(sc, BGE_STS_LINK);
			else if (BGE_STS_BIT(sc, BGE_STS_LINK) &&
			    (!(mii->mii_media_status & IFM_ACTIVE) ||
			    IFM_SUBTYPE(mii->mii_media_active) == IFM_NONE))
				BGE_STS_CLRBIT(sc, BGE_STS_LINK);
		}
	} else {
		/*
		 * For controllers that call mii_tick, we have to poll
		 * link status.
		 */
		mii_pollstat(mii);
	}

	if (BGE_CHIPREV(sc->bge_chipid) == BGE_CHIPREV_5784_AX) {
		uint32_t reg, scale;

		reg = CSR_READ_4(sc, BGE_CPMU_CLCK_STAT) &
		    BGE_CPMU_CLCK_STAT_MAC_CLCK_MASK;
		if (reg == BGE_CPMU_CLCK_STAT_MAC_CLCK_62_5)
			scale = 65;
		else if (reg == BGE_CPMU_CLCK_STAT_MAC_CLCK_6_25)
			scale = 6;
		else
			scale = 12;

		reg = CSR_READ_4(sc, BGE_MISC_CFG) &
		    ~BGE_MISCCFG_TIMER_PRESCALER;
		reg |= scale << 1;
		CSR_WRITE_4(sc, BGE_MISC_CFG, reg);
	}
	/* Clear the attention */
	CSR_WRITE_4(sc, BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED|
	    BGE_MACSTAT_CFG_CHANGED|BGE_MACSTAT_MI_COMPLETE|
	    BGE_MACSTAT_LINK_CHANGED);
}

static int
bge_sysctl_verify(SYSCTLFN_ARGS)
{
	int error, t;
	struct sysctlnode node;

	node = *rnode;
	t = *(int*)rnode->sysctl_data;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL)
		return error;

#if 0
	DPRINTF2(("%s: t = %d, nodenum = %d, rnodenum = %d\n", __func__, t,
	    node.sysctl_num, rnode->sysctl_num));
#endif

	if (node.sysctl_num == bge_rxthresh_nodenum) {
		if (t < 0 || t >= NBGE_RX_THRESH)
			return EINVAL;
		bge_update_all_threshes(t);
	} else
		return EINVAL;

	*(int*)rnode->sysctl_data = t;

	return 0;
}

/*
 * Set up sysctl(3) MIB, hw.bge.*.
 */
static void
bge_sysctl_init(struct bge_softc *sc)
{
	int rc, bge_root_num;
	const struct sysctlnode *node;

	if ((rc = sysctl_createv(&sc->bge_log, 0, NULL, &node,
	    0, CTLTYPE_NODE, "bge",
	    SYSCTL_DESCR("BGE interface controls"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0) {
		goto out;
	}

	bge_root_num = node->sysctl_num;

	/* BGE Rx interrupt mitigation level */
	if ((rc = sysctl_createv(&sc->bge_log, 0, NULL, &node,
	    CTLFLAG_READWRITE,
	    CTLTYPE_INT, "rx_lvl",
	    SYSCTL_DESCR("BGE receive interrupt mitigation level"),
	    bge_sysctl_verify, 0,
	    &bge_rx_thresh_lvl,
	    0, CTL_HW, bge_root_num, CTL_CREATE,
	    CTL_EOL)) != 0) {
		goto out;
	}

	bge_rxthresh_nodenum = node->sysctl_num;

	return;

out:
	aprint_error("%s: sysctl_createv failed (rc = %d)\n", __func__, rc);
}

#ifdef BGE_DEBUG
void
bge_debug_info(struct bge_softc *sc)
{

	printf("Hardware Flags:\n");
	if (BGE_IS_57765_PLUS(sc))
		printf(" - 57765 Plus\n");
	if (BGE_IS_5717_PLUS(sc))
		printf(" - 5717 Plus\n");
	if (BGE_IS_5755_PLUS(sc))
		printf(" - 5755 Plus\n");
	if (BGE_IS_575X_PLUS(sc))
		printf(" - 575X Plus\n");
	if (BGE_IS_5705_PLUS(sc))
		printf(" - 5705 Plus\n");
	if (BGE_IS_5714_FAMILY(sc))
		printf(" - 5714 Family\n");
	if (BGE_IS_5700_FAMILY(sc))
		printf(" - 5700 Family\n");
	if (sc->bge_flags & BGEF_IS_5788)
		printf(" - 5788\n");
	if (sc->bge_flags & BGEF_JUMBO_CAPABLE)
		printf(" - Supports Jumbo Frames\n");
	if (sc->bge_flags & BGEF_NO_EEPROM)
		printf(" - No EEPROM\n");
	if (sc->bge_flags & BGEF_PCIX)
		printf(" - PCI-X Bus\n");
	if (sc->bge_flags & BGEF_PCIE)
		printf(" - PCI Express Bus\n");
	if (sc->bge_flags & BGEF_RX_ALIGNBUG)
		printf(" - RX Alignment Bug\n");
	if (sc->bge_flags & BGEF_APE)
		printf(" - APE\n");
	if (sc->bge_flags & BGEF_CPMU_PRESENT)
		printf(" - CPMU\n");
	if (sc->bge_flags & BGEF_TSO)
		printf(" - TSO\n");
	if (sc->bge_flags & BGEF_TAGGED_STATUS)
		printf(" - TAGGED_STATUS\n");

	/* PHY related */
	if (sc->bge_phy_flags & BGEPHYF_NO_3LED)
		printf(" - No 3 LEDs\n");
	if (sc->bge_phy_flags & BGEPHYF_CRC_BUG)
		printf(" - CRC bug\n");
	if (sc->bge_phy_flags & BGEPHYF_ADC_BUG)
		printf(" - ADC bug\n");
	if (sc->bge_phy_flags & BGEPHYF_5704_A0_BUG)
		printf(" - 5704 A0 bug\n");
	if (sc->bge_phy_flags & BGEPHYF_JITTER_BUG)
		printf(" - jitter bug\n");
	if (sc->bge_phy_flags & BGEPHYF_BER_BUG)
		printf(" - BER bug\n");
	if (sc->bge_phy_flags & BGEPHYF_ADJUST_TRIM)
		printf(" - adjust trim\n");
	if (sc->bge_phy_flags & BGEPHYF_NO_WIRESPEED)
		printf(" - no wirespeed\n");

	/* ASF related */
	if (sc->bge_asf_mode & ASF_ENABLE)
		printf(" - ASF enable\n");
	if (sc->bge_asf_mode & ASF_NEW_HANDSHAKE)
		printf(" - ASF new handshake\n");
	if (sc->bge_asf_mode & ASF_STACKUP)
		printf(" - ASF stackup\n");
}
#endif /* BGE_DEBUG */

static int
bge_get_eaddr_fw(struct bge_softc *sc, uint8_t ether_addr[])
{
	prop_dictionary_t dict;
	prop_data_t ea;

	if ((sc->bge_flags & BGEF_NO_EEPROM) == 0)
		return 1;

	dict = device_properties(sc->bge_dev);
	ea = prop_dictionary_get(dict, "mac-address");
	if (ea != NULL) {
		KASSERT(prop_object_type(ea) == PROP_TYPE_DATA);
		KASSERT(prop_data_size(ea) == ETHER_ADDR_LEN);
		memcpy(ether_addr, prop_data_data_nocopy(ea), ETHER_ADDR_LEN);
		return 0;
	}

	return 1;
}

static int
bge_get_eaddr_mem(struct bge_softc *sc, uint8_t ether_addr[])
{
	uint32_t mac_addr;

	mac_addr = bge_readmem_ind(sc, BGE_SRAM_MAC_ADDR_HIGH_MB);
	if ((mac_addr >> 16) == 0x484b) {
		ether_addr[0] = (uint8_t)(mac_addr >> 8);
		ether_addr[1] = (uint8_t)mac_addr;
		mac_addr = bge_readmem_ind(sc, BGE_SRAM_MAC_ADDR_LOW_MB);
		ether_addr[2] = (uint8_t)(mac_addr >> 24);
		ether_addr[3] = (uint8_t)(mac_addr >> 16);
		ether_addr[4] = (uint8_t)(mac_addr >> 8);
		ether_addr[5] = (uint8_t)mac_addr;
		return 0;
	}
	return 1;
}

static int
bge_get_eaddr_nvram(struct bge_softc *sc, uint8_t ether_addr[])
{
	int mac_offset = BGE_EE_MAC_OFFSET;

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906)
		mac_offset = BGE_EE_MAC_OFFSET_5906;

	return (bge_read_nvram(sc, ether_addr, mac_offset + 2,
	    ETHER_ADDR_LEN));
}

static int
bge_get_eaddr_eeprom(struct bge_softc *sc, uint8_t ether_addr[])
{

	if (BGE_ASICREV(sc->bge_chipid) == BGE_ASICREV_BCM5906)
		return 1;

	return (bge_read_eeprom(sc, ether_addr, BGE_EE_MAC_OFFSET + 2,
	   ETHER_ADDR_LEN));
}

static int
bge_get_eaddr(struct bge_softc *sc, uint8_t eaddr[])
{
	static const bge_eaddr_fcn_t bge_eaddr_funcs[] = {
		/* NOTE: Order is critical */
		bge_get_eaddr_fw,
		bge_get_eaddr_mem,
		bge_get_eaddr_nvram,
		bge_get_eaddr_eeprom,
		NULL
	};
	const bge_eaddr_fcn_t *func;

	for (func = bge_eaddr_funcs; *func != NULL; ++func) {
		if ((*func)(sc, eaddr) == 0)
			break;
	}
	return (*func == NULL ? ENXIO : 0);
}
