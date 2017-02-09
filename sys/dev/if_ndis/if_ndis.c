/*	$NetBSD: if_ndis.c,v 1.34 2012/10/27 17:18:23 chs Exp $	*/

/*-
 * Copyright (c) 2003
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
 */

#include <sys/cdefs.h>
#ifdef __FreeBSD__
__FBSDID("$FreeBSD: src/sys/dev/if_ndis/if_ndis.c,v 1.69.2.6 2005/03/31 04:24:36 wpaul Exp $");
#endif
#ifdef __NetBSD__
__KERNEL_RCSID(0, "$NetBSD: if_ndis.c,v 1.34 2012/10/27 17:18:23 chs Exp $");
#endif


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <sys/device.h>
#include <sys/kauth.h>

#include <sys/module.h>

#include <sys/proc.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_arp.h>

#include <net/if_ether.h>

#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/route.h>

#include <net/bpf.h>


#include <sys/bus.h>


#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_ioctl.h>

#include <dev/ic/wi_ieee.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <compat/ndis/pe_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/ntoskrnl_var.h>
#include <compat/ndis/hal_var.h>
#include <compat/ndis/ndis_var.h>
#include <compat/ndis/cfg_var.h>
#include <compat/ndis/nbcompat.h>
#include <dev/if_ndis/if_ndisvar.h>

#define NDIS_IMAGE
#define NDIS_REGVALS

#include "ndis_driver_data.h"

void ndis_attach(void *);
int ndis_detach(device_t, int);
int ndis_suspend(device_t);
int ndis_resume(device_t);
void ndis_shutdown(device_t);


/* I moved these to if_ndisvar.h */
/*
static __stdcall void ndis_txeof	(ndis_handle,
	ndis_packet *, ndis_status);
static __stdcall void ndis_rxeof	(ndis_handle,
	ndis_packet **, uint32_t);
static __stdcall void ndis_linksts	(ndis_handle,
	ndis_status, void *, uint32_t);
static __stdcall void ndis_linksts_done	(ndis_handle);
*/

/* We need to wrap these functions for amd64. */

static funcptr ndis_txeof_wrap;
static funcptr ndis_rxeof_wrap;
static funcptr ndis_linksts_wrap;
static funcptr ndis_linksts_done_wrap;

int ndis_intr(void *);
static void ndis_tick		(void *);
static void ndis_ticktask	(void *);
static void ndis_start		(struct ifnet *);
static void ndis_starttask	(void *);
static int ndis_ioctl		(struct ifnet *, u_long, void *);
static int ndis_wi_ioctl_get	(struct ifnet *, u_long, void *);
static int ndis_wi_ioctl_set	(struct ifnet *, u_long, void *);
static int ndis_init		(struct ifnet *);
static void ndis_stop		(struct ndis_softc *);
static void ndis_watchdog	(struct ifnet *);
static int ndis_ifmedia_upd	(struct ifnet *);
static void ndis_ifmedia_sts	(struct ifnet *, struct ifmediareq *);
static int ndis_get_assoc	(struct ndis_softc *, ndis_wlan_bssid_ex **);
static int ndis_probe_offload	(struct ndis_softc *);
static int ndis_set_offload	(struct ndis_softc *);
static void ndis_getstate_80211	(struct ndis_softc *);
static void ndis_setstate_80211	(struct ndis_softc *);
static void ndis_media_status	(struct ifnet *, struct ifmediareq *);

static void ndis_setmulti	(struct ndis_softc *);
static void ndis_map_sclist	(void *, bus_dma_segment_t *,
	int, bus_size_t, int);

int ndis_in_isr;

#ifdef _MODULE

static int ndisdrv_loaded = 0;
int ndisdrv_modevent(module_t,  int);

MODULE(MODULE_CLASS_DRIVER, ndisdrv_modevent, NULL);
/*
 * This routine should call windrv_load() once for each driver
 * image. This will do the relocation and dynalinking for the
 * image, and create a Windows driver object which will be
 * saved in our driver database.
 */ 
int
ndisdrv_modevent(module_t mod, int cmd)
{
	int			error = 0;

#ifdef NDIS_DBG
	printf("in ndisdrv_modevent\n");
#endif
	switch (cmd) {
	case MODULE_CMD_INIT:
		ndisdrv_loaded++;
                if (ndisdrv_loaded > 1)
			break;
		error = windrv_load(mod, (vm_offset_t)drv_data, 0);
		windrv_wrap((funcptr)ndis_rxeof, &ndis_rxeof_wrap);
		windrv_wrap((funcptr)ndis_txeof, &ndis_txeof_wrap);
		windrv_wrap((funcptr)ndis_linksts, &ndis_linksts_wrap);
		windrv_wrap((funcptr)ndis_linksts_done,
		    &ndis_linksts_done_wrap);
		break;
	case MODULE_CMD_FINI:
		ndisdrv_loaded--;
		if (ndisdrv_loaded > 0)
			break;
		windrv_unload(mod, (vm_offset_t)drv_data, 0);
		windrv_unwrap(ndis_rxeof_wrap);
		windrv_unwrap(ndis_txeof_wrap);
		windrv_unwrap(ndis_linksts_wrap);
		windrv_unwrap(ndis_linksts_done_wrap);
		break;
	case MODULE_CMD_STAT:
		error = ENOTTY;
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

int if_ndis_lkmentry(struct lkm_table *lkmtp, int cmd, int ver);

CFDRIVER_DECL(ndis, DV_DULL, NULL);
extern struct cfattach ndis_ca;

static int pciloc[] = { -1, -1 }; /* device, function */
static struct cfparent pciparent = {
	"pci", "pci", DVUNIT_ANY
};
static struct cfdata ndis_cfdata[] = {
	{"ndis", "ndis", 0, FSTATE_STAR, pciloc, 0, &pciparent, 0},
	{ 0 }
};

static struct cfdriver *ndis_cfdrivers[] = {
	&ndis_cd,
	NULL
};
static struct cfattach *ndis_cfattachs[] = {
	&ndis_ca,
	NULL
};
static const struct cfattachlkminit ndis_cfattachinit[] = {
	{ "ndis", ndis_cfattachs },
	{ NULL }
};

MOD_DRV("ndis", ndis_cfdrivers, ndis_cfattachinit,
	ndis_cfdata);

int
if_ndis_lkmentry(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, lkm_nofunc, lkm_nofunc, lkm_nofunc);
}

#endif /* _MODULE */

/*
 * Program the 64-bit multicast hash filter.
 */
static void
ndis_setmulti(struct ndis_softc *sc)
{
	struct ifnet		*ifp;
	struct ether_multi	*ifma;
	int			len, mclistsz, error;
	uint8_t			*mclist;

	ifp = &sc->arpcom.ac_if;

	if (!NDIS_INITIALIZED(sc))
		return;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		sc->ndis_filter |= NDIS_PACKET_TYPE_ALL_MULTICAST;
		len = sizeof(sc->ndis_filter);
		error = ndis_set_info(sc, OID_GEN_CURRENT_PACKET_FILTER,
		    &sc->ndis_filter, &len);
        if (error) {
		aprint_error_dev(sc->ndis_dev, "set filter failed: %d\n", 
			     error);
        }
		return;
	}

	if (LIST_EMPTY(&sc->arpcom.ec_multiaddrs))
		return;

	len = sizeof(mclistsz);
	ndis_get_info(sc, OID_802_3_MAXIMUM_LIST_SIZE, &mclistsz, &len);

	mclist = malloc(ETHER_ADDR_LEN * mclistsz, M_TEMP, M_NOWAIT|M_ZERO);

	if (mclist == NULL) {
		sc->ndis_filter |= NDIS_PACKET_TYPE_ALL_MULTICAST;
		goto out;
	}

	sc->ndis_filter |= NDIS_PACKET_TYPE_MULTICAST;

	len = 0;
	LIST_FOREACH(ifma, &sc->arpcom.ec_multiaddrs, enm_list) {
/*
 *****************************************************************************
 * TODO: The NetBSD ether_multi structure (sys/net/if_ether.h) defines a range 
 * of addresses TODO: (enm_addrlo to enm_addrhi), but FreeBSD's ifmultiaddr 
 * structure (in sys/net/if_var.h) defines only a single address.  Do we need 
 * to add every address in the range to the list?  Seems like it to me.  
 * But for right now I'm assuming there is only a single address.
 *****************************************************************************
 */
		memcpy(mclist + (ETHER_ADDR_LEN * len), ifma->enm_addrlo,
			ETHER_ADDR_LEN);
		len++;
		if (len > mclistsz) {
			sc->ndis_filter |= NDIS_PACKET_TYPE_ALL_MULTICAST;
			sc->ndis_filter &= ~NDIS_PACKET_TYPE_MULTICAST;
			goto out;
		}
	}

	len = len * ETHER_ADDR_LEN;
	error = ndis_set_info(sc, OID_802_3_MULTICAST_LIST, mclist, &len);
	if (error) {    
		aprint_error_dev(sc->ndis_dev, "set mclist failed: %d\n", 
			     error);
		sc->ndis_filter |= NDIS_PACKET_TYPE_ALL_MULTICAST;
		sc->ndis_filter &= ~NDIS_PACKET_TYPE_MULTICAST;
	}

out:
	free(mclist, M_TEMP);

	len = sizeof(sc->ndis_filter);
	error = ndis_set_info(sc, OID_GEN_CURRENT_PACKET_FILTER,
	    &sc->ndis_filter, &len);
    if (error) {
		aprint_error_dev(sc->ndis_dev, "set filter failed: %d\n", 
			     error);
    }

	return;
}

static int
ndis_set_offload(struct ndis_softc *sc)
{
	ndis_task_offload	*nto;
	ndis_task_offload_hdr	*ntoh;
	ndis_task_tcpip_csum	*nttc;
	struct ifnet		*ifp;
	int			len, error;

	ifp = &sc->arpcom.ac_if;

	if (!NDIS_INITIALIZED(sc))
		return(EINVAL);

	/* See if there's anything to set. */

	error = ndis_probe_offload(sc);
	if (error)
		return(error);
		
	if (sc->ndis_hwassist == 0 && ifp->if_capabilities == 0)
		return(0);

	len = sizeof(ndis_task_offload_hdr) + sizeof(ndis_task_offload) +
	    sizeof(ndis_task_tcpip_csum);

	ntoh = malloc(len, M_TEMP, M_NOWAIT|M_ZERO);

	if (ntoh == NULL)
		return(ENOMEM);

	ntoh->ntoh_vers = NDIS_TASK_OFFLOAD_VERSION;
	ntoh->ntoh_len = sizeof(ndis_task_offload_hdr);
	ntoh->ntoh_offset_firsttask = sizeof(ndis_task_offload_hdr);
	ntoh->ntoh_encapfmt.nef_encaphdrlen = sizeof(struct ether_header);
	ntoh->ntoh_encapfmt.nef_encap = NDIS_ENCAP_IEEE802_3;
	ntoh->ntoh_encapfmt.nef_flags = NDIS_ENCAPFLAG_FIXEDHDRLEN;

	nto = (ndis_task_offload *)((char *)ntoh +
	    ntoh->ntoh_offset_firsttask);

	nto->nto_vers = NDIS_TASK_OFFLOAD_VERSION;
	nto->nto_len = sizeof(ndis_task_offload);
	nto->nto_task = NDIS_TASK_TCPIP_CSUM;
	nto->nto_offset_nexttask = 0;
	nto->nto_taskbuflen = sizeof(ndis_task_tcpip_csum);

	nttc = (ndis_task_tcpip_csum *)nto->nto_taskbuf;

	if (ifp->if_capenable & IFCAP_TXCSUM)
		nttc->nttc_v4tx = sc->ndis_v4tx;

	if (ifp->if_capenable & IFCAP_RXCSUM)
		nttc->nttc_v4rx = sc->ndis_v4rx;

	error = ndis_set_info(sc, OID_TCP_TASK_OFFLOAD, ntoh, &len);
	free(ntoh, M_TEMP);

	return(error);
}

static int
ndis_probe_offload(struct ndis_softc *sc)
{
	ndis_task_offload	*nto;
	ndis_task_offload_hdr	*ntoh;
	ndis_task_tcpip_csum	*nttc = NULL;
	struct ifnet		*ifp;
	int			len, error, dummy;

	ifp = &sc->arpcom.ac_if;

	len = sizeof(dummy);
	error = ndis_get_info(sc, OID_TCP_TASK_OFFLOAD, &dummy, &len);

	if (error != ENOSPC)
		return(error);

	ntoh = malloc(len, M_TEMP, M_NOWAIT|M_ZERO);

	if (ntoh == NULL)
		return(ENOMEM);

	ntoh->ntoh_vers = NDIS_TASK_OFFLOAD_VERSION;
	ntoh->ntoh_len = sizeof(ndis_task_offload_hdr);
	ntoh->ntoh_encapfmt.nef_encaphdrlen = sizeof(struct ether_header);
	ntoh->ntoh_encapfmt.nef_encap = NDIS_ENCAP_IEEE802_3;
	ntoh->ntoh_encapfmt.nef_flags = NDIS_ENCAPFLAG_FIXEDHDRLEN;

	error = ndis_get_info(sc, OID_TCP_TASK_OFFLOAD, ntoh, &len);

	if (error) {
		free(ntoh, M_TEMP);
		return(error);
	}

	if (ntoh->ntoh_vers != NDIS_TASK_OFFLOAD_VERSION) {
		free(ntoh, M_TEMP);
		return(EINVAL);
	}

	nto = (ndis_task_offload *)((char *)ntoh +
	    ntoh->ntoh_offset_firsttask);

	while (1) {
		switch (nto->nto_task) {
		case NDIS_TASK_TCPIP_CSUM:
			nttc = (ndis_task_tcpip_csum *)nto->nto_taskbuf;
			break;
		/* Don't handle these yet. */
		case NDIS_TASK_IPSEC:
		case NDIS_TASK_TCP_LARGESEND:
		default:
			break;
		}
		if (nto->nto_offset_nexttask == 0)
			break;
		nto = (ndis_task_offload *)((char *)nto +
		    nto->nto_offset_nexttask);
	}

	if (nttc == NULL) {
		free(ntoh, M_TEMP);
		return(ENOENT);
	}

	sc->ndis_v4tx = nttc->nttc_v4tx;
	sc->ndis_v4rx = nttc->nttc_v4rx;

	if (nttc->nttc_v4tx & NDIS_TCPSUM_FLAGS_IP_CSUM)
		sc->ndis_hwassist |= CSUM_IP;
	if (nttc->nttc_v4tx & NDIS_TCPSUM_FLAGS_TCP_CSUM)
		sc->ndis_hwassist |= CSUM_TCP;
	if (nttc->nttc_v4tx & NDIS_TCPSUM_FLAGS_UDP_CSUM)
		sc->ndis_hwassist |= CSUM_UDP;
	if (sc->ndis_hwassist)
		ifp->if_capabilities |= IFCAP_TXCSUM;	
		
	if (nttc->nttc_v4rx & NDIS_TCPSUM_FLAGS_IP_CSUM)
		ifp->if_capabilities |= IFCAP_RXCSUM;
	if (nttc->nttc_v4rx & NDIS_TCPSUM_FLAGS_TCP_CSUM)
		ifp->if_capabilities |= IFCAP_RXCSUM;
	if (nttc->nttc_v4rx & NDIS_TCPSUM_FLAGS_UDP_CSUM)
		ifp->if_capabilities |= IFCAP_RXCSUM;	

	free(ntoh, M_TEMP);
	return(0);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void
ndis_attach(dev)
	void			*dev;
{
	u_char			eaddr[ETHER_ADDR_LEN];
	struct ndis_softc	*sc;
	driver_object		*drv;
	driver_object		*pdrv;
	device_object		*pdo;
	struct ifnet		*ifp = NULL;
	void			*img;
	int			error = 0, len;
	int			j;
	
#ifdef NDIS_DBG
	printf("In ndis_attach()\n");
#endif
		
	sc = device_get_softc(dev);
	
	/* start out at dispatch level */
	win_irql = DISPATCH_LEVEL;

	mutex_init(&sc->ndis_mtx, MUTEX_DEFAULT, IPL_NET);

	/*
	 * Hook interrupt early, since calling the driver's
	 * init routine may trigger an interrupt. Note that
	 * we don't need to do any explicit interrupt setup
	 * for USB.
	 */
	/* 
	 * For NetBSD, the interrupt is set up in the bus-dependent
	 * code.  For PCI it's done in ndis_attach_pci()
	 */

/* 
 * TODO: remove this #ifdef once if_ndis_pcmcia.c compiles
 */

	sc->ndis_regvals = ndis_regvals;


	/* Create sysctl registry nodes */
	ndis_create_sysctls(sc);
	
	/* Find the PDO for this device instance. */
	if (sc->ndis_iftype == PCIBus)
		pdrv = windrv_lookup(0, "PCI Bus");
	else if (sc->ndis_iftype == PCMCIABus)
		pdrv = windrv_lookup(0, "PCCARD Bus");
	else
		pdrv = windrv_lookup(0, "USB Bus");
	pdo = windrv_find_pdo(pdrv, device_parent(sc->ndis_dev));


	/*
	 * Create a new functional device object for this
	 * device. This is what creates the miniport block
	 * for this device instance.
	 */ 
	 
	img = drv_data;
	drv = windrv_lookup((vm_offset_t)img, NULL);
	/* 
	 * Stash a pointer to the softc in the Windows device_object, since 
	 * we can't get it from the NetBSD device structure.
	 */
	pdo->pdo_sc = sc;
	pdo->fdo_sc = sc;
	
	if (NdisAddDevice(drv, pdo) != STATUS_SUCCESS) {
		aprint_error_dev(sc->ndis_dev, "failed to create FDO!\n");
		error = ENXIO;
		goto fail;
	}

	/* Tell the user what version of the API the driver is using. */
	aprint_normal_dev(sc->ndis_dev, "NDIS API version: %d.%d\n",
		      sc->ndis_chars->nmc_version_major,
		      sc->ndis_chars->nmc_version_minor);

	/* 
	 * For NetBSD so far we do the resource conversion directly in
	 * ndis_attach_pci()
	 */

	/* Install our RX and TX interrupt handlers. */
	sc->ndis_block->nmb_senddone_func = ndis_txeof_wrap;
	sc->ndis_block->nmb_pktind_func = ndis_rxeof_wrap;
	
	/* Set up the resource list in the block */
	sc->ndis_block->nmb_rlist = sc->ndis_rl;	
	/* sc->ndis_block->nmb_rlist = &sc->ndis_rl; */
	
	/* TODO: Free this memory! */
	sc->arpcom.ec_if.if_sadl = 
		malloc(sizeof(struct sockaddr_dl), M_DEVBUF, M_NOWAIT|M_ZERO);

	/* Call driver's init routine. */
	if (ndis_init_nic(sc)) {
		aprint_error_dev(sc->ndis_dev, "init handler failed\n");
		error = ENXIO;
		goto fail;
	}

	/*
	 * Get station address from the driver.
	 */
	len = sizeof(eaddr);
	ndis_get_info(sc, OID_802_3_CURRENT_ADDRESS, &eaddr, &len);

	/*
	 * Figure out if we're allowed to use multipacket sends
	 * with this driver, and if so, how many.
	 */

	if (sc->ndis_chars->nmc_sendsingle_func &&
	    sc->ndis_chars->nmc_sendmulti_func == NULL) {
		sc->ndis_maxpkts = 1;
	} else {
		len = sizeof(sc->ndis_maxpkts);
		ndis_get_info(sc, OID_GEN_MAXIMUM_SEND_PACKETS,
		    &sc->ndis_maxpkts, &len);
	}

	sc->ndis_txarray = malloc(sizeof(ndis_packet *) *
	    sc->ndis_maxpkts, M_DEVBUF, M_NOWAIT|M_ZERO);

	/* Allocate a pool of ndis_packets for TX encapsulation. */

	NdisAllocatePacketPool(&j, &sc->ndis_txpool,
	   sc->ndis_maxpkts, PROTOCOL_RESERVED_SIZE_IN_PACKET);

	if (j != NDIS_STATUS_SUCCESS) {
		sc->ndis_txpool = NULL;
		aprint_error_dev(sc->ndis_dev, "failed to allocate TX packet pool");
		error = ENOMEM;
		goto fail;
	}

	sc->ndis_txpending = sc->ndis_maxpkts;

	sc->ndis_oidcnt = 0;
	/* Get supported oid list. */
	ndis_get_supported_oids(sc, &sc->ndis_oids, &sc->ndis_oidcnt);

	/* If the NDIS module requested scatter/gather, init maps. */
	if (sc->ndis_sc)
		ndis_init_dma(sc);

	/*
	 * See if the OID_802_11_CONFIGURATION OID is
	 * supported by this driver. If it is, then this an 802.11
	 * wireless driver, and we should set up media for wireless.
	 */
	for (j = 0; j < sc->ndis_oidcnt; j++) {
		if (sc->ndis_oids[j] == OID_802_11_CONFIGURATION) {
			sc->ndis_80211++;
			break;
		}
	}

	/* Check for task offload support. */
	ndis_probe_offload(sc);

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	
	sc->ic.ic_ifp = ifp;
	
	strlcpy(ifp->if_xname, device_xname(sc->ndis_dev), IFNAMSIZ);
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ndis_ioctl;
	ifp->if_start = ndis_start;
	ifp->if_watchdog = ndis_watchdog;
	ifp->if_init = ndis_init;
	ifp->if_baudrate = 10000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, 50);
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_capenable = ifp->if_capabilities;
	/* 
	 * TODO: I don't think NetBSD has this field describing "HW offload 
	 * capabilities" as found in FreeBSD's 
	 * if_data structure, but maybe there is something else that 
	 * needs to be done here for NetBSD
	 */

	/* Do media setup */
	if (sc->ndis_80211) {
		struct ieee80211com *ic = &sc->ic;
		ndis_80211_rates_ex	rates;
		struct ndis_80211_nettype_list *ntl;
		uint32_t		arg;
		int			r;

	    ic->ic_phytype = IEEE80211_T_DS;
		ic->ic_opmode = IEEE80211_M_STA;
		ic->ic_caps = IEEE80211_C_IBSS;
		ic->ic_state = IEEE80211_S_ASSOC;
		ic->ic_modecaps = (1<<IEEE80211_MODE_AUTO);
		len = 0;
		r = ndis_get_info(sc, OID_802_11_NETWORK_TYPES_SUPPORTED,
		    NULL, &len);
		if (r != ENOSPC)
			goto nonettypes;
		ntl = malloc(len, M_DEVBUF, M_WAITOK|M_ZERO);
		r = ndis_get_info(sc, OID_802_11_NETWORK_TYPES_SUPPORTED,
		    ntl, &len);
		if (r != 0) {
			free(ntl, M_DEVBUF);
			goto nonettypes;
		}

		for (j = 0; j < ntl->ntl_items; j++) {
			switch (ntl->ntl_type[j]) {
			case NDIS_80211_NETTYPE_11FH:
			case NDIS_80211_NETTYPE_11DS:
				ic->ic_modecaps |= (1<<IEEE80211_MODE_11B);
				break;
			case NDIS_80211_NETTYPE_11OFDM5:
				ic->ic_modecaps |= (1<<IEEE80211_MODE_11A);
				break;
			case NDIS_80211_NETTYPE_11OFDM24:
				ic->ic_modecaps |= (1<<IEEE80211_MODE_11G);
				break;
			default:
				break;
			}
		}
		free(ntl, M_DEVBUF);
nonettypes:
		len = sizeof(rates);
		memset((char *)&rates, 0, len);
		r = ndis_get_info(sc, OID_802_11_SUPPORTED_RATES,
		    (void *)rates, &len);
		if (r)
			aprint_error_dev(sc->ndis_dev, "get rates failed: 0x%x\n", r);
		/*
		 * Since the supported rates only up to 8 can be supported,
		 * if this is not 802.11b we're just going to be faking it
		 * all up to heck.
		 */

#define TESTSETRATE(x, y)						\
	do {								\
		int			i;				\
		for (i = 0; i < ic->ic_sup_rates[x].rs_nrates; i++) {	\
			if (ic->ic_sup_rates[x].rs_rates[i] == (y))	\
				break;					\
		}							\
		if (i == ic->ic_sup_rates[x].rs_nrates) {		\
			ic->ic_sup_rates[x].rs_rates[i] = (y);		\
			ic->ic_sup_rates[x].rs_nrates++;		\
		}							\
	} while (0)

#define SETRATE(x, y)	\
	ic->ic_sup_rates[x].rs_rates[ic->ic_sup_rates[x].rs_nrates] = (y)
#define INCRATE(x)	\
	ic->ic_sup_rates[x].rs_nrates++

		ic->ic_curmode = IEEE80211_MODE_AUTO;
		if (ic->ic_modecaps & (1<<IEEE80211_MODE_11A))
			ic->ic_sup_rates[IEEE80211_MODE_11A].rs_nrates = 0;
		if (ic->ic_modecaps & (1<<IEEE80211_MODE_11B))
			ic->ic_sup_rates[IEEE80211_MODE_11B].rs_nrates = 0;
		if (ic->ic_modecaps & (1<<IEEE80211_MODE_11G))
			ic->ic_sup_rates[IEEE80211_MODE_11G].rs_nrates = 0;
		for (j = 0; j < len; j++) {
			switch (rates[j] & IEEE80211_RATE_VAL) {
			case 2:
			case 4:
			case 11:
			case 10:
			case 22:
				if (!(ic->ic_modecaps &
				    (1<<IEEE80211_MODE_11B))) {
					/* Lazy-init 802.11b. */
					ic->ic_modecaps |=
					    (1<<IEEE80211_MODE_11B);
					ic->ic_sup_rates[IEEE80211_MODE_11B].
					    rs_nrates = 0;
				}
				SETRATE(IEEE80211_MODE_11B, rates[j]);
				INCRATE(IEEE80211_MODE_11B);
				break;
			default:
				if (ic->ic_modecaps & (1<<IEEE80211_MODE_11A)) {
					SETRATE(IEEE80211_MODE_11A, rates[j]);
					INCRATE(IEEE80211_MODE_11A);
				}
				if (ic->ic_modecaps & (1<<IEEE80211_MODE_11G)) {
					SETRATE(IEEE80211_MODE_11G, rates[j]);
					INCRATE(IEEE80211_MODE_11G);
				}
				break;
			}
		}

		/*
		 * If the hardware supports 802.11g, it most
		 * likely supports 802.11b and all of the
		 * 802.11b and 802.11g speeds, so maybe we can
		 * just cheat here.  Just how in the heck do
		 * we detect turbo modes, though?
		 */
		if (ic->ic_modecaps & (1<<IEEE80211_MODE_11B)) {
			TESTSETRATE(IEEE80211_MODE_11B,
			    IEEE80211_RATE_BASIC|2);
			TESTSETRATE(IEEE80211_MODE_11B,
			    IEEE80211_RATE_BASIC|4);
			TESTSETRATE(IEEE80211_MODE_11B,
			    IEEE80211_RATE_BASIC|11);
			TESTSETRATE(IEEE80211_MODE_11B,
			    IEEE80211_RATE_BASIC|22);
		}
		if (ic->ic_modecaps & (1<<IEEE80211_MODE_11G)) {
			TESTSETRATE(IEEE80211_MODE_11G, 47);
			TESTSETRATE(IEEE80211_MODE_11G, 72);
			TESTSETRATE(IEEE80211_MODE_11G, 96);
			TESTSETRATE(IEEE80211_MODE_11G, 108);
		}
		if (ic->ic_modecaps & (1<<IEEE80211_MODE_11A)) {
			TESTSETRATE(IEEE80211_MODE_11A, 47);
			TESTSETRATE(IEEE80211_MODE_11A, 72);
			TESTSETRATE(IEEE80211_MODE_11A, 96);
			TESTSETRATE(IEEE80211_MODE_11A, 108);
		}
#undef SETRATE
#undef INCRATE
		/*
		 * Taking yet more guesses here.
		 */
		for (j = 1; j < IEEE80211_CHAN_MAX; j++) {
			int chanflag = 0;

			if (ic->ic_sup_rates[IEEE80211_MODE_11G].rs_nrates)
				chanflag |= IEEE80211_CHAN_G;
			if (j <= 14)
				chanflag |= IEEE80211_CHAN_B;
			if (ic->ic_sup_rates[IEEE80211_MODE_11A].rs_nrates &&
			    j > 14)
				chanflag = IEEE80211_CHAN_A;
			if (chanflag == 0)
				break;
			ic->ic_channels[j].ic_freq =
			    ieee80211_ieee2mhz(j, chanflag);
			ic->ic_channels[j].ic_flags = chanflag;
		}

		j = sizeof(arg);
		r = ndis_get_info(sc, OID_802_11_WEP_STATUS, &arg, &j);
		if (arg != NDIS_80211_WEPSTAT_NOTSUPPORTED)
			ic->ic_caps |= IEEE80211_C_WEP;
		j = sizeof(arg);
		r = ndis_get_info(sc, OID_802_11_POWER_MODE, &arg, &j);
		if (r == 0)
			ic->ic_caps |= IEEE80211_C_PMGT;
		memcpy(&ic->ic_myaddr, eaddr, sizeof(eaddr));
		if_attach(ifp);
		ieee80211_ifattach(&sc->ic);
		ieee80211_media_init(&sc->ic, ieee80211_media_change,
		    ndis_media_status);		

		ic->ic_ibss_chan = IEEE80211_CHAN_ANYC;
		ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	} else {
		ifmedia_init(&sc->ifmedia, IFM_IMASK, ndis_ifmedia_upd,
		    ndis_ifmedia_sts);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->ifmedia,
		    IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		ifmedia_set(&sc->ifmedia, IFM_ETHER|IFM_AUTO);
		if_attach(ifp);
		ether_ifattach(ifp, eaddr);
	}

	/* Override the status handler so we can detect link changes. */
	sc->ndis_block->nmb_status_func = ndis_linksts_wrap;
	sc->ndis_block->nmb_statusdone_func = ndis_linksts_done_wrap;
fail:
	if (error)
		ndis_detach(dev, 0);
	else
		/* We're done talking to the NIC for now; halt it. */
		ndis_halt_nic(sc);

	return;
}

/*
 * Shutdown hardware and free up resources. This can be called any
 * time after the mutex has been initialized. It is called in both
 * the error case in attach and the normal detach case so it needs
 * to be careful about only freeing resources that have actually been
 * allocated.
 */
int
ndis_detach (dev, flags)
	device_t 		dev;
	int			flags;
{
	struct ndis_softc	*sc;
	struct ifnet		*ifp;
	driver_object		*drv;
	int			s;

#ifdef NDIS_DBG
	printf("in ndis_detach\n");
#endif

	sc = device_get_softc(dev);

	NDIS_LOCK(sc);

	ifp = &sc->arpcom.ac_if;
	ifp->if_flags &= ~IFF_UP;

	if (device_is_attached(dev)) {
		NDIS_UNLOCK(sc);	
		ndis_stop(sc);
		if (sc->ndis_80211)
			ieee80211_ifdetach(&sc->ic);
		else
			ether_ifdetach(ifp);
	} else {
		NDIS_UNLOCK(sc);	
	}


/* 
 * TODO: unmap interrupts when unloading in NetBSD
 */
	if (sc->ndis_res_io)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    sc->ndis_io_rid, sc->ndis_res_io);
	if (sc->ndis_res_mem)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->ndis_mem_rid, sc->ndis_res_mem);
	if (sc->ndis_sc)
		ndis_destroy_dma(sc);

	if (sc->ndis_txarray)
		free(sc->ndis_txarray, M_DEVBUF);


	ndis_unload_driver((void *)ifp);

	if (sc->ndis_txpool != NULL)
		NdisFreePacketPool(sc->ndis_txpool);

	/* Destroy the PDO for this device. */
	
	if (sc->ndis_iftype == PCIBus)
		drv = windrv_lookup(0, "PCI Bus");
	else if (sc->ndis_iftype == PCMCIABus)
		drv = windrv_lookup(0, "PCCARD Bus");
	else
		drv = windrv_lookup(0, "USB Bus");
	if (drv == NULL)
		panic("couldn't find driver object");
	windrv_destroy_pdo(drv, dev);

/* 
 * TODO: Unmap dma for NetBSD
 */
	mutex_destroy(&sc->ndis_mtx);

	return(0);
}

/* TODO: write a NetBSD version of ndis_suspend() */

/* TODO: write a NetBSD version of ndis_resume() */

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 *
 * When handling received NDIS packets, the 'status' field in the
 * out-of-band portion of the ndis_packet has special meaning. In the
 * most common case, the underlying NDIS driver will set this field
 * to NDIS_STATUS_SUCCESS, which indicates that it's ok for us to
 * take posession of it. We then change the status field to
 * NDIS_STATUS_PENDING to tell the driver that we now own the packet,
 * and that we will return it at some point in the future via the
 * return packet handler.
 *
 * If the driver hands us a packet with a status of NDIS_STATUS_RESOURCES,
 * this means the driver is running out of packet/buffer resources and
 * wants to maintain ownership of the packet. In this case, we have to
 * copy the packet data into local storage and let the driver keep the
 * packet.
 */
__stdcall /*static*/ void
ndis_rxeof(ndis_handle adapter, ndis_packet **packets, uint32_t pktcnt)
{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
	ndis_packet		*p;
	uint32_t		s;
	ndis_tcpip_csum		*csum;
	struct ifnet		*ifp;
	struct mbuf		*m0, *m;
	int			i;

	block = (ndis_miniport_block *)adapter;
	sc = (struct ndis_softc *)block->nmb_physdeviceobj->pdo_sc;
	ifp = &sc->arpcom.ac_if;

	for (i = 0; i < pktcnt; i++) {
		p = packets[i];
		/* Stash the softc here so ptom can use it. */
		p->np_softc = sc;
		if (ndis_ptom(&m0, p)) {
			aprint_error_dev(sc->ndis_dev, "ptom failed\n");
			if (p->np_oob.npo_status == NDIS_STATUS_SUCCESS)
				ndis_return_packet(NULL, (void *)sc, 0, p);
		} else {
			if (p->np_oob.npo_status == NDIS_STATUS_RESOURCES) {
				m = m_dup(m0, 0, m0->m_pkthdr.len, FALSE);
				/*
				 * NOTE: we want to destroy the mbuf here, but
				 * we don't actually want to return it to the
				 * driver via the return packet handler. By
				 * bumping np_refcnt, we can prevent the
				 * ndis_return_packet() routine from actually
				 * doing anything.
				 */
				p->np_refcnt++;
				m_freem(m0);
				if (m == NULL)
					ifp->if_ierrors++;
				else
					m0 = m;
			} else
				p->np_oob.npo_status = NDIS_STATUS_PENDING;
			m0->m_pkthdr.rcvif = ifp;
			ifp->if_ipackets++;

			/* Deal with checksum offload. */
/* 
 * TODO: deal with checksum offload in NetBSD 
 * (see IFCAP_XXX in sys/net/if.h, these differ from the FreeBSD ones)
 */
			if (ifp->if_capenable & IFCAP_RXCSUM &&
			    p->np_ext.npe_info[ndis_tcpipcsum_info] != NULL) {
				s = (uintptr_t)
			 	    p->np_ext.npe_info[ndis_tcpipcsum_info];
				csum = (ndis_tcpip_csum *)&s;
				if (!(csum->u.ntc_rxflags &
				    NDIS_RXCSUM_IP_PASSED))
					m0->m_pkthdr.csum_flags |=
					    M_CSUM_IPv4_BAD;
				if (csum->u.ntc_rxflags &
				    (NDIS_RXCSUM_TCP_PASSED |
				    NDIS_RXCSUM_UDP_PASSED)) {
					//m0->m_pkthdr.csum_flags |=
					  //  CSUM_DATA_VALID|CSUM_PSEUDO_HDR;
					m0->m_pkthdr.csum_data = 0xFFFF;
				}
			}			

			bpf_mtap(ifp, m0);

			(*ifp->if_input)(ifp, m0);
		}
	}

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
__stdcall /*static*/ void
ndis_txeof(adapter, packet, status)
	ndis_handle		adapter;
	ndis_packet		*packet;
	ndis_status		status;

{
	struct ndis_softc	*sc;
	ndis_miniport_block	*block;
	struct ifnet		*ifp;
	int			idx;
	struct mbuf		*m;
	int			s;

	block = (ndis_miniport_block *)adapter;
	sc = (struct ndis_softc *)block->nmb_physdeviceobj->pdo_sc;
	ifp = &sc->arpcom.ac_if;

	m = packet->np_m0;
	idx = packet->np_txidx;
	if (sc->ndis_sc)
		bus_dmamap_unload(sc->ndis_ttag, sc->ndis_tmaps[idx]);

	ndis_free_packet(packet);
	m_freem(m);
	
	NDIS_LOCK(sc);

	sc->ndis_txarray[idx] = NULL;
	sc->ndis_txpending++;

	if (status == NDIS_STATUS_SUCCESS)
		ifp->if_opackets++;
	else
		ifp->if_oerrors++;
	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	NDIS_UNLOCK(sc);	

	ndis_sched(ndis_starttask, ifp, NDIS_TASKQUEUE);

	return;
}

__stdcall /*static*/ void
ndis_linksts(ndis_handle adapter, ndis_status status, void *sbuf, uint32_t slen)
{
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;

	block = adapter;
	sc = (struct ndis_softc *)block->nmb_physdeviceobj->pdo_sc;
	

	block->nmb_getstat = status;

	return;
}

__stdcall /*static*/ void
ndis_linksts_done(ndis_handle adapter)
{
	ndis_miniport_block	*block;
	struct ndis_softc	*sc;
	struct ifnet		*ifp;

	block = adapter;
	sc = (struct ndis_softc *)block->nmb_physdeviceobj->pdo_sc;
	ifp = &sc->arpcom.ac_if;

	if (!NDIS_INITIALIZED(sc))
		return;

	switch (block->nmb_getstat) {
	case NDIS_STATUS_MEDIA_CONNECT:
		ndis_sched(ndis_ticktask, sc, NDIS_TASKQUEUE);
		ndis_sched(ndis_starttask, ifp, NDIS_TASKQUEUE);
		break;
	case NDIS_STATUS_MEDIA_DISCONNECT:
		if (sc->ndis_link)
			ndis_sched(ndis_ticktask, sc, NDIS_TASKQUEUE);
		break;
	default:
		break;
	}

	return;
}

int
ndis_intr(arg)
	void			*arg;
{
	struct ndis_softc	*sc;
	struct ifnet		*ifp;
	int			is_our_intr = 0;
	int			call_isr = 0;
	uint8_t			irql;
	ndis_miniport_interrupt	*intr;
	
	sc = arg;
	ifp = &sc->arpcom.ac_if;

	/* 
	 * I was getting an interrupt before NdisAddDevice was called, 
	 * which sets up the ndis_block, so...
	 */
	if(sc->ndis_block == NULL) {
		return 0;
	}
	
	intr = sc->ndis_block->nmb_interrupt;

	if (sc->ndis_block->nmb_miniportadapterctx == NULL) {
		return 0;
	}			

	KeAcquireSpinLock(&intr->ni_dpccountlock, &irql);
	if (sc->ndis_block->nmb_interrupt->ni_isrreq == TRUE)
		ndis_isr(sc, &is_our_intr, &call_isr);
	else {
		ndis_disable_intr(sc);
		call_isr = 1;
	}
	KeReleaseSpinLock(&intr->ni_dpccountlock, irql);

	if ((is_our_intr || call_isr)) {
		ndis_in_isr = TRUE;
		IoRequestDpc(sc->ndis_block->nmb_deviceobj, NULL, sc);
		ndis_in_isr = FALSE;
	}

	return 0;
}

/* 
 * just here so I can wake up the SWI thread
 * in ndis_ticktask
 */
struct ndisproc {
	struct ndisqhead	*np_q;
	struct proc		*np_p;
	int			np_state;
	uint8_t			np_stack[PAGE_SIZE*NDIS_KSTACK_PAGES];
	int			np_needs_wakeup;
};
extern struct ndisproc ndis_iproc;

static void
ndis_tick(void *xsc)
{
	struct ndis_softc	*sc;

	/* TODO: do we need the lock for NetBSD? */

	sc = xsc;

	ndis_sched(ndis_ticktask, sc, NDIS_TASKQUEUE);
	
	callout_reset(&sc->ndis_stat_ch, hz * 
		sc->ndis_block->nmb_checkforhangsecs, ndis_tick, sc);


	return;
}

static void
ndis_ticktask(void *xsc)
{
	struct ndis_softc	*sc;
	__stdcall ndis_checkforhang_handler hangfunc;
	uint8_t			rval;
	ndis_media_state	linkstate;
	int			error, len;
	int			s;

	sc = xsc;

	hangfunc = sc->ndis_chars->nmc_checkhang_func;

	if (hangfunc != NULL) {
		rval = MSCALL1(hangfunc,
		    sc->ndis_block->nmb_miniportadapterctx);
		if (rval == TRUE) {
			ndis_reset_nic(sc);
			return;
		}
	}

	len = sizeof(linkstate);
	error = ndis_get_info(sc, OID_GEN_MEDIA_CONNECT_STATUS,
	    (void *)&linkstate, &len);

	NDIS_LOCK(sc);

	if (sc->ndis_link == 0 && linkstate == nmc_connected) {
		aprint_normal_dev(sc->ndis_dev, "link up\n");
		sc->ndis_link = 1;
	
		NDIS_UNLOCK(sc);
		if (sc->ndis_80211)
			ndis_getstate_80211(sc);
		NDIS_LOCK(sc);
		
#ifdef LINK_STATE_UP
		sc->arpcom.ac_if.if_link_state = LINK_STATE_UP;
		rt_ifmsg(&(sc->arpcom.ac_if));
#endif /* LINK_STATE_UP */
	}

	if (sc->ndis_link == 1 && linkstate == nmc_disconnected) {
		aprint_normal_dev(sc->ndis_dev, "link down\n");
		sc->ndis_link = 0;
#ifdef LINK_STATE_DOWN
		sc->arpcom.ac_if.if_link_state = LINK_STATE_DOWN;
		rt_ifmsg(&(sc->arpcom.ac_if));
#endif /* LINK_STATE_DOWN */
	}

	NDIS_UNLOCK(sc);

	return;
}

static void
ndis_map_sclist(arg, segs, nseg, mapsize, error)
	void			*arg;
	bus_dma_segment_t	*segs;
	int			nseg;
	bus_size_t		mapsize;
	int			error;

{
	struct ndis_sc_list	*sclist;
	int			i;

	if (error || arg == NULL)
		return;

	sclist = arg;

	sclist->nsl_frags = nseg;

	for (i = 0; i < nseg; i++) {
		sclist->nsl_elements[i].nse_addr.np_quad = segs[i].ds_addr;
		sclist->nsl_elements[i].nse_len = segs[i].ds_len;
	}

	return;
}

static void
ndis_starttask(void *arg)
{
	struct ifnet		*ifp;

	ifp = arg;
	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		ndis_start(ifp);
	return;
}

/*
 * Main transmit routine. To make NDIS drivers happy, we need to
 * transform mbuf chains into NDIS packets and feed them to the
 * send packet routines. Most drivers allow you to send several
 * packets at once (up to the maxpkts limit). Unfortunately, rather
 * that accepting them in the form of a linked list, they expect
 * a contiguous array of pointers to packets.
 *
 * For those drivers which use the NDIS scatter/gather DMA mechanism,
 * we need to perform busdma work here. Those that use map registers
 * will do the mapping themselves on a buffer by buffer basis.
 */
static void
ndis_start(struct ifnet *ifp)
{
	struct ndis_softc	*sc;
	struct mbuf		*m = NULL;
	ndis_packet		**p0 = NULL, *p = NULL;
    ndis_tcpip_csum		*csum;
	int			pcnt = 0, status;
	int			s;

	sc = ifp->if_softc;
		
	NDIS_LOCK(sc);

	if (!sc->ndis_link || ifp->if_flags & IFF_OACTIVE) {	
		NDIS_UNLOCK(sc);	
		return;
	}

	p0 = &sc->ndis_txarray[sc->ndis_txidx];

	while(sc->ndis_txpending) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		NdisAllocatePacket(&status,
		    &sc->ndis_txarray[sc->ndis_txidx], sc->ndis_txpool);

		if (status != NDIS_STATUS_SUCCESS)
			break;

		if (ndis_mtop(m, &sc->ndis_txarray[sc->ndis_txidx])) {
#if __FreeBSD_version >= 502114
			IFQ_DRV_PREPEND(&ifp->if_snd, m);
#endif
			NDIS_UNLOCK(sc);
#if __FreeBSD_version < 502114
			IF_PREPEND(&ifp->if_snd, m);
#endif
			return;
		}

		/*
		 * Save pointer to original mbuf
		 * so we can free it later.
		 */

		p = sc->ndis_txarray[sc->ndis_txidx];
		p->np_txidx = sc->ndis_txidx;
		p->np_m0 = m;
		p->np_oob.npo_status = NDIS_STATUS_PENDING;

		/*
		 * Do scatter/gather processing, if driver requested it.
		 */
		if (sc->ndis_sc) {
/* 
 * TODO: NetBSD's bus_dmamap_load_mbuf dosen't provide a callback function 
 * argumet as FreeBSD's does figure out what to do about this. 
 */
			bus_dmamap_load_mbuf(sc->ndis_ttag,
			    sc->ndis_tmaps[sc->ndis_txidx], m,
			    BUS_DMA_WRITE|BUS_DMA_NOWAIT);
			/* Just call the callback function ? */
			ndis_map_sclist(&p->np_sclist, 
					sc->ndis_tmaps[sc->ndis_txidx]->dm_segs,
					sc->ndis_tmaps[sc->ndis_txidx]->dm_nsegs, 
					sc->ndis_tmaps[sc->ndis_txidx]->dm_mapsize, 0);
/* 
 * TODO: Need an offset and length to pass to bus_dmamap_sync() (not needed in 
 * FreeBSD), I'm not sure I did this correctly, as man 9 bus_dma says that 
 * dm_segs is "an array of segments or a pointer to an array of segments". 
 */
			bus_dmamap_sync(sc->ndis_ttag,
			    sc->ndis_tmaps[sc->ndis_txidx],
			    sc->ndis_tmaps[sc->ndis_txidx]->dm_segs->ds_addr,
			    sc->ndis_tmaps[sc->ndis_txidx]->dm_segs->ds_len,
			    BUS_DMASYNC_PREREAD);			    
			p->np_ext.npe_info[ndis_sclist_info] = &p->np_sclist;
		}

		/* Handle checksum offload. */
		if (ifp->if_capenable & IFCAP_TXCSUM &&
		    m->m_pkthdr.csum_flags) {
			csum = (ndis_tcpip_csum *)
				&p->np_ext.npe_info[ndis_tcpipcsum_info];
			csum->u.ntc_txflags = NDIS_TXCSUM_DO_IPV4;
			if (m->m_pkthdr.csum_flags & CSUM_IP)
				csum->u.ntc_txflags |= NDIS_TXCSUM_DO_IP;
			if (m->m_pkthdr.csum_flags & CSUM_TCP)
				csum->u.ntc_txflags |= NDIS_TXCSUM_DO_TCP;
			if (m->m_pkthdr.csum_flags & CSUM_UDP)
				csum->u.ntc_txflags |= NDIS_TXCSUM_DO_UDP;
			p->np_private.npp_flags = NDIS_PROTOCOL_ID_TCP_IP;
		}

		NDIS_INC(sc);
		sc->ndis_txpending--;

		pcnt++;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		bpf_mtap(ifp, m);
		/*
		 * The array that p0 points to must appear contiguous,
		 * so we must not wrap past the end of sc->ndis_txarray[].
		 * If it looks like we're about to wrap, break out here
		 * so the this batch of packets can be transmitted, then
		 * wait for txeof to ask us to send the rest.
		 */

		if (sc->ndis_txidx == 0)
			break;
	}

	if (pcnt == 0) {	
		NDIS_UNLOCK(sc);	
		return;
	}

	if (sc->ndis_txpending == 0)
		ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
	
	NDIS_UNLOCK(sc);

	if (sc->ndis_maxpkts == 1)
		ndis_send_packet(sc, p);
	else
		ndis_send_packets(sc, p0, pcnt);		
		
	return;
}

static int
ndis_init(xsc)
	struct ifnet 		*xsc;
{
	struct ndis_softc	*sc  = xsc->if_softc;
	struct ifnet		*ifp = xsc;
	int 			s;
	int			i, error;
	
	/*
	 * Avoid reintializing the link unnecessarily.
	 * This should be dealt with in a better way by
	 * fixing the upper layer modules so they don't
	 * call ifp->if_init() quite as often.
	 */
	if (sc->ndis_link && sc->ndis_skip)
		return 0;

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	 ndis_stop(sc);
	 if (ndis_init_nic(sc)) {
		return 0;
	}

	/* Init our MAC address */

	/* Program the packet filter */

	sc->ndis_filter = NDIS_PACKET_TYPE_DIRECTED;

	if (ifp->if_flags & IFF_BROADCAST)
		sc->ndis_filter |= NDIS_PACKET_TYPE_BROADCAST;

	if (ifp->if_flags & IFF_PROMISC)
		sc->ndis_filter |= NDIS_PACKET_TYPE_PROMISCUOUS;

	i = sizeof(sc->ndis_filter);

	error = ndis_set_info(sc, OID_GEN_CURRENT_PACKET_FILTER,
	    &sc->ndis_filter, &i);

	if (error)
		aprint_error_dev(sc->ndis_dev, "set filter failed: %d\n", 
			     error);

	/*
	 * Program the multicast filter, if necessary.
	 */
	ndis_setmulti(sc);

	/* Setup task offload. */
	ndis_set_offload(sc);

	/* Enable interrupts. */
	ndis_enable_intr(sc);

	if (sc->ndis_80211)
		ndis_setstate_80211(sc);

	NDIS_LOCK(sc);	

	sc->ndis_txidx = 0;
	sc->ndis_txpending = sc->ndis_maxpkts;
	sc->ndis_link = 0;

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	
	NDIS_UNLOCK(sc);

	/*
	 * Some drivers don't set this value. The NDIS spec says
	 * the default checkforhang timeout is "approximately 2
	 * seconds." We use 3 seconds, because it seems for some
	 * drivers, exactly 2 seconds is too fast.
	 */

	if (sc->ndis_block->nmb_checkforhangsecs == 0)
		sc->ndis_block->nmb_checkforhangsecs = 3;

	callout_reset(&sc->ndis_stat_ch, 
		      hz * sc->ndis_block->nmb_checkforhangsecs,
		      ndis_tick, sc);

		return 0;
}

/*
 * Set media options.
 */
static int
ndis_ifmedia_upd(struct ifnet *ifp)
{
	struct ndis_softc		*sc;

	sc = ifp->if_softc;

	if (NDIS_INITIALIZED(sc))
		ndis_init(&sc->arpcom.ac_if);

	return(0);
}

/*
 * Report current media status.
 */
static void
ndis_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct ndis_softc	*sc;
	uint32_t		media_info;
	ndis_media_state	linkstate;
	int			error, len;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;
	sc = ifp->if_softc;

	if (!NDIS_INITIALIZED(sc))
		return;

	len = sizeof(linkstate);
	error = ndis_get_info(sc, OID_GEN_MEDIA_CONNECT_STATUS,
	    (void *)&linkstate, &len);

	len = sizeof(media_info);
	error = ndis_get_info(sc, OID_GEN_LINK_SPEED,
	    (void *)&media_info, &len);

	if (linkstate == nmc_connected)
		ifmr->ifm_status |= IFM_ACTIVE;

	switch(media_info) {
	case 100000:
		ifmr->ifm_active |= IFM_10_T;
		break;
	case 1000000:
		ifmr->ifm_active |= IFM_100_TX;
		break;
	case 10000000:
		ifmr->ifm_active |= IFM_1000_T;
		break;
	default:
		aprint_error_dev(sc->ndis_dev, "unknown speed: %d\n", 
			     media_info);
		break;
	}

	return;
}

/* TODO: Perhaps raise the IPL while in these wireless functions ? */

static void
ndis_setstate_80211(struct ndis_softc *sc)
{
	struct ieee80211com	*ic;
	ndis_80211_ssid		ssid;
	ndis_80211_config	config;
	ndis_80211_wep		wep;
	int			i, rval = 0, len;
	uint32_t		arg;
	struct ifnet		*ifp;
	
#define wk_len 		wk_keylen
#define ic_wep_txkey	ic_def_txkey

ic = &sc->ic;

/* TODO: are these equivelant? */	
	ifp = sc->ic.ic_ifp;

	if (!NDIS_INITIALIZED(sc))
		return;

	/* Set network infrastructure mode. */

	len = sizeof(arg);
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		arg = NDIS_80211_NET_INFRA_IBSS;
	else
		arg = NDIS_80211_NET_INFRA_BSS;

	rval = ndis_set_info(sc, OID_802_11_INFRASTRUCTURE_MODE, &arg, &len);

	if (rval)
		aprint_error_dev(sc->ndis_dev, "set infra failed: %d\n", rval);

	/* Set WEP */
/* TODO: Clean up these #ifdef's */	
	if (ic->ic_flags & IEEE80211_F_PRIVACY) {	
		for (i = 0; i < IEEE80211_WEP_NKID; i++) {		
			if (ic->ic_nw_keys[i].wk_len) {
				memset((char *)&wep, 0, sizeof(wep));
				wep.nw_keylen = ic->ic_nw_keys[i].wk_len;
#ifdef notdef
				/* 5 and 13 are the only valid key lengths */
				if (ic->ic_nw_keys[i].wk_len < 5)
					wep.nw_keylen = 5;
				else if (ic->ic_nw_keys[i].wk_len > 5 &&
				     ic->ic_nw_keys[i].wk_len < 13)
					wep.nw_keylen = 13;
#endif
				wep.nw_keyidx = i;
				wep.nw_length = (sizeof(uint32_t) * 3)
				    + wep.nw_keylen;
				if (i == ic->ic_wep_txkey)
					wep.nw_keyidx |= NDIS_80211_WEPKEY_TX;
				bcopy(ic->ic_nw_keys[i].wk_key,
				    wep.nw_keydata, wep.nw_length);
				len = sizeof(wep);
				rval = ndis_set_info(sc,
				    OID_802_11_ADD_WEP, &wep, &len);
				if (rval)
					aprint_error_dev(sc->ndis_dev, "set wepkey failed: %d\n", rval);
			}
		}
		arg = NDIS_80211_WEPSTAT_ENABLED;
		len = sizeof(arg);
		rval = ndis_set_info(sc, OID_802_11_WEP_STATUS, &arg, &len);
		if (rval)
			aprint_error_dev(sc->ndis_dev, "enable WEP failed: %d\n", 
				     rval);
			arg = NDIS_80211_PRIVFILT_8021XWEP;
		len = sizeof(arg);
		rval = ndis_set_info(sc, OID_802_11_PRIVACY_FILTER, &arg, &len);
#ifdef IEEE80211_WEP_8021X /*IEEE80211_F_WEPON*/
		/* Accept that we only have "shared" and 802.1x modes. */
		if (rval == 0) {
			if (arg == NDIS_80211_PRIVFILT_ACCEPTALL)
				ic->ic_wep_mode = IEEE80211_WEP_MIXED;
			else
				ic->ic_wep_mode = IEEE80211_WEP_8021X;
		}
#endif
		arg = NDIS_80211_AUTHMODE_OPEN;
	} else {
		arg = NDIS_80211_WEPSTAT_DISABLED;
		len = sizeof(arg);
		ndis_set_info(sc, OID_802_11_WEP_STATUS, &arg, &len);
		arg = NDIS_80211_AUTHMODE_OPEN;
	}

	len = sizeof(arg);
	rval = ndis_set_info(sc, OID_802_11_AUTHENTICATION_MODE, &arg, &len);

#ifdef notyet
	if (rval)
		aprint_error_dev(sc->ndis_dev, "set auth failed: %d\n", rval);
#endif

#ifdef notyet
	/* Set network type. */

	arg = 0;

	switch (ic->ic_curmode) {
	case IEEE80211_MODE_11A:
		arg = NDIS_80211_NETTYPE_11OFDM5;
		break;
	case IEEE80211_MODE_11B:
		arg = NDIS_80211_NETTYPE_11DS;
		break;
	case IEEE80211_MODE_11G:
		arg = NDIS_80211_NETTYPE_11OFDM24;
		break;
	default:
		aprint_error_dev(sc->ndis_dev, "unknown mode: %d\n",
			     ic->ic_curmode);
	}

	if (arg) {
		len = sizeof(arg);
		rval = ndis_set_info(sc, OID_802_11_NETWORK_TYPE_IN_USE,
		    &arg, &len);
		if (rval)
			aprint_error_dev(sc->ndis_dev, "set nettype failed: %d\n", 
				     rval);
	}
#endif

	len = sizeof(config);
	memset((char *)&config, 0, len);
	config.nc_length = len;
	config.nc_fhconfig.ncf_length = sizeof(ndis_80211_config_fh);
	rval = ndis_get_info(sc, OID_802_11_CONFIGURATION, &config, &len); 

	/*
	 * Some drivers expect us to initialize these values, so
	 * provide some defaults.
	 */
	if (config.nc_beaconperiod == 0)
		config.nc_beaconperiod = 100;
	if (config.nc_atimwin == 0)
		config.nc_atimwin = 100;
	if (config.nc_fhconfig.ncf_dwelltime == 0)
		config.nc_fhconfig.ncf_dwelltime = 200;

	if (rval == 0 && ic->ic_ibss_chan != IEEE80211_CHAN_ANYC) { 
		int chan, chanflag;

		chan = ieee80211_chan2ieee(ic, ic->ic_ibss_chan);
		chanflag = config.nc_dsconfig > 2500000 ? IEEE80211_CHAN_2GHZ :
		    IEEE80211_CHAN_5GHZ;
		if (chan != ieee80211_mhz2ieee(config.nc_dsconfig / 1000, 0)) {
			config.nc_dsconfig =
			    ic->ic_ibss_chan->ic_freq * 1000;
			ic->ic_bss->ni_chan = ic->ic_ibss_chan;
			len = sizeof(config);
			config.nc_length = len;
			config.nc_fhconfig.ncf_length =
			    sizeof(ndis_80211_config_fh);
			rval = ndis_set_info(sc, OID_802_11_CONFIGURATION,
			    &config, &len);
			if (rval)
				aprint_error_dev(sc->ndis_dev, "couldn't change "
					     "DS config to %ukHz: %d\n",
					     config.nc_dsconfig, 
					     rval);
		}
	} else if (rval)
		aprint_error_dev(sc->ndis_dev, "couldn't retrieve "
			     "channel info: %d\n", rval);

	/* Set SSID -- always do this last. */

	len = sizeof(ssid);
	memset((char *)&ssid, 0, len);
	ssid.ns_ssidlen = ic->ic_des_esslen;
	if (ssid.ns_ssidlen == 0) {
		ssid.ns_ssidlen = 1;
	} else
		memcpy(ssid.ns_ssid, ic->ic_des_essid, ssid.ns_ssidlen);
	rval = ndis_set_info(sc, OID_802_11_SSID, &ssid, &len);

	if (rval)
		aprint_error_dev(sc->ndis_dev, "set ssid failed: %d\n", rval);

	return;
}

static void
ndis_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
        struct ieee80211com *ic = (void *)ifp;
        struct ieee80211_node *ni = NULL;

        imr->ifm_status = IFM_AVALID;
        imr->ifm_active = IFM_IEEE80211;
        if (ic->ic_state == IEEE80211_S_RUN)
                imr->ifm_status |= IFM_ACTIVE;
        imr->ifm_active |= IFM_AUTO;
        switch (ic->ic_opmode) {
        case IEEE80211_M_STA:
                ni = ic->ic_bss;
                /* calculate rate subtype */
                imr->ifm_active |= ieee80211_rate2media(ic,
                        ni->ni_rates.rs_rates[ni->ni_txrate], ic->ic_curmode);
                break;
        case IEEE80211_M_IBSS:
                ni = ic->ic_bss;
                /* calculate rate subtype */
                imr->ifm_active |= ieee80211_rate2media(ic,
                        ni->ni_rates.rs_rates[ni->ni_txrate], ic->ic_curmode);
                imr->ifm_active |= IFM_IEEE80211_ADHOC;
                break;
        case IEEE80211_M_AHDEMO:
                /* should not come here */
                break;
        case IEEE80211_M_HOSTAP:
                imr->ifm_active |= IFM_IEEE80211_HOSTAP;
                break;
        case IEEE80211_M_MONITOR:
                imr->ifm_active |= IFM_IEEE80211_MONITOR;
                break;
        }
        switch (ic->ic_curmode) {
        case IEEE80211_MODE_11A:
                imr->ifm_active |= IFM_MAKEMODE(IFM_IEEE80211_11A);
                break;
        case IEEE80211_MODE_11B:
                imr->ifm_active |= IFM_MAKEMODE(IFM_IEEE80211_11B);
                break;
        case IEEE80211_MODE_11G:
                imr->ifm_active |= IFM_MAKEMODE(IFM_IEEE80211_11G);
                break;
/* 
 * TODO: is this correct? (IEEE80211_MODE_TURBO_A and IEEE80211_MODE_TURBO_G 
 * are defined in _ieee80211.h)
 */
	case IEEE80211_MODE_TURBO_A:
	case IEEE80211_MODE_TURBO_G:
                imr->ifm_active |= IFM_MAKEMODE(IFM_IEEE80211_11A)
                                |  IFM_IEEE80211_TURBO;
                break;
        }
}

static int
ndis_get_assoc(struct ndis_softc *sc, ndis_wlan_bssid_ex **assoc)
{
	ndis_80211_bssid_list_ex	*bl;
	ndis_wlan_bssid_ex	*bs;
	ndis_80211_macaddr	bssid;
	int			i, len, error;

	if (!sc->ndis_link)
		return(ENOENT);

	len = sizeof(bssid);
	error = ndis_get_info(sc, OID_802_11_BSSID, &bssid, &len);
	if (error) {
		aprint_error_dev(sc->ndis_dev, "failed to get bssid\n");
		return(ENOENT);
	}
	len = 0;
	error = ndis_get_info(sc, OID_802_11_BSSID_LIST, NULL, &len);
	if (error != ENOSPC) {
		aprint_error_dev(sc->ndis_dev, "bssid_list failed\n");
		return (error);
	}

	bl = malloc(len, M_TEMP, M_NOWAIT|M_ZERO);
	error = ndis_get_info(sc, OID_802_11_BSSID_LIST, bl, &len);
	if (error) {
		free(bl, M_TEMP);
		aprint_error_dev(sc->ndis_dev, "bssid_list failed\n");
		return (error);
	}

	bs = (ndis_wlan_bssid_ex *)&bl->nblx_bssid[0];
	for (i = 0; i < bl->nblx_items; i++) {
		if (memcmp(bs->nwbx_macaddr, bssid, sizeof(bssid)) == 0) {
			*assoc = malloc(bs->nwbx_len, M_TEMP, M_NOWAIT);
			if (*assoc == NULL) {
				free(bl, M_TEMP);
				return(ENOMEM);
			}
			memcpy((char *)*assoc, (char *)bs, bs->nwbx_len);
			free(bl, M_TEMP);
			return(0);
		}	
		bs = (ndis_wlan_bssid_ex *)((char *)bs + bs->nwbx_len);
	}

	free(bl, M_TEMP);
	return(ENOENT);
}

static void
ndis_getstate_80211(struct ndis_softc *sc)
{
	struct ieee80211com	*ic;
	ndis_80211_ssid		ssid;
	ndis_80211_config	config;
	ndis_wlan_bssid_ex	*bs = 0;
	int			rval, len, i = 0;
	uint32_t		arg;
	struct ifnet		*ifp;

	ic = &sc->ic;
/* TODO: are these equivelant? */	
	ifp = sc->ic.ic_ifp;

	if (!NDIS_INITIALIZED(sc))
		return;

	if (sc->ndis_link)
		ic->ic_state = IEEE80211_S_RUN;
	else
		ic->ic_state = IEEE80211_S_ASSOC;


	/*
	 * If we're associated, retrieve info on the current bssid.
	 */
	if ((rval = ndis_get_assoc(sc, &bs)) == 0) {
		switch(bs->nwbx_nettype) {
		case NDIS_80211_NETTYPE_11FH:
		case NDIS_80211_NETTYPE_11DS:
			ic->ic_curmode = IEEE80211_MODE_11B;
			break;
		case NDIS_80211_NETTYPE_11OFDM5:
			ic->ic_curmode = IEEE80211_MODE_11A;
			break;
		case NDIS_80211_NETTYPE_11OFDM24:
			ic->ic_curmode = IEEE80211_MODE_11G;
			break;
		default:
			aprint_error_dev(sc->ndis_dev, "unknown nettype %d\n", 
				     arg);
			break;
		}
		free(bs, M_TEMP);
	} else
		return;

	len = sizeof(ssid);
	memset((char *)&ssid, 0, len);
	rval = ndis_get_info(sc, OID_802_11_SSID, &ssid, &len);

	if (rval)
		aprint_error_dev(sc->ndis_dev, "get ssid failed: %d\n", rval);
	memcpy(ic->ic_bss->ni_essid, ssid.ns_ssid, ssid.ns_ssidlen);
	ic->ic_bss->ni_esslen = ssid.ns_ssidlen;

	len = sizeof(arg);
	rval = ndis_get_info(sc, OID_GEN_LINK_SPEED, &arg, &len);
	if (rval)
		aprint_error_dev(sc->ndis_dev, "get link speed failed: %d\n",
				rval);

	if (ic->ic_modecaps & (1<<IEEE80211_MODE_11B)) {
		ic->ic_bss->ni_rates = ic->ic_sup_rates[IEEE80211_MODE_11B];
		for (i = 0; i < ic->ic_bss->ni_rates.rs_nrates; i++) {
			if ((ic->ic_bss->ni_rates.rs_rates[i] &
			    IEEE80211_RATE_VAL) == arg / 5000)
				break;
		}
	}

	if (i == ic->ic_bss->ni_rates.rs_nrates &&
	    ic->ic_modecaps & (1<<IEEE80211_MODE_11G)) {
		ic->ic_bss->ni_rates = ic->ic_sup_rates[IEEE80211_MODE_11G];
		for (i = 0; i < ic->ic_bss->ni_rates.rs_nrates; i++) {
			if ((ic->ic_bss->ni_rates.rs_rates[i] &
			    IEEE80211_RATE_VAL) == arg / 5000)
				break;
		}
	}

	if (i == ic->ic_bss->ni_rates.rs_nrates)
		aprint_error_dev(sc->ndis_dev, "no matching rate for: %d\n",
			     arg / 5000);
	else
		ic->ic_bss->ni_txrate = i;

	if (ic->ic_caps & IEEE80211_C_PMGT) {
		len = sizeof(arg);
		rval = ndis_get_info(sc, OID_802_11_POWER_MODE, &arg, &len);

		if (rval)
			aprint_error_dev(sc->ndis_dev, "get power mode failed: %d\n", 
				     rval);
		if (arg == NDIS_80211_POWERMODE_CAM)
			ic->ic_flags &= ~IEEE80211_F_PMGTON;
		else
			ic->ic_flags |= IEEE80211_F_PMGTON;
	}

	len = sizeof(config);
	memset((char *)&config, 0, len);
	config.nc_length = len;
	config.nc_fhconfig.ncf_length = sizeof(ndis_80211_config_fh);
	rval = ndis_get_info(sc, OID_802_11_CONFIGURATION, &config, &len);   
	if (rval == 0) { 
		int chan;

		chan = ieee80211_mhz2ieee(config.nc_dsconfig / 1000, 0);
		if (chan < 0 || chan >= IEEE80211_CHAN_MAX) {
			if (ifp->if_flags & IFF_DEBUG)
				aprint_error_dev(sc->ndis_dev, "current channel "
					     "(%uMHz) out of bounds\n", 
					     config.nc_dsconfig / 1000);
			ic->ic_bss->ni_chan = &ic->ic_channels[1];
		} else
			ic->ic_bss->ni_chan = &ic->ic_channels[chan];
	} else
		aprint_error_dev(sc->ndis_dev, "couldn't retrieve "
			     "channel info: %d\n", rval);

/*
	len = sizeof(arg);
	rval = ndis_get_info(sc, OID_802_11_WEP_STATUS, &arg, &len);

	if (rval)
		device_printf (sc->ndis_dev,
		    "get wep status failed: %d\n", rval);

	if (arg == NDIS_80211_WEPSTAT_ENABLED)
		ic->ic_flags |= IEEE80211_F_WEPON;
	else
		ic->ic_flags &= ~IEEE80211_F_WEPON;
*/
	return;
}

static int
ndis_ioctl(struct ifnet *ifp, u_long command, void *data)
{
	struct ndis_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	int			i, error = 0;
	int			s;

	/*NDIS_LOCK(sc);*/
	s = splnet();

	switch(command) {
	case SIOCSIFFLAGS:
		if ((error = ifioctl_common(ifp, command, data)) != 0)
			break;
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->ndis_if_flags & IFF_PROMISC)) {
				sc->ndis_filter |=
				    NDIS_PACKET_TYPE_PROMISCUOUS;
				i = sizeof(sc->ndis_filter);
				error = ndis_set_info(sc,
				    OID_GEN_CURRENT_PACKET_FILTER,
				    &sc->ndis_filter, &i);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->ndis_if_flags & IFF_PROMISC) {
				sc->ndis_filter &=
				    ~NDIS_PACKET_TYPE_PROMISCUOUS;
				i = sizeof(sc->ndis_filter);
				error = ndis_set_info(sc,
				    OID_GEN_CURRENT_PACKET_FILTER,
				    &sc->ndis_filter, &i);
			} else
				ndis_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ndis_stop(sc);
		}
		sc->ndis_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
/* 
 * TODO: I'm really not sure this is the correct thing to do here, but multicast
 * address lists weren't getting set in ether_ioctl because they SIOCADDMULTI 
 * is routed to ndis_setmulti here.
 */
		error = ether_ioctl(ifp, command, data);
		ndis_setmulti(sc);
		error = 0;
		break;	
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		if (sc->ndis_80211) {
			error = ieee80211_ioctl(&sc->ic, command, data);
			if (error == ENETRESET) {
				ndis_setstate_80211(sc);
				/*ndis_init(sc);*/
				error = 0;
			}
		} else
			error = ifmedia_ioctl(ifp, ifr, &sc->ifmedia, command);
		break;
	case SIOCSIFCAP:
		if ((error = ether_ioctl(ifp, command, data)) == ENETRESET) {
			ndis_set_offload(sc);
			error = 0;
		}
		break;
	case SIOCGIFGENERIC:
	case SIOCSIFGENERIC:
		if (sc->ndis_80211 && NDIS_INITIALIZED(sc)) {
			if (command == SIOCGIFGENERIC)
				error = ndis_wi_ioctl_get(ifp, command, data);
			else
				error = ndis_wi_ioctl_set(ifp, command, data);
		} else
			error = ENOTTY;
		if (error != ENOTTY)
			break;
	default:
		sc->ndis_skip = 1;
		if (sc->ndis_80211) {
			error = ieee80211_ioctl(&sc->ic, command, data);

			if (error == ENETRESET) {
				ndis_setstate_80211(sc);
				error = 0;
			}
		} else {
			error = ether_ioctl(ifp, command, data);
		}
		sc->ndis_skip = 0;
		break;
	}

	/*NDIS_UNLOCK(sc);*/
			splx(s);

	return(error);
}

static int
ndis_wi_ioctl_get(struct ifnet *ifp, u_long command, void * data)
{
	struct wi_req		wreq;
	struct ifreq		*ifr;
	struct ndis_softc	*sc;
	ndis_80211_bssid_list_ex *bl;
	ndis_wlan_bssid_ex	*wb;
	struct wi_apinfo	*api;
	int			error, i, j, len, maxaps;

	sc = ifp->if_softc;
	ifr = (struct ifreq *)data;
	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return (error);

	switch (wreq.wi_type) {
	case WI_RID_READ_APS:
		len = 0;
		error = ndis_set_info(sc, OID_802_11_BSSID_LIST_SCAN,
		    NULL, &len);
		if (error == 0)
			tsleep(&error, PPAUSE|PCATCH, "ssidscan", hz * 2);
		len = 0;
		error = ndis_get_info(sc, OID_802_11_BSSID_LIST, NULL, &len);
		if (error != ENOSPC)
			break;
		bl = malloc(len, M_DEVBUF, M_WAITOK|M_ZERO);
		error = ndis_get_info(sc, OID_802_11_BSSID_LIST, bl, &len);
		if (error) {
			free(bl, M_DEVBUF);
			break;
		}
		maxaps = (2 * wreq.wi_len - sizeof(int)) / sizeof(*api);
		maxaps = MIN(maxaps, bl->nblx_items);
		wreq.wi_len = (maxaps * sizeof(*api) + sizeof(int)) / 2;
		*(int *)&wreq.wi_val = maxaps;
		api = (struct wi_apinfo *)&((int *)&wreq.wi_val)[1];
		wb = bl->nblx_bssid;
		while (maxaps--) {
			memset(api, 0, sizeof(*api));
			memcpy(&api->bssid, &wb->nwbx_macaddr,
			    sizeof(api->bssid));
			api->namelen = wb->nwbx_ssid.ns_ssidlen;
			memcpy(&api->name, &wb->nwbx_ssid.ns_ssid, api->namelen);
			if (wb->nwbx_privacy)
				api->capinfo |= IEEE80211_CAPINFO_PRIVACY;
			/* XXX Where can we get noise information? */
			api->signal = wb->nwbx_rssi + 149;	/* XXX */
			api->quality = api->signal;
			api->channel =
			    ieee80211_mhz2ieee(wb->nwbx_config.nc_dsconfig /
			    1000, 0);
			/* In "auto" infrastructure mode, this is useless. */
			if (wb->nwbx_netinfra == NDIS_80211_NET_INFRA_IBSS)
				api->capinfo |= IEEE80211_CAPINFO_IBSS;
			if (wb->nwbx_len > sizeof(ndis_wlan_bssid)) {
				j = sizeof(ndis_80211_rates_ex);
				/* handle other extended things */
			} else
				j = sizeof(ndis_80211_rates);
			for (i = api->rate = 0; i < j; i++)
				api->rate = MAX(api->rate, 5 *
				    (wb->nwbx_supportedrates[i] & 0x7f));
			api++;
			wb = (ndis_wlan_bssid_ex *)((char *)wb + wb->nwbx_len);
		}
		free(bl, M_DEVBUF);
		error = copyout(&wreq, ifr->ifr_data, sizeof(wreq));
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static int
ndis_wi_ioctl_set(struct ifnet *ifp, u_long command, void * data)
{
	struct wi_req		wreq;
	struct ifreq		*ifr;
	struct ndis_softc	*sc;
	uint32_t		foo;
	int			error, len;

	error = kauth_authorize_network(kauth_cred_get(),
	    KAUTH_NETWORK_INTERFACE, KAUTH_REQ_NETWORK_INTERFACE_SETPRIV,
	    ifp, KAUTH_ARG(command), NULL);
	if (error)
		return (error);

	sc = ifp->if_softc;
	ifr = (struct ifreq *)data;
	error = copyin(ifr->ifr_data, &wreq, sizeof(wreq));
	if (error)
		return (error);

	switch (wreq.wi_type) {
	case WI_RID_SCAN_APS:
	case WI_RID_SCAN_REQ:			/* arguments ignored */
		len = sizeof(foo);
		foo = 0;
		error = ndis_set_info(sc, OID_802_11_BSSID_LIST_SCAN, &foo,
		    &len);
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static void
ndis_watchdog(struct ifnet *ifp)
{
	struct ndis_softc		*sc;
	int				s;

	sc = ifp->if_softc;
	
	NDIS_LOCK(sc);
	
	ifp->if_oerrors++;
	aprint_error_dev(sc->ndis_dev, "watchdog timeout\n");
	
	NDIS_UNLOCK(sc);

	ndis_sched((void(*)(void *))ndis_reset_nic, sc, NDIS_TASKQUEUE);
	ndis_sched(ndis_starttask, ifp, NDIS_TASKQUEUE);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
ndis_stop(struct ndis_softc *sc)
{
	struct ifnet		*ifp;
	int 			s;

	ifp = &sc->arpcom.ac_if;
	callout_stop(&sc->ndis_stat_ch);

	ndis_halt_nic(sc);

	NDIS_LOCK(sc);
	
	ifp->if_timer = 0;
	sc->ndis_link = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	NDIS_UNLOCK(sc);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
/* TODO: remove this #ifdef once ndis_shutdown_nic() is working on NetBSD */
