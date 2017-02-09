/*	$NetBSD: rbus_ppb.c,v 1.44 2015/04/13 16:33:24 riastradh Exp $	*/

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael Richardson <mcr@sandelman.ottawa.on.ca>
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

/*
 * CardBus front-end for the Intel/Digital DECchip 21152 PCI-PCI bridge
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rbus_ppb.c,v 1.44 2015/04/13 16:33:24 riastradh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/kmem.h>

#include <machine/endian.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/ppbreg.h>

#include <dev/ic/i82365reg.h>

#include <dev/cardbus/rbus.h>
#include <dev/pci/pccbbreg.h>
#include <dev/pci/pccbbvar.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

#include <x86/pci/pci_addr_fixup.h>
#include <x86/pci/pci_bus_fixup.h>
#include <i386/pci/pci_intr_fixup.h>
#include <i386/pci/pcibios.h>

struct ppb_softc;

static int  ppb_cardbus_match(device_t, cfdata_t, void *);
static void ppb_cardbus_attach(device_t, device_t, void *);
static int  ppb_activate(device_t, enum devact);
int rppbprint(void *, const char *);
int rbus_intr_fixup(pci_chipset_tag_t, int, int, int);
void rbus_do_header_fixup(pci_chipset_tag_t, pcitag_t, void *);

static void rbus_pci_phys_allocate(pci_chipset_tag_t, pcitag_t, void *);

static int rbus_do_phys_allocate(pci_chipset_tag_t, pcitag_t, int,
				 void *, int, bus_addr_t *, bus_size_t);

static void rbus_pci_phys_countspace(pci_chipset_tag_t, pcitag_t, void *);

static int rbus_do_phys_countspace(pci_chipset_tag_t, pcitag_t, int,
				   void *, int, bus_addr_t *, bus_size_t);

unsigned int rbus_round_up(unsigned int, unsigned int);


struct ppb_cardbus_softc {
  device_t sc_dev;
  pcitag_t sc_tag;
  int foo;
};

CFATTACH_DECL_NEW(rbus_ppb, sizeof(struct ppb_cardbus_softc),
    ppb_cardbus_match, ppb_cardbus_attach, NULL, ppb_activate);

#ifdef  CBB_DEBUG
int rbus_ppb_debug = 0;   /* hack with kdb */
#define DPRINTF(X) if(rbus_ppb_debug) printf X
#else
#define DPRINTF(X)
#endif

static int
ppb_cardbus_match(device_t parent, cfdata_t match, void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (PCI_VENDOR(ca->ca_id) ==  PCI_VENDOR_DEC &&
	    PCI_PRODUCT(ca->ca_id) == PCI_PRODUCT_DEC_21152)
		return (1);

	if(PCI_CLASS(ca->ca_class) == PCI_CLASS_BRIDGE &&
	   PCI_SUBCLASS(ca->ca_class) == PCI_SUBCLASS_BRIDGE_PCI) {
	  /* XXX */
	  printf("recognizing generic bridge chip\n");
	}

	return (0);
}


int
rppbprint(void *aux, const char *pnp)
{
	struct pcibus_attach_args *pba = aux;

	/* only PCIs can attach to PPBs; easy. */
	if (pnp)
		aprint_normal("pci at %s", pnp);
	aprint_normal(" bus %d (rbus)", pba->pba_bus);
	return (UNCONF);
}

int
rbus_intr_fixup(pci_chipset_tag_t pc,
		int minbus,
		int maxbus,
		int line)
{
  pci_device_foreach_min(pc, minbus,
			 maxbus, rbus_do_header_fixup, (void *)&line);
  return 0;
}

void
rbus_do_header_fixup(pci_chipset_tag_t pc, pcitag_t tag, void *context)
{
  int bus, device, function;
  pcireg_t intr;
  int *pline = (int *)context;
  int line = *pline;

  pci_decompose_tag(pc, tag, &bus, &device, &function);

  intr = pci_conf_read(pc, tag, PCI_INTERRUPT_REG);

  intr &= ~(PCI_INTERRUPT_LINE_MASK << PCI_INTERRUPT_LINE_SHIFT);
  intr |= (line << PCI_INTERRUPT_LINE_SHIFT);
  pci_conf_write(pc, tag, PCI_INTERRUPT_REG, intr);

}

/*
 * This function takes a range of PCI bus numbers and
 * allocates space for all devices found in this space (the BARs) from
 * the rbus space maps (I/O and memory).
 *
 * It assumes that "rbus" is defined. The whole concept does.
 *
 * It uses pci_device_foreach_min() to call rbus_pci_phys_allocate.
 * This function is mostly stolen from
 *     pci_addr_fixup.c:pciaddr_resource_reserve.
 *
 */
struct rbus_pci_addr_fixup_context {
  struct ppb_cardbus_softc *csc;
  cardbus_chipset_tag_t ct;
  struct cardbus_softc *sc;
  struct cardbus_attach_args *caa;
  int    minbus;
  int    maxbus;
  bus_size_t  *bussize_ioreqs;
  bus_size_t  *bussize_memreqs;
  rbus_tag_t   *iobustags;
  rbus_tag_t   *membustags;
};

unsigned int
rbus_round_up(unsigned int size, unsigned int minval)
{
  unsigned int power2;

  if(size == 0) {
    return 0;
  }

  power2=minval;

  while(power2 < (1 << 31) &&
	power2 < size) {
    power2 = power2 << 1;
  }

  return power2;
}

static void
rbus_pci_addr_fixup(struct ppb_cardbus_softc *csc,
		    cardbus_chipset_tag_t ct,
		    struct cardbus_softc *sc,
		    pci_chipset_tag_t     pc,
		    struct cardbus_attach_args *caa,
		    int minbus, const int maxbus)
{
	struct rbus_pci_addr_fixup_context rct;
	const size_t size = sizeof(bus_size_t[maxbus+1]);
	int busnum;
	bus_addr_t start;
	bus_space_handle_t handle;
	u_int32_t reg;

	rct.csc=csc;
	rct.ct=ct;
	rct.sc=sc;
	rct.caa=caa;
	rct.minbus = minbus;
	rct.maxbus = maxbus;
	if ((rct.bussize_ioreqs  = kmem_zalloc(size, KM_SLEEP)) == NULL ||
	    (rct.bussize_memreqs = kmem_zalloc(size, KM_SLEEP)) == NULL ||
	    (rct.iobustags =
	     kmem_zalloc(maxbus * sizeof(rbus_tag_t), KM_SLEEP)) == NULL ||
	    (rct.membustags =
	     kmem_zalloc(maxbus * sizeof(rbus_tag_t), KM_SLEEP)) == NULL)
		panic("%s: memory allocation failed", __func__);

	printf("%s: sizing buses %d-%d\n",
	       device_xname(rct.csc->sc_dev),
	       minbus, maxbus);

	pci_device_foreach_min(pc, minbus, maxbus,
			       rbus_pci_phys_countspace, &rct);

	/*
	 * we need to determine amount of address space for each
	 * bus. To do this, we have to roll up amounts and then
	 * we need to divide up the cardbus's extent to allocate
	 * some space to each bus.
	 */

	for(busnum=maxbus; busnum > minbus; busnum--) {
	  if(pci_bus_parent[busnum] != 0) {
	    if(pci_bus_parent[busnum] < minbus ||
	       pci_bus_parent[busnum] >= maxbus) {
	      printf("%s: bus %d has illegal parent %d\n",
		     device_xname(rct.csc->sc_dev),
		     busnum, pci_bus_parent[busnum]);
	      continue;
	    }

	    /* first round amount of space up */
	    rct.bussize_ioreqs[busnum] =
	      rbus_round_up(rct.bussize_ioreqs[busnum],  PPB_IO_MIN);
	    rct.bussize_ioreqs[pci_bus_parent[busnum]] +=
	      rct.bussize_ioreqs[busnum];

	    rct.bussize_memreqs[busnum] =
	      rbus_round_up(rct.bussize_memreqs[busnum], PPB_MEM_MIN);
	    rct.bussize_memreqs[pci_bus_parent[busnum]] +=
	      rct.bussize_memreqs[busnum];

	  }
	}

	rct.bussize_ioreqs[minbus] =
	  rbus_round_up(rct.bussize_ioreqs[minbus], 4096);
	rct.bussize_memreqs[minbus] =
	  rbus_round_up(rct.bussize_memreqs[minbus], 8);

	printf("%s: total needs IO %08zx and MEM %08zx\n",
	       device_xname(rct.csc->sc_dev),
	       rct.bussize_ioreqs[minbus], rct.bussize_memreqs[minbus]);

	if(!caa->ca_rbus_iot) {
	  panic("no iot bus");
	}

	if(rct.bussize_ioreqs[minbus]) {
	  if(rbus_space_alloc(caa->ca_rbus_iot, 0,
			      rct.bussize_ioreqs[minbus],
			      rct.bussize_ioreqs[minbus]-1 /* mask  */,
			      rct.bussize_ioreqs[minbus] /* align */,
			      /* flags */ 0,
			      &start,
			      &handle) != 0) {
	    panic("rbus_ppb: can not allocate %zu bytes in IO bus %d",
		  rct.bussize_ioreqs[minbus], minbus);
	  }
	  rct.iobustags[minbus]=rbus_new(caa->ca_rbus_iot,
					 start,
					 rct.bussize_ioreqs[minbus],
					 0 /* offset to add to physical address
					      to make processor address */,
					 RBUS_SPACE_DEDICATE);
	}

	if(rct.bussize_memreqs[minbus]) {
	  if(rbus_space_alloc(caa->ca_rbus_memt, 0,
			      rct.bussize_memreqs[minbus],
			      rct.bussize_memreqs[minbus]-1 /* mask */,
			      rct.bussize_memreqs[minbus] /* align */,
			      /* flags */ 0,
			      &start,
			      &handle) != 0) {
	    panic("%s: can not allocate %zu bytes in MEM bus %d",
		  device_xname(rct.csc->sc_dev),
		  rct.bussize_memreqs[minbus], minbus);
	  }
	  rct.membustags[minbus]=rbus_new(caa->ca_rbus_memt,
					  start,
					  rct.bussize_memreqs[minbus],
					  0 /* offset to add to physical
					       address to make processor
					       address */,
					  RBUS_SPACE_DEDICATE);
	}

	for(busnum=minbus+1; busnum <= maxbus; busnum++) {
	  int busparent;

	  busparent = pci_bus_parent[busnum];

	  printf("%s: bus %d (parent=%d) needs IO %08zx and MEM %08zx\n",
		 device_xname(rct.csc->sc_dev),
		 busnum,
		 busparent,
		 rct.bussize_ioreqs[busnum],
		 rct.bussize_memreqs[busnum]);

	  if(busparent > maxbus) {
	    panic("rbus_ppb: illegal parent");
	  }

	  if(rct.bussize_ioreqs[busnum]) {
	    if(rbus_space_alloc(rct.iobustags[busparent],
				0,
				rct.bussize_ioreqs[busnum],
				rct.bussize_ioreqs[busnum]-1 /*mask */,
				rct.bussize_ioreqs[busnum] /* align */,
				/* flags */ 0,
				&start,
				&handle) != 0) {
	      panic("rbus_ppb: can not allocate %zu bytes in IO bus %d",
		    rct.bussize_ioreqs[busnum], busnum);
	    }
	    rct.iobustags[busnum]=rbus_new(rct.iobustags[busparent],
					   start,
					   rct.bussize_ioreqs[busnum],
					   0 /* offset to add to physical
						address
						to make processor address */,
					   RBUS_SPACE_DEDICATE);

	    /* program the bridge */

	    /* enable I/O space */
	    reg = pci_conf_read(pc, pci_bus_tag[busnum],
				PCI_COMMAND_STATUS_REG);
	    reg |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	    pci_conf_write(pc, pci_bus_tag[busnum],
			   PCI_COMMAND_STATUS_REG, reg);

	    /* now init the limit register for I/O */
	    pci_conf_write(pc, pci_bus_tag[busnum], PPB_REG_IOSTATUS,
			   (((start & 0xf000) >> 8) << PPB_IOBASE_SHIFT) |
			   ((((start +
			       rct.bussize_ioreqs[busnum] +
			       4095) & 0xf000) >> 8) << PPB_IOLIMIT_SHIFT));
	  }

	  if(rct.bussize_memreqs[busnum]) {
	    if(rbus_space_alloc(rct.membustags[busparent],
				0,
				rct.bussize_memreqs[busnum] /* size  */,
				rct.bussize_memreqs[busnum]-1 /*mask */,
				rct.bussize_memreqs[busnum] /* align */,
				/* flags */ 0,
				&start,
				&handle) != 0) {
	      panic("rbus_ppb: can not allocate %zu bytes in MEM bus %d",
		    rct.bussize_memreqs[busnum], busnum);
	    }
	    rct.membustags[busnum]=rbus_new(rct.membustags[busparent],
					    start,
					    rct.bussize_memreqs[busnum],
					    0 /* offset to add to physical
						 address to make processor
						 address */,
					    RBUS_SPACE_DEDICATE);

	    /* program the bridge */
	    /* enable memory space */
	    reg = pci_conf_read(pc, pci_bus_tag[busnum],
				PCI_COMMAND_STATUS_REG);
	    reg |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	    pci_conf_write(pc, pci_bus_tag[busnum],
			   PCI_COMMAND_STATUS_REG, reg);

	    /* now init the limit register for memory */
	    pci_conf_write(pc, pci_bus_tag[busnum], PPB_REG_MEM,
			   ((start & PPB_MEM_MASK)
			    >> PPB_MEM_SHIFT) << PPB_MEMBASE_SHIFT |
			   (((start +
			     rct.bussize_memreqs[busnum] +
			      PPB_MEM_MIN-1) >> PPB_MEM_SHIFT)
			    << PPB_MEMLIMIT_SHIFT));

	    /* and set the prefetchable limits as well */
	    pci_conf_write(pc, pci_bus_tag[busnum], PPB_REG_PREFMEM,
			   ((start & PPB_MEM_MASK)
			    >> PPB_MEM_SHIFT) << PPB_MEMBASE_SHIFT |
			   (((start +
			     rct.bussize_memreqs[busnum] +
			      PPB_MEM_MIN-1) >> PPB_MEM_SHIFT)
			    << PPB_MEMLIMIT_SHIFT));

	    /* pci_conf_print(pc, pci_bus_tag[busnum], NULL); */
	  }
	}

	printf("%s: configuring buses %d-%d\n",
		device_xname(rct.csc->sc_dev),
	       minbus, maxbus);
	pci_device_foreach_min(pc, minbus, maxbus,
			       rbus_pci_phys_allocate, &rct);

	kmem_free(rct.bussize_ioreqs, size);
	kmem_free(rct.bussize_memreqs, size);
	kmem_free(rct.iobustags, maxbus * sizeof(rbus_tag_t));
	kmem_free(rct.membustags, maxbus * sizeof(rbus_tag_t));
}

static void
rbus_pci_phys_countspace(pci_chipset_tag_t pc, pcitag_t tag, void *context)
{
        int bus, device, function;
	struct  rbus_pci_addr_fixup_context *rct =
	  (struct  rbus_pci_addr_fixup_context *)context;

	pci_decompose_tag(pc, tag, &bus, &device, &function);

	printf("%s: configuring device %02x:%02x:%02x\n",
	       device_xname(rct->csc->sc_dev),
	       bus, device, function);

	pciaddr_resource_manage(pc, tag,
				rbus_do_phys_countspace, context);
}


int
rbus_do_phys_countspace(pci_chipset_tag_t pc, pcitag_t tag, int mapreg, void *ctx, int type, bus_addr_t *addr, bus_size_t size)
{
	struct  rbus_pci_addr_fixup_context *rct =
	  (struct  rbus_pci_addr_fixup_context *)ctx;
	int bus, device, function;

	pci_decompose_tag(pc, tag, &bus, &device, &function);

	if(size > (1<<24)) {
	  printf("%s: skipping huge space request of size=%08x\n",
		 device_xname(rct->csc->sc_dev), (unsigned int)size);
	  return 0;
	}

	if(PCI_MAPREG_TYPE(type) == PCI_MAPREG_TYPE_IO) {
	  rct->bussize_ioreqs[bus] += size;
	} else {
	  rct->bussize_memreqs[bus]+= size;
	}

	return 0;
}

static void
rbus_pci_phys_allocate(pci_chipset_tag_t pc, pcitag_t tag, void *context)
{
        int bus, device, function, command;
	struct rbus_pci_addr_fixup_context *rct =
	  (struct rbus_pci_addr_fixup_context *)context;

	pci_decompose_tag(pc, tag, &bus, &device, &function);

	printf("%s: configuring device %02x:%02x:%02x\n",
	       device_xname(rct->csc->sc_dev),
	       bus, device, function);

	pciaddr_resource_manage(pc, tag,
				rbus_do_phys_allocate, context);

	/* now turn the device's memory and I/O on */
	command = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_IO_ENABLE|PCI_COMMAND_MEM_ENABLE;
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, command);
}

int
rbus_do_phys_allocate(pci_chipset_tag_t pc, pcitag_t tag, int mapreg, void *ctx, int type, bus_addr_t *addr, bus_size_t size)
{
	struct  rbus_pci_addr_fixup_context *rct =
	  (struct  rbus_pci_addr_fixup_context *)ctx;
	cardbus_chipset_tag_t ct     = rct->ct;
	struct cardbus_softc *sc     = rct->sc;
	cardbus_function_t       *cf = sc->sc_cf;
	rbus_tag_t          rbustag;
	bus_addr_t mask = size -1;
	bus_addr_t base = 0;
	bus_space_handle_t handle;
	int busflags = 0;
	int flags    = 0;
	const char *bustype;
	int bus, device, function;

	pci_decompose_tag(pc, tag, &bus, &device, &function);

	/*
	 * some devices come up with garbage in them (Tulip?)
	 * we are in charge here, so give them address
	 * space anyway.
	 *
	 * XXX this may be due to no secondary PCI reset!!!
	 */
#if 0
	if (*addr) {
		printf("Already allocated space at %08x\n",
		       (unsigned int)*addr);
		return (0);
	}
#endif

	if(size > (1<<24)) {
	  printf("%s: skipping huge space request of size=%08x\n",
		 device_xname(rct->csc->sc_dev), (unsigned int)size);
	  return 0;
	}

	if(PCI_MAPREG_TYPE(type) == PCI_MAPREG_TYPE_IO) {
	  rbustag = rct->iobustags[bus];
	  bustype = "io";
	} else {
	  rbustag = rct->membustags[bus];
	  bustype = "mem";
	}

	if((*cf->cardbus_space_alloc)(ct, rbustag, base, size,
				      mask, size, busflags|flags,
				      addr, &handle)) {
	  printf("%s: no available resources (size=%08x) for bar %2d. fixup failed\n",
		 device_xname(rct->csc->sc_dev), (unsigned int)size, mapreg);

	  *addr = 0;
	  pci_conf_write(pc, tag, mapreg, *addr);
	  return (1);
	}

	printf("%s: alloc %s space of size %08x for %02d:%02d:%02d -> %08x\n",
	       device_xname(rct->csc->sc_dev),
	       bustype,
	       (unsigned int)size,
	       bus, device, function, (unsigned int)*addr);

	/* write new address to PCI device configuration header */
	pci_conf_write(pc, tag, mapreg, *addr);

	/* check */
	{
		DPRINTF(("%s: pci_addr_fixup: ",
			 device_xname(rct->csc->sc_dev)));
#ifdef  CBB_DEBUG
		if(rbus_ppb_debug) { pciaddr_print_devid(pc, tag); }
#endif
	}

	/* double check that the value got inserted correctly */
	if (pciaddr_ioaddr(pci_conf_read(pc, tag, mapreg)) != *addr) {
		pci_conf_write(pc, tag, mapreg, 0); /* clear */
		printf("%s: fixup failed. (new address=%#x)\n",
		       device_xname(rct->csc->sc_dev),
		       (unsigned)*addr);
		return (1);
	}

	DPRINTF(("new address 0x%08x\n",
		 (unsigned)*addr));

	return (0);
}

static void
ppb_cardbus_attach(device_t parent, device_t self, void *aux)
{
	struct ppb_cardbus_softc *csc = device_private(self);
	struct cardbus_softc *parent_sc = device_private(parent);
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	cardbus_chipset_tag_t cc = ct->ct_cc;
	struct pccbb_softc *psc = (struct pccbb_softc *)cc;
	struct pcibus_attach_args pba;
	char devinfo[256];
	pcireg_t busdata;
	int minbus, maxbus;

	csc->sc_dev = self;

	pci_devinfo(ca->ca_id, ca->ca_class, 0, devinfo, sizeof(devinfo));
	printf(": %s (rev. 0x%02x)\n", devinfo, PCI_REVISION(ca->ca_class));

	csc->sc_tag = ca->ca_tag;

	busdata = Cardbus_conf_read(ct, ca->ca_tag, PPB_REG_BUSINFO);
	minbus = pcibios_max_bus;
	maxbus = minbus;		/* XXX; gcc */

	if (PPB_BUSINFO_SECONDARY(busdata) == 0) {
		aprint_error_dev(self, "not configured by system firmware calling pci_bus_fixup(%d)\n", 0);

	  /*
	   * first, pull the reset wire on the secondary bridge
	   * to clear all devices
	   */
	  busdata = Cardbus_conf_read(ct, ca->ca_tag,
				      PPB_REG_BRIDGECONTROL);
	  Cardbus_conf_write(ct, ca->ca_tag, PPB_REG_BRIDGECONTROL,
			     busdata | PPB_BC_SECONDARY_RESET);
	  delay(1);
	  Cardbus_conf_write(ct, ca->ca_tag, PPB_REG_BRIDGECONTROL,
			     busdata);

	  /* then go initialize the bridge control registers */
	  maxbus = pci_bus_fixup(psc->sc_pc, 0);
	}

	busdata = Cardbus_conf_read(ct, ca->ca_tag, PPB_REG_BUSINFO);
	if(PPB_BUSINFO_SECONDARY(busdata) == 0) {
		aprint_error_dev(self, "still not configured, not fixable.\n");
		return;
	}

#if 0
	minbus = PPB_BUSINFO_SECONDARY(busdata);
	maxbus = PPB_BUSINFO_SUBORDINATE(busdata);
#endif

	/* now, go and assign addresses for the new devices */
	rbus_pci_addr_fixup(csc, cc, parent_sc,
			    psc->sc_pc,
			    ca,
			    minbus, maxbus);

	/*
	 * now configure all connected devices to the IRQ which
	 * was assigned to this slot, as they will all arrive from
	 * that IRQ.
	 */
	rbus_intr_fixup(psc->sc_pc, minbus, maxbus, 0);

	/*
	 * enable direct routing of interrupts. We do this because
	 * we can not manage to get pccb_intr_establish() called until
	 * PCI subsystem is merged with rbus. The major thing that this
	 * routine does is avoid calling the driver's interrupt routine
	 * when the card has been removed.
	 *
	 * The rbus_ppb.c can not cope with card desertions until the merging
	 * anyway.
	 */
	pccbb_intr_route(psc);

	/*
	 * Attach the PCI bus than hangs off of it.
	 *
	 * XXX Don't pass-through Memory Read Multiple.  Should we?
	 * XXX Consult the spec...
	 */
	pba.pba_iot  = ca->ca_iot;
	pba.pba_memt = ca->ca_memt;
	pba.pba_dmat = ca->ca_dmat;
	pba.pba_pc   = psc->sc_pc;
	pba.pba_flags    = PCI_FLAGS_IO_OKAY|PCI_FLAGS_MEM_OKAY;
	pba.pba_bus      = PPB_BUSINFO_SECONDARY(busdata);
	pba.pba_bridgetag = &csc->sc_tag;
	/*pba.pba_intrswiz = parent_sc->sc_intrswiz; */
	pba.pba_intrtag  = psc->sc_pa.pa_intrtag;

	config_found_ia(self, "pcibus", &pba, rppbprint);
}

int
ppb_activate(device_t self, enum devact act)
{
  printf("ppb_activate called\n");
  return 0;
}

