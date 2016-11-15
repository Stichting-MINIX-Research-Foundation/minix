/*	$NetBSD: if_ndisvar.h,v 1.9 2012/10/27 17:18:23 chs Exp $	*/

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
 *
 * $FreeBSD: src/sys/dev/if_ndis/if_ndisvar.h,v 1.15.2.2 2005/02/18 16:30:10 wpaul Exp $
 */ 
 
#define NDIS_DEFAULT_NODENAME	"FreeBSD NDIS node"
#define NDIS_NODENAME_LEN	32

#ifdef __NetBSD__
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/cardbus/cardbusreg.h>
#endif

struct ndis_pci_type {
	const uint16_t		ndis_vid;
	const uint16_t		ndis_did;
	const uint32_t		ndis_subsys;
	const char			*ndis_name;
};

struct ndis_pccard_type {
	const char		*ndis_vid;
	const char		*ndis_did;
	char			*ndis_name;
};

struct ndis_shmem {
	bus_dma_tag_t		ndis_stag;
	bus_dmamap_t		ndis_smap;
	void			*ndis_saddr;
	struct ndis_shmem	*ndis_next;
};

struct ndis_cfglist {
	ndis_cfg		ndis_cfg;
        TAILQ_ENTRY(ndis_cfglist)	link;
};

TAILQ_HEAD(nch, ndis_cfglist);

#define NDIS_INITIALIZED(sc)	(sc->ndis_block->nmb_miniportadapterctx != NULL)

#define NDIS_INC(x)		\
	(x)->ndis_txidx = ((x)->ndis_txidx + 1) % (x)->ndis_maxpkts
	
#ifdef __NetBSD__
/* 
 * A linked list of resources 
 */
struct resource {
	SLIST_ENTRY(resource)    link;
	cm_partial_resource_desc win_res;
};
SLIST_HEAD(resource_list, resource);
#endif /* __NetBSD__ */

#ifdef __FreeBSD__
#define arpcom ic.ic_ac
#endif

#ifdef __NetBSD__
struct ndis_resource {
   bus_space_handle_t res_handle;
   bus_space_tag_t    res_tag;
   bus_addr_t         res_base;
   bus_size_t         res_size;
};
#endif

#ifdef __NetBSD__
extern int ndis_in_isr;
#endif

struct ndis_softc {
#ifdef __NetBSD__
	struct ethercom		arpcom;
#endif
	struct ieee80211com	ic;		/* interface info */
#ifdef notdef
	struct ieee80211com	arpcom;		/* interface info */
#endif
	struct ifmedia		ifmedia;	/* media info */
	u_long			ndis_hwassist;
	uint32_t		ndis_v4tx;
	uint32_t		ndis_v4rx;
	bus_space_handle_t	ndis_bhandle;
	bus_space_tag_t		ndis_btag;
	void			*ndis_intrhand;
#ifdef __FreeBSD__
	struct resource		*ndis_irq;
	struct resource		*ndis_res;
	struct resource		*ndis_res_io;
	int			ndis_io_rid;
	struct resource		*ndis_res_mem;
	int			ndis_mem_rid;
	struct resource		*ndis_res_altmem;
	int			ndis_altmem_rid;
	struct resource		*ndis_res_am;	/* attribute mem (pccard) */
	int			ndis_am_rid;
	struct resource		*ndis_res_cm;	/* common mem (pccard) */
	struct resource_list	ndis_rl;
#else /* __NetBSD__ */
	uint8_t	ndis_mac[ETHER_ADDR_LEN];
	int ndis_sysctl_mib;
	struct sysctllog *sysctllog;
	
	//ndis_resource_list 	ndis_rl;	
	ndis_resource_list 	*ndis_rl;
	int error;
/* TODO: Is the ndis_irq set up right? */	
	void *ndis_irq;
	
	/* for both pci and cardbus ? */
	struct ndis_resource 	*ndis_res_io;
	int 			 ndis_io_rid;	/* not actuially used, just for bus_release_resource() */
	struct ndis_resource 	*ndis_res_mem;
	struct ndis_resource	*ndis_res_altmem;
	int			 ndis_mem_rid;  /* not actuially used, just for bus_release_resource() */

	/* pci specific */
	pci_chipset_tag_t   ndis_res_pc;	/* pci chipset */
	pcitag_t          	ndis_res_pctag; /* pci tag */
	pci_intr_handle_t	pci_ih;		/* interrupt handle */
	
	/* pcmcia specific */
	struct pcmcia_io_handle ndis_res_pcioh;	  /* specific i/o for pcmcia */
	struct pcmcia_mem_handle ndis_res_pcmem;  /* specific mem for pcmcia */
	int sc_io_windows;			  /* i/o window */
	struct pcmcia_function * ndis_res_pcfunc; /* pcmcia function */
	
	/* cardbus specific */
	cardbus_devfunc_t    ndis_res_ct;	/* cardbus devfuncs */
	pcitag_t         ndis_res_ctag;	/* carbus tag */
	bus_size_t           ndis_res_mapsize;	/* size of mapped bus space region */
#endif /* end __NetBSD__ section */
	int			ndis_rescnt;
#ifdef __FreeBSD__	
	struct mtx		ndis_mtx;
#else /* __NetBSD__ */
	kmutex_t		ndis_mtx;
#endif	
        device_t		ndis_dev;
	int			ndis_unit;
	ndis_miniport_block	*ndis_block;
	ndis_miniport_characteristics	*ndis_chars;
	interface_type		ndis_type;
#ifdef __FreeBSD__
	struct callout_handle	ndis_stat_ch;
#else
	struct callout		ndis_stat_ch;
#endif
	int			ndis_maxpkts;
	ndis_oid		*ndis_oids;
	int			ndis_oidcnt;
	int			ndis_txidx;
	int			ndis_txpending;
	ndis_packet		**ndis_txarray;
	ndis_handle		ndis_txpool;
	int			ndis_sc;
	ndis_cfg		*ndis_regvals;
	struct nch		ndis_cfglist_head;
	int			ndis_80211;
	int			ndis_link;
	uint32_t		ndis_filter;
	int			ndis_if_flags;
	int			ndis_skip;

#ifdef __FreeBSD__
	struct sysctl_ctx_list	ndis_ctx;
#endif	
#if __FreeBSD__ && __FreeBSD_version < 502113
	struct sysctl_oid	*ndis_tree;
#endif
	int			ndis_devidx;
	interface_type		ndis_iftype;

	bus_dma_tag_t		ndis_parent_tag;
	struct ndis_shmem	*ndis_shlist;
	bus_dma_tag_t		ndis_mtag;
	bus_dma_tag_t		ndis_ttag;
	bus_dmamap_t		*ndis_mmaps;
	bus_dmamap_t		*ndis_tmaps;
	int			ndis_mmapcnt;
};

#define NDIS_LOCK(_sc)		mtx_lock(&(_sc)->ndis_mtx)
#define NDIS_UNLOCK(_sc)	mtx_unlock(&(_sc)->ndis_mtx)

/*static*/ __stdcall void ndis_txeof	    (ndis_handle, ndis_packet *, ndis_status);
/*static*/ __stdcall void ndis_rxeof	    (ndis_handle, ndis_packet **, uint32_t);
/*static*/ __stdcall void ndis_linksts	    (ndis_handle, ndis_status, void *, uint32_t);
/*static*/ __stdcall void ndis_linksts_done (ndis_handle);
