/*	$NetBSD: vme_two_isr.c,v 1.16 2012/10/27 17:18:27 chs Exp $	*/

/*-
 * Copyright (c) 2001, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Steve C. Woodford.
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
 * Split off from vme_two.c specifically to deal with hardware assisted
 * soft interrupts when the user hasn't specified `vmetwo0' in the
 * kernel config file (mvme1[67]2 only).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vme_two_isr.c,v 1.16 2012/10/27 17:18:27 chs Exp $");

#include "vmetwo.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/cpu.h>
#include <sys/bus.h>

#include <dev/vme/vmereg.h>
#include <dev/vme/vmevar.h>

#include <dev/mvme/mvmebus.h>
#include <dev/mvme/vme_tworeg.h>
#include <dev/mvme/vme_twovar.h>

/*
 * Non-zero if there is no VMEChip2 on this board.
 */
int vmetwo_not_present;

/*
 * Array of interrupt handlers registered with us for the non-VMEbus
 * vectored interrupts. Eg. ABORT Switch, SYSFAIL etc.
 *
 * We can't just install a caller's handler directly, since these
 * interrupts have to be manually cleared, so we have a trampoline
 * which does the clearing automatically.
 */
static struct vme_two_handler {
	int (*isr_hand)(void *);
	void *isr_arg;
} vme_two_handlers[(VME2_VECTOR_LOCAL_MAX - VME2_VECTOR_LOCAL_MIN) + 1];

#define VMETWO_HANDLERS_SZ	(sizeof(vme_two_handlers) /	\
				 sizeof(struct vme_two_handler))

static	int  vmetwo_local_isr_trampoline(void *);
#ifdef notyet
static	void vmetwo_softintr_assert(void);
#endif

static	struct vmetwo_softc *vmetwo_sc;

int
vmetwo_probe(bus_space_tag_t bt, bus_addr_t offset)
{
	bus_space_handle_t bh;

	bus_space_map(bt, offset + VME2REG_LCSR_OFFSET, VME2LCSR_SIZE, 0, &bh);

	if (bus_space_peek_4(bt, bh, VME2LCSR_MISC_STATUS, NULL) != 0) {
#if defined(MVME162) || defined(MVME172)
#if defined(MVME167) || defined(MVME177)
		if (machineid == MVME_162 || machineid == MVME_172)
#endif
		{
			/*
			 * No VMEChip2 on mvme162/172 is not too big a
			 * deal; we can fall back on timer4 in the
			 * mcchip for h/w assisted soft interrupts...
			 */
			extern void pcctwosoftintrinit(void);
			bus_space_unmap(bt, bh, VME2LCSR_SIZE);
			vmetwo_not_present = 1;
			pcctwosoftintrinit();
			return (0);
		}
#endif
#if defined(MVME167) || defined(MVME177) || defined(MVME88K)
		/*
		 * No VMEChip2 on mvme167/177, however, is a Big Deal.
		 * In fact, it means the hardware's shot since the
		 * VMEChip2 is not a `build-option' on those boards.
		 */
		panic("VMEChip2 not responding! Faulty board?");
		/* NOTREACHED */
#endif
#if defined(MVMEPPC)
		/*
		 * No VMEChip2 on mvmeppc is no big deal.
		 */
		bus_space_unmap(bt, bh, VME2LCSR_SIZE);
		vmetwo_not_present = 1;
		return (0);
#endif
	}
#if NVMETWO == 0
	else {
		/*
		 * The kernel config file has no `vmetwo0' device, but
		 * there is a VMEChip2 on the board. Fix up things
		 * just enough to hook VMEChip2 local interrupts.
		 */
		struct vmetwo_softc *sc;

		/* XXX Should check sc != NULL here... */
		sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT);

		sc->sc_mvmebus.sc_bust = bt;
		sc->sc_lcrh = bh;
		vmetwo_intr_init(sc);
		return 0;
	}
#else
	bus_space_unmap(bt, bh, VME2LCSR_SIZE);
	return (1);
#endif
}

void
vmetwo_intr_init(struct vmetwo_softc *sc)
{
	u_int32_t reg;
	int i;

	vmetwo_sc = sc;

	/* Clear out the ISR handler array */
	for (i = 0; i < VMETWO_HANDLERS_SZ; i++)
		vme_two_handlers[i].isr_hand = NULL;

	/*
	 * Initialize the chip.
	 * Firstly, disable all VMEChip2 Interrupts
	 */
	reg = vme2_lcsr_read(sc, VME2LCSR_MISC_STATUS) & ~VME2_MISC_STATUS_MIEN;
	vme2_lcsr_write(sc, VME2LCSR_MISC_STATUS, reg);
	vme2_lcsr_write(sc, VME2LCSR_LOCAL_INTERRUPT_ENABLE, 0);
	vme2_lcsr_write(sc, VME2LCSR_LOCAL_INTERRUPT_CLEAR,
	    VME2_LOCAL_INTERRUPT_CLEAR_ALL);

	/* Zap all the IRQ level registers */
	for (i = 0; i < VME2_NUM_IL_REGS; i++)
		vme2_lcsr_write(sc, VME2LCSR_INTERRUPT_LEVEL_BASE + (i * 4), 0);

	/* Disable the tick timers */
	reg = vme2_lcsr_read(sc, VME2LCSR_TIMER_CONTROL);
	reg &= ~VME2_TIMER_CONTROL_EN(0);
	reg &= ~VME2_TIMER_CONTROL_EN(1);
	vme2_lcsr_write(sc, VME2LCSR_TIMER_CONTROL, reg);

	/* Set the VMEChip2's vector base register to the required value */
	reg = vme2_lcsr_read(sc, VME2LCSR_VECTOR_BASE);
	reg &= ~VME2_VECTOR_BASE_MASK;
	reg |= VME2_VECTOR_BASE_REG_VALUE;
	vme2_lcsr_write(sc, VME2LCSR_VECTOR_BASE, reg);

	/* Set the Master Interrupt Enable bit now */
	reg = vme2_lcsr_read(sc, VME2LCSR_MISC_STATUS) | VME2_MISC_STATUS_MIEN;
	vme2_lcsr_write(sc, VME2LCSR_MISC_STATUS, reg);

	/* Allow the MD code the chance to do some initialising */
	vmetwo_md_intr_init(sc);

#if defined(MVME167) || defined(MVME177)
#if defined(MVME162) || defined(MVME172)
	if (machineid != MVME_162 && machineid != MVME_172)
#endif
	{
		/*
		 * Let the NMI handler deal with level 7 ABORT switch
		 * interrupts
		 */
		vmetwo_intr_establish(sc, 7, 7, VME2_VEC_ABORT, 1,
		    nmihand, NULL, NULL);
	}
#endif

	/* Setup hardware assisted soft interrupts */
#ifdef notyet
	vmetwo_intr_establish(sc, 1, 1, VME2_VEC_SOFT0, 1,
	    (int (*)(void *))softintr_dispatch, NULL, NULL);
	_softintr_chipset_assert = vmetwo_softintr_assert;
#endif
}

static int
vmetwo_local_isr_trampoline(void *arg)
{
	struct vme_two_handler *isr;
	int vec;

	vec = (int) arg;	/* 0x08 <= vec <= 0x1f */

	/* Clear the interrupt source */
	vme2_lcsr_write(vmetwo_sc, VME2LCSR_LOCAL_INTERRUPT_CLEAR,
	    VME2_LOCAL_INTERRUPT(vec));

	isr = &vme_two_handlers[vec - VME2_VECTOR_LOCAL_OFFSET];
	if (isr->isr_hand)
		(void) (*isr->isr_hand) (isr->isr_arg);
	else
		printf("vmetwo: Spurious local interrupt, vector 0x%x\n", vec);

	return (1);
}

void
vmetwo_local_intr_establish(int pri, int vec, int (*hand)(void *), void *arg, struct evcnt *evcnt)
{

	vmetwo_intr_establish(vmetwo_sc, pri, pri, vec, 1, hand, arg, evcnt);
}

/* ARGSUSED */
void
vmetwo_intr_establish(void *csc, int prior, int lvl, int vec, int first, int (*hand)(void *), void *arg, struct evcnt *evcnt)
{
	struct vmetwo_softc *sc = csc;
	u_int32_t reg;
	int bitoff;
	int iloffset, ilshift;
	int s;

	s = splhigh();

#if NVMETWO > 0
	/*
	 * Sort out interrupts generated locally by the VMEChip2 from
	 * those generated by VMEbus devices...
	 */
	if (vec >= VME2_VECTOR_LOCAL_MIN && vec <= VME2_VECTOR_LOCAL_MAX) {
#endif
		/*
		 * Local interrupts need to be bounced through some
		 * trampoline code which acknowledges/clears them.
		 */
		vme_two_handlers[vec - VME2_VECTOR_LOCAL_MIN].isr_hand = hand;
		vme_two_handlers[vec - VME2_VECTOR_LOCAL_MIN].isr_arg = arg;
		hand = vmetwo_local_isr_trampoline;
		arg = (void *) (vec - VME2_VECTOR_BASE);

		/*
		 * Interrupt enable/clear bit offset is 0x08 - 0x1f
		 */
		bitoff = vec - VME2_VECTOR_BASE;
#if NVMETWO > 0
		first = 1;	/* Force the interrupt to be enabled */
	} else {
		/*
		 * Interrupts originating from the VMEbus are
		 * controlled by an offset of 0x00 - 0x07
		 */
		bitoff = lvl - 1;
	}
#endif

	/* Hook the interrupt */
	(*sc->sc_isrlink)(sc->sc_isrcookie, hand, arg, prior, vec, evcnt);

	/*
	 * Do we need to tell the VMEChip2 to let the interrupt through?
	 * (This is always true for locally-generated interrupts, but only
	 * needs doing once for each VMEbus interrupt level which is hooked)
	 */
#if NVMETWO > 0
	if (first) {
		if (evcnt)
			evcnt_attach_dynamic(evcnt, EVCNT_TYPE_INTR,
			    (*sc->sc_isrevcnt)(sc->sc_isrcookie, prior),
			    device_xname(sc->sc_mvmebus.sc_dev),
			    mvmebus_irq_name[lvl]);
#endif
		iloffset = VME2_ILOFFSET_FROM_VECTOR(bitoff) +
		    VME2LCSR_INTERRUPT_LEVEL_BASE;
		ilshift = VME2_ILSHIFT_FROM_VECTOR(bitoff);

		/* Program the specified interrupt to signal at 'prior' */
		reg = vme2_lcsr_read(sc, iloffset);
		reg &= ~(VME2_INTERRUPT_LEVEL_MASK << ilshift);
		reg |= (prior << ilshift);
		vme2_lcsr_write(sc, iloffset, reg);

		/* Clear it */
		vme2_lcsr_write(sc, VME2LCSR_LOCAL_INTERRUPT_CLEAR,
		    VME2_LOCAL_INTERRUPT(bitoff));

		/* Enable it. */
		reg = vme2_lcsr_read(sc, VME2LCSR_LOCAL_INTERRUPT_ENABLE);
		reg |= VME2_LOCAL_INTERRUPT(bitoff);
		vme2_lcsr_write(sc, VME2LCSR_LOCAL_INTERRUPT_ENABLE, reg);
#if NVMETWO > 0
	}
#ifdef DIAGNOSTIC
	else {
		/* Verify the interrupt priority is the same */
		iloffset = VME2_ILOFFSET_FROM_VECTOR(bitoff) +
		    VME2LCSR_INTERRUPT_LEVEL_BASE;
		ilshift = VME2_ILSHIFT_FROM_VECTOR(bitoff);

		reg = vme2_lcsr_read(sc, iloffset);
		reg &= (VME2_INTERRUPT_LEVEL_MASK << ilshift);

		if ((prior << ilshift) != reg)
			panic("vmetwo_intr_establish: priority mismatch!");
	}
#endif
#endif
	splx(s);
}

void
vmetwo_intr_disestablish(void *csc, int lvl, int vec, int last, struct evcnt *evcnt)
{
	struct vmetwo_softc *sc = csc;
	u_int32_t reg;
	int iloffset, ilshift;
	int bitoff;
	int s;

	s = splhigh();

#if NVMETWO > 0
	/*
	 * Sort out interrupts generated locally by the VMEChip2 from
	 * those generated by VMEbus devices...
	 */
	if (vec >= VME2_VECTOR_LOCAL_MIN && vec <= VME2_VECTOR_LOCAL_MAX) {
#endif
		/*
		 * Interrupt enable/clear bit offset is 0x08 - 0x1f
		 */
		bitoff = vec - VME2_VECTOR_BASE;
		vme_two_handlers[vec - VME2_VECTOR_LOCAL_MIN].isr_hand = NULL;
		last = 1; /* Force the interrupt to be cleared */
#if NVMETWO > 0
	} else {
		/*
		 * Interrupts originating from the VMEbus are
		 * controlled by an offset of 0x00 - 0x07
		 */
		bitoff = lvl - 1;
	}
#endif

	/*
	 * Do we need to tell the VMEChip2 to block the interrupt?
	 * (This is always true for locally-generated interrupts, but only
	 * needs doing once when the last VMEbus handler for any given level
	 * has been unhooked.)
	 */
	if (last) {
		iloffset = VME2_ILOFFSET_FROM_VECTOR(bitoff) +
		    VME2LCSR_INTERRUPT_LEVEL_BASE;
		ilshift = VME2_ILSHIFT_FROM_VECTOR(bitoff);

		/* Disable it. */
		reg = vme2_lcsr_read(sc, VME2LCSR_LOCAL_INTERRUPT_ENABLE);
		reg &= ~VME2_LOCAL_INTERRUPT(bitoff);
		vme2_lcsr_write(sc, VME2LCSR_LOCAL_INTERRUPT_ENABLE, reg);

		/* Set the interrupt's level to zero */
		reg = vme2_lcsr_read(sc, iloffset);
		reg &= ~(VME2_INTERRUPT_LEVEL_MASK << ilshift);
		vme2_lcsr_write(sc, iloffset, reg);

		/* Clear it */
		vme2_lcsr_write(sc, VME2LCSR_LOCAL_INTERRUPT_CLEAR,
		    VME2_LOCAL_INTERRUPT(vec));

		if (evcnt)
			evcnt_detach(evcnt);
	}
	/* Un-hook it */
	(*sc->sc_isrunlink)(sc->sc_isrcookie, vec);

	splx(s);
}

#ifdef notyet
static void
vmetwo_softintr_assert(void)
{

	vme2_lcsr_write(vmetwo_sc, VME2LCSR_SOFTINT_SET, VME2_SOFTINT_SET(0));
}
#endif
