/*	$NetBSD: cardbusvar.h,v 1.56 2012/02/18 00:36:57 matt Exp $	*/

/*
 * Copyright (c) 1998, 1999 and 2000
 *       HAYAKAWA Koichi.  All rights reserved.
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_CARDBUS_CARDBUSVAR_H_
#define _DEV_CARDBUS_CARDBUSVAR_H_

#include <dev/cardbus/cardbusreg.h>
#include <dev/cardbus/rbus.h>

typedef struct cardbus_chipset_tag *cardbus_chipset_tag_t;

/*
 * struct cardbus_functions contains the pointers for basic cardbus
 * functions.  Those functions must be provided by cardbus bridge.
 * The child devices can use those functions.  The contained functions
 * are: cardbus_space_alloc, cardbus_space_free,
 * cardbus_intr_establish, cardbus_intr_disestablish, cardbus_ctrl,
 * cardbus_power, cardbus_make_tag, and cardbus_conf_write.
 *
 *	int (*cardbus_space_alloc)(cardbus_chipset_tag_t ct, rbus_tag_t rb,
 *	    bus_addr_t addr, bus_size_t size,
 *	    bus_addr_t mask, bus_size_t align,
 *	    int flags, bus_addr_t *addrp,
 *	    bus_space_handle_t *bshp);
 *
 *	int (*cardbus_space_free)(cardbus_chipset_tag_t ct, rbus_tag_t rb,
 *	    bus_space_handle_t, bus_size_t);
 *
 * cardbus_space_alloc and cardbus_space_free allocates and
 * disallocate bus space for the requesters.
 *
 *	void *(*cardbus_intr_establish)(cardbus_chipset_tag_t ct, int irq,
 *	     int level, int (*ih)(void *), void *sc);
 *
 *	void (*cardbus_intr_disestablish)(cardbus_chipset_tag_t ct, void *ih);
 *	int (*cardbus_ctrl)(cardbus_chipset_tag_t ct, int command);
 *	int (*cardbus_power)(cardbus_chipset_tag_t ct, int voltage);
 *
 *	pcitag_t (*cardbus_make_tag)(cardbus_chipset_tag_t ct,
 *	    int busno, int functionno);
 *	pcireg_t (*cardbus_conf_read)(cardbus_chipset_tag_t ct,
 *	    pcitag_t tag, int offs);
 *	void (*cardbus_conf_write)(cardbus_chipset_tag_t ct,
 *	    pcitag_t tag, int offs, pcireg_t val);
 */
typedef const struct cardbus_functions {
	int (*cardbus_space_alloc)(cardbus_chipset_tag_t, rbus_tag_t,
	    bus_addr_t, bus_size_t, bus_addr_t, bus_size_t,
	    int, bus_addr_t *, bus_space_handle_t *);
	int (*cardbus_space_free)(cardbus_chipset_tag_t, rbus_tag_t,
	    bus_space_handle_t, bus_size_t);
	void *(*cardbus_intr_establish)(cardbus_chipset_tag_t,
	    int, int (*)(void *), void *);
	void (*cardbus_intr_disestablish)(cardbus_chipset_tag_t, void *);
	int (*cardbus_ctrl)(cardbus_chipset_tag_t, int);
	int (*cardbus_power)(cardbus_chipset_tag_t, int);

	pcitag_t (*cardbus_make_tag)(cardbus_chipset_tag_t, int, int);
	pcireg_t (*cardbus_conf_read)(cardbus_chipset_tag_t,
	    pcitag_t, int);
	void (*cardbus_conf_write)(cardbus_chipset_tag_t, pcitag_t,
	    int, pcireg_t);
} cardbus_function_t, *cardbus_function_tag_t;

/*
 * struct cbslot_attach_args is the attach argument for Cardbus cardslot.
 */
struct cbslot_attach_args {
	bus_space_tag_t cba_iot;	/* cardbus i/o space tag */
	bus_space_tag_t cba_memt;	/* cardbus mem space tag */
	bus_dma_tag_t cba_dmat;		/* DMA tag */

	int cba_bus;			/* cardbus bus number */

	cardbus_chipset_tag_t cba_cc;	/* cardbus chipset */
	cardbus_function_tag_t cba_cf; /* cardbus functions */

	rbus_tag_t cba_rbus_iot;	/* CardBus i/o rbus tag */
	rbus_tag_t cba_rbus_memt;	/* CardBus mem rbus tag */

	int cba_cacheline;		/* cache line size */
	int cba_max_lattimer;		/* No card's latency timer may
					 * exceed this.  On a PCI-Cardbus
					 * bridge, I let the latency timer on
					 * the primary bus (PCI bus) set
					 * the maximum.  That is kind of an
					 * arbitrary choice.  A more sensible
					 * choice may be either the
					 * latency timer on the primary bus,
					 * or the bridge's buffer size,
					 * whichever is larger.
					 */
};


struct cardbus_devfunc;

/*
 * struct cardbus_softc is the softc for cardbus card.
 */
struct cardbus_softc {
	device_t sc_dev;		/* fundamental device structure */

	int sc_bus;			/* cardbus bus number */

	bus_space_tag_t sc_iot;		/* CardBus I/O space tag */
	bus_space_tag_t sc_memt;	/* CardBus MEM space tag */
	bus_dma_tag_t sc_dmat;		/* DMA tag */

	cardbus_chipset_tag_t sc_cc;	/* CardBus chipset */
	cardbus_function_tag_t sc_cf;	/* CardBus function */

	rbus_tag_t sc_rbus_iot;		/* CardBus i/o rbus tag */
	rbus_tag_t sc_rbus_memt;	/* CardBus mem rbus tag */

	int sc_cacheline;		/* cache line size */
	int sc_max_lattimer;		/* No card's latency timer
					 * may exceed this.  On a PCI-Cardbus
					 * bridge, the latency timer on
					 * the primary bus (PCI bus) sets
					 * the maximum.
					 */
	int sc_poweron_func;
  struct cardbus_devfunc *sc_funcs[8];	/* list of cardbus device functions */
};

struct cardbus_conf_state {
	pcireg_t reg[16];
};

/*
 * struct cardbus_devfunc:
 *
 *   This is the data deposit for each function of a CardBus device.
 *   This structure is used for memory or i/o space allocation and
 *   disallocation.
 */
typedef struct cardbus_devfunc {
	cardbus_chipset_tag_t ct_cc;
	cardbus_function_tag_t ct_cf;
	struct cardbus_softc *ct_sc;	/* pointer to the parent */
	int ct_bus;			/* bus number */
	int ct_func;			/* function number */

	pcireg_t ct_bhlc;		/* Latency timer and cache line size */
	/* u_int32_t ct_cisreg; */	/* CIS reg: is it needed??? */

	device_t ct_device;	/* pointer to the device */

	/* some data structure needed for tuple??? */
} *cardbus_devfunc_t;


/* XXX various things extracted from CIS */
struct cardbus_cis_info {
	int32_t		manufacturer;
	int32_t		product;
	char		cis1_info_buf[256];
	char*		cis1_info[4];
	struct cb_bar_info {
		unsigned int flags;
		unsigned int size;
	} bar[7];
	unsigned int	funcid;
	union {
		struct {
			int uart_type;
			int uart_present;
		} serial;
		struct {
			char netid[6];
			char netid_present;
			char __filler;
		} network;
	} funce;
};

struct cardbus_attach_args {
	cardbus_devfunc_t ca_ct;

	bus_space_tag_t ca_iot;		/* CardBus I/O space tag */
	bus_space_tag_t ca_memt;	/* CardBus MEM space tag */
	bus_dma_tag_t ca_dmat;		/* DMA tag */

	u_int ca_bus;
	u_int ca_function;
	pcitag_t ca_tag;
	pcireg_t ca_id;
	pcireg_t ca_class;

	rbus_tag_t ca_rbus_iot;		/* CardBus i/o rbus tag */
	rbus_tag_t ca_rbus_memt;	/* CardBus mem rbus tag */

	struct cardbus_cis_info ca_cis;
};


#define CARDBUS_ENABLE  1	/* enable the channel */
#define CARDBUS_DISABLE 2	/* disable the channel */
#define CARDBUS_RESET   4
#define CARDBUS_CD      7
#  define CARDBUS_NOCARD 0
#  define CARDBUS_5V_CARD 0x01	/* XXX: It must not exist */
#  define CARDBUS_3V_CARD 0x02
#  define CARDBUS_XV_CARD 0x04
#  define CARDBUS_YV_CARD 0x08
#define CARDBUS_IO_ENABLE    100
#define CARDBUS_IO_DISABLE   101
#define CARDBUS_MEM_ENABLE   102
#define CARDBUS_MEM_DISABLE  103
#define CARDBUS_BM_ENABLE    104 /* bus master */
#define CARDBUS_BM_DISABLE   105

#define CARDBUS_VCC_UC  0x0000
#define CARDBUS_VCC_3V  0x0001
#define CARDBUS_VCC_XV  0x0002
#define CARDBUS_VCC_YV  0x0003
#define CARDBUS_VCC_0V  0x0004
#define CARDBUS_VCC_5V  0x0005	/* ??? */
#define CARDBUS_VCCMASK 0x000f
#define CARDBUS_VPP_UC  0x0000
#define CARDBUS_VPP_VCC 0x0010
#define CARDBUS_VPP_12V 0x0030
#define CARDBUS_VPP_0V  0x0040
#define CARDBUS_VPPMASK 0x00f0


int cardbus_attach_card(struct cardbus_softc *);
void cardbus_detach_card(struct cardbus_softc *);
void *Cardbus_intr_establish(cardbus_devfunc_t,
    int, int (*)(void *), void *);
void Cardbus_intr_disestablish(cardbus_devfunc_t, void *);
void *cardbus_intr_establish(cardbus_chipset_tag_t, cardbus_function_tag_t,
    int, int (*) (void *), void *arg);
void cardbus_intr_disestablish(cardbus_chipset_tag_t, cardbus_function_tag_t,
    void *);

int cardbus_mapreg_map(struct cardbus_softc *, int, int, pcireg_t,
    int, bus_space_tag_t *, bus_space_handle_t *, bus_addr_t *, bus_size_t *);
int cardbus_mapreg_unmap(struct cardbus_softc *, int, int,
    bus_space_tag_t, bus_space_handle_t, bus_size_t);

int cardbus_function_enable(struct cardbus_softc *, int);
int cardbus_function_disable(struct cardbus_softc *, int);

int cardbus_get_capability(cardbus_chipset_tag_t, cardbus_function_tag_t,
    pcitag_t, int, int *, pcireg_t *);
int cardbus_get_powerstate(cardbus_devfunc_t, pcitag_t, pcireg_t *);
int cardbus_set_powerstate(cardbus_devfunc_t, pcitag_t, pcireg_t);

void cardbus_conf_capture(cardbus_chipset_tag_t, cardbus_function_tag_t,
    pcitag_t, struct cardbus_conf_state *);
void cardbus_conf_restore(cardbus_chipset_tag_t, cardbus_function_tag_t,
    pcitag_t, struct cardbus_conf_state *);

#define Cardbus_function_enable(ct) cardbus_function_enable((ct)->ct_sc, (ct)->ct_func)
#define Cardbus_function_disable(ct) cardbus_function_disable((ct)->ct_sc, (ct)->ct_func)



#define Cardbus_mapreg_map(ct, reg, type, busflags, tagp, handlep, basep, sizep) \
	cardbus_mapreg_map((ct)->ct_sc, (ct->ct_func), (reg), (type),\
			   (busflags), (tagp), (handlep), (basep), (sizep))
#define Cardbus_mapreg_unmap(ct, reg, tag, handle, size) \
	cardbus_mapreg_unmap((ct)->ct_sc, (ct->ct_func), (reg), (tag), (handle), (size))

#define Cardbus_make_tag(ct) (*(ct)->ct_cf->cardbus_make_tag)((ct)->ct_cc, (ct)->ct_bus, (ct)->ct_func)
#define cardbus_make_tag(cc, cf, bus, function) ((cf)->cardbus_make_tag)((cc), (bus), (function))

#define Cardbus_conf_read(ct, tag, offs) (*(ct)->ct_cf->cardbus_conf_read)((ct)->ct_cc, (tag), (offs))
#define cardbus_conf_read(cc, cf, tag, offs) ((cf)->cardbus_conf_read)((cc), (tag), (offs))
#define Cardbus_conf_write(ct, tag, offs, val) (*(ct)->ct_cf->cardbus_conf_write)((ct)->ct_cc, (tag), (offs), (val))
#define cardbus_conf_write(cc, cf, tag, offs, val) ((cf)->cardbus_conf_write)((cc), (tag), (offs), (val))

#endif /* !_DEV_CARDBUS_CARDBUSVAR_H_ */
