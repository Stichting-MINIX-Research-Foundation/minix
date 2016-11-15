/*
 *   Copyright (c) 1999 Udo Schweigert. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *   Based on ELSA Quickstep 1000pro PCI driver (i4b_elsa_qs1p.c)
 *---------------------------------------------------------------------------
 *   In case of trouble please contact Udo Schweigert <ust@cert.siemens.de>
 *---------------------------------------------------------------------------
 *
 *	Siemens I-Surf 2.0 PnP specific routines for isic driver
 *	--------------------------------------------------------
 *
 *	$Id: isic_isapnp_siemens_isurf.c,v 1.11 2007/10/19 12:00:32 ad Exp $
 *
 *      last edit-date: [Fri Jan  5 11:38:29 2001]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: isic_isapnp_siemens_isurf.c,v 1.11 2007/10/19 12:00:32 ad Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#if defined(__NetBSD__) && __NetBSD_Version__ >= 104230000
#include <sys/callout.h>
#endif

#if defined(__FreeBSD__)
#if __FreeBSD__ >= 3
#include <sys/ioccom.h>
#else
#include <sys/ioctl.h>
#endif

#include <machine/clock.h>
#include <i386/isa/isa_device.h>

#else
#include <sys/bus.h>
#include <sys/device.h>
#endif

#include <sys/socket.h>
#include <net/if.h>

#if defined(__FreeBSD__)
#include <machine/i4b_ioctl.h>
#else
#include <netisdn/i4b_ioctl.h>
#endif

#include <netisdn/i4b_global.h>
#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_mbuf.h>

#include <dev/ic/isic_l1.h>
#include <dev/ic/ipac.h>
#include <dev/ic/isac.h>
#include <dev/ic/hscx.h>

#if !defined(__FreeBSD__)
void isic_attach_siemens_isurf(struct isic_softc *sc);
#endif

/* masks for register encoded in base addr */

#define SIE_ISURF_BASE_MASK		0x0ffff
#define SIE_ISURF_OFF_MASK		0xf0000

/* register id's to be encoded in base addr */

#define SIE_ISURF_IDISAC		0x00000
#define SIE_ISURF_IDHSCXA		0x10000
#define SIE_ISURF_IDHSCXB		0x20000
#define SIE_ISURF_IDIPAC		0x40000

/* offsets from base address */

#define SIE_ISURF_OFF_ALE		0x00
#define SIE_ISURF_OFF_RW		0x01

/*---------------------------------------------------------------------------*
 *      Siemens I-Surf 2.0 PnP ISAC get fifo routine
 *---------------------------------------------------------------------------*/
#if defined(__FreeBSD__)

static void
siemens_isurf_read_fifo(void *buf, const void *base, size_t len)
{
	if(((u_int)base & SIE_ISURF_OFF_MASK) == SIE_ISURF_IDHSCXB)
	{
	        outb((u_int)((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_ALE, IPAC_HSCXB_OFF);
		insb((((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_RW), (u_char *)buf, (u_int)len);
	}
	else if(((u_int)base & SIE_ISURF_OFF_MASK) == SIE_ISURF_IDHSCXA)
	{
	        outb((u_int)((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_ALE, IPAC_HSCXA_OFF);
		insb((((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_RW), (u_char *)buf, (u_int)len);
	}
	else /* if(((u_int)base & SIE_ISURF_OFF_MASK) == SIE_ISURF_IDISAC) */
	{
	        outb((u_int)((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_ALE, IPAC_ISAC_OFF);
		insb((((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_RW), (u_char *)buf, (u_int)len);
	}
}

#else

static void
siemens_isurf_read_fifo(struct isic_softc *sc, int what, void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
	case ISIC_WHAT_ISAC:
		bus_space_write_1(t, h, SIE_ISURF_OFF_ALE, IPAC_ISAC_OFF);
		bus_space_read_multi_1(t, h, SIE_ISURF_OFF_RW, buf, size);
		break;
	case ISIC_WHAT_HSCXA:
		bus_space_write_1(t, h, SIE_ISURF_OFF_ALE, IPAC_HSCXA_OFF);
		bus_space_read_multi_1(t, h, SIE_ISURF_OFF_RW, buf, size);
		break;
	case ISIC_WHAT_HSCXB:
		bus_space_write_1(t, h, SIE_ISURF_OFF_ALE, IPAC_HSCXB_OFF);
		bus_space_read_multi_1(t, h, SIE_ISURF_OFF_RW, buf, size);
		break;
	}
}

#endif

/*---------------------------------------------------------------------------*
 *      Siemens I-Surf 2.0 PnP ISAC put fifo routine
 *---------------------------------------------------------------------------*/
#if defined(__FreeBSD__)

static void
siemens_isurf_write_fifo(void *base, const void *buf, size_t len)
{
	if(((u_int)base & SIE_ISURF_OFF_MASK) == SIE_ISURF_IDHSCXB)
	{
	        outb((u_int)((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_ALE, IPAC_HSCXB_OFF);
		outsb((((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_RW), (const u_char *)buf, (u_int)len);
	}
	else if(((u_int)base & SIE_ISURF_OFF_MASK) == SIE_ISURF_IDHSCXA)
	{
	        outb((u_int)((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_ALE, IPAC_HSCXA_OFF);
		outsb((((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_RW), (const u_char *)buf, (u_int)len);
	}
	else /* if(((u_int)base & SIE_ISURF_OFF_MASK) == SIE_ISURF_IDISAC) */
	{
	        outb((u_int)((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_ALE, IPAC_ISAC_OFF);
		outsb((((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_RW), (const u_char *)buf, (u_int)len);
	}
}

#else

static void
siemens_isurf_write_fifo(struct isic_softc *sc, int what, const void *buf, size_t size)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
	case ISIC_WHAT_ISAC:
		bus_space_write_1(t, h, SIE_ISURF_OFF_ALE, IPAC_ISAC_OFF);
		bus_space_write_multi_1(t, h, SIE_ISURF_OFF_RW, buf, size);
		break;
	case ISIC_WHAT_HSCXA:
		bus_space_write_1(t, h, SIE_ISURF_OFF_ALE, IPAC_HSCXA_OFF);
		bus_space_write_multi_1(t, h, SIE_ISURF_OFF_RW, buf, size);
		break;
	case ISIC_WHAT_HSCXB:
		bus_space_write_1(t, h, SIE_ISURF_OFF_ALE, IPAC_HSCXB_OFF);
		bus_space_write_multi_1(t, h, SIE_ISURF_OFF_RW, buf, size);
		break;
	}
}

#endif

/*---------------------------------------------------------------------------*
 *      Siemens I-Surf 2.0 PnP ISAC put register routine
 *---------------------------------------------------------------------------*/
#if defined(__FreeBSD__)

static void
siemens_isurf_write_reg(u_char *base, u_int offset, u_int v)
{
	if(((u_int)base & SIE_ISURF_OFF_MASK) == SIE_ISURF_IDHSCXB)
	{
	        outb(((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_ALE, (u_char)(offset+IPAC_HSCXB_OFF));
	        outb(((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_RW, (u_char)v);
	}
	else if(((u_int)base & SIE_ISURF_OFF_MASK) == SIE_ISURF_IDHSCXA)
	{
	        outb(((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_ALE, (u_char)(offset+IPAC_HSCXA_OFF));
	        outb(((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_RW, (u_char)v);
	}
	else if(((u_int)base & SIE_ISURF_OFF_MASK) == SIE_ISURF_IDISAC)
	{
	        outb(((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_ALE, (u_char)(offset+IPAC_ISAC_OFF));
	        outb(((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_RW, (u_char)v);
	}
	else /* IPAC */
	{
	        outb(((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_ALE, (u_char)(offset+IPAC_IPAC_OFF));
	        outb(((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_RW, (u_char)v);
	}
}

#else

static void
siemens_isurf_write_reg(struct isic_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
	case ISIC_WHAT_ISAC:
		bus_space_write_1(t, h, SIE_ISURF_OFF_ALE, IPAC_ISAC_OFF+offs);
		bus_space_write_1(t, h, SIE_ISURF_OFF_RW, data);
		break;
	case ISIC_WHAT_HSCXA:
		bus_space_write_1(t, h, SIE_ISURF_OFF_ALE, IPAC_HSCXA_OFF+offs);
		bus_space_write_1(t, h, SIE_ISURF_OFF_RW, data);
		break;
	case ISIC_WHAT_HSCXB:
		bus_space_write_1(t, h, SIE_ISURF_OFF_ALE, IPAC_HSCXB_OFF+offs);
		bus_space_write_1(t, h, SIE_ISURF_OFF_RW, data);
		break;
	case ISIC_WHAT_IPAC:
		bus_space_write_1(t, h, SIE_ISURF_OFF_ALE, IPAC_IPAC_OFF+offs);
		bus_space_write_1(t, h, SIE_ISURF_OFF_RW, data);
		break;
	}
}

#endif

/*---------------------------------------------------------------------------*
 *	Siemens I-Surf 2.0 PnP ISAC get register routine
 *---------------------------------------------------------------------------*/
#if defined(__FreeBSD__)

static u_char
siemens_isurf_read_reg(u_char *base, u_int offset)
{
	if(((u_int)base & SIE_ISURF_OFF_MASK) == SIE_ISURF_IDHSCXB)
	{
	        outb((u_int)((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_ALE, (u_char)(offset+IPAC_HSCXB_OFF));
		return(inb(((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_RW));
	}
	else if(((u_int)base & SIE_ISURF_OFF_MASK) == SIE_ISURF_IDHSCXA)
	{
	        outb((u_int)((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_ALE, (u_char)(offset+IPAC_HSCXA_OFF));
		return(inb(((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_RW));
	}
	else if(((u_int)base & SIE_ISURF_OFF_MASK) == SIE_ISURF_IDISAC)
	{
	        outb((u_int)((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_ALE, (u_char)(offset+IPAC_ISAC_OFF));
		return(inb(((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_RW));
	}
	else /* IPAC */
	{
	        outb((u_int)((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_ALE, (u_char)(offset+IPAC_IPAC_OFF));
		return(inb(((u_int)base & SIE_ISURF_BASE_MASK) + SIE_ISURF_OFF_RW));
	}
}

#else

static u_int8_t
siemens_isurf_read_reg(struct isic_softc *sc, int what, bus_size_t offs)
{
	bus_space_tag_t t = sc->sc_maps[0].t;
	bus_space_handle_t h = sc->sc_maps[0].h;
	switch (what) {
	case ISIC_WHAT_ISAC:
		bus_space_write_1(t, h, SIE_ISURF_OFF_ALE, IPAC_ISAC_OFF+offs);
		return bus_space_read_1(t, h, SIE_ISURF_OFF_RW);
	case ISIC_WHAT_HSCXA:
		bus_space_write_1(t, h, SIE_ISURF_OFF_ALE, IPAC_HSCXA_OFF+offs);
		return bus_space_read_1(t, h, SIE_ISURF_OFF_RW);
	case ISIC_WHAT_HSCXB:
		bus_space_write_1(t, h, SIE_ISURF_OFF_ALE, IPAC_HSCXB_OFF+offs);
		return bus_space_read_1(t, h, SIE_ISURF_OFF_RW);
	case ISIC_WHAT_IPAC:
		bus_space_write_1(t, h, SIE_ISURF_OFF_ALE, IPAC_IPAC_OFF+offs);
		return bus_space_read_1(t, h, SIE_ISURF_OFF_RW);
	}
	return 0;
}

#endif

/*---------------------------------------------------------------------------*
 *	isic_probe_siemens_isurf - probe for Siemens I-Surf 2.0 PnP
 *---------------------------------------------------------------------------*/
#if defined(__FreeBSD__)

int
isic_probe_siemens_isurf(struct isa_device *dev, unsigned int iobase2)
{
	struct isic_softc *sc = &l1_sc[dev->id_unit];

	/* check max unit range */

	if(dev->id_unit >= ISIC_MAXUNIT)
	{
		printf("isic%d: Error, unit %d >= ISIC_MAXUNIT for Siemens I-Surf 2.0 PnP\n",
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
			printf("isic%d: Error, invalid IRQ [%d] specified for Siemens I-Surf 2.0 PnP!\n",
				dev->id_unit, ffs(dev->id_irq)-1);
			return(0);
			break;
	}
	sc->sc_irq = dev->id_irq;

	/* check if memory addr specified */

	if(dev->id_maddr)
	{
		printf("isic%d: Error, mem addr 0x%lx specified for Siemens I-Surf 2.0 PnP!\n",
			dev->id_unit, (u_long)dev->id_maddr);
		return(0);
	}
	dev->id_msize = 0;

	/* check if we got an iobase */

	if(!((dev->id_iobase >= 0x100) && (dev->id_iobase <= 0xfffc)))
	{
		printf("isic%d: Error, invalid iobase 0x%x specified for Siemens I-Surf 2.0 PnP!\n",
			dev->id_unit, dev->id_iobase);
		return(0);
	}
	sc->sc_port = dev->id_iobase;


	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = siemens_isurf_read_reg;
	sc->writereg = siemens_isurf_write_reg;

	sc->readfifo = siemens_isurf_read_fifo;
	sc->writefifo = siemens_isurf_write_fifo;

	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_SIE_ISURF2;

	/* setup IOM bus type */

	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* setup chip type = IPAC ! */

	sc->sc_ipac = 1;
	sc->sc_bfifolen = IPAC_BFIFO_LEN;


	return (1);
}

/*---------------------------------------------------------------------------*
 *	isic_attach_siemens_isurf - attach for Siemens I-Surf 2.0 PnP
 *---------------------------------------------------------------------------*/
int
isic_attach_siemens_isurf(struct isa_device *dev, unsigned int iobase2)
{
	struct isic_softc *sc = &l1_sc[dev->id_unit];

	/* setup ISAC and HSCX base addr */

	ISAC_BASE   = (void *) ((u_int)sc->sc_port | SIE_ISURF_IDISAC);
	HSCX_A_BASE = (void *) ((u_int)sc->sc_port | SIE_ISURF_IDHSCXA);
	HSCX_B_BASE = (void *) ((u_int)sc->sc_port | SIE_ISURF_IDHSCXB);
	IPAC_BASE   = (void *) ((u_int)sc->sc_port | SIE_ISURF_IDIPAC);

	/* enable hscx/isac irq's */
	IPAC_WRITE(IPAC_MASK, (IPAC_MASK_INT1 | IPAC_MASK_INT0));

	IPAC_WRITE(IPAC_ACFG, 0);	/* outputs are open drain */
	IPAC_WRITE(IPAC_AOE,		/* aux 5..2 are inputs, 7, 6 outputs */
		(IPAC_AOE_OE5 | IPAC_AOE_OE4 | IPAC_AOE_OE3 | IPAC_AOE_OE2));
	IPAC_WRITE(IPAC_ATX, 0xff);	/* set all output lines high */

	return(1);
}

#else

void
isic_attach_siemens_isurf(struct isic_softc *sc)
{
	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = siemens_isurf_read_reg;
	sc->writereg = siemens_isurf_write_reg;

	sc->readfifo = siemens_isurf_read_fifo;
	sc->writefifo = siemens_isurf_write_fifo;

	/* setup card type */

	sc->sc_cardtyp = CARD_TYPEP_SIE_ISURF2;

	/* setup IOM bus type */

	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* setup chip type = IPAC ! */

	sc->sc_ipac = 1;
	sc->sc_bfifolen = IPAC_BFIFO_LEN;

	/* enable hscx/isac irq's */

	IPAC_WRITE(IPAC_MASK, (IPAC_MASK_INT1 | IPAC_MASK_INT0));

	IPAC_WRITE(IPAC_ACFG, 0);	/* outputs are open drain */
	IPAC_WRITE(IPAC_AOE,		/* aux 5..2 are inputs, 7, 6 outputs */
		(IPAC_AOE_OE5 | IPAC_AOE_OE4 | IPAC_AOE_OE3 | IPAC_AOE_OE2));
	IPAC_WRITE(IPAC_ATX, 0xff);	/* set all output lines high */
}

#endif
