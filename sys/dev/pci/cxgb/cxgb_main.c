/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cxgb_main.c,v 1.4 2013/01/23 23:31:26 joerg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/queue.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/if_inarp.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#ifdef CONFIG_DEFINED
#include <cxgb_include.h>
#else
#include <dev/pci/cxgb/cxgb_include.h>
#endif

#ifdef PRIV_SUPPORTED
#include <sys/priv.h>
#endif

#include <altq/altq_conf.h>

static int cxgb_setup_msix(adapter_t *, int);
static void cxgb_teardown_msix(adapter_t *);
static int cxgb_init(struct ifnet *);
static void cxgb_init_locked(struct port_info *);
static void cxgb_stop_locked(struct port_info *);
static void cxgb_set_rxmode(struct port_info *);
static int cxgb_ioctl(struct ifnet *, unsigned long, void *);
static void cxgb_start(struct ifnet *);
static void cxgb_stop(struct ifnet *, int);
static void cxgb_start_proc(struct work *, void *);
static int cxgb_media_change(struct ifnet *);
static void cxgb_media_status(struct ifnet *, struct ifmediareq *);
static int setup_sge_qsets(adapter_t *);
static int cxgb_async_intr(void *);
static void cxgb_ext_intr_handler(struct work *, void *);
static void cxgb_tick_handler(struct work *, void *);
static void cxgb_down_locked(struct adapter *sc);
static void cxgb_tick(void *);
static void setup_rss(adapter_t *sc);

/* Attachment glue for the PCI controller end of the device.  Each port of
 * the device is attached separately, as defined later.
 */
static int cxgb_controller_match(device_t dev, cfdata_t match, void *context);
static void cxgb_controller_attach(device_t parent, device_t dev, void *context);
static int cxgb_controller_detach(device_t dev, int flags);
static void cxgb_free(struct adapter *);
static __inline void reg_block_dump(struct adapter *ap, uint8_t *buf, unsigned int start,
    unsigned int end);
static void touch_bars(device_t dev);

#ifdef notyet
static int offload_close(struct toedev *tdev);
#endif


CFATTACH_DECL_NEW(cxgbc, sizeof(struct adapter), cxgb_controller_match, cxgb_controller_attach, cxgb_controller_detach, NULL);

/*
 * Attachment glue for the ports.  Attachment is done directly to the
 * controller device.
 */
static int cxgb_port_match(device_t dev, cfdata_t match, void *context);
static void cxgb_port_attach(device_t dev, device_t self, void *context);
static int cxgb_port_detach(device_t dev, int flags);

CFATTACH_DECL_NEW(cxgb, sizeof(struct port_device), cxgb_port_match, cxgb_port_attach, cxgb_port_detach, NULL);

#define SGE_MSIX_COUNT (SGE_QSETS + 1)

extern int collapse_mbufs;
#ifdef MSI_SUPPORTED
/*
 * The driver uses the best interrupt scheme available on a platform in the
 * order MSI-X, MSI, legacy pin interrupts.  This parameter determines which
 * of these schemes the driver may consider as follows:
 *
 * msi = 2: choose from among all three options
 * msi = 1 : only consider MSI and pin interrupts
 * msi = 0: force pin interrupts
 */
static int msi_allowed = 2;
#endif

/*
 * The driver uses an auto-queue algorithm by default.
 * To disable it and force a single queue-set per port, use singleq = 1.
 */
static int singleq = 1;

enum {
    MAX_TXQ_ENTRIES      = 16384,
    MAX_CTRL_TXQ_ENTRIES = 1024,
    MAX_RSPQ_ENTRIES     = 16384,
    MAX_RX_BUFFERS       = 16384,
    MAX_RX_JUMBO_BUFFERS = 16384,
    MIN_TXQ_ENTRIES      = 4,
    MIN_CTRL_TXQ_ENTRIES = 4,
    MIN_RSPQ_ENTRIES     = 32,
    MIN_FL_ENTRIES       = 32,
    MIN_FL_JUMBO_ENTRIES = 32
};

struct filter_info {
    u32 sip;
    u32 sip_mask;
    u32 dip;
    u16 sport;
    u16 dport;
    u32 vlan:12;
    u32 vlan_prio:3;
    u32 mac_hit:1;
    u32 mac_idx:4;
    u32 mac_vld:1;
    u32 pkt_type:2;
    u32 report_filter_id:1;
    u32 pass:1;
    u32 rss:1;
    u32 qset:3;
    u32 locked:1;
    u32 valid:1;
};

enum { FILTER_NO_VLAN_PRI = 7 };

#define PORT_MASK ((1 << MAX_NPORTS) - 1)

/* Table for probing the cards.  The desc field isn't actually used */
struct cxgb_ident {
    uint16_t    vendor;
    uint16_t    device;
    int         index;
    const char  *desc;
} cxgb_identifiers[] = {
    {PCI_VENDOR_ID_CHELSIO, 0x0020, 0, "PE9000"},
    {PCI_VENDOR_ID_CHELSIO, 0x0021, 1, "T302E"},
    {PCI_VENDOR_ID_CHELSIO, 0x0022, 2, "T310E"},
    {PCI_VENDOR_ID_CHELSIO, 0x0023, 3, "T320X"},
    {PCI_VENDOR_ID_CHELSIO, 0x0024, 1, "T302X"},
    {PCI_VENDOR_ID_CHELSIO, 0x0025, 3, "T320E"},
    {PCI_VENDOR_ID_CHELSIO, 0x0026, 2, "T310X"},
    {PCI_VENDOR_ID_CHELSIO, 0x0030, 2, "T3B10"},
    {PCI_VENDOR_ID_CHELSIO, 0x0031, 3, "T3B20"},
    {PCI_VENDOR_ID_CHELSIO, 0x0032, 1, "T3B02"},
    {PCI_VENDOR_ID_CHELSIO, 0x0033, 4, "T3B04"},
    {0, 0, 0, NULL}
};


static inline char
t3rev2char(struct adapter *adapter)
{
    char rev = 'z';

    switch(adapter->params.rev) {
    case T3_REV_A:
        rev = 'a';
        break;
    case T3_REV_B:
    case T3_REV_B2:
        rev = 'b';
        break;
    case T3_REV_C:
        rev = 'c';
        break;
    }
    return rev;
}

static struct cxgb_ident *cxgb_get_ident(struct pci_attach_args *pa)
{
    struct cxgb_ident *id;
    int vendorid, deviceid;

    vendorid = PCI_VENDOR(pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_ID_REG));
    deviceid = PCI_PRODUCT(pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_ID_REG));

    for (id = cxgb_identifiers; id->desc != NULL; id++) {
        if ((id->vendor == vendorid) &&
            (id->device == deviceid)) {
            return (id);
        }
    }
    return (NULL);
}

static const struct adapter_info *cxgb_get_adapter_info(struct pci_attach_args *pa)
{
    struct cxgb_ident *id;
    const struct adapter_info *ai;

    id = cxgb_get_ident(pa);
    if (id == NULL)
        return (NULL);

    ai = t3_get_adapter_info(id->index);
    return (ai);
}

static int cxgb_controller_match(device_t dev, cfdata_t match, void *context)
{
    struct pci_attach_args *pa = context;
    const struct adapter_info *ai;

    ai = cxgb_get_adapter_info(pa);
    if (ai == NULL)
        return (0);

    return (100); // we ARE the best driver for this card!!
}

#define FW_FNAME "t3fw%d%d%d"
#define TPEEPROM_NAME "t3%ctpe%d%d%d"
#define TPSRAM_NAME "t3%cps%d%d%d"

int cxgb_cfprint(void *aux, const char *info);
int cxgb_cfprint(void *aux, const char *info)
{
    if (info)
    {
        printf("cxgb_cfprint(%p, \"%s\")\n", aux, info);
        INT3;
    }

    return (QUIET);
}

void cxgb_make_task(void *context)
{
    struct cxgb_task *w = (struct cxgb_task *)context;

    // we can only use workqueue_create() once the system is up and running
    workqueue_create(&w->wq, w->name, w->func, w->context, PRIBIO, IPL_NET, 0);
//  printf("======>> create workqueue for %s %p\n", w->name, w->wq);
}

static void
cxgb_controller_attach(device_t parent, device_t dev, void *context)
{
    device_t child;
    const struct adapter_info *ai;
    struct adapter *sc;
    struct pci_attach_args *pa = context;
    struct cxgb_attach_args cxgb_args;
    int locs[2];
    int i, error = 0;
    uint32_t vers;
    int port_qsets = 1;
    int reg;
#ifdef MSI_SUPPORTED
    int msi_needed;
#endif

    sc = device_private(dev);
    sc->dev = dev;
    memcpy(&sc->pa, pa, sizeof(struct pci_attach_args));
    sc->msi_count = 0;
    ai = cxgb_get_adapter_info(pa);

    /*
     * XXX not really related but a recent addition
     */
#ifdef MSI_SUPPORTED
    /* find the PCIe link width and set max read request to 4KB*/
    if (pci_find_extcap(dev, PCIY_EXPRESS, &reg) == 0) {
        uint16_t lnk, pectl;
        lnk = pci_read_config(dev, reg + 0x12, 2);
        sc->link_width = (lnk >> 4) & 0x3f;

        pectl = pci_read_config(dev, reg + 0x8, 2);
        pectl = (pectl & ~0x7000) | (5 << 12);
        pci_write_config(dev, reg + 0x8, pectl, 2);
    }

    if (sc->link_width != 0 && sc->link_width <= 4 &&
        (ai->nports0 + ai->nports1) <= 2) {
        device_printf(sc->dev,
            "PCIe x%d Link, expect reduced performance\n",
            sc->link_width);
    }
#endif

    touch_bars(dev);

    pci_enable_busmaster(dev);

    /*
     * Allocate the registers and make them available to the driver.
     * The registers that we care about for NIC mode are in BAR 0
     */
	sc->regs_rid = PCI_MAPREG_START;
	t3_os_pci_read_config_4(sc, PCI_MAPREG_START, &reg);

	// call bus_space_map
	sc->bar0 = reg&0xFFFFF000;
	bus_space_map(sc->pa.pa_memt, sc->bar0, 4096, 0, &sc->bar0_handle);

    MTX_INIT(&sc->sge.reg_lock, sc->reglockbuf, NULL, MTX_DEF);
    MTX_INIT(&sc->mdio_lock, sc->mdiolockbuf, NULL, MTX_DEF);
    MTX_INIT(&sc->elmer_lock, sc->elmerlockbuf, NULL, MTX_DEF);

    sc->bt = sc->pa.pa_memt;
    sc->bh = sc->bar0_handle;
    sc->mmio_len = 4096;

    if (t3_prep_adapter(sc, ai, 1) < 0) {
        printf("prep adapter failed\n");
        error = ENODEV;
        goto out;
    }
    /* Allocate the BAR for doing MSI-X.  If it succeeds, try to allocate
     * enough messages for the queue sets.  If that fails, try falling
     * back to MSI.  If that fails, then try falling back to the legacy
     * interrupt pin model.
     */
#ifdef MSI_SUPPORTED

    sc->msix_regs_rid = 0x20;
    if ((msi_allowed >= 2) &&
        (sc->msix_regs_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
        &sc->msix_regs_rid, RF_ACTIVE)) != NULL) {

        msi_needed = sc->msi_count = SGE_MSIX_COUNT;

        if (((error = pci_alloc_msix(dev, &sc->msi_count)) != 0) ||
            (sc->msi_count != msi_needed)) {
            device_printf(dev, "msix allocation failed - msi_count = %d"
                " msi_needed=%d will try msi err=%d\n", sc->msi_count,
                msi_needed, error);
            sc->msi_count = 0;
            pci_release_msi(dev);
            bus_release_resource(dev, SYS_RES_MEMORY,
                sc->msix_regs_rid, sc->msix_regs_res);
            sc->msix_regs_res = NULL;
        } else {
            sc->flags |= USING_MSIX;
            sc->cxgb_intr = t3_intr_msix;
        }
    }

    if ((msi_allowed >= 1) && (sc->msi_count == 0)) {
        sc->msi_count = 1;
        if (pci_alloc_msi(dev, &sc->msi_count)) {
            device_printf(dev, "alloc msi failed - will try INTx\n");
            sc->msi_count = 0;
            pci_release_msi(dev);
        } else {
            sc->flags |= USING_MSI;
            sc->irq_rid = 1;
            sc->cxgb_intr = t3_intr_msi;
        }
    }
#endif
    if (sc->msi_count == 0) {
        device_printf(dev, "using line interrupts\n");
        sc->irq_rid = 0;
        sc->cxgb_intr = t3b_intr;
    }

    sc->ext_intr_task.name = "cxgb_ext_intr_handler";
    sc->ext_intr_task.func = cxgb_ext_intr_handler;
    sc->ext_intr_task.context = sc;
    kthread_create(PRI_NONE, 0, NULL, cxgb_make_task, &sc->ext_intr_task, NULL, "cxgb_make_task");

    sc->tick_task.name = "cxgb_tick_handler";
    sc->tick_task.func = cxgb_tick_handler;
    sc->tick_task.context = sc;
    kthread_create(PRI_NONE, 0, NULL, cxgb_make_task, &sc->tick_task, NULL, "cxgb_make_task");

    /* Create a periodic callout for checking adapter status */
    callout_init(&sc->cxgb_tick_ch, 0);

    if (t3_check_fw_version(sc) != 0) {
        /*
         * Warn user that a firmware update will be attempted in init.
         */
        device_printf(dev, "firmware needs to be updated to version %d.%d.%d\n",
            FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_MICRO);
        sc->flags &= ~FW_UPTODATE;
    } else {
        sc->flags |= FW_UPTODATE;
    }

    if (t3_check_tpsram_version(sc) != 0) {
        /*
         * Warn user that a firmware update will be attempted in init.
         */
        device_printf(dev, "SRAM needs to be updated to version %c-%d.%d.%d\n",
            t3rev2char(sc), TP_VERSION_MAJOR, TP_VERSION_MINOR, TP_VERSION_MICRO);
        sc->flags &= ~TPS_UPTODATE;
    } else {
        sc->flags |= TPS_UPTODATE;
    }

    if ((sc->flags & USING_MSIX) && !singleq)
        port_qsets = (SGE_QSETS/(sc)->params.nports);

    /*
     * Create a child device for each MAC.  The ethernet attachment
     * will be done in these children.
     */
    for (i = 0; i < (sc)->params.nports; i++) {
        struct port_info *pi;

        pi = &sc->port[i];
        pi->adapter = sc;
        pi->nqsets = port_qsets;
        pi->first_qset = i*port_qsets;
        pi->port_id = i;
        pi->tx_chan = i >= ai->nports0;
        pi->txpkt_intf = pi->tx_chan ? 2 * (i - ai->nports0) + 1 : 2 * i;
        sc->rxpkt_map[pi->txpkt_intf] = i;
        cxgb_args.port = i;
        locs[0] = 1;
        locs[1] = i;
	printf("\n"); // for cleaner formatting in dmesg
        child = config_found_sm_loc(dev, "cxgbc", locs, &cxgb_args,
                    cxgb_cfprint, config_stdsubmatch);
	printf("\n"); // for cleaner formatting in dmesg
        sc->portdev[i] = child;
    }

    /*
     * XXX need to poll for link status
     */
    sc->params.stats_update_period = 1;

    /* initialize sge private state */
    t3_sge_init_adapter(sc);

    t3_led_ready(sc);

    error = t3_get_fw_version(sc, &vers);
    if (error)
        goto out;

    snprintf(&sc->fw_version[0], sizeof(sc->fw_version), "%d.%d.%d",
        G_FW_VERSION_MAJOR(vers), G_FW_VERSION_MINOR(vers),
        G_FW_VERSION_MICRO(vers));
out:
    if (error)
    {
        cxgb_free(sc);
    }
}

static int
cxgb_controller_detach(device_t dev, int flags)
{
    struct adapter *sc;

    sc = device_private(dev);

    cxgb_free(sc);

    return (0);
}

static void
cxgb_free(struct adapter *sc)
{
    int i;

    ADAPTER_LOCK(sc);
    /*
     * drops the lock
     */
    cxgb_down_locked(sc);

#ifdef MSI_SUPPORTED
    if (sc->flags & (USING_MSI | USING_MSIX)) {
        device_printf(sc->dev, "releasing msi message(s)\n");
        pci_release_msi(sc->dev);
    } else {
        device_printf(sc->dev, "no msi message to release\n");
    }
    if (sc->msix_regs_res != NULL) {
        bus_release_resource(sc->dev, SYS_RES_MEMORY, sc->msix_regs_rid,
            sc->msix_regs_res);
    }
#endif

    t3_sge_deinit_sw(sc);
    /*
     * Wait for last callout
     */

    tsleep(&sc, 0, "cxgb unload", 3*hz);

    for (i = 0; i < (sc)->params.nports; ++i) {
        if (sc->portdev[i] != NULL)
	{
		INT3;
	}
    }

#ifdef notyet
    if (is_offload(sc)) {
        cxgb_adapter_unofld(sc);
        if (isset(&sc->open_device_map, OFFLOAD_DEVMAP_BIT))
            offload_close(&sc->tdev);
    }
#endif

    t3_free_sge_resources(sc);
    free(sc->filters, M_DEVBUF);
    t3_sge_free(sc);

    MTX_DESTROY(&sc->mdio_lock);
    MTX_DESTROY(&sc->sge.reg_lock);
    MTX_DESTROY(&sc->elmer_lock);
    ADAPTER_LOCK_DEINIT(sc);

    return;
}

/**
 *  setup_sge_qsets - configure SGE Tx/Rx/response queues
 *  @sc: the controller softc
 *
 *  Determines how many sets of SGE queues to use and initializes them.
 *  We support multiple queue sets per port if we have MSI-X, otherwise
 *  just one queue set per port.
 */
static int
setup_sge_qsets(adapter_t *sc)
{
    int i, j, err, irq_idx = 0, qset_idx = 0;
    u_int ntxq = SGE_TXQ_PER_SET;

    if ((err = t3_sge_alloc(sc)) != 0) {
        device_printf(sc->dev, "t3_sge_alloc returned %d\n", err);
        return (err);
    }

    if (sc->params.rev > 0 && !(sc->flags & USING_MSI))
        irq_idx = -1;

    for (i = 0; i < (sc)->params.nports; i++) {
        struct port_info *pi = &sc->port[i];

        for (j = 0; j < pi->nqsets; j++, qset_idx++) {
            err = t3_sge_alloc_qset(sc, qset_idx, (sc)->params.nports,
                (sc->flags & USING_MSIX) ? qset_idx + 1 : irq_idx,
                &sc->params.sge.qset[qset_idx], ntxq, pi);
            if (err) {
                t3_free_sge_resources(sc);
                device_printf(sc->dev, "t3_sge_alloc_qset failed with %d\n",
                    err);
                return (err);
            }
        }
    }

    return (0);
}

static void
cxgb_teardown_msix(adapter_t *sc)
{
    int i, nqsets;

    for (nqsets = i = 0; i < (sc)->params.nports; i++)
        nqsets += sc->port[i].nqsets;

    for (i = 0; i < nqsets; i++) {
        if (sc->msix_intr_tag[i] != NULL) {
            sc->msix_intr_tag[i] = NULL;
        }
        if (sc->msix_irq_res[i] != NULL) {
            sc->msix_irq_res[i] = NULL;
        }
    }
}

static int
cxgb_setup_msix(adapter_t *sc, int msix_count)
{
    int i, j, k, nqsets, rid;

    /* The first message indicates link changes and error conditions */
    sc->irq_rid = 1;
    /* Allocate PCI interrupt resources. */
    if (pci_intr_map(&sc->pa, &sc->intr_handle))
    {
        printf("cxgb_setup_msix(%d): pci_intr_map() failed\n", __LINE__);
        return (EINVAL);
    }
    sc->intr_cookie = pci_intr_establish(sc->pa.pa_pc, sc->intr_handle,
                        IPL_NET, cxgb_async_intr, sc);
    if (sc->intr_cookie == NULL)
    {
        printf("cxgb_setup_msix(%d): pci_intr_establish() failed\n", __LINE__);
        return (EINVAL);
    }
    for (i = k = 0; i < (sc)->params.nports; i++) {
        nqsets = sc->port[i].nqsets;
        for (j = 0; j < nqsets; j++, k++) {
            rid = k + 2;
            if (cxgb_debug)
                printf("rid=%d ", rid);
            INT3;
        }
    }


    return (0);
}

static int cxgb_port_match(device_t dev, cfdata_t match, void *context)
{
    return (100);
}

#define IFCAP_HWCSUM (IFCAP_CSUM_IPv4_Rx | IFCAP_CSUM_IPv4_Tx)
#define IFCAP_RXCSUM IFCAP_CSUM_IPv4_Rx
#define IFCAP_TXCSUM IFCAP_CSUM_IPv4_Tx

#ifdef TSO_SUPPORTED
#define CXGB_CAP (IFCAP_HWCSUM | IFCAP_TSO)
/* Don't enable TSO6 yet */
#define CXGB_CAP_ENABLE (IFCAP_HWCSUM | IFCAP_TSO4)
#else
#define CXGB_CAP (IFCAP_HWCSUM)
/* Don't enable TSO6 yet */
#define CXGB_CAP_ENABLE (IFCAP_HWCSUM)
#define IFCAP_TSO4 0x0
#define IFCAP_TSO6 0x0
#define CSUM_TSO   0x0
#endif

static void
cxgb_port_attach(device_t parent, device_t self, void *context)
{
    struct port_info *p;
    struct port_device *pd;
    int *port_number = (int *)context;
    char buf[32];
    struct ifnet *ifp;
    int media_flags;
    pd = device_private(self);
    pd->dev = self;
    pd->parent = device_private(parent);
    pd->port_number = *port_number;
    p = &pd->parent->port[*port_number];
    p->pd = pd;

    PORT_LOCK_INIT(p, p->lockbuf);

    /* Allocate an ifnet object and set it up */
    ifp = p->ifp = (void *)malloc(sizeof (struct ifnet), M_IFADDR, M_WAITOK);
    if (ifp == NULL) {
        device_printf(self, "Cannot allocate ifnet\n");
        return;
    }
    memset(ifp, 0, sizeof(struct ifnet));

    /*
     * Note that there is currently no watchdog timer.
     */
    snprintf(buf, sizeof(buf), "cxgb%d", p->port);
    strcpy(ifp->if_xname, buf);
    ifp->if_init = cxgb_init;
    ifp->if_softc = p;
    ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
    ifp->if_ioctl = cxgb_ioctl;
    ifp->if_start = cxgb_start;
    ifp->if_stop = cxgb_stop;
    ifp->if_timer = 0;  /* Disable ifnet watchdog */
    ifp->if_watchdog = NULL;

    ifp->if_snd.ifq_maxlen = TX_ETH_Q_SIZE;
    IFQ_SET_MAXLEN(&ifp->if_snd, ifp->if_snd.ifq_maxlen);

    IFQ_SET_READY(&ifp->if_snd);

    ifp->if_capabilities = ifp->if_capenable = 0;
    ifp->if_baudrate = 10000000000; // 10 Gbps
    /*
     * disable TSO on 4-port - it isn't supported by the firmware yet
     */
    if (p->adapter->params.nports > 2) {
        ifp->if_capabilities &= ~(IFCAP_TSO4 | IFCAP_TSO6);
        ifp->if_capenable &= ~(IFCAP_TSO4 | IFCAP_TSO6);
    }

    if_attach(ifp);
    ether_ifattach(ifp, p->hw_addr);
    /*
     * Only default to jumbo frames on 10GigE
     */
    if (p->adapter->params.nports <= 2)
        ifp->if_mtu = 9000;
    ifmedia_init(&p->media, IFM_IMASK, cxgb_media_change,
        cxgb_media_status);

    if (!strcmp(p->port_type->desc, "10GBASE-CX4")) {
        media_flags = IFM_ETHER | IFM_10G_CX4 | IFM_FDX;
    } else if (!strcmp(p->port_type->desc, "10GBASE-SR")) {
        media_flags = IFM_ETHER | IFM_10G_SR | IFM_FDX;
    } else if (!strcmp(p->port_type->desc, "10GBASE-XR")) {
        media_flags = IFM_ETHER | IFM_10G_LR | IFM_FDX;
    } else if (!strcmp(p->port_type->desc, "10/100/1000BASE-T")) {
        ifmedia_add(&p->media, IFM_ETHER | IFM_10_T, 0, NULL);
        ifmedia_add(&p->media, IFM_ETHER | IFM_10_T | IFM_FDX,
                0, NULL);
        ifmedia_add(&p->media, IFM_ETHER | IFM_100_TX,
                0, NULL);
        ifmedia_add(&p->media, IFM_ETHER | IFM_100_TX | IFM_FDX,
                0, NULL);
        ifmedia_add(&p->media, IFM_ETHER | IFM_1000_T | IFM_FDX,
                0, NULL);
        media_flags = 0;
    } else {
            printf("unsupported media type %s\n", p->port_type->desc);
        return;
    }
    if (media_flags) {
        ifmedia_add(&p->media, media_flags, 0, NULL);
        ifmedia_set(&p->media, media_flags);
    } else {
        ifmedia_add(&p->media, IFM_ETHER | IFM_AUTO, 0, NULL);
        ifmedia_set(&p->media, IFM_ETHER | IFM_AUTO);
    }

    snprintf(p->taskqbuf, TASKQ_NAME_LEN, "cxgb_port_taskq%d", p->port_id);
    p->start_task.name = "cxgb_start_proc";
    p->start_task.func = cxgb_start_proc;
    p->start_task.context = ifp;
    kthread_create(PRI_NONE, 0, NULL, cxgb_make_task, &p->start_task, NULL, "cxgb_make_task");

    t3_sge_init_port(p);
}

static int
cxgb_port_detach(device_t self, int flags)
{
    struct port_info *p;

    p = device_private(self);

    PORT_LOCK(p);
    if (p->ifp->if_drv_flags & IFF_DRV_RUNNING)
        cxgb_stop_locked(p);
    PORT_UNLOCK(p);

    if (p->start_task.wq != NULL) {
        workqueue_destroy(p->start_task.wq);
        p->start_task.wq = NULL;
    }

    ether_ifdetach(p->ifp);
    /*
     * the lock may be acquired in ifdetach
     */
    PORT_LOCK_DEINIT(p);
    if_detach(p->ifp);

    return (0);
}

void
t3_fatal_err(struct adapter *sc)
{
    u_int fw_status[4];

    if (sc->flags & FULL_INIT_DONE) {
        t3_sge_stop(sc);
        t3_write_reg(sc, A_XGM_TX_CTRL, 0);
        t3_write_reg(sc, A_XGM_RX_CTRL, 0);
        t3_write_reg(sc, XGM_REG(A_XGM_TX_CTRL, 1), 0);
        t3_write_reg(sc, XGM_REG(A_XGM_RX_CTRL, 1), 0);
        t3_intr_disable(sc);
    }
    device_printf(sc->dev,"encountered fatal error, operation suspended\n");
    if (!t3_cim_ctl_blk_read(sc, 0xa0, 4, fw_status))
        device_printf(sc->dev, "FW_ status: 0x%x, 0x%x, 0x%x, 0x%x\n",
            fw_status[0], fw_status[1], fw_status[2], fw_status[3]);
}

int
t3_os_find_pci_capability(adapter_t *sc, int cap)
{
    device_t dev;
    uint32_t status;
    uint32_t bhlc;
    uint32_t temp;
    uint8_t ptr;
    dev = sc->dev;
    status = pci_conf_read(sc->pa.pa_pc, sc->pa.pa_tag, PCI_COMMAND_STATUS_REG);
    if (!(status&PCI_STATUS_CAPLIST_SUPPORT))
        return (0);
    bhlc = pci_conf_read(sc->pa.pa_pc, sc->pa.pa_tag, PCI_BHLC_REG);
    switch (PCI_HDRTYPE(bhlc))
    {
    case 0:
    case 1:
        ptr = PCI_CAPLISTPTR_REG;
        break;
    case 2:
        ptr = PCI_CARDBUS_CAPLISTPTR_REG;
        break;
    default:
        return (0);
    }
    temp = pci_conf_read(sc->pa.pa_pc, sc->pa.pa_tag, ptr);
    ptr = PCI_CAPLIST_PTR(temp);
    while (ptr != 0) {
        temp = pci_conf_read(sc->pa.pa_pc, sc->pa.pa_tag, ptr);
        if (PCI_CAPLIST_CAP(temp) == cap)
            return (ptr);
        ptr = PCI_CAPLIST_NEXT(temp);
    }

    return (0);
}

int
t3_os_pci_save_state(struct adapter *sc)
{
    INT3;
    return (0);
}

int
t3_os_pci_restore_state(struct adapter *sc)
{
    INT3;
    return (0);
}

/**
 *  t3_os_link_changed - handle link status changes
 *  @adapter: the adapter associated with the link change
 *  @port_id: the port index whose limk status has changed
 *  @link_stat: the new status of the link
 *  @speed: the new speed setting
 *  @duplex: the new duplex setting
 *  @fc: the new flow-control setting
 *
 *  This is the OS-dependent handler for link status changes.  The OS
 *  neutral handler takes care of most of the processing for these events,
 *  then calls this handler for any OS-specific processing.
 */
void
t3_os_link_changed(adapter_t *adapter, int port_id, int link_status, int speed,
     int duplex, int fc)
{
    struct port_info *pi = &adapter->port[port_id];
    struct cmac *mac = &adapter->port[port_id].mac;

    if ((pi->ifp->if_flags & IFF_UP) == 0)
        return;

    if (link_status) {
        t3_mac_enable(mac, MAC_DIRECTION_RX);
        if_link_state_change(pi->ifp, LINK_STATE_UP);
    } else {
        if_link_state_change(pi->ifp, LINK_STATE_DOWN);
        pi->phy.ops->power_down(&pi->phy, 1);
        t3_mac_disable(mac, MAC_DIRECTION_RX);
        t3_link_start(&pi->phy, mac, &pi->link_config);
    }
}

/*
 * Interrupt-context handler for external (PHY) interrupts.
 */
void
t3_os_ext_intr_handler(adapter_t *sc)
{
    if (cxgb_debug)
        printf("t3_os_ext_intr_handler\n");
    /*
     * Schedule a task to handle external interrupts as they may be slow
     * and we use a mutex to protect MDIO registers.  We disable PHY
     * interrupts in the meantime and let the task reenable them when
     * it's done.
     */
    ADAPTER_LOCK(sc);
    if (sc->slow_intr_mask) {
        sc->slow_intr_mask &= ~F_T3DBG;
        t3_write_reg(sc, A_PL_INT_ENABLE0, sc->slow_intr_mask);
        workqueue_enqueue(sc->ext_intr_task.wq, &sc->ext_intr_task.w, NULL);
    }
    ADAPTER_UNLOCK(sc);
}

void
t3_os_set_hw_addr(adapter_t *adapter, int port_idx, u8 hw_addr[])
{

    /*
     * The ifnet might not be allocated before this gets called,
     * as this is called early on in attach by t3_prep_adapter
     * save the address off in the port structure
     */
    if (cxgb_debug)
	printf("set_hw_addr on idx %d addr %02x:%02x:%02x:%02x:%02x:%02x\n", 
		port_idx, hw_addr[0], hw_addr[1], hw_addr[2], hw_addr[3], hw_addr[4], hw_addr[5]);
    memcpy(adapter->port[port_idx].hw_addr, hw_addr, ETHER_ADDR_LEN);
}

/**
 *  link_start - enable a port
 *  @p: the port to enable
 *
 *  Performs the MAC and PHY actions needed to enable a port.
 */
static void
cxgb_link_start(struct port_info *p)
{
    struct ifnet *ifp;
    struct t3_rx_mode rm;
    struct cmac *mac = &p->mac;

    ifp = p->ifp;

    t3_init_rx_mode(&rm, p);
    if (!mac->multiport)
        t3_mac_reset(mac);
    t3_mac_set_mtu(mac, ifp->if_mtu + ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN);
    t3_mac_set_address(mac, 0, p->hw_addr);
    t3_mac_set_rx_mode(mac, &rm);
    t3_link_start(&p->phy, mac, &p->link_config);
    t3_mac_enable(mac, MAC_DIRECTION_RX | MAC_DIRECTION_TX);
}

/**
 *  setup_rss - configure Receive Side Steering (per-queue connection demux)
 *  @adap: the adapter
 *
 *  Sets up RSS to distribute packets to multiple receive queues.  We
 *  configure the RSS CPU lookup table to distribute to the number of HW
 *  receive queues, and the response queue lookup table to narrow that
 *  down to the response queues actually configured for each port.
 *  We always configure the RSS mapping for two ports since the mapping
 *  table has plenty of entries.
 */
static void
setup_rss(adapter_t *adap)
{
    int i;
    u_int nq[2];
    uint8_t cpus[SGE_QSETS + 1];
    uint16_t rspq_map[RSS_TABLE_SIZE];

    for (i = 0; i < SGE_QSETS; ++i)
        cpus[i] = i;
    cpus[SGE_QSETS] = 0xff;

    nq[0] = nq[1] = 0;
    for_each_port(adap, i) {
        const struct port_info *pi = adap2pinfo(adap, i);

        nq[pi->tx_chan] += pi->nqsets;
    }
    nq[0] = max(nq[0], 1U);
    nq[1] = max(nq[1], 1U);
    for (i = 0; i < RSS_TABLE_SIZE / 2; ++i) {
        rspq_map[i] = i % nq[0];
        rspq_map[i + RSS_TABLE_SIZE / 2] = (i % nq[1]) + nq[0];
    }
    /* Calculate the reverse RSS map table */
    for (i = 0; i < RSS_TABLE_SIZE; ++i)
        if (adap->rrss_map[rspq_map[i]] == 0xff)
            adap->rrss_map[rspq_map[i]] = i;

    t3_config_rss(adap, F_RQFEEDBACKENABLE | F_TNLLKPEN | F_TNLMAPEN |
              F_TNLPRTEN | F_TNL2TUPEN | F_TNL4TUPEN | F_OFDMAPEN |
              V_RRCPLCPUSIZE(6), cpus, rspq_map);

}

/*
 * Sends an mbuf to an offload queue driver
 * after dealing with any active network taps.
 */
static inline int
offload_tx(struct toedev *tdev, struct mbuf *m)
{
    int ret;

    critical_enter();
    ret = t3_offload_tx(tdev, m);
    critical_exit();
    return (ret);
}

static void
send_pktsched_cmd(struct adapter *adap, int sched, int qidx, int lo,
                  int hi, int port)
{
    struct mbuf *m;
    struct mngt_pktsched_wr *req;

    m = m_gethdr(M_DONTWAIT, MT_DATA);
    if (m) {
        req = mtod(m, struct mngt_pktsched_wr *);
        req->wr_hi = htonl(V_WR_OP(FW_WROPCODE_MNGT));
        req->mngt_opcode = FW_MNGTOPCODE_PKTSCHED_SET;
        req->sched = sched;
        req->idx = qidx;
        req->min = lo;
        req->max = hi;
        req->binding = port;
        m->m_len = m->m_pkthdr.len = sizeof(*req);
        t3_mgmt_tx(adap, m);
    }
}

static void
bind_qsets(adapter_t *sc)
{
    int i, j;

    for (i = 0; i < (sc)->params.nports; ++i) {
        const struct port_info *pi = adap2pinfo(sc, i);

        for (j = 0; j < pi->nqsets; ++j) {
            send_pktsched_cmd(sc, 1, pi->first_qset + j, -1,
                      -1, pi->tx_chan);

        }
    }
}

/**
 *  cxgb_up - enable the adapter
 *  @adap: adapter being enabled
 *
 *  Called when the first port is enabled, this function performs the
 *  actions necessary to make an adapter operational, such as completing
 *  the initialization of HW modules, and enabling interrupts.
 *
 */
static int
cxgb_up(struct adapter *sc)
{
    int err = 0;

    if ((sc->flags & FULL_INIT_DONE) == 0) {

        if ((sc->flags & FW_UPTODATE) == 0)
	    printf("SHOULD UPGRADE FIRMWARE!\n");
        if ((sc->flags & TPS_UPTODATE) == 0)
	    printf("SHOULD UPDATE TPSRAM\n");
        err = t3_init_hw(sc, 0);
        if (err)
            goto out;

        t3_write_reg(sc, A_ULPRX_TDDP_PSZ, V_HPZ0(PAGE_SHIFT - 12));

        err = setup_sge_qsets(sc);
        if (err)
            goto out;

        setup_rss(sc);
        sc->flags |= FULL_INIT_DONE;
    }

    t3_intr_clear(sc);

    /* If it's MSI or INTx, allocate a single interrupt for everything */
    if ((sc->flags & USING_MSIX) == 0) {
        if (pci_intr_map(&sc->pa, &sc->intr_handle))
        {
            device_printf(sc->dev, "Cannot allocate interrupt\n");
            err = EINVAL;
            goto out;
        }
        device_printf(sc->dev, "allocated intr_handle=%d\n", sc->intr_handle);
        sc->intr_cookie = pci_intr_establish(sc->pa.pa_pc,
                    sc->intr_handle, IPL_NET,
                    sc->cxgb_intr, sc);
        if (sc->intr_cookie == NULL)
        {
            device_printf(sc->dev, "Cannot establish interrupt\n");
            err = EINVAL;
            goto irq_err;
        }
    } else {
        printf("Using MSIX?!?!?!\n");
        INT3;
        cxgb_setup_msix(sc, sc->msi_count);
    }

    t3_sge_start(sc);
    t3_intr_enable(sc);

    if (!(sc->flags & QUEUES_BOUND)) {
        bind_qsets(sc);
        sc->flags |= QUEUES_BOUND;
    }
out:
    return (err);
irq_err:
    CH_ERR(sc, "request_irq failed, err %d\n", err);
    goto out;
}


/*
 * Release resources when all the ports and offloading have been stopped.
 */
static void
cxgb_down_locked(struct adapter *sc)
{
    t3_sge_stop(sc);
    t3_intr_disable(sc);

    INT3; // XXXXXXXXXXXXXXXXXX

    if (sc->flags & USING_MSIX)
        cxgb_teardown_msix(sc);
    ADAPTER_UNLOCK(sc);

    callout_drain(&sc->cxgb_tick_ch);
    callout_drain(&sc->sge_timer_ch);

#ifdef notyet

        if (sc->port[i].tq != NULL)
#endif

}

static int
cxgb_init(struct ifnet *ifp)
{
    struct port_info *p = ifp->if_softc;

    PORT_LOCK(p);
    cxgb_init_locked(p);
    PORT_UNLOCK(p);

    return (0); // ????????????
}

static void
cxgb_init_locked(struct port_info *p)
{
    struct ifnet *ifp;
    adapter_t *sc = p->adapter;
    int err;

    PORT_LOCK_ASSERT_OWNED(p);
    ifp = p->ifp;

    ADAPTER_LOCK(p->adapter);
    if ((sc->open_device_map == 0) && (err = cxgb_up(sc))) {
        ADAPTER_UNLOCK(p->adapter);
        cxgb_stop_locked(p);
        return;
    }
    if (p->adapter->open_device_map == 0) {
        t3_intr_clear(sc);
        t3_sge_init_adapter(sc);
    }
    setbit(&p->adapter->open_device_map, p->port_id);
    ADAPTER_UNLOCK(p->adapter);

    cxgb_link_start(p);
    t3_link_changed(sc, p->port_id);
    ifp->if_baudrate = p->link_config.speed * 1000000;

    device_printf(sc->dev, "enabling interrupts on port=%d\n", p->port_id);
    t3_port_intr_enable(sc, p->port_id);

    callout_reset(&sc->cxgb_tick_ch, sc->params.stats_update_period * hz,
        cxgb_tick, sc);

    ifp->if_drv_flags |= IFF_DRV_RUNNING;
    ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
}

static void
cxgb_set_rxmode(struct port_info *p)
{
    struct t3_rx_mode rm;
    struct cmac *mac = &p->mac;

    PORT_LOCK_ASSERT_OWNED(p);

    t3_init_rx_mode(&rm, p);
    t3_mac_set_rx_mode(mac, &rm);
}

static void
cxgb_stop_locked(struct port_info *p)
{
    struct ifnet *ifp;

    PORT_LOCK_ASSERT_OWNED(p);
    ADAPTER_LOCK_ASSERT_NOTOWNED(p->adapter);

    ifp = p->ifp;

    t3_port_intr_disable(p->adapter, p->port_id);
    ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
    p->phy.ops->power_down(&p->phy, 1);
    t3_mac_disable(&p->mac, MAC_DIRECTION_TX | MAC_DIRECTION_RX);

    ADAPTER_LOCK(p->adapter);
    clrbit(&p->adapter->open_device_map, p->port_id);


    if (p->adapter->open_device_map == 0) {
        cxgb_down_locked(p->adapter);
    } else
        ADAPTER_UNLOCK(p->adapter);

}

static int
cxgb_set_mtu(struct port_info *p, int mtu)
{
    struct ifnet *ifp = p->ifp;
    struct ifreq ifr;
    int error = 0;

    ifr.ifr_mtu = mtu;

    if ((mtu < ETHERMIN) || (mtu > ETHER_MAX_LEN_JUMBO))
        error = EINVAL;
    else if ((error = ifioctl_common(ifp, SIOCSIFMTU, &ifr)) == ENETRESET) {
        error = 0;
        PORT_LOCK(p);
        if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
            callout_stop(&p->adapter->cxgb_tick_ch);
            cxgb_stop_locked(p);
            cxgb_init_locked(p);
        }
        PORT_UNLOCK(p);
    }
    return (error);
}

static int
cxgb_ioctl(struct ifnet *ifp, unsigned long command, void *data)
{
    struct port_info *p = ifp->if_softc;
    struct ifaddr *ifa = (struct ifaddr *)data;
    struct ifreq *ifr = (struct ifreq *)data;
    int flags, error = 0;

    /*
     * XXX need to check that we aren't in the middle of an unload
     */
    printf("cxgb_ioctl(%d): command=%08lx\n", __LINE__, command);
    switch (command) {
    case SIOCSIFMTU:
        error = cxgb_set_mtu(p, ifr->ifr_mtu);
	printf("SIOCSIFMTU: error=%d\n", error);
        break;
    case SIOCINITIFADDR:
	printf("SIOCINITIFADDR:\n");
        PORT_LOCK(p);
        if (ifa->ifa_addr->sa_family == AF_INET) {
            ifp->if_flags |= IFF_UP;
            if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
                cxgb_init_locked(p);
            arp_ifinit(ifp, ifa);
        } else
            error = ether_ioctl(ifp, command, data);
        PORT_UNLOCK(p);
        break;
    case SIOCSIFFLAGS:
	printf("SIOCSIFFLAGS:\n");
#if 0
	if ((error = ifioctl_common(ifp, cmd, data)) != 0)
		break;
#endif
        callout_drain(&p->adapter->cxgb_tick_ch);
        PORT_LOCK(p);
        if (ifp->if_flags & IFF_UP) {
            if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
                flags = p->if_flags;
                if (((ifp->if_flags ^ flags) & IFF_PROMISC) ||
                    ((ifp->if_flags ^ flags) & IFF_ALLMULTI))
                    cxgb_set_rxmode(p);
            } else
                cxgb_init_locked(p);
            p->if_flags = ifp->if_flags;
        } else if (ifp->if_drv_flags & IFF_DRV_RUNNING)
            cxgb_stop_locked(p);

        if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
            adapter_t *sc = p->adapter;
            callout_reset(&sc->cxgb_tick_ch,
                sc->params.stats_update_period * hz,
                cxgb_tick, sc);
        }
        PORT_UNLOCK(p);
        break;
    case SIOCSIFMEDIA:
	printf("SIOCSIFMEDIA:\n");
    case SIOCGIFMEDIA:
        error = ifmedia_ioctl(ifp, ifr, &p->media, command);
	printf("SIOCGIFMEDIA: error=%d\n", error);
        break;
    default:
	printf("Dir = %x  Len = %x  Group = '%c'  Num = %x\n",
		(unsigned int)(command&0xe0000000)>>28, (unsigned int)(command&0x1fff0000)>>16,
		(unsigned int)(command&0xff00)>>8, (unsigned int)command&0xff);
        if ((error = ether_ioctl(ifp, command, data)) != ENETRESET)
		break;
	error = 0;
        break;
    }
    return (error);
}

static int
cxgb_start_tx(struct ifnet *ifp, uint32_t txmax)
{
    struct sge_qset *qs;
    struct sge_txq *txq;
    struct port_info *p = ifp->if_softc;
    struct mbuf *m = NULL;
    int err, in_use_init, free_it;

    if (!p->link_config.link_ok)
    {
        return (ENXIO);
    }

    if (IFQ_IS_EMPTY(&ifp->if_snd))
    {
        return (ENOBUFS);
    }

    qs = &p->adapter->sge.qs[p->first_qset];
    txq = &qs->txq[TXQ_ETH];
    err = 0;

    if (txq->flags & TXQ_TRANSMITTING)
    {
        return (EINPROGRESS);
    }

    mtx_lock(&txq->lock);
    txq->flags |= TXQ_TRANSMITTING;
    in_use_init = txq->in_use;
    while ((txq->in_use - in_use_init < txmax) &&
        (txq->size > txq->in_use + TX_MAX_DESC)) {
        free_it = 0;
        IFQ_DEQUEUE(&ifp->if_snd, m);
        if (m == NULL)
            break;
        /*
         * Convert chain to M_IOVEC
         */
        KASSERT((m->m_flags & M_IOVEC) == 0);
#ifdef notyet
        m0 = m;
        if (collapse_mbufs && m->m_pkthdr.len > MCLBYTES &&
            m_collapse(m, TX_MAX_SEGS, &m0) == EFBIG) {
            if ((m0 = m_defrag(m, M_NOWAIT)) != NULL) {
                m = m0;
                m_collapse(m, TX_MAX_SEGS, &m0);
            } else
                break;
        }
        m = m0;
#endif
        if ((err = t3_encap(p, &m, &free_it)) != 0)
        {
            printf("t3_encap() returned %d\n", err);
            break;
        }
//        bpf_mtap(ifp, m);
        if (free_it)
	{
            m_freem(m);
	}
    }
    txq->flags &= ~TXQ_TRANSMITTING;
    mtx_unlock(&txq->lock);

    if (__predict_false(err)) {
        if (err == ENOMEM) {
            ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	// XXXXXXXXXX lock/unlock??
            IF_PREPEND(&ifp->if_snd, m);
        }
    }
    if (err == 0 && m == NULL)
        err = ENOBUFS;
    else if ((err == 0) &&  (txq->size <= txq->in_use + TX_MAX_DESC) &&
        (ifp->if_drv_flags & IFF_DRV_OACTIVE) == 0) {
        ifp->if_drv_flags |= IFF_DRV_OACTIVE;
        err = ENOSPC;
    }
    return (err);
}

static void
cxgb_start_proc(struct work *wk, void *arg)
{
    struct ifnet *ifp = arg;
    struct port_info *pi = ifp->if_softc;
    struct sge_qset *qs;
    struct sge_txq *txq;
    int error;

    qs = &pi->adapter->sge.qs[pi->first_qset];
    txq = &qs->txq[TXQ_ETH];

    do {
        if (desc_reclaimable(txq) > TX_CLEAN_MAX_DESC >> 2)
            workqueue_enqueue(pi->timer_reclaim_task.wq, &pi->timer_reclaim_task.w, NULL);

        error = cxgb_start_tx(ifp, TX_START_MAX_DESC);
    } while (error == 0);
}

static void
cxgb_start(struct ifnet *ifp)
{
    struct port_info *pi = ifp->if_softc;
    struct sge_qset *qs;
    struct sge_txq *txq;
    int err;

    qs = &pi->adapter->sge.qs[pi->first_qset];
    txq = &qs->txq[TXQ_ETH];

    if (desc_reclaimable(txq) > TX_CLEAN_MAX_DESC >> 2)
        workqueue_enqueue(pi->timer_reclaim_task.wq, &pi->timer_reclaim_task.w, NULL);

    err = cxgb_start_tx(ifp, TX_START_MAX_DESC);

    if (err == 0)
        workqueue_enqueue(pi->start_task.wq, &pi->start_task.w, NULL);
}

static void
cxgb_stop(struct ifnet *ifp, int reason)
{
    struct port_info *pi = ifp->if_softc;

    printf("cxgb_stop(): pi=%p, reason=%d\n", pi, reason);
    INT3;
}

static int
cxgb_media_change(struct ifnet *ifp)
{
    printf("media change not supported: ifp=%p\n", ifp);
    return (ENXIO);
}

static void
cxgb_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
    struct port_info *p;

    p = ifp->if_softc;

    ifmr->ifm_status = IFM_AVALID;
    ifmr->ifm_active = IFM_ETHER;

    if (!p->link_config.link_ok)
        return;

    ifmr->ifm_status |= IFM_ACTIVE;

    switch (p->link_config.speed) {
    case 10:
        ifmr->ifm_active |= IFM_10_T;
        break;
    case 100:
        ifmr->ifm_active |= IFM_100_TX;
            break;
    case 1000:
        ifmr->ifm_active |= IFM_1000_T;
        break;
    }

    if (p->link_config.duplex)
        ifmr->ifm_active |= IFM_FDX;
    else
        ifmr->ifm_active |= IFM_HDX;
}

static int
cxgb_async_intr(void *data)
{
    adapter_t *sc = data;

    if (cxgb_debug)
        device_printf(sc->dev, "cxgb_async_intr\n");
    /*
     * May need to sleep - defer to taskqueue
     */
    workqueue_enqueue(sc->slow_intr_task.wq, &sc->slow_intr_task.w, NULL);

    return (1);
}

static void
cxgb_ext_intr_handler(struct work *wk, void *arg)
{
    adapter_t *sc = (adapter_t *)arg;

    if (cxgb_debug)
        printf("cxgb_ext_intr_handler\n");

    t3_phy_intr_handler(sc);

    /* Now reenable external interrupts */
    ADAPTER_LOCK(sc);
    if (sc->slow_intr_mask) {
        sc->slow_intr_mask |= F_T3DBG;
        t3_write_reg(sc, A_PL_INT_CAUSE0, F_T3DBG);
        t3_write_reg(sc, A_PL_INT_ENABLE0, sc->slow_intr_mask);
    }
    ADAPTER_UNLOCK(sc);
}

static void
check_link_status(adapter_t *sc)
{
    int i;

    for (i = 0; i < (sc)->params.nports; ++i) {
        struct port_info *p = &sc->port[i];

        if (!(p->port_type->caps & SUPPORTED_IRQ))
            t3_link_changed(sc, i);
        p->ifp->if_baudrate = p->link_config.speed * 1000000;
    }
}

static void
check_t3b2_mac(struct adapter *adapter)
{
    int i;

    for_each_port(adapter, i) {
        struct port_info *p = &adapter->port[i];
        struct ifnet *ifp = p->ifp;
        int status;

        if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
            continue;

        status = 0;
        PORT_LOCK(p);
        if ((ifp->if_drv_flags & IFF_DRV_RUNNING))
            status = t3b2_mac_watchdog_task(&p->mac);
        if (status == 1)
            p->mac.stats.num_toggled++;
        else if (status == 2) {
            struct cmac *mac = &p->mac;

            t3_mac_set_mtu(mac, ifp->if_mtu + ETHER_HDR_LEN
                + ETHER_VLAN_ENCAP_LEN);
            t3_mac_set_address(mac, 0, p->hw_addr);
            cxgb_set_rxmode(p);
            t3_link_start(&p->phy, mac, &p->link_config);
            t3_mac_enable(mac, MAC_DIRECTION_RX | MAC_DIRECTION_TX);
            t3_port_intr_enable(adapter, p->port_id);
            p->mac.stats.num_resets++;
        }
        PORT_UNLOCK(p);
    }
}

static void
cxgb_tick(void *arg)
{
    adapter_t *sc = (adapter_t *)arg;

    workqueue_enqueue(sc->tick_task.wq, &sc->tick_task.w, NULL);

    if (sc->open_device_map != 0)
        callout_reset(&sc->cxgb_tick_ch, sc->params.stats_update_period * hz,
            cxgb_tick, sc);
}

static void
cxgb_tick_handler(struct work *wk, void *arg)
{
    adapter_t *sc = (adapter_t *)arg;
    const struct adapter_params *p = &sc->params;

    ADAPTER_LOCK(sc);
    if (p->linkpoll_period)
        check_link_status(sc);

    /*
     * adapter lock can currently only be acquire after the
     * port lock
     */
    ADAPTER_UNLOCK(sc);

    if (p->rev == T3_REV_B2 && p->nports < 4)
        check_t3b2_mac(sc);
}

static void
touch_bars(device_t dev)
{
    /*
     * Don't enable yet
     */
#if !defined(__LP64__) && 0
    u32 v;

    pci_read_config_dword(pdev, PCI_BASE_ADDRESS_1, &v);
    pci_write_config_dword(pdev, PCI_BASE_ADDRESS_1, v);
    pci_read_config_dword(pdev, PCI_BASE_ADDRESS_3, &v);
    pci_write_config_dword(pdev, PCI_BASE_ADDRESS_3, v);
    pci_read_config_dword(pdev, PCI_BASE_ADDRESS_5, &v);
    pci_write_config_dword(pdev, PCI_BASE_ADDRESS_5, v);
#endif
}

static __inline void
reg_block_dump(struct adapter *ap, uint8_t *buf, unsigned int start,
    unsigned int end)
{
    uint32_t *p = (uint32_t *)buf + start;

    for ( ; start <= end; start += sizeof(uint32_t))
        *p++ = t3_read_reg(ap, start);
}

