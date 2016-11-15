/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann <martin@NetBSD.org>.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: isic_isa.c,v 1.37 2014/03/23 02:46:55 christos Exp $");

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#if defined(__NetBSD__) && __NetBSD_Version__ >= 104230000
#include <sys/callout.h>
#endif

#include <sys/cpu.h>
#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/isa/isavar.h>

#ifdef __FreeBSD__
#include <machine/i4b_ioctl.h>
#else
#include <netisdn/i4b_ioctl.h>
#endif

#include "opt_isicisa.h"

#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_trace.h>

#include <netisdn/i4b_l2.h>
#include <netisdn/i4b_l1l2.h>
#include <dev/ic/isic_l1.h>
#include <dev/ic/ipac.h>
#include <dev/ic/isac.h>
#include <dev/ic/hscx.h>

#include <netisdn/i4b_mbuf.h>
#include <netisdn/i4b_global.h>

extern const struct isdn_layer1_isdnif_driver isic_std_driver;

/* local functions */
static int isic_isa_probe(device_t, cfdata_t, void *);

static void isic_isa_attach(device_t, device_t, void *);
static int setup_io_map(int flags, bus_space_tag_t iot,
	bus_space_tag_t memt, bus_size_t iobase, bus_size_t maddr,
	int *num_mappings, struct isic_io_map *maps, int *iosize,
	int *msize);
static void args_unmap(int *num_mappings, struct isic_io_map *maps);

CFATTACH_DECL_NEW(isic_isa, sizeof(struct isic_softc),
    isic_isa_probe, isic_isa_attach, NULL, NULL);

#define	ISIC_FMT	"%s: "
#define	ISIC_PARM	device_xname(sc->sc_dev)
#define	TERMFMT	"\n"

/*
 * Probe card
 */
static int
isic_isa_probe(device_t parent, cfdata_t cf, void *aux)
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t memt = ia->ia_memt, iot = ia->ia_iot;
	int flags = cf->cf_flags;
	struct isic_attach_args args;
	int ret = 0, iobase, iosize, maddr, msize;

#if 0
	printf("isic%d: enter isic_isa_probe\n", cf->cf_unit);
#endif

	if (ia->ia_nio < 1)
		return (0);
	if (ia->ia_niomem < 1)
		return (0);
	if (ia->ia_nirq < 1)
		return (0);

	if (ISA_DIRECT_CONFIG(ia))
		return (0);

	/* check irq */
	if (ia->ia_irq[0].ir_irq == ISA_UNKNOWN_IRQ) {
		printf("isic%d: config error: no IRQ specified\n", cf->cf_unit);
		return 0;
	}

	iobase = ia->ia_io[0].ir_addr;
	iosize = ia->ia_io[0].ir_size;

	maddr = ia->ia_iomem[0].ir_addr;
	msize = ia->ia_iomem[0].ir_size;

	/* setup MI attach args */
	memset(&args, 0, sizeof(args));
	args.ia_flags = flags;

	/* if card type specified setup io map for that card */
	switch(flags)
	{
		case FLAG_TELES_S0_8:
		case FLAG_TELES_S0_16:
		case FLAG_TELES_S0_163:
		case FLAG_AVM_A1:
		case FLAG_USR_ISDN_TA_INT:
		case FLAG_ITK_IX1:
			if (setup_io_map(flags, iot, memt, iobase, maddr,
			    &args.ia_num_mappings, &args.ia_maps[0],
			    &iosize, &msize)) {
				ret = 0;
				goto done;
			}
			break;

		default:
			/* no io map now, will figure card type later */
			break;
	}

	/* probe card */
	switch(flags)
	{
#ifdef ISICISA_DYNALINK
#ifdef __bsdi__
		case FLAG_DYNALINK:
			ret = isic_probe_Dyn(&args);
			break;
#endif
#endif

#ifdef ISICISA_TEL_S0_8
		case FLAG_TELES_S0_8:
			ret = isic_probe_s08(&args);
			break;
#endif

#ifdef ISICISA_TEL_S0_16
		case FLAG_TELES_S0_16:
			ret = isic_probe_s016(&args);
			break;
#endif

#ifdef ISICISA_TEL_S0_16_3
		case FLAG_TELES_S0_163:
			ret = isic_probe_s0163(&args);
			break;
#endif

#ifdef ISICISA_AVM_A1
		case FLAG_AVM_A1:
			ret = isic_probe_avma1(&args);
			break;
#endif

#ifdef ISICISA_USR_STI
		case FLAG_USR_ISDN_TA_INT:
			ret = isic_probe_usrtai(&args);
			break;
#endif

#ifdef ISICISA_ITKIX1
		case FLAG_ITK_IX1:
			ret = isic_probe_itkix1(&args);
			break;
#endif

		default:
			/* No card type given, try to figure ... */
			if (iobase == ISA_UNKNOWN_PORT) {
				ret = 0;
#ifdef ISICISA_TEL_S0_8
				/* only Teles S0/8 will work without IO */
				args.ia_flags = FLAG_TELES_S0_8;
				if (setup_io_map(args.ia_flags, iot, memt,
				    iobase, maddr, &args.ia_num_mappings,
				    &args.ia_maps[0], &iosize, &msize) == 0)
				{
					ret = isic_probe_s08(&args);
				}
#endif /* ISICISA_TEL_S0_8 */
			} else if (maddr == ISA_UNKNOWN_IOMEM) {
				ret = 0;
#ifdef ISICISA_TEL_S0_16_3
				/* no shared memory, only a 16.3 based card,
				   AVM A1, the usr sportster or an ITK would work */
				args.ia_flags = FLAG_TELES_S0_163;
				if (setup_io_map(args.ia_flags, iot, memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&iosize, &msize) == 0)
				{
					ret = isic_probe_s0163(&args);
					if (ret)
						break;
				}
#endif /* ISICISA_TEL_S0_16_3 */
#ifdef	ISICISA_AVM_A1
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
				args.ia_flags = FLAG_AVM_A1;
				if (setup_io_map(args.ia_flags, iot, memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&iosize, &msize) == 0)
				{
					ret = isic_probe_avma1(&args);
					if (ret)
						break;
				}
#endif /* ISICISA_AVM_A1 */
#ifdef ISICISA_USR_STI
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
				args.ia_flags = FLAG_USR_ISDN_TA_INT;
				if (setup_io_map(args.ia_flags, iot, memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&iosize, &msize) == 0)
				{
					ret = isic_probe_usrtai(&args);
					if (ret)
						break;
				}
#endif /* ISICISA_USR_STI */

#ifdef ISICISA_ITKIX1
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
				args.ia_flags = FLAG_ITK_IX1;
				if (setup_io_map(args.ia_flags, iot, memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&iosize, &msize) == 0)
				{
					ret = isic_probe_itkix1(&args);
					if (ret)
						break;
				}
#endif /* ISICISA_ITKIX1 */

			} else {
#ifdef ISICISA_TEL_S0_16_3
				/* could be anything */
				args.ia_flags = FLAG_TELES_S0_163;
				if (setup_io_map(args.ia_flags, iot, memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&iosize, &msize) == 0)
				{
					ret = isic_probe_s0163(&args);
					if (ret)
						break;
				}
#endif /* ISICISA_TEL_S0_16_3 */
#ifdef ISICISA_TEL_S0_16
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
				args.ia_flags = FLAG_TELES_S0_16;
				if (setup_io_map(args.ia_flags, iot, memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&iosize, &msize) == 0)
				{
					ret = isic_probe_s016(&args);
					if (ret)
						break;
				}
#endif /* ISICISA_TEL_S0_16 */
#ifdef ISICISA_AVM_A1
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
				args.ia_flags = FLAG_AVM_A1;
				if (setup_io_map(args.ia_flags, iot, memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&iosize, &msize) == 0)
				{
					ret = isic_probe_avma1(&args);
					if (ret)
						break;
				}
#endif /* ISICISA_AVM_A1 */
#ifdef ISICISA_TEL_S0_8
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
				args.ia_flags = FLAG_TELES_S0_8;
				if (setup_io_map(args.ia_flags, iot, memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0],
					&iosize, &msize) == 0)
				{
					ret = isic_probe_s08(&args);
				}
#endif /* ISICISA_TEL_S0_8 */
			}
			break;
	}

done:
	/* unmap resources */
	args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);

#if 0
	printf("isic%d: exit isic_isa_probe, return = %d\n", cf->cf_unit, ret);
#endif

	if (ret) {
		if (iosize != 0) {
			ia->ia_nio = 1;
			ia->ia_io[0].ir_addr = iobase;
			ia->ia_io[0].ir_size = iosize;
		} else
			ia->ia_nio = 0;
		if (msize != 0) {
			ia->ia_niomem = 1;
			ia->ia_iomem[0].ir_addr = maddr;
			ia->ia_iomem[0].ir_size = msize;
		} else
			ia->ia_niomem = 0;
		ia->ia_nirq = 1;

		ia->ia_ndrq = 0;
	}

	return ret;
}

static int
isicattach(int flags, struct isic_softc *sc)
{
	int ret = 0;
	const char *drvid;

#ifdef __FreeBSD__

	struct isic_softc *sc = &l1_sc[dev->id_unit];
#define	PARM	dev
#define	PARM2	dev, iobase2
#define	FLAGS	dev->id_flags

#elif defined(__bsdi__)

	struct isic_softc *sc = device_private(self);
#define	PARM	parent, self, ia
#define	PARM2	parent, self, ia
#define	FLAGS	sc->sc_flags

#else

#define PARM	sc
#define PARM2	sc
#define	FLAGS	flags

#endif /* __FreeBSD__ */

  	static const char *ISACversion[] = {
  		"2085 Version A1/A2 or 2086/2186 Version 1.1",
		"2085 Version B1",
		"2085 Version B2",
		"2085 Version V2.3 (B3)",
		"Unknown Version"
	};

	static const char *HSCXversion[] = {
		"82525 Version A1",
		"Unknown (0x01)",
		"82525 Version A2",
		"Unknown (0x03)",
		"82525 Version A3",
		"82525 or 21525 Version 2.1",
		"Unknown Version"
	};

	/* card dependent setup */
	switch(FLAGS)
	{
#ifdef ISICISA_DYNALINK
#if defined(__bsdi__) || defined(__FreeBSD__)
		case FLAG_DYNALINK:
			ret = isic_attach_Dyn(PARM2);
			break;
#endif
#endif

#ifdef ISICISA_TEL_S0_8
		case FLAG_TELES_S0_8:
			ret = isic_attach_s08(PARM);
			break;
#endif

#ifdef ISICISA_TEL_S0_16
		case FLAG_TELES_S0_16:
			ret = isic_attach_s016(PARM);
			break;
#endif

#ifdef ISICISA_TEL_S0_16_3
		case FLAG_TELES_S0_163:
			ret = isic_attach_s0163(PARM);
			break;
#endif

#ifdef ISICISA_AVM_A1
		case FLAG_AVM_A1:
			ret = isic_attach_avma1(PARM);
			break;
#endif

#ifdef ISICISA_USR_STI
		case FLAG_USR_ISDN_TA_INT:
			ret = isic_attach_usrtai(PARM);
			break;
#endif

#ifdef ISICISA_ITKIX1
		case FLAG_ITK_IX1:
			ret = isic_attach_itkix1(PARM);
			break;
#endif

#ifdef ISICISA_ELSA_PCC16
		case FLAG_ELSA_PCC16:
			ret = isic_attach_Eqs1pi(dev, 0);
			break;
#endif

#ifdef amiga
		case FLAG_BLMASTER:
			ret = 1; /* full detection was done in caller */
			break;
#endif

/* ======================================================================
 * Only P&P cards follow below!!!
 */

#ifdef __FreeBSD__		/* we've already splitted all non-ISA stuff
				   out of this ISA specific part for the other
				   OS */

#ifdef AVM_A1_PCMCIA
		case FLAG_AVM_A1_PCMCIA:
                      ret = isic_attach_fritzpcmcia(PARM);
			break;
#endif

#ifdef TEL_S0_16_3_P
		case FLAG_TELES_S0_163_PnP:
			ret = isic_attach_s0163P(PARM2);
			break;
#endif

#ifdef CRTX_S0_P
		case FLAG_CREATIX_S0_PnP:
			ret = isic_attach_Cs0P(PARM2);
			break;
#endif

#ifdef DRN_NGO
		case FLAG_DRN_NGO:
			ret = isic_attach_drnngo(PARM2);
			break;
#endif

#ifdef SEDLBAUER
		case FLAG_SWS:
			ret = isic_attach_sws(PARM);
			break;
#endif

#ifdef ELSA_QS1ISA
		case FLAG_ELSA_QS1P_ISA:
			ret = isic_attach_Eqs1pi(PARM2);
			break;
#endif

#ifdef AVM_PNP
		case FLAG_AVM_PNP:
			ret = isic_attach_avm_pnp(PARM2);
			ret = 0;
			break;
#endif

#ifdef SIEMENS_ISURF2
		case FLAG_SIEMENS_ISURF2:
			ret = isic_attach_siemens_isurf(PARM2);
			break;
#endif

#ifdef ASUSCOM_IPAC
		case FLAG_ASUSCOM_IPAC:
			ret = isic_attach_asi(PARM2);
			break;
#endif

#endif /* __FreeBSD__ / P&P specific part */

		default:
			break;
	}

	if(ret == 0)
		return(0);

	if(sc->sc_ipac)
	{
		sc->sc_ipac_version = IPAC_READ(IPAC_ID);

		switch(sc->sc_ipac_version)
		{
			case IPAC_V11:
			case IPAC_V12:
				break;

			default:
				aprint_error_dev(sc->sc_dev, "Error, IPAC version %d unknown!\n", ret);
				return(0);
				break;
		}
	}
	else
	{
		sc->sc_isac_version = ((ISAC_READ(I_RBCH)) >> 5) & 0x03;

		switch(sc->sc_isac_version)
		{
			case ISAC_VA:
			case ISAC_VB1:
			case ISAC_VB2:
			case ISAC_VB3:
				break;

			default:
				printf(ISIC_FMT "Error, ISAC version %d unknown!\n",
				ISIC_PARM, sc->sc_isac_version);
				return(0);
				break;
		}

		sc->sc_hscx_version = HSCX_READ(0, H_VSTR) & 0xf;

		switch(sc->sc_hscx_version)
		{
			case HSCX_VA1:
			case HSCX_VA2:
			case HSCX_VA3:
			case HSCX_V21:
				break;

			default:
				printf(ISIC_FMT "Error, HSCX version %d unknown!\n",
					ISIC_PARM, sc->sc_hscx_version);
				return(0);
				break;
		}
	}

        sc->sc_intr_valid = ISIC_INTR_DISABLED;

	/* HSCX setup */

	isic_bchannel_setup(sc, HSCX_CH_A, BPROT_NONE, 0);

	isic_bchannel_setup(sc, HSCX_CH_B, BPROT_NONE, 0);

	/* setup linktab */

	isic_init_linktab(sc);

	/* set trace level */

	sc->sc_trace = TRACE_OFF;

	sc->sc_state = ISAC_IDLE;

	sc->sc_ibuf = NULL;
	sc->sc_ib = NULL;
	sc->sc_ilen = 0;

	sc->sc_obuf = NULL;
	sc->sc_op = NULL;
	sc->sc_ol = 0;
	sc->sc_freeflag = 0;

	sc->sc_obuf2 = NULL;
	sc->sc_freeflag2 = 0;

#if defined(__FreeBSD__) && __FreeBSD__ >=3
	callout_handle_init(&sc->sc_T3_callout);
	callout_handle_init(&sc->sc_T4_callout);
#endif

#if defined(__NetBSD__) && __NetBSD_Version__ >= 104230000
	callout_init(&sc->sc_T3_callout, 0);
	callout_init(&sc->sc_T4_callout, 0);
#endif

	/* announce manufacturer and card type */

	switch(FLAGS)
	{
		case FLAG_TELES_S0_8:
			drvid = "Teles S0/8 or Niccy 1008";
			break;

		case FLAG_TELES_S0_16:
			drvid = "Teles S0/16, Creatix ISDN S0-16 or Niccy 1016";
			break;

		case FLAG_TELES_S0_163:
			drvid = "Teles S0/16.3";
			break;

		case FLAG_AVM_A1:
			drvid = "AVM A1 or AVM Fritz!Card";
			break;

		case FLAG_AVM_A1_PCMCIA:
			drvid = "AVM PCMCIA Fritz!Card";
			break;

		case FLAG_TELES_S0_163_PnP:
			drvid = "Teles S0/PnP";
			break;

		case FLAG_CREATIX_S0_PnP:
			drvid = "Creatix ISDN S0-16 P&P";
			break;

		case FLAG_USR_ISDN_TA_INT:
			drvid = "USRobotics Sportster ISDN TA intern";
			break;

		case FLAG_DRN_NGO:
			drvid = "Dr. Neuhaus NICCY Go@";
			break;

		case FLAG_DYNALINK:
			drvid = "Dynalink IS64PH";
			break;

		case FLAG_SWS:
			drvid = "Sedlbauer WinSpeed";
			break;

		case FLAG_BLMASTER:
			/* board announcement was done by caller */
			drvid = (char *)0;
			break;

		case FLAG_ELSA_QS1P_ISA:
			drvid = "ELSA QuickStep 1000pro (ISA)";
			break;

		case FLAG_ITK_IX1:
			drvid = "ITK ix1 micro";
			break;

		case FLAG_ELSA_PCC16:
			drvid = "ELSA PCC-16";
			break;

		case FLAG_ASUSCOM_IPAC:
			drvid = "Asuscom ISDNlink 128K PnP";
			break;

		case FLAG_SIEMENS_ISURF2:
			drvid = "Siemens I-Surf 2.0";
			break;

		default:
			drvid = "ERROR, unknown flag used";
			break;
	}
#ifndef __FreeBSD__
	printf("\n");
#endif
	if (drvid)
		printf(ISIC_FMT "%s\n", ISIC_PARM, drvid);

	/* announce chip versions */

	if(sc->sc_ipac)
	{
		if(sc->sc_ipac_version == IPAC_V11)
			printf(ISIC_FMT "IPAC PSB2115 Version 1.1\n", ISIC_PARM);
		else
			printf(ISIC_FMT "IPAC PSB2115 Version 1.2\n", ISIC_PARM);
	}
	else
	{
		if(sc->sc_isac_version >= ISAC_UNKN)
		{
			printf(ISIC_FMT "ISAC Version UNKNOWN (VN=0x%x)" TERMFMT,
					ISIC_PARM,
					sc->sc_isac_version);
			sc->sc_isac_version = ISAC_UNKN;
		}
		else
		{
			printf(ISIC_FMT "ISAC %s (IOM-%c)" TERMFMT,
					ISIC_PARM,
					ISACversion[sc->sc_isac_version],
					sc->sc_bustyp == BUS_TYPE_IOM1 ? '1' : '2');
		}

#ifdef __FreeBSD__
		printf("(Addr=0x%lx)\n", (u_long)ISAC_BASE);
#endif

		if(sc->sc_hscx_version >= HSCX_UNKN)
		{
			printf(ISIC_FMT "HSCX Version UNKNOWN (VN=0x%x)" TERMFMT,
					ISIC_PARM,
					sc->sc_hscx_version);
			sc->sc_hscx_version = HSCX_UNKN;
		}
		else
		{
			printf(ISIC_FMT "HSCX %s" TERMFMT,
					ISIC_PARM,
					HSCXversion[sc->sc_hscx_version]);
		}

#ifdef __FreeBSD__
		printf("(AddrA=0x%lx, AddrB=0x%lx)\n", (u_long)HSCX_A_BASE, (u_long)HSCX_B_BASE);

#endif /* __FreeBSD__ */
	}

#ifdef __FreeBSD__
	next_isic_unit++;

#if defined(__FreeBSD_version) && __FreeBSD_version >= 300003

	/* set the interrupt handler - no need to change isa_device.h */
	dev->id_intr = (inthand2_t *)isicintr;

#endif

#endif /* __FreeBSD__ */

	/* init higher protocol layers */
	isic_attach_bri(sc, drvid, &isic_std_driver);

	return(1);
#undef PARM
#undef FLAGS
}

/*
 * Attach the card
 */
static void
isic_isa_attach(device_t parent, device_t self, void *aux)
{
	struct isic_softc *sc = device_private(self);
	struct isa_attach_args *ia = aux;
	int flags = device_cfdata(self)->cf_flags;
	int ret = 0, iobase, maddr;
	struct isic_attach_args args;

	iobase = ia->ia_nio > 0 ? ia->ia_io[0].ir_addr : ISA_UNKNOWN_PORT;
	maddr = ia->ia_niomem > 0 ? ia->ia_iomem[0].ir_addr : ISA_UNKNOWN_IOMEM;

	/* Setup parameters */
	sc->sc_dev = self;
	sc->sc_irq = ia->ia_irq[0].ir_irq;
	sc->sc_maddr = maddr;
	sc->sc_num_mappings = 0;
	sc->sc_maps = NULL;
	switch(flags)
	{
		case FLAG_TELES_S0_8:
		case FLAG_TELES_S0_16:
		case FLAG_TELES_S0_163:
		case FLAG_AVM_A1:
		case FLAG_USR_ISDN_TA_INT:
			setup_io_map(flags, ia->ia_iot, ia->ia_memt, iobase, maddr,
					&(sc->sc_num_mappings), NULL, NULL, NULL);
			MALLOC_MAPS(sc);
			setup_io_map(flags, ia->ia_iot, ia->ia_memt, iobase, maddr,
					&(sc->sc_num_mappings), &(sc->sc_maps[0]), NULL, NULL);
			break;

		default:
			/* No card type given, try to figure ... */

			/* setup MI attach args */
			memset(&args, 0, sizeof(args));
			args.ia_flags = flags;

			/* Probe cards */
			if (iobase == ISA_UNKNOWN_PORT) {
				ret = 0;
#ifdef ISICISA_TEL_S0_8
				/* only Teles S0/8 will work without IO */
				args.ia_flags = FLAG_TELES_S0_8;
				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
				ret = isic_probe_s08(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* ISICISA_TEL_S0_8 */
			} else if (maddr == ISA_UNKNOWN_IOMEM) {
				/* no shared memory, only a 16.3 based card,
				   AVM A1, the usr sportster or an ITK would work */
				ret = 0;
#ifdef	ISICISA_TEL_S0_16_3
				args.ia_flags = FLAG_TELES_S0_163;
				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
				ret = isic_probe_s0163(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* ISICISA_TEL_S0_16_3 */
#ifdef ISICISA_AVM_A1
				args.ia_flags = FLAG_AVM_A1;
				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
				ret = isic_probe_avma1(&args);
 				if (ret)
 					goto found;
 				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* ISICISA_AVM_A1 */
#ifdef ISICISA_USR_STI
 				args.ia_flags = FLAG_USR_ISDN_TA_INT;
 				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, iobase, maddr,
 					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
 				ret = isic_probe_usrtai(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* ISICISA_USR_STI */
#ifdef ISICISA_ITKIX1
 				args.ia_flags = FLAG_ITK_IX1;
 				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, iobase, maddr,
 					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
 				ret = isic_probe_itkix1(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* ISICISA_ITKIX1 */
			} else {
				/* could be anything */
				ret = 0;
#ifdef	ISICISA_TEL_S0_16_3
				args.ia_flags = FLAG_TELES_S0_163;
				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
				ret = isic_probe_s0163(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif	/* ISICISA_TEL_S0_16_3 */
#ifdef	ISICISA_TEL_S0_16
				args.ia_flags = FLAG_TELES_S0_16;
				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
				ret = isic_probe_s016(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* ISICISA_TEL_S0_16 */
#ifdef ISICISA_AVM_A1
				args.ia_flags = FLAG_AVM_A1;
				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
				ret = isic_probe_avma1(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* ISICISA_AVM_A1 */
#ifdef ISICISA_TEL_S0_8
				args.ia_flags = FLAG_TELES_S0_8;
				setup_io_map(args.ia_flags, ia->ia_iot, ia->ia_memt, iobase, maddr,
					&args.ia_num_mappings, &args.ia_maps[0], NULL, NULL);
				ret = isic_probe_s08(&args);
				if (ret)
					goto found;
				args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
#endif /* ISICISA_TEL_S0_8 */
			}
			break;

		found:
			flags = args.ia_flags;
			sc->sc_num_mappings = args.ia_num_mappings;
			args_unmap(&args.ia_num_mappings, &args.ia_maps[0]);
			if (ret) {
				MALLOC_MAPS(sc);
				setup_io_map(flags, ia->ia_iot, ia->ia_memt, iobase, maddr,
					&(sc->sc_num_mappings), &(sc->sc_maps[0]), NULL, NULL);
			} else {
				printf(": could not determine card type - not configured!\n");
				return;
			}
			break;
	}

	/* MI initialization of card */
	isicattach(flags, sc);

	/*
	 * Try to get a level-triggered interrupt first. If that doesn't
	 * work (like on NetBSD/Atari, try to establish an edge triggered
	 * interrupt.
	 */
	if (isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq, IST_LEVEL,
				IPL_NET, isicintr, sc) == NULL) {
		if(isa_intr_establish(ia->ia_ic, ia->ia_irq[0].ir_irq, IST_EDGE,
				IPL_NET, isicintr, sc) == NULL) {
			args_unmap(&(sc->sc_num_mappings), &(sc->sc_maps[0]));
			free((sc)->sc_maps, M_DEVBUF);
		}
		else {
			/*
			 * XXX: This is a hack that probably needs to be
			 * solved by setting an interrupt type in the sc
			 * structure. I don't feel familiar enough with the
			 * code to do this currently. Feel free to contact
			 * me about it (leo@NetBSD.org).
			 */
			isicintr(sc);
		}
	}
}

/*
 * Setup card specific io mapping. Return 0 on success,
 * any other value on config error.
 * Be prepared to get NULL as maps array.
 * Make sure to keep *num_mappings in sync with the real
 * mappings already setup when returning!
 */
static int
setup_io_map(int flags, bus_space_tag_t iot, bus_space_tag_t memt, bus_size_t iobase, bus_size_t maddr, int *num_mappings, struct isic_io_map *maps, int *iosize, int *msize)
{
	/* nothing mapped yet */
	*num_mappings = 0;

	/* which resources do we need? */
	switch(flags)
	{
		case FLAG_TELES_S0_8:
			if (maddr == ISA_UNKNOWN_IOMEM) {
				printf("isic: config error: no shared memory specified for Teles S0/8!\n");
				return 1;
			}
			if (iosize) *iosize = 0;	/* no i/o ports */
			if (msize) *msize = 0x1000;	/* shared memory size */

			/* this card uses a single memory mapping */
			if (maps == NULL) {
				*num_mappings = 1;
				return 0;
			}
			*num_mappings = 0;
			maps[0].t = memt;
			maps[0].offset = 0;
			maps[0].size = 0x1000;
			if (bus_space_map(maps[0].t, maddr,
				maps[0].size, 0, &maps[0].h)) {
				return 1;
			}
			(*num_mappings)++;
			break;

		case FLAG_TELES_S0_16:
			if (iobase == ISA_UNKNOWN_PORT) {
				printf("isic: config error: no i/o address specified for Teles S0/16!\n");
				return 1;
			}
			if (maddr == ISA_UNKNOWN_IOMEM) {
				printf("isic: config error: no shared memory specified for Teles S0/16!\n");
				return 1;
			}
			if (iosize) *iosize = 8;	/* i/o ports */
			if (msize) *msize = 0x1000;	/* shared memory size */

			/* one io and one memory mapping */
			if (maps == NULL) {
				*num_mappings = 2;
				return 0;
			}
			*num_mappings = 0;
			maps[0].t = iot;
			maps[0].offset = 0;
			maps[0].size = 8;
			if (bus_space_map(maps[0].t, iobase,
				maps[0].size, 0, &maps[0].h)) {
				return 1;
			}
			(*num_mappings)++;
			maps[1].t = memt;
			maps[1].offset = 0;
			maps[1].size = 0x1000;
			if (bus_space_map(maps[1].t, maddr,
				maps[1].size, 0, &maps[1].h)) {
				return 1;
			}
			(*num_mappings)++;
			break;

		case FLAG_TELES_S0_163:
			if (iobase == ISA_UNKNOWN_PORT) {
				printf("isic: config error: no i/o address specified for Teles S0/16!\n");
				return 1;
			}
			if (iosize) *iosize = 8;	/* only some i/o ports shown */
			if (msize) *msize = 0;		/* no shared memory */

			/* Four io mappings: config, isac, 2 * hscx */
			if (maps == NULL) {
				*num_mappings = 4;
				return 0;
			}
			*num_mappings = 0;
			maps[0].t = iot;
			maps[0].offset = 0;
			maps[0].size = 8;
			if (bus_space_map(maps[0].t, iobase,
				maps[0].size, 0, &maps[0].h)) {
				return 1;
			}
			(*num_mappings)++;
			maps[1].t = iot;
			maps[1].offset = 0;
			maps[1].size = 0x40;	/* XXX - ??? */
			if ((iobase - 0xd80 + 0x980) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[1].t, iobase - 0xd80 + 0x980,
				maps[1].size, 0, &maps[1].h)) {
				return 1;
			}
			(*num_mappings)++;
			maps[2].t = iot;
			maps[2].offset = 0;
			maps[2].size = 0x40;	/* XXX - ??? */
			if ((iobase - 0xd80 + 0x180) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[2].t, iobase - 0xd80 + 0x180,
				maps[2].size, 0, &maps[2].h)) {
				return 1;
			}
			(*num_mappings)++;
			maps[3].t = iot;
			maps[3].offset = 0;
			maps[3].size = 0x40;	/* XXX - ??? */
			if ((iobase - 0xd80 + 0x580) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[3].t, iobase - 0xd80 + 0x580,
				maps[3].size, 0, &maps[3].h)) {
				return 1;
			}
			(*num_mappings)++;
			break;

		case FLAG_AVM_A1:
			if (iobase == ISA_UNKNOWN_PORT) {
				printf("isic: config error: no i/o address specified for AVM A1/Fritz! card!\n");
				return 1;
			}
			if (iosize) *iosize = 8;	/* only some i/o ports shown */
			if (msize) *msize = 0;		/* no shared memory */

			/* Seven io mappings: config, isac, 2 * hscx,
			   isac-fifo, 2 * hscx-fifo */
			if (maps == NULL) {
				*num_mappings = 7;
				return 0;
			}
			*num_mappings = 0;
			maps[0].t = iot;	/* config */
			maps[0].offset = 0;
			maps[0].size = 8;
			if ((iobase + 0x1800) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[0].t, iobase + 0x1800, maps[0].size, 0, &maps[0].h))
				return 1;
			(*num_mappings)++;
			maps[1].t = iot;	/* isac */
			maps[1].offset = 0;
			maps[1].size = 0x80;	/* XXX - ??? */
			if ((iobase + 0x1400 - 0x20) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[1].t, iobase + 0x1400 - 0x20, maps[1].size, 0, &maps[1].h))
				return 1;
			(*num_mappings)++;
			maps[2].t = iot;	/* hscx 0 */
			maps[2].offset = 0;
			maps[2].size = 0x40;	/* XXX - ??? */
			if ((iobase + 0x400 - 0x20) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[2].t, iobase + 0x400 - 0x20, maps[2].size, 0, &maps[2].h))
				return 1;
			(*num_mappings)++;
			maps[3].t = iot;	/* hscx 1 */
			maps[3].offset = 0;
			maps[3].size = 0x40;	/* XXX - ??? */
			if ((iobase + 0xc00 - 0x20) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[3].t, iobase + 0xc00 - 0x20, maps[3].size, 0, &maps[3].h))
				return 1;
			(*num_mappings)++;
			maps[4].t = iot;	/* isac-fifo */
			maps[4].offset = 0;
			maps[4].size = 1;
			if ((iobase + 0x1400 - 0x20 -0x3e0) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[4].t, iobase + 0x1400 - 0x20 -0x3e0, maps[4].size, 0, &maps[4].h))
				return 1;
			(*num_mappings)++;
			maps[5].t = iot;	/* hscx 0 fifo */
			maps[5].offset = 0;
			maps[5].size = 1;
			if ((iobase + 0x400 - 0x20 -0x3e0) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[5].t, iobase + 0x400 - 0x20 -0x3e0, maps[5].size, 0, &maps[5].h))
				return 1;
			(*num_mappings)++;
			maps[6].t = iot;	/* hscx 1 fifo */
			maps[6].offset = 0;
			maps[6].size = 1;
			if ((iobase + 0xc00 - 0x20 -0x3e0) > 0x0ffff)
				return 1;
			if (bus_space_map(maps[6].t, iobase + 0xc00 - 0x20 -0x3e0, maps[6].size, 0, &maps[6].h))
				return 1;
			(*num_mappings)++;
			break;

		case FLAG_USR_ISDN_TA_INT:
			if (iobase == ISA_UNKNOWN_PORT) {
				printf("isic: config error: no I/O base specified for USR Sportster TA intern!\n");
				return 1;
			}
			if (iosize) *iosize = 8;	/* scattered ports, only some shown */
			if (msize) *msize = 0;		/* no shared memory */

			/* 49 io mappings: 1 config and 48x8 registers */
			if (maps == NULL) {
				*num_mappings = 49;
				return 0;
			}
			*num_mappings = 0;
			{
				int i, num;
				bus_size_t base;

				/* config at offset 0x8000 */
				base = iobase + 0x8000;
				maps[0].size = 1;
				maps[0].t = iot;
				maps[0].offset = 0;
				if (base > 0x0ffff)
					return 1;
				if (bus_space_map(iot, base, 1, 0, &maps[0].h)) {
					return 1;
				}
				*num_mappings = num = 1;

				/* HSCX A at offset 0 */
				base = iobase;
				for (i = 0; i < 16; i++) {
					maps[num].size = 8;
					maps[num].offset = 0;
					maps[num].t = iot;
					if (base+i*1024+8 > 0x0ffff)
						return 1;
					if (bus_space_map(iot, base+i*1024, 8, 0, &maps[num].h)) {
						return 1;
					}
					*num_mappings = ++num;
				}
				/* HSCX B at offset 0x4000 */
				base = iobase + 0x4000;
				for (i = 0; i < 16; i++) {
					maps[num].size = 8;
					maps[num].offset = 0;
					maps[num].t = iot;
					if (base+i*1024+8 > 0x0ffff)
						return 1;
					if (bus_space_map(iot, base+i*1024, 8, 0, &maps[num].h)) {
						return 1;
					}
					*num_mappings = ++num;
				}
				/* ISAC at offset 0xc000 */
				base = iobase + 0xc000;
				for (i = 0; i < 16; i++) {
					maps[num].size = 8;
					maps[num].offset = 0;
					maps[num].t = iot;
					if (base+i*1024+8 > 0x0ffff)
						return 1;
					if (bus_space_map(iot, base+i*1024, 8, 0, &maps[num].h)) {
						return 1;
					}
					*num_mappings = ++num;
				}
			}
			break;

		case FLAG_ITK_IX1:
			if (iobase == ISA_UNKNOWN_PORT) {
				printf("isic: config error: no I/O base specified for ITK ix1 micro!\n");
				return 1;
			}
			if (iosize) *iosize = 4;
			if (msize) *msize = 0;
			if (maps == NULL) {
				*num_mappings = 1;
				return 0;
			}
			*num_mappings = 0;
			maps[0].size = 4;
			maps[0].t = iot;
			maps[0].offset = 0;
			if (bus_space_map(iot, iobase, 4, 0, &maps[0].h)) {
				return 1;
			}
			*num_mappings = 1;
  			break;

		default:
			printf("isic: config error: flags do not specify any known card!\n");
			return 1;
			break;
	}

	return 0;
}

static void
args_unmap(int *num_mappings, struct isic_io_map *maps)
{
	int i, n;
	for (i = 0, n = *num_mappings; i < n; i++)
        	if (maps[i].size)
			bus_space_unmap(maps[i].t, maps[i].h, maps[i].size);
	*num_mappings = 0;
}
