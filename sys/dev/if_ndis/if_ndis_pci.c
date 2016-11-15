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
__KERNEL_RCSID(0, "$NetBSD: if_ndis_pci.c,v 1.21 2015/04/04 15:22:02 christos Exp $");
#ifdef __FreeBSD__
__FBSDID("$FreeBSD: src/sys/dev/if_ndis/if_ndis_pci.c,v 1.8.2.3 2005/03/31 04:24:36 wpaul Exp $");
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>

#include <sys/bus.h>

#include <sys/kthread.h>
#include <net/if_ether.h>

#include <net80211/ieee80211_var.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <compat/ndis/pe_var.h>
#include <compat/ndis/resource_var.h>
#include <compat/ndis/ntoskrnl_var.h>
#include <compat/ndis/ndis_var.h>
#include <compat/ndis/cfg_var.h>
#include <dev/if_ndis/if_ndisvar.h>

#include "ndis_driver_data.h"

#ifndef _MODULE
#include <compat/ndis/hal_var.h>
#endif

#ifdef NDIS_PCI_DEV_TABLE 


/*
 * Various supported device vendors/types and their names.
 * These are defined in the ndis_driver_data.h file.
 */
static struct ndis_pci_type ndis_devs[] = {
#ifdef NDIS_PCI_DEV_TABLE
	NDIS_PCI_DEV_TABLE
#endif
	{ 0, 0, 0, NULL }
};

/*static*/ int  ndis_probe_pci(device_t parent, 
				cfdata_t match,
				void *aux);
/*static*/ void ndis_attach_pci(device_t parent,
				device_t self,
				void *aux);
extern void ndis_attach		(void *);
extern int ndis_shutdown	(device_t);
extern int ndis_detach		(device_t, int);
extern int ndis_suspend		(device_t);
extern int ndis_resume		(device_t);

extern int ndis_intr(void *);

extern unsigned char drv_data[];

#ifndef _MODULE
//static funcptr ndis_txeof_wrap;
//static funcptr ndis_rxeof_wrap;
//static funcptr ndis_linksts_wrap;
//static funcptr ndis_linksts_done_wrap;
#endif


CFATTACH_DECL_NEW(
#ifdef NDIS_DEVNAME
	NDIS_DEVNAME,
#else
	ndis,
#endif
	sizeof(struct ndis_softc),
	ndis_probe_pci,
	ndis_attach_pci,
	ndis_detach,
	NULL);



#ifdef _MODULE
extern int 
ndisdrv_modevent(module_t mod, int cmd);

/* 
 * These are just for the in-kernel version, to delay calling
 * these functions untill enough context is built up.
 */
void load_ndisapi(void *);
void load_ndisdrv(void *);

void load_ndisapi(void *arg)
{
	ndis_lkm_handle(NULL, MOD_LOAD);
}
void load_ndisdrv(void *arg)
{
	ndisdrv_modevent(NULL, MOD_LOAD);
}
#endif

/*static*/ int
ndis_probe_pci(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;
	int vendor  = PCI_VENDOR(pa->pa_id);
	int product = PCI_PRODUCT(pa->pa_id);
	
	struct ndis_pci_type *t = ndis_devs;
	driver_object        *drv = NULL;	/* = windrv_lookup(0, "PCI Bus");**/
		
#ifdef NDIS_DBG
	printf("in ndis_probe_pci\n");
	printf("vendor = %x, product = %x\n", vendor, product);
#endif
	
	while(t->ndis_name != NULL) {
#ifdef NDIS_DBG
			printf("t->ndis_vid = %x, t->ndis_did = %x\n",
			       t->ndis_vid, t->ndis_did);
#endif
		if((vendor  == t->ndis_vid) && (product == t->ndis_did)) {
#ifdef _MODULE	
			ndisdrv_modevent(NULL, MOD_LOAD);
			//kthread_create(load_ndisdrv, NULL);
#endif /* _MODULE */
				
			drv = windrv_lookup(0, "PCI Bus");
			printf("Matching vendor: %x, product: %x, name: %s\n", vendor, product, t->ndis_name);
			windrv_create_pdo(drv, parent);
			return 1;
		}
		t++;
	}
	
	return 0;  /* dosen't match */
}

/* 6 BADR's + 1 IRQ  (so far) */
#define MAX_RESOURCES 7

/*static*/ 
void ndis_attach_pci(device_t parent, device_t self, void *aux)
{
	struct ndis_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
#ifdef NDIS_DBG       
	char devinfo[256];
#endif
	pci_intr_handle_t ih;
	pcireg_t type;
	bus_addr_t	base;
	bus_size_t	size;
	int		flags;
	ndis_resource_list 		*rl  = NULL;
	struct cm_partial_resource_desc	*prd = NULL;
#ifdef NDIS_DBG
	struct pci_conf_state conf_state;
	int revision, i;
#endif
	int bar;
	size_t rllen;
	
	printf("in ndis_attach_pci()\n");

	/* initalize the softc */
	//sc->ndis_hardware_type  = NDIS_PCI;
	sc->ndis_dev		= self;
	sc->ndis_iftype 	= PCIBus;
	sc->ndis_res_pc		= pa->pa_pc;
	sc->ndis_res_pctag	= pa->pa_tag;
	/* TODO: is this correct? All are just pa->pa_dmat? */	
	sc->ndis_mtag		= pa->pa_dmat;
	sc->ndis_ttag		= pa->pa_dmat;
	sc->ndis_parent_tag 	= pa->pa_dmat;
	sc->ndis_res_io		= NULL;
	sc->ndis_res_mem	= NULL;
	sc->ndis_res_altmem	= NULL;
	sc->ndis_block 		= NULL;
	sc->ndis_shlist		= NULL;
	
	ndis_in_isr		= FALSE;
	
	printf("sc->ndis_mtag = %x\n", (unsigned int)sc->ndis_mtag);

	rllen = sizeof(ndis_resource_list) +
	    sizeof(cm_partial_resource_desc) * (MAX_RESOURCES - 1);
	rl = malloc(rllen, M_DEVBUF, M_NOWAIT|M_ZERO);

	if(rl == NULL) {
		sc->error = ENOMEM;
		//printf("error: out of memory\n");
		return;
	}
	
	rl->cprl_version = 5;
	rl->cprl_version = 1;    
	rl->cprl_count = 0;
	prd = rl->cprl_partial_descs;
	
#ifdef NDIS_DBG
        pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo, sizeof devinfo);
        revision = PCI_REVISION(pa->pa_class);
        printf(": %s (rev. 0x%02x)\n", devinfo, revision);
	
	pci_conf_print(sc->ndis_res_pc, sc->ndis_res_pctag, NULL);

	pci_conf_capture(sc->ndis_res_pc, sc->ndis_res_pctag, &conf_state);
	for(i=0; i<16; i++) {
		printf("conf_state.reg[%d] = %x\n", i, conf_state.reg[i]);
	}
#endif
	
	/* just do the conversion work in attach instead of calling ndis_convert_res() */
	for(bar = 0x10; bar <= 0x24; bar += 0x04) {
		type = pci_mapreg_type(sc->ndis_res_pc, sc->ndis_res_pctag, bar);
		if(pci_mapreg_info(sc->ndis_res_pc, sc->ndis_res_pctag, bar, type, &base,
			&size, &flags)) {
			printf("pci_mapreg_info() failed on BAR 0x%x!\n", bar);
		} else {			
			switch(type) {
			case PCI_MAPREG_TYPE_IO:
				prd->cprd_type 				= CmResourceTypePort;
				prd->cprd_flags 			= CM_RESOURCE_PORT_IO;
				prd->u.cprd_port.cprd_start.np_quad 	= (uint64_t)base;
				prd->u.cprd_port.cprd_len  	  	= (uint32_t)size;
				if((sc->ndis_res_io = 
					malloc(sizeof(struct ndis_resource), M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
					//printf("error: out of memory\n");
					sc->error = ENOMEM;
					goto out;
				}
				sc->ndis_res_io->res_base = base;
				sc->ndis_res_io->res_size = size;
				sc->ndis_res_io->res_tag  = x86_bus_space_io;
				bus_space_map(sc->ndis_res_io->res_tag,
					 sc->ndis_res_io->res_base,
					 sc->ndis_res_io->res_size,
					 flags,
					&sc->ndis_res_io->res_handle);
				break;
			case PCI_MAPREG_TYPE_MEM:
				prd->cprd_type 				= CmResourceTypeMemory;
				prd->cprd_flags 			= CM_RESOURCE_MEMORY_READ_WRITE;
				prd->u.cprd_mem.cprd_start.np_quad 	= (uint64_t)base;
				prd->u.cprd_mem.cprd_len		= (uint32_t)size;
				
				if(sc->ndis_res_mem != NULL && 
					sc->ndis_res_altmem != NULL) {
					printf("too many resources\n");
					sc->error = ENXIO;
					goto out;
				}
				if(sc->ndis_res_mem) {
					if((sc->ndis_res_altmem = 
						malloc(sizeof(struct ndis_resource), M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
						sc->error = ENOMEM;
						return;
					}
					sc->ndis_res_altmem->res_base = base;
					sc->ndis_res_altmem->res_size = size;
					sc->ndis_res_altmem->res_tag  = x86_bus_space_mem;
					
					
					if(bus_space_map(sc->ndis_res_altmem->res_tag,
						sc->ndis_res_altmem->res_base,
						sc->ndis_res_altmem->res_size,
						flags|BUS_SPACE_MAP_LINEAR,
						&sc->ndis_res_altmem->res_handle)) {
							printf("bus_space_map failed\n");
					}
				} else {
					if((sc->ndis_res_mem = 
						malloc(sizeof(struct ndis_resource), M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL) {
						sc->error = ENOMEM;
						goto out;
					}
					sc->ndis_res_mem->res_base = base;
					sc->ndis_res_mem->res_size = size;
					sc->ndis_res_mem->res_tag  = x86_bus_space_mem;
					
					if(bus_space_map(sc->ndis_res_mem->res_tag,
						sc->ndis_res_mem->res_base,
						sc->ndis_res_mem->res_size,
						flags|BUS_SPACE_MAP_LINEAR,
						&sc->ndis_res_mem->res_handle)) {
							printf("bus_space_map failed\n");
					}
				}
				break;
											   
			default:
				printf("unknown type\n");
			}
			prd->cprd_sharedisp = CmResourceShareDeviceExclusive;

			rl->cprl_count++;								
			prd++;
		}
	}
	
	/* add the interrupt to the list */
	prd->cprd_type 	= CmResourceTypeInterrupt;
	prd->cprd_flags = 0;
	/* TODO: is this all we need to save for the interrupt? */
	prd->u.cprd_intr.cprd_level = pa->pa_intrline;
	prd->u.cprd_intr.cprd_vector = pa->pa_intrline;
	prd->u.cprd_intr.cprd_affinity = 0;
	rl->cprl_count++;
	
	pci_intr_map(pa, &ih);
	sc->ndis_intrhand = pci_intr_establish(pa->pa_pc, ih, IPL_NET /*| PCATCH*/, ndis_intr, sc);
	sc->ndis_irq = (void *)sc->ndis_intrhand;
	
	printf("pci interrupt: %s\n", pci_intr_string(pa->pa_pc, ih));
	
	/* save resource list in the softc */
	sc->ndis_rl = rl;
	sc->ndis_rescnt = rl->cprl_count;
	
	kthread_create(PRI_NONE, 0, NULL, ndis_attach, (void *)sc,
	    NULL, "ndis_attach");
	return;
out:
	free(rl, M_DEVBUF); 
	return;
}


#endif /* NDIS_PCI_DEV_TABLE */
