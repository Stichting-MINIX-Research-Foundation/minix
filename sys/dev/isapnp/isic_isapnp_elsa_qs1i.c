/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	isic - I4B Siemens ISDN Chipset Driver for ELSA Quickstep 1000pro ISA
 *	=====================================================================
 *
 *	$Id: isic_isapnp_elsa_qs1i.c,v 1.16 2007/10/19 12:00:32 ad Exp $
 *
 *      last edit-date: [Fri Jan  5 11:38:29 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: isic_isapnp_elsa_qs1i.c,v 1.16 2007/10/19 12:00:32 ad Exp $");

#include "opt_isicpnp.h"
#if defined(ISICPNP_ELSA_QS1ISA) || defined(ISICPNP_ELSA_PCC16)

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>

#if defined(__NetBSD__) && __NetBSD_Version__ >= 104230000
#include <sys/callout.h>
#endif

#ifdef __FreeBSD__
#if __FreeBSD__ >= 3
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif
#include <machine/clock.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/pnp.h>
#else
#include <sys/bus.h>
#include <sys/device.h>
#endif

#ifdef __FreeBSD__
#include <machine/i4b_debug.h>
#include <machine/i4b_ioctl.h>
#else
#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#endif

#include <netisdn/i4b_global.h>
#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_mbuf.h>

#include <dev/ic/isic_l1.h>
#include <dev/ic/isac.h>
#include <dev/ic/hscx.h>

#ifdef __FreeBSD__
static void i4b_eq1i_clrirq(void* base);
#else
static void i4b_eq1i_clrirq(struct isic_softc *sc);
void isic_attach_Eqs1pi(struct isic_softc *sc);
static void elsa_command_req(struct isic_softc *sc, int command, void *data);
static void elsa_led_handler(void *);
#endif

/* masks for register encoded in base addr */

#define ELSA_BASE_MASK		0x0ffff
#define ELSA_OFF_MASK		0xf0000

/* register id's to be encoded in base addr */

#define ELSA_IDISAC		0x00000
#define ELSA_IDHSCXA		0x10000
#define ELSA_IDHSCXB		0x20000

/* offsets from base address */

#define ELSA_OFF_ISAC		0x00
#define ELSA_OFF_HSCX		0x02
#define ELSA_OFF_OFF		0x03
#define ELSA_OFF_CTRL		0x04
#define ELSA_OFF_CFG		0x05
#define ELSA_OFF_TIMR		0x06
#define ELSA_OFF_IRQ		0x07

/* control register (write access) */

#define ELSA_CTRL_LED_YELLOW	0x02
#define ELSA_CTRL_LED_GREEN	0x08
#define ELSA_CTRL_RESET		0x20
#define ELSA_CTRL_TIMEREN	0x80
#define ELSA_CTRL_SECRET	0x50

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/ISA clear IRQ routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__
static void
i4b_eq1i_clrirq(void* base)
{
	outb((u_int)base + ELSA_OFF_IRQ, 0);
}

#else
static void
i4b_eq1i_clrirq(struct isic_softc *sc)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	bus_space_write_1(t, h, ELSA_OFF_IRQ, 0);
}
#endif

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/ISA ISAC get fifo routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
eqs1pi_read_fifo(void *buf, const void *base, size_t len)
{
	if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXB)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, 0x40);
		insb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX), (u_char *)buf, (u_int)len);
	}
	else if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXA)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, 0);
		insb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX), (u_char *)buf, (u_int)len);
	}
	else /* if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDISAC) */
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, 0);
		insb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ISAC), (u_char *)buf, (u_int)len);
	}
}

#else

static void
eqs1pi_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_read_multi_1(t, h, ELSA_OFF_ISAC, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_read_multi_1(t, h, ELSA_OFF_HSCX, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40);
			bus_space_read_multi_1(t, h, ELSA_OFF_HSCX, buf, size);
			break;
	}
}

#endif

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/ISA ISAC put fifo routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
eqs1pi_write_fifo(void *base, const void *buf, size_t len)
{
	if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXB)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, 0x40);
		outsb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX), (u_char *)buf, (u_int)len);
	}
	else if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXA)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, 0);
		outsb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX), (u_char *)buf, (u_int)len);
	}
	else /* if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDISAC) */
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, 0);
		outsb((((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ISAC), (u_char *)buf, (u_int)len);
	}
}

#else

static void
eqs1pi_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_write_multi_1(t, h, ELSA_OFF_ISAC, buf, size);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0);
			bus_space_write_multi_1(t, h, ELSA_OFF_HSCX, buf, size);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40);
			bus_space_write_multi_1(t, h, ELSA_OFF_HSCX, buf, size);
			break;
	}
}
#endif

/*---------------------------------------------------------------------------*
 *      ELSA QuickStep 1000pro/ISA ISAC put register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static void
eqs1pi_write_reg(u_char *base, u_int offset, u_int v)
{
	if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXB)
	{
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, (u_char)(offset+0x40));
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX, (u_char)v);
	}
	else if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXA)
	{
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, (u_char)offset);
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX, (u_char)v);
	}
	else /* if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDISAC) */
	{
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, (u_char)offset);
	        outb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ISAC, (u_char)v);
	}
}

#else

static void
eqs1pi_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			bus_space_write_1(t, h, ELSA_OFF_ISAC, data);
			break;
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			bus_space_write_1(t, h, ELSA_OFF_HSCX, data);
			break;
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40+offs);
			bus_space_write_1(t, h, ELSA_OFF_HSCX, data);
			break;
	}
}
#endif

/*---------------------------------------------------------------------------*
 *	ELSA QuickStep 1000pro/ISA ISAC get register routine
 *---------------------------------------------------------------------------*/
#ifdef __FreeBSD__

static u_char
eqs1pi_read_reg(u_char *base, u_int offset)
{
	if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXB)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, (u_char)(offset+0x40));
		return(inb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX));
	}
	else if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDHSCXA)
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, (u_char)offset);
		return(inb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_HSCX));
	}
	else /* if(((u_int)base & ELSA_OFF_MASK) == ELSA_IDISAC) */
	{
	        outb((u_int)((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_OFF, (u_char)offset);
		return(inb(((u_int)base & ELSA_BASE_MASK) + ELSA_OFF_ISAC));
	}
}

#else

static u_int8_t
eqs1pi_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
		case ISIC_WHAT_ISAC:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			return bus_space_read_1(t, h, ELSA_OFF_ISAC);
		case ISIC_WHAT_HSCXA:
			bus_space_write_1(t, h, ELSA_OFF_OFF, offs);
			return bus_space_read_1(t, h, ELSA_OFF_HSCX);
		case ISIC_WHAT_HSCXB:
			bus_space_write_1(t, h, ELSA_OFF_OFF, 0x40+offs);
			return bus_space_read_1(t, h, ELSA_OFF_HSCX);
	}
	return 0;
}

#endif

#ifdef __FreeBSD__

/*---------------------------------------------------------------------------*
 *	isic_probe_Eqs1pi - probe for ELSA QuickStep 1000pro/ISA and compatibles
 *---------------------------------------------------------------------------*/
int
isic_probe_Eqs1pi(struct isa_device *dev, unsigned int iobase2)
{
	struct isic_softc *sc = &l1_sc[dev->id_unit];

	/* check max unit range */

	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for ELSA QuickStep 1000pro/ISA!\n",
				dev->id_unit, dev->id_unit);
		return(0);
	}
	sc->sc_unit = dev->id_unit;

	/* check IRQ validity */

	switch(ffs(dev->id_irq) - 1)
	{
		case 3:
		case 4:
		case 5:
		case 7:
		case 10:
		case 11:
		case 12:
		case 15:
			break;

		default:
			printf("isic%d: Error, invalid IRQ [%d] specified for ELSA QuickStep 1000pro/ISA!\n",
				dev->id_unit, ffs(dev->id_irq)-1);
			return(0);
			break;
	}
	sc->sc_irq = dev->id_irq;

	/* check if memory addr specified */

	if(dev->id_maddr)
	{
		printf("isic%d: Error, mem addr 0x%lx specified for ELSA QuickStep 1000pro/ISA!\n",
			dev->id_unit, (u_long)dev->id_maddr);
		return(0);
	}
	dev->id_msize = 0;

	/* check if we got an iobase */

	if(!((dev->id_iobase >= 0x160) && (dev->id_iobase <= 0x360)))
	{
		printf("isic%d: Error, invalid iobase 0x%x specified for ELSA QuickStep 1000pro/ISA!\n",
			dev->id_unit, dev->id_iobase);
		return(0);
	}
	sc->sc_port = dev->id_iobase;

	/* setup access routines */

	sc->clearirq = i4b_eq1i_clrirq;
	sc->readreg = eqs1pi_read_reg;
	sc->writereg = eqs1pi_write_reg;

	sc->readfifo = eqs1pi_read_fifo;
	sc->writefifo = eqs1pi_write_fifo;

	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_ELSAQS1ISA;

	/* setup IOM bus type */

	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	/* setup ISAC and HSCX base addr */

	ISAC_BASE   = (void *) ((u_int)dev->id_iobase | ELSA_IDISAC);
	HSCX_A_BASE = (void *) ((u_int)dev->id_iobase | ELSA_IDHSCXA);
	HSCX_B_BASE = (void *) ((u_int)dev->id_iobase | ELSA_IDHSCXB);

	/*
	 * Read HSCX A/B VSTR.  Expected value for the ELSA QuickStep 1000pro
	 * ISA card is 0x05 ( = version 2.1 ) in the least significant bits.
	 */

	if( ((HSCX_READ(0, H_VSTR) & 0xf) != 0x5) ||
            ((HSCX_READ(1, H_VSTR) & 0xf) != 0x5) )
	{
		printf("isic%d: HSCX VSTR test failed for ELSA QuickStep 1000pro/ISA\n",
			dev->id_unit);
		printf("isic%d: HSC0: VSTR: %#x\n",
			dev->id_unit, HSCX_READ(0, H_VSTR));
		printf("isic%d: HSC1: VSTR: %#x\n",
			dev->id_unit, HSCX_READ(1, H_VSTR));
		return (0);
	}

	return (1);
}

/*---------------------------------------------------------------------------*
 *	isic_attach_s0163P - attach ELSA QuickStep 1000pro/ISA
 *---------------------------------------------------------------------------*/
int
isic_attach_Eqs1pi(struct isa_device *dev, unsigned int iobase2)
{
	u_char byte = ELSA_CTRL_SECRET;

	byte &= ~ELSA_CTRL_RESET;
        outb(dev->id_iobase + ELSA_OFF_CTRL, byte);
        DELAY(20);
	byte |= ELSA_CTRL_RESET;
        outb(dev->id_iobase + ELSA_OFF_CTRL, byte);

        DELAY(20);
        outb(dev->id_iobase + ELSA_OFF_IRQ, 0xff);

	return(1);
}

#else /* !__FreeBSD__ */

static void
elsa_command_req(struct isic_softc *sc, int command, void *data)
{
	int v, s, blink;
	u_int8_t led_val;

	switch (command) {
	case CMR_DOPEN:
		s = splnet();

		v = ELSA_CTRL_SECRET & ~ELSA_CTRL_RESET;
	        bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h,
		    ELSA_OFF_CTRL, v);
		delay(20);
		v |= ELSA_CTRL_RESET;
		bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h,
		    ELSA_OFF_CTRL, v);
		delay(20);
		bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h,
		    ELSA_OFF_IRQ, 0xff);

		splx(s);
		break;

	case CMR_DCLOSE:
		s = splnet();
		callout_stop(&sc->sc_driver_callout);
		bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h,
		    ELSA_OFF_IRQ, 0);
		v = ELSA_CTRL_SECRET & ~ELSA_CTRL_RESET;
	        bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h,
		    ELSA_OFF_CTRL, v);
		delay(20);
		v |= ELSA_CTRL_RESET;
		bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h,
		    ELSA_OFF_CTRL, v);
		splx(s);
		break;

	case CMR_SETLEDS:
		/* the magic value and keep reset off */
		led_val = ELSA_CTRL_SECRET|ELSA_CTRL_RESET;

		/* now see what LEDs we want to add */
		v = (int)data;
		if (v & CMRLEDS_TEI)
			led_val |= ELSA_CTRL_LED_GREEN;
		blink = 0;
		if (v & (CMRLEDS_B0|CMRLEDS_B1)) {
			led_val |= ELSA_CTRL_LED_YELLOW;
			if ((v & (CMRLEDS_B0|CMRLEDS_B1)) == (CMRLEDS_B0|CMRLEDS_B1))
				blink = hz/4;
			else
				blink = hz;
			sc->sc_driver_specific = v;
		}

		s = splnet();
		bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h,
		    ELSA_OFF_CTRL, led_val);
		callout_stop(&sc->sc_driver_callout);
		if (blink)
			callout_reset(&sc->sc_driver_callout, blink,
			    elsa_led_handler, sc);
		splx(s);

		break;

	default:
		return;
	}
}

static void
elsa_led_handler(void *token)
{
	struct isic_softc *sc = token;
	int v, s, blink, off = 0;
	u_int8_t led_val = ELSA_CTRL_SECRET|ELSA_CTRL_RESET;

	s = splnet();
	v = sc->sc_driver_specific;
	if (v > 0) {
		/* turn blinking LED off */
		v = -sc->sc_driver_specific;
		sc->sc_driver_specific = v;
		off = 1;
	} else {
		sc->sc_driver_specific = -v;
	}
	if (v & CMRLEDS_TEI)
		led_val |= ELSA_CTRL_LED_GREEN;
	blink = 0;
	if (off == 0) {
		if (v & (CMRLEDS_B0|CMRLEDS_B1))
			led_val |= ELSA_CTRL_LED_YELLOW;
	}
	if ((v & (CMRLEDS_B0|CMRLEDS_B1)) == (CMRLEDS_B0|CMRLEDS_B1))
		blink = hz/4;
	else
		blink = hz;

	bus_space_write_1(sc->sc_maps[0].t, sc->sc_maps[0].h,
	    ELSA_OFF_CTRL, led_val);
	if (blink)
		callout_reset(&sc->sc_driver_callout, blink,
		    elsa_led_handler, sc);
	splx(s);
}

void
isic_attach_Eqs1pi(struct isic_softc *sc)
{
	/* setup access routines */

	sc->clearirq = i4b_eq1i_clrirq;
	sc->readreg = eqs1pi_read_reg;
	sc->writereg = eqs1pi_write_reg;

	sc->readfifo = eqs1pi_read_fifo;
	sc->writefifo = eqs1pi_write_fifo;

	sc->drv_command = elsa_command_req;

	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_ELSAQS1ISA;

	/* setup IOM bus type */

	sc->sc_bustyp = BUS_TYPE_IOM2;

	sc->sc_ipac = 0;
	sc->sc_bfifolen = HSCX_FIFO_LEN;

	callout_init(&sc->sc_driver_callout, 0);
	sc->sc_driver_specific = 0;
}

#endif

#endif /* ISICPNP_ELSA_QS1ISA or ISICPNP_ELSA_PCC16 */
