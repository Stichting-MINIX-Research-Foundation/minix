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
/*
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc.
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
/*$FreeBSD: head/sys/dev/ixgbe/ixgbe.c 279805 2015-03-09 10:29:15Z araujo $*/
/*$NetBSD: ixgbe.c,v 1.36 2015/08/17 06:16:03 knakahara Exp $*/

#include "opt_inet.h"
#include "opt_inet6.h"

#include "ixgbe.h"
#include "vlan.h"

#include <sys/cprng.h>

/*********************************************************************
 *  Set this to one to display debug statistics
 *********************************************************************/
int             ixgbe_display_debug_stats = 0;

/*********************************************************************
 *  Driver version
 *********************************************************************/
char ixgbe_driver_version[] = "2.5.15";

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into ixgbe_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/

static ixgbe_vendor_info_t ixgbe_vendor_info_array[] =
{
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598AF_DUAL_PORT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598AF_SINGLE_PORT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598EB_CX4, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598AT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598AT2, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598_DA_DUAL_PORT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598_CX4_DUAL_PORT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598EB_XF_LR, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598EB_SFP_LOM, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_KX4, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_KX4_MEZZ, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_SFP, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_XAUI_LOM, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_CX4, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_T3_LOM, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_COMBO_BACKPLANE, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_BACKPLANE_FCOE, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_SFP_SF2, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_SFP_FCOE, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599EN_SFP, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_SFP_SF_QP, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X540T, 0, 0, 0},
	/* required last entry */
	{0, 0, 0, 0, 0}
};

/*********************************************************************
 *  Table of branding strings
 *********************************************************************/

static const char    *ixgbe_strings[] = {
	"Intel(R) PRO/10GbE PCI-Express Network Driver"
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static int      ixgbe_probe(device_t, cfdata_t, void *);
static void     ixgbe_attach(device_t, device_t, void *);
static int      ixgbe_detach(device_t, int);
#if 0
static int      ixgbe_shutdown(device_t);
#endif
#ifdef IXGBE_LEGACY_TX
static void     ixgbe_start(struct ifnet *);
static void     ixgbe_start_locked(struct tx_ring *, struct ifnet *);
#else /* ! IXGBE_LEGACY_TX */
static int	ixgbe_mq_start(struct ifnet *, struct mbuf *);
static int	ixgbe_mq_start_locked(struct ifnet *, struct tx_ring *);
static void	ixgbe_qflush(struct ifnet *);
static void	ixgbe_deferred_mq_start(void *, int);
#endif /* IXGBE_LEGACY_TX */
static int      ixgbe_ioctl(struct ifnet *, u_long, void *);
static void	ixgbe_ifstop(struct ifnet *, int);
static int	ixgbe_init(struct ifnet *);
static void	ixgbe_init_locked(struct adapter *);
static void     ixgbe_stop(void *);
static void     ixgbe_media_status(struct ifnet *, struct ifmediareq *);
static int      ixgbe_media_change(struct ifnet *);
static void     ixgbe_identify_hardware(struct adapter *);
static int      ixgbe_allocate_pci_resources(struct adapter *,
		    const struct pci_attach_args *);
static void	ixgbe_get_slot_info(struct ixgbe_hw *);
static int      ixgbe_allocate_msix(struct adapter *,
		    const struct pci_attach_args *);
static int      ixgbe_allocate_legacy(struct adapter *,
		    const struct pci_attach_args *);
static int	ixgbe_allocate_queues(struct adapter *);
static int	ixgbe_setup_msix(struct adapter *);
static void	ixgbe_free_pci_resources(struct adapter *);
static void	ixgbe_local_timer(void *);
static int	ixgbe_setup_interface(device_t, struct adapter *);
static void	ixgbe_config_link(struct adapter *);

static int      ixgbe_allocate_transmit_buffers(struct tx_ring *);
static int	ixgbe_setup_transmit_structures(struct adapter *);
static void	ixgbe_setup_transmit_ring(struct tx_ring *);
static void     ixgbe_initialize_transmit_units(struct adapter *);
static void     ixgbe_free_transmit_structures(struct adapter *);
static void     ixgbe_free_transmit_buffers(struct tx_ring *);

static int      ixgbe_allocate_receive_buffers(struct rx_ring *);
static int      ixgbe_setup_receive_structures(struct adapter *);
static int	ixgbe_setup_receive_ring(struct rx_ring *);
static void     ixgbe_initialize_receive_units(struct adapter *);
static void     ixgbe_free_receive_structures(struct adapter *);
static void     ixgbe_free_receive_buffers(struct rx_ring *);
static void	ixgbe_setup_hw_rsc(struct rx_ring *);

static void     ixgbe_enable_intr(struct adapter *);
static void     ixgbe_disable_intr(struct adapter *);
static void     ixgbe_update_stats_counters(struct adapter *);
static void	ixgbe_txeof(struct tx_ring *);
static bool	ixgbe_rxeof(struct ix_queue *);
static void	ixgbe_rx_checksum(u32, struct mbuf *, u32,
		    struct ixgbe_hw_stats *);
static void     ixgbe_set_promisc(struct adapter *);
static void     ixgbe_set_multi(struct adapter *);
static void     ixgbe_update_link_status(struct adapter *);
static void	ixgbe_refresh_mbufs(struct rx_ring *, int);
static int      ixgbe_xmit(struct tx_ring *, struct mbuf *);
static int	ixgbe_set_flowcntl(SYSCTLFN_PROTO);
static int	ixgbe_set_advertise(SYSCTLFN_PROTO);
static int	ixgbe_set_thermal_test(SYSCTLFN_PROTO);
static int	ixgbe_dma_malloc(struct adapter *, bus_size_t,
		    struct ixgbe_dma_alloc *, int);
static void     ixgbe_dma_free(struct adapter *, struct ixgbe_dma_alloc *);
static int	ixgbe_tx_ctx_setup(struct tx_ring *,
		    struct mbuf *, u32 *, u32 *);
static int	ixgbe_tso_setup(struct tx_ring *,
		    struct mbuf *, u32 *, u32 *);
static void	ixgbe_set_ivar(struct adapter *, u8, u8, s8);
static void	ixgbe_configure_ivars(struct adapter *);
static u8 *	ixgbe_mc_array_itr(struct ixgbe_hw *, u8 **, u32 *);

static void	ixgbe_setup_vlan_hw_support(struct adapter *);
#if 0
static void	ixgbe_register_vlan(void *, struct ifnet *, u16);
static void	ixgbe_unregister_vlan(void *, struct ifnet *, u16);
#endif

static void     ixgbe_add_hw_stats(struct adapter *adapter);

static __inline void ixgbe_rx_discard(struct rx_ring *, int);
static __inline void ixgbe_rx_input(struct rx_ring *, struct ifnet *,
		    struct mbuf *, u32);

static void	ixgbe_enable_rx_drop(struct adapter *);
static void	ixgbe_disable_rx_drop(struct adapter *);

/* Support for pluggable optic modules */
static bool	ixgbe_sfp_probe(struct adapter *);
static void	ixgbe_setup_optics(struct adapter *);

/* Legacy (single vector interrupt handler */
static int	ixgbe_legacy_irq(void *);

#if defined(NETBSD_MSI_OR_MSIX)
/* The MSI/X Interrupt handlers */
static int	ixgbe_msix_que(void *);
static int	ixgbe_msix_link(void *);
#endif

/* Software interrupts for deferred work */
static void	ixgbe_handle_que(void *);
static void	ixgbe_handle_link(void *);
static void	ixgbe_handle_msf(void *);
static void	ixgbe_handle_mod(void *);

const struct sysctlnode *ixgbe_sysctl_instance(struct adapter *);
static ixgbe_vendor_info_t *ixgbe_lookup(const struct pci_attach_args *);

#ifdef IXGBE_FDIR
static void	ixgbe_atr(struct tx_ring *, struct mbuf *);
static void	ixgbe_reinit_fdir(void *, int);
#endif

/* Missing shared code prototype */
extern void ixgbe_stop_mac_link_on_d3_82599(struct ixgbe_hw *hw);

/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/

CFATTACH_DECL3_NEW(ixg, sizeof(struct adapter),
    ixgbe_probe, ixgbe_attach, ixgbe_detach, NULL, NULL, NULL,
    DVF_DETACH_SHUTDOWN);

#if 0
devclass_t ixgbe_devclass;
DRIVER_MODULE(ixgbe, pci, ixgbe_driver, ixgbe_devclass, 0, 0);

MODULE_DEPEND(ixgbe, pci, 1, 1, 1);
MODULE_DEPEND(ixgbe, ether, 1, 1, 1);
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
static int ixgbe_enable_aim = TRUE;
#define SYSCTL_INT(__x, __y)
SYSCTL_INT("hw.ixgbe.enable_aim", &ixgbe_enable_aim);

static int ixgbe_max_interrupt_rate = (4000000 / IXGBE_LOW_LATENCY);
SYSCTL_INT("hw.ixgbe.max_interrupt_rate", &ixgbe_max_interrupt_rate);

/* How many packets rxeof tries to clean at a time */
static int ixgbe_rx_process_limit = 256;
SYSCTL_INT("hw.ixgbe.rx_process_limit", &ixgbe_rx_process_limit);

/* How many packets txeof tries to clean at a time */
static int ixgbe_tx_process_limit = 256;
SYSCTL_INT("hw.ixgbe.tx_process_limit", &ixgbe_tx_process_limit);

/*
** Smart speed setting, default to on
** this only works as a compile option
** right now as its during attach, set
** this to 'ixgbe_smart_speed_off' to
** disable.
*/
static int ixgbe_smart_speed = ixgbe_smart_speed_on;

/*
 * MSIX should be the default for best performance,
 * but this allows it to be forced off for testing.
 */
static int ixgbe_enable_msix = 1;
SYSCTL_INT("hw.ixgbe.enable_msix", &ixgbe_enable_msix);

#if defined(NETBSD_MSI_OR_MSIX)
/*
 * Number of Queues, can be set to 0,
 * it then autoconfigures based on the
 * number of cpus with a max of 8. This
 * can be overriden manually here.
 */
static int ixgbe_num_queues = 1;
SYSCTL_INT("hw.ixgbe.num_queues", &ixgbe_num_queues);
#endif

/*
** Number of TX descriptors per ring,
** setting higher than RX as this seems
** the better performing choice.
*/
static int ixgbe_txd = PERFORM_TXD;
SYSCTL_INT("hw.ixgbe.txd", &ixgbe_txd);

/* Number of RX descriptors per ring */
static int ixgbe_rxd = PERFORM_RXD;
SYSCTL_INT("hw.ixgbe.rxd", &ixgbe_rxd);

/*
** Defining this on will allow the use
** of unsupported SFP+ modules, note that
** doing so you are on your own :)
*/
static int allow_unsupported_sfp = false;
SYSCTL_INT("hw.ix.unsupported_sfp", &allow_unsupported_sfp);

/*
** HW RSC control: 
**  this feature only works with
**  IPv4, and only on 82599 and later.
**  Also this will cause IP forwarding to
**  fail and that can't be controlled by
**  the stack as LRO can. For all these
**  reasons I've deemed it best to leave
**  this off and not bother with a tuneable
**  interface, this would need to be compiled
**  to enable.
*/
static bool ixgbe_rsc_enable = FALSE;

/* Keep running tab on them for sanity check */
static int ixgbe_total_ports;

#ifdef IXGBE_FDIR
/*
** For Flow Director: this is the
** number of TX packets we sample
** for the filter pool, this means
** every 20th packet will be probed.
**
** This feature can be disabled by 
** setting this to 0.
*/
static int atr_sample_rate = 20;
/* 
** Flow Director actually 'steals'
** part of the packet buffer as its
** filter pool, this variable controls
** how much it uses:
**  0 = 64K, 1 = 128K, 2 = 256K
*/
static int fdir_pballoc = 1;
#endif

#ifdef DEV_NETMAP
/*
 * The #ifdef DEV_NETMAP / #endif blocks in this file are meant to
 * be a reference on how to implement netmap support in a driver.
 * Additional comments are in ixgbe_netmap.h .
 *
 * <dev/netmap/ixgbe_netmap.h> contains functions for netmap support
 * that extend the standard driver.
 */
#include <dev/netmap/ixgbe_netmap.h>
#endif /* DEV_NETMAP */

/*********************************************************************
 *  Device identification routine
 *
 *  ixgbe_probe determines if the driver should be loaded on
 *  adapter based on PCI vendor/device id of the adapter.
 *
 *  return 1 on success, 0 on failure
 *********************************************************************/

static int
ixgbe_probe(device_t dev, cfdata_t cf, void *aux)
{
	const struct pci_attach_args *pa = aux;

	return (ixgbe_lookup(pa) != NULL) ? 1 : 0;
}

static ixgbe_vendor_info_t *
ixgbe_lookup(const struct pci_attach_args *pa)
{
	pcireg_t subid;
	ixgbe_vendor_info_t *ent;

	INIT_DEBUGOUT("ixgbe_probe: begin");

	if (PCI_VENDOR(pa->pa_id) != IXGBE_INTEL_VENDOR_ID)
		return NULL;

	subid = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

	for (ent = ixgbe_vendor_info_array; ent->vendor_id != 0; ent++) {
		if (PCI_VENDOR(pa->pa_id) == ent->vendor_id &&
		    PCI_PRODUCT(pa->pa_id) == ent->device_id &&

		    (PCI_SUBSYS_VENDOR(subid) == ent->subvendor_id ||
		     ent->subvendor_id == 0) &&

		    (PCI_SUBSYS_ID(subid) == ent->subdevice_id ||
		     ent->subdevice_id == 0)) {
			++ixgbe_total_ports;
			return ent;
		}
	}
	return NULL;
}


static void
ixgbe_sysctl_attach(struct adapter *adapter)
{
	struct sysctllog **log;
	const struct sysctlnode *rnode, *cnode;
	device_t dev;

	dev = adapter->dev;
	log = &adapter->sysctllog;

	if ((rnode = ixgbe_sysctl_instance(adapter)) == NULL) {
		aprint_error_dev(dev, "could not create sysctl root\n");
		return;
	}

	if (sysctl_createv(log, 0, &rnode, &cnode,
	    CTLFLAG_READONLY, CTLTYPE_INT,
	    "num_rx_desc", SYSCTL_DESCR("Number of rx descriptors"),
	    NULL, 0, &adapter->num_rx_desc, 0, CTL_CREATE, CTL_EOL) != 0)
		aprint_error_dev(dev, "could not create sysctl\n");

	if (sysctl_createv(log, 0, &rnode, &cnode,
	    CTLFLAG_READONLY, CTLTYPE_INT,
	    "num_queues", SYSCTL_DESCR("Number of queues"),
	    NULL, 0, &adapter->num_queues, 0, CTL_CREATE, CTL_EOL) != 0)
		aprint_error_dev(dev, "could not create sysctl\n");

	if (sysctl_createv(log, 0, &rnode, &cnode,
	    CTLFLAG_READWRITE, CTLTYPE_INT,
	    "fc", SYSCTL_DESCR("Flow Control"),
	    ixgbe_set_flowcntl, 0, (void *)adapter, 0, CTL_CREATE, CTL_EOL) != 0)
		aprint_error_dev(dev, "could not create sysctl\n");

	/* XXX This is an *instance* sysctl controlling a *global* variable.
	 * XXX It's that way in the FreeBSD driver that this derives from.
	 */
	if (sysctl_createv(log, 0, &rnode, &cnode,
	    CTLFLAG_READWRITE, CTLTYPE_INT,
	    "enable_aim", SYSCTL_DESCR("Interrupt Moderation"),
	    NULL, 0, &ixgbe_enable_aim, 0, CTL_CREATE, CTL_EOL) != 0)
		aprint_error_dev(dev, "could not create sysctl\n");

	if (sysctl_createv(log, 0, &rnode, &cnode,
	    CTLFLAG_READWRITE, CTLTYPE_INT,
	    "advertise_speed", SYSCTL_DESCR("Link Speed"),
	    ixgbe_set_advertise, 0, (void *)adapter, 0, CTL_CREATE, CTL_EOL) != 0)
		aprint_error_dev(dev, "could not create sysctl\n");

	if (sysctl_createv(log, 0, &rnode, &cnode,
	    CTLFLAG_READWRITE, CTLTYPE_INT,
	    "ts", SYSCTL_DESCR("Thermal Test"),
	    ixgbe_set_thermal_test, 0, (void *)adapter, 0, CTL_CREATE, CTL_EOL) != 0)
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
ixgbe_attach(device_t parent, device_t dev, void *aux)
{
	struct adapter *adapter;
	struct ixgbe_hw *hw;
	int             error = -1;
	u16		csum;
	u32		ctrl_ext;
	ixgbe_vendor_info_t *ent;
	const struct pci_attach_args *pa = aux;

	INIT_DEBUGOUT("ixgbe_attach: begin");

	/* Allocate, clear, and link in our adapter structure */
	adapter = device_private(dev);
	adapter->dev = adapter->osdep.dev = dev;
	hw = &adapter->hw;
	adapter->osdep.pc = pa->pa_pc;
	adapter->osdep.tag = pa->pa_tag;
	adapter->osdep.dmat = pa->pa_dmat;
	adapter->osdep.attached = false;

	ent = ixgbe_lookup(pa);

	KASSERT(ent != NULL);

	aprint_normal(": %s, Version - %s\n",
	    ixgbe_strings[ent->index], ixgbe_driver_version);

	/* Core Lock Init*/
	IXGBE_CORE_LOCK_INIT(adapter, device_xname(dev));

	/* SYSCTL APIs */

	ixgbe_sysctl_attach(adapter);

	/* Set up the timer callout */
	callout_init(&adapter->timer, 0);

	/* Determine hardware revision */
	ixgbe_identify_hardware(adapter);

	/* Do base PCI setup - map BAR0 */
	if (ixgbe_allocate_pci_resources(adapter, pa)) {
		aprint_error_dev(dev, "Allocation of PCI resources failed\n");
		error = ENXIO;
		goto err_out;
	}

	/* Do descriptor calc and sanity checks */
	if (((ixgbe_txd * sizeof(union ixgbe_adv_tx_desc)) % DBA_ALIGN) != 0 ||
	    ixgbe_txd < MIN_TXD || ixgbe_txd > MAX_TXD) {
		aprint_error_dev(dev, "TXD config issue, using default!\n");
		adapter->num_tx_desc = DEFAULT_TXD;
	} else
		adapter->num_tx_desc = ixgbe_txd;

	/*
	** With many RX rings it is easy to exceed the
	** system mbuf allocation. Tuning nmbclusters
	** can alleviate this.
	*/
	if (nmbclusters > 0 ) {
		int s;
		s = (ixgbe_rxd * adapter->num_queues) * ixgbe_total_ports;
		if (s > nmbclusters) {
			aprint_error_dev(dev, "RX Descriptors exceed "
			    "system mbuf max, using default instead!\n");
			ixgbe_rxd = DEFAULT_RXD;
		}
	}

	if (((ixgbe_rxd * sizeof(union ixgbe_adv_rx_desc)) % DBA_ALIGN) != 0 ||
	    ixgbe_rxd < MIN_RXD || ixgbe_rxd > MAX_RXD) {
		aprint_error_dev(dev, "RXD config issue, using default!\n");
		adapter->num_rx_desc = DEFAULT_RXD;
	} else
		adapter->num_rx_desc = ixgbe_rxd;

	/* Allocate our TX/RX Queues */
	if (ixgbe_allocate_queues(adapter)) {
		error = ENOMEM;
		goto err_out;
	}

	/* Allocate multicast array memory. */
	adapter->mta = malloc(sizeof(u8) * IXGBE_ETH_LENGTH_OF_ADDRESS *
	    MAX_NUM_MULTICAST_ADDRESSES, M_DEVBUF, M_NOWAIT);
	if (adapter->mta == NULL) {
		aprint_error_dev(dev, "Cannot allocate multicast setup array\n");
		error = ENOMEM;
		goto err_late;
	}

	/* Initialize the shared code */
	hw->allow_unsupported_sfp = allow_unsupported_sfp;
	error = ixgbe_init_shared_code(hw);
	if (error == IXGBE_ERR_SFP_NOT_PRESENT) {
		/*
		** No optics in this port, set up
		** so the timer routine will probe 
		** for later insertion.
		*/
		adapter->sfp_probe = TRUE;
		error = 0;
	} else if (error == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		aprint_error_dev(dev,"Unsupported SFP+ module detected!\n");
		error = EIO;
		goto err_late;
	} else if (error) {
		aprint_error_dev(dev,"Unable to initialize the shared code\n");
		error = EIO;
		goto err_late;
	}

	/* Make sure we have a good EEPROM before we read from it */
	if (ixgbe_validate_eeprom_checksum(&adapter->hw, &csum) < 0) {
		aprint_error_dev(dev,"The EEPROM Checksum Is Not Valid\n");
		error = EIO;
		goto err_late;
	}

	error = ixgbe_init_hw(hw);
	switch (error) {
	case IXGBE_ERR_EEPROM_VERSION:
		aprint_error_dev(dev, "This device is a pre-production adapter/"
		    "LOM.  Please be aware there may be issues associated "
		    "with your hardware.\n If you are experiencing problems "
		    "please contact your Intel or hardware representative "
		    "who provided you with this hardware.\n");
		break;
	case IXGBE_ERR_SFP_NOT_SUPPORTED:
		aprint_error_dev(dev,"Unsupported SFP+ Module\n");
		error = EIO;
		aprint_error_dev(dev,"Hardware Initialization Failure\n");
		goto err_late;
	case IXGBE_ERR_SFP_NOT_PRESENT:
		device_printf(dev,"No SFP+ Module found\n");
		/* falls thru */
	default:
		break;
	}

	/* Detect and set physical type */
	ixgbe_setup_optics(adapter);

	error = -1;
	if ((adapter->msix > 1) && (ixgbe_enable_msix))
		error = ixgbe_allocate_msix(adapter, pa);
	if (error != 0)
		error = ixgbe_allocate_legacy(adapter, pa);
	if (error) 
		goto err_late;

	/* Setup OS specific network interface */
	if (ixgbe_setup_interface(dev, adapter) != 0)
		goto err_late;

	/* Initialize statistics */
	ixgbe_update_stats_counters(adapter);

	/*
	** Check PCIE slot type/speed/width
	*/
	ixgbe_get_slot_info(hw);

	/* Set an initial default flow control value */
	adapter->fc =  ixgbe_fc_full;

	/* let hardware know driver is loaded */
	ctrl_ext = IXGBE_READ_REG(hw, IXGBE_CTRL_EXT);
	ctrl_ext |= IXGBE_CTRL_EXT_DRV_LOAD;
	IXGBE_WRITE_REG(hw, IXGBE_CTRL_EXT, ctrl_ext);

	ixgbe_add_hw_stats(adapter);

#ifdef DEV_NETMAP
	ixgbe_netmap_attach(adapter);
#endif /* DEV_NETMAP */
	INIT_DEBUGOUT("ixgbe_attach: end");
	adapter->osdep.attached = true;
	return;
err_late:
	ixgbe_free_transmit_structures(adapter);
	ixgbe_free_receive_structures(adapter);
err_out:
	if (adapter->ifp != NULL)
		if_free(adapter->ifp);
	ixgbe_free_pci_resources(adapter);
	if (adapter->mta != NULL)
		free(adapter->mta, M_DEVBUF);
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
ixgbe_detach(device_t dev, int flags)
{
	struct adapter *adapter = device_private(dev);
	struct rx_ring *rxr = adapter->rx_rings;
	struct ixgbe_hw_stats *stats = &adapter->stats;
	struct ix_queue *que = adapter->queues;
	struct tx_ring *txr = adapter->tx_rings;
	u32	ctrl_ext;

	INIT_DEBUGOUT("ixgbe_detach: begin");
	if (adapter->osdep.attached == false)
		return 0;

#if NVLAN > 0
	/* Make sure VLANs are not using driver */
	if (!VLAN_ATTACHED(&adapter->osdep.ec))
		;	/* nothing to do: no VLANs */ 
	else if ((flags & (DETACH_SHUTDOWN|DETACH_FORCE)) != 0)
		vlan_ifdetach(adapter->ifp);
	else {
		aprint_error_dev(dev, "VLANs in use\n");
		return EBUSY;
	}
#endif

	IXGBE_CORE_LOCK(adapter);
	ixgbe_stop(adapter);
	IXGBE_CORE_UNLOCK(adapter);

	for (int i = 0; i < adapter->num_queues; i++, que++, txr++) {
#ifndef IXGBE_LEGACY_TX
		softint_disestablish(txr->txq_si);
#endif
		softint_disestablish(que->que_si);
	}

	/* Drain the Link queue */
	softint_disestablish(adapter->link_si);
	softint_disestablish(adapter->mod_si);
	softint_disestablish(adapter->msf_si);
#ifdef IXGBE_FDIR
	softint_disestablish(adapter->fdir_si);
#endif

	/* let hardware know driver is unloading */
	ctrl_ext = IXGBE_READ_REG(&adapter->hw, IXGBE_CTRL_EXT);
	ctrl_ext &= ~IXGBE_CTRL_EXT_DRV_LOAD;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_CTRL_EXT, ctrl_ext);

	ether_ifdetach(adapter->ifp);
	callout_halt(&adapter->timer, NULL);
#ifdef DEV_NETMAP
	netmap_detach(adapter->ifp);
#endif /* DEV_NETMAP */
	ixgbe_free_pci_resources(adapter);
#if 0	/* XXX the NetBSD port is probably missing something here */
	bus_generic_detach(dev);
#endif
	if_detach(adapter->ifp);

	sysctl_teardown(&adapter->sysctllog);
	evcnt_detach(&adapter->handleq);
	evcnt_detach(&adapter->req);
	evcnt_detach(&adapter->morerx);
	evcnt_detach(&adapter->moretx);
	evcnt_detach(&adapter->txloops);
	evcnt_detach(&adapter->efbig_tx_dma_setup);
	evcnt_detach(&adapter->m_defrag_failed);
	evcnt_detach(&adapter->efbig2_tx_dma_setup);
	evcnt_detach(&adapter->einval_tx_dma_setup);
	evcnt_detach(&adapter->other_tx_dma_setup);
	evcnt_detach(&adapter->eagain_tx_dma_setup);
	evcnt_detach(&adapter->enomem_tx_dma_setup);
	evcnt_detach(&adapter->watchdog_events);
	evcnt_detach(&adapter->tso_err);
	evcnt_detach(&adapter->link_irq);

	txr = adapter->tx_rings;
	for (int i = 0; i < adapter->num_queues; i++, rxr++, txr++) {
		evcnt_detach(&txr->no_desc_avail);
		evcnt_detach(&txr->total_packets);
		evcnt_detach(&txr->tso_tx);

		if (i < __arraycount(adapter->stats.mpc)) {
			evcnt_detach(&adapter->stats.mpc[i]);
		}
		if (i < __arraycount(adapter->stats.pxontxc)) {
			evcnt_detach(&adapter->stats.pxontxc[i]);
			evcnt_detach(&adapter->stats.pxonrxc[i]);
			evcnt_detach(&adapter->stats.pxofftxc[i]);
			evcnt_detach(&adapter->stats.pxoffrxc[i]);
			evcnt_detach(&adapter->stats.pxon2offc[i]);
		}
		if (i < __arraycount(adapter->stats.qprc)) {
			evcnt_detach(&adapter->stats.qprc[i]);
			evcnt_detach(&adapter->stats.qptc[i]);
			evcnt_detach(&adapter->stats.qbrc[i]);
			evcnt_detach(&adapter->stats.qbtc[i]);
			evcnt_detach(&adapter->stats.qprdc[i]);
		}

		evcnt_detach(&rxr->rx_packets);
		evcnt_detach(&rxr->rx_bytes);
		evcnt_detach(&rxr->rx_copies);
		evcnt_detach(&rxr->no_jmbuf);
		evcnt_detach(&rxr->rx_discarded);
		evcnt_detach(&rxr->rx_irq);
	}
	evcnt_detach(&stats->ipcs);
	evcnt_detach(&stats->l4cs);
	evcnt_detach(&stats->ipcs_bad);
	evcnt_detach(&stats->l4cs_bad);
	evcnt_detach(&stats->intzero);
	evcnt_detach(&stats->legint);
	evcnt_detach(&stats->crcerrs);
	evcnt_detach(&stats->illerrc);
	evcnt_detach(&stats->errbc);
	evcnt_detach(&stats->mspdc);
	evcnt_detach(&stats->mlfc);
	evcnt_detach(&stats->mrfc);
	evcnt_detach(&stats->rlec);
	evcnt_detach(&stats->lxontxc);
	evcnt_detach(&stats->lxonrxc);
	evcnt_detach(&stats->lxofftxc);
	evcnt_detach(&stats->lxoffrxc);

	/* Packet Reception Stats */
	evcnt_detach(&stats->tor);
	evcnt_detach(&stats->gorc);
	evcnt_detach(&stats->tpr);
	evcnt_detach(&stats->gprc);
	evcnt_detach(&stats->mprc);
	evcnt_detach(&stats->bprc);
	evcnt_detach(&stats->prc64);
	evcnt_detach(&stats->prc127);
	evcnt_detach(&stats->prc255);
	evcnt_detach(&stats->prc511);
	evcnt_detach(&stats->prc1023);
	evcnt_detach(&stats->prc1522);
	evcnt_detach(&stats->ruc);
	evcnt_detach(&stats->rfc);
	evcnt_detach(&stats->roc);
	evcnt_detach(&stats->rjc);
	evcnt_detach(&stats->mngprc);
	evcnt_detach(&stats->xec);

	/* Packet Transmission Stats */
	evcnt_detach(&stats->gotc);
	evcnt_detach(&stats->tpt);
	evcnt_detach(&stats->gptc);
	evcnt_detach(&stats->bptc);
	evcnt_detach(&stats->mptc);
	evcnt_detach(&stats->mngptc);
	evcnt_detach(&stats->ptc64);
	evcnt_detach(&stats->ptc127);
	evcnt_detach(&stats->ptc255);
	evcnt_detach(&stats->ptc511);
	evcnt_detach(&stats->ptc1023);
	evcnt_detach(&stats->ptc1522);

	ixgbe_free_transmit_structures(adapter);
	ixgbe_free_receive_structures(adapter);
	free(adapter->mta, M_DEVBUF);

	IXGBE_CORE_LOCK_DESTROY(adapter);
	return (0);
}

/*********************************************************************
 *
 *  Shutdown entry point
 *
 **********************************************************************/

#if 0 /* XXX NetBSD ought to register something like this through pmf(9) */
static int
ixgbe_shutdown(device_t dev)
{
	struct adapter *adapter = device_private(dev);
	IXGBE_CORE_LOCK(adapter);
	ixgbe_stop(adapter);
	IXGBE_CORE_UNLOCK(adapter);
	return (0);
}
#endif


#ifdef IXGBE_LEGACY_TX
/*********************************************************************
 *  Transmit entry point
 *
 *  ixgbe_start is called by the stack to initiate a transmit.
 *  The driver will remain in this routine as long as there are
 *  packets to transmit and transmit resources are available.
 *  In case resources are not available stack is notified and
 *  the packet is requeued.
 **********************************************************************/

static void
ixgbe_start_locked(struct tx_ring *txr, struct ifnet * ifp)
{
	int rc;
	struct mbuf    *m_head;
	struct adapter *adapter = txr->adapter;

	IXGBE_TX_LOCK_ASSERT(txr);

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;
	if (!adapter->link_active)
		return;

	while (!IFQ_IS_EMPTY(&ifp->if_snd)) {
		if (txr->tx_avail <= IXGBE_QUEUE_MIN_FREE)
			break;

		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if ((rc = ixgbe_xmit(txr, m_head)) == EAGAIN) {
			break;
		}
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		if (rc == EFBIG) {
			struct mbuf *mtmp;

			if ((mtmp = m_defrag(m_head, M_NOWAIT)) != NULL) {
				m_head = mtmp;
				rc = ixgbe_xmit(txr, m_head);
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
		getmicrotime(&txr->watchdog_time);
		txr->queue_status = IXGBE_QUEUE_WORKING;

	}
	return;
}

/*
 * Legacy TX start - called by the stack, this
 * always uses the first tx ring, and should
 * not be used with multiqueue tx enabled.
 */
static void
ixgbe_start(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;
	struct tx_ring	*txr = adapter->tx_rings;

	if (ifp->if_flags & IFF_RUNNING) {
		IXGBE_TX_LOCK(txr);
		ixgbe_start_locked(txr, ifp);
		IXGBE_TX_UNLOCK(txr);
	}
	return;
}

#else /* ! IXGBE_LEGACY_TX */

/*
** Multiqueue Transmit driver
**
*/
static int
ixgbe_mq_start(struct ifnet *ifp, struct mbuf *m)
{
	struct adapter	*adapter = ifp->if_softc;
	struct ix_queue	*que;
	struct tx_ring	*txr;
	int 		i, err = 0;
#ifdef	RSS
	uint32_t bucket_id;
#endif

	/* Which queue to use */
	/*
	 * When doing RSS, map it to the same outbound queue
	 * as the incoming flow would be mapped to.
	 *
	 * If everything is setup correctly, it should be the
	 * same bucket that the current CPU we're on is.
	 */
	if (M_HASHTYPE_GET(m) != M_HASHTYPE_NONE) {
#ifdef	RSS
		if (rss_hash2bucket(m->m_pkthdr.flowid,
		    M_HASHTYPE_GET(m), &bucket_id) == 0) {
			/* XXX TODO: spit out something if bucket_id > num_queues? */
			i = bucket_id % adapter->num_queues;
		} else {
#endif
			i = m->m_pkthdr.flowid % adapter->num_queues;
#ifdef	RSS
		}
#endif
	} else {
		i = curcpu % adapter->num_queues;
	}

	txr = &adapter->tx_rings[i];
	que = &adapter->queues[i];

	err = drbr_enqueue(ifp, txr->br, m);
	if (err)
		return (err);
	if (IXGBE_TX_TRYLOCK(txr)) {
		ixgbe_mq_start_locked(ifp, txr);
		IXGBE_TX_UNLOCK(txr);
	} else
		softint_schedule(txr->txq_si);

	return (0);
}

static int
ixgbe_mq_start_locked(struct ifnet *ifp, struct tx_ring *txr)
{
	struct adapter  *adapter = txr->adapter;
	struct mbuf     *next;
	int             enqueued = 0, err = 0;

	if (((ifp->if_flags & IFF_RUNNING) == 0) ||
	    adapter->link_active == 0)
		return (ENETDOWN);

	/* Process the queue */
#if __FreeBSD_version < 901504
	next = drbr_dequeue(ifp, txr->br);
	while (next != NULL) {
		if ((err = ixgbe_xmit(txr, &next)) != 0) {
			if (next != NULL)
				err = drbr_enqueue(ifp, txr->br, next);
#else
	while ((next = drbr_peek(ifp, txr->br)) != NULL) {
		if ((err = ixgbe_xmit(txr, &next)) != 0) {
			if (next == NULL) {
				drbr_advance(ifp, txr->br);
			} else {
				drbr_putback(ifp, txr->br, next);
			}
#endif
			break;
		}
#if __FreeBSD_version >= 901504
		drbr_advance(ifp, txr->br);
#endif
		enqueued++;
		/* Send a copy of the frame to the BPF listener */
		bpf_mtap(ifp, next);
		if ((ifp->if_flags & IFF_RUNNING) == 0)
			break;
#if __FreeBSD_version < 901504
		next = drbr_dequeue(ifp, txr->br);
#endif
	}

	if (enqueued > 0) {
		/* Set watchdog on */
		txr->queue_status = IXGBE_QUEUE_WORKING;
		getmicrotime(&txr->watchdog_time);
	}

	if (txr->tx_avail < IXGBE_TX_CLEANUP_THRESHOLD)
		ixgbe_txeof(txr);

	return (err);
}

/*
 * Called from a taskqueue to drain queued transmit packets.
 */
static void
ixgbe_deferred_mq_start(void *arg, int pending)
{
	struct tx_ring *txr = arg;
	struct adapter *adapter = txr->adapter;
	struct ifnet *ifp = adapter->ifp;

	IXGBE_TX_LOCK(txr);
	if (!drbr_empty(ifp, txr->br))
		ixgbe_mq_start_locked(ifp, txr);
	IXGBE_TX_UNLOCK(txr);
}

/*
** Flush all ring buffers
*/
static void
ixgbe_qflush(struct ifnet *ifp)
{
	struct adapter	*adapter = ifp->if_softc;
	struct tx_ring	*txr = adapter->tx_rings;
	struct mbuf	*m;

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		IXGBE_TX_LOCK(txr);
		while ((m = buf_ring_dequeue_sc(txr->br)) != NULL)
			m_freem(m);
		IXGBE_TX_UNLOCK(txr);
	}
	if_qflush(ifp);
}
#endif /* IXGBE_LEGACY_TX */

static int
ixgbe_ifflags_cb(struct ethercom *ec)
{
	struct ifnet *ifp = &ec->ec_if;
	struct adapter *adapter = ifp->if_softc;
	int change = ifp->if_flags ^ adapter->if_flags, rc = 0;

	IXGBE_CORE_LOCK(adapter);

	if (change != 0)
		adapter->if_flags = ifp->if_flags;

	if ((change & ~(IFF_CANTCHANGE|IFF_DEBUG)) != 0)
		rc = ENETRESET;
	else if ((change & (IFF_PROMISC | IFF_ALLMULTI)) != 0)
		ixgbe_set_promisc(adapter);

	/* Set up VLAN support and filter */
	ixgbe_setup_vlan_hw_support(adapter);

	IXGBE_CORE_UNLOCK(adapter);

	return rc;
}

/*********************************************************************
 *  Ioctl entry point
 *
 *  ixgbe_ioctl is called when the user wants to configure the
 *  interface.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static int
ixgbe_ioctl(struct ifnet * ifp, u_long command, void *data)
{
	struct adapter	*adapter = ifp->if_softc;
	struct ixgbe_hw *hw = &adapter->hw;
	struct ifcapreq *ifcr = data;
	struct ifreq	*ifr = data;
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
	case SIOCGI2C:
	{
		struct ixgbe_i2c_req	i2c;
		IOCTL_DEBUGOUT("ioctl: SIOCGI2C (Get I2C Data)");
		error = copyin(ifr->ifr_data, &i2c, sizeof(i2c));
		if (error != 0)
			break;
		if (i2c.dev_addr != 0xA0 && i2c.dev_addr != 0xA2) {
			error = EINVAL;
			break;
		}
		if (i2c.len > sizeof(i2c.data)) {
			error = EINVAL;
			break;
		}

		hw->phy.ops.read_i2c_byte(hw, i2c.offset,
		    i2c.dev_addr, i2c.data);
		error = copyout(&i2c, ifr->ifr_data, sizeof(i2c));
		break;
	}
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
			IXGBE_CORE_LOCK(adapter);
			ixgbe_init_locked(adapter);
			IXGBE_CORE_UNLOCK(adapter);
		} else if (command == SIOCADDMULTI || command == SIOCDELMULTI) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			IXGBE_CORE_LOCK(adapter);
			ixgbe_disable_intr(adapter);
			ixgbe_set_multi(adapter);
			ixgbe_enable_intr(adapter);
			IXGBE_CORE_UNLOCK(adapter);
		}
		return 0;
	}

	return error;
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
ixgbe_init_locked(struct adapter *adapter)
{
	struct ifnet   *ifp = adapter->ifp;
	device_t 	dev = adapter->dev;
	struct ixgbe_hw *hw = &adapter->hw;
	u32		k, txdctl, mhadd, gpie;
	u32		rxdctl, rxctrl;

	/* XXX check IFF_UP and IFF_RUNNING, power-saving state! */

	KASSERT(mutex_owned(&adapter->core_mtx));
	INIT_DEBUGOUT("ixgbe_init_locked: begin");
	hw->adapter_stopped = FALSE;
	ixgbe_stop_adapter(hw);
        callout_stop(&adapter->timer);

	/* XXX I moved this here from the SIOCSIFMTU case in ixgbe_ioctl(). */
	adapter->max_frame_size =
		ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;

        /* reprogram the RAR[0] in case user changed it. */
        ixgbe_set_rar(hw, 0, adapter->hw.mac.addr, 0, IXGBE_RAH_AV);

	/* Get the latest mac address, User can use a LAA */
	memcpy(hw->mac.addr, CLLADDR(adapter->ifp->if_sadl),
	    IXGBE_ETH_LENGTH_OF_ADDRESS);
	ixgbe_set_rar(hw, 0, hw->mac.addr, 0, 1);
	hw->addr_ctrl.rar_used_count = 1;

	/* Prepare transmit descriptors and buffers */
	if (ixgbe_setup_transmit_structures(adapter)) {
		device_printf(dev,"Could not setup transmit structures\n");
		ixgbe_stop(adapter);
		return;
	}

	ixgbe_init_hw(hw);
	ixgbe_initialize_transmit_units(adapter);

	/* Setup Multicast table */
	ixgbe_set_multi(adapter);

	/*
	** Determine the correct mbuf pool
	** for doing jumbo frames
	*/
	if (adapter->max_frame_size <= 2048)
		adapter->rx_mbuf_sz = MCLBYTES;
	else if (adapter->max_frame_size <= 4096)
		adapter->rx_mbuf_sz = MJUMPAGESIZE;
	else if (adapter->max_frame_size <= 9216)
		adapter->rx_mbuf_sz = MJUM9BYTES;
	else
		adapter->rx_mbuf_sz = MJUM16BYTES;

	/* Prepare receive descriptors and buffers */
	if (ixgbe_setup_receive_structures(adapter)) {
		device_printf(dev,"Could not setup receive structures\n");
		ixgbe_stop(adapter);
		return;
	}

	/* Configure RX settings */
	ixgbe_initialize_receive_units(adapter);

	gpie = IXGBE_READ_REG(&adapter->hw, IXGBE_GPIE);

	/* Enable Fan Failure Interrupt */
	gpie |= IXGBE_SDP1_GPIEN;

	/* Add for Module detection */
	if (hw->mac.type == ixgbe_mac_82599EB)
		gpie |= IXGBE_SDP2_GPIEN;

	/* Thermal Failure Detection */
	if (hw->mac.type == ixgbe_mac_X540)
		gpie |= IXGBE_SDP0_GPIEN;

	if (adapter->msix > 1) {
		/* Enable Enhanced MSIX mode */
		gpie |= IXGBE_GPIE_MSIX_MODE;
		gpie |= IXGBE_GPIE_EIAME | IXGBE_GPIE_PBA_SUPPORT |
		    IXGBE_GPIE_OCD;
	}
	IXGBE_WRITE_REG(hw, IXGBE_GPIE, gpie);

	/* Set MTU size */
	if (ifp->if_mtu > ETHERMTU) {
		mhadd = IXGBE_READ_REG(hw, IXGBE_MHADD);
		mhadd &= ~IXGBE_MHADD_MFS_MASK;
		mhadd |= adapter->max_frame_size << IXGBE_MHADD_MFS_SHIFT;
		IXGBE_WRITE_REG(hw, IXGBE_MHADD, mhadd);
	}
	
	/* Now enable all the queues */

	for (int i = 0; i < adapter->num_queues; i++) {
		txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(i));
		txdctl |= IXGBE_TXDCTL_ENABLE;
		/* Set WTHRESH to 8, burst writeback */
		txdctl |= (8 << 16);
		/*
		 * When the internal queue falls below PTHRESH (32),
		 * start prefetching as long as there are at least
		 * HTHRESH (1) buffers ready. The values are taken
		 * from the Intel linux driver 3.8.21.
		 * Prefetching enables tx line rate even with 1 queue.
		 */
		txdctl |= (32 << 0) | (1 << 8);
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(i), txdctl);
	}

	for (int i = 0; i < adapter->num_queues; i++) {
		rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(i));
		if (hw->mac.type == ixgbe_mac_82598EB) {
			/*
			** PTHRESH = 21
			** HTHRESH = 4
			** WTHRESH = 8
			*/
			rxdctl &= ~0x3FFFFF;
			rxdctl |= 0x080420;
		}
		rxdctl |= IXGBE_RXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(i), rxdctl);
		/* XXX I don't trust this loop, and I don't trust the
		 * XXX memory barrier.  What is this meant to do? --dyoung
		 */
		for (k = 0; k < 10; k++) {
			if (IXGBE_READ_REG(hw, IXGBE_RXDCTL(i)) &
			    IXGBE_RXDCTL_ENABLE)
				break;
			else
				msec_delay(1);
		}
		wmb();
#ifdef DEV_NETMAP
		/*
		 * In netmap mode, we must preserve the buffers made
		 * available to userspace before the if_init()
		 * (this is true by default on the TX side, because
		 * init makes all buffers available to userspace).
		 *
		 * netmap_reset() and the device specific routines
		 * (e.g. ixgbe_setup_receive_rings()) map these
		 * buffers at the end of the NIC ring, so here we
		 * must set the RDT (tail) register to make sure
		 * they are not overwritten.
		 *
		 * In this driver the NIC ring starts at RDH = 0,
		 * RDT points to the last slot available for reception (?),
		 * so RDT = num_rx_desc - 1 means the whole ring is available.
		 */
		if (ifp->if_capenable & IFCAP_NETMAP) {
			struct netmap_adapter *na = NA(adapter->ifp);
			struct netmap_kring *kring = &na->rx_rings[i];
			int t = na->num_rx_desc - 1 - nm_kr_rxspace(kring);

			IXGBE_WRITE_REG(hw, IXGBE_RDT(i), t);
		} else
#endif /* DEV_NETMAP */
		IXGBE_WRITE_REG(hw, IXGBE_RDT(i), adapter->num_rx_desc - 1);
	}

	/* Enable Receive engine */
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	if (hw->mac.type == ixgbe_mac_82598EB)
		rxctrl |= IXGBE_RXCTRL_DMBYPS;
	rxctrl |= IXGBE_RXCTRL_RXEN;
	ixgbe_enable_rx_dma(hw, rxctrl);

	callout_reset(&adapter->timer, hz, ixgbe_local_timer, adapter);

	/* Set up MSI/X routing */
	if (ixgbe_enable_msix)  {
		ixgbe_configure_ivars(adapter);
		/* Set up auto-mask */
		if (hw->mac.type == ixgbe_mac_82598EB)
			IXGBE_WRITE_REG(hw, IXGBE_EIAM, IXGBE_EICS_RTX_QUEUE);
		else {
			IXGBE_WRITE_REG(hw, IXGBE_EIAM_EX(0), 0xFFFFFFFF);
			IXGBE_WRITE_REG(hw, IXGBE_EIAM_EX(1), 0xFFFFFFFF);
		}
	} else {  /* Simple settings for Legacy/MSI */
                ixgbe_set_ivar(adapter, 0, 0, 0);
                ixgbe_set_ivar(adapter, 0, 0, 1);
		IXGBE_WRITE_REG(hw, IXGBE_EIAM, IXGBE_EICS_RTX_QUEUE);
	}

#ifdef IXGBE_FDIR
	/* Init Flow director */
	if (hw->mac.type != ixgbe_mac_82598EB) {
		u32 hdrm = 32 << fdir_pballoc;

		hw->mac.ops.setup_rxpba(hw, 0, hdrm, PBA_STRATEGY_EQUAL);
		ixgbe_init_fdir_signature_82599(&adapter->hw, fdir_pballoc);
	}
#endif

	/*
	** Check on any SFP devices that
	** need to be kick-started
	*/
	if (hw->phy.type == ixgbe_phy_none) {
		int err = hw->phy.ops.identify(hw);
		if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
                	device_printf(dev,
			    "Unsupported SFP+ module type was detected.\n");
			return;
        	}
	}

	/* Set moderation on the Link interrupt */
	IXGBE_WRITE_REG(hw, IXGBE_EITR(adapter->linkvec), IXGBE_LINK_ITR);

	/* Config/Enable Link */
	ixgbe_config_link(adapter);

	/* Hardware Packet Buffer & Flow Control setup */
	{
		u32 rxpb, frame, size, tmp;

		frame = adapter->max_frame_size;

		/* Calculate High Water */
		if (hw->mac.type == ixgbe_mac_X540)
			tmp = IXGBE_DV_X540(frame, frame);
		else
			tmp = IXGBE_DV(frame, frame);
		size = IXGBE_BT2KB(tmp);
		rxpb = IXGBE_READ_REG(hw, IXGBE_RXPBSIZE(0)) >> 10;
		hw->fc.high_water[0] = rxpb - size;

		/* Now calculate Low Water */
		if (hw->mac.type == ixgbe_mac_X540)
			tmp = IXGBE_LOW_DV_X540(frame);
		else
			tmp = IXGBE_LOW_DV(frame);
		hw->fc.low_water[0] = IXGBE_BT2KB(tmp);
		
		hw->fc.requested_mode = adapter->fc;
		hw->fc.pause_time = IXGBE_FC_PAUSE;
		hw->fc.send_xon = TRUE;
	}
	/* Initialize the FC settings */
	ixgbe_start_hw(hw);

	/* Set up VLAN support and filter */
	ixgbe_setup_vlan_hw_support(adapter);

	/* And now turn on interrupts */
	ixgbe_enable_intr(adapter);

	/* Now inform the stack we're ready */
	ifp->if_flags |= IFF_RUNNING;

	return;
}

static int
ixgbe_init(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;

	IXGBE_CORE_LOCK(adapter);
	ixgbe_init_locked(adapter);
	IXGBE_CORE_UNLOCK(adapter);
	return 0;	/* XXX ixgbe_init_locked cannot fail?  really? */
}


/*
**
** MSIX Interrupt Handlers and Tasklets
**
*/

static inline void
ixgbe_enable_queue(struct adapter *adapter, u32 vector)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u64	queue = (u64)(1ULL << vector);
	u32	mask;

	if (hw->mac.type == ixgbe_mac_82598EB) {
                mask = (IXGBE_EIMS_RTX_QUEUE & queue);
                IXGBE_WRITE_REG(hw, IXGBE_EIMS, mask);
	} else {
                mask = (queue & 0xFFFFFFFF);
                if (mask)
                        IXGBE_WRITE_REG(hw, IXGBE_EIMS_EX(0), mask);
                mask = (queue >> 32);
                if (mask)
                        IXGBE_WRITE_REG(hw, IXGBE_EIMS_EX(1), mask);
	}
}

__unused static inline void
ixgbe_disable_queue(struct adapter *adapter, u32 vector)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u64	queue = (u64)(1ULL << vector);
	u32	mask;

	if (hw->mac.type == ixgbe_mac_82598EB) {
                mask = (IXGBE_EIMS_RTX_QUEUE & queue);
                IXGBE_WRITE_REG(hw, IXGBE_EIMC, mask);
	} else {
                mask = (queue & 0xFFFFFFFF);
                if (mask)
                        IXGBE_WRITE_REG(hw, IXGBE_EIMC_EX(0), mask);
                mask = (queue >> 32);
                if (mask)
                        IXGBE_WRITE_REG(hw, IXGBE_EIMC_EX(1), mask);
	}
}

static void
ixgbe_handle_que(void *context)
{
	struct ix_queue *que = context;
	struct adapter  *adapter = que->adapter;
	struct tx_ring  *txr = que->txr;
	struct ifnet    *ifp = adapter->ifp;

	adapter->handleq.ev_count++;

	if (ifp->if_flags & IFF_RUNNING) {
		ixgbe_rxeof(que);
		IXGBE_TX_LOCK(txr);
		ixgbe_txeof(txr);
#ifndef IXGBE_LEGACY_TX
		if (!drbr_empty(ifp, txr->br))
			ixgbe_mq_start_locked(ifp, txr);
#else
		if (!IFQ_IS_EMPTY(&ifp->if_snd))
			ixgbe_start_locked(txr, ifp);
#endif
		IXGBE_TX_UNLOCK(txr);
	}

	/* Reenable this interrupt */
	if (que->res != NULL)
		ixgbe_enable_queue(adapter, que->msix);
	else
		ixgbe_enable_intr(adapter);
	return;
}


/*********************************************************************
 *
 *  Legacy Interrupt Service routine
 *
 **********************************************************************/

static int
ixgbe_legacy_irq(void *arg)
{
	struct ix_queue *que = arg;
	struct adapter	*adapter = que->adapter;
	struct ixgbe_hw	*hw = &adapter->hw;
	struct ifnet    *ifp = adapter->ifp;
	struct 		tx_ring *txr = adapter->tx_rings;
	bool		more = false;
	u32       	reg_eicr;


	reg_eicr = IXGBE_READ_REG(hw, IXGBE_EICR);

	adapter->stats.legint.ev_count++;
	++que->irqs;
	if (reg_eicr == 0) {
		adapter->stats.intzero.ev_count++;
		if ((ifp->if_flags & IFF_UP) != 0)
			ixgbe_enable_intr(adapter);
		return 0;
	}

	if ((ifp->if_flags & IFF_RUNNING) != 0) {
		more = ixgbe_rxeof(que);

		IXGBE_TX_LOCK(txr);
		ixgbe_txeof(txr);
#ifdef IXGBE_LEGACY_TX
		if (!IFQ_IS_EMPTY(&ifp->if_snd))
			ixgbe_start_locked(txr, ifp);
#else
		if (!drbr_empty(ifp, txr->br))
			ixgbe_mq_start_locked(ifp, txr);
#endif
		IXGBE_TX_UNLOCK(txr);
	}

	/* Check for fan failure */
	if ((hw->phy.media_type == ixgbe_media_type_copper) &&
	    (reg_eicr & IXGBE_EICR_GPI_SDP1)) {
                device_printf(adapter->dev, "\nCRITICAL: FAN FAILURE!! "
		    "REPLACE IMMEDIATELY!!\n");
		IXGBE_WRITE_REG(hw, IXGBE_EIMS, IXGBE_EICR_GPI_SDP1);
	}

	/* Link status change */
	if (reg_eicr & IXGBE_EICR_LSC)
		softint_schedule(adapter->link_si);

	if (more)
#ifndef IXGBE_LEGACY_TX
		softint_schedule(txr->txq_si);
#else
		softint_schedule(que->que_si);
#endif
	else
		ixgbe_enable_intr(adapter);
	return 1;
}


#if defined(NETBSD_MSI_OR_MSIX)
/*********************************************************************
 *
 *  MSIX Queue Interrupt Service routine
 *
 **********************************************************************/
static int
ixgbe_msix_que(void *arg)
{
	struct ix_queue	*que = arg;
	struct adapter  *adapter = que->adapter;
	struct ifnet    *ifp = adapter->ifp;
	struct tx_ring	*txr = que->txr;
	struct rx_ring	*rxr = que->rxr;
	bool		more;
	u32		newitr = 0;

	/* Protect against spurious interrupts */
	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return 0;

	ixgbe_disable_queue(adapter, que->msix);
	++que->irqs;

	more = ixgbe_rxeof(que);

	IXGBE_TX_LOCK(txr);
	ixgbe_txeof(txr);
#ifdef IXGBE_LEGACY_TX
	if (!IFQ_IS_EMPTY(&adapter->ifp->if_snd))
		ixgbe_start_locked(txr, ifp);
#else
	if (!drbr_empty(ifp, txr->br))
		ixgbe_mq_start_locked(ifp, txr);
#endif
	IXGBE_TX_UNLOCK(txr);

	/* Do AIM now? */

	if (ixgbe_enable_aim == FALSE)
		goto no_calc;
	/*
	** Do Adaptive Interrupt Moderation:
        **  - Write out last calculated setting
	**  - Calculate based on average size over
	**    the last interval.
	*/
        if (que->eitr_setting)
                IXGBE_WRITE_REG(&adapter->hw,
                    IXGBE_EITR(que->msix), que->eitr_setting);
 
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

        if (adapter->hw.mac.type == ixgbe_mac_82598EB)
                newitr |= newitr << 16;
        else
                newitr |= IXGBE_EITR_CNT_WDIS;
                 
        /* save for next interrupt */
        que->eitr_setting = newitr;

        /* Reset state */
        txr->bytes = 0;
        txr->packets = 0;
        rxr->bytes = 0;
        rxr->packets = 0;

no_calc:
	if (more)
		softint_schedule(que->que_si);
	else
		ixgbe_enable_queue(adapter, que->msix);
	return 1;
}


static int
ixgbe_msix_link(void *arg)
{
	struct adapter	*adapter = arg;
	struct ixgbe_hw *hw = &adapter->hw;
	u32		reg_eicr;

	++adapter->link_irq.ev_count;

	/* First get the cause */
	reg_eicr = IXGBE_READ_REG(hw, IXGBE_EICS);
	/* Be sure the queue bits are not cleared */
	reg_eicr &= ~IXGBE_EICR_RTX_QUEUE;
	/* Clear interrupt with write */
	IXGBE_WRITE_REG(hw, IXGBE_EICR, reg_eicr);

	/* Link status change */
	if (reg_eicr & IXGBE_EICR_LSC)
		softint_schedule(adapter->link_si);

	if (adapter->hw.mac.type != ixgbe_mac_82598EB) {
#ifdef IXGBE_FDIR
		if (reg_eicr & IXGBE_EICR_FLOW_DIR) {
			/* This is probably overkill :) */
			if (!atomic_cmpset_int(&adapter->fdir_reinit, 0, 1))
				return 1;
                	/* Disable the interrupt */
			IXGBE_WRITE_REG(hw, IXGBE_EIMC, IXGBE_EICR_FLOW_DIR);
			softint_schedule(adapter->fdir_si);
		} else
#endif
		if (reg_eicr & IXGBE_EICR_ECC) {
                	device_printf(adapter->dev, "\nCRITICAL: ECC ERROR!! "
			    "Please Reboot!!\n");
			IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_ECC);
		} else

		if (reg_eicr & IXGBE_EICR_GPI_SDP1) {
                	/* Clear the interrupt */
                	IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_GPI_SDP1);
			softint_schedule(adapter->msf_si);
        	} else if (reg_eicr & IXGBE_EICR_GPI_SDP2) {
                	/* Clear the interrupt */
                	IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_GPI_SDP2);
			softint_schedule(adapter->mod_si);
		}
        } 

	/* Check for fan failure */
	if ((hw->device_id == IXGBE_DEV_ID_82598AT) &&
	    (reg_eicr & IXGBE_EICR_GPI_SDP1)) {
                device_printf(adapter->dev, "\nCRITICAL: FAN FAILURE!! "
		    "REPLACE IMMEDIATELY!!\n");
		IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_GPI_SDP1);
	}

	/* Check for over temp condition */
	if ((hw->mac.type == ixgbe_mac_X540) &&
	    (reg_eicr & IXGBE_EICR_TS)) {
		device_printf(adapter->dev, "\nCRITICAL: OVER TEMP!! "
		    "PHY IS SHUT DOWN!!\n");
		device_printf(adapter->dev, "System shutdown required\n");
		IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_TS);
	}

	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS, IXGBE_EIMS_OTHER);
	return 1;
}
#endif

/*********************************************************************
 *
 *  Media Ioctl callback
 *
 *  This routine is called whenever the user queries the status of
 *  the interface using ifconfig.
 *
 **********************************************************************/
static void
ixgbe_media_status(struct ifnet * ifp, struct ifmediareq * ifmr)
{
	struct adapter *adapter = ifp->if_softc;
	struct ixgbe_hw *hw = &adapter->hw;

	INIT_DEBUGOUT("ixgbe_media_status: begin");
	IXGBE_CORE_LOCK(adapter);
	ixgbe_update_link_status(adapter);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!adapter->link_active) {
		IXGBE_CORE_UNLOCK(adapter);
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;

	/*
	 * Not all NIC are 1000baseSX as an example X540T.
	 * We must set properly the media based on NIC model.
	 */
	switch (hw->device_id) {
	case IXGBE_DEV_ID_X540T:
		if (adapter->link_speed == IXGBE_LINK_SPEED_100_FULL)
			ifmr->ifm_active |= IFM_100_TX | IFM_FDX;
		else if (adapter->link_speed == IXGBE_LINK_SPEED_1GB_FULL)
			ifmr->ifm_active |= IFM_1000_T | IFM_FDX;
		else if (adapter->link_speed == IXGBE_LINK_SPEED_10GB_FULL)
			ifmr->ifm_active |= adapter->optics | IFM_FDX;
		break;
	default:
		if (adapter->link_speed == IXGBE_LINK_SPEED_100_FULL)
			ifmr->ifm_active |= IFM_100_TX | IFM_FDX;
		else if (adapter->link_speed == IXGBE_LINK_SPEED_1GB_FULL)
			ifmr->ifm_active |= IFM_1000_SX | IFM_FDX;
		else if (adapter->link_speed == IXGBE_LINK_SPEED_10GB_FULL)
			ifmr->ifm_active |= adapter->optics | IFM_FDX;
		break;
	}

	IXGBE_CORE_UNLOCK(adapter);

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
ixgbe_media_change(struct ifnet * ifp)
{
	struct adapter *adapter = ifp->if_softc;
	struct ifmedia *ifm = &adapter->media;

	INIT_DEBUGOUT("ixgbe_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

        switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_10G_T:
	case IFM_AUTO:
		adapter->hw.phy.autoneg_advertised =
		    IXGBE_LINK_SPEED_100_FULL |
		    IXGBE_LINK_SPEED_1GB_FULL |
		    IXGBE_LINK_SPEED_10GB_FULL;
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
ixgbe_xmit(struct tx_ring *txr, struct mbuf *m_head)
{
	struct m_tag *mtag;
	struct adapter  *adapter = txr->adapter;
	struct ethercom *ec = &adapter->osdep.ec;
	u32		olinfo_status = 0, cmd_type_len;
	int             i, j, error;
	int		first;
	bus_dmamap_t	map;
	struct ixgbe_tx_buf *txbuf;
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

	if (__predict_false(error)) {

		switch (error) {
		case EAGAIN:
			adapter->eagain_tx_dma_setup.ev_count++;
			return EAGAIN;
		case ENOMEM:
			adapter->enomem_tx_dma_setup.ev_count++;
			return EAGAIN;
		case EFBIG:
			/*
			 * XXX Try it again?
			 * do m_defrag() and retry bus_dmamap_load_mbuf().
			 */
			adapter->efbig_tx_dma_setup.ev_count++;
			return error;
		case EINVAL:
			adapter->einval_tx_dma_setup.ev_count++;
			return error;
		default:
			adapter->other_tx_dma_setup.ev_count++;
			return error;
		}
	}

	/* Make certain there are enough descriptors */
	if (map->dm_nsegs > txr->tx_avail - 2) {
		txr->no_desc_avail.ev_count++;
		ixgbe_dmamap_unload(txr->txtag, txbuf->map);
		return EAGAIN;
	}

	/*
	** Set up the appropriate offload context
	** this will consume the first descriptor
	*/
	error = ixgbe_tx_ctx_setup(txr, m_head, &cmd_type_len, &olinfo_status);
	if (__predict_false(error)) {
		return (error);
	}

#ifdef IXGBE_FDIR
	/* Do the flow director magic */
	if ((txr->atr_sample) && (!adapter->fdir_reinit)) {
		++txr->atr_count;
		if (txr->atr_count >= atr_sample_rate) {
			ixgbe_atr(txr, m_head);
			txr->atr_count = 0;
		}
	}
#endif

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

		if (++i == txr->num_desc)
			i = 0;
	}

	txd->read.cmd_type_len |=
	    htole32(IXGBE_TXD_CMD_EOP | IXGBE_TXD_CMD_RS);
	txr->tx_avail -= map->dm_nsegs;
	txr->next_avail_desc = i;

	txbuf->m_head = m_head;
	/*
	** Here we swap the map so the last descriptor,
	** which gets the completion interrupt has the
	** real map, and the first descriptor gets the
	** unused map from this descriptor.
	*/
	txr->tx_buffers[first].map = txbuf->map;
	txbuf->map = map;
	bus_dmamap_sync(txr->txtag->dt_dmat, map, 0, m_head->m_pkthdr.len,
	    BUS_DMASYNC_PREWRITE);

        /* Set the EOP descriptor that will be marked done */
        txbuf = &txr->tx_buffers[first];
	txbuf->eop = txd;

        ixgbe_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	/*
	 * Advance the Transmit Descriptor Tail (Tdt), this tells the
	 * hardware that this frame is available to transmit.
	 */
	++txr->total_packets.ev_count;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_TDT(txr->me), i);

	return 0;
}

static void
ixgbe_set_promisc(struct adapter *adapter)
{
	struct ether_multi *enm;
	struct ether_multistep step;
	u_int32_t       reg_rctl;
	struct ethercom *ec = &adapter->osdep.ec;
	struct ifnet   *ifp = adapter->ifp;
	int		mcnt = 0;

	reg_rctl = IXGBE_READ_REG(&adapter->hw, IXGBE_FCTRL);
	reg_rctl &= (~IXGBE_FCTRL_UPE);
	if (ifp->if_flags & IFF_ALLMULTI)
		mcnt = MAX_NUM_MULTICAST_ADDRESSES;
	else {
		ETHER_FIRST_MULTI(step, ec, enm);
		while (enm != NULL) {
			if (mcnt == MAX_NUM_MULTICAST_ADDRESSES)
				break;
			mcnt++;
			ETHER_NEXT_MULTI(step, enm);
		}
	}
	if (mcnt < MAX_NUM_MULTICAST_ADDRESSES)
		reg_rctl &= (~IXGBE_FCTRL_MPE);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, reg_rctl);

	if (ifp->if_flags & IFF_PROMISC) {
		reg_rctl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, reg_rctl);
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		reg_rctl |= IXGBE_FCTRL_MPE;
		reg_rctl &= ~IXGBE_FCTRL_UPE;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, reg_rctl);
	}
	return;
}


/*********************************************************************
 *  Multicast Update
 *
 *  This routine is called whenever multicast address list is updated.
 *
 **********************************************************************/
#define IXGBE_RAR_ENTRIES 16

static void
ixgbe_set_multi(struct adapter *adapter)
{
	struct ether_multi *enm;
	struct ether_multistep step;
	u32	fctrl;
	u8	*mta;
	u8	*update_ptr;
	int	mcnt = 0;
	struct ethercom *ec = &adapter->osdep.ec;
	struct ifnet   *ifp = adapter->ifp;

	IOCTL_DEBUGOUT("ixgbe_set_multi: begin");

	mta = adapter->mta;
	bzero(mta, sizeof(u8) * IXGBE_ETH_LENGTH_OF_ADDRESS *
	    MAX_NUM_MULTICAST_ADDRESSES);

	ifp->if_flags &= ~IFF_ALLMULTI;
	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		if ((mcnt == MAX_NUM_MULTICAST_ADDRESSES) ||
		    (memcmp(enm->enm_addrlo, enm->enm_addrhi,
			ETHER_ADDR_LEN) != 0)) {
			ifp->if_flags |= IFF_ALLMULTI;
			break;
		}
		bcopy(enm->enm_addrlo,
		    &mta[mcnt * IXGBE_ETH_LENGTH_OF_ADDRESS],
		    IXGBE_ETH_LENGTH_OF_ADDRESS);
		mcnt++;
		ETHER_NEXT_MULTI(step, enm);
	}

	fctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_FCTRL);
	fctrl &= ~(IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	if (ifp->if_flags & IFF_PROMISC)
		fctrl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	else if (ifp->if_flags & IFF_ALLMULTI) {
		fctrl |= IXGBE_FCTRL_MPE;
	}
	
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, fctrl);

	if (mcnt < MAX_NUM_MULTICAST_ADDRESSES) {
		update_ptr = mta;
		ixgbe_update_mc_addr_list(&adapter->hw,
		    update_ptr, mcnt, ixgbe_mc_array_itr, TRUE);
	}

	return;
}

/*
 * This is an iterator function now needed by the multicast
 * shared code. It simply feeds the shared code routine the
 * addresses in the array of ixgbe_set_multi() one by one.
 */
static u8 *
ixgbe_mc_array_itr(struct ixgbe_hw *hw, u8 **update_ptr, u32 *vmdq)
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
ixgbe_local_timer1(void *arg)
{
	struct adapter	*adapter = arg;
	device_t	dev = adapter->dev;
	struct ix_queue *que = adapter->queues;
	struct tx_ring	*txr = adapter->tx_rings;
	int		hung = 0, paused = 0;

	KASSERT(mutex_owned(&adapter->core_mtx));

	/* Check for pluggable optics */
	if (adapter->sfp_probe)
		if (!ixgbe_sfp_probe(adapter))
			goto out; /* Nothing to do */

	ixgbe_update_link_status(adapter);
	ixgbe_update_stats_counters(adapter);

	/*
	 * If the interface has been paused
	 * then don't do the watchdog check
	 */
	if (IXGBE_READ_REG(&adapter->hw, IXGBE_TFCS) & IXGBE_TFCS_TXOFF)
		paused = 1;

	/*
	** Check the TX queues status
	**      - watchdog only if all queues show hung
	*/          
	for (int i = 0; i < adapter->num_queues; i++, que++, txr++) {
		if ((txr->queue_status == IXGBE_QUEUE_HUNG) &&
		    (paused == 0))
			++hung;
		else if (txr->queue_status == IXGBE_QUEUE_WORKING)
#ifndef IXGBE_LEGACY_TX
			softint_schedule(txr->txq_si);
#else
			softint_schedule(que->que_si);
#endif
	}
	/* Only truely watchdog if all queues show hung */
	if (hung == adapter->num_queues)
		goto watchdog;

out:
	callout_reset(&adapter->timer, hz, ixgbe_local_timer, adapter);
	return;

watchdog:
	device_printf(adapter->dev, "Watchdog timeout -- resetting\n");
	device_printf(dev,"Queue(%d) tdh = %d, hw tdt = %d\n", txr->me,
	    IXGBE_READ_REG(&adapter->hw, IXGBE_TDH(txr->me)),
	    IXGBE_READ_REG(&adapter->hw, IXGBE_TDT(txr->me)));
	device_printf(dev,"TX(%d) desc avail = %d,"
	    "Next TX to Clean = %d\n",
	    txr->me, txr->tx_avail, txr->next_to_clean);
	adapter->ifp->if_flags &= ~IFF_RUNNING;
	adapter->watchdog_events.ev_count++;
	ixgbe_init_locked(adapter);
}

static void
ixgbe_local_timer(void *arg)
{
	struct adapter *adapter = arg;

	IXGBE_CORE_LOCK(adapter);
	ixgbe_local_timer1(adapter);
	IXGBE_CORE_UNLOCK(adapter);
}

/*
** Note: this routine updates the OS on the link state
**	the real check of the hardware only happens with
**	a link interrupt.
*/
static void
ixgbe_update_link_status(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	device_t dev = adapter->dev;


	if (adapter->link_up){ 
		if (adapter->link_active == FALSE) {
			if (bootverbose)
				device_printf(dev,"Link is up %d Gbps %s \n",
				    ((adapter->link_speed == 128)? 10:1),
				    "Full Duplex");
			adapter->link_active = TRUE;
			/* Update any Flow Control changes */
			ixgbe_fc_enable(&adapter->hw);
			if_link_state_change(ifp, LINK_STATE_UP);
		}
	} else { /* Link down */
		if (adapter->link_active == TRUE) {
			if (bootverbose)
				device_printf(dev,"Link is Down\n");
			if_link_state_change(ifp, LINK_STATE_DOWN);
			adapter->link_active = FALSE;
		}
	}

	return;
}


static void
ixgbe_ifstop(struct ifnet *ifp, int disable)
{
	struct adapter *adapter = ifp->if_softc;

	IXGBE_CORE_LOCK(adapter);
	ixgbe_stop(adapter);
	IXGBE_CORE_UNLOCK(adapter);
}

/*********************************************************************
 *
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC and deallocates TX/RX buffers.
 *
 **********************************************************************/

static void
ixgbe_stop(void *arg)
{
	struct ifnet   *ifp;
	struct adapter *adapter = arg;
	struct ixgbe_hw *hw = &adapter->hw;
	ifp = adapter->ifp;

	KASSERT(mutex_owned(&adapter->core_mtx));

	INIT_DEBUGOUT("ixgbe_stop: begin\n");
	ixgbe_disable_intr(adapter);
	callout_stop(&adapter->timer);

	/* Let the stack know...*/
	ifp->if_flags &= ~IFF_RUNNING;

	ixgbe_reset_hw(hw);
	hw->adapter_stopped = FALSE;
	ixgbe_stop_adapter(hw);
	if (hw->mac.type == ixgbe_mac_82599EB)
		ixgbe_stop_mac_link_on_d3_82599(hw);
	/* Turn off the laser - noop with no optics */
	ixgbe_disable_tx_laser(hw);

	/* Update the stack */
	adapter->link_up = FALSE;
	ixgbe_update_link_status(adapter);

	/* reprogram the RAR[0] in case user changed it. */
	ixgbe_set_rar(&adapter->hw, 0, adapter->hw.mac.addr, 0, IXGBE_RAH_AV);

	return;
}


/*********************************************************************
 *
 *  Determine hardware revision.
 *
 **********************************************************************/
static void
ixgbe_identify_hardware(struct adapter *adapter)
{
	pcitag_t tag;
	pci_chipset_tag_t pc;
	pcireg_t subid, id;
	struct ixgbe_hw *hw = &adapter->hw;

	pc = adapter->osdep.pc;
	tag = adapter->osdep.tag;

	id = pci_conf_read(pc, tag, PCI_ID_REG);
	subid = pci_conf_read(pc, tag, PCI_SUBSYS_ID_REG);

	/* Save off the information about this board */
	hw->vendor_id = PCI_VENDOR(id);
	hw->device_id = PCI_PRODUCT(id);
	hw->revision_id =
	    PCI_REVISION(pci_conf_read(pc, tag, PCI_CLASS_REG));
	hw->subsystem_vendor_id = PCI_SUBSYS_VENDOR(subid);
	hw->subsystem_device_id = PCI_SUBSYS_ID(subid);

	/* We need this here to set the num_segs below */
	ixgbe_set_mac_type(hw);

	/* Pick up the 82599 and VF settings */
	if (hw->mac.type != ixgbe_mac_82598EB) {
		hw->phy.smart_speed = ixgbe_smart_speed;
		adapter->num_segs = IXGBE_82599_SCATTER;
	} else
		adapter->num_segs = IXGBE_82598_SCATTER;

	return;
}

/*********************************************************************
 *
 *  Determine optic type
 *
 **********************************************************************/
static void
ixgbe_setup_optics(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	int		layer;

	layer = ixgbe_get_supported_physical_layer(hw);

	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_T) {
		adapter->optics = IFM_10G_T;
		return;
	}

	if (layer & IXGBE_PHYSICAL_LAYER_1000BASE_T) {
		adapter->optics = IFM_1000_T;
		return;
	}

	if (layer & IXGBE_PHYSICAL_LAYER_1000BASE_SX) {
		adapter->optics = IFM_1000_SX;
		return;
	}

	if (layer & (IXGBE_PHYSICAL_LAYER_10GBASE_LR |
	    IXGBE_PHYSICAL_LAYER_10GBASE_LRM)) {
		adapter->optics = IFM_10G_LR;
		return;
	}

	if (layer & IXGBE_PHYSICAL_LAYER_10GBASE_SR) {
		adapter->optics = IFM_10G_SR;
		return;
	}

	if (layer & IXGBE_PHYSICAL_LAYER_SFP_PLUS_CU) {
		adapter->optics = IFM_10G_TWINAX;
		return;
	}

	if (layer & (IXGBE_PHYSICAL_LAYER_10GBASE_KX4 |
	    IXGBE_PHYSICAL_LAYER_10GBASE_CX4)) {
		adapter->optics = IFM_10G_CX4;
		return;
	}

	/* If we get here just set the default */
	adapter->optics = IFM_ETHER | IFM_AUTO;
	return;
}

/*********************************************************************
 *
 *  Setup the Legacy or MSI Interrupt handler
 *
 **********************************************************************/
static int
ixgbe_allocate_legacy(struct adapter *adapter,
    const struct pci_attach_args *pa)
{
	device_t	dev = adapter->dev;
	struct		ix_queue *que = adapter->queues;
#ifndef IXGBE_LEGACY_TX
	struct tx_ring		*txr = adapter->tx_rings;
#endif
#ifndef NETBSD_MSI_OR_MSIX
	pci_intr_handle_t	ih;
#else
	int		counts[PCI_INTR_TYPE_SIZE];
	pci_intr_type_t intr_type, max_type;
#endif
	char intrbuf[PCI_INTRSTR_LEN];
	const char	*intrstr = NULL;
 
#ifndef NETBSD_MSI_OR_MSIX
	/* We allocate a single interrupt resource */
 	if (pci_intr_map(pa, &ih) != 0) {
		aprint_error_dev(dev, "unable to map interrupt\n");
		return ENXIO;
	} else {
		intrstr = pci_intr_string(adapter->osdep.pc, ih, intrbuf,
		    sizeof(intrbuf));
	}
	adapter->osdep.ihs[0] = pci_intr_establish(adapter->osdep.pc, ih,
	    IPL_NET, ixgbe_legacy_irq, que);
#else
	/* Allocation settings */
	max_type = PCI_INTR_TYPE_MSI;
	counts[PCI_INTR_TYPE_MSIX] = 0;
	counts[PCI_INTR_TYPE_MSI] = 1;
	counts[PCI_INTR_TYPE_INTX] = 1;

alloc_retry:
	if (pci_intr_alloc(pa, &adapter->osdep.intrs, counts, max_type) != 0) {
		aprint_error_dev(dev, "couldn't alloc interrupt\n");
		return ENXIO;
	}
	adapter->osdep.nintrs = 1;
	intrstr = pci_intr_string(adapter->osdep.pc, adapter->osdep.intrs[0],
	    intrbuf, sizeof(intrbuf));
	adapter->osdep.ihs[0] = pci_intr_establish(adapter->osdep.pc,
	    adapter->osdep.intrs[0], IPL_NET, ixgbe_legacy_irq, que);
	if (adapter->osdep.ihs[0] == NULL) {
		intr_type = pci_intr_type(adapter->osdep.intrs[0]);
		aprint_error_dev(dev,"unable to establish %s\n",
		    (intr_type == PCI_INTR_TYPE_MSI) ? "MSI" : "INTx");
		pci_intr_release(adapter->osdep.pc, adapter->osdep.intrs, 1);
		switch (intr_type) {
		case PCI_INTR_TYPE_MSI:
			/* The next try is for INTx: Disable MSI */
			max_type = PCI_INTR_TYPE_INTX;
			counts[PCI_INTR_TYPE_INTX] = 1;
			goto alloc_retry;
		case PCI_INTR_TYPE_INTX:
		default:
			/* See below */
			break;
		}
	}
#endif
	if (adapter->osdep.ihs[0] == NULL) {
		aprint_error_dev(dev,
		    "couldn't establish interrupt%s%s\n",
		    intrstr ? " at " : "", intrstr ? intrstr : "");
#ifdef NETBSD_MSI_OR_MSIX
		pci_intr_release(adapter->osdep.pc, adapter->osdep.intrs, 1);
#endif
		return ENXIO;
	}
	aprint_normal_dev(dev, "interrupting at %s\n", intrstr);
	/*
	 * Try allocating a fast interrupt and the associated deferred
	 * processing contexts.
	 */
#ifndef IXGBE_LEGACY_TX
	txr->txq_si = softint_establish(SOFTINT_NET, ixgbe_deferred_mq_start,
	    txr);
#endif
	que->que_si = softint_establish(SOFTINT_NET, ixgbe_handle_que, que);

	/* Tasklets for Link, SFP and Multispeed Fiber */
	adapter->link_si =
	    softint_establish(SOFTINT_NET, ixgbe_handle_link, adapter);
	adapter->mod_si =
	    softint_establish(SOFTINT_NET, ixgbe_handle_mod, adapter);
	adapter->msf_si =
	    softint_establish(SOFTINT_NET, ixgbe_handle_msf, adapter);

#ifdef IXGBE_FDIR
	adapter->fdir_si =
	    softint_establish(SOFTINT_NET, ixgbe_reinit_fdir, adapter);
#endif
	if (que->que_si == NULL ||
	    adapter->link_si == NULL ||
	    adapter->mod_si == NULL ||
#ifdef IXGBE_FDIR
	    adapter->fdir_si == NULL ||
#endif
	    adapter->msf_si == NULL) {
		aprint_error_dev(dev,
		    "could not establish software interrupts\n"); 
		return ENXIO;
	}

	/* For simplicity in the handlers */
	adapter->que_mask = IXGBE_EIMS_ENABLE_MASK;

	return (0);
}


/*********************************************************************
 *
 *  Setup MSIX Interrupt resources and handlers 
 *
 **********************************************************************/
static int
ixgbe_allocate_msix(struct adapter *adapter, const struct pci_attach_args *pa)
{
#if !defined(NETBSD_MSI_OR_MSIX)
	return 0;
#else
	device_t        dev = adapter->dev;
	struct 		ix_queue *que = adapter->queues;
	struct  	tx_ring *txr = adapter->tx_rings;
	pci_chipset_tag_t pc;
	char		intrbuf[PCI_INTRSTR_LEN];
	const char	*intrstr = NULL;
	int 		error, vector = 0;
	int		cpu_id = 0;
	kcpuset_t	*affinity;

	pc = adapter->osdep.pc;
#ifdef	RSS
	cpuset_t cpu_mask;
	/*
	 * If we're doing RSS, the number of queues needs to
	 * match the number of RSS buckets that are configured.
	 *
	 * + If there's more queues than RSS buckets, we'll end
	 *   up with queues that get no traffic.
	 *
	 * + If there's more RSS buckets than queues, we'll end
	 *   up having multiple RSS buckets map to the same queue,
	 *   so there'll be some contention.
	 */
	if (adapter->num_queues != rss_getnumbuckets()) {
		device_printf(dev,
		    "%s: number of queues (%d) != number of RSS buckets (%d)"
		    "; performance will be impacted.\n",
		    __func__,
		    adapter->num_queues,
		    rss_getnumbuckets());
	}
#endif

	adapter->osdep.nintrs = adapter->num_queues + 1;
	if (pci_msix_alloc_exact(pa, &adapter->osdep.intrs,
	    adapter->osdep.nintrs) != 0) {
		aprint_error_dev(dev,
		    "failed to allocate MSI-X interrupt\n");
		return (ENXIO);
	}

	kcpuset_create(&affinity, false);
	for (int i = 0; i < adapter->num_queues; i++, vector++, que++, txr++) {
		intrstr = pci_intr_string(pc, adapter->osdep.intrs[i], intrbuf,
		    sizeof(intrbuf));
#ifdef IXG_MPSAFE
		pci_intr_setattr(pc, adapter->osdep.intrs[i], PCI_INTR_MPSAFE,
		    true);
#endif
		/* Set the handler function */
		que->res = adapter->osdep.ihs[i] = pci_intr_establish(pc,
		    adapter->osdep.intrs[i], IPL_NET, ixgbe_msix_que, que);
		if (que->res == NULL) {
			pci_intr_release(pc, adapter->osdep.intrs,
			    adapter->osdep.nintrs);
			aprint_error_dev(dev,
			    "Failed to register QUE handler\n");
			kcpuset_destroy(affinity);
			return ENXIO;
		}
		que->msix = vector;
        	adapter->que_mask |= (u64)(1 << que->msix);
#ifdef	RSS
		/*
		 * The queue ID is used as the RSS layer bucket ID.
		 * We look up the queue ID -> RSS CPU ID and select
		 * that.
		 */
		cpu_id = rss_getcpu(i % rss_getnumbuckets());
#else
		/*
		 * Bind the msix vector, and thus the
		 * rings to the corresponding cpu.
		 *
		 * This just happens to match the default RSS round-robin
		 * bucket -> queue -> CPU allocation.
		 */
		if (adapter->num_queues > 1)
			cpu_id = i;
#endif
		/* Round-robin affinity */
		kcpuset_zero(affinity);
		kcpuset_set(affinity, cpu_id % ncpu);
		error = interrupt_distribute(adapter->osdep.ihs[i], affinity,
		    NULL);
		aprint_normal_dev(dev, "for TX/RX, interrupting at %s",
		    intrstr);
		if (error == 0) {
#ifdef	RSS
			aprintf_normal(", bound RSS bucket %d to CPU %d\n",
			    i, cpu_id);
#else
			aprint_normal(", bound queue %d to cpu %d\n",
			    i, cpu_id);
#endif
		} else
			aprint_normal("\n");

#ifndef IXGBE_LEGACY_TX
		txr->txq_si = softint_establish(SOFTINT_NET,
		    ixgbe_deferred_mq_start, txr);
#endif
		que->que_si = softint_establish(SOFTINT_NET, ixgbe_handle_que,
		    que);
		if (que->que_si == NULL) {
			aprint_error_dev(dev,
			    "could not establish software interrupt\n"); 
		}
	}

	/* and Link */
	cpu_id++;
	intrstr = pci_intr_string(pc, adapter->osdep.intrs[vector], intrbuf,
	    sizeof(intrbuf));
#ifdef IXG_MPSAFE
	pci_intr_setattr(pc, &adapter->osdep.intrs[vector], PCI_INTR_MPSAFE,
	    true);
#endif
	/* Set the link handler function */
	adapter->osdep.ihs[vector] = pci_intr_establish(pc,
	    adapter->osdep.intrs[vector], IPL_NET, ixgbe_msix_link, adapter);
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
	    "for link, interrupting at %s", intrstr);
	if (error == 0)
		aprint_normal(", affinity to cpu %d\n", cpu_id);
	else
		aprint_normal("\n");

	adapter->linkvec = vector;
	/* Tasklets for Link, SFP and Multispeed Fiber */
	adapter->link_si =
	    softint_establish(SOFTINT_NET, ixgbe_handle_link, adapter);
	adapter->mod_si =
	    softint_establish(SOFTINT_NET, ixgbe_handle_mod, adapter);
	adapter->msf_si =
	    softint_establish(SOFTINT_NET, ixgbe_handle_msf, adapter);
#ifdef IXGBE_FDIR
	adapter->fdir_si =
	    softint_establish(SOFTINT_NET, ixgbe_reinit_fdir, adapter);
#endif

	kcpuset_destroy(affinity);
	return (0);
#endif
}

/*
 * Setup Either MSI/X or MSI
 */
static int
ixgbe_setup_msix(struct adapter *adapter)
{
#if !defined(NETBSD_MSI_OR_MSIX)
	return 0;
#else
	device_t dev = adapter->dev;
	int want, queues, msgs;

	/* Override by tuneable */
	if (ixgbe_enable_msix == 0)
		goto msi;

	/* First try MSI/X */
	msgs = pci_msix_count(adapter->osdep.pc, adapter->osdep.tag);
	if (msgs < IXG_MSIX_NINTR)
		goto msi;

	adapter->msix_mem = (void *)1; /* XXX */

	/* Figure out a reasonable auto config value */
	queues = (ncpu > (msgs-1)) ? (msgs-1) : ncpu;

	/* Override based on tuneable */
	if (ixgbe_num_queues != 0)
		queues = ixgbe_num_queues;

#ifdef	RSS
	/* If we're doing RSS, clamp at the number of RSS buckets */
	if (queues > rss_getnumbuckets())
		queues = rss_getnumbuckets();
#endif

	/* reflect correct sysctl value */
	ixgbe_num_queues = queues;

	/*
	** Want one vector (RX/TX pair) per queue
	** plus an additional for Link.
	*/
	want = queues + 1;
	if (msgs >= want)
		msgs = want;
	else {
               	aprint_error_dev(dev,
		    "MSIX Configuration Problem, "
		    "%d vectors but %d queues wanted!\n",
		    msgs, want);
		goto msi;
	}
	device_printf(dev,
	    "Using MSIX interrupts with %d vectors\n", msgs);
	adapter->num_queues = queues;
	return (msgs);

	/*
	** If MSIX alloc failed or provided us with
	** less than needed, free and fall through to MSI
	*/
msi:
       	msgs = pci_msi_count(adapter->osdep.pc, adapter->osdep.tag);
	adapter->msix_mem = NULL; /* XXX */
       	msgs = 1;
	aprint_normal_dev(dev,"Using an MSI interrupt\n");
	return (msgs);
#endif
}


static int
ixgbe_allocate_pci_resources(struct adapter *adapter,
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

	/* Legacy defaults */
	adapter->num_queues = 1;
	adapter->hw.back = &adapter->osdep;

	/*
	** Now setup MSI or MSI/X, should
	** return us the number of supported
	** vectors. (Will be 1 for MSI)
	*/
	adapter->msix = ixgbe_setup_msix(adapter);
	return (0);
}

static void
ixgbe_free_pci_resources(struct adapter * adapter)
{
#if defined(NETBSD_MSI_OR_MSIX)
	struct 		ix_queue *que = adapter->queues;
#endif
	int		rid;

#if defined(NETBSD_MSI_OR_MSIX)
	/*
	**  Release all msix queue resources:
	*/
	for (int i = 0; i < adapter->num_queues; i++, que++) {
		if (que->res != NULL)
			pci_intr_disestablish(adapter->osdep.pc,
			    adapter->osdep.ihs[i]);
	}
#endif

	/* Clean the Legacy or Link interrupt last */
	if (adapter->linkvec) /* we are doing MSIX */
		rid = adapter->linkvec;
	else
		rid = 0;

	if (adapter->osdep.ihs[rid] != NULL) {
		pci_intr_disestablish(adapter->osdep.pc,
		    adapter->osdep.ihs[rid]);
		adapter->osdep.ihs[rid] = NULL;
	}

#if defined(NETBSD_MSI_OR_MSIX)
	pci_intr_release(adapter->osdep.pc, adapter->osdep.intrs,
	    adapter->osdep.nintrs);
#endif

	if (adapter->osdep.mem_size != 0) {
		bus_space_unmap(adapter->osdep.mem_bus_space_tag,
		    adapter->osdep.mem_bus_space_handle,
		    adapter->osdep.mem_size);
	}

	return;
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
static int
ixgbe_setup_interface(device_t dev, struct adapter *adapter)
{
	struct ethercom *ec = &adapter->osdep.ec;
	struct ixgbe_hw *hw = &adapter->hw;
	struct ifnet   *ifp;

	INIT_DEBUGOUT("ixgbe_setup_interface: begin");

	ifp = adapter->ifp = &ec->ec_if;
	strlcpy(ifp->if_xname, device_xname(dev), IFNAMSIZ);
	ifp->if_baudrate = IF_Gbps(10);
	ifp->if_init = ixgbe_init;
	ifp->if_stop = ixgbe_ifstop;
	ifp->if_softc = adapter;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ixgbe_ioctl;
#ifndef IXGBE_LEGACY_TX
	ifp->if_transmit = ixgbe_mq_start;
	ifp->if_qflush = ixgbe_qflush;
#else
	ifp->if_start = ixgbe_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, adapter->num_tx_desc - 2);
#if 0
	ifp->if_snd.ifq_drv_maxlen = adapter->num_tx_desc - 2;
#endif
	IFQ_SET_READY(&ifp->if_snd);
#endif

	if_attach(ifp);
	ether_ifattach(ifp, adapter->hw.mac.addr);
	ether_set_ifflags_cb(ec, ixgbe_ifflags_cb);

	adapter->max_frame_size =
	    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_hdrlen = sizeof(struct ether_vlan_header);

	ifp->if_capabilities |= IFCAP_HWCSUM | IFCAP_TSOv4 | IFCAP_TSOv6;
	ifp->if_capenable = 0;

	ec->ec_capabilities |= ETHERCAP_VLAN_HWCSUM;
	ec->ec_capabilities |= ETHERCAP_JUMBO_MTU;
	ifp->if_capabilities |= IFCAP_LRO;
	ec->ec_capabilities |= ETHERCAP_VLAN_HWTAGGING
	    		    | ETHERCAP_VLAN_MTU;
	ec->ec_capenable = ec->ec_capabilities;

	/*
	** Don't turn this on by default, if vlans are
	** created on another pseudo device (eg. lagg)
	** then vlan events are not passed thru, breaking
	** operation, but with HW FILTER off it works. If
	** using vlans directly on the ixgbe driver you can
	** enable this and get full hardware tag filtering.
	*/
	ec->ec_capabilities |= ETHERCAP_VLAN_HWFILTER;

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&adapter->media, IFM_IMASK, ixgbe_media_change,
		     ixgbe_media_status);
	ifmedia_add(&adapter->media, IFM_ETHER | adapter->optics, 0, NULL);
	ifmedia_set(&adapter->media, IFM_ETHER | adapter->optics);
	if (hw->device_id == IXGBE_DEV_ID_82598AT) {
		ifmedia_add(&adapter->media,
		    IFM_ETHER | IFM_1000_T | IFM_FDX, 0, NULL);
		ifmedia_add(&adapter->media,
		    IFM_ETHER | IFM_1000_T, 0, NULL);
	}
	ifmedia_add(&adapter->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&adapter->media, IFM_ETHER | IFM_AUTO);

	return (0);
}

static void
ixgbe_config_link(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32	autoneg, err = 0;
	bool	sfp, negotiate;

	sfp = ixgbe_is_sfp(hw);

	if (sfp) { 
		void *ip;

		if (hw->phy.multispeed_fiber) {
			hw->mac.ops.setup_sfp(hw);
			ixgbe_enable_tx_laser(hw);
			ip = adapter->msf_si;
		} else {
			ip = adapter->mod_si;
		}

		kpreempt_disable();
		softint_schedule(ip);
		kpreempt_enable();
	} else {
		if (hw->mac.ops.check_link)
			err = ixgbe_check_link(hw, &adapter->link_speed,
			    &adapter->link_up, FALSE);
		if (err)
			goto out;
		autoneg = hw->phy.autoneg_advertised;
		if ((!autoneg) && (hw->mac.ops.get_link_capabilities))
                	err  = hw->mac.ops.get_link_capabilities(hw,
			    &autoneg, &negotiate);
		else
			negotiate = 0;
		if (err)
			goto out;
		if (hw->mac.ops.setup_link)
                	err = hw->mac.ops.setup_link(hw,
			    autoneg, adapter->link_up);
	}
out:
	return;
}

/********************************************************************
 * Manage DMA'able memory.
 *******************************************************************/

static int
ixgbe_dma_malloc(struct adapter *adapter, const bus_size_t size,
		struct ixgbe_dma_alloc *dma, const int mapflags)
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
		    "%s: ixgbe_dma_tag_create failed; error %d\n", __func__, r);
		goto fail_0;
	}

	r = bus_dmamem_alloc(dma->dma_tag->dt_dmat,
		size,
		dma->dma_tag->dt_alignment,
		dma->dma_tag->dt_boundary,
		&dma->dma_seg, 1, &rsegs, BUS_DMA_NOWAIT);
	if (r != 0) {
		aprint_error_dev(dev,
		    "%s: bus_dmamem_alloc failed; error %d\n", __func__, r);
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
		aprint_error_dev(dev, "%s: bus_dmamap_load failed; error %d\n",
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
	return r;
}

static void
ixgbe_dma_free(struct adapter *adapter, struct ixgbe_dma_alloc *dma)
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
ixgbe_allocate_queues(struct adapter *adapter)
{
	device_t	dev = adapter->dev;
	struct ix_queue	*que;
	struct tx_ring	*txr;
	struct rx_ring	*rxr;
	int rsize, tsize, error = IXGBE_SUCCESS;
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
		txr->num_desc = adapter->num_tx_desc;

		/* Initialize the TX side lock */
		snprintf(txr->mtx_name, sizeof(txr->mtx_name), "%s:tx(%d)",
		    device_xname(dev), txr->me);
		mutex_init(&txr->tx_mtx, MUTEX_DEFAULT, IPL_NET);

		if (ixgbe_dma_malloc(adapter, tsize,
			&txr->txdma, BUS_DMA_NOWAIT)) {
			aprint_error_dev(dev,
			    "Unable to allocate TX Descriptor memory\n");
			error = ENOMEM;
			goto err_tx_desc;
		}
		txr->tx_base = (union ixgbe_adv_tx_desc *)txr->txdma.dma_vaddr;
		bzero((void *)txr->tx_base, tsize);

        	/* Now allocate transmit buffers for the ring */
        	if (ixgbe_allocate_transmit_buffers(txr)) {
			aprint_error_dev(dev,
			    "Critical Failure setting up transmit buffers\n");
			error = ENOMEM;
			goto err_tx_desc;
        	}
#ifndef IXGBE_LEGACY_TX
		/* Allocate a buf ring */
		txr->br = buf_ring_alloc(IXGBE_BR_SIZE, M_DEVBUF,
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
		rxr->num_desc = adapter->num_rx_desc;

		/* Initialize the RX side lock */
		snprintf(rxr->mtx_name, sizeof(rxr->mtx_name), "%s:rx(%d)",
		    device_xname(dev), rxr->me);
		mutex_init(&rxr->rx_mtx, MUTEX_DEFAULT, IPL_NET);

		if (ixgbe_dma_malloc(adapter, rsize,
			&rxr->rxdma, BUS_DMA_NOWAIT)) {
			aprint_error_dev(dev,
			    "Unable to allocate RxDescriptor memory\n");
			error = ENOMEM;
			goto err_rx_desc;
		}
		rxr->rx_base = (union ixgbe_adv_rx_desc *)rxr->rxdma.dma_vaddr;
		bzero((void *)rxr->rx_base, rsize);

        	/* Allocate receive buffers for the ring*/
		if (ixgbe_allocate_receive_buffers(rxr)) {
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
		ixgbe_dma_free(adapter, &rxr->rxdma);
err_tx_desc:
	for (txr = adapter->tx_rings; txconf > 0; txr++, txconf--)
		ixgbe_dma_free(adapter, &txr->txdma);
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
ixgbe_allocate_transmit_buffers(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	device_t dev = adapter->dev;
	struct ixgbe_tx_buf *txbuf;
	int error, i;

	/*
	 * Setup DMA descriptor areas.
	 */
	if ((error = ixgbe_dma_tag_create(adapter->osdep.dmat,	/* parent */
			       1, 0,		/* alignment, bounds */
			       IXGBE_TSO_SIZE,		/* maxsize */
			       adapter->num_segs,	/* nsegments */
			       PAGE_SIZE,		/* maxsegsize */
			       0,			/* flags */
			       &txr->txtag))) {
		aprint_error_dev(dev,"Unable to allocate TX DMA tag\n");
		goto fail;
	}

	if (!(txr->tx_buffers =
	    (struct ixgbe_tx_buf *) malloc(sizeof(struct ixgbe_tx_buf) *
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
			aprint_error_dev(dev,
			    "Unable to create TX DMA map (%d)\n", error);
			goto fail;
		}
	}

	return 0;
fail:
	/* We free all, it handles case where we are in the middle */
	ixgbe_free_transmit_structures(adapter);
	return (error);
}

/*********************************************************************
 *
 *  Initialize a transmit ring.
 *
 **********************************************************************/
static void
ixgbe_setup_transmit_ring(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	struct ixgbe_tx_buf *txbuf;
	int i;
#ifdef DEV_NETMAP
	struct netmap_adapter *na = NA(adapter->ifp);
	struct netmap_slot *slot;
#endif /* DEV_NETMAP */

	/* Clear the old ring contents */
	IXGBE_TX_LOCK(txr);
#ifdef DEV_NETMAP
	/*
	 * (under lock): if in netmap mode, do some consistency
	 * checks and set slot to entry 0 of the netmap ring.
	 */
	slot = netmap_reset(na, NR_TX, txr->me, 0);
#endif /* DEV_NETMAP */
	bzero((void *)txr->tx_base,
	      (sizeof(union ixgbe_adv_tx_desc)) * adapter->num_tx_desc);
	/* Reset indices */
	txr->next_avail_desc = 0;
	txr->next_to_clean = 0;

	/* Free any existing tx buffers. */
        txbuf = txr->tx_buffers;
	for (i = 0; i < txr->num_desc; i++, txbuf++) {
		if (txbuf->m_head != NULL) {
			bus_dmamap_sync(txr->txtag->dt_dmat, txbuf->map,
			    0, txbuf->m_head->m_pkthdr.len,
			    BUS_DMASYNC_POSTWRITE);
			ixgbe_dmamap_unload(txr->txtag, txbuf->map);
			m_freem(txbuf->m_head);
			txbuf->m_head = NULL;
		}
#ifdef DEV_NETMAP
		/*
		 * In netmap mode, set the map for the packet buffer.
		 * NOTE: Some drivers (not this one) also need to set
		 * the physical buffer address in the NIC ring.
		 * Slots in the netmap ring (indexed by "si") are
		 * kring->nkr_hwofs positions "ahead" wrt the
		 * corresponding slot in the NIC ring. In some drivers
		 * (not here) nkr_hwofs can be negative. Function
		 * netmap_idx_n2k() handles wraparounds properly.
		 */
		if (slot) {
			int si = netmap_idx_n2k(&na->tx_rings[txr->me], i);
			netmap_load_map(na, txr->txtag, txbuf->map, NMB(na, slot + si));
		}
#endif /* DEV_NETMAP */
		/* Clear the EOP descriptor pointer */
		txbuf->eop = NULL;
        }

#ifdef IXGBE_FDIR
	/* Set the rate at which we sample packets */
	if (adapter->hw.mac.type != ixgbe_mac_82598EB)
		txr->atr_sample = atr_sample_rate;
#endif

	/* Set number of descriptors available */
	txr->tx_avail = adapter->num_tx_desc;

	ixgbe_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	IXGBE_TX_UNLOCK(txr);
}

/*********************************************************************
 *
 *  Initialize all transmit rings.
 *
 **********************************************************************/
static int
ixgbe_setup_transmit_structures(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;

	for (int i = 0; i < adapter->num_queues; i++, txr++)
		ixgbe_setup_transmit_ring(txr);

	return (0);
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
ixgbe_initialize_transmit_units(struct adapter *adapter)
{
	struct tx_ring	*txr = adapter->tx_rings;
	struct ixgbe_hw	*hw = &adapter->hw;

	/* Setup the Base and Length of the Tx Descriptor Ring */

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		u64	tdba = txr->txdma.dma_paddr;
		u32	txctrl;

		IXGBE_WRITE_REG(hw, IXGBE_TDBAL(i),
		       (tdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_TDBAH(i), (tdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_TDLEN(i),
		    adapter->num_tx_desc * sizeof(union ixgbe_adv_tx_desc));

		/* Setup the HW Tx Head and Tail descriptor pointers */
		IXGBE_WRITE_REG(hw, IXGBE_TDH(i), 0);
		IXGBE_WRITE_REG(hw, IXGBE_TDT(i), 0);

		/* Setup Transmit Descriptor Cmd Settings */
		txr->txd_cmd = IXGBE_TXD_CMD_IFCS;
		txr->queue_status = IXGBE_QUEUE_IDLE;

		/* Set the processing limit */
		txr->process_limit = ixgbe_tx_process_limit;

		/* Disable Head Writeback */
		switch (hw->mac.type) {
		case ixgbe_mac_82598EB:
			txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL(i));
			break;
		case ixgbe_mac_82599EB:
		case ixgbe_mac_X540:
		default:
			txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL_82599(i));
			break;
                }
		txctrl &= ~IXGBE_DCA_TXCTRL_DESC_WRO_EN;
		switch (hw->mac.type) {
		case ixgbe_mac_82598EB:
			IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL(i), txctrl);
			break;
		case ixgbe_mac_82599EB:
		case ixgbe_mac_X540:
		default:
			IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL_82599(i), txctrl);
			break;
		}

	}

	if (hw->mac.type != ixgbe_mac_82598EB) {
		u32 dmatxctl, rttdcs;
		dmatxctl = IXGBE_READ_REG(hw, IXGBE_DMATXCTL);
		dmatxctl |= IXGBE_DMATXCTL_TE;
		IXGBE_WRITE_REG(hw, IXGBE_DMATXCTL, dmatxctl);
		/* Disable arbiter to set MTQC */
		rttdcs = IXGBE_READ_REG(hw, IXGBE_RTTDCS);
		rttdcs |= IXGBE_RTTDCS_ARBDIS;
		IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);
		IXGBE_WRITE_REG(hw, IXGBE_MTQC, IXGBE_MTQC_64Q_1PB);
		rttdcs &= ~IXGBE_RTTDCS_ARBDIS;
		IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);
	}

	return;
}

/*********************************************************************
 *
 *  Free all transmit rings.
 *
 **********************************************************************/
static void
ixgbe_free_transmit_structures(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		ixgbe_free_transmit_buffers(txr);
		ixgbe_dma_free(adapter, &txr->txdma);
		IXGBE_TX_LOCK_DESTROY(txr);
	}
	free(adapter->tx_rings, M_DEVBUF);
}

/*********************************************************************
 *
 *  Free transmit ring related data structures.
 *
 **********************************************************************/
static void
ixgbe_free_transmit_buffers(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	struct ixgbe_tx_buf *tx_buffer;
	int             i;

	INIT_DEBUGOUT("ixgbe_free_transmit_ring: begin");

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
#ifndef IXGBE_LEGACY_TX
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
 *  Advanced Context Descriptor setup for VLAN, CSUM or TSO
 *
 **********************************************************************/

static int
ixgbe_tx_ctx_setup(struct tx_ring *txr, struct mbuf *mp,
    u32 *cmd_type_len, u32 *olinfo_status)
{
	struct m_tag *mtag;
	struct adapter *adapter = txr->adapter;
	struct ethercom *ec = &adapter->osdep.ec;
	struct ixgbe_adv_tx_context_desc *TXD;
	struct ether_vlan_header *eh;
	struct ip ip;
	struct ip6_hdr ip6;
	u32 vlan_macip_lens = 0, type_tucmd_mlhl = 0;
	int	ehdrlen, ip_hlen = 0;
	u16	etype;
	u8	ipproto __diagused = 0;
	int	offload = TRUE;
	int	ctxd = txr->next_avail_desc;
	u16	vtag = 0;

	/* First check if TSO is to be used */
	if (mp->m_pkthdr.csum_flags & (M_CSUM_TSOv4|M_CSUM_TSOv6))
		return (ixgbe_tso_setup(txr, mp, cmd_type_len, olinfo_status));

	if ((mp->m_pkthdr.csum_flags & M_CSUM_OFFLOAD) == 0)
		offload = FALSE;

	/* Indicate the whole packet as payload when not doing TSO */
       	*olinfo_status |= mp->m_pkthdr.len << IXGBE_ADVTXD_PAYLEN_SHIFT;

	/* Now ready a context descriptor */
	TXD = (struct ixgbe_adv_tx_context_desc *) &txr->tx_base[ctxd];

	/*
	** In advanced descriptors the vlan tag must 
	** be placed into the context descriptor. Hence
	** we need to make one even if not doing offloads.
	*/
	if ((mtag = VLAN_OUTPUT_TAG(ec, mp)) != NULL) {
		vtag = htole16(VLAN_TAG_VALUE(mtag) & 0xffff);
		vlan_macip_lens |= (vtag << IXGBE_ADVTXD_VLAN_SHIFT);
	} else if (offload == FALSE) /* ... no offload to do */
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
		/* XXX-BZ this will go badly in case of ext hdrs. */
		ipproto = ip6.ip6_nxt;
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV6;
		break;
	default:
		break;
	}

	if ((mp->m_pkthdr.csum_flags & M_CSUM_IPv4) != 0)
		*olinfo_status |= IXGBE_TXD_POPTS_IXSM << 8;

	vlan_macip_lens |= ip_hlen;
	type_tucmd_mlhl |= IXGBE_ADVTXD_DCMD_DEXT | IXGBE_ADVTXD_DTYP_CTXT;

	if (mp->m_pkthdr.csum_flags & (M_CSUM_TCPv4|M_CSUM_TCPv6)) {
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_TCP;
		*olinfo_status |= IXGBE_TXD_POPTS_TXSM << 8;
		KASSERT(ipproto == IPPROTO_TCP);
	} else if (mp->m_pkthdr.csum_flags & (M_CSUM_UDPv4|M_CSUM_UDPv6)) {
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_UDP;
		*olinfo_status |= IXGBE_TXD_POPTS_TXSM << 8;
		KASSERT(ipproto == IPPROTO_UDP);
	}

	/* Now copy bits into descriptor */
	TXD->vlan_macip_lens = htole32(vlan_macip_lens);
	TXD->type_tucmd_mlhl = htole32(type_tucmd_mlhl);
	TXD->seqnum_seed = htole32(0);
	TXD->mss_l4len_idx = htole32(0);

	/* We've consumed the first desc, adjust counters */
	if (++ctxd == txr->num_desc)
		ctxd = 0;
	txr->next_avail_desc = ctxd;
	--txr->tx_avail;

        return 0;
}

/**********************************************************************
 *
 *  Setup work for hardware segmentation offload (TSO) on
 *  adapters using advanced tx descriptors
 *
 **********************************************************************/
static int
ixgbe_tso_setup(struct tx_ring *txr, struct mbuf *mp,
    u32 *cmd_type_len, u32 *olinfo_status)
{
	struct m_tag *mtag;
	struct adapter *adapter = txr->adapter;
	struct ethercom *ec = &adapter->osdep.ec;
	struct ixgbe_adv_tx_context_desc *TXD;
	u32 vlan_macip_lens = 0, type_tucmd_mlhl = 0;
	u32 mss_l4len_idx = 0, paylen;
	u16 vtag = 0, eh_type;
	int ctxd, ehdrlen, ip_hlen, tcp_hlen;
	struct ether_vlan_header *eh;
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
#ifdef INET
	struct ip *ip;
#endif
	struct tcphdr *th;


	/*
	 * Determine where frame payload starts.
	 * Jump over vlan headers if already present
	 */
	eh = mtod(mp, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		eh_type = eh->evl_proto;
	} else {
		ehdrlen = ETHER_HDR_LEN;
		eh_type = eh->evl_encap_proto;
	}

	switch (ntohs(eh_type)) {
#ifdef INET6
	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *)(mp->m_data + ehdrlen);
		/* XXX-BZ For now we do not pretend to support ext. hdrs. */
		if (ip6->ip6_nxt != IPPROTO_TCP)
			return (ENXIO);
		ip_hlen = sizeof(struct ip6_hdr);
		ip6 = (struct ip6_hdr *)(mp->m_data + ehdrlen);
		th = (struct tcphdr *)((char *)ip6 + ip_hlen);
		th->th_sum = in6_cksum_phdr(&ip6->ip6_src,
		    &ip6->ip6_dst, 0, htonl(IPPROTO_TCP));
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV6;
		break;
#endif
#ifdef INET
	case ETHERTYPE_IP:
		ip = (struct ip *)(mp->m_data + ehdrlen);
		if (ip->ip_p != IPPROTO_TCP)
			return (ENXIO);
		ip->ip_sum = 0;
		ip_hlen = ip->ip_hl << 2;
		th = (struct tcphdr *)((char *)ip + ip_hlen);
		th->th_sum = in_cksum_phdr(ip->ip_src.s_addr,
		    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
		type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV4;
		/* Tell transmit desc to also do IPv4 checksum. */
		*olinfo_status |= IXGBE_TXD_POPTS_IXSM << 8;
		break;
#endif
	default:
		panic("%s: CSUM_TSO but no supported IP version (0x%04x)",
		    __func__, ntohs(eh_type));
		break;
	}

	ctxd = txr->next_avail_desc;
	TXD = (struct ixgbe_adv_tx_context_desc *) &txr->tx_base[ctxd];

	tcp_hlen = th->th_off << 2;

	/* This is used in the transmit desc in encap */
	paylen = mp->m_pkthdr.len - ehdrlen - ip_hlen - tcp_hlen;

	/* VLAN MACLEN IPLEN */
	if ((mtag = VLAN_OUTPUT_TAG(ec, mp)) != NULL) {
		vtag = htole16(VLAN_TAG_VALUE(mtag) & 0xffff);
                vlan_macip_lens |= (vtag << IXGBE_ADVTXD_VLAN_SHIFT);
	}

	vlan_macip_lens |= ehdrlen << IXGBE_ADVTXD_MACLEN_SHIFT;
	vlan_macip_lens |= ip_hlen;
	TXD->vlan_macip_lens = htole32(vlan_macip_lens);

	/* ADV DTYPE TUCMD */
	type_tucmd_mlhl |= IXGBE_ADVTXD_DCMD_DEXT | IXGBE_ADVTXD_DTYP_CTXT;
	type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_TCP;
	TXD->type_tucmd_mlhl = htole32(type_tucmd_mlhl);

	/* MSS L4LEN IDX */
	mss_l4len_idx |= (mp->m_pkthdr.segsz << IXGBE_ADVTXD_MSS_SHIFT);
	mss_l4len_idx |= (tcp_hlen << IXGBE_ADVTXD_L4LEN_SHIFT);
	TXD->mss_l4len_idx = htole32(mss_l4len_idx);

	TXD->seqnum_seed = htole32(0);

	if (++ctxd == txr->num_desc)
		ctxd = 0;

	txr->tx_avail--;
	txr->next_avail_desc = ctxd;
	*cmd_type_len |= IXGBE_ADVTXD_DCMD_TSE;
	*olinfo_status |= IXGBE_TXD_POPTS_TXSM << 8;
	*olinfo_status |= paylen << IXGBE_ADVTXD_PAYLEN_SHIFT;
	++txr->tso_tx.ev_count;
	return (0);
}

#ifdef IXGBE_FDIR
/*
** This routine parses packet headers so that Flow
** Director can make a hashed filter table entry 
** allowing traffic flows to be identified and kept
** on the same cpu.  This would be a performance
** hit, but we only do it at IXGBE_FDIR_RATE of
** packets.
*/
static void
ixgbe_atr(struct tx_ring *txr, struct mbuf *mp)
{
	struct adapter			*adapter = txr->adapter;
	struct ix_queue			*que;
	struct ip			*ip;
	struct tcphdr			*th;
	struct udphdr			*uh;
	struct ether_vlan_header	*eh;
	union ixgbe_atr_hash_dword	input = {.dword = 0}; 
	union ixgbe_atr_hash_dword	common = {.dword = 0}; 
	int  				ehdrlen, ip_hlen;
	u16				etype;

	eh = mtod(mp, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		etype = eh->evl_proto;
	} else {
		ehdrlen = ETHER_HDR_LEN;
		etype = eh->evl_encap_proto;
	}

	/* Only handling IPv4 */
	if (etype != htons(ETHERTYPE_IP))
		return;

	ip = (struct ip *)(mp->m_data + ehdrlen);
	ip_hlen = ip->ip_hl << 2;

	/* check if we're UDP or TCP */
	switch (ip->ip_p) {
	case IPPROTO_TCP:
		th = (struct tcphdr *)((char *)ip + ip_hlen);
		/* src and dst are inverted */
		common.port.dst ^= th->th_sport;
		common.port.src ^= th->th_dport;
		input.formatted.flow_type ^= IXGBE_ATR_FLOW_TYPE_TCPV4;
		break;
	case IPPROTO_UDP:
		uh = (struct udphdr *)((char *)ip + ip_hlen);
		/* src and dst are inverted */
		common.port.dst ^= uh->uh_sport;
		common.port.src ^= uh->uh_dport;
		input.formatted.flow_type ^= IXGBE_ATR_FLOW_TYPE_UDPV4;
		break;
	default:
		return;
	}

	input.formatted.vlan_id = htobe16(mp->m_pkthdr.ether_vtag);
	if (mp->m_pkthdr.ether_vtag)
		common.flex_bytes ^= htons(ETHERTYPE_VLAN);
	else
		common.flex_bytes ^= etype;
	common.ip ^= ip->ip_src.s_addr ^ ip->ip_dst.s_addr;

	que = &adapter->queues[txr->me];
	/*
	** This assumes the Rx queue and Tx
	** queue are bound to the same CPU
	*/
	ixgbe_fdir_add_signature_filter_82599(&adapter->hw,
	    input, common, que->msix);
}
#endif /* IXGBE_FDIR */

/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue.
 *
 **********************************************************************/
static void
ixgbe_txeof(struct tx_ring *txr)
{
	struct adapter		*adapter = txr->adapter;
	struct ifnet		*ifp = adapter->ifp;
	u32			work, processed = 0;
	u16			limit = txr->process_limit;
	struct ixgbe_tx_buf	*buf;
	union ixgbe_adv_tx_desc *txd;
	struct timeval now, elapsed;

	KASSERT(mutex_owned(&txr->tx_mtx));

#ifdef DEV_NETMAP
	if (ifp->if_capenable & IFCAP_NETMAP) {
		struct netmap_adapter *na = NA(ifp);
		struct netmap_kring *kring = &na->tx_rings[txr->me];
		txd = txr->tx_base;
		bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
		    BUS_DMASYNC_POSTREAD);
		/*
		 * In netmap mode, all the work is done in the context
		 * of the client thread. Interrupt handlers only wake up
		 * clients, which may be sleeping on individual rings
		 * or on a global resource for all rings.
		 * To implement tx interrupt mitigation, we wake up the client
		 * thread roughly every half ring, even if the NIC interrupts
		 * more frequently. This is implemented as follows:
		 * - ixgbe_txsync() sets kring->nr_kflags with the index of
		 *   the slot that should wake up the thread (nkr_num_slots
		 *   means the user thread should not be woken up);
		 * - the driver ignores tx interrupts unless netmap_mitigate=0
		 *   or the slot has the DD bit set.
		 */
		if (!netmap_mitigate ||
		    (kring->nr_kflags < kring->nkr_num_slots &&
		    txd[kring->nr_kflags].wb.status & IXGBE_TXD_STAT_DD)) {
			netmap_tx_irq(ifp, txr->me);
		}
		return;
	}
#endif /* DEV_NETMAP */

	if (txr->tx_avail == txr->num_desc) {
		txr->queue_status = IXGBE_QUEUE_IDLE;
		return;
	}

	/* Get work starting point */
	work = txr->next_to_clean;
	buf = &txr->tx_buffers[work];
	txd = &txr->tx_base[work];
	work -= txr->num_desc; /* The distance to ring end */
        ixgbe_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_POSTREAD);
	do {
		union ixgbe_adv_tx_desc *eop= buf->eop;
		if (eop == NULL) /* No work */
			break;

		if ((eop->wb.status & IXGBE_TXD_STAT_DD) == 0)
			break;	/* I/O not complete */

		if (buf->m_head) {
			txr->bytes +=
			    buf->m_head->m_pkthdr.len;
			bus_dmamap_sync(txr->txtag->dt_dmat,
			    buf->map,
			    0, buf->m_head->m_pkthdr.len,
			    BUS_DMASYNC_POSTWRITE);
			ixgbe_dmamap_unload(txr->txtag,
			    buf->map);
			m_freem(buf->m_head);
			buf->m_head = NULL;
			/*
			 * NetBSD: Don't override buf->map with NULL here.
			 * It'll panic when a ring runs one lap around.
			 */
		}
		buf->eop = NULL;
		++txr->tx_avail;

		/* We clean the range if multi segment */
		while (txd != eop) {
			++txd;
			++buf;
			++work;
			/* wrap the ring? */
			if (__predict_false(!work)) {
				work -= txr->num_desc;
				buf = txr->tx_buffers;
				txd = txr->tx_base;
			}
			if (buf->m_head) {
				txr->bytes +=
				    buf->m_head->m_pkthdr.len;
				bus_dmamap_sync(txr->txtag->dt_dmat,
				    buf->map,
				    0, buf->m_head->m_pkthdr.len,
				    BUS_DMASYNC_POSTWRITE);
				ixgbe_dmamap_unload(txr->txtag,
				    buf->map);
				m_freem(buf->m_head);
				buf->m_head = NULL;
				/*
				 * NetBSD: Don't override buf->map with NULL
				 * here. It'll panic when a ring runs one lap
				 * around.
				 */
			}
			++txr->tx_avail;
			buf->eop = NULL;

		}
		++txr->packets;
		++processed;
		++ifp->if_opackets;
		getmicrotime(&txr->watchdog_time);

		/* Try the next packet */
		++txd;
		++buf;
		++work;
		/* reset with a wrap */
		if (__predict_false(!work)) {
			work -= txr->num_desc;
			buf = txr->tx_buffers;
			txd = txr->tx_base;
		}
		prefetch(txd);
	} while (__predict_true(--limit));

	ixgbe_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	work += txr->num_desc;
	txr->next_to_clean = work;

	/*
	** Watchdog calculation, we know there's
	** work outstanding or the first return
	** would have been taken, so none processed
	** for too long indicates a hang.
	*/
	getmicrotime(&now);
	timersub(&now, &txr->watchdog_time, &elapsed);
	if (!processed && tvtohz(&elapsed) > IXGBE_WATCHDOG)
		txr->queue_status = IXGBE_QUEUE_HUNG;

	if (txr->tx_avail == txr->num_desc)
		txr->queue_status = IXGBE_QUEUE_IDLE;

	return;
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
ixgbe_refresh_mbufs(struct rx_ring *rxr, int limit)
{
	struct adapter		*adapter = rxr->adapter;
	struct ixgbe_rx_buf	*rxbuf;
	struct mbuf		*mp;
	int			i, j, error;
	bool			refreshed = false;

	i = j = rxr->next_to_refresh;
	/* Control the loop with one beyond */
	if (++j == rxr->num_desc)
		j = 0;

	while (j != limit) {
		rxbuf = &rxr->rx_buffers[i];
		if (rxbuf->buf == NULL) {
			mp = ixgbe_getjcl(&adapter->jcl_head, M_NOWAIT,
			    MT_DATA, M_PKTHDR, rxr->mbuf_sz);
			if (mp == NULL) {
				rxr->no_jmbuf.ev_count++;
				goto update;
			}
			if (adapter->max_frame_size <= (MCLBYTES - ETHER_ALIGN))
				m_adj(mp, ETHER_ALIGN);
		} else
			mp = rxbuf->buf;

		mp->m_pkthdr.len = mp->m_len = rxr->mbuf_sz;

		/* If we're dealing with an mbuf that was copied rather
		 * than replaced, there's no need to go through busdma.
		 */
		if ((rxbuf->flags & IXGBE_RX_COPY) == 0) {
			/* Get the memory mapping */
			error = bus_dmamap_load_mbuf(rxr->ptag->dt_dmat,
			    rxbuf->pmap, mp, BUS_DMA_NOWAIT);
			if (error != 0) {
				printf("Refresh mbufs: payload dmamap load"
				    " failure - %d\n", error);
				m_free(mp);
				rxbuf->buf = NULL;
				goto update;
			}
			rxbuf->buf = mp;
			bus_dmamap_sync(rxr->ptag->dt_dmat, rxbuf->pmap,
			    0, mp->m_pkthdr.len, BUS_DMASYNC_PREREAD);
			rxbuf->addr = rxr->rx_base[i].read.pkt_addr =
			    htole64(rxbuf->pmap->dm_segs[0].ds_addr);
		} else {
			rxr->rx_base[i].read.pkt_addr = rxbuf->addr;
			rxbuf->flags &= ~IXGBE_RX_COPY;
		}

		refreshed = true;
		/* Next is precalculated */
		i = j;
		rxr->next_to_refresh = i;
		if (++j == rxr->num_desc)
			j = 0;
	}
update:
	if (refreshed) /* Update hardware tail index */
		IXGBE_WRITE_REG(&adapter->hw,
		    IXGBE_RDT(rxr->me), rxr->next_to_refresh);
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
ixgbe_allocate_receive_buffers(struct rx_ring *rxr)
{
	struct	adapter 	*adapter = rxr->adapter;
	device_t 		dev = adapter->dev;
	struct ixgbe_rx_buf 	*rxbuf;
	int             	i, bsize, error;

	bsize = sizeof(struct ixgbe_rx_buf) * rxr->num_desc;
	if (!(rxr->rx_buffers =
	    (struct ixgbe_rx_buf *) malloc(bsize,
	    M_DEVBUF, M_NOWAIT | M_ZERO))) {
		aprint_error_dev(dev, "Unable to allocate rx_buffer memory\n");
		error = ENOMEM;
		goto fail;
	}

	if ((error = ixgbe_dma_tag_create(adapter->osdep.dmat,	/* parent */
				   1, 0,	/* alignment, bounds */
				   MJUM16BYTES,		/* maxsize */
				   1,			/* nsegments */
				   MJUM16BYTES,		/* maxsegsize */
				   0,			/* flags */
				   &rxr->ptag))) {
		aprint_error_dev(dev, "Unable to create RX DMA tag\n");
		goto fail;
	}

	for (i = 0; i < rxr->num_desc; i++, rxbuf++) {
		rxbuf = &rxr->rx_buffers[i];
		error = ixgbe_dmamap_create(rxr->ptag,
		    BUS_DMA_NOWAIT, &rxbuf->pmap);
		if (error) {
			aprint_error_dev(dev, "Unable to create RX dma map\n");
			goto fail;
		}
	}

	return (0);

fail:
	/* Frees all, but can handle partial completion */
	ixgbe_free_receive_structures(adapter);
	return (error);
}

/*
** Used to detect a descriptor that has
** been merged by Hardware RSC.
*/
static inline u32
ixgbe_rsc_count(union ixgbe_adv_rx_desc *rx)
{
	return (le32toh(rx->wb.lower.lo_dword.data) &
	    IXGBE_RXDADV_RSCCNT_MASK) >> IXGBE_RXDADV_RSCCNT_SHIFT;
}

/*********************************************************************
 *
 *  Initialize Hardware RSC (LRO) feature on 82599
 *  for an RX ring, this is toggled by the LRO capability
 *  even though it is transparent to the stack.
 *
 *  NOTE: since this HW feature only works with IPV4 and 
 *        our testing has shown soft LRO to be as effective
 *        I have decided to disable this by default.
 *
 **********************************************************************/
static void
ixgbe_setup_hw_rsc(struct rx_ring *rxr)
{
	struct	adapter 	*adapter = rxr->adapter;
	struct	ixgbe_hw	*hw = &adapter->hw;
	u32			rscctrl, rdrxctl;

	/* If turning LRO/RSC off we need to disable it */
	if ((adapter->ifp->if_capenable & IFCAP_LRO) == 0) {
		rscctrl = IXGBE_READ_REG(hw, IXGBE_RSCCTL(rxr->me));
		rscctrl &= ~IXGBE_RSCCTL_RSCEN;
		return;
	}

	rdrxctl = IXGBE_READ_REG(hw, IXGBE_RDRXCTL);
	rdrxctl &= ~IXGBE_RDRXCTL_RSCFRSTSIZE;
#ifdef DEV_NETMAP /* crcstrip is optional in netmap */
	if (adapter->ifp->if_capenable & IFCAP_NETMAP && !ix_crcstrip)
#endif /* DEV_NETMAP */
	rdrxctl |= IXGBE_RDRXCTL_CRCSTRIP;
	rdrxctl |= IXGBE_RDRXCTL_RSCACKC;
	IXGBE_WRITE_REG(hw, IXGBE_RDRXCTL, rdrxctl);

	rscctrl = IXGBE_READ_REG(hw, IXGBE_RSCCTL(rxr->me));
	rscctrl |= IXGBE_RSCCTL_RSCEN;
	/*
	** Limit the total number of descriptors that
	** can be combined, so it does not exceed 64K
	*/
	if (rxr->mbuf_sz == MCLBYTES)
		rscctrl |= IXGBE_RSCCTL_MAXDESC_16;
	else if (rxr->mbuf_sz == MJUMPAGESIZE)
		rscctrl |= IXGBE_RSCCTL_MAXDESC_8;
	else if (rxr->mbuf_sz == MJUM9BYTES)
		rscctrl |= IXGBE_RSCCTL_MAXDESC_4;
	else  /* Using 16K cluster */
		rscctrl |= IXGBE_RSCCTL_MAXDESC_1;

	IXGBE_WRITE_REG(hw, IXGBE_RSCCTL(rxr->me), rscctrl);

	/* Enable TCP header recognition */
	IXGBE_WRITE_REG(hw, IXGBE_PSRTYPE(0),
	    (IXGBE_READ_REG(hw, IXGBE_PSRTYPE(0)) |
	    IXGBE_PSRTYPE_TCPHDR));

	/* Disable RSC for ACK packets */
	IXGBE_WRITE_REG(hw, IXGBE_RSCDBU,
	    (IXGBE_RSCDBU_RSCACKDIS | IXGBE_READ_REG(hw, IXGBE_RSCDBU)));

	rxr->hw_rsc = TRUE;
}


static void     
ixgbe_free_receive_ring(struct rx_ring *rxr)
{ 
	struct ixgbe_rx_buf       *rxbuf;
	int i;

	for (i = 0; i < rxr->num_desc; i++) {
		rxbuf = &rxr->rx_buffers[i];
		if (rxbuf->buf != NULL) {
			bus_dmamap_sync(rxr->ptag->dt_dmat, rxbuf->pmap,
			    0, rxbuf->buf->m_pkthdr.len,
			    BUS_DMASYNC_POSTREAD);
			ixgbe_dmamap_unload(rxr->ptag, rxbuf->pmap);
			rxbuf->buf->m_flags |= M_PKTHDR;
			m_freem(rxbuf->buf);
			rxbuf->buf = NULL;
			rxbuf->flags = 0;
		}
	}
}


/*********************************************************************
 *
 *  Initialize a receive ring and its buffers.
 *
 **********************************************************************/
static int
ixgbe_setup_receive_ring(struct rx_ring *rxr)
{
	struct	adapter 	*adapter;
	struct ixgbe_rx_buf	*rxbuf;
#ifdef LRO
	struct ifnet		*ifp;
	struct lro_ctrl		*lro = &rxr->lro;
#endif /* LRO */
	int			rsize, error = 0;
#ifdef DEV_NETMAP
	struct netmap_adapter *na = NA(rxr->adapter->ifp);
	struct netmap_slot *slot;
#endif /* DEV_NETMAP */

	adapter = rxr->adapter;
#ifdef LRO
	ifp = adapter->ifp;
#endif /* LRO */

	/* Clear the ring contents */
	IXGBE_RX_LOCK(rxr);
#ifdef DEV_NETMAP
	/* same as in ixgbe_setup_transmit_ring() */
	slot = netmap_reset(na, NR_RX, rxr->me, 0);
#endif /* DEV_NETMAP */
	rsize = roundup2(adapter->num_rx_desc *
	    sizeof(union ixgbe_adv_rx_desc), DBA_ALIGN);
	bzero((void *)rxr->rx_base, rsize);
	/* Cache the size */
	rxr->mbuf_sz = adapter->rx_mbuf_sz;

	/* Free current RX buffer structs and their mbufs */
	ixgbe_free_receive_ring(rxr);

	IXGBE_RX_UNLOCK(rxr);

	/* Now reinitialize our supply of jumbo mbufs.  The number
	 * or size of jumbo mbufs may have changed.
	 */
	ixgbe_jcl_reinit(&adapter->jcl_head, rxr->ptag->dt_dmat,
	    2 * adapter->num_rx_desc, adapter->rx_mbuf_sz);

	IXGBE_RX_LOCK(rxr);

	/* Now replenish the mbufs */
	for (int j = 0; j != rxr->num_desc; ++j) {
		struct mbuf	*mp;

		rxbuf = &rxr->rx_buffers[j];
#ifdef DEV_NETMAP
		/*
		 * In netmap mode, fill the map and set the buffer
		 * address in the NIC ring, considering the offset
		 * between the netmap and NIC rings (see comment in
		 * ixgbe_setup_transmit_ring() ). No need to allocate
		 * an mbuf, so end the block with a continue;
		 */
		if (slot) {
			int sj = netmap_idx_n2k(&na->rx_rings[rxr->me], j);
			uint64_t paddr;
			void *addr;

			addr = PNMB(na, slot + sj, &paddr);
			netmap_load_map(na, rxr->ptag, rxbuf->pmap, addr);
			/* Update descriptor and the cached value */
			rxr->rx_base[j].read.pkt_addr = htole64(paddr);
			rxbuf->addr = htole64(paddr);
			continue;
		}
#endif /* DEV_NETMAP */
		rxbuf->flags = 0; 
		rxbuf->buf = ixgbe_getjcl(&adapter->jcl_head, M_NOWAIT,
		    MT_DATA, M_PKTHDR, adapter->rx_mbuf_sz);
		if (rxbuf->buf == NULL) {
			error = ENOBUFS;
                        goto fail;
		}
		mp = rxbuf->buf;
		mp->m_pkthdr.len = mp->m_len = rxr->mbuf_sz;
		/* Get the memory mapping */
		error = bus_dmamap_load_mbuf(rxr->ptag->dt_dmat,
		    rxbuf->pmap, mp, BUS_DMA_NOWAIT);
		if (error != 0)
                        goto fail;
		bus_dmamap_sync(rxr->ptag->dt_dmat, rxbuf->pmap,
		    0, adapter->rx_mbuf_sz, BUS_DMASYNC_PREREAD);
		/* Update the descriptor and the cached value */
		rxr->rx_base[j].read.pkt_addr =
		    htole64(rxbuf->pmap->dm_segs[0].ds_addr);
		rxbuf->addr = htole64(rxbuf->pmap->dm_segs[0].ds_addr);
	}


	/* Setup our descriptor indices */
	rxr->next_to_check = 0;
	rxr->next_to_refresh = 0;
	rxr->lro_enabled = FALSE;
	rxr->rx_copies.ev_count = 0;
	rxr->rx_bytes.ev_count = 0;
	rxr->vtag_strip = FALSE;

	ixgbe_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/*
	** Now set up the LRO interface:
	*/
	if (ixgbe_rsc_enable)
		ixgbe_setup_hw_rsc(rxr);
#ifdef LRO
	else if (ifp->if_capenable & IFCAP_LRO) {
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

	IXGBE_RX_UNLOCK(rxr);
	return (0);

fail:
	ixgbe_free_receive_ring(rxr);
	IXGBE_RX_UNLOCK(rxr);
	return (error);
}

/*********************************************************************
 *
 *  Initialize all receive rings.
 *
 **********************************************************************/
static int
ixgbe_setup_receive_structures(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;
	int j;

	for (j = 0; j < adapter->num_queues; j++, rxr++)
		if (ixgbe_setup_receive_ring(rxr))
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
		ixgbe_free_receive_ring(rxr);
	}

	return (ENOBUFS);
}

static void
ixgbe_initialise_rss_mapping(struct adapter *adapter)
{
	struct ixgbe_hw	*hw = &adapter->hw;
	uint32_t reta;
	int i, j, queue_id;
	uint32_t rss_key[10];
	uint32_t mrqc;
#ifdef	RSS
	uint32_t rss_hash_config;
#endif

	/* Setup RSS */
	reta = 0;

#ifdef	RSS
	/* Fetch the configured RSS key */
	rss_getkey((uint8_t *) &rss_key);
#else
	/* set up random bits */
	cprng_fast(&rss_key, sizeof(rss_key));
#endif

	/* Set up the redirection table */
	for (i = 0, j = 0; i < 128; i++, j++) {
		if (j == adapter->num_queues) j = 0;
#ifdef	RSS
		/*
		 * Fetch the RSS bucket id for the given indirection entry.
		 * Cap it at the number of configured buckets (which is
		 * num_queues.)
		 */
		queue_id = rss_get_indirection_to_bucket(i);
		queue_id = queue_id % adapter->num_queues;
#else
		queue_id = (j * 0x11);
#endif
		/*
		 * The low 8 bits are for hash value (n+0);
		 * The next 8 bits are for hash value (n+1), etc.
		 */
		reta = reta >> 8;
		reta = reta | ( ((uint32_t) queue_id) << 24);
		if ((i & 3) == 3) {
			IXGBE_WRITE_REG(hw, IXGBE_RETA(i >> 2), reta);
			reta = 0;
		}
	}

	/* Now fill our hash function seeds */
	for (i = 0; i < 10; i++)
		IXGBE_WRITE_REG(hw, IXGBE_RSSRK(i), rss_key[i]);

	/* Perform hash on these packet types */
#ifdef	RSS
	mrqc = IXGBE_MRQC_RSSEN;
	rss_hash_config = rss_gethashconfig();
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV4)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4;
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV4)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4_TCP;
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV6)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6;
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV6)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_TCP;
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV6_EX)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_EX;
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV6_EX)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_EX_TCP;
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV4)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4_UDP;
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV4_EX)
		device_printf(adapter->dev,
		    "%s: RSS_HASHTYPE_RSS_UDP_IPV4_EX defined, "
		    "but not supported\n", __func__);
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV6)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_UDP;
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV6_EX)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_EX_UDP;
#else
	/*
	 * Disable UDP - IP fragments aren't currently being handled
	 * and so we end up with a mix of 2-tuple and 4-tuple
	 * traffic.
	 */
	mrqc = IXGBE_MRQC_RSSEN
	     | IXGBE_MRQC_RSS_FIELD_IPV4
	     | IXGBE_MRQC_RSS_FIELD_IPV4_TCP
#if 0
	     | IXGBE_MRQC_RSS_FIELD_IPV4_UDP
#endif
	     | IXGBE_MRQC_RSS_FIELD_IPV6_EX_TCP
	     | IXGBE_MRQC_RSS_FIELD_IPV6_EX
	     | IXGBE_MRQC_RSS_FIELD_IPV6
	     | IXGBE_MRQC_RSS_FIELD_IPV6_TCP
#if 0
	     | IXGBE_MRQC_RSS_FIELD_IPV6_UDP
	     | IXGBE_MRQC_RSS_FIELD_IPV6_EX_UDP
#endif
	;
#endif /* RSS */
	IXGBE_WRITE_REG(hw, IXGBE_MRQC, mrqc);
}


/*********************************************************************
 *
 *  Setup receive registers and features.
 *
 **********************************************************************/
#define IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT 2

#define BSIZEPKT_ROUNDUP ((1<<IXGBE_SRRCTL_BSIZEPKT_SHIFT)-1)
	
static void
ixgbe_initialize_receive_units(struct adapter *adapter)
{
	int i;
	struct	rx_ring	*rxr = adapter->rx_rings;
	struct ixgbe_hw	*hw = &adapter->hw;
	struct ifnet   *ifp = adapter->ifp;
	u32		bufsz, rxctrl, fctrl, srrctl, rxcsum;
	u32		hlreg;


	/*
	 * Make sure receives are disabled while
	 * setting up the descriptor ring
	 */
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL,
	    rxctrl & ~IXGBE_RXCTRL_RXEN);

	/* Enable broadcasts */
	fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);
	fctrl |= IXGBE_FCTRL_BAM;
	fctrl |= IXGBE_FCTRL_DPF;
	fctrl |= IXGBE_FCTRL_PMCF;
	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);

	/* Set for Jumbo Frames? */
	hlreg = IXGBE_READ_REG(hw, IXGBE_HLREG0);
	if (ifp->if_mtu > ETHERMTU)
		hlreg |= IXGBE_HLREG0_JUMBOEN;
	else
		hlreg &= ~IXGBE_HLREG0_JUMBOEN;
#ifdef DEV_NETMAP
	/* crcstrip is conditional in netmap (in RDRXCTL too ?) */
	if (ifp->if_capenable & IFCAP_NETMAP && !ix_crcstrip)
		hlreg &= ~IXGBE_HLREG0_RXCRCSTRP;
	else
		hlreg |= IXGBE_HLREG0_RXCRCSTRP;
#endif /* DEV_NETMAP */
	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg);

	bufsz = (adapter->rx_mbuf_sz +
	    BSIZEPKT_ROUNDUP) >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;

	for (i = 0; i < adapter->num_queues; i++, rxr++) {
		u64 rdba = rxr->rxdma.dma_paddr;

		/* Setup the Base and Length of the Rx Descriptor Ring */
		IXGBE_WRITE_REG(hw, IXGBE_RDBAL(i),
			       (rdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_RDBAH(i), (rdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_RDLEN(i),
		    adapter->num_rx_desc * sizeof(union ixgbe_adv_rx_desc));

		/* Set up the SRRCTL register */
		srrctl = IXGBE_READ_REG(hw, IXGBE_SRRCTL(i));
		srrctl &= ~IXGBE_SRRCTL_BSIZEHDR_MASK;
		srrctl &= ~IXGBE_SRRCTL_BSIZEPKT_MASK;
		srrctl |= bufsz;
		srrctl |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;

		/*
		 * Set DROP_EN iff we have no flow control and >1 queue.
		 * Note that srrctl was cleared shortly before during reset,
		 * so we do not need to clear the bit, but do it just in case
		 * this code is moved elsewhere.
		 */
		if (adapter->num_queues > 1 &&
		    adapter->fc == ixgbe_fc_none) {
			srrctl |= IXGBE_SRRCTL_DROP_EN;
		} else {
			srrctl &= ~IXGBE_SRRCTL_DROP_EN;
		}

		IXGBE_WRITE_REG(hw, IXGBE_SRRCTL(i), srrctl);

		/* Setup the HW Rx Head and Tail Descriptor Pointers */
		IXGBE_WRITE_REG(hw, IXGBE_RDH(i), 0);
		IXGBE_WRITE_REG(hw, IXGBE_RDT(i), 0);

		/* Set the processing limit */
		rxr->process_limit = ixgbe_rx_process_limit;
	}

	if (adapter->hw.mac.type != ixgbe_mac_82598EB) {
		u32 psrtype = IXGBE_PSRTYPE_TCPHDR |
			      IXGBE_PSRTYPE_UDPHDR |
			      IXGBE_PSRTYPE_IPV4HDR |
			      IXGBE_PSRTYPE_IPV6HDR;
		IXGBE_WRITE_REG(hw, IXGBE_PSRTYPE(0), psrtype);
	}

	rxcsum = IXGBE_READ_REG(hw, IXGBE_RXCSUM);

	ixgbe_initialise_rss_mapping(adapter);

	if (adapter->num_queues > 1) {
		/* RSS and RX IPP Checksum are mutually exclusive */
		rxcsum |= IXGBE_RXCSUM_PCSD;
	}

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
ixgbe_free_receive_structures(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;

	INIT_DEBUGOUT("ixgbe_free_receive_structures: begin");

	for (int i = 0; i < adapter->num_queues; i++, rxr++) {
#ifdef LRO
		struct lro_ctrl		*lro = &rxr->lro;
#endif /* LRO */
		ixgbe_free_receive_buffers(rxr);
#ifdef LRO
		/* Free LRO memory */
		tcp_lro_free(lro);
#endif /* LRO */
		/* Free the ring memory as well */
		ixgbe_dma_free(adapter, &rxr->rxdma);
		IXGBE_RX_LOCK_DESTROY(rxr);
	}

	free(adapter->rx_rings, M_DEVBUF);
}


/*********************************************************************
 *
 *  Free receive ring data structures
 *
 **********************************************************************/
static void
ixgbe_free_receive_buffers(struct rx_ring *rxr)
{
	struct adapter		*adapter = rxr->adapter;
	struct ixgbe_rx_buf	*rxbuf;

	INIT_DEBUGOUT("ixgbe_free_receive_buffers: begin");

	/* Cleanup any existing buffers */
	if (rxr->rx_buffers != NULL) {
		for (int i = 0; i < adapter->num_rx_desc; i++) {
			rxbuf = &rxr->rx_buffers[i];
			if (rxbuf->buf != NULL) {
				bus_dmamap_sync(rxr->ptag->dt_dmat,
				    rxbuf->pmap, 0, rxbuf->buf->m_pkthdr.len,
				    BUS_DMASYNC_POSTREAD);
				ixgbe_dmamap_unload(rxr->ptag, rxbuf->pmap);
				rxbuf->buf->m_flags |= M_PKTHDR;
				m_freem(rxbuf->buf);
			}
			rxbuf->buf = NULL;
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

	if (rxr->ptag != NULL) {
		ixgbe_dma_tag_destroy(rxr->ptag);
		rxr->ptag = NULL;
	}

	return;
}

static __inline void
ixgbe_rx_input(struct rx_ring *rxr, struct ifnet *ifp, struct mbuf *m, u32 ptype)
{
	int s;

#ifdef LRO
	struct adapter	*adapter = ifp->if_softc;
	struct ethercom *ec = &adapter->osdep.ec;

        /*
         * ATM LRO is only for IP/TCP packets and TCP checksum of the packet
         * should be computed by hardware. Also it should not have VLAN tag in
         * ethernet header.  In case of IPv6 we do not yet support ext. hdrs.
         */
        if (rxr->lro_enabled &&
            (ec->ec_capenable & ETHERCAP_VLAN_HWTAGGING) != 0 &&
            (ptype & IXGBE_RXDADV_PKTTYPE_ETQF) == 0 &&
            ((ptype & (IXGBE_RXDADV_PKTTYPE_IPV4 | IXGBE_RXDADV_PKTTYPE_TCP)) ==
            (IXGBE_RXDADV_PKTTYPE_IPV4 | IXGBE_RXDADV_PKTTYPE_TCP) ||
            (ptype & (IXGBE_RXDADV_PKTTYPE_IPV6 | IXGBE_RXDADV_PKTTYPE_TCP)) ==
            (IXGBE_RXDADV_PKTTYPE_IPV6 | IXGBE_RXDADV_PKTTYPE_TCP)) &&
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

	IXGBE_RX_UNLOCK(rxr);

	s = splnet();
	/* Pass this up to any BPF listeners. */
	bpf_mtap(ifp, m);
	(*ifp->if_input)(ifp, m);
	splx(s);

	IXGBE_RX_LOCK(rxr);
}

static __inline void
ixgbe_rx_discard(struct rx_ring *rxr, int i)
{
	struct ixgbe_rx_buf	*rbuf;

	rbuf = &rxr->rx_buffers[i];


	/*
	** With advanced descriptors the writeback
	** clobbers the buffer addrs, so its easier
	** to just free the existing mbufs and take
	** the normal refresh path to get new buffers
	** and mapping.
	*/

	if (rbuf->buf != NULL) {/* Partial chain ? */
		rbuf->fmp->m_flags |= M_PKTHDR;
		m_freem(rbuf->fmp);
		rbuf->fmp = NULL;
		rbuf->buf = NULL; /* rbuf->buf is part of fmp's chain */
	} else if (rbuf->buf) {
		m_free(rbuf->buf);
		rbuf->buf = NULL;
	}

	rbuf->flags = 0;

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
ixgbe_rxeof(struct ix_queue *que)
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
	u16			count = rxr->process_limit;
	union ixgbe_adv_rx_desc	*cur;
	struct ixgbe_rx_buf	*rbuf, *nbuf;
#ifdef RSS
	u16			pkt_info;
#endif

	IXGBE_RX_LOCK(rxr);

#ifdef DEV_NETMAP
	/* Same as the txeof routine: wakeup clients on intr. */
	if (netmap_rx_irq(ifp, rxr->me, &processed)) {
		IXGBE_RX_UNLOCK(rxr);
		return (FALSE);
	}
#endif /* DEV_NETMAP */

	for (i = rxr->next_to_check; count != 0;) {
		struct mbuf	*sendmp, *mp;
		u32		rsc, ptype;
		u16		len;
		u16		vtag = 0;
		bool		eop;
 
		/* Sync the ring. */
		ixgbe_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		cur = &rxr->rx_base[i];
		staterr = le32toh(cur->wb.upper.status_error);
#ifdef RSS
		pkt_info = le16toh(cur->wb.lower.lo_dword.hs_rss.pkt_info);
#endif

		if ((staterr & IXGBE_RXD_STAT_DD) == 0)
			break;
		if ((ifp->if_flags & IFF_RUNNING) == 0)
			break;

		count--;
		sendmp = NULL;
		nbuf = NULL;
		rsc = 0;
		cur->wb.upper.status_error = 0;
		rbuf = &rxr->rx_buffers[i];
		mp = rbuf->buf;

		len = le16toh(cur->wb.upper.length);
		ptype = le32toh(cur->wb.lower.lo_dword.data) &
		    IXGBE_RXDADV_PKTTYPE_MASK;
		eop = ((staterr & IXGBE_RXD_STAT_EOP) != 0);

		/* Make sure bad packets are discarded */
		if (eop && (staterr & IXGBE_RXDADV_ERR_FRAME_ERR_MASK) != 0) {
			rxr->rx_discarded.ev_count++;
			ixgbe_rx_discard(rxr, i);
			goto next_desc;
		}

		/*
		** On 82599 which supports a hardware
		** LRO (called HW RSC), packets need
		** not be fragmented across sequential
		** descriptors, rather the next descriptor
		** is indicated in bits of the descriptor.
		** This also means that we might proceses
		** more than one packet at a time, something
		** that has never been true before, it
		** required eliminating global chain pointers
		** in favor of what we are doing here.  -jfv
		*/
		if (!eop) {
			/*
			** Figure out the next descriptor
			** of this frame.
			*/
			if (rxr->hw_rsc == TRUE) {
				rsc = ixgbe_rsc_count(cur);
				rxr->rsc_num += (rsc - 1);
			}
			if (rsc) { /* Get hardware index */
				nextp = ((staterr &
				    IXGBE_RXDADV_NEXTP_MASK) >>
				    IXGBE_RXDADV_NEXTP_SHIFT);
			} else { /* Just sequential */
				nextp = i + 1;
				if (nextp == adapter->num_rx_desc)
					nextp = 0;
			}
			nbuf = &rxr->rx_buffers[nextp];
			prefetch(nbuf);
		}
		/*
		** Rather than using the fmp/lmp global pointers
		** we now keep the head of a packet chain in the
		** buffer struct and pass this along from one
		** descriptor to the next, until we get EOP.
		*/
		mp->m_len = len;
		/*
		** See if there is a stored head
		** that determines what we are
		*/
		sendmp = rbuf->fmp;
		if (sendmp != NULL) {  /* secondary frag */
			rbuf->buf = rbuf->fmp = NULL;
			mp->m_flags &= ~M_PKTHDR;
			sendmp->m_pkthdr.len += mp->m_len;
		} else {
			/*
			 * Optimize.  This might be a small packet,
			 * maybe just a TCP ACK.  Do a fast copy that
			 * is cache aligned into a new mbuf, and
			 * leave the old mbuf+cluster for re-use.
			 */
			if (eop && len <= IXGBE_RX_COPY_LEN) {
				sendmp = m_gethdr(M_NOWAIT, MT_DATA);
				if (sendmp != NULL) {
					sendmp->m_data +=
					    IXGBE_RX_COPY_ALIGN;
					ixgbe_bcopy(mp->m_data,
					    sendmp->m_data, len);
					sendmp->m_len = len;
					rxr->rx_copies.ev_count++;
					rbuf->flags |= IXGBE_RX_COPY;
				}
			}
			if (sendmp == NULL) {
				rbuf->buf = rbuf->fmp = NULL;
				sendmp = mp;
			}

			/* first desc of a non-ps chain */
			sendmp->m_flags |= M_PKTHDR;
			sendmp->m_pkthdr.len = mp->m_len;
		}
		++processed;

		/* Pass the head pointer on */
		if (eop == 0) {
			nbuf->fmp = sendmp;
			sendmp = NULL;
			mp->m_next = nbuf->buf;
		} else { /* Sending this frame */
			sendmp->m_pkthdr.rcvif = ifp;
			ifp->if_ipackets++;
			rxr->rx_packets.ev_count++;
			/* capture data for AIM */
			rxr->bytes += sendmp->m_pkthdr.len;
			rxr->rx_bytes.ev_count += sendmp->m_pkthdr.len;
			/* Process vlan info */
			if ((rxr->vtag_strip) &&
			    (staterr & IXGBE_RXD_STAT_VP))
				vtag = le16toh(cur->wb.upper.vlan);
			if (vtag) {
				VLAN_INPUT_TAG(ifp, sendmp, vtag,
				    printf("%s: could not apply VLAN "
					"tag", __func__));
			}
			if ((ifp->if_capenable & IFCAP_RXCSUM) != 0) {
				ixgbe_rx_checksum(staterr, sendmp, ptype,
				   &adapter->stats);
			}
#if __FreeBSD_version >= 800000
#ifdef RSS
			sendmp->m_pkthdr.flowid =
			    le32toh(cur->wb.lower.hi_dword.rss);
			switch (pkt_info & IXGBE_RXDADV_RSSTYPE_MASK) {
			case IXGBE_RXDADV_RSSTYPE_IPV4_TCP:
				M_HASHTYPE_SET(sendmp, M_HASHTYPE_RSS_TCP_IPV4);
				break;
			case IXGBE_RXDADV_RSSTYPE_IPV4:
				M_HASHTYPE_SET(sendmp, M_HASHTYPE_RSS_IPV4);
				break;
			case IXGBE_RXDADV_RSSTYPE_IPV6_TCP:
				M_HASHTYPE_SET(sendmp, M_HASHTYPE_RSS_TCP_IPV6);
				break;
			case IXGBE_RXDADV_RSSTYPE_IPV6_EX:
				M_HASHTYPE_SET(sendmp, M_HASHTYPE_RSS_IPV6_EX);
				break;
			case IXGBE_RXDADV_RSSTYPE_IPV6:
				M_HASHTYPE_SET(sendmp, M_HASHTYPE_RSS_IPV6);
				break;
			case IXGBE_RXDADV_RSSTYPE_IPV6_TCP_EX:
				M_HASHTYPE_SET(sendmp, M_HASHTYPE_RSS_TCP_IPV6_EX);
				break;
			case IXGBE_RXDADV_RSSTYPE_IPV4_UDP:
				M_HASHTYPE_SET(sendmp, M_HASHTYPE_RSS_UDP_IPV4);
				break;
			case IXGBE_RXDADV_RSSTYPE_IPV6_UDP:
				M_HASHTYPE_SET(sendmp, M_HASHTYPE_RSS_UDP_IPV6);
				break;
			case IXGBE_RXDADV_RSSTYPE_IPV6_UDP_EX:
				M_HASHTYPE_SET(sendmp, M_HASHTYPE_RSS_UDP_IPV6_EX);
				break;
			default:
				/* XXX fallthrough */
				M_HASHTYPE_SET(sendmp, M_HASHTYPE_OPAQUE);
				break;
			}
#else /* RSS */
			sendmp->m_pkthdr.flowid = que->msix;
			M_HASHTYPE_SET(sendmp, M_HASHTYPE_OPAQUE);
#endif /* RSS */
#endif /* FreeBSD_version */
		}
next_desc:
		ixgbe_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* Advance our pointers to the next descriptor. */
		if (++i == rxr->num_desc)
			i = 0;

		/* Now send to the stack or do LRO */
		if (sendmp != NULL) {
			rxr->next_to_check = i;
			ixgbe_rx_input(rxr, ifp, sendmp, ptype);
			i = rxr->next_to_check;
		}

               /* Every 8 descriptors we go to refresh mbufs */
		if (processed == 8) {
			ixgbe_refresh_mbufs(rxr, i);
			processed = 0;
		}
	}

	/* Refresh any remaining buf structs */
	if (ixgbe_rx_unrefreshed(rxr))
		ixgbe_refresh_mbufs(rxr, i);

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

	IXGBE_RX_UNLOCK(rxr);

	/*
	** Still have cleaning to do?
	*/
	if ((staterr & IXGBE_RXD_STAT_DD) != 0)
		return true;
	else
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
ixgbe_rx_checksum(u32 staterr, struct mbuf * mp, u32 ptype,
    struct ixgbe_hw_stats *stats)
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
			mp->m_pkthdr.csum_flags = M_CSUM_IPv4;

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


#if 0	/* XXX Badly need to overhaul vlan(4) on NetBSD. */
/*
** This routine is run via an vlan config EVENT,
** it enables us to use the HW Filter table since
** we can get the vlan id. This just creates the
** entry in the soft version of the VFTA, init will
** repopulate the real table.
*/
static void
ixgbe_register_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u16		index, bit;

	if (ifp->if_softc !=  arg)   /* Not our event */
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	IXGBE_CORE_LOCK(adapter);
	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	adapter->shadow_vfta[index] |= (1 << bit);
	ixgbe_setup_vlan_hw_support(adapter);
	IXGBE_CORE_UNLOCK(adapter);
}

/*
** This routine is run via an vlan
** unconfig EVENT, remove our entry
** in the soft vfta.
*/
static void
ixgbe_unregister_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u16		index, bit;

	if (ifp->if_softc !=  arg)
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	IXGBE_CORE_LOCK(adapter);
	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	adapter->shadow_vfta[index] &= ~(1 << bit);
	/* Re-init to load the changes */
	ixgbe_setup_vlan_hw_support(adapter);
	IXGBE_CORE_UNLOCK(adapter);
}
#endif

static void
ixgbe_setup_vlan_hw_support(struct adapter *adapter)
{
	struct ethercom *ec = &adapter->osdep.ec;
	struct ixgbe_hw *hw = &adapter->hw;
	struct rx_ring	*rxr;
	u32		ctrl;


	/*
	** We get here thru init_locked, meaning
	** a soft reset, this has already cleared
	** the VFTA and other state, so if there
	** have been no vlan's registered do nothing.
	*/
	if (!VLAN_ATTACHED(&adapter->osdep.ec)) {
		return;
	}

	/* Setup the queues for vlans */
	for (int i = 0; i < adapter->num_queues; i++) {
		rxr = &adapter->rx_rings[i];
		/* On 82599 the VLAN enable is per/queue in RXDCTL */
		if (hw->mac.type != ixgbe_mac_82598EB) {
			ctrl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(i));
			ctrl |= IXGBE_RXDCTL_VME;
			IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(i), ctrl);
		}
		rxr->vtag_strip = TRUE;
	}

	if ((ec->ec_capenable & ETHERCAP_VLAN_HWFILTER) == 0)
		return;
	/*
	** A soft reset zero's out the VFTA, so
	** we need to repopulate it now.
	*/
	for (int i = 0; i < IXGBE_VFTA_SIZE; i++)
		if (adapter->shadow_vfta[i] != 0)
			IXGBE_WRITE_REG(hw, IXGBE_VFTA(i),
			    adapter->shadow_vfta[i]);

	ctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);
	/* Enable the Filter Table if enabled */
	if (ec->ec_capenable & ETHERCAP_VLAN_HWFILTER) {
		ctrl &= ~IXGBE_VLNCTRL_CFIEN;
		ctrl |= IXGBE_VLNCTRL_VFE;
	}
	if (hw->mac.type == ixgbe_mac_82598EB)
		ctrl |= IXGBE_VLNCTRL_VME;
	IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, ctrl);
}

static void
ixgbe_enable_intr(struct adapter *adapter)
{
	struct ixgbe_hw	*hw = &adapter->hw;
	struct ix_queue	*que = adapter->queues;
	u32		mask, fwsm;

	mask = (IXGBE_EIMS_ENABLE_MASK & ~IXGBE_EIMS_RTX_QUEUE);
	/* Enable Fan Failure detection */
	if (hw->device_id == IXGBE_DEV_ID_82598AT)
		    mask |= IXGBE_EIMS_GPI_SDP1;

	switch (adapter->hw.mac.type) {
		case ixgbe_mac_82599EB:
			mask |= IXGBE_EIMS_ECC;
			mask |= IXGBE_EIMS_GPI_SDP0;
			mask |= IXGBE_EIMS_GPI_SDP1;
			mask |= IXGBE_EIMS_GPI_SDP2;
#ifdef IXGBE_FDIR
			mask |= IXGBE_EIMS_FLOW_DIR;
#endif
			break;
		case ixgbe_mac_X540:
			mask |= IXGBE_EIMS_ECC;
			/* Detect if Thermal Sensor is enabled */
			fwsm = IXGBE_READ_REG(hw, IXGBE_FWSM);
			if (fwsm & IXGBE_FWSM_TS_ENABLED)
				mask |= IXGBE_EIMS_TS;
#ifdef IXGBE_FDIR
			mask |= IXGBE_EIMS_FLOW_DIR;
#endif
		/* falls through */
		default:
			break;
	}

	IXGBE_WRITE_REG(hw, IXGBE_EIMS, mask);

	/* With RSS we use auto clear */
	if (adapter->msix_mem) {
		mask = IXGBE_EIMS_ENABLE_MASK;
		/* Don't autoclear Link */
		mask &= ~IXGBE_EIMS_OTHER;
		mask &= ~IXGBE_EIMS_LSC;
		IXGBE_WRITE_REG(hw, IXGBE_EIAC, mask);
	}

	/*
	** Now enable all queues, this is done separately to
	** allow for handling the extended (beyond 32) MSIX
	** vectors that can be used by 82599
	*/
        for (int i = 0; i < adapter->num_queues; i++, que++)
                ixgbe_enable_queue(adapter, que->msix);

	IXGBE_WRITE_FLUSH(hw);

	return;
}

static void
ixgbe_disable_intr(struct adapter *adapter)
{
	if (adapter->msix_mem)
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIAC, 0);
	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC, ~0);
	} else {
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC, 0xFFFF0000);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC_EX(0), ~0);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC_EX(1), ~0);
	}
	IXGBE_WRITE_FLUSH(&adapter->hw);
	return;
}

u16
ixgbe_read_pci_cfg(struct ixgbe_hw *hw, u32 reg)
{
	switch (reg % 4) {
	case 0:
		return pci_conf_read(hw->back->pc, hw->back->tag, reg) &
		    __BITS(15, 0);
	case 2:
		return __SHIFTOUT(pci_conf_read(hw->back->pc, hw->back->tag,
		    reg - 2), __BITS(31, 16));
	default:
		panic("%s: invalid register (%" PRIx32, __func__, reg); 
		break;
	}
}

void
ixgbe_write_pci_cfg(struct ixgbe_hw *hw, u32 reg, u16 value)
{
	pcireg_t old;

	switch (reg % 4) {
	case 0:
		old = pci_conf_read(hw->back->pc, hw->back->tag, reg) &
		    __BITS(31, 16);
		pci_conf_write(hw->back->pc, hw->back->tag, reg, value | old);
		break;
	case 2:
		old = pci_conf_read(hw->back->pc, hw->back->tag, reg - 2) &
		    __BITS(15, 0);
		pci_conf_write(hw->back->pc, hw->back->tag, reg - 2,
		    __SHIFTIN(value, __BITS(31, 16)) | old);
		break;
	default:
		panic("%s: invalid register (%" PRIx32, __func__, reg); 
		break;
	}

	return;
}

/*
** Get the width and transaction speed of
** the slot this adapter is plugged into.
*/
static void
ixgbe_get_slot_info(struct ixgbe_hw *hw)
{
	device_t		dev = ((struct ixgbe_osdep *)hw->back)->dev;
	struct ixgbe_mac_info	*mac = &hw->mac;
	u16			link;

	/* For most devices simply call the shared code routine */
	if (hw->device_id != IXGBE_DEV_ID_82599_SFP_SF_QP) {
		ixgbe_get_bus_info(hw);
		goto display;
	}

	/*
	** For the Quad port adapter we need to parse back
	** up the PCI tree to find the speed of the expansion
	** slot into which this adapter is plugged. A bit more work.
	*/
	dev = device_parent(device_parent(dev));
#ifdef IXGBE_DEBUG
	device_printf(dev, "parent pcib = %x,%x,%x\n",
	    pci_get_bus(dev), pci_get_slot(dev), pci_get_function(dev));
#endif
	dev = device_parent(device_parent(dev));
#ifdef IXGBE_DEBUG
	device_printf(dev, "slot pcib = %x,%x,%x\n",
	    pci_get_bus(dev), pci_get_slot(dev), pci_get_function(dev));
#endif
	/* Now get the PCI Express Capabilities offset */
	/* ...and read the Link Status Register */
	link = IXGBE_READ_PCIE_WORD(hw, IXGBE_PCI_LINK_STATUS);
	switch (link & IXGBE_PCI_LINK_WIDTH) {
	case IXGBE_PCI_LINK_WIDTH_1:
		hw->bus.width = ixgbe_bus_width_pcie_x1;
		break;
	case IXGBE_PCI_LINK_WIDTH_2:
		hw->bus.width = ixgbe_bus_width_pcie_x2;
		break;
	case IXGBE_PCI_LINK_WIDTH_4:
		hw->bus.width = ixgbe_bus_width_pcie_x4;
		break;
	case IXGBE_PCI_LINK_WIDTH_8:
		hw->bus.width = ixgbe_bus_width_pcie_x8;
		break;
	default:
		hw->bus.width = ixgbe_bus_width_unknown;
		break;
	}

	switch (link & IXGBE_PCI_LINK_SPEED) {
	case IXGBE_PCI_LINK_SPEED_2500:
		hw->bus.speed = ixgbe_bus_speed_2500;
		break;
	case IXGBE_PCI_LINK_SPEED_5000:
		hw->bus.speed = ixgbe_bus_speed_5000;
		break;
	case IXGBE_PCI_LINK_SPEED_8000:
		hw->bus.speed = ixgbe_bus_speed_8000;
		break;
	default:
		hw->bus.speed = ixgbe_bus_speed_unknown;
		break;
	}

	mac->ops.set_lan_id(hw);

display:
	device_printf(dev,"PCI Express Bus: Speed %s %s\n",
	    ((hw->bus.speed == ixgbe_bus_speed_8000) ? "8.0GT/s":
	    (hw->bus.speed == ixgbe_bus_speed_5000) ? "5.0GT/s":
	    (hw->bus.speed == ixgbe_bus_speed_2500) ? "2.5GT/s":"Unknown"),
	    (hw->bus.width == ixgbe_bus_width_pcie_x8) ? "Width x8" :
	    (hw->bus.width == ixgbe_bus_width_pcie_x4) ? "Width x4" :
	    (hw->bus.width == ixgbe_bus_width_pcie_x1) ? "Width x1" :
	    ("Unknown"));

	if ((hw->device_id != IXGBE_DEV_ID_82599_SFP_SF_QP) &&
	    ((hw->bus.width <= ixgbe_bus_width_pcie_x4) &&
	    (hw->bus.speed == ixgbe_bus_speed_2500))) {
		device_printf(dev, "PCI-Express bandwidth available"
		    " for this card\n     is not sufficient for"
		    " optimal performance.\n");
		device_printf(dev, "For optimal performance a x8 "
		    "PCIE, or x4 PCIE Gen2 slot is required.\n");
        }
	if ((hw->device_id == IXGBE_DEV_ID_82599_SFP_SF_QP) &&
	    ((hw->bus.width <= ixgbe_bus_width_pcie_x8) &&
	    (hw->bus.speed < ixgbe_bus_speed_8000))) {
		device_printf(dev, "PCI-Express bandwidth available"
		    " for this card\n     is not sufficient for"
		    " optimal performance.\n");
		device_printf(dev, "For optimal performance a x8 "
		    "PCIE Gen3 slot is required.\n");
        }

	return;
}


/*
** Setup the correct IVAR register for a particular MSIX interrupt
**   (yes this is all very magic and confusing :)
**  - entry is the register array entry
**  - vector is the MSIX vector for this queue
**  - type is RX/TX/MISC
*/
static void
ixgbe_set_ivar(struct adapter *adapter, u8 entry, u8 vector, s8 type)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 ivar, index;

	vector |= IXGBE_IVAR_ALLOC_VAL;

	switch (hw->mac.type) {

	case ixgbe_mac_82598EB:
		if (type == -1)
			entry = IXGBE_IVAR_OTHER_CAUSES_INDEX;
		else
			entry += (type * 64);
		index = (entry >> 2) & 0x1F;
		ivar = IXGBE_READ_REG(hw, IXGBE_IVAR(index));
		ivar &= ~(0xFF << (8 * (entry & 0x3)));
		ivar |= (vector << (8 * (entry & 0x3)));
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_IVAR(index), ivar);
		break;

	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
		if (type == -1) { /* MISC IVAR */
			index = (entry & 1) * 8;
			ivar = IXGBE_READ_REG(hw, IXGBE_IVAR_MISC);
			ivar &= ~(0xFF << index);
			ivar |= (vector << index);
			IXGBE_WRITE_REG(hw, IXGBE_IVAR_MISC, ivar);
		} else {	/* RX/TX IVARS */
			index = (16 * (entry & 1)) + (8 * type);
			ivar = IXGBE_READ_REG(hw, IXGBE_IVAR(entry >> 1));
			ivar &= ~(0xFF << index);
			ivar |= (vector << index);
			IXGBE_WRITE_REG(hw, IXGBE_IVAR(entry >> 1), ivar);
		}

	default:
		break;
	}
}

static void
ixgbe_configure_ivars(struct adapter *adapter)
{
	struct  ix_queue *que = adapter->queues;
	u32 newitr;

	if (ixgbe_max_interrupt_rate > 0)
		newitr = (4000000 / ixgbe_max_interrupt_rate) & 0x0FF8;
	else
		newitr = 0;

        for (int i = 0; i < adapter->num_queues; i++, que++) {
		/* First the RX queue entry */
                ixgbe_set_ivar(adapter, i, que->msix, 0);
		/* ... and the TX */
		ixgbe_set_ivar(adapter, i, que->msix, 1);
		/* Set an Initial EITR value */
                IXGBE_WRITE_REG(&adapter->hw,
                    IXGBE_EITR(que->msix), newitr);
	}

	/* For the Link interrupt */
        ixgbe_set_ivar(adapter, 1, adapter->linkvec, -1);
}

/*
** ixgbe_sfp_probe - called in the local timer to
** determine if a port had optics inserted.
*/  
static bool ixgbe_sfp_probe(struct adapter *adapter)
{
	struct ixgbe_hw	*hw = &adapter->hw;
	device_t	dev = adapter->dev;
	bool		result = FALSE;

	if ((hw->phy.type == ixgbe_phy_nl) &&
	    (hw->phy.sfp_type == ixgbe_sfp_type_not_present)) {
		s32 ret = hw->phy.ops.identify_sfp(hw);
		if (ret)
                        goto out;
		ret = hw->phy.ops.reset(hw);
		if (ret == IXGBE_ERR_SFP_NOT_SUPPORTED) {
			device_printf(dev,"Unsupported SFP+ module detected!");
			device_printf(dev, "Reload driver with supported module.\n");
			adapter->sfp_probe = FALSE;
                        goto out;
		} else
			device_printf(dev,"SFP+ module detected!\n");
		/* We now have supported optics */
		adapter->sfp_probe = FALSE;
		/* Set the optics type so system reports correctly */
		ixgbe_setup_optics(adapter);
		result = TRUE;
	}
out:
	return (result);
}

/*
** Tasklet handler for MSIX Link interrupts
**  - do outside interrupt since it might sleep
*/
static void
ixgbe_handle_link(void *context)
{
	struct adapter  *adapter = context;

	if (ixgbe_check_link(&adapter->hw,
	    &adapter->link_speed, &adapter->link_up, 0) == 0)
	    ixgbe_update_link_status(adapter);
}

/*
** Tasklet for handling SFP module interrupts
*/
static void
ixgbe_handle_mod(void *context)
{
	struct adapter  *adapter = context;
	struct ixgbe_hw *hw = &adapter->hw;
	device_t	dev = adapter->dev;
	u32 err;

	err = hw->phy.ops.identify_sfp(hw);
	if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		device_printf(dev,
		    "Unsupported SFP+ module type was detected.\n");
		return;
	}
	err = hw->mac.ops.setup_sfp(hw);
	if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		device_printf(dev,
		    "Setup failure - unsupported SFP+ module type.\n");
		return;
	}
	softint_schedule(adapter->msf_si);
	return;
}


/*
** Tasklet for handling MSF (multispeed fiber) interrupts
*/
static void
ixgbe_handle_msf(void *context)
{
	struct adapter  *adapter = context;
	struct ixgbe_hw *hw = &adapter->hw;
	u32 autoneg;
	bool negotiate;

	autoneg = hw->phy.autoneg_advertised;
	if ((!autoneg) && (hw->mac.ops.get_link_capabilities))
		hw->mac.ops.get_link_capabilities(hw, &autoneg, &negotiate);
	else
		negotiate = 0;
	if (hw->mac.ops.setup_link)
		hw->mac.ops.setup_link(hw, autoneg, TRUE);
	return;
}

#ifdef IXGBE_FDIR
/*
** Tasklet for reinitializing the Flow Director filter table
*/
static void
ixgbe_reinit_fdir(void *context)
{
	struct adapter  *adapter = context;
	struct ifnet   *ifp = adapter->ifp;

	if (adapter->fdir_reinit != 1) /* Shouldn't happen */
		return;
	ixgbe_reinit_fdir_tables_82599(&adapter->hw);
	adapter->fdir_reinit = 0;
	/* re-enable flow director interrupts */
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS, IXGBE_EIMS_FLOW_DIR);
	/* Restart the interface */
	ifp->if_flags |= IFF_RUNNING;
	return;
}
#endif

/**********************************************************************
 *
 *  Update the board statistics counters.
 *
 **********************************************************************/
static void
ixgbe_update_stats_counters(struct adapter *adapter)
{
	struct ifnet   *ifp = adapter->ifp;
	struct ixgbe_hw *hw = &adapter->hw;
	u32  missed_rx = 0, bprc, lxon, lxoff, total;
	u64  total_missed_rx = 0;
	uint64_t crcerrs, rlec;

	crcerrs = IXGBE_READ_REG(hw, IXGBE_CRCERRS);
	adapter->stats.crcerrs.ev_count += crcerrs;
	adapter->stats.illerrc.ev_count += IXGBE_READ_REG(hw, IXGBE_ILLERRC);
	adapter->stats.errbc.ev_count += IXGBE_READ_REG(hw, IXGBE_ERRBC);
	adapter->stats.mspdc.ev_count += IXGBE_READ_REG(hw, IXGBE_MSPDC);

	/*
	** Note: these are for the 8 possible traffic classes,
	**	 which in current implementation is unused,
	**	 therefore only 0 should read real data.
	*/
	for (int i = 0; i < __arraycount(adapter->stats.mpc); i++) {
		int j = i % adapter->num_queues;
		u32 mp;
		mp = IXGBE_READ_REG(hw, IXGBE_MPC(i));
		/* missed_rx tallies misses for the gprc workaround */
		missed_rx += mp;
		/* global total per queue */
        	adapter->stats.mpc[j].ev_count += mp;
		/* Running comprehensive total for stats display */
		total_missed_rx += mp;
		if (hw->mac.type == ixgbe_mac_82598EB) {
			adapter->stats.rnbc[j] +=
			    IXGBE_READ_REG(hw, IXGBE_RNBC(i));
			adapter->stats.qbtc[j].ev_count +=
			    IXGBE_READ_REG(hw, IXGBE_QBTC(i));
			adapter->stats.qbrc[j].ev_count +=
			    IXGBE_READ_REG(hw, IXGBE_QBRC(i));
			adapter->stats.pxonrxc[j].ev_count +=
			    IXGBE_READ_REG(hw, IXGBE_PXONRXC(i));
		} else {
			adapter->stats.pxonrxc[j].ev_count +=
			    IXGBE_READ_REG(hw, IXGBE_PXONRXCNT(i));
		}
		adapter->stats.pxontxc[j].ev_count +=
		    IXGBE_READ_REG(hw, IXGBE_PXONTXC(i));
		adapter->stats.pxofftxc[j].ev_count +=
		    IXGBE_READ_REG(hw, IXGBE_PXOFFTXC(i));
		adapter->stats.pxoffrxc[j].ev_count +=
		    IXGBE_READ_REG(hw, IXGBE_PXOFFRXC(i));
		adapter->stats.pxon2offc[j].ev_count +=
		    IXGBE_READ_REG(hw, IXGBE_PXON2OFFCNT(i));
	}
	for (int i = 0; i < __arraycount(adapter->stats.qprc); i++) {
		int j = i % adapter->num_queues;
		adapter->stats.qprc[j].ev_count += IXGBE_READ_REG(hw, IXGBE_QPRC(i));
		adapter->stats.qptc[j].ev_count += IXGBE_READ_REG(hw, IXGBE_QPTC(i));
		adapter->stats.qprdc[j].ev_count += IXGBE_READ_REG(hw, IXGBE_QPRDC(i));
	}
	adapter->stats.mlfc.ev_count += IXGBE_READ_REG(hw, IXGBE_MLFC);
	adapter->stats.mrfc.ev_count += IXGBE_READ_REG(hw, IXGBE_MRFC);
	rlec = IXGBE_READ_REG(hw, IXGBE_RLEC);
	adapter->stats.rlec.ev_count += rlec;

	/* Hardware workaround, gprc counts missed packets */
	adapter->stats.gprc.ev_count += IXGBE_READ_REG(hw, IXGBE_GPRC) - missed_rx;

	lxon = IXGBE_READ_REG(hw, IXGBE_LXONTXC);
	adapter->stats.lxontxc.ev_count += lxon;
	lxoff = IXGBE_READ_REG(hw, IXGBE_LXOFFTXC);
	adapter->stats.lxofftxc.ev_count += lxoff;
	total = lxon + lxoff;

	if (hw->mac.type != ixgbe_mac_82598EB) {
		adapter->stats.gorc.ev_count += IXGBE_READ_REG(hw, IXGBE_GORCL) +
		    ((u64)IXGBE_READ_REG(hw, IXGBE_GORCH) << 32);
		adapter->stats.gotc.ev_count += IXGBE_READ_REG(hw, IXGBE_GOTCL) +
		    ((u64)IXGBE_READ_REG(hw, IXGBE_GOTCH) << 32) - total * ETHER_MIN_LEN;
		adapter->stats.tor.ev_count += IXGBE_READ_REG(hw, IXGBE_TORL) +
		    ((u64)IXGBE_READ_REG(hw, IXGBE_TORH) << 32);
		adapter->stats.lxonrxc.ev_count += IXGBE_READ_REG(hw, IXGBE_LXONRXCNT);
		adapter->stats.lxoffrxc.ev_count += IXGBE_READ_REG(hw, IXGBE_LXOFFRXCNT);
	} else {
		adapter->stats.lxonrxc.ev_count += IXGBE_READ_REG(hw, IXGBE_LXONRXC);
		adapter->stats.lxoffrxc.ev_count += IXGBE_READ_REG(hw, IXGBE_LXOFFRXC);
		/* 82598 only has a counter in the high register */
		adapter->stats.gorc.ev_count += IXGBE_READ_REG(hw, IXGBE_GORCH);
		adapter->stats.gotc.ev_count += IXGBE_READ_REG(hw, IXGBE_GOTCH) - total * ETHER_MIN_LEN;
		adapter->stats.tor.ev_count += IXGBE_READ_REG(hw, IXGBE_TORH);
	}

	/*
	 * Workaround: mprc hardware is incorrectly counting
	 * broadcasts, so for now we subtract those.
	 */
	bprc = IXGBE_READ_REG(hw, IXGBE_BPRC);
	adapter->stats.bprc.ev_count += bprc;
	adapter->stats.mprc.ev_count += IXGBE_READ_REG(hw, IXGBE_MPRC) - ((hw->mac.type == ixgbe_mac_82598EB) ? bprc : 0);

	adapter->stats.prc64.ev_count += IXGBE_READ_REG(hw, IXGBE_PRC64);
	adapter->stats.prc127.ev_count += IXGBE_READ_REG(hw, IXGBE_PRC127);
	adapter->stats.prc255.ev_count += IXGBE_READ_REG(hw, IXGBE_PRC255);
	adapter->stats.prc511.ev_count += IXGBE_READ_REG(hw, IXGBE_PRC511);
	adapter->stats.prc1023.ev_count += IXGBE_READ_REG(hw, IXGBE_PRC1023);
	adapter->stats.prc1522.ev_count += IXGBE_READ_REG(hw, IXGBE_PRC1522);

	adapter->stats.gptc.ev_count += IXGBE_READ_REG(hw, IXGBE_GPTC) - total;
	adapter->stats.mptc.ev_count += IXGBE_READ_REG(hw, IXGBE_MPTC) - total;
	adapter->stats.ptc64.ev_count += IXGBE_READ_REG(hw, IXGBE_PTC64) - total;

	adapter->stats.ruc.ev_count += IXGBE_READ_REG(hw, IXGBE_RUC);
	adapter->stats.rfc.ev_count += IXGBE_READ_REG(hw, IXGBE_RFC);
	adapter->stats.roc.ev_count += IXGBE_READ_REG(hw, IXGBE_ROC);
	adapter->stats.rjc.ev_count += IXGBE_READ_REG(hw, IXGBE_RJC);
	adapter->stats.mngprc.ev_count += IXGBE_READ_REG(hw, IXGBE_MNGPRC);
	adapter->stats.mngpdc.ev_count += IXGBE_READ_REG(hw, IXGBE_MNGPDC);
	adapter->stats.mngptc.ev_count += IXGBE_READ_REG(hw, IXGBE_MNGPTC);
	adapter->stats.tpr.ev_count += IXGBE_READ_REG(hw, IXGBE_TPR);
	adapter->stats.tpt.ev_count += IXGBE_READ_REG(hw, IXGBE_TPT);
	adapter->stats.ptc127.ev_count += IXGBE_READ_REG(hw, IXGBE_PTC127);
	adapter->stats.ptc255.ev_count += IXGBE_READ_REG(hw, IXGBE_PTC255);
	adapter->stats.ptc511.ev_count += IXGBE_READ_REG(hw, IXGBE_PTC511);
	adapter->stats.ptc1023.ev_count += IXGBE_READ_REG(hw, IXGBE_PTC1023);
	adapter->stats.ptc1522.ev_count += IXGBE_READ_REG(hw, IXGBE_PTC1522);
	adapter->stats.bptc.ev_count += IXGBE_READ_REG(hw, IXGBE_BPTC);
	adapter->stats.xec.ev_count += IXGBE_READ_REG(hw, IXGBE_XEC);
	adapter->stats.fccrc.ev_count += IXGBE_READ_REG(hw, IXGBE_FCCRC);
	adapter->stats.fclast.ev_count += IXGBE_READ_REG(hw, IXGBE_FCLAST);

	/* Only read FCOE on 82599 */
	if (hw->mac.type != ixgbe_mac_82598EB) {
		adapter->stats.fcoerpdc.ev_count +=
		    IXGBE_READ_REG(hw, IXGBE_FCOERPDC);
		adapter->stats.fcoeprc.ev_count +=
		    IXGBE_READ_REG(hw, IXGBE_FCOEPRC);
		adapter->stats.fcoeptc.ev_count +=
		    IXGBE_READ_REG(hw, IXGBE_FCOEPTC);
		adapter->stats.fcoedwrc.ev_count +=
		    IXGBE_READ_REG(hw, IXGBE_FCOEDWRC);
		adapter->stats.fcoedwtc.ev_count +=
		    IXGBE_READ_REG(hw, IXGBE_FCOEDWTC);
	}

	/* Fill out the OS statistics structure */
	/*
	 * NetBSD: Don't override if_{i|o}{packets|bytes|mcasts} with
	 * adapter->stats counters. It's required to make ifconfig -z
	 * (SOICZIFDATA) work.
	 */
	ifp->if_collisions = 0;

	/* Rx Errors */
	ifp->if_iqdrops += total_missed_rx;
	ifp->if_ierrors += crcerrs + rlec;
}

/** ixgbe_sysctl_tdh_handler - Handler function
 *  Retrieves the TDH value from the hardware
 */
static int 
ixgbe_sysctl_tdh_handler(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	uint32_t val;
	struct tx_ring *txr;

	node = *rnode;
	txr = (struct tx_ring *)node.sysctl_data;
	if (txr == NULL)
		return 0;
	val = IXGBE_READ_REG(&txr->adapter->hw, IXGBE_TDH(txr->me));
	node.sysctl_data = &val;
	return sysctl_lookup(SYSCTLFN_CALL(&node));
}

/** ixgbe_sysctl_tdt_handler - Handler function
 *  Retrieves the TDT value from the hardware
 */
static int 
ixgbe_sysctl_tdt_handler(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	uint32_t val;
	struct tx_ring *txr;

	node = *rnode;
	txr = (struct tx_ring *)node.sysctl_data;
	if (txr == NULL)
		return 0;
	val = IXGBE_READ_REG(&txr->adapter->hw, IXGBE_TDT(txr->me));
	node.sysctl_data = &val;
	return sysctl_lookup(SYSCTLFN_CALL(&node));
}

/** ixgbe_sysctl_rdh_handler - Handler function
 *  Retrieves the RDH value from the hardware
 */
static int 
ixgbe_sysctl_rdh_handler(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	uint32_t val;
	struct rx_ring *rxr;

	node = *rnode;
	rxr = (struct rx_ring *)node.sysctl_data;
	if (rxr == NULL)
		return 0;
	val = IXGBE_READ_REG(&rxr->adapter->hw, IXGBE_RDH(rxr->me));
	node.sysctl_data = &val;
	return sysctl_lookup(SYSCTLFN_CALL(&node));
}

/** ixgbe_sysctl_rdt_handler - Handler function
 *  Retrieves the RDT value from the hardware
 */
static int 
ixgbe_sysctl_rdt_handler(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	uint32_t val;
	struct rx_ring *rxr;

	node = *rnode;
	rxr = (struct rx_ring *)node.sysctl_data;
	if (rxr == NULL)
		return 0;
	val = IXGBE_READ_REG(&rxr->adapter->hw, IXGBE_RDT(rxr->me));
	node.sysctl_data = &val;
	return sysctl_lookup(SYSCTLFN_CALL(&node));
}

static int
ixgbe_sysctl_interrupt_rate_handler(SYSCTLFN_ARGS)
{
	int error;
	struct sysctlnode node;
	struct ix_queue *que;
	uint32_t reg, usec, rate;

	node = *rnode;
	que = (struct ix_queue *)node.sysctl_data;
	if (que == NULL)
		return 0;
	reg = IXGBE_READ_REG(&que->adapter->hw, IXGBE_EITR(que->msix));
	usec = ((reg & 0x0FF8) >> 3);
	if (usec > 0)
		rate = 500000 / usec;
	else
		rate = 0;
	node.sysctl_data = &rate;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error)
		return error;
	reg &= ~0xfff; /* default, no limitation */
	ixgbe_max_interrupt_rate = 0;
	if (rate > 0 && rate < 500000) {
		if (rate < 1000)
			rate = 1000;
		ixgbe_max_interrupt_rate = rate;
		reg |= ((4000000/rate) & 0xff8 );
	}
	IXGBE_WRITE_REG(&que->adapter->hw, IXGBE_EITR(que->msix), reg);
	return 0;
}

const struct sysctlnode *
ixgbe_sysctl_instance(struct adapter *adapter)
{
	const char *dvname;
	struct sysctllog **log;
	int rc;
	const struct sysctlnode *rnode;

	log = &adapter->sysctllog;
	dvname = device_xname(adapter->dev);

	if ((rc = sysctl_createv(log, 0, NULL, &rnode,
	    0, CTLTYPE_NODE, dvname,
	    SYSCTL_DESCR("ixgbe information and settings"),
	    NULL, 0, NULL, 0, CTL_HW, CTL_CREATE, CTL_EOL)) != 0)
		goto err;

	return rnode;
err:
	printf("%s: sysctl_createv failed, rc = %d\n", __func__, rc);
	return NULL;
}

/*
 * Add sysctl variables, one per statistic, to the system.
 */
static void
ixgbe_add_hw_stats(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	const struct sysctlnode *rnode, *cnode;
	struct sysctllog **log = &adapter->sysctllog;
	struct tx_ring *txr = adapter->tx_rings;
	struct rx_ring *rxr = adapter->rx_rings;
	struct ixgbe_hw_stats *stats = &adapter->stats;

	/* Driver Statistics */
#if 0
	/* These counters are not updated by the software */
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "dropped",
			CTLFLAG_RD, &adapter->dropped_pkts,
			"Driver dropped packets");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "mbuf_header_failed",
			CTLFLAG_RD, &adapter->mbuf_header_failed,
			"???");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "mbuf_packet_failed",
			CTLFLAG_RD, &adapter->mbuf_packet_failed,
			"???");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "no_tx_map_avail",
			CTLFLAG_RD, &adapter->no_tx_map_avail,
			"???");
#endif
	evcnt_attach_dynamic(&adapter->handleq, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "Handled queue in softint");
	evcnt_attach_dynamic(&adapter->req, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "Requeued in softint");
	evcnt_attach_dynamic(&adapter->morerx, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "Interrupt handler more rx");
	evcnt_attach_dynamic(&adapter->moretx, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "Interrupt handler more tx");
	evcnt_attach_dynamic(&adapter->txloops, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "Interrupt handler tx loops");
	evcnt_attach_dynamic(&adapter->efbig_tx_dma_setup, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "Driver tx dma soft fail EFBIG");
	evcnt_attach_dynamic(&adapter->m_defrag_failed, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "m_defrag() failed");
	evcnt_attach_dynamic(&adapter->efbig2_tx_dma_setup, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "Driver tx dma hard fail EFBIG");
	evcnt_attach_dynamic(&adapter->einval_tx_dma_setup, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "Driver tx dma hard fail EINVAL");
	evcnt_attach_dynamic(&adapter->other_tx_dma_setup, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "Driver tx dma hard fail other");
	evcnt_attach_dynamic(&adapter->eagain_tx_dma_setup, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "Driver tx dma soft fail EAGAIN");
	evcnt_attach_dynamic(&adapter->enomem_tx_dma_setup, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "Driver tx dma soft fail ENOMEM");
	evcnt_attach_dynamic(&adapter->watchdog_events, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "Watchdog timeouts");
	evcnt_attach_dynamic(&adapter->tso_err, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "TSO errors");
	evcnt_attach_dynamic(&adapter->link_irq, EVCNT_TYPE_MISC,
	    NULL, device_xname(dev), "Link MSIX IRQ Handled");

	for (int i = 0; i < adapter->num_queues; i++, rxr++, txr++) {
		snprintf(adapter->queues[i].evnamebuf,
		    sizeof(adapter->queues[i].evnamebuf), "%s queue%d",
		    device_xname(dev), i);
		snprintf(adapter->queues[i].namebuf,
		    sizeof(adapter->queues[i].namebuf), "queue%d", i);

		if ((rnode = ixgbe_sysctl_instance(adapter)) == NULL) {
			aprint_error_dev(dev, "could not create sysctl root\n");
			break;
		}

		if (sysctl_createv(log, 0, &rnode, &rnode,
		    0, CTLTYPE_NODE,
		    adapter->queues[i].namebuf, SYSCTL_DESCR("Queue Name"),
		    NULL, 0, NULL, 0, CTL_CREATE, CTL_EOL) != 0)
			break;

		if (sysctl_createv(log, 0, &rnode, &cnode,
		    CTLFLAG_READWRITE, CTLTYPE_INT,
		    "interrupt_rate", SYSCTL_DESCR("Interrupt Rate"),
		    ixgbe_sysctl_interrupt_rate_handler, 0,
		    (void *)&adapter->queues[i], 0, CTL_CREATE, CTL_EOL) != 0)
			break;

		if (sysctl_createv(log, 0, &rnode, &cnode,
		    CTLFLAG_READONLY, CTLTYPE_QUAD,
		    "irqs", SYSCTL_DESCR("irqs on this queue"),
			NULL, 0, &(adapter->queues[i].irqs),
		    0, CTL_CREATE, CTL_EOL) != 0)
			break;

		if (sysctl_createv(log, 0, &rnode, &cnode,
		    CTLFLAG_READONLY, CTLTYPE_INT,
		    "txd_head", SYSCTL_DESCR("Transmit Descriptor Head"),
		    ixgbe_sysctl_tdh_handler, 0, (void *)txr,
		    0, CTL_CREATE, CTL_EOL) != 0)
			break;

		if (sysctl_createv(log, 0, &rnode, &cnode,
		    CTLFLAG_READONLY, CTLTYPE_INT,
		    "txd_tail", SYSCTL_DESCR("Transmit Descriptor Tail"),
		    ixgbe_sysctl_tdt_handler, 0, (void *)txr,
		    0, CTL_CREATE, CTL_EOL) != 0)
			break;

		evcnt_attach_dynamic(&txr->tso_tx, EVCNT_TYPE_MISC,
		    NULL, device_xname(dev), "TSO");
		evcnt_attach_dynamic(&txr->no_desc_avail, EVCNT_TYPE_MISC,
		    NULL, adapter->queues[i].evnamebuf,
		    "Queue No Descriptor Available");
		evcnt_attach_dynamic(&txr->total_packets, EVCNT_TYPE_MISC,
		    NULL, adapter->queues[i].evnamebuf,
		    "Queue Packets Transmitted");

#ifdef LRO
		struct lro_ctrl *lro = &rxr->lro;
#endif /* LRO */

		if (sysctl_createv(log, 0, &rnode, &cnode,
		    CTLFLAG_READONLY,
		    CTLTYPE_INT,
		    "rxd_head", SYSCTL_DESCR("Receive Descriptor Head"),
		    ixgbe_sysctl_rdh_handler, 0, (void *)rxr, 0,
		    CTL_CREATE, CTL_EOL) != 0)
			break;

		if (sysctl_createv(log, 0, &rnode, &cnode,
		    CTLFLAG_READONLY,
		    CTLTYPE_INT,
		    "rxd_tail", SYSCTL_DESCR("Receive Descriptor Tail"),
		    ixgbe_sysctl_rdt_handler, 0, (void *)rxr, 0,
		    CTL_CREATE, CTL_EOL) != 0)
			break;

		if (i < __arraycount(adapter->stats.mpc)) {
			evcnt_attach_dynamic(&adapter->stats.mpc[i],
			    EVCNT_TYPE_MISC, NULL, adapter->queues[i].evnamebuf,
			    "Missed Packet Count");
		}
		if (i < __arraycount(adapter->stats.pxontxc)) {
			evcnt_attach_dynamic(&adapter->stats.pxontxc[i],
			    EVCNT_TYPE_MISC, NULL, adapter->queues[i].evnamebuf,
			    "pxontxc");
			evcnt_attach_dynamic(&adapter->stats.pxonrxc[i],
			    EVCNT_TYPE_MISC, NULL, adapter->queues[i].evnamebuf,
			    "pxonrxc");
			evcnt_attach_dynamic(&adapter->stats.pxofftxc[i],
			    EVCNT_TYPE_MISC, NULL, adapter->queues[i].evnamebuf,
			    "pxofftxc");
			evcnt_attach_dynamic(&adapter->stats.pxoffrxc[i],
			    EVCNT_TYPE_MISC, NULL, adapter->queues[i].evnamebuf,
			    "pxoffrxc");
			evcnt_attach_dynamic(&adapter->stats.pxon2offc[i],
			    EVCNT_TYPE_MISC, NULL, adapter->queues[i].evnamebuf,
			    "pxon2offc");
		}
		if (i < __arraycount(adapter->stats.qprc)) {
			evcnt_attach_dynamic(&adapter->stats.qprc[i],
			    EVCNT_TYPE_MISC, NULL, adapter->queues[i].evnamebuf,
			    "qprc");
			evcnt_attach_dynamic(&adapter->stats.qptc[i],
			    EVCNT_TYPE_MISC, NULL, adapter->queues[i].evnamebuf,
			    "qptc");
			evcnt_attach_dynamic(&adapter->stats.qbrc[i],
			    EVCNT_TYPE_MISC, NULL, adapter->queues[i].evnamebuf,
			    "qbrc");
			evcnt_attach_dynamic(&adapter->stats.qbtc[i],
			    EVCNT_TYPE_MISC, NULL, adapter->queues[i].evnamebuf,
			    "qbtc");
			evcnt_attach_dynamic(&adapter->stats.qprdc[i],
			    EVCNT_TYPE_MISC, NULL, adapter->queues[i].evnamebuf,
			    "qprdc");
		}

		evcnt_attach_dynamic(&rxr->rx_packets, EVCNT_TYPE_MISC,
		    NULL, adapter->queues[i].evnamebuf, "Queue Packets Received");
		evcnt_attach_dynamic(&rxr->rx_bytes, EVCNT_TYPE_MISC,
		    NULL, adapter->queues[i].evnamebuf, "Queue Bytes Received");
		evcnt_attach_dynamic(&rxr->rx_copies, EVCNT_TYPE_MISC,
		    NULL, adapter->queues[i].evnamebuf, "Copied RX Frames");
		evcnt_attach_dynamic(&rxr->no_jmbuf, EVCNT_TYPE_MISC,
		    NULL, adapter->queues[i].evnamebuf, "Rx no jumbo mbuf");
		evcnt_attach_dynamic(&rxr->rx_discarded, EVCNT_TYPE_MISC,
		    NULL, adapter->queues[i].evnamebuf, "Rx discarded");
		evcnt_attach_dynamic(&rxr->rx_irq, EVCNT_TYPE_MISC,
		    NULL, adapter->queues[i].evnamebuf, "Rx interrupts");
#ifdef LRO
		SYSCTL_ADD_INT(ctx, queue_list, OID_AUTO, "lro_queued",
				CTLFLAG_RD, &lro->lro_queued, 0,
				"LRO Queued");
		SYSCTL_ADD_INT(ctx, queue_list, OID_AUTO, "lro_flushed",
				CTLFLAG_RD, &lro->lro_flushed, 0,
				"LRO Flushed");
#endif /* LRO */
	}

	/* MAC stats get the own sub node */


	snprintf(stats->namebuf,
	    sizeof(stats->namebuf), "%s MAC Statistics", device_xname(dev));

	evcnt_attach_dynamic(&stats->ipcs, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "rx csum offload - IP");
	evcnt_attach_dynamic(&stats->l4cs, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "rx csum offload - L4");
	evcnt_attach_dynamic(&stats->ipcs_bad, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "rx csum offload - IP bad");
	evcnt_attach_dynamic(&stats->l4cs_bad, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "rx csum offload - L4 bad");
	evcnt_attach_dynamic(&stats->intzero, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Interrupt conditions zero");
	evcnt_attach_dynamic(&stats->legint, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Legacy interrupts");
	evcnt_attach_dynamic(&stats->crcerrs, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "CRC Errors");
	evcnt_attach_dynamic(&stats->illerrc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Illegal Byte Errors");
	evcnt_attach_dynamic(&stats->errbc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Byte Errors");
	evcnt_attach_dynamic(&stats->mspdc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "MAC Short Packets Discarded");
	evcnt_attach_dynamic(&stats->mlfc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "MAC Local Faults");
	evcnt_attach_dynamic(&stats->mrfc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "MAC Remote Faults");
	evcnt_attach_dynamic(&stats->rlec, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Receive Length Errors");
	evcnt_attach_dynamic(&stats->lxontxc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Link XON Transmitted");
	evcnt_attach_dynamic(&stats->lxonrxc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Link XON Received");
	evcnt_attach_dynamic(&stats->lxofftxc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Link XOFF Transmitted");
	evcnt_attach_dynamic(&stats->lxoffrxc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Link XOFF Received");

	/* Packet Reception Stats */
	evcnt_attach_dynamic(&stats->tor, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Total Octets Received");
	evcnt_attach_dynamic(&stats->gorc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Good Octets Received");
	evcnt_attach_dynamic(&stats->tpr, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Total Packets Received");
	evcnt_attach_dynamic(&stats->gprc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Good Packets Received");
	evcnt_attach_dynamic(&stats->mprc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Multicast Packets Received");
	evcnt_attach_dynamic(&stats->bprc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Broadcast Packets Received");
	evcnt_attach_dynamic(&stats->prc64, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "64 byte frames received ");
	evcnt_attach_dynamic(&stats->prc127, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "65-127 byte frames received");
	evcnt_attach_dynamic(&stats->prc255, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "128-255 byte frames received");
	evcnt_attach_dynamic(&stats->prc511, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "256-511 byte frames received");
	evcnt_attach_dynamic(&stats->prc1023, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "512-1023 byte frames received");
	evcnt_attach_dynamic(&stats->prc1522, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "1023-1522 byte frames received");
	evcnt_attach_dynamic(&stats->ruc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Receive Undersized");
	evcnt_attach_dynamic(&stats->rfc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Fragmented Packets Received ");
	evcnt_attach_dynamic(&stats->roc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Oversized Packets Received");
	evcnt_attach_dynamic(&stats->rjc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Received Jabber");
	evcnt_attach_dynamic(&stats->mngprc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Management Packets Received");
	evcnt_attach_dynamic(&stats->xec, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Checksum Errors");

	/* Packet Transmission Stats */
	evcnt_attach_dynamic(&stats->gotc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Good Octets Transmitted");
	evcnt_attach_dynamic(&stats->tpt, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Total Packets Transmitted");
	evcnt_attach_dynamic(&stats->gptc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Good Packets Transmitted");
	evcnt_attach_dynamic(&stats->bptc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Broadcast Packets Transmitted");
	evcnt_attach_dynamic(&stats->mptc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Multicast Packets Transmitted");
	evcnt_attach_dynamic(&stats->mngptc, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "Management Packets Transmitted");
	evcnt_attach_dynamic(&stats->ptc64, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "64 byte frames transmitted ");
	evcnt_attach_dynamic(&stats->ptc127, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "65-127 byte frames transmitted");
	evcnt_attach_dynamic(&stats->ptc255, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "128-255 byte frames transmitted");
	evcnt_attach_dynamic(&stats->ptc511, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "256-511 byte frames transmitted");
	evcnt_attach_dynamic(&stats->ptc1023, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "512-1023 byte frames transmitted");
	evcnt_attach_dynamic(&stats->ptc1522, EVCNT_TYPE_MISC, NULL,
	    stats->namebuf, "1024-1522 byte frames transmitted");
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
ixgbe_set_flowcntl(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int error, last;
	struct adapter *adapter;

	node = *rnode;
	adapter = (struct adapter *)node.sysctl_data;
	node.sysctl_data = &adapter->fc;
	last = adapter->fc;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error != 0 || newp == NULL)
		return error;

	/* Don't bother if it's not changed */
	if (adapter->fc == last)
		return (0);

	switch (adapter->fc) {
		case ixgbe_fc_rx_pause:
		case ixgbe_fc_tx_pause:
		case ixgbe_fc_full:
			adapter->hw.fc.requested_mode = adapter->fc;
			if (adapter->num_queues > 1)
				ixgbe_disable_rx_drop(adapter);
			break;
		case ixgbe_fc_none:
			adapter->hw.fc.requested_mode = ixgbe_fc_none;
			if (adapter->num_queues > 1)
				ixgbe_enable_rx_drop(adapter);
			break;
		default:
			adapter->fc = last;
			return (EINVAL);
	}
	/* Don't autoneg if forcing a value */
	adapter->hw.fc.disable_fc_autoneg = TRUE;
	ixgbe_fc_enable(&adapter->hw);
	return 0;
}


/*
** Control link advertise speed:
**	1 - advertise only 1G
**	2 - advertise 100Mb
**	3 - advertise normal
*/
static int
ixgbe_set_advertise(SYSCTLFN_ARGS)
{
	struct sysctlnode	node;
	int			t, error = 0;
	struct adapter		*adapter;
	device_t		dev;
	struct ixgbe_hw		*hw;
	ixgbe_link_speed	speed, last;

	node = *rnode;
	adapter = (struct adapter *)node.sysctl_data;
	dev = adapter->dev;
	hw = &adapter->hw;
	last = adapter->advertise;
	t = adapter->advertise;
	node.sysctl_data = &t;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error != 0 || newp == NULL)
		return error;

	if (adapter->advertise == last) /* no change */
		return (0);

	if (t == -1)
		return 0;

	adapter->advertise = t;

	if (!((hw->phy.media_type == ixgbe_media_type_copper) ||
            (hw->phy.multispeed_fiber)))
		return (EINVAL);

	if ((adapter->advertise == 2) && (hw->mac.type != ixgbe_mac_X540)) {
		device_printf(dev, "Set Advertise: 100Mb on X540 only\n");
		return (EINVAL);
	}

	if (adapter->advertise == 1)
                speed = IXGBE_LINK_SPEED_1GB_FULL;
	else if (adapter->advertise == 2)
                speed = IXGBE_LINK_SPEED_100_FULL;
	else if (adapter->advertise == 3)
                speed = IXGBE_LINK_SPEED_1GB_FULL |
			IXGBE_LINK_SPEED_10GB_FULL;
	else {	/* bogus value */
		adapter->advertise = last;
		return (EINVAL);
	}

	hw->mac.autotry_restart = TRUE;
	hw->mac.ops.setup_link(hw, speed, TRUE);

	return 0;
}

/*
** Thermal Shutdown Trigger
**   - cause a Thermal Overtemp IRQ
**   - this now requires firmware enabling
*/
static int
ixgbe_set_thermal_test(SYSCTLFN_ARGS)
{
	struct sysctlnode node;
	int		error, fire = 0;
	struct adapter	*adapter;
	struct ixgbe_hw *hw;

	node = *rnode;
	adapter = (struct adapter *)node.sysctl_data;
	hw = &adapter->hw;

	if (hw->mac.type != ixgbe_mac_X540)
		return (0);

	node.sysctl_data = &fire;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if ((error) || (newp == NULL))
		return (error);

	if (fire) {
		u32 reg = IXGBE_READ_REG(hw, IXGBE_EICS);
		reg |= IXGBE_EICR_TS;
		IXGBE_WRITE_REG(hw, IXGBE_EICS, reg);
	}

	return (0);
}

/*
** Enable the hardware to drop packets when the buffer is
** full. This is useful when multiqueue,so that no single
** queue being full stalls the entire RX engine. We only
** enable this when Multiqueue AND when Flow Control is 
** disabled.
*/
static void
ixgbe_enable_rx_drop(struct adapter *adapter)
{
        struct ixgbe_hw *hw = &adapter->hw;

	for (int i = 0; i < adapter->num_queues; i++) {
        	u32 srrctl = IXGBE_READ_REG(hw, IXGBE_SRRCTL(i));
        	srrctl |= IXGBE_SRRCTL_DROP_EN;
        	IXGBE_WRITE_REG(hw, IXGBE_SRRCTL(i), srrctl);
	}
}

static void
ixgbe_disable_rx_drop(struct adapter *adapter)
{
        struct ixgbe_hw *hw = &adapter->hw;

	for (int i = 0; i < adapter->num_queues; i++) {
        	u32 srrctl = IXGBE_READ_REG(hw, IXGBE_SRRCTL(i));
        	srrctl &= ~IXGBE_SRRCTL_DROP_EN;
        	IXGBE_WRITE_REG(hw, IXGBE_SRRCTL(i), srrctl);
	}
}
