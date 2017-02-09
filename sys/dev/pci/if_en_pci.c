/*	$NetBSD: if_en_pci.c,v 1.37 2014/03/29 19:28:24 christos Exp $	*/

/*
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *
 * i f _ e n _ p c i . c
 *
 * author: Chuck Cranor <chuck@netbsd>
 * started: spring, 1996.
 *
 * PCI glue for the eni155p card.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_en_pci.c,v 1.37 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/midwayreg.h>
#include <dev/ic/midwayvar.h>


/*
 * local structures
 */

struct en_pci_softc {
  /* bus independent stuff */
  struct en_softc esc;

  /* PCI bus glue */
  void *sc_ih;			/* interrupt handle */
  pci_chipset_tag_t en_pc;	/* for PCI calls */

};

/*
 * local defines (PCI specific stuff)
 */

#if !defined(MIDWAY_ENIONLY)
static  void eni_get_macaddr(struct en_pci_softc *, struct pci_attach_args *);
#endif
#if !defined(MIDWAY_ADPONLY)
static  void adp_get_macaddr(struct en_pci_softc *, struct pci_attach_args *);
#endif

/*
 * address of config base memory address register in PCI config space
 * (this is card specific)
 */

#define PCI_CBMA PCI_BAR(0)

/*
 * tonga (pci bridge).   ENI cards only!
 */

#define EN_TONGA        0x60            /* PCI config addr of tonga reg */

#define TONGA_SWAP_DMA  0x80            /* endian swap control */
#define TONGA_SWAP_BYTE 0x40
#define TONGA_SWAP_WORD 0x20

/*
 * adaptec pci bridge.   ADP cards only!
 */

#define ADP_PCIREG	0x050040	/* PCI control register */

#define ADP_PCIREG_RESET	0x1	/* reset card */
#define ADP_PCIREG_IENABLE	0x2	/* interrupt enable */
#define ADP_PCIREG_SWAP_WORD	0x4	/* swap byte on slave access */
#define ADP_PCIREG_SWAP_DMA	0x8	/* swap bytes on DMA */

/*
 * prototypes
 */

static	int en_pci_match(device_t, cfdata_t, void *);
static	void en_pci_attach(device_t, device_t, void *);

/*
 * PCI autoconfig attachments
 */

CFATTACH_DECL_NEW(en_pci, sizeof(struct en_pci_softc),
    en_pci_match, en_pci_attach, NULL, NULL);

#if !defined(MIDWAY_ENIONLY)

static void adp_busreset(void *);

/*
 * bus specific reset function [ADP only!]
 */

static void adp_busreset(void *v)
{
  struct en_softc *sc = (struct en_softc *) v;
  u_int32_t dummy;

  bus_space_write_4(sc->en_memt, sc->en_base, ADP_PCIREG, ADP_PCIREG_RESET);
  DELAY(1000);  /* let it reset */
  dummy = bus_space_read_4(sc->en_memt, sc->en_base, ADP_PCIREG);
  bus_space_write_4(sc->en_memt, sc->en_base, ADP_PCIREG,
                (ADP_PCIREG_SWAP_WORD|ADP_PCIREG_SWAP_DMA|ADP_PCIREG_IENABLE));
  dummy = bus_space_read_4(sc->en_memt, sc->en_base, ADP_PCIREG);
  if ((dummy & (ADP_PCIREG_SWAP_WORD|ADP_PCIREG_SWAP_DMA)) !=
                (ADP_PCIREG_SWAP_WORD|ADP_PCIREG_SWAP_DMA))
    printf("adp_busreset: Adaptec ATM did NOT reset!\n");

}
#endif

/***********************************************************************/

/*
 * autoconfig stuff
 */

static int
en_pci_match(device_t parent, cfdata_t match, void *aux)
{
  struct pci_attach_args *pa = (struct pci_attach_args *) aux;

#if !defined(MIDWAY_ADPONLY)
  if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_EFFICIENTNETS &&
      (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_EFFICIENTNETS_ENI155PF ||
       PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_EFFICIENTNETS_ENI155PA))
    return 1;
#endif

#if !defined(MIDWAY_ENIONLY)
  if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ADP &&
      (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADP_AIC5900 ||
       PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADP_AIC5905))
    return 1;
#endif

  return 0;
}


static void
en_pci_attach(device_t parent, device_t self, void *aux)
{
  struct en_pci_softc *scp = device_private(self);
  struct en_softc *sc = &scp->esc;
  struct pci_attach_args *pa = aux;
  pci_intr_handle_t ih;
  const char *intrstr;
  int retval;
  char intrbuf[PCI_INTRSTR_LEN];

  sc->sc_dev = self;

  aprint_naive(": ATM controller\n");
  aprint_normal("\n");

  sc->is_adaptec = (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ADP) ? 1 : 0;
  scp->en_pc = pa->pa_pc;

  /*
   * make sure bus mastering is enabled
   */
  pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
    pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG) |
    PCI_COMMAND_MASTER_ENABLE);

  /*
   * interrupt map
   */

  if (pci_intr_map(pa, &ih)) {
    aprint_error_dev(sc->sc_dev, "couldn't map interrupt\n");
    return;
  }
  intrstr = pci_intr_string(scp->en_pc, ih, intrbuf, sizeof(intrbuf));
  scp->sc_ih = pci_intr_establish(scp->en_pc, ih, IPL_NET, en_intr, sc);
  if (scp->sc_ih == NULL) {
    aprint_error_dev(sc->sc_dev, "couldn't establish interrupt\n");
    if (intrstr != NULL)
      aprint_error(" at %s", intrstr);
    aprint_error("\n");
    return;
  }
  aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);
  sc->ipl = 1; /* XXX */

  /*
   * memory map
   */

  retval = pci_mapreg_map(pa, PCI_CBMA,
			  PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
			  &sc->en_memt, &sc->en_base, NULL, &sc->en_obmemsz);
  if (retval) {
    aprint_error_dev(sc->sc_dev, "couldn't map memory\n");
    return;
  }

  /*
   * set up pci bridge
   */

#if !defined(MIDWAY_ENIONLY)
  if (sc->is_adaptec) {
    adp_get_macaddr(scp, pa);
    sc->en_busreset = adp_busreset;
    adp_busreset(sc);
  }
#endif

#if !defined(MIDWAY_ADPONLY)
  if (!sc->is_adaptec) {
    eni_get_macaddr(scp, pa);
    sc->en_busreset = NULL;
    pci_conf_write(scp->en_pc, pa->pa_tag, EN_TONGA,
		  (TONGA_SWAP_DMA|TONGA_SWAP_WORD));
  }
#endif

  /*
   * done PCI specific stuff
   */

  en_attach(sc);

}

#if 0
static void
en_pci_shutdown(
	int howto,
	void *sc)
{
    struct en_pci_softc *psc = (struct en_pci_softc *)sc;

    en_reset(&psc->esc);
    DELAY(10);
}
#endif

#if !defined(MIDWAY_ENIONLY)

#if defined(__FreeBSD__)
#define bus_space_read_1(t, h, o) \
  		((void)t, (*(volatile u_int8_t *)((h) + (o))))
#endif

static void
adp_get_macaddr(struct en_pci_softc *scp, struct pci_attach_args *pa)
{
  struct en_softc * sc = (struct en_softc *)scp;
  int lcv;

  for (lcv = 0; lcv < sizeof(sc->macaddr); lcv++)
    sc->macaddr[lcv] = bus_space_read_1(sc->en_memt, sc->en_base,
					MID_ADPMACOFF + lcv);
}

#endif /* MIDWAY_ENIONLY */

#if !defined(MIDWAY_ADPONLY)

/*
 * Read station (MAC) address from serial EEPROM.
 * derived from linux drivers/atm/eni.c by Werner Almesberger, EPFL LRC.
 */
#define EN_PROM_MAGIC  0x0c
#define EN_PROM_DATA   0x02
#define EN_PROM_CLK    0x01
#define EN_ESI         64

static void
eni_get_macaddr(struct en_pci_softc *scp, struct pci_attach_args *pa)
{
  struct en_softc *sc = (struct en_softc *)scp;
  pci_chipset_tag_t id = scp->en_pc;
  pcitag_t tag = pa->pa_tag;
  int i, j, address;
  u_int32_t data, t_data;
  u_int8_t tmp;

  t_data = pci_conf_read(id, tag, EN_TONGA) & 0xffffff00;

  data =  EN_PROM_MAGIC | EN_PROM_DATA | EN_PROM_CLK;
  pci_conf_write(id, tag, EN_TONGA, data);

  for (i = 0; i < sizeof(sc->macaddr); i ++){
    /* start operation */
    data |= EN_PROM_DATA ;
    pci_conf_write(id, tag, EN_TONGA, data);
    data |= EN_PROM_CLK ;
    pci_conf_write(id, tag, EN_TONGA, data);
    data &= ~EN_PROM_DATA ;
    pci_conf_write(id, tag, EN_TONGA, data);
    data &= ~EN_PROM_CLK ;
    pci_conf_write(id, tag, EN_TONGA, data);
    /* send address with serial line */
    address = ((i + EN_ESI) << 1) + 1;
    for ( j = 7 ; j >= 0 ; j --){
      data = (address >> j) & 1 ? data | EN_PROM_DATA :
      data & ~EN_PROM_DATA;
      pci_conf_write(id, tag, EN_TONGA, data);
      data |= EN_PROM_CLK ;
      pci_conf_write(id, tag, EN_TONGA, data);
      data &= ~EN_PROM_CLK ;
      pci_conf_write(id, tag, EN_TONGA, data);
    }
    /* get ack */
    data |= EN_PROM_DATA ;
    pci_conf_write(id, tag, EN_TONGA, data);
    data |= EN_PROM_CLK ;
    pci_conf_write(id, tag, EN_TONGA, data);
    data = pci_conf_read(id, tag, EN_TONGA);
    data &= ~EN_PROM_CLK ;
    pci_conf_write(id, tag, EN_TONGA, data);
    data |= EN_PROM_DATA ;
    pci_conf_write(id, tag, EN_TONGA, data);

    tmp = 0;

    for ( j = 7 ; j >= 0 ; j --){
      tmp <<= 1;
      data |= EN_PROM_DATA ;
      pci_conf_write(id, tag, EN_TONGA, data);
      data |= EN_PROM_CLK ;
      pci_conf_write(id, tag, EN_TONGA, data);
      data = pci_conf_read(id, tag, EN_TONGA);
      if(data & EN_PROM_DATA) tmp |= 1;
      data &= ~EN_PROM_CLK ;
      pci_conf_write(id, tag, EN_TONGA, data);
      data |= EN_PROM_DATA ;
      pci_conf_write(id, tag, EN_TONGA, data);
    }
    /* get ack */
    data |= EN_PROM_DATA ;
    pci_conf_write(id, tag, EN_TONGA, data);
    data |= EN_PROM_CLK ;
    pci_conf_write(id, tag, EN_TONGA, data);
    data = pci_conf_read(id, tag, EN_TONGA);
    data &= ~EN_PROM_CLK ;
    pci_conf_write(id, tag, EN_TONGA, data);
    data |= EN_PROM_DATA ;
    pci_conf_write(id, tag, EN_TONGA, data);

    sc->macaddr[i] = tmp;
  }
  /* stop operation */
  data &=  ~EN_PROM_DATA;
  pci_conf_write(id, tag, EN_TONGA, data);
  data |=  EN_PROM_CLK;
  pci_conf_write(id, tag, EN_TONGA, data);
  data |=  EN_PROM_DATA;
  pci_conf_write(id, tag, EN_TONGA, data);
  pci_conf_write(id, tag, EN_TONGA, t_data);
}

#endif /* !MIDWAY_ADPONLY */
