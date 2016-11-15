/******************************************************************************

  Copyright (c) 2001-2013, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
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

******************************************************************************/
/*$FreeBSD: head/sys/dev/ixgbe/ixv.c 275358 2014-12-01 11:45:24Z hselasky $*/
/*$NetBSD: ixv.c,v 1.15 2015/08/17 06:16:03 knakahara Exp $*/

#include "opt_inet.h"
#include "opt_inet6.h"

#include "ixv.h"
#include "vlan.h"

/*********************************************************************
 *  Driver version
 *********************************************************************/
char ixv_driver_version[] = "1.1.4";

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into ixv_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/

static ixv_vendor_info_t ixv_vendor_info_array[] =
{
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_VF, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X540_VF, 0, 0, 0},
	/* required last entry */
	{0, 0, 0, 0, 0}
};

/*********************************************************************
 *  Table of branding strings
 *********************************************************************/

static const char    *ixv_strings[] = {
	"Intel(R) PRO/10GbE Virtual Function Network Driver"
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static int      ixv_probe(device_t, cfdata_t, void *);
static void      ixv_attach(device_t, device_t, void *);
static int      ixv_detach(device_t, int);
#if 0
static int      ixv_shutdown(device_t);
#endif
#if __FreeBSD_version < 800000
static void     ixv_start(struct ifnet *);
static void     ixv_start_locked(struct tx_ring *, struct ifnet *);
#else
static int	ixv_mq_start(struct ifnet *, struct mbuf *);
static int	ixv_mq_start_locked(struct ifnet *,
		    struct tx_ring *, struct mbuf *);
static void	ixv_qflush(struct ifnet *);
#endif
static int      ixv_ioctl(struct ifnet *, u_long, void *);
static int	ixv_init(struct ifnet *);
static void	ixv_init_locked(struct adapter *);
static void     ixv_stop(void *);
static void     ixv_media_status(struct ifnet *, struct ifmediareq *);
static int      ixv_media_change(struct ifnet *);
static void     ixv_identify_hardware(struct adapter *);
static int      ixv_allocate_pci_resources(struct adapter *,
		    const struct pci_attach_args *);
static int      ixv_allocate_msix(struct adapter *,
		    const struct pci_attach_args *);
static int	ixv_allocate_queues(struct adapter *);
static int	ixv_setup_msix(struct adapter *);
static void	ixv_free_pci_resources(struct adapter *);
static void     ixv_local_timer(void *);
static void     ixv_setup_interface(device_t, struct adapter *);
static void     ixv_config_link(struct adapter *);

static int      ixv_allocate_transmit_buffers(struct tx_ring *);
static int	ixv_setup_transmit_structures(struct adapter *);
static void	ixv_setup_transmit_ring(struct tx_ring *);
static void     ixv_initialize_transmit_units(struct adapter *);
static void     ixv_free_transmit_structures(struct adapter *);
static void     ixv_free_transmit_buffers(struct tx_ring *);

static int      ixv_allocate_receive_buffers(struct rx_ring *);
static int      ixv_setup_receive_structures(struct adapter *);
static int	ixv_setup_receive_ring(struct rx_ring *);
static void     ixv_initialize_receive_units(struct adapter *);
static void     ixv_free_receive_structures(struct adapter *);
static void     ixv_free_receive_buffers(struct rx_ring *);

static void     ixv_enable_intr(struct adapter *);
static void     ixv_disable_intr(struct adapter *);
static bool	ixv_txeof(struct tx_ring *);
static bool	ixv_rxeof(struct ix_queue *, int);
static void	ixv_rx_checksum(u32, struct mbuf *, u32,
		    struct ixgbevf_hw_stats *);
static void     ixv_set_multi(struct adapter *);
static void     ixv_update_link_status(struct adapter *);
static void	ixv_refresh_mbufs(struct rx_ring *, int);
static int      ixv_xmit(struct tx_ring *, struct mbuf *);
static int	ixv_sysctl_stats(SYSCTLFN_PROTO);
static int	ixv_sysctl_debug(SYSCTLFN_PROTO);
static int	ixv_set_flowcntl(SYSCTLFN_PROTO);
static int	ixv_dma_malloc(struct adapter *, bus_size_t,
		    struct ixv_dma_alloc *, int);
static void     ixv_dma_free(struct adapter *, struct ixv_dma_alloc *);
static void	ixv_add_rx_process_limit(struct adapter *, const char *,
		    const char *, int *, int);
static u32	ixv_tx_ctx_setup(struct tx_ring *, struct mbuf *);
static bool	ixv_tso_setup(struct tx_ring *, struct mbuf *, u32 *);
static void	ixv_set_ivar(struct adapter *, u8, u8, s8);
static void	ixv_configure_ivars(struct adapter *);
static u8 *	ixv_mc_array_itr(struct ixgbe_hw *, u8 **, u32 *);

static void	ixv_setup_vlan_support(struct adapter *);
#if 0
static void	ixv_register_vlan(void *, struct ifnet *, u16);
static void	ixv_unregister_vlan(void *, struct ifnet *, u16);
#endif

static void	ixv_save_stats(struct adapter *);
static void	ixv_init_stats(struct adapter *);
static void	ixv_update_stats(struct adapter *);

static __inline void ixv_rx_discard(struct rx_ring *, int);
static __inline void ixv_rx_input(struct rx_ring *, struct ifnet *,
		    struct mbuf *, u32);

/* The MSI/X Interrupt handlers */
static int	ixv_msix_que(void *);
static int	ixv_msix_mbx(void *);

/* Deferred interrupt tasklets */
static void	ixv_handle_que(void *);
static void	ixv_handle_mbx(void *);

const struct sysctlnode *ixv_sysctl_instance(struct adapter *);
static ixv_vendor_info_t *ixv_lookup(const struct pci_attach_args *);

/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/

CFATTACH_DECL3_NEW(ixv, sizeof(struct adapter),
    ixv_probe, ixv_attach, ixv_detach, NULL, NULL, NULL,
    DVF_DETACH_SHUTDOWN);

# if 0
static device_method_t ixv_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ixv_probe),
	DEVMETHOD(device_attach, ixv_attach),
	DEVMETHOD(device_detach, ixv_detach),
	DEVMETHOD(device_shutdown, ixv_shutdown),
	DEVMETHOD_END
};
#endif

#if 0
static driver_t ixv_driver = {
	"ix", ixv_methods, sizeof(struct adapter),
};

extern devclass_t ixgbe_devclass;
DRIVER_MODULE(ixv, pci, ixv_driver, ixgbe_devclass, 0, 0);
MODULE_DEPEND(ixv, pci, 1, 1, 1);
MODULE_DEPEND(ixv, ether, 1, 1, 1);
#endif

/*
** TUNEABLE PARAMETERS:
*/

/*
** AIM: Adaptive Interrupt Moderation
** which means that the interrupt rate
** is varied over time based on the
** traffic for that interrupt vector
*/
static int ixv_enable_aim = FALSE;
#define	TUNABLE_INT(__x, __y)
TUNABLE_INT("hw.ixv.enable_aim", &ixv_enable_aim);

/* How many packets rxeof tries to clean at a time */
static int ixv_rx_process_limit = 128;
TUNABLE_INT("hw.ixv.rx_process_limit", &ixv_rx_process_limit);

/* Flow control setting, default to full */
static int ixv_flow_control = ixgbe_fc_full;
TUNABLE_INT("hw.ixv.flow_control", &ixv_flow_control);

/*
 * Header split: this causes the hardware to DMA
 * the header into a seperate mbuf from the payload,
 * it can be a performance win in some workloads, but
 * in others it actually hurts, its off by default.
 */
static int ixv_header_split = FALSE;
TUNABLE_INT("hw.ixv.hdr_split", &ixv_header_split);

/*
** Number of TX descriptors per ring,
** setting higher than RX as this seems
** the better performing choice.
*/
static int ixv_txd = DEFAULT_TXD;
TUNABLE_INT("hw.ixv.txd", &ixv_txd);

/* Number of RX descriptors per ring */
static int ixv_rxd = DEFAULT_RXD;
TUNABLE_INT("hw.ixv.rxd", &ixv_rxd);

/*
** Shadow VFTA table, this is needed because
** the real filter table gets cleared during
** a soft reset and we need to repopulate it.
*/
static u32 ixv_shadow_vfta[VFTA_SIZE];

/* Keep running tab on them for sanity check */
static int ixv_total_ports;

/*********************************************************************
 *  Device identification routine
 *
 *  ixv_probe determines if the driver should be loaded on
 *  adapter based on PCI vendor/device id of the adapter.
 *
 *  return 1 on success, 0 on failure
 *********************************************************************/

static int
ixv_probe(device_t dev, cfdata_t cf, void *aux)
{
	const struct pci_attach_args *pa = aux;

	return (ixv_lookup(pa) != NULL) ? 1 : 0;
}

static ixv_vendor_info_t *
ixv_lookup(const struct pci_attach_args *pa)
{
	pcireg_t subid;
	ixv_vendor_info_t *ent;

	INIT_DEBUGOUT("ixv_probe: begin");

	if (PCI_VENDOR(pa->pa_id) != IXGBE_INTEL_VENDOR_ID)
		return NULL;

	subid = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

	for (ent = ixv_vendor_info_array; ent->vendor_id != 0; ent++) {
		if ((PCI_VENDOR(pa->pa_id) == ent->vendor_id) &&
		    (PCI_PRODUCT(pa->pa_id) == ent->device_id) &&

		    ((PCI_SUBSYS_VENDOR(subid) == ent->subvendor_id) ||
		     (ent->subvendor_id == 0)) &&

		    ((PCI_SUBSYS_ID(subid) == ent->subdevice_id) ||
		     (ent->subdevice_id == 0))) {
			++ixv_total_ports;
			return ent;
		}
	}
	return NULL;
}


static void
ixv_sysctl_attach(struct adapter *adapter)
{
	struct sysctllog **log;
	const struct sysctlnode *rnode, *cnode;
	device_t dev;

	dev = adapter->dev;
	log = &adapter->sysctllog;

	if ((rnode = ixv_sysctl_instance(adapter)) == NULL) {
		aprint_error_dev(dev, "could not create sysctl root\n");
		return;
	}

	if (sysctl_createv(log, 0, &rnode, &cnode,
	    CTLFLAG_READWRITE, CTLTYPE_INT,
	    "stats", SYSCTL_DESCR("Statistics"),
	    ixv_sysctl_stats, 0, (void *)adapter, 0, CTL_CREATE, CTL_EOL) != 0)
		aprint_error_dev(dev, "could not create sysctl\n");

	if (sysctl_createv(log, 0, &rnode, &cnode,
	    CTLFLAG_READWRITE, CTLTYPE_INT,
	    "debug", SYSCTL_DESCR("Debug Info"),
	    ixv_sysctl_debug, 0, (void *)adapter, 0, CTL_CREATE, CTL_EOL) != 0)
		aprint_error_dev(dev, "could not create sysctl\n");

	if (sysctl_createv(log, 0, &rnode, &cnode,
	    CTLFLAG_READWRITE, CTLTYPE_INT,
	    "flow_control", SYSCTL_DESCR("Flow Control"),
	    ixv_set_flowcntl, 0, (void *)adapter, 0, CTL_CREATE, CTL_EOL) != 0)
		aprint_error_dev(dev, "could not create sysctl\n");

	/* XXX This is an *instance* sysctl controlling a *global* variable.
	 * XXX It's that way in the FreeBSD driver that this derives from.
	 */
	if (sysctl_createv(log, 0, &rnode, &cnode,
	    CTLFLAG_READWRITE, CTLTYPE_INT,
	    "enable_aim", SYSCTL_DESCR("Interrupt Moderation"),
	    NULL, 0, &ixv_enable_aim, 0, CTL_CREATE, CTL_EOL) != 0)
		aprint_error_dev(dev, "could not create sysctl\n");
}

/*********************************************************************
 *  Device initialization routine
 *
 *  The attach entry point is called when the driver is being loaded.
 *  This routine identifies the type of hardware, allocates all resources
 *  and initializes the hardware.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/

static void
ixv_attach(device_t parent, device_t dev, void *aux)
{
	struct adapter *adapter;
	struct ixgbe_hw *hw;
	int             error = 0;
	ixv_vendor_info_t *ent;
	const struct pci_attach_args *pa = aux;

	INIT_DEBUGOUT("ixv_attach: begin");

	/* Allocate, clear, and link in our adapter structure */
	adapter = device_private(dev);
	adapter->dev = adapter->osdep.dev = dev;
	hw = &adapter->hw;
	adapter->osdep.pc = pa->pa_pc;
	adapter->osdep.tag = pa->pa_tag;
	adapter->osdep.dmat = pa->pa_dmat;
	adapter->osdep.attached = false;

	ent = ixv_lookup(pa);

	KASSERT(ent != NULL);

	aprint_normal(": %s, Version - %s\n",
	    ixv_strings[ent->index], ixv_driver_version);

	/* Core Lock Init*/
	IXV_CORE_LOCK_INIT(adapter, device_xname(dev));

	/* SYSCTL APIs */
	ixv_sysctl_attach(adapter);

	/* Set up the timer callout */
	callout_init(&adapter->timer, 0);

	/* Determine hardware revision */
	ixv_identify_hardware(adapter);

	/* Do base PCI setup - map BAR0 */
	if (ixv_allocate_pci_resources(adapter, pa)) {
		aprint_error_dev(dev, "Allocation of PCI resources failed\n");
		error = ENXIO;
		goto err_out;
	}

	/* Do descriptor calc and sanity checks */
	if (((ixv_txd * sizeof(union ixgbe_adv_tx_desc)) % DBA_ALIGN) != 0 ||
	    ixv_txd < MIN_TXD || ixv_txd > MAX_TXD) {
		aprint_error_dev(dev, "TXD config issue, using default!\n");
		adapter->num_tx_desc = DEFAULT_TXD;
	} else
		adapter->num_tx_desc = ixv_txd;

	if (((ixv_rxd * sizeof(union ixgbe_adv_rx_desc)) % DBA_ALIGN) != 0 ||
	    ixv_rxd < MIN_RXD || ixv_rxd > MAX_RXD) {
		aprint_error_dev(dev, "RXD config issue, using default!\n");
		adapter->num_rx_desc = DEFAULT_RXD;
	} else
		adapter->num_rx_desc = ixv_rxd;

	/* Allocate our TX/RX Queues */
	if (ixv_allocate_queues(adapter)) {
		error = ENOMEM;
		goto err_out;
	}

	/*
	** Initialize the shared code: its
	** at this point the mac type is set.
	*/
	error = ixgbe_init_shared_code(hw);
	if (error) {
		aprint_error_dev(dev,"Shared Code Initialization Failure\n");
		error = EIO;
		goto err_late;
	}

	/* Setup the mailbox */
	ixgbe_init_mbx_params_vf(hw);

	ixgbe_reset_hw(hw);

	/* Get Hardware Flow Control setting */
	hw->fc.requested_mode = ixgbe_fc_full;
	hw->fc.pause_time = IXV_FC_PAUSE;
	hw->fc.low_water[0] = IXV_FC_LO;
	hw->fc.high_water[0] = IXV_FC_HI;
	hw->fc.send_xon = TRUE;

	error = ixgbe_init_hw(hw);
	if (error) {
		aprint_error_dev(dev,"Hardware Initialization Failure\n");
		error = EIO;
		goto err_late;
	}
	
	error = ixv_allocate_msix(adapter, pa); 
	if (error) 
		goto err_late;

	/* Setup OS specific network interface */
	ixv_setup_interface(dev, adapter);

	/* Sysctl for limiting the amount of work done in the taskqueue */
	ixv_add_rx_process_limit(adapter, "rx_processing_limit",
	    "max number of rx packets to process", &adapter->rx_process_limit,
	    ixv_rx_process_limit);

	/* Do the stats setup */
	ixv_save_stats(adapter);
	ixv_init_stats(adapter);

	/* Register for VLAN events */
#if 0 /* XXX delete after write? */
	adapter->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    ixv_register_vlan, adapter, EVENTHANDLER_PRI_FIRST);
	adapter->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    ixv_unregister_vlan, adapter, EVENTHANDLER_PRI_FIRST);
#endif

	INIT_DEBUGOUT("ixv_attach: end");
	adapter->osdep.attached = true;
	return;

err_late:
	ixv_free_transmit_structures(adapter);
	ixv_free_receive_structures(adapter);
err_out:
	ixv_free_pci_resources(adapter);
	return;

}

/*********************************************************************
 *  Device removal routine
 *
 *  The detach entry point is called when the driver is being removed.
 *  This routine stops the adapter and deallocates all the resources
 *  that were allocated for driver operation.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/

static int
ixv_detach(device_t dev, int flags)
{
	struct adapter *adapter = device_private(dev);
	struct ix_queue *que = adapter->queues;

	INIT_DEBUGOUT("ixv_detach: begin");
	if (adapter->osdep.attached == false)
		return 0;

#if NVLAN > 0
	/* Make sure VLANS are not using driver */
	if (!VLAN_ATTACHED(&adapter->osdep.ec))
		;	/* nothing to do: no VLANs */ 
	else if ((flags & (DETACH_SHUTDOWN|DETACH_FORCE)) != 0)
		vlan_ifdetach(adapter->ifp);
	else {
		aprint_error_dev(dev, "VLANs in use\n");
		return EBUSY;
	}
#endif

	IXV_CORE_LOCK(adapter);
	ixv_stop(adapter);
	IXV_CORE_UNLOCK(adapter);

	for (int i = 0; i < adapter->num_queues; i++, que++) {
		softint_disestablish(que->que_si);
	}

	/* Drain the Link queue */
	softint_disestablish(adapter->mbx_si);

	/* Unregister VLAN events */
#if 0 /* XXX msaitoh delete after write? */
	if (adapter->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, adapter->vlan_attach);
	if (adapter->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, adapter->vlan_detach);
#endif

	ether_ifdetach(adapter->ifp);
	callout_halt(&adapter->timer, NULL);
	ixv_free_pci_resources(adapter);
#if 0 /* XXX the NetBSD port is probably missing something here */
	bus_generic_detach(dev);
#endif
	if_detach(adapter->ifp);

	ixv_free_transmit_structures(adapter);
	ixv_free_receive_structures(adapter);

	IXV_CORE_LOCK_DESTROY(adapter);
	return (0);
}

/*********************************************************************
 *
 *  Shutdown entry point
 *
 **********************************************************************/
#if 0 /* XXX NetBSD ought to register something like this through pmf(9) */
static int
ixv_shutdown(device_t dev)
{
	struct adapter *adapter = device_private(dev);
	IXV_CORE_LOCK(adapter);
	ixv_stop(adapter);
	IXV_CORE_UNLOCK(adapter);
	return (0);
}
#endif

#if __FreeBSD_version < 800000
/*********************************************************************
 *  Transmit entry point
 *
 *  ixv_start is called by the stack to initiate a transmit.
 *  The driver will remain in this routine as long as there are
 *  packets to transmit and transmit resources are available.
 *  In case resources are not available stack is notified and
 *  the packet is requeued.
 **********************************************************************/
static void
ixv_start_locked(struct tx_ring *txr, struct ifnet * ifp)
{
	int rc;
	struct mbuf    *m_head;
	struct adapter *adapter = txr->adapter;

	IXV_TX_LOCK_ASSERT(txr);

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) !=
	    IFF_RUNNING)
		return;
	if (!adapter->link_active)
		return;

	while (!IFQ_IS_EMPTY(&ifp->if_snd)) {

		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if ((rc = ixv_xmit(txr, m_head)) == EAGAIN) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		if (rc == EFBIG) {
			struct mbuf *mtmp;

			if ((mtmp = m_defrag(m_head, M_NOWAIT)) != NULL) {
				m_head = mtmp;
				rc = ixv_xmit(txr, m_head);
				if (rc != 0)
					adapter->efbig2_tx_dma_setup.ev_count++;
			} else
				adapter->m_defrag_failed.ev_count++;
		}
		if (rc != 0) {
			m_freem(m_head);
			continue;
		}
		/* Send a copy of the frame to the BPF listener */
		bpf_mtap(ifp, m_head);

		/* Set watchdog on */
		txr->watchdog_check = TRUE;
		getmicrotime(&txr->watchdog_time);
	}
	return;
}

/*
 * Legacy TX start - called by the stack, this
 * always uses the first tx ring, and should
 * not be used with multiqueue tx enabled.
 */
static void
ixv_start(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;
	struct tx_ring	*txr = adapter->tx_rings;

	if (ifp->if_flags & IFF_RUNNING) {
		IXV_TX_LOCK(txr);
		ixv_start_locked(txr, ifp);
		IXV_TX_UNLOCK(txr);
	}
	return;
}

#else

/*
** Multiqueue Transmit driver
**
*/
static int
ixv_mq_start(struct ifnet *ifp, struct mbuf *m)
{
	struct adapter	*adapter = ifp->if_softc;
	struct ix_queue	*que;
	struct tx_ring	*txr;
	int 		i = 0, err = 0;

	/* Which queue to use */
	if ((m->m_flags & M_FLOWID) != 0)
		i = m->m_pkthdr.flowid % adapter->num_queues;

	txr = &adapter->tx_rings[i];
	que = &adapter->queues[i];

	if (IXV_TX_TRYLOCK(txr)) {
		err = ixv_mq_start_locked(ifp, txr, m);
		IXV_TX_UNLOCK(txr);
	} else {
		err = drbr_enqueue(ifp, txr->br, m);
		softint_schedule(que->que_si);
	}

	return (err);
}

static int
ixv_mq_start_locked(struct ifnet *ifp, struct tx_ring *txr, struct mbuf *m)
{
	struct adapter  *adapter = txr->adapter;
        struct mbuf     *next;
        int             enqueued, err = 0;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) !=
	    IFF_RUNNING || adapter->link_active == 0) {
		if (m != NULL)
			err = drbr_enqueue(ifp, txr->br, m);
		return (err);
	}

	/* Do a clean if descriptors are low */
	if (txr->tx_avail <= IXV_TX_CLEANUP_THRESHOLD)
		ixv_txeof(txr);

	enqueued = 0;
	if (m != NULL) {
		err = drbr_dequeue(ifp, txr->br, m);
		if (err) {
			return (err);
		}
	}
	/* Process the queue */
	while ((next = drbr_peek(ifp, txr->br)) != NULL) {
		if ((err = ixv_xmit(txr, next)) != 0) {
			if (next != NULL) {
				drbr_advance(ifp, txr->br);
			} else {
				drbr_putback(ifp, txr->br, next);
			}
			break;
		}
		drbr_advance(ifp, txr->br);
		enqueued++;
		ifp->if_obytes += next->m_pkthdr.len;
		if (next->m_flags & M_MCAST)
			ifp->if_omcasts++;
		/* Send a copy of the frame to the BPF listener */
		ETHER_BPF_MTAP(ifp, next);
		if ((ifp->if_flags & IFF_RUNNING) == 0)
			break;
		if (txr->tx_avail <= IXV_TX_OP_THRESHOLD) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
	}

	if (enqueued > 0) {
		/* Set watchdog on */
		txr->watchdog_check = TRUE;
		getmicrotime(&txr->watchdog_time);
	}

	return (err);
}

/*
** Flush all ring buffers
*/
static void     
ixv_qflush(struct ifnet *ifp)
{
	struct adapter  *adapter = ifp->if_softc;
	struct tx_ring  *txr = adapter->tx_rings;
	struct mbuf     *m;

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		IXV_TX_LOCK(txr);
		while ((m = buf_ring_dequeue_sc(txr->br)) != NULL)
			m_freem(m);
		IXV_TX_UNLOCK(txr);
	}
	if_qflush(ifp);
}

#endif

static int
ixv_ifflags_cb(struct ethercom *ec)
{
	struct ifnet *ifp = &ec->ec_if;
	struct adapter *adapter = ifp->if_softc;
	int change = ifp->if_flags ^ adapter->if_flags, rc = 0;

	IXV_CORE_LOCK(adapter);

	if (change != 0)
		adapter->if_flags = ifp->if_flags;

	if ((change & ~(IFF_CANTCHANGE|IFF_DEBUG)) != 0)
		rc = ENETRESET;

	IXV_CORE_UNLOCK(adapter);

	return rc;
}

/*********************************************************************
 *  Ioctl entry point
 *
 *  ixv_ioctl is called when the user wants to configure the
 *  interface.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static int
ixv_ioctl(struct ifnet * ifp, u_long command, void *data)
{
	struct adapter	*adapter = ifp->if_softc;
	struct ifcapreq *ifcr = data;
	struct ifreq	*ifr = (struct ifreq *) data;
	int             error = 0;
	int l4csum_en;
	const int l4csum = IFCAP_CSUM_TCPv4_Rx|IFCAP_CSUM_UDPv4_Rx|
	     IFCAP_CSUM_TCPv6_Rx|IFCAP_CSUM_UDPv6_Rx;

	switch (command) {
	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFFLAGS (Set Interface Flags)");
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl: SIOC(ADD|DEL)MULTI");
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		IOCTL_DEBUGOUT("ioctl: SIOCxIFMEDIA (Get/Set Interface Media)");
		break;
	case SIOCSIFCAP:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFCAP (Set Capabilities)");
		break;
	case SIOCSIFMTU:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFMTU (Set Interface MTU)");
		break;
	default:
		IOCTL_DEBUGOUT1("ioctl: UNKNOWN (0x%X)\n", (int)command);
		break;
	}

	switch (command) {
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		return ifmedia_ioctl(ifp, ifr, &adapter->media, command);
	case SIOCSIFCAP:
		/* Layer-4 Rx checksum offload has to be turned on and
		 * off as a unit.
		 */
		l4csum_en = ifcr->ifcr_capenable & l4csum;
		if (l4csum_en != l4csum && l4csum_en != 0)
			return EINVAL;
		/*FALLTHROUGH*/
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	case SIOCSIFFLAGS:
	case SIOCSIFMTU:
	default:
		if ((error = ether_ioctl(ifp, command, data)) != ENETRESET)
			return error;
		if ((ifp->if_flags & IFF_RUNNING) == 0)
			;
		else if (command == SIOCSIFCAP || command == SIOCSIFMTU) {
			IXV_CORE_LOCK(adapter);
			ixv_init_locked(adapter);
			IXV_CORE_UNLOCK(adapter);
		} else if (command == SIOCADDMULTI || command == SIOCDELMULTI) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			IXV_CORE_LOCK(adapter);
			ixv_disable_intr(adapter);
			ixv_set_multi(adapter);
			ixv_enable_intr(adapter);
			IXV_CORE_UNLOCK(adapter);
		}
		return 0;
	}
}

/*********************************************************************
 *  Init entry point
 *
 *  This routine is used in two ways. It is used by the stack as
 *  init entry point in network interface structure. It is also used
 *  by the driver as a hw/sw initialization routine to get to a
 *  consistent state.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/
#define IXGBE_MHADD_MFS_SHIFT 16

static void
ixv_init_locked(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	device_t 	dev = adapter->dev;
	struct ixgbe_hw *hw = &adapter->hw;
	u32		mhadd, gpie;

	INIT_DEBUGOUT("ixv_init: begin");
	KASSERT(mutex_owned(&adapter->core_mtx));
	hw->adapter_stopped = FALSE;
	ixgbe_stop_adapter(hw);
        callout_stop(&adapter->timer);

        /* reprogram the RAR[0] in case user changed it. */
        ixgbe_set_rar(hw, 0, hw->mac.addr, 0, IXGBE_RAH_AV);

	/* Get the latest mac address, User can use a LAA */
	memcpy(hw->mac.addr, CLLADDR(adapter->ifp->if_sadl),
	     IXGBE_ETH_LENGTH_OF_ADDRESS);
        ixgbe_set_rar(hw, 0, hw->mac.addr, 0, 1);
	hw->addr_ctrl.rar_used_count = 1;

	/* Prepare transmit descriptors and buffers */
	if (ixv_setup_transmit_structures(adapter)) {
		aprint_error_dev(dev,"Could not setup transmit structures\n");
		ixv_stop(adapter);
		return;
	}

	ixgbe_reset_hw(hw);
	ixv_initialize_transmit_units(adapter);

	/* Setup Multicast table */
	ixv_set_multi(adapter);

	/*
	** Determine the correct mbuf pool
	** for doing jumbo/headersplit
	*/
	if (ifp->if_mtu > ETHERMTU)
		adapter->rx_mbuf_sz = MJUMPAGESIZE;
	else
		adapter->rx_mbuf_sz = MCLBYTES;

	/* Prepare receive descriptors and buffers */
	if (ixv_setup_receive_structures(adapter)) {
		device_printf(dev,"Could not setup receive structures\n");
		ixv_stop(adapter);
		return;
	}

	/* Configure RX settings */
	ixv_initialize_receive_units(adapter);

	/* Enable Enhanced MSIX mode */
	gpie = IXGBE_READ_REG(&adapter->hw, IXGBE_GPIE);
	gpie |= IXGBE_GPIE_MSIX_MODE | IXGBE_GPIE_EIAME;
	gpie |= IXGBE_GPIE_PBA_SUPPORT | IXGBE_GPIE_OCD;
        IXGBE_WRITE_REG(hw, IXGBE_GPIE, gpie);

#if 0 /* XXX isn't it required? -- msaitoh  */
	/* Set the various hardware offload abilities */
	ifp->if_hwassist = 0;
	if (ifp->if_capenable & IFCAP_TSO4)
		ifp->if_hwassist |= CSUM_TSO;
	if (ifp->if_capenable & IFCAP_TXCSUM) {
		ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
#if __FreeBSD_version >= 800000
		ifp->if_hwassist |= CSUM_SCTP;
#endif
	}
#endif
	
	/* Set MTU size */
	if (ifp->if_mtu > ETHERMTU) {
		mhadd = IXGBE_READ_REG(hw, IXGBE_MHADD);
		mhadd &= ~IXGBE_MHADD_MFS_MASK;
		mhadd |= adapter->max_frame_size << IXGBE_MHADD_MFS_SHIFT;
		IXGBE_WRITE_REG(hw, IXGBE_MHADD, mhadd);
	}

	/* Set up VLAN offload and filter */
	ixv_setup_vlan_support(adapter);

	callout_reset(&adapter->timer, hz, ixv_local_timer, adapter);

	/* Set up MSI/X routing */
	ixv_configure_ivars(adapter);

	/* Set up auto-mask */
	IXGBE_WRITE_REG(hw, IXGBE_VTEIAM, IXGBE_EICS_RTX_QUEUE);

        /* Set moderation on the Link interrupt */
        IXGBE_WRITE_REG(hw, IXGBE_VTEITR(adapter->mbxvec), IXV_LINK_ITR);

	/* Stats init */
	ixv_init_stats(adapter);

	/* Config/Enable Link */
	ixv_config_link(adapter);

	/* And now turn on interrupts */
	ixv_enable_intr(adapter);

	/* Now inform the stack we're ready */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	return;
}

static int
ixv_init(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;

	IXV_CORE_LOCK(adapter);
	ixv_init_locked(adapter);
	IXV_CORE_UNLOCK(adapter);
	return 0;
}


/*
**
** MSIX Interrupt Handlers and Tasklets
**
*/

static inline void
ixv_enable_queue(struct adapter *adapter, u32 vector)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32	queue = 1 << vector;
	u32	mask;

	mask = (IXGBE_EIMS_RTX_QUEUE & queue);
	IXGBE_WRITE_REG(hw, IXGBE_VTEIMS, mask);
}

static inline void
ixv_disable_queue(struct adapter *adapter, u32 vector)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u64	queue = (u64)(1 << vector);
	u32	mask;

	mask = (IXGBE_EIMS_RTX_QUEUE & queue);
	IXGBE_WRITE_REG(hw, IXGBE_VTEIMC, mask);
}

static inline void
ixv_rearm_queues(struct adapter *adapter, u64 queues)
{
	u32 mask = (IXGBE_EIMS_RTX_QUEUE & queues);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_VTEICS, mask);
}


static void
ixv_handle_que(void *context)
{
	struct ix_queue *que = context;
	struct adapter  *adapter = que->adapter;
	struct tx_ring  *txr = que->txr;
	struct ifnet    *ifp = adapter->ifp;
	bool		more;

	if (ifp->if_flags & IFF_RUNNING) {
		more = ixv_rxeof(que, adapter->rx_process_limit);
		IXV_TX_LOCK(txr);
		ixv_txeof(txr);
#if __FreeBSD_version >= 800000
		if (!drbr_empty(ifp, txr->br))
			ixv_mq_start_locked(ifp, txr, NULL);
#else
		if (!IFQ_IS_EMPTY(&ifp->if_snd))
			ixv_start_locked(txr, ifp);
#endif
		IXV_TX_UNLOCK(txr);
		if (more) {
			adapter->req.ev_count++;
			softint_schedule(que->que_si);
			return;
		}
	}

	/* Reenable this interrupt */
	ixv_enable_queue(adapter, que->msix);
	return;
}

/*********************************************************************
 *
 *  MSI Queue Interrupt Service routine
 *
 **********************************************************************/
int
ixv_msix_que(void *arg)
{
	struct ix_queue	*que = arg;
	struct adapter  *adapter = que->adapter;
	struct tx_ring	*txr = que->txr;
	struct rx_ring	*rxr = que->rxr;
	bool		more_tx, more_rx;
	u32		newitr = 0;

	ixv_disable_queue(adapter, que->msix);
	++que->irqs;

	more_rx = ixv_rxeof(que, adapter->rx_process_limit);

	IXV_TX_LOCK(txr);
	more_tx = ixv_txeof(txr);
	/*
	** Make certain that if the stack
	** has anything queued the task gets
	** scheduled to handle it.
	*/
#if __FreeBSD_version < 800000
	if (!IFQ_IS_EMPTY(&adapter->ifp->if_snd))
#else
	if (!drbr_empty(adapter->ifp, txr->br))
#endif
                more_tx = 1;
	IXV_TX_UNLOCK(txr);

	more_rx = ixv_rxeof(que, adapter->rx_process_limit);

	/* Do AIM now? */

	if (ixv_enable_aim == FALSE)
		goto no_calc;
	/*
	** Do Adaptive Interrupt Moderation:
        **  - Write out last calculated setting
	**  - Calculate based on average size over
	**    the last interval.
	*/
        if (que->eitr_setting)
                IXGBE_WRITE_REG(&adapter->hw,
                    IXGBE_VTEITR(que->msix),
		    que->eitr_setting);
 
        que->eitr_setting = 0;

        /* Idle, do nothing */
        if ((txr->bytes == 0) && (rxr->bytes == 0))
                goto no_calc;
                                
	if ((txr->bytes) && (txr->packets))
               	newitr = txr->bytes/txr->packets;
	if ((rxr->bytes) && (rxr->packets))
		newitr = max(newitr,
		    (rxr->bytes / rxr->packets));
	newitr += 24; /* account for hardware frame, crc */

	/* set an upper boundary */
	newitr = min(newitr, 3000);

	/* Be nice to the mid range */
	if ((newitr > 300) && (newitr < 1200))
		newitr = (newitr / 3);
	else
		newitr = (newitr / 2);

	newitr |= newitr << 16;
                 
        /* save for next interrupt */
        que->eitr_setting = newitr;

        /* Reset state */
        txr->bytes = 0;
        txr->packets = 0;
        rxr->bytes = 0;
        rxr->packets = 0;

no_calc:
	if (more_tx || more_rx)
		softint_schedule(que->que_si);
	else /* Reenable this interrupt */
		ixv_enable_queue(adapter, que->msix);
	return 1;
}

static int
ixv_msix_mbx(void *arg)
{
	struct adapter	*adapter = arg;
	struct ixgbe_hw *hw = &adapter->hw;
	u32		reg;

	++adapter->mbx_irq.ev_count;

	/* First get the cause */
	reg = IXGBE_READ_REG(hw, IXGBE_VTEICS);
	/* Clear interrupt with write */
	IXGBE_WRITE_REG(hw, IXGBE_VTEICR, reg);

	/* Link status change */
	if (reg & IXGBE_EICR_LSC)
		softint_schedule(adapter->mbx_si);

	IXGBE_WRITE_REG(hw, IXGBE_VTEIMS, IXGBE_EIMS_OTHER);
	return 1;
}

/*********************************************************************
 *
 *  Media Ioctl callback
 *
 *  This routine is called whenever the user queries the status of
 *  the interface using ifconfig.
 *
 **********************************************************************/
static void
ixv_media_status(struct ifnet * ifp, struct ifmediareq * ifmr)
{
	struct adapter *adapter = ifp->if_softc;

	INIT_DEBUGOUT("ixv_media_status: begin");
	IXV_CORE_LOCK(adapter);
	ixv_update_link_status(adapter);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!adapter->link_active) {
		IXV_CORE_UNLOCK(adapter);
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;

	switch (adapter->link_speed) {
		case IXGBE_LINK_SPEED_1GB_FULL:
			ifmr->ifm_active |= IFM_1000_T | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifmr->ifm_active |= IFM_FDX;
			break;
	}

	IXV_CORE_UNLOCK(adapter);

	return;
}

/*********************************************************************
 *
 *  Media Ioctl callback
 *
 *  This routine is called when the user changes speed/duplex using
 *  media/mediopt option with ifconfig.
 *
 **********************************************************************/
static int
ixv_media_change(struct ifnet * ifp)
{
	struct adapter *adapter = ifp->if_softc;
	struct ifmedia *ifm = &adapter->media;

	INIT_DEBUGOUT("ixv_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

        switch (IFM_SUBTYPE(ifm->ifm_media)) {
        case IFM_AUTO:
                break;
        default:
                device_printf(adapter->dev, "Only auto media type\n");
		return (EINVAL);
        }

	return (0);
}

/*********************************************************************
 *
 *  This routine maps the mbufs to tx descriptors, allowing the
 *  TX engine to transmit the packets. 
 *  	- return 0 on success, positive on failure
 *
 **********************************************************************/

static int
ixv_xmit(struct tx_ring *txr, struct mbuf *m_head)
{
	struct m_tag *mtag;
	struct adapter  *adapter = txr->adapter;
	struct ethercom *ec = &adapter->osdep.ec;
	u32		olinfo_status = 0, cmd_type_len;
	u32		paylen = 0;
	int             i, j, error;
	int		first, last = 0;
	bus_dmamap_t	map;
	struct ixv_tx_buf *txbuf;
	union ixgbe_adv_tx_desc *txd = NULL;

	/* Basic descriptor defines */
        cmd_type_len = (IXGBE_ADVTXD_DTYP_DATA |
	    IXGBE_ADVTXD_DCMD_IFCS | IXGBE_ADVTXD_DCMD_DEXT);

	if ((mtag = VLAN_OUTPUT_TAG(ec, m_head)) != NULL)
        	cmd_type_len |= IXGBE_ADVTXD_DCMD_VLE;

        /*
         * Important to capture the first descriptor
         * used because it will contain the index of
         * the one we tell the hardware to report back
         */
        first = txr->next_avail_desc;
	txbuf = &txr->tx_buffers[first];
	map = txbuf->map;

	/*
	 * Map the packet for DMA.
	 */
	error = bus_dmamap_load_mbuf(txr->txtag->dt_dmat, map,
	    m_head, BUS_DMA_NOWAIT);

	switch (error) {
	case EAGAIN:
		adapter->eagain_tx_dma_setup.ev_count++;
		return EAGAIN;
	case ENOMEM:
		adapter->enomem_tx_dma_setup.ev_count++;
		return EAGAIN;
	case EFBIG:
		adapter->efbig_tx_dma_setup.ev_count++;
		return error;
	case EINVAL:
		adapter->einval_tx_dma_setup.ev_count++;
		return error;
	default:
		adapter->other_tx_dma_setup.ev_count++;
		return error;
	case 0:
		break;
	}

	/* Make certain there are enough descriptors */
	if (map->dm_nsegs > txr->tx_avail - 2) {
		txr->no_desc_avail.ev_count++;
		/* XXX s/ixgbe/ixv/ */
		ixgbe_dmamap_unload(txr->txtag, txbuf->map);
		return EAGAIN;
	}

	/*
	** Set up the appropriate offload context
	** this becomes the first descriptor of 
	** a packet.
	*/
	if (m_head->m_pkthdr.csum_flags & (M_CSUM_TSOv4|M_CSUM_TSOv6)) {
		if (ixv_tso_setup(txr, m_head, &paylen)) {
			cmd_type_len |= IXGBE_ADVTXD_DCMD_TSE;
			olinfo_status |= IXGBE_TXD_POPTS_IXSM << 8;
			olinfo_status |= IXGBE_TXD_POPTS_TXSM << 8;
			olinfo_status |= paylen << IXGBE_ADVTXD_PAYLEN_SHIFT;
			++adapter->tso_tx.ev_count;
		} else {
			++adapter->tso_err.ev_count;
			/* XXX unload DMA map! --dyoung -> easy? --msaitoh */
			return (ENXIO);
		}
	} else
		olinfo_status |= ixv_tx_ctx_setup(txr, m_head);

        /* Record payload length */
	if (paylen == 0)
        	olinfo_status |= m_head->m_pkthdr.len <<
		    IXGBE_ADVTXD_PAYLEN_SHIFT;

	i = txr->next_avail_desc;
	for (j = 0; j < map->dm_nsegs; j++) {
		bus_size_t seglen;
		bus_addr_t segaddr;

		txbuf = &txr->tx_buffers[i];
		txd = &txr->tx_base[i];
		seglen = map->dm_segs[j].ds_len;
		segaddr = htole64(map->dm_segs[j].ds_addr);

		txd->read.buffer_addr = segaddr;
		txd->read.cmd_type_len = htole32(txr->txd_cmd |
		    cmd_type_len |seglen);
		txd->read.olinfo_status = htole32(olinfo_status);
		last = i; /* descriptor that will get completion IRQ */

		if (++i == adapter->num_tx_desc)
			i = 0;

		txbuf->m_head = NULL;
		txbuf->eop_index = -1;
	}

	txd->read.cmd_type_len |=
	    htole32(IXGBE_TXD_CMD_EOP | IXGBE_TXD_CMD_RS);
	txr->tx_avail -= map->dm_nsegs;
	txr->next_avail_desc = i;

	txbuf->m_head = m_head;
	/* Swap the dma map between the first and last descriptor */
	txr->tx_buffers[first].map = txbuf->map;
	txbuf->map = map;
	bus_dmamap_sync(txr->txtag->dt_dmat, map, 0, m_head->m_pkthdr.len,
	    BUS_DMASYNC_PREWRITE);

        /* Set the index of the descriptor that will be marked done */
        txbuf = &txr->tx_buffers[first];
	txbuf->eop_index = last;

	/* XXX s/ixgbe/ixg/ */
        ixgbe_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
            BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	/*
	 * Advance the Transmit Descriptor Tail (Tdt), this tells the
	 * hardware that this frame is available to transmit.
	 */
	++txr->total_packets.ev_count;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_VFTDT(txr->me), i);

	return 0;
}


/*********************************************************************
 *  Multicast Update
 *
 *  This routine is called whenever multicast address list is updated.
 *
 **********************************************************************/
#define IXGBE_RAR_ENTRIES 16

static void
ixv_set_multi(struct adapter *adapter)
{
	struct ether_multi *enm;
	struct ether_multistep step;
	u8	mta[MAX_NUM_MULTICAST_ADDRESSES * IXGBE_ETH_LENGTH_OF_ADDRESS];
	u8	*update_ptr;
	int	mcnt = 0;
	struct ethercom *ec = &adapter->osdep.ec;

	IOCTL_DEBUGOUT("ixv_set_multi: begin");

	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		bcopy(enm->enm_addrlo,
		    &mta[mcnt * IXGBE_ETH_LENGTH_OF_ADDRESS],
		    IXGBE_ETH_LENGTH_OF_ADDRESS);
		mcnt++;
		/* XXX This might be required --msaitoh */
		if (mcnt >= MAX_NUM_MULTICAST_ADDRESSES)
			break;
		ETHER_NEXT_MULTI(step, enm);
	}

	update_ptr = mta;

	ixgbe_update_mc_addr_list(&adapter->hw,
	    update_ptr, mcnt, ixv_mc_array_itr, TRUE);

	return;
}

/*
 * This is an iterator function now needed by the multicast
 * shared code. It simply feeds the shared code routine the
 * addresses in the array of ixv_set_multi() one by one.
 */
static u8 *
ixv_mc_array_itr(struct ixgbe_hw *hw, u8 **update_ptr, u32 *vmdq)
{
	u8 *addr = *update_ptr;
	u8 *newptr;
	*vmdq = 0;

	newptr = addr + IXGBE_ETH_LENGTH_OF_ADDRESS;
	*update_ptr = newptr;
	return addr;
}

/*********************************************************************
 *  Timer routine
 *
 *  This routine checks for link status,updates statistics,
 *  and runs the watchdog check.
 *
 **********************************************************************/

static void
ixv_local_timer1(void *arg)
{
	struct adapter	*adapter = arg;
	device_t	dev = adapter->dev;
	struct tx_ring	*txr = adapter->tx_rings;
	int		i;
	struct timeval now, elapsed;

	KASSERT(mutex_owned(&adapter->core_mtx));

	ixv_update_link_status(adapter);

	/* Stats Update */
	ixv_update_stats(adapter);

	/*
	 * If the interface has been paused
	 * then don't do the watchdog check
	 */
	if (IXGBE_READ_REG(&adapter->hw, IXGBE_TFCS) & IXGBE_TFCS_TXOFF)
		goto out;
	/*
	** Check for time since any descriptor was cleaned
	*/
        for (i = 0; i < adapter->num_queues; i++, txr++) {
		IXV_TX_LOCK(txr);
		if (txr->watchdog_check == FALSE) {
			IXV_TX_UNLOCK(txr);
			continue;
		}
		getmicrotime(&now);
		timersub(&now, &txr->watchdog_time, &elapsed);
		if (tvtohz(&elapsed) > IXV_WATCHDOG)
			goto hung;
		IXV_TX_UNLOCK(txr);
	}
out:
       	ixv_rearm_queues(adapter, adapter->que_mask);
	callout_reset(&adapter->timer, hz, ixv_local_timer, adapter);
	return;

hung:
	device_printf(adapter->dev, "Watchdog timeout -- resetting\n");
	device_printf(dev,"Queue(%d) tdh = %d, hw tdt = %d\n", txr->me,
	    IXGBE_READ_REG(&adapter->hw, IXGBE_VFTDH(i)),
	    IXGBE_READ_REG(&adapter->hw, IXGBE_VFTDT(i)));
	device_printf(dev,"TX(%d) desc avail = %d,"
	    "Next TX to Clean = %d\n",
	    txr->me, txr->tx_avail, txr->next_to_clean);
	adapter->ifp->if_flags &= ~IFF_RUNNING;
	adapter->watchdog_events.ev_count++;
	IXV_TX_UNLOCK(txr);
	ixv_init_locked(adapter);
}

static void
ixv_local_timer(void *arg)
{
	struct adapter *adapter = arg;

	IXV_CORE_LOCK(adapter);
	ixv_local_timer1(adapter);
	IXV_CORE_UNLOCK(adapter);
}

/*
** Note: this routine updates the OS on the link state
**	the real check of the hardware only happens with
**	a link interrupt.
*/
static void
ixv_update_link_status(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	struct tx_ring *txr = adapter->tx_rings;
	device_t dev = adapter->dev;


	if (adapter->link_up){ 
		if (adapter->link_active == FALSE) {
			if (bootverbose)
				device_printf(dev,"Link is up %d Gbps %s \n",
				    ((adapter->link_speed == 128)? 10:1),
				    "Full Duplex");
			adapter->link_active = TRUE;
			if_link_state_change(ifp, LINK_STATE_UP);
		}
	} else { /* Link down */
		if (adapter->link_active == TRUE) {
			if (bootverbose)
				device_printf(dev,"Link is Down\n");
			if_link_state_change(ifp, LINK_STATE_DOWN);
			adapter->link_active = FALSE;
			for (int i = 0; i < adapter->num_queues;
			    i++, txr++)
				txr->watchdog_check = FALSE;
		}
	}

	return;
}


static void
ixv_ifstop(struct ifnet *ifp, int disable)
{
	struct adapter *adapter = ifp->if_softc;

	IXV_CORE_LOCK(adapter);
	ixv_stop(adapter);
	IXV_CORE_UNLOCK(adapter);
}

/*********************************************************************
 *
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC and deallocates TX/RX buffers.
 *
 **********************************************************************/

static void
ixv_stop(void *arg)
{
	struct ifnet   *ifp;
	struct adapter *adapter = arg;
	struct ixgbe_hw *hw = &adapter->hw;
	ifp = adapter->ifp;

	KASSERT(mutex_owned(&adapter->core_mtx));

	INIT_DEBUGOUT("ixv_stop: begin\n");
	ixv_disable_intr(adapter);

	/* Tell the stack that the interface is no longer active */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	ixgbe_reset_hw(hw);
	adapter->hw.adapter_stopped = FALSE;
	ixgbe_stop_adapter(hw);
	callout_stop(&adapter->timer);

	/* reprogram the RAR[0] in case user changed it. */
	ixgbe_set_rar(hw, 0, hw->mac.addr, 0, IXGBE_RAH_AV);

	return;
}


/*********************************************************************
 *
 *  Determine hardware revision.
 *
 **********************************************************************/
static void
ixv_identify_hardware(struct adapter *adapter)
{
	u16		pci_cmd_word;
	pcitag_t tag;
	pci_chipset_tag_t pc;
	pcireg_t subid, id;
	struct ixgbe_hw *hw = &adapter->hw;

	pc = adapter->osdep.pc;
	tag = adapter->osdep.tag;

	/*
	** Make sure BUSMASTER is set, on a VM under
	** KVM it may not be and will break things.
	*/
	pci_cmd_word = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	if (!(pci_cmd_word & PCI_COMMAND_MASTER_ENABLE)) {
		INIT_DEBUGOUT("Bus Master bit was not set!\n");
		pci_cmd_word |= PCI_COMMAND_MASTER_ENABLE;
		pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, pci_cmd_word);
	}

	id = pci_conf_read(pc, tag, PCI_ID_REG);
	subid = pci_conf_read(pc, tag, PCI_SUBSYS_ID_REG);

	/* Save off the information about this board */
	hw->vendor_id = PCI_VENDOR(id);
	hw->device_id = PCI_PRODUCT(id);
	hw->revision_id = PCI_REVISION(pci_conf_read(pc, tag, PCI_CLASS_REG));
	hw->subsystem_vendor_id = PCI_SUBSYS_VENDOR(subid);
	hw->subsystem_device_id = PCI_SUBSYS_ID(subid);

	return;
}

/*********************************************************************
 *
 *  Setup MSIX Interrupt resources and handlers 
 *
 **********************************************************************/
static int
ixv_allocate_msix(struct adapter *adapter, const struct pci_attach_args *pa)
{
#if !defined(NETBSD_MSI_OR_MSIX)
	return 0;
#else
	device_t        dev = adapter->dev;
	struct ix_queue *que = adapter->queues;
	int 		error, rid, vector = 0;
	pci_chipset_tag_t pc;
	pcitag_t	tag;
	char intrbuf[PCI_INTRSTR_LEN];
	const char	*intrstr = NULL;
	kcpuset_t	*affinity;
	int		cpu_id = 0;

	pc = adapter->osdep.pc;
	tag = adapter->osdep.tag;

	if (pci_msix_alloc_exact(pa,
		&adapter->osdep.intrs, IXG_MSIX_NINTR) != 0)
		return (ENXIO);

	kcpuset_create(&affinity, false);
	for (int i = 0; i < adapter->num_queues; i++, vector++, que++) {
		intrstr = pci_intr_string(pc, adapter->osdep.intrs[i], intrbuf,
		    sizeof(intrbuf));
#ifdef IXV_MPSAFE
		pci_intr_setattr(pc, adapter->osdep.intrs[i], PCI_INTR_MPSAFE,
		    true);
#endif
		/* Set the handler function */
		adapter->osdep.ihs[i] = pci_intr_establish(pc,
		    adapter->osdep.intrs[i], IPL_NET, ixv_msix_que, que);
		if (adapter->osdep.ihs[i] == NULL) {
			que->res = NULL;
			aprint_error_dev(dev,
			    "Failed to register QUE handler");
			kcpuset_destroy(affinity);
			return (ENXIO);
		}
		que->msix = vector;
        	adapter->que_mask |= (u64)(1 << que->msix);

		cpu_id = i;
		/* Round-robin affinity */
		kcpuset_zero(affinity);
		kcpuset_set(affinity, cpu_id % ncpu);
		error = interrupt_distribute(adapter->osdep.ihs[i], affinity,
		    NULL);
		aprint_normal_dev(dev, "for TX/RX, interrupting at %s",
		    intrstr);
		if (error == 0)
			aprint_normal(", bound queue %d to cpu %d\n",
			    i, cpu_id);
		else
			aprint_normal("\n");
		
		que->que_si = softint_establish(SOFTINT_NET, ixv_handle_que,
		    que);
		if (que->que_si == NULL) {
			aprint_error_dev(dev,
			    "could not establish software interrupt\n"); 
		}
	}

	/* and Mailbox */
	cpu_id++;
	intrstr = pci_intr_string(pc, adapter->osdep.intrs[vector], intrbuf,
	    sizeof(intrbuf));
#ifdef IXG_MPSAFE
	pci_intr_setattr(pc, &adapter->osdep.intrs[vector], PCI_INTR_MPSAFE, true);
#endif
	/* Set the mbx handler function */
	adapter->osdep.ihs[vector] = pci_intr_establish(pc,
	    adapter->osdep.intrs[vector], IPL_NET, ixv_msix_mbx, adapter);
	if (adapter->osdep.ihs[vector] == NULL) {
		adapter->res = NULL;
		aprint_error_dev(dev, "Failed to register LINK handler\n");
		kcpuset_destroy(affinity);
		return (ENXIO);
	}
	/* Round-robin affinity */
	kcpuset_zero(affinity);
	kcpuset_set(affinity, cpu_id % ncpu);
	error = interrupt_distribute(adapter->osdep.ihs[vector], affinity,NULL);

	aprint_normal_dev(dev,
	    "for link, interrupting at %s, ", intrstr);
	if (error == 0) {
		aprint_normal("affinity to cpu %d\n", cpu_id);
	}
	adapter->mbxvec = vector;
	/* Tasklets for Mailbox */
	adapter->mbx_si = softint_establish(SOFTINT_NET, ixv_handle_mbx,
	    adapter);
	/*
	** Due to a broken design QEMU will fail to properly
	** enable the guest for MSIX unless the vectors in
	** the table are all set up, so we must rewrite the
	** ENABLE in the MSIX control register again at this
	** point to cause it to successfully initialize us.
	*/
	if (adapter->hw.mac.type == ixgbe_mac_82599_vf) {
		int msix_ctrl;
		pci_get_capability(pc, tag, PCI_CAP_MSIX, &rid, NULL);
		rid += PCI_MSIX_CTL;
		msix_ctrl = pci_conf_read(pc, tag, rid);
		msix_ctrl |= PCI_MSIX_CTL_ENABLE;
		pci_conf_write(pc, tag, rid, msix_ctrl);
	}

	return (0);
#endif
}

/*
 * Setup MSIX resources, note that the VF
 * device MUST use MSIX, there is no fallback.
 */
static int
ixv_setup_msix(struct adapter *adapter)
{
#if !defined(NETBSD_MSI_OR_MSIX)
	return 0;
#else
	device_t dev = adapter->dev;
	int want, msgs;

	/*
	** Want two vectors: one for a queue,
	** plus an additional for mailbox.
	*/
	msgs = pci_msix_count(adapter->osdep.pc, adapter->osdep.tag);
	if (msgs < IXG_MSIX_NINTR) {
		aprint_error_dev(dev,"MSIX config error\n");
		return (ENXIO);
	}
	want = MIN(msgs, IXG_MSIX_NINTR);

	adapter->msix_mem = (void *)1; /* XXX */
	aprint_normal_dev(dev,
	    "Using MSIX interrupts with %d vectors\n", msgs);
	return (want);
#endif
}


static int
ixv_allocate_pci_resources(struct adapter *adapter,
    const struct pci_attach_args *pa)
{
	pcireg_t	memtype;
	device_t        dev = adapter->dev;
	bus_addr_t addr;
	int flags;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, PCI_BAR(0));

	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		adapter->osdep.mem_bus_space_tag = pa->pa_memt;
		if (pci_mapreg_info(pa->pa_pc, pa->pa_tag, PCI_BAR(0),
	              memtype, &addr, &adapter->osdep.mem_size, &flags) != 0)
			goto map_err;
		if ((flags & BUS_SPACE_MAP_PREFETCHABLE) != 0) {
			aprint_normal_dev(dev, "clearing prefetchable bit\n");
			flags &= ~BUS_SPACE_MAP_PREFETCHABLE;
		}
		if (bus_space_map(adapter->osdep.mem_bus_space_tag, addr,
		     adapter->osdep.mem_size, flags,
		     &adapter->osdep.mem_bus_space_handle) != 0) {
map_err:
			adapter->osdep.mem_size = 0;
			aprint_error_dev(dev, "unable to map BAR0\n");
			return ENXIO;
		}
		break;
	default:
		aprint_error_dev(dev, "unexpected type on BAR0\n");
		return ENXIO;
	}

	adapter->num_queues = 1;
	adapter->hw.back = &adapter->osdep;

	/*
	** Now setup MSI/X, should
	** return us the number of
	** configured vectors.
	*/
	adapter->msix = ixv_setup_msix(adapter);
	if (adapter->msix == ENXIO)
		return (ENXIO);
	else
		return (0);
}

static void
ixv_free_pci_resources(struct adapter * adapter)
{
#if !defined(NETBSD_MSI_OR_MSIX)
#else
	struct 		ix_queue *que = adapter->queues;
	int		rid;

	/*
	**  Release all msix queue resources:
	*/
	for (int i = 0; i < adapter->num_queues; i++, que++) {
		rid = que->msix + 1;
		if (que->res != NULL)
			pci_intr_disestablish(adapter->osdep.pc,
			    adapter->osdep.ihs[i]);
	}


	/* Clean the Legacy or Link interrupt last */
	if (adapter->mbxvec) /* we are doing MSIX */
		rid = adapter->mbxvec + 1;
	else
		(adapter->msix != 0) ? (rid = 1):(rid = 0);

	if (adapter->osdep.ihs[rid] != NULL)
		pci_intr_disestablish(adapter->osdep.pc,
		    adapter->osdep.ihs[rid]);
	adapter->osdep.ihs[rid] = NULL;

#if defined(NETBSD_MSI_OR_MSIX)
	pci_intr_release(adapter->osdep.pc, adapter->osdep.intrs,
	    adapter->osdep.nintrs);
#endif

	if (adapter->osdep.mem_size != 0) {
		bus_space_unmap(adapter->osdep.mem_bus_space_tag,
		    adapter->osdep.mem_bus_space_handle,
		    adapter->osdep.mem_size);
	}

#endif
	return;
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
static void
ixv_setup_interface(device_t dev, struct adapter *adapter)
{
	struct ethercom *ec = &adapter->osdep.ec;
	struct ifnet   *ifp;

	INIT_DEBUGOUT("ixv_setup_interface: begin");

	ifp = adapter->ifp = &ec->ec_if;
	strlcpy(ifp->if_xname, device_xname(dev), IFNAMSIZ);
	ifp->if_baudrate = 1000000000;
	ifp->if_init = ixv_init;
	ifp->if_stop = ixv_ifstop;
	ifp->if_softc = adapter;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ixv_ioctl;
#if __FreeBSD_version >= 800000
	ifp->if_transmit = ixv_mq_start;
	ifp->if_qflush = ixv_qflush;
#else
	ifp->if_start = ixv_start;
#endif
	ifp->if_snd.ifq_maxlen = adapter->num_tx_desc - 2;

	if_attach(ifp);
	ether_ifattach(ifp, adapter->hw.mac.addr);
	ether_set_ifflags_cb(ec, ixv_ifflags_cb);

	adapter->max_frame_size =
	    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_hdrlen = sizeof(struct ether_vlan_header);

	ifp->if_capabilities |= IFCAP_HWCSUM | IFCAP_TSOv4;
	ifp->if_capenable = 0;

	ec->ec_capabilities |= ETHERCAP_VLAN_HWCSUM;
	ec->ec_capabilities |= ETHERCAP_JUMBO_MTU;
	ec->ec_capabilities |= ETHERCAP_VLAN_HWTAGGING
	    		| ETHERCAP_VLAN_MTU;
	ec->ec_capenable = ec->ec_capabilities;

	/* Don't enable LRO by default */
	ifp->if_capabilities |= IFCAP_LRO;

	/*
	** Dont turn this on by default, if vlans are
	** created on another pseudo device (eg. lagg)
	** then vlan events are not passed thru, breaking
	** operation, but with HW FILTER off it works. If
	** using vlans directly on the em driver you can
	** enable this and get full hardware tag filtering.
	*/
	ec->ec_capabilities |= ETHERCAP_VLAN_HWFILTER;

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&adapter->media, IFM_IMASK, ixv_media_change,
		     ixv_media_status);
	ifmedia_add(&adapter->media, IFM_ETHER | IFM_FDX, 0, NULL);
	ifmedia_add(&adapter->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&adapter->media, IFM_ETHER | IFM_AUTO);

	return;
}
	
static void
ixv_config_link(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32	autoneg, err = 0;

	if (hw->mac.ops.check_link)
		err = hw->mac.ops.check_link(hw, &autoneg,
		    &adapter->link_up, FALSE);
	if (err)
		goto out;

	if (hw->mac.ops.setup_link)
               	err = hw->mac.ops.setup_link(hw,
		    autoneg, adapter->link_up);
out:
	return;
}

/********************************************************************
 * Manage DMA'able memory.
 *******************************************************************/

static int
ixv_dma_malloc(struct adapter *adapter, bus_size_t size,
		struct ixv_dma_alloc *dma, int mapflags)
{
	device_t dev = adapter->dev;
	int             r, rsegs;

	r = ixgbe_dma_tag_create(adapter->osdep.dmat,	/* parent */
			       DBA_ALIGN, 0,	/* alignment, bounds */
			       size,	/* maxsize */
			       1,	/* nsegments */
			       size,	/* maxsegsize */
			       BUS_DMA_ALLOCNOW,	/* flags */
			       &dma->dma_tag);
	if (r != 0) {
		aprint_error_dev(dev,
		    "ixv_dma_malloc: bus_dma_tag_create failed; error %u\n", r);
		goto fail_0;
	}
	r = bus_dmamem_alloc(dma->dma_tag->dt_dmat,
		size,
		dma->dma_tag->dt_alignment,
		dma->dma_tag->dt_boundary,
		&dma->dma_seg, 1, &rsegs, BUS_DMA_NOWAIT);
	if (r != 0) {
		aprint_error_dev(dev,
		    "%s: bus_dmamem_alloc failed; error %u\n", __func__, r);
		goto fail_1;
	}

	r = bus_dmamem_map(dma->dma_tag->dt_dmat, &dma->dma_seg, rsegs,
	    size, &dma->dma_vaddr, BUS_DMA_NOWAIT);
	if (r != 0) {
		aprint_error_dev(dev, "%s: bus_dmamem_map failed; error %d\n",
		    __func__, r);
		goto fail_2;
	}

	r = ixgbe_dmamap_create(dma->dma_tag, 0, &dma->dma_map);
	if (r != 0) {
		aprint_error_dev(dev, "%s: bus_dmamem_map failed; error %d\n",
		    __func__, r);
		goto fail_3;
	}

	r = bus_dmamap_load(dma->dma_tag->dt_dmat, dma->dma_map, dma->dma_vaddr,
			    size,
			    NULL,
			    mapflags | BUS_DMA_NOWAIT);
	if (r != 0) {
		aprint_error_dev(dev,"%s: bus_dmamap_load failed; error %u\n",
		    __func__, r);
		goto fail_4;
	}
	dma->dma_paddr = dma->dma_map->dm_segs[0].ds_addr;
	dma->dma_size = size;
	return 0;
fail_4:
	ixgbe_dmamap_destroy(dma->dma_tag, dma->dma_map);
fail_3:
	bus_dmamem_unmap(dma->dma_tag->dt_dmat, dma->dma_vaddr, size);
fail_2:
	bus_dmamem_free(dma->dma_tag->dt_dmat, &dma->dma_seg, rsegs);
fail_1:
	ixgbe_dma_tag_destroy(dma->dma_tag);
fail_0:
	dma->dma_tag = NULL;
	return (r);
}

static void
ixv_dma_free(struct adapter *adapter, struct ixv_dma_alloc *dma)
{
	bus_dmamap_sync(dma->dma_tag->dt_dmat, dma->dma_map, 0, dma->dma_size,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	ixgbe_dmamap_unload(dma->dma_tag, dma->dma_map);
	bus_dmamem_free(dma->dma_tag->dt_dmat, &dma->dma_seg, 1);
	ixgbe_dma_tag_destroy(dma->dma_tag);
}


/*********************************************************************
 *
 *  Allocate memory for the transmit and receive rings, and then
 *  the descriptors associated with each, called only once at attach.
 *
 **********************************************************************/
static int
ixv_allocate_queues(struct adapter *adapter)
{
	device_t	dev = adapter->dev;
	struct ix_queue	*que;
	struct tx_ring	*txr;
	struct rx_ring	*rxr;
	int rsize, tsize, error = 0;
	int txconf = 0, rxconf = 0;

        /* First allocate the top level queue structs */
        if (!(adapter->queues =
            (struct ix_queue *) malloc(sizeof(struct ix_queue) *
            adapter->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
                aprint_error_dev(dev, "Unable to allocate queue memory\n");
                error = ENOMEM;
                goto fail;
        }

	/* First allocate the TX ring struct memory */
	if (!(adapter->tx_rings =
	    (struct tx_ring *) malloc(sizeof(struct tx_ring) *
	    adapter->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		aprint_error_dev(dev, "Unable to allocate TX ring memory\n");
		error = ENOMEM;
		goto tx_fail;
	}

	/* Next allocate the RX */
	if (!(adapter->rx_rings =
	    (struct rx_ring *) malloc(sizeof(struct rx_ring) *
	    adapter->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		aprint_error_dev(dev, "Unable to allocate RX ring memory\n");
		error = ENOMEM;
		goto rx_fail;
	}

	/* For the ring itself */
	tsize = roundup2(adapter->num_tx_desc *
	    sizeof(union ixgbe_adv_tx_desc), DBA_ALIGN);

	/*
	 * Now set up the TX queues, txconf is needed to handle the
	 * possibility that things fail midcourse and we need to
	 * undo memory gracefully
	 */ 
	for (int i = 0; i < adapter->num_queues; i++, txconf++) {
		/* Set up some basics */
		txr = &adapter->tx_rings[i];
		txr->adapter = adapter;
		txr->me = i;

		/* Initialize the TX side lock */
		snprintf(txr->mtx_name, sizeof(txr->mtx_name), "%s:tx(%d)",
		    device_xname(dev), txr->me);
		mutex_init(&txr->tx_mtx, MUTEX_DEFAULT, IPL_NET);

		if (ixv_dma_malloc(adapter, tsize,
			&txr->txdma, BUS_DMA_NOWAIT)) {
			aprint_error_dev(dev,
			    "Unable to allocate TX Descriptor memory\n");
			error = ENOMEM;
			goto err_tx_desc;
		}
		txr->tx_base = (union ixgbe_adv_tx_desc *)txr->txdma.dma_vaddr;
		bzero((void *)txr->tx_base, tsize);

        	/* Now allocate transmit buffers for the ring */
        	if (ixv_allocate_transmit_buffers(txr)) {
			aprint_error_dev(dev,
			    "Critical Failure setting up transmit buffers\n");
			error = ENOMEM;
			goto err_tx_desc;
        	}
#if __FreeBSD_version >= 800000
		/* Allocate a buf ring */
		txr->br = buf_ring_alloc(IXV_BR_SIZE, M_DEVBUF,
		    M_WAITOK, &txr->tx_mtx);
		if (txr->br == NULL) {
			aprint_error_dev(dev,
			    "Critical Failure setting up buf ring\n");
			error = ENOMEM;
			goto err_tx_desc;
		}
#endif
	}

	/*
	 * Next the RX queues...
	 */ 
	rsize = roundup2(adapter->num_rx_desc *
	    sizeof(union ixgbe_adv_rx_desc), DBA_ALIGN);
	for (int i = 0; i < adapter->num_queues; i++, rxconf++) {
		rxr = &adapter->rx_rings[i];
		/* Set up some basics */
		rxr->adapter = adapter;
		rxr->me = i;

		/* Initialize the RX side lock */
		snprintf(rxr->mtx_name, sizeof(rxr->mtx_name), "%s:rx(%d)",
		    device_xname(dev), rxr->me);
		mutex_init(&rxr->rx_mtx, MUTEX_DEFAULT, IPL_NET);

		if (ixv_dma_malloc(adapter, rsize,
			&rxr->rxdma, BUS_DMA_NOWAIT)) {
			aprint_error_dev(dev,
			    "Unable to allocate RxDescriptor memory\n");
			error = ENOMEM;
			goto err_rx_desc;
		}
		rxr->rx_base = (union ixgbe_adv_rx_desc *)rxr->rxdma.dma_vaddr;
		bzero((void *)rxr->rx_base, rsize);

        	/* Allocate receive buffers for the ring*/
		if (ixv_allocate_receive_buffers(rxr)) {
			aprint_error_dev(dev,
			    "Critical Failure setting up receive buffers\n");
			error = ENOMEM;
			goto err_rx_desc;
		}
	}

	/*
	** Finally set up the queue holding structs
	*/
	for (int i = 0; i < adapter->num_queues; i++) {
		que = &adapter->queues[i];
		que->adapter = adapter;
		que->txr = &adapter->tx_rings[i];
		que->rxr = &adapter->rx_rings[i];
	}

	return (0);

err_rx_desc:
	for (rxr = adapter->rx_rings; rxconf > 0; rxr++, rxconf--)
		ixv_dma_free(adapter, &rxr->rxdma);
err_tx_desc:
	for (txr = adapter->tx_rings; txconf > 0; txr++, txconf--)
		ixv_dma_free(adapter, &txr->txdma);
	free(adapter->rx_rings, M_DEVBUF);
rx_fail:
	free(adapter->tx_rings, M_DEVBUF);
tx_fail:
	free(adapter->queues, M_DEVBUF);
fail:
	return (error);
}


/*********************************************************************
 *
 *  Allocate memory for tx_buffer structures. The tx_buffer stores all
 *  the information needed to transmit a packet on the wire. This is
 *  called only once at attach, setup is done every reset.
 *
 **********************************************************************/
static int
ixv_allocate_transmit_buffers(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	device_t dev = adapter->dev;
	struct ixv_tx_buf *txbuf;
	int error, i;

	/*
	 * Setup DMA descriptor areas.
	 */
	if ((error = ixgbe_dma_tag_create(adapter->osdep.dmat,	/* parent */
			       1, 0,		/* alignment, bounds */
			       IXV_TSO_SIZE,		/* maxsize */
			       32,			/* nsegments */
			       PAGE_SIZE,		/* maxsegsize */
			       0,			/* flags */
			       &txr->txtag))) {
		aprint_error_dev(dev,"Unable to allocate TX DMA tag\n");
		goto fail;
	}

	if (!(txr->tx_buffers =
	    (struct ixv_tx_buf *) malloc(sizeof(struct ixv_tx_buf) *
	    adapter->num_tx_desc, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		aprint_error_dev(dev, "Unable to allocate tx_buffer memory\n");
		error = ENOMEM;
		goto fail;
	}

        /* Create the descriptor buffer dma maps */
	txbuf = txr->tx_buffers;
	for (i = 0; i < adapter->num_tx_desc; i++, txbuf++) {
		error = ixgbe_dmamap_create(txr->txtag, 0, &txbuf->map);
		if (error != 0) {
			aprint_error_dev(dev, "Unable to create TX DMA map\n");
			goto fail;
		}
	}

	return 0;
fail:
	/* We free all, it handles case where we are in the middle */
	ixv_free_transmit_structures(adapter);
	return (error);
}

/*********************************************************************
 *
 *  Initialize a transmit ring.
 *
 **********************************************************************/
static void
ixv_setup_transmit_ring(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	struct ixv_tx_buf *txbuf;
	int i;

	/* Clear the old ring contents */
	IXV_TX_LOCK(txr);
	bzero((void *)txr->tx_base,
	      (sizeof(union ixgbe_adv_tx_desc)) * adapter->num_tx_desc);
	/* Reset indices */
	txr->next_avail_desc = 0;
	txr->next_to_clean = 0;

	/* Free any existing tx buffers. */
        txbuf = txr->tx_buffers;
	for (i = 0; i < adapter->num_tx_desc; i++, txbuf++) {
		if (txbuf->m_head != NULL) {
			bus_dmamap_sync(txr->txtag->dt_dmat, txbuf->map,
			    0, txbuf->m_head->m_pkthdr.len,
			    BUS_DMASYNC_POSTWRITE);
			ixgbe_dmamap_unload(txr->txtag, txbuf->map);
			m_freem(txbuf->m_head);
			txbuf->m_head = NULL;
		}
		/* Clear the EOP index */
		txbuf->eop_index = -1;
        }

	/* Set number of descriptors available */
	txr->tx_avail = adapter->num_tx_desc;

	ixgbe_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	IXV_TX_UNLOCK(txr);
}

/*********************************************************************
 *
 *  Initialize all transmit rings.
 *
 **********************************************************************/
static int
ixv_setup_transmit_structures(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;

	for (int i = 0; i < adapter->num_queues; i++, txr++)
		ixv_setup_transmit_ring(txr);

	return (0);
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
ixv_initialize_transmit_units(struct adapter *adapter)
{
	struct tx_ring	*txr = adapter->tx_rings;
	struct ixgbe_hw	*hw = &adapter->hw;


	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		u64	tdba = txr->txdma.dma_paddr;
		u32	txctrl, txdctl;

		/* Set WTHRESH to 8, burst writeback */
		txdctl = IXGBE_READ_REG(hw, IXGBE_VFTXDCTL(i));
		txdctl |= (8 << 16);
		IXGBE_WRITE_REG(hw, IXGBE_VFTXDCTL(i), txdctl);
		/* Now enable */
		txdctl = IXGBE_READ_REG(hw, IXGBE_VFTXDCTL(i));
		txdctl |= IXGBE_TXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_VFTXDCTL(i), txdctl);

		/* Set the HW Tx Head and Tail indices */
	    	IXGBE_WRITE_REG(&adapter->hw, IXGBE_VFTDH(i), 0);
	    	IXGBE_WRITE_REG(&adapter->hw, IXGBE_VFTDT(i), 0);

		/* Setup Transmit Descriptor Cmd Settings */
		txr->txd_cmd = IXGBE_TXD_CMD_IFCS;
		txr->watchdog_check = FALSE;

		/* Set Ring parameters */
		IXGBE_WRITE_REG(hw, IXGBE_VFTDBAL(i),
		       (tdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_VFTDBAH(i), (tdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_VFTDLEN(i),
		    adapter->num_tx_desc *
		    sizeof(struct ixgbe_legacy_tx_desc));
		txctrl = IXGBE_READ_REG(hw, IXGBE_VFDCA_TXCTRL(i));
		txctrl &= ~IXGBE_DCA_TXCTRL_DESC_WRO_EN;
		IXGBE_WRITE_REG(hw, IXGBE_VFDCA_TXCTRL(i), txctrl);
		break;
	}

	return;
}

/*********************************************************************
 *
 *  Free all transmit rings.
 *
 **********************************************************************/
static void
ixv_free_transmit_structures(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		ixv_free_transmit_buffers(txr);
		ixv_dma_free(adapter, &txr->txdma);
		IXV_TX_LOCK_DESTROY(txr);
	}
	free(adapter->tx_rings, M_DEVBUF);
}

/*********************************************************************
 *
 *  Free transmit ring related data structures.
 *
 **********************************************************************/
static void
ixv_free_transmit_buffers(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	struct ixv_tx_buf *tx_buffer;
	int             i;

	INIT_DEBUGOUT("free_transmit_ring: begin");

	if (txr->tx_buffers == NULL)
		return;

	tx_buffer = txr->tx_buffers;
	for (i = 0; i < adapter->num_tx_desc; i++, tx_buffer++) {
		if (tx_buffer->m_head != NULL) {
			bus_dmamap_sync(txr->txtag->dt_dmat, tx_buffer->map,
			    0, tx_buffer->m_head->m_pkthdr.len,
			    BUS_DMASYNC_POSTWRITE);
			ixgbe_dmamap_unload(txr->txtag, tx_buffer->map);
			m_freem(tx_buffer->m_head);
			tx_buffer->m_head = NULL;
			if (tx_buffer->map != NULL) {
				ixgbe_dmamap_destroy(txr->txtag,
				    tx_buffer->map);
				tx_buffer->map = NULL;
			}
		} else if (tx_buffer->map != NULL) {
			ixgbe_dmamap_unload(txr->txtag, tx_buffer->map);
			ixgbe_dmamap_destroy(txr->txtag, tx_buffer->map);
			tx_buffer->map = NULL;
		}
	}
#if __FreeBSD_version >= 800000
	if (txr->br != NULL)
		buf_ring_free(txr->br, M_DEVBUF);
#endif
	if (txr->tx_buffers != NULL) {
		free(txr->tx_buffers, M_DEVBUF);
		txr->tx_buffers = NULL;
	}
	if (txr->txtag != NULL) {
		ixgbe_dma_tag_destroy(txr->txtag);
		txr->txtag = NULL;
	}
	return;
}

/*********************************************************************
 *
 *  Advanced Context Descriptor setup for VLAN or CSUM
 *
 **********************************************************************/

static u32
ixv_tx_ctx_setup(struct tx_ring *txr, struct mbuf *mp)
{
	struct m_tag *mtag;
	struct adapter *adapter = txr->adapter;
	struct ethercom *ec = &adapter->osdep.ec;
	struct ixgbe_adv_tx_context_desc *TXD;
	struct ixv_tx_buf        *tx_buffer;
	u32 olinfo = 0, vlan_macip_lens = 0, type_tucmd_mlhl = 0;
	struct ether_vlan_header *eh;
	struct ip ip;
	struct ip6_hdr ip6;
	int  ehdrlen, ip_hlen = 0;
	u16	etype;
	u8	ipproto __diagused = 0;
	bool	offload;
	int ctxd = txr->next_avail_desc;
	u16 vtag = 0;


	offload = ((mp->m_pkthdr.csum_flags & M_CSUM_OFFLOAD) != 0);

	tx_buffer = &txr->tx_buffers[ctxd];
	TXD = (struct ixgbe_adv_tx_context_desc *) &txr->tx_base[ctxd];

	/*
	** In advanced descriptors the vlan tag must 
	** be placed into the descriptor itself.
	*/
	if ((mtag = VLAN_OUTPUT_TAG(ec, mp)) != NULL) {
		vtag = htole16(VLAN_TAG_VALUE(mtag) & 0xffff);
		vlan_macip_lens |= (vtag << IXGBE_ADVTXD_VLAN_SHIFT);
	} else if (!offload)
		return 0;

	/*
	 * Determine where frame payload starts.
	 * Jump over vlan headers if already present,
	 * helpful for QinQ too.
	 */
	KASSERT(mp->m_len >= offsetof(struct ether_vlan_header, evl_tag));
	eh = mtod(mp, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		KASSERT(mp->m_len >= sizeof(struct ether_vlan_header));
		etype = ntohs(eh->evl_proto);
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		etype = ntohs(eh->evl_encap_proto);
		ehdrlen = ETHER_HDR_LEN;
	}

	/* Set the ether header length */
	vlan_macip_lens |= ehdrlen << IXGBE_ADVTXD_MACLEN_SHIFT;

	switch (etype) {
	case ETHERTYPE_IP:
		m_copydata(mp, ehdrlen, sizeof(ip), &ip);
		ip_hlen = ip.ip_hl << 2;
		ipproto = ip.ip_p;
#if 0
		ip.ip_sum = 0;
		m_copyback(mp, ehdrlen, sizeof(ip), &ip);
#else
		KASSERT((mp->m_pkthdr.csum_flags & M_CSUM_IPv4) == 0 ||
		    ip.ip_sum == 0);
#endif
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV4;
		break;
	case ETHERTYPE_IPV6:
		m_copydata(mp, ehdrlen, sizeof(ip6), &ip6);
		ip_hlen = sizeof(ip6);
		ipproto = ip6.ip6_nxt;
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV6;
		break;
	default:
		break;
	}

	if ((mp->m_pkthdr.csum_flags & M_CSUM_IPv4) != 0)
		olinfo |= IXGBE_TXD_POPTS_IXSM << 8;

	vlan_macip_lens |= ip_hlen;
	type_tucmd_mlhl |= IXGBE_ADVTXD_DCMD_DEXT | IXGBE_ADVTXD_DTYP_CTXT;

	if (mp->m_pkthdr.csum_flags & (M_CSUM_TCPv4|M_CSUM_TCPv6)) {
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_TCP;
		olinfo |= IXGBE_TXD_POPTS_TXSM << 8;
		KASSERT(ipproto == IPPROTO_TCP);
	} else if (mp->m_pkthdr.csum_flags & (M_CSUM_UDPv4|M_CSUM_UDPv6)) {
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_UDP;
		olinfo |= IXGBE_TXD_POPTS_TXSM << 8;
		KASSERT(ipproto == IPPROTO_UDP);
	}

	/* Now copy bits into descriptor */
	TXD->vlan_macip_lens |= htole32(vlan_macip_lens);
	TXD->type_tucmd_mlhl |= htole32(type_tucmd_mlhl);
	TXD->seqnum_seed = htole32(0);
	TXD->mss_l4len_idx = htole32(0);

	tx_buffer->m_head = NULL;
	tx_buffer->eop_index = -1;

	/* We've consumed the first desc, adjust counters */
	if (++ctxd == adapter->num_tx_desc)
		ctxd = 0;
	txr->next_avail_desc = ctxd;
	--txr->tx_avail;

        return olinfo;
}

/**********************************************************************
 *
 *  Setup work for hardware segmentation offload (TSO) on
 *  adapters using advanced tx descriptors
 *
 **********************************************************************/
static bool
ixv_tso_setup(struct tx_ring *txr, struct mbuf *mp, u32 *paylen)
{
	struct m_tag *mtag;
	struct adapter *adapter = txr->adapter;
	struct ethercom *ec = &adapter->osdep.ec;
	struct ixgbe_adv_tx_context_desc *TXD;
	struct ixv_tx_buf        *tx_buffer;
	u32 vlan_macip_lens = 0, type_tucmd_mlhl = 0;
	u32 mss_l4len_idx = 0;
	u16 vtag = 0;
	int ctxd, ehdrlen,  hdrlen, ip_hlen, tcp_hlen;
	struct ether_vlan_header *eh;
	struct ip *ip;
	struct tcphdr *th;


	/*
	 * Determine where frame payload starts.
	 * Jump over vlan headers if already present
	 */
	eh = mtod(mp, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) 
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	else
		ehdrlen = ETHER_HDR_LEN;

        /* Ensure we have at least the IP+TCP header in the first mbuf. */
        if (mp->m_len < ehdrlen + sizeof(struct ip) + sizeof(struct tcphdr))
		return FALSE;

	ctxd = txr->next_avail_desc;
	tx_buffer = &txr->tx_buffers[ctxd];
	TXD = (struct ixgbe_adv_tx_context_desc *) &txr->tx_base[ctxd];

	ip = (struct ip *)(mp->m_data + ehdrlen);
	if (ip->ip_p != IPPROTO_TCP)
		return FALSE;   /* 0 */
	ip->ip_sum = 0;
	ip_hlen = ip->ip_hl << 2;
	th = (struct tcphdr *)((char *)ip + ip_hlen);
	/* XXX Educated guess: FreeBSD's in_pseudo == NetBSD's in_cksum_phdr */
	th->th_sum = in_cksum_phdr(ip->ip_src.s_addr,
	    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
	tcp_hlen = th->th_off << 2;
	hdrlen = ehdrlen + ip_hlen + tcp_hlen;

	/* This is used in the transmit desc in encap */
	*paylen = mp->m_pkthdr.len - hdrlen;

	/* VLAN MACLEN IPLEN */
	if ((mtag = VLAN_OUTPUT_TAG(ec, mp)) != NULL) {
		vtag = htole16(VLAN_TAG_VALUE(mtag) & 0xffff);
                vlan_macip_lens |= (vtag << IXGBE_ADVTXD_VLAN_SHIFT);
	}

	vlan_macip_lens |= ehdrlen << IXGBE_ADVTXD_MACLEN_SHIFT;
	vlan_macip_lens |= ip_hlen;
	TXD->vlan_macip_lens |= htole32(vlan_macip_lens);

	/* ADV DTYPE TUCMD */
	type_tucmd_mlhl |= IXGBE_ADVTXD_DCMD_DEXT | IXGBE_ADVTXD_DTYP_CTXT;
	type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_TCP;
	type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV4;
	TXD->type_tucmd_mlhl |= htole32(type_tucmd_mlhl);


	/* MSS L4LEN IDX */
	mss_l4len_idx |= (mp->m_pkthdr.segsz << IXGBE_ADVTXD_MSS_SHIFT);
	mss_l4len_idx |= (tcp_hlen << IXGBE_ADVTXD_L4LEN_SHIFT);
	TXD->mss_l4len_idx = htole32(mss_l4len_idx);

	TXD->seqnum_seed = htole32(0);
	tx_buffer->m_head = NULL;
	tx_buffer->eop_index = -1;

	if (++ctxd == adapter->num_tx_desc)
		ctxd = 0;

	txr->tx_avail--;
	txr->next_avail_desc = ctxd;
	return TRUE;
}


/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue.
 *
 **********************************************************************/
static bool
ixv_txeof(struct tx_ring *txr)
{
	struct adapter	*adapter = txr->adapter;
	struct ifnet	*ifp = adapter->ifp;
	u32	first, last, done;
	struct ixv_tx_buf *tx_buffer;
	struct ixgbe_legacy_tx_desc *tx_desc, *eop_desc;

	KASSERT(mutex_owned(&txr->tx_mtx));

	if (txr->tx_avail == adapter->num_tx_desc)
		return false;

	first = txr->next_to_clean;
	tx_buffer = &txr->tx_buffers[first];
	/* For cleanup we just use legacy struct */
	tx_desc = (struct ixgbe_legacy_tx_desc *)&txr->tx_base[first];
	last = tx_buffer->eop_index;
	if (last == -1)
		return false;
	eop_desc = (struct ixgbe_legacy_tx_desc *)&txr->tx_base[last];

	/*
	** Get the index of the first descriptor
	** BEYOND the EOP and call that 'done'.
	** I do this so the comparison in the
	** inner while loop below can be simple
	*/
	if (++last == adapter->num_tx_desc) last = 0;
	done = last;

        ixgbe_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
            BUS_DMASYNC_POSTREAD);
	/*
	** Only the EOP descriptor of a packet now has the DD
	** bit set, this is what we look for...
	*/
	while (eop_desc->upper.fields.status & IXGBE_TXD_STAT_DD) {
		/* We clean the range of the packet */
		while (first != done) {
			tx_desc->upper.data = 0;
			tx_desc->lower.data = 0;
			tx_desc->buffer_addr = 0;
			++txr->tx_avail;

			if (tx_buffer->m_head) {
				bus_dmamap_sync(txr->txtag->dt_dmat,
				    tx_buffer->map,
				    0, tx_buffer->m_head->m_pkthdr.len,
				    BUS_DMASYNC_POSTWRITE);
				ixgbe_dmamap_unload(txr->txtag, tx_buffer->map);
				m_freem(tx_buffer->m_head);
				tx_buffer->m_head = NULL;
				tx_buffer->map = NULL;
			}
			tx_buffer->eop_index = -1;
			getmicrotime(&txr->watchdog_time);

			if (++first == adapter->num_tx_desc)
				first = 0;

			tx_buffer = &txr->tx_buffers[first];
			tx_desc =
			    (struct ixgbe_legacy_tx_desc *)&txr->tx_base[first];
		}
		++ifp->if_opackets;
		/* See if there is more work now */
		last = tx_buffer->eop_index;
		if (last != -1) {
			eop_desc =
			    (struct ixgbe_legacy_tx_desc *)&txr->tx_base[last];
			/* Get next done point */
			if (++last == adapter->num_tx_desc) last = 0;
			done = last;
		} else
			break;
	}
	ixgbe_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	txr->next_to_clean = first;

	/*
	 * If we have enough room, clear IFF_OACTIVE to tell the stack that
	 * it is OK to send packets. If there are no pending descriptors,
	 * clear the timeout. Otherwise, if some descriptors have been freed,
	 * restart the timeout.
	 */
	if (txr->tx_avail > IXV_TX_CLEANUP_THRESHOLD) {
		ifp->if_flags &= ~IFF_OACTIVE;
		if (txr->tx_avail == adapter->num_tx_desc) {
			txr->watchdog_check = FALSE;
			return false;
		}
	}

	return true;
}

/*********************************************************************
 *
 *  Refresh mbuf buffers for RX descriptor rings
 *   - now keeps its own state so discards due to resource
 *     exhaustion are unnecessary, if an mbuf cannot be obtained
 *     it just returns, keeping its placeholder, thus it can simply
 *     be recalled to try again.
 *
 **********************************************************************/
static void
ixv_refresh_mbufs(struct rx_ring *rxr, int limit)
{
	struct adapter		*adapter = rxr->adapter;
	struct ixv_rx_buf	*rxbuf;
	struct mbuf		*mh, *mp;
	int			i, j, error;
	bool			refreshed = false;

	i = j = rxr->next_to_refresh;
        /* Get the control variable, one beyond refresh point */
	if (++j == adapter->num_rx_desc)
		j = 0;
	while (j != limit) {
		rxbuf = &rxr->rx_buffers[i];
		if ((rxbuf->m_head == NULL) && (rxr->hdr_split)) {
			mh = m_gethdr(M_NOWAIT, MT_DATA);
			if (mh == NULL)
				goto update;
			mh->m_pkthdr.len = mh->m_len = MHLEN;
			mh->m_flags |= M_PKTHDR;
			m_adj(mh, ETHER_ALIGN);
			/* Get the memory mapping */
			error = bus_dmamap_load_mbuf(rxr->htag->dt_dmat,
			    rxbuf->hmap, mh, BUS_DMA_NOWAIT);
			if (error != 0) {
				printf("GET BUF: dmamap load"
				    " failure - %d\n", error);
				m_free(mh);
				goto update;
			}
			rxbuf->m_head = mh;
			ixgbe_dmamap_sync(rxr->htag, rxbuf->hmap,
			    BUS_DMASYNC_PREREAD);
			rxr->rx_base[i].read.hdr_addr =
			    htole64(rxbuf->hmap->dm_segs[0].ds_addr);
		}

		if (rxbuf->m_pack == NULL) {
			mp = ixgbe_getjcl(&adapter->jcl_head, M_NOWAIT,
			    MT_DATA, M_PKTHDR, adapter->rx_mbuf_sz);
			if (mp == NULL) {
				rxr->no_jmbuf.ev_count++;
				goto update;
			} else
				mp = rxbuf->m_pack;

			mp->m_pkthdr.len = mp->m_len = adapter->rx_mbuf_sz;
			/* Get the memory mapping */
			error = bus_dmamap_load_mbuf(rxr->ptag->dt_dmat,
			    rxbuf->pmap, mp, BUS_DMA_NOWAIT);
			if (error != 0) {
				printf("GET BUF: dmamap load"
				    " failure - %d\n", error);
				m_free(mp);
				rxbuf->m_pack = NULL;
				goto update;
			}
			rxbuf->m_pack = mp;
			bus_dmamap_sync(rxr->ptag->dt_dmat, rxbuf->pmap,
			    0, mp->m_pkthdr.len, BUS_DMASYNC_PREREAD);
			rxr->rx_base[i].read.pkt_addr =
			    htole64(rxbuf->pmap->dm_segs[0].ds_addr);
		}

		refreshed = true;
		rxr->next_to_refresh = i = j;
		/* Calculate next index */
		if (++j == adapter->num_rx_desc)
			j = 0;
	}
update:
	if (refreshed) /* update tail index */
		IXGBE_WRITE_REG(&adapter->hw,
		    IXGBE_VFRDT(rxr->me), rxr->next_to_refresh);
	return;
}

/*********************************************************************
 *
 *  Allocate memory for rx_buffer structures. Since we use one
 *  rx_buffer per received packet, the maximum number of rx_buffer's
 *  that we'll need is equal to the number of receive descriptors
 *  that we've allocated.
 *
 **********************************************************************/
static int
ixv_allocate_receive_buffers(struct rx_ring *rxr)
{
	struct	adapter 	*adapter = rxr->adapter;
	device_t 		dev = adapter->dev;
	struct ixv_rx_buf 	*rxbuf;
	int             	i, bsize, error;

	bsize = sizeof(struct ixv_rx_buf) * adapter->num_rx_desc;
	if (!(rxr->rx_buffers =
	    (struct ixv_rx_buf *) malloc(bsize,
	    M_DEVBUF, M_NOWAIT | M_ZERO))) {
		aprint_error_dev(dev, "Unable to allocate rx_buffer memory\n");
		error = ENOMEM;
		goto fail;
	}

	if ((error = ixgbe_dma_tag_create(adapter->osdep.dmat,	/* parent */
				   1, 0,	/* alignment, bounds */
				   MSIZE,		/* maxsize */
				   1,			/* nsegments */
				   MSIZE,		/* maxsegsize */
				   0,			/* flags */
				   &rxr->htag))) {
		aprint_error_dev(dev, "Unable to create RX DMA tag\n");
		goto fail;
	}

	if ((error = ixgbe_dma_tag_create(adapter->osdep.dmat,	/* parent */
				   1, 0,	/* alignment, bounds */
				   MJUMPAGESIZE,	/* maxsize */
				   1,			/* nsegments */
				   MJUMPAGESIZE,	/* maxsegsize */
				   0,			/* flags */
				   &rxr->ptag))) {
		aprint_error_dev(dev, "Unable to create RX DMA tag\n");
		goto fail;
	}

	for (i = 0; i < adapter->num_rx_desc; i++, rxbuf++) {
		rxbuf = &rxr->rx_buffers[i];
		error = ixgbe_dmamap_create(rxr->htag,
		    BUS_DMA_NOWAIT, &rxbuf->hmap);
		if (error) {
			aprint_error_dev(dev, "Unable to create RX head map\n");
			goto fail;
		}
		error = ixgbe_dmamap_create(rxr->ptag,
		    BUS_DMA_NOWAIT, &rxbuf->pmap);
		if (error) {
			aprint_error_dev(dev, "Unable to create RX pkt map\n");
			goto fail;
		}
	}

	return (0);

fail:
	/* Frees all, but can handle partial completion */
	ixv_free_receive_structures(adapter);
	return (error);
}

static void     
ixv_free_receive_ring(struct rx_ring *rxr)
{ 
	struct  adapter         *adapter;
	struct ixv_rx_buf       *rxbuf;
	int i;

	adapter = rxr->adapter;
	for (i = 0; i < adapter->num_rx_desc; i++) {
		rxbuf = &rxr->rx_buffers[i];
		if (rxbuf->m_head != NULL) {
			ixgbe_dmamap_sync(rxr->htag, rxbuf->hmap,
			    BUS_DMASYNC_POSTREAD);
			ixgbe_dmamap_unload(rxr->htag, rxbuf->hmap);
			rxbuf->m_head->m_flags |= M_PKTHDR;
			m_freem(rxbuf->m_head);
		}
		if (rxbuf->m_pack != NULL) {
			/* XXX not ixgbe_ ? */
			bus_dmamap_sync(rxr->ptag->dt_dmat, rxbuf->pmap,
			    0, rxbuf->m_pack->m_pkthdr.len,
			    BUS_DMASYNC_POSTREAD);
			ixgbe_dmamap_unload(rxr->ptag, rxbuf->pmap);
			rxbuf->m_pack->m_flags |= M_PKTHDR;
			m_freem(rxbuf->m_pack);
		}
		rxbuf->m_head = NULL;
		rxbuf->m_pack = NULL;
	}
}


/*********************************************************************
 *
 *  Initialize a receive ring and its buffers.
 *
 **********************************************************************/
static int
ixv_setup_receive_ring(struct rx_ring *rxr)
{
	struct	adapter 	*adapter;
	struct ixv_rx_buf	*rxbuf;
#ifdef LRO
	struct ifnet		*ifp;
	struct lro_ctrl		*lro = &rxr->lro;
#endif /* LRO */
	int			rsize, error = 0;

	adapter = rxr->adapter;
#ifdef LRO
	ifp = adapter->ifp;
#endif /* LRO */

	/* Clear the ring contents */
	IXV_RX_LOCK(rxr);
	rsize = roundup2(adapter->num_rx_desc *
	    sizeof(union ixgbe_adv_rx_desc), DBA_ALIGN);
	bzero((void *)rxr->rx_base, rsize);

	/* Free current RX buffer structs and their mbufs */
	ixv_free_receive_ring(rxr);

	IXV_RX_UNLOCK(rxr);

	/* Now reinitialize our supply of jumbo mbufs.  The number
	 * or size of jumbo mbufs may have changed.
	 */
	ixgbe_jcl_reinit(&adapter->jcl_head, rxr->ptag->dt_dmat,
	    2 * adapter->num_rx_desc, adapter->rx_mbuf_sz);

	IXV_RX_LOCK(rxr);

	/* Configure header split? */
	if (ixv_header_split)
		rxr->hdr_split = TRUE;

	/* Now replenish the mbufs */
	for (int j = 0; j != adapter->num_rx_desc; ++j) {
		struct mbuf	*mh, *mp;

		rxbuf = &rxr->rx_buffers[j];
		/*
		** Dont allocate mbufs if not
		** doing header split, its wasteful
		*/ 
		if (rxr->hdr_split == FALSE)
			goto skip_head;

		/* First the header */
		rxbuf->m_head = m_gethdr(M_DONTWAIT, MT_DATA);
		if (rxbuf->m_head == NULL) {
			error = ENOBUFS;
			goto fail;
		}
		m_adj(rxbuf->m_head, ETHER_ALIGN);
		mh = rxbuf->m_head;
		mh->m_len = mh->m_pkthdr.len = MHLEN;
		mh->m_flags |= M_PKTHDR;
		/* Get the memory mapping */
		error = bus_dmamap_load_mbuf(rxr->htag->dt_dmat,
		    rxbuf->hmap, rxbuf->m_head, BUS_DMA_NOWAIT);
		if (error != 0) /* Nothing elegant to do here */
			goto fail;
		bus_dmamap_sync(rxr->htag->dt_dmat, rxbuf->hmap,
		    0, mh->m_pkthdr.len, BUS_DMASYNC_PREREAD);
		/* Update descriptor */
		rxr->rx_base[j].read.hdr_addr =
		    htole64(rxbuf->hmap->dm_segs[0].ds_addr);

skip_head:
		/* Now the payload cluster */
		rxbuf->m_pack = ixgbe_getjcl(&adapter->jcl_head, M_DONTWAIT,
		    MT_DATA, M_PKTHDR, adapter->rx_mbuf_sz);
		if (rxbuf->m_pack == NULL) {
			error = ENOBUFS;
                        goto fail;
		}
		mp = rxbuf->m_pack;
		mp->m_pkthdr.len = mp->m_len = adapter->rx_mbuf_sz;
		/* Get the memory mapping */
		error = bus_dmamap_load_mbuf(rxr->ptag->dt_dmat,
		    rxbuf->pmap, mp, BUS_DMA_NOWAIT);
		if (error != 0)
                        goto fail;
		bus_dmamap_sync(rxr->ptag->dt_dmat, rxbuf->pmap,
		    0, adapter->rx_mbuf_sz, BUS_DMASYNC_PREREAD);
		/* Update descriptor */
		rxr->rx_base[j].read.pkt_addr =
		    htole64(rxbuf->pmap->dm_segs[0].ds_addr);
	}


	/* Setup our descriptor indices */
	rxr->next_to_check = 0;
	rxr->next_to_refresh = 0;
	rxr->lro_enabled = FALSE;
	rxr->rx_split_packets.ev_count = 0;
	rxr->rx_bytes.ev_count = 0;
	rxr->discard = FALSE;

	ixgbe_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

#ifdef LRO
	/*
	** Now set up the LRO interface:
	*/
	if (ifp->if_capenable & IFCAP_LRO) {
		device_t dev = adapter->dev;
		int err = tcp_lro_init(lro);
		if (err) {
			device_printf(dev, "LRO Initialization failed!\n");
			goto fail;
		}
		INIT_DEBUGOUT("RX Soft LRO Initialized\n");
		rxr->lro_enabled = TRUE;
		lro->ifp = adapter->ifp;
	}
#endif /* LRO */

	IXV_RX_UNLOCK(rxr);
	return (0);

fail:
	ixv_free_receive_ring(rxr);
	IXV_RX_UNLOCK(rxr);
	return (error);
}

/*********************************************************************
 *
 *  Initialize all receive rings.
 *
 **********************************************************************/
static int
ixv_setup_receive_structures(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;
	int j;

	for (j = 0; j < adapter->num_queues; j++, rxr++)
		if (ixv_setup_receive_ring(rxr))
			goto fail;

	return (0);
fail:
	/*
	 * Free RX buffers allocated so far, we will only handle
	 * the rings that completed, the failing case will have
	 * cleaned up for itself. 'j' failed, so its the terminus.
	 */
	for (int i = 0; i < j; ++i) {
		rxr = &adapter->rx_rings[i];
		ixv_free_receive_ring(rxr);
	}

	return (ENOBUFS);
}

/*********************************************************************
 *
 *  Setup receive registers and features.
 *
 **********************************************************************/
#define IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT 2

static void
ixv_initialize_receive_units(struct adapter *adapter)
{
	int i;
	struct	rx_ring	*rxr = adapter->rx_rings;
	struct ixgbe_hw	*hw = &adapter->hw;
	struct ifnet   *ifp = adapter->ifp;
	u32		bufsz, fctrl, rxcsum, hlreg;


	/* Enable broadcasts */
	fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);
	fctrl |= IXGBE_FCTRL_BAM;
	fctrl |= IXGBE_FCTRL_DPF;
	fctrl |= IXGBE_FCTRL_PMCF;
	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);

	/* Set for Jumbo Frames? */
	hlreg = IXGBE_READ_REG(hw, IXGBE_HLREG0);
	if (ifp->if_mtu > ETHERMTU) {
		hlreg |= IXGBE_HLREG0_JUMBOEN;
		bufsz = 4096 >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
	} else {
		hlreg &= ~IXGBE_HLREG0_JUMBOEN;
		bufsz = 2048 >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
	}
	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg);

	for (i = 0; i < adapter->num_queues; i++, rxr++) {
		u64 rdba = rxr->rxdma.dma_paddr;
		u32 reg, rxdctl;

		/* Do the queue enabling first */
		rxdctl = IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(i));
		rxdctl |= IXGBE_RXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_VFRXDCTL(i), rxdctl);
		for (int k = 0; k < 10; k++) {
			if (IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(i)) &
			    IXGBE_RXDCTL_ENABLE)
				break;
			else
				msec_delay(1);
		}
		wmb();

		/* Setup the Base and Length of the Rx Descriptor Ring */
		IXGBE_WRITE_REG(hw, IXGBE_VFRDBAL(i),
		    (rdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_VFRDBAH(i),
		    (rdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_VFRDLEN(i),
		    adapter->num_rx_desc * sizeof(union ixgbe_adv_rx_desc));

		/* Set up the SRRCTL register */
		reg = IXGBE_READ_REG(hw, IXGBE_VFSRRCTL(i));
		reg &= ~IXGBE_SRRCTL_BSIZEHDR_MASK;
		reg &= ~IXGBE_SRRCTL_BSIZEPKT_MASK;
		reg |= bufsz;
		if (rxr->hdr_split) {
			/* Use a standard mbuf for the header */
			reg |= ((IXV_RX_HDR <<
			    IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT)
			    & IXGBE_SRRCTL_BSIZEHDR_MASK);
			reg |= IXGBE_SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS;
		} else
			reg |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;
		IXGBE_WRITE_REG(hw, IXGBE_VFSRRCTL(i), reg);

		/* Setup the HW Rx Head and Tail Descriptor Pointers */
		IXGBE_WRITE_REG(hw, IXGBE_VFRDH(rxr->me), 0);
		IXGBE_WRITE_REG(hw, IXGBE_VFRDT(rxr->me),
		    adapter->num_rx_desc - 1);
	}

	rxcsum = IXGBE_READ_REG(hw, IXGBE_RXCSUM);

	if (ifp->if_capenable & IFCAP_RXCSUM)
		rxcsum |= IXGBE_RXCSUM_PCSD;

	if (!(rxcsum & IXGBE_RXCSUM_PCSD))
		rxcsum |= IXGBE_RXCSUM_IPPCSE;

	IXGBE_WRITE_REG(hw, IXGBE_RXCSUM, rxcsum);

	return;
}

/*********************************************************************
 *
 *  Free all receive rings.
 *
 **********************************************************************/
static void
ixv_free_receive_structures(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;

	for (int i = 0; i < adapter->num_queues; i++, rxr++) {
#ifdef LRO
		struct lro_ctrl		*lro = &rxr->lro;
#endif /* LRO */
		ixv_free_receive_buffers(rxr);
#ifdef LRO
		/* Free LRO memory */
		tcp_lro_free(lro);
#endif /* LRO */
		/* Free the ring memory as well */
		ixv_dma_free(adapter, &rxr->rxdma);
		IXV_RX_LOCK_DESTROY(rxr);
	}

	free(adapter->rx_rings, M_DEVBUF);
}


/*********************************************************************
 *
 *  Free receive ring data structures
 *
 **********************************************************************/
static void
ixv_free_receive_buffers(struct rx_ring *rxr)
{
	struct adapter		*adapter = rxr->adapter;
	struct ixv_rx_buf	*rxbuf;

	INIT_DEBUGOUT("free_receive_structures: begin");

	/* Cleanup any existing buffers */
	if (rxr->rx_buffers != NULL) {
		for (int i = 0; i < adapter->num_rx_desc; i++) {
			rxbuf = &rxr->rx_buffers[i];
			if (rxbuf->m_head != NULL) {
				ixgbe_dmamap_sync(rxr->htag, rxbuf->hmap,
				    BUS_DMASYNC_POSTREAD);
				ixgbe_dmamap_unload(rxr->htag, rxbuf->hmap);
				rxbuf->m_head->m_flags |= M_PKTHDR;
				m_freem(rxbuf->m_head);
			}
			if (rxbuf->m_pack != NULL) {
				/* XXX not ixgbe_* ? */
				bus_dmamap_sync(rxr->ptag->dt_dmat, rxbuf->pmap,
				    0, rxbuf->m_pack->m_pkthdr.len,
				    BUS_DMASYNC_POSTREAD);
				ixgbe_dmamap_unload(rxr->ptag, rxbuf->pmap);
				rxbuf->m_pack->m_flags |= M_PKTHDR;
				m_freem(rxbuf->m_pack);
			}
			rxbuf->m_head = NULL;
			rxbuf->m_pack = NULL;
			if (rxbuf->hmap != NULL) {
				ixgbe_dmamap_destroy(rxr->htag, rxbuf->hmap);
				rxbuf->hmap = NULL;
			}
			if (rxbuf->pmap != NULL) {
				ixgbe_dmamap_destroy(rxr->ptag, rxbuf->pmap);
				rxbuf->pmap = NULL;
			}
		}
		if (rxr->rx_buffers != NULL) {
			free(rxr->rx_buffers, M_DEVBUF);
			rxr->rx_buffers = NULL;
		}
	}

	if (rxr->htag != NULL) {
		ixgbe_dma_tag_destroy(rxr->htag);
		rxr->htag = NULL;
	}
	if (rxr->ptag != NULL) {
		ixgbe_dma_tag_destroy(rxr->ptag);
		rxr->ptag = NULL;
	}

	return;
}

static __inline void
ixv_rx_input(struct rx_ring *rxr, struct ifnet *ifp, struct mbuf *m, u32 ptype)
{
	int s;
                 
#ifdef LRO
	struct adapter	*adapter = ifp->if_softc;
	struct ethercom *ec = &adapter->osdep.ec;

        /*
         * ATM LRO is only for IPv4/TCP packets and TCP checksum of the packet
         * should be computed by hardware. Also it should not have VLAN tag in
         * ethernet header.
         */
        if (rxr->lro_enabled &&
            (ec->ec_capenable & ETHERCAP_VLAN_HWTAGGING) != 0 &&
            (ptype & IXGBE_RXDADV_PKTTYPE_ETQF) == 0 &&
            (ptype & (IXGBE_RXDADV_PKTTYPE_IPV4 | IXGBE_RXDADV_PKTTYPE_TCP)) ==
            (IXGBE_RXDADV_PKTTYPE_IPV4 | IXGBE_RXDADV_PKTTYPE_TCP) &&
            (m->m_pkthdr.csum_flags & (CSUM_DATA_VALID | CSUM_PSEUDO_HDR)) ==
            (CSUM_DATA_VALID | CSUM_PSEUDO_HDR)) {
                /*
                 * Send to the stack if:
                 **  - LRO not enabled, or
                 **  - no LRO resources, or
                 **  - lro enqueue fails
                 */
                if (rxr->lro.lro_cnt != 0)
                        if (tcp_lro_rx(&rxr->lro, m, 0) == 0)
                                return;
        }
#endif /* LRO */

	IXV_RX_UNLOCK(rxr);

	s = splnet();
	/* Pass this up to any BPF listeners. */
	bpf_mtap(ifp, m);
        (*ifp->if_input)(ifp, m);
	splx(s);

	IXV_RX_LOCK(rxr);
}

static __inline void
ixv_rx_discard(struct rx_ring *rxr, int i)
{
	struct ixv_rx_buf	*rbuf;

	rbuf = &rxr->rx_buffers[i];
	if (rbuf->fmp != NULL) {/* Partial chain ? */
		rbuf->fmp->m_flags |= M_PKTHDR;
		m_freem(rbuf->fmp);
		rbuf->fmp = NULL;
	}

	/*
	** With advanced descriptors the writeback
	** clobbers the buffer addrs, so its easier
	** to just free the existing mbufs and take
	** the normal refresh path to get new buffers
	** and mapping.
	*/
	if (rbuf->m_head) {
		m_free(rbuf->m_head);
		rbuf->m_head = NULL;
	}

	if (rbuf->m_pack) {
		m_free(rbuf->m_pack);
		rbuf->m_pack = NULL;
	}

	return;
}


/*********************************************************************
 *
 *  This routine executes in interrupt context. It replenishes
 *  the mbufs in the descriptor and sends data which has been
 *  dma'ed into host memory to upper layer.
 *
 *  We loop at most count times if count is > 0, or until done if
 *  count < 0.
 *
 *  Return TRUE for more work, FALSE for all clean.
 *********************************************************************/
static bool
ixv_rxeof(struct ix_queue *que, int count)
{
	struct adapter		*adapter = que->adapter;
	struct rx_ring		*rxr = que->rxr;
	struct ifnet		*ifp = adapter->ifp;
#ifdef LRO
	struct lro_ctrl		*lro = &rxr->lro;
	struct lro_entry	*queued;
#endif /* LRO */
	int			i, nextp, processed = 0;
	u32			staterr = 0;
	union ixgbe_adv_rx_desc	*cur;
	struct ixv_rx_buf	*rbuf, *nbuf;

	IXV_RX_LOCK(rxr);

	for (i = rxr->next_to_check; count != 0;) {
		struct mbuf	*sendmp, *mh, *mp;
		u32		ptype;
		u16		hlen, plen, hdr, vtag;
		bool		eop;
 
		/* Sync the ring. */
		ixgbe_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		cur = &rxr->rx_base[i];
		staterr = le32toh(cur->wb.upper.status_error);

		if ((staterr & IXGBE_RXD_STAT_DD) == 0)
			break;
		if ((ifp->if_flags & IFF_RUNNING) == 0)
			break;

		count--;
		sendmp = NULL;
		nbuf = NULL;
		cur->wb.upper.status_error = 0;
		rbuf = &rxr->rx_buffers[i];
		mh = rbuf->m_head;
		mp = rbuf->m_pack;

		plen = le16toh(cur->wb.upper.length);
		ptype = le32toh(cur->wb.lower.lo_dword.data) &
		    IXGBE_RXDADV_PKTTYPE_MASK;
		hdr = le16toh(cur->wb.lower.lo_dword.hs_rss.hdr_info);
		vtag = le16toh(cur->wb.upper.vlan);
		eop = ((staterr & IXGBE_RXD_STAT_EOP) != 0);

		/* Make sure all parts of a bad packet are discarded */
		if (((staterr & IXGBE_RXDADV_ERR_FRAME_ERR_MASK) != 0) ||
		    (rxr->discard)) {
			ifp->if_ierrors++;
			rxr->rx_discarded.ev_count++;
			if (!eop)
				rxr->discard = TRUE;
			else
				rxr->discard = FALSE;
			ixv_rx_discard(rxr, i);
			goto next_desc;
		}

		if (!eop) {
			nextp = i + 1;
			if (nextp == adapter->num_rx_desc)
				nextp = 0;
			nbuf = &rxr->rx_buffers[nextp];
			prefetch(nbuf);
		}
		/*
		** The header mbuf is ONLY used when header 
		** split is enabled, otherwise we get normal 
		** behavior, ie, both header and payload
		** are DMA'd into the payload buffer.
		**
		** Rather than using the fmp/lmp global pointers
		** we now keep the head of a packet chain in the
		** buffer struct and pass this along from one
		** descriptor to the next, until we get EOP.
		*/
		if (rxr->hdr_split && (rbuf->fmp == NULL)) {
			/* This must be an initial descriptor */
			hlen = (hdr & IXGBE_RXDADV_HDRBUFLEN_MASK) >>
			    IXGBE_RXDADV_HDRBUFLEN_SHIFT;
			if (hlen > IXV_RX_HDR)
				hlen = IXV_RX_HDR;
			mh->m_len = hlen;
			mh->m_flags |= M_PKTHDR;
			mh->m_next = NULL;
			mh->m_pkthdr.len = mh->m_len;
			/* Null buf pointer so it is refreshed */
			rbuf->m_head = NULL;
			/*
			** Check the payload length, this
			** could be zero if its a small
			** packet.
			*/
			if (plen > 0) {
				mp->m_len = plen;
				mp->m_next = NULL;
				mp->m_flags &= ~M_PKTHDR;
				mh->m_next = mp;
				mh->m_pkthdr.len += mp->m_len;
				/* Null buf pointer so it is refreshed */
				rbuf->m_pack = NULL;
				rxr->rx_split_packets.ev_count++;
			}
			/*
			** Now create the forward
			** chain so when complete 
			** we wont have to.
			*/
                        if (eop == 0) {
				/* stash the chain head */
                                nbuf->fmp = mh;
				/* Make forward chain */
                                if (plen)
                                        mp->m_next = nbuf->m_pack;
                                else
                                        mh->m_next = nbuf->m_pack;
                        } else {
				/* Singlet, prepare to send */
                                sendmp = mh;
                                if (VLAN_ATTACHED(&adapter->osdep.ec) &&
				  (staterr & IXGBE_RXD_STAT_VP)) {
					VLAN_INPUT_TAG(ifp, sendmp, vtag,
					    printf("%s: could not apply VLAN "
					        "tag", __func__));
                                }
                        }
		} else {
			/*
			** Either no header split, or a
			** secondary piece of a fragmented
			** split packet.
			*/
			mp->m_len = plen;
			/*
			** See if there is a stored head
			** that determines what we are
			*/
			sendmp = rbuf->fmp;
			rbuf->m_pack = rbuf->fmp = NULL;

			if (sendmp != NULL) /* secondary frag */
				sendmp->m_pkthdr.len += mp->m_len;
			else {
				/* first desc of a non-ps chain */
				sendmp = mp;
				sendmp->m_flags |= M_PKTHDR;
				sendmp->m_pkthdr.len = mp->m_len;
				if (staterr & IXGBE_RXD_STAT_VP) {
					/* XXX Do something reasonable on
					 * error.
					 */
					VLAN_INPUT_TAG(ifp, sendmp, vtag,
					    printf("%s: could not apply VLAN "
					        "tag", __func__));
				}
                        }
			/* Pass the head pointer on */
			if (eop == 0) {
				nbuf->fmp = sendmp;
				sendmp = NULL;
				mp->m_next = nbuf->m_pack;
			}
		}
		++processed;
		/* Sending this frame? */
		if (eop) {
			sendmp->m_pkthdr.rcvif = ifp;
			ifp->if_ipackets++;
			rxr->rx_packets.ev_count++;
			/* capture data for AIM */
			rxr->bytes += sendmp->m_pkthdr.len;
			rxr->rx_bytes.ev_count += sendmp->m_pkthdr.len;
			if ((ifp->if_capenable & IFCAP_RXCSUM) != 0) {
				ixv_rx_checksum(staterr, sendmp, ptype,
				   &adapter->stats);
			}
#if __FreeBSD_version >= 800000
			sendmp->m_pkthdr.flowid = que->msix;
			sendmp->m_flags |= M_FLOWID;
#endif
		}
next_desc:
		ixgbe_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* Advance our pointers to the next descriptor. */
		if (++i == adapter->num_rx_desc)
			i = 0;

		/* Now send to the stack or do LRO */
		if (sendmp != NULL)
			ixv_rx_input(rxr, ifp, sendmp, ptype);

               /* Every 8 descriptors we go to refresh mbufs */
		if (processed == 8) {
			ixv_refresh_mbufs(rxr, i);
			processed = 0;
		}
	}

	/* Refresh any remaining buf structs */
	if (ixv_rx_unrefreshed(rxr))
		ixv_refresh_mbufs(rxr, i);

	rxr->next_to_check = i;

#ifdef LRO
	/*
	 * Flush any outstanding LRO work
	 */
	while ((queued = SLIST_FIRST(&lro->lro_active)) != NULL) {
		SLIST_REMOVE_HEAD(&lro->lro_active, next);
		tcp_lro_flush(lro, queued);
	}
#endif /* LRO */

	IXV_RX_UNLOCK(rxr);

	/*
	** We still have cleaning to do?
	** Schedule another interrupt if so.
	*/
	if ((staterr & IXGBE_RXD_STAT_DD) != 0) {
		ixv_rearm_queues(adapter, (u64)(1ULL << que->msix));
		return true;
	}

	return false;
}


/*********************************************************************
 *
 *  Verify that the hardware indicated that the checksum is valid.
 *  Inform the stack about the status of checksum so that stack
 *  doesn't spend time verifying the checksum.
 *
 *********************************************************************/
static void
ixv_rx_checksum(u32 staterr, struct mbuf * mp, u32 ptype,
    struct ixgbevf_hw_stats *stats)
{
	u16	status = (u16) staterr;
	u8	errors = (u8) (staterr >> 24);
#if 0
	bool	sctp = FALSE;

	if ((ptype & IXGBE_RXDADV_PKTTYPE_ETQF) == 0 &&
	    (ptype & IXGBE_RXDADV_PKTTYPE_SCTP) != 0)
		sctp = TRUE;
#endif
	if (status & IXGBE_RXD_STAT_IPCS) {
		stats->ipcs.ev_count++;
		if (!(errors & IXGBE_RXD_ERR_IPE)) {
			/* IP Checksum Good */
			mp->m_pkthdr.csum_flags |= M_CSUM_IPv4;

		} else {
			stats->ipcs_bad.ev_count++;
			mp->m_pkthdr.csum_flags = M_CSUM_IPv4|M_CSUM_IPv4_BAD;
		}
	}
	if (status & IXGBE_RXD_STAT_L4CS) {
		stats->l4cs.ev_count++;
		int type = M_CSUM_TCPv4|M_CSUM_TCPv6|M_CSUM_UDPv4|M_CSUM_UDPv6;
		if (!(errors & IXGBE_RXD_ERR_TCPE)) {
			mp->m_pkthdr.csum_flags |= type;
		} else {
			stats->l4cs_bad.ev_count++;
			mp->m_pkthdr.csum_flags |= type | M_CSUM_TCP_UDP_BAD;
		} 
	}
	return;
}

static void
ixv_setup_vlan_support(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32		ctrl, vid, vfta, retry;


	/*
	** We get here thru init_locked, meaning
	** a soft reset, this has already cleared
	** the VFTA and other state, so if there
	** have been no vlan's registered do nothing.
	*/
	if (adapter->num_vlans == 0)
		return;

	/* Enable the queues */
	for (int i = 0; i < adapter->num_queues; i++) {
		ctrl = IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(i));
		ctrl |= IXGBE_RXDCTL_VME;
		IXGBE_WRITE_REG(hw, IXGBE_VFRXDCTL(i), ctrl);
	}

	/*
	** A soft reset zero's out the VFTA, so
	** we need to repopulate it now.
	*/
	for (int i = 0; i < VFTA_SIZE; i++) {
		if (ixv_shadow_vfta[i] == 0)
			continue;
		vfta = ixv_shadow_vfta[i];
		/*
		** Reconstruct the vlan id's
		** based on the bits set in each
		** of the array ints.
		*/
		for ( int j = 0; j < 32; j++) {
			retry = 0;
			if ((vfta & (1 << j)) == 0)
				continue;
			vid = (i * 32) + j;
			/* Call the shared code mailbox routine */
			while (ixgbe_set_vfta(hw, vid, 0, TRUE)) {
				if (++retry > 5)
					break;
			}
		}
	}
}

#if 0	/* XXX Badly need to overhaul vlan(4) on NetBSD. */
/*
** This routine is run via an vlan config EVENT,
** it enables us to use the HW Filter table since
** we can get the vlan id. This just creates the
** entry in the soft version of the VFTA, init will
** repopulate the real table.
*/
static void
ixv_register_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u16		index, bit;

	if (ifp->if_softc !=  arg)   /* Not our event */
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	IXV_CORE_LOCK(adapter);
	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	ixv_shadow_vfta[index] |= (1 << bit);
	/* Re-init to load the changes */
	ixv_init_locked(adapter);
	IXV_CORE_UNLOCK(adapter);
}

/*
** This routine is run via an vlan
** unconfig EVENT, remove our entry
** in the soft vfta.
*/
static void
ixv_unregister_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u16		index, bit;

	if (ifp->if_softc !=  arg)
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	IXV_CORE_LOCK(adapter);
	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	ixv_shadow_vfta[index] &= ~(1 << bit);
	/* Re-init to load the changes */
	ixv_init_locked(adapter);
	IXV_CORE_UNLOCK(adapter);
}
#endif

static void
ixv_enable_intr(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct ix_queue *que = adapter->queues;
	u32 mask = (IXGBE_EIMS_ENABLE_MASK & ~IXGBE_EIMS_RTX_QUEUE);


	IXGBE_WRITE_REG(hw, IXGBE_VTEIMS, mask);

	mask = IXGBE_EIMS_ENABLE_MASK;
	mask &= ~(IXGBE_EIMS_OTHER | IXGBE_EIMS_LSC);
	IXGBE_WRITE_REG(hw, IXGBE_VTEIAC, mask);

        for (int i = 0; i < adapter->num_queues; i++, que++)
		ixv_enable_queue(adapter, que->msix);

	IXGBE_WRITE_FLUSH(hw);

	return;
}

static void
ixv_disable_intr(struct adapter *adapter)
{
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_VTEIAC, 0);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_VTEIMC, ~0);
	IXGBE_WRITE_FLUSH(&adapter->hw);
	return;
}

/*
** Setup the correct IVAR register for a particular MSIX interrupt
**  - entry is the register array entry
**  - vector is the MSIX vector for this queue
**  - type is RX/TX/MISC
*/
static void
ixv_set_ivar(struct adapter *adapter, u8 entry, u8 vector, s8 type)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 ivar, index;

	vector |= IXGBE_IVAR_ALLOC_VAL;

	if (type == -1) { /* MISC IVAR */
		ivar = IXGBE_READ_REG(hw, IXGBE_VTIVAR_MISC);
		ivar &= ~0xFF;
		ivar |= vector;
		IXGBE_WRITE_REG(hw, IXGBE_VTIVAR_MISC, ivar);
	} else {	/* RX/TX IVARS */
		index = (16 * (entry & 1)) + (8 * type);
		ivar = IXGBE_READ_REG(hw, IXGBE_VTIVAR(entry >> 1));
		ivar &= ~(0xFF << index);
		ivar |= (vector << index);
		IXGBE_WRITE_REG(hw, IXGBE_VTIVAR(entry >> 1), ivar);
	}
}

static void
ixv_configure_ivars(struct adapter *adapter)
{
	struct  ix_queue *que = adapter->queues;

        for (int i = 0; i < adapter->num_queues; i++, que++) {
		/* First the RX queue entry */
                ixv_set_ivar(adapter, i, que->msix, 0);
		/* ... and the TX */
		ixv_set_ivar(adapter, i, que->msix, 1);
		/* Set an initial value in EITR */
                IXGBE_WRITE_REG(&adapter->hw,
                    IXGBE_VTEITR(que->msix), IXV_EITR_DEFAULT);
	}

	/* For the Link interrupt */
        ixv_set_ivar(adapter, 1, adapter->mbxvec, -1);
}


/*
** Tasklet handler for MSIX MBX interrupts
**  - do outside interrupt since it might sleep
*/
static void
ixv_handle_mbx(void *context)
{
	struct adapter  *adapter = context;

	ixgbe_check_link(&adapter->hw,
	    &adapter->link_speed, &adapter->link_up, 0);
	ixv_update_link_status(adapter);
}

/*
** The VF stats registers never have a truely virgin
** starting point, so this routine tries to make an
** artificial one, marking ground zero on attach as
** it were.
*/
static void
ixv_save_stats(struct adapter *adapter)
{
	if (adapter->stats.vfgprc || adapter->stats.vfgptc) {
		adapter->stats.saved_reset_vfgprc +=
		    adapter->stats.vfgprc - adapter->stats.base_vfgprc;
		adapter->stats.saved_reset_vfgptc +=
		    adapter->stats.vfgptc - adapter->stats.base_vfgptc;
		adapter->stats.saved_reset_vfgorc +=
		    adapter->stats.vfgorc - adapter->stats.base_vfgorc;
		adapter->stats.saved_reset_vfgotc +=
		    adapter->stats.vfgotc - adapter->stats.base_vfgotc;
		adapter->stats.saved_reset_vfmprc +=
		    adapter->stats.vfmprc - adapter->stats.base_vfmprc;
	}
}
 
static void
ixv_init_stats(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
 
	adapter->stats.last_vfgprc = IXGBE_READ_REG(hw, IXGBE_VFGPRC);
	adapter->stats.last_vfgorc = IXGBE_READ_REG(hw, IXGBE_VFGORC_LSB);
	adapter->stats.last_vfgorc |=
	    (((u64)(IXGBE_READ_REG(hw, IXGBE_VFGORC_MSB))) << 32);

	adapter->stats.last_vfgptc = IXGBE_READ_REG(hw, IXGBE_VFGPTC);
	adapter->stats.last_vfgotc = IXGBE_READ_REG(hw, IXGBE_VFGOTC_LSB);
	adapter->stats.last_vfgotc |=
	    (((u64)(IXGBE_READ_REG(hw, IXGBE_VFGOTC_MSB))) << 32);

	adapter->stats.last_vfmprc = IXGBE_READ_REG(hw, IXGBE_VFMPRC);

	adapter->stats.base_vfgprc = adapter->stats.last_vfgprc;
	adapter->stats.base_vfgorc = adapter->stats.last_vfgorc;
	adapter->stats.base_vfgptc = adapter->stats.last_vfgptc;
	adapter->stats.base_vfgotc = adapter->stats.last_vfgotc;
	adapter->stats.base_vfmprc = adapter->stats.last_vfmprc;
}

#define UPDATE_STAT_32(reg, last, count)		\
{							\
	u32 current = IXGBE_READ_REG(hw, reg);		\
	if (current < last)				\
		count += 0x100000000LL;			\
	last = current;					\
	count &= 0xFFFFFFFF00000000LL;			\
	count |= current;				\
}

#define UPDATE_STAT_36(lsb, msb, last, count) 		\
{							\
	u64 cur_lsb = IXGBE_READ_REG(hw, lsb);		\
	u64 cur_msb = IXGBE_READ_REG(hw, msb);		\
	u64 current = ((cur_msb << 32) | cur_lsb);	\
	if (current < last)				\
		count += 0x1000000000LL;		\
	last = current;					\
	count &= 0xFFFFFFF000000000LL;			\
	count |= current;				\
}

/*
** ixv_update_stats - Update the board statistics counters.
*/
void
ixv_update_stats(struct adapter *adapter)
{
        struct ixgbe_hw *hw = &adapter->hw;

        UPDATE_STAT_32(IXGBE_VFGPRC, adapter->stats.last_vfgprc,
	    adapter->stats.vfgprc);
        UPDATE_STAT_32(IXGBE_VFGPTC, adapter->stats.last_vfgptc,
	    adapter->stats.vfgptc);
        UPDATE_STAT_36(IXGBE_VFGORC_LSB, IXGBE_VFGORC_MSB,
	    adapter->stats.last_vfgorc, adapter->stats.vfgorc);
        UPDATE_STAT_36(IXGBE_VFGOTC_LSB, IXGBE_VFGOTC_MSB,
	    adapter->stats.last_vfgotc, adapter->stats.vfgotc);
        UPDATE_STAT_32(IXGBE_VFMPRC, adapter->stats.last_vfmprc,
	    adapter->stats.vfmprc);
}

/**********************************************************************
 *
 *  This routine is called only when ixgbe_display_debug_stats is enabled.
 *  This routine provides a way to take a look at important statistics
 *  maintained by the driver and hardware.
 *
 **********************************************************************/
static void
ixv_print_hw_stats(struct adapter * adapter)
{
        device_t dev = adapter->dev;

        device_printf(dev,"Std Mbuf Failed = %"PRIu64"\n",
               adapter->mbuf_defrag_failed.ev_count);
        device_printf(dev,"Driver dropped packets = %"PRIu64"\n",
               adapter->dropped_pkts.ev_count);
        device_printf(dev, "watchdog timeouts = %"PRIu64"\n",
               adapter->watchdog_events.ev_count);

        device_printf(dev,"Good Packets Rcvd = %lld\n",
               (long long)adapter->stats.vfgprc);
        device_printf(dev,"Good Packets Xmtd = %lld\n",
               (long long)adapter->stats.vfgptc);
        device_printf(dev,"TSO Transmissions = %"PRIu64"\n",
               adapter->tso_tx.ev_count);

}

/**********************************************************************
 *
 *  This routine is called only when em_display_debug_stats is enabled.
 *  This routine provides a way to take a look at important statistics
 *  maintained by the driver and hardware.
 *
 **********************************************************************/
static void
ixv_print_debug_info(struct adapter *adapter)
{
        device_t dev = adapter->dev;
        struct ixgbe_hw         *hw = &adapter->hw;
        struct ix_queue         *que = adapter->queues;
        struct rx_ring          *rxr;
        struct tx_ring          *txr;
#ifdef LRO
        struct lro_ctrl         *lro;
#endif /* LRO */

        device_printf(dev,"Error Byte Count = %u \n",
            IXGBE_READ_REG(hw, IXGBE_ERRBC));

        for (int i = 0; i < adapter->num_queues; i++, que++) {
                txr = que->txr;
                rxr = que->rxr;
#ifdef LRO
                lro = &rxr->lro;
#endif /* LRO */
                device_printf(dev,"QUE(%d) IRQs Handled: %lu\n",
                    que->msix, (long)que->irqs);
                device_printf(dev,"RX(%d) Packets Received: %lld\n",
                    rxr->me, (long long)rxr->rx_packets.ev_count);
                device_printf(dev,"RX(%d) Split RX Packets: %lld\n",
                    rxr->me, (long long)rxr->rx_split_packets.ev_count);
                device_printf(dev,"RX(%d) Bytes Received: %lu\n",
                    rxr->me, (long)rxr->rx_bytes.ev_count);
#ifdef LRO
                device_printf(dev,"RX(%d) LRO Queued= %d\n",
                    rxr->me, lro->lro_queued);
                device_printf(dev,"RX(%d) LRO Flushed= %d\n",
                    rxr->me, lro->lro_flushed);
#endif /* LRO */
                device_printf(dev,"TX(%d) Packets Sent: %lu\n",
                    txr->me, (long)txr->total_packets.ev_count);
                device_printf(dev,"TX(%d) NO Desc Avail: %lu\n",
                    txr->me, (long)txr->no_desc_avail.ev_count);
        }

        device_printf(dev,"MBX IRQ Handled: %lu\n",
            (long)adapter->mbx_irq.ev_count);
        return;
}

static int
ixv_sysctl_stats(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int             error;
	int		result;
	struct adapter *adapter;

	node = *rnode;
	adapter = (struct adapter *)node.sysctl_data;
	node.sysctl_data = &result;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error != 0)
		return error;

	if (result == 1)
		ixv_print_hw_stats(adapter);

	return 0;
}

static int
ixv_sysctl_debug(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int error, result;
	struct adapter *adapter;

	node = *rnode;
	adapter = (struct adapter *)node.sysctl_data;
	node.sysctl_data = &result;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error)
		return error;

	if (result == 1)
		ixv_print_debug_info(adapter);

	return 0;
}

/*
** Set flow control using sysctl:
** Flow control values:
** 	0 - off
**	1 - rx pause
**	2 - tx pause
**	3 - full
*/
static int
ixv_set_flowcntl(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int error;
	struct adapter *adapter;

	node = *rnode;
	adapter = (struct adapter *)node.sysctl_data;
	node.sysctl_data = &ixv_flow_control;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));

	if (error)
		return (error);

	switch (ixv_flow_control) {
		case ixgbe_fc_rx_pause:
		case ixgbe_fc_tx_pause:
		case ixgbe_fc_full:
			adapter->hw.fc.requested_mode = ixv_flow_control;
			break;
		case ixgbe_fc_none:
		default:
			adapter->hw.fc.requested_mode = ixgbe_fc_none;
	}

	ixgbe_fc_enable(&adapter->hw);
	return error;
}

const struct sysctlnode *
ixv_sysctl_instance(struct adapter *adapter)
{
	const char *dvname;
	struct sysctllog **log;
	int rc;
	const struct sysctlnode *rnode;

	log = &adapter->sysctllog;
	dvname = device_xname(adapter->dev);

	if ((rc = sysctl_createv(log, 0, NULL, &rnode,
	    0, CTLTYPE_NODE, dvname,
	    SYSCTL_DESCR("ixv information and settings"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	return rnode;
err:
	printf("%s: sysctl_createv failed, rc = %d\n", __func__, rc);
	return NULL;
}

static void
ixv_add_rx_process_limit(struct adapter *adapter, const char *name,
        const char *description, int *limit, int value)
{
	const struct sysctlnode *rnode, *cnode;
	struct sysctllog **log = &adapter->sysctllog;

        *limit = value;

	if ((rnode = ixv_sysctl_instance(adapter)) == NULL)
		aprint_error_dev(adapter->dev,
		    "could not create sysctl root\n");
	else if (sysctl_createv(log, 0, &rnode, &cnode,
	    CTLFLAG_READWRITE,
	    CTLTYPE_INT,
	    name, SYSCTL_DESCR(description),
	    NULL, 0, limit, 0,
	    CTL_CREATE, CTL_EOL) != 0) {
		aprint_error_dev(adapter->dev, "%s: could not create sysctl",
		    __func__);
	}
}

