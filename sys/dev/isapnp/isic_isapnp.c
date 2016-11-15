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
__KERNEL_RCSID(0, "$NetBSD: isic_isapnp.c,v 1.32 2012/10/27 17:18:26 chs Exp $");

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
#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>

#ifdef __FreeBSD__
#include <machine/i4b_ioctl.h>
#include <machine/i4b_trace.h>
#else
#include <netisdn/i4b_debug.h>
#include <netisdn/i4b_ioctl.h>
#include <netisdn/i4b_trace.h>
#include <netisdn/i4b_l2.h>
#endif

#include <dev/ic/isic_l1.h>
#include <dev/ic/ipac.h>
#include <dev/ic/isac.h>
#include <dev/ic/hscx.h>

#include <netisdn/i4b_l1l2.h>
#include <netisdn/i4b_global.h>

#include "opt_isicpnp.h"

extern const struct isdn_layer1_isdnif_driver isic_std_driver;

static int isic_isapnp_probe(device_t, cfdata_t, void *);
static void isic_isapnp_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(isic_isapnp, sizeof(struct isic_softc),
    isic_isapnp_probe, isic_isapnp_attach, NULL, NULL);

typedef void (*allocmaps_func)(struct isapnp_attach_args *ipa, struct isic_softc *sc);
typedef void (*attach_func)(struct isic_softc *sc);

/* map allocators */
#if defined(ISICPNP_ELSA_QS1ISA) || defined(ISICPNP_SEDLBAUER) \
	|| defined(ISICPNP_DYNALINK) || defined(ISICPNP_SIEMENS_ISURF2)	\
	|| defined(ISICPNP_ITKIX)
static void generic_pnp_mapalloc(struct isapnp_attach_args *ipa, struct isic_softc *sc);
#endif
#ifdef ISICPNP_DRN_NGO
static void ngo_pnp_mapalloc(struct isapnp_attach_args *ipa, struct isic_softc *sc);
#endif
#if defined(ISICPNP_CRTX_S0_P) || defined(ISICPNP_TEL_S0_16_3_P)
static void tls_pnp_mapalloc(struct isapnp_attach_args *ipa, struct isic_softc *sc);
#endif

/* card attach functions */
extern void isic_attach_Cs0P(struct isic_softc *sc);
extern void isic_attach_Dyn(struct isic_softc *sc);
extern void isic_attach_s0163P(struct isic_softc *sc);
extern void isic_attach_drnngo(struct isic_softc *sc);
extern void isic_attach_sws(struct isic_softc *sc);
extern void isic_attach_Eqs1pi(struct isic_softc *sc);
extern void isic_attach_siemens_isurf(struct isic_softc *sc);
extern void isic_attach_isapnp_itkix1(struct isic_softc *sc);

struct isic_isapnp_card_desc {
	const char *devlogic;		/* ISAPNP logical device ID */
	const char *name;		/* Name of the card */
	int card_type;			/* isic card type identifier */
	allocmaps_func allocmaps;	/* map allocator function */
	attach_func attach;		/* card attach and init function */
};
static const struct isic_isapnp_card_desc
isic_isapnp_descriptions[] =
{
#ifdef ISICPNP_CRTX_S0_P
	{ "CTX0000", "Creatix ISDN S0-16 P&P", CARD_TYPEP_CS0P,
	  tls_pnp_mapalloc, isic_attach_Cs0P },
#endif
#ifdef ISICPNP_TEL_S0_16_3_P
	{ "TAG2110", "Teles S0/PnP", CARD_TYPEP_163P,
	  tls_pnp_mapalloc, isic_attach_s0163P },
#endif
#ifdef ISICPNP_DRN_NGO
	{ "SDA0150", "Dr. Neuhaus NICCY GO@", CARD_TYPEP_DRNNGO,
	  ngo_pnp_mapalloc, isic_attach_drnngo },
#endif
#ifdef ISICPNP_ELSA_QS1ISA
	{ "ELS0133", "Elsa QuickStep 1000 (ISA)", CARD_TYPEP_ELSAQS1ISA,
	  generic_pnp_mapalloc, isic_attach_Eqs1pi },
#endif
#ifdef ISICPNP_SEDLBAUER
	{ "SAG0001", "Sedlbauer WinSpeed", CARD_TYPEP_SWS,
	  generic_pnp_mapalloc, isic_attach_sws },
#endif
#ifdef ISICPNP_DYNALINK
	{ "ASU1688", "Dynalink IS64PH", CARD_TYPEP_DYNALINK,
	  generic_pnp_mapalloc, isic_attach_Dyn },
#endif
#ifdef ISICPNP_SIEMENS_ISURF2
	{ "SIE0020", "Siemens I-Surf 2.0 PnP", CARD_TYPEP_SIE_ISURF2,
	  generic_pnp_mapalloc, isic_attach_siemens_isurf },
#endif
#ifdef ISICPNP_ITKIX
	{ "ITK0025", "ix1-micro 3.0", 0,
	  generic_pnp_mapalloc, isic_attach_isapnp_itkix1 },
#endif
};
#define	NUM_DESCRIPTIONS	(sizeof(isic_isapnp_descriptions)/sizeof(isic_isapnp_descriptions[0]))

/*
 * Probe card
 */
static int
isic_isapnp_probe(device_t parent,
	cfdata_t cf, void *aux)
{
	struct isapnp_attach_args *ipa = aux;
	const struct isic_isapnp_card_desc *desc = isic_isapnp_descriptions;
	int i;

	for (i = 0; i < NUM_DESCRIPTIONS; i++, desc++)
		if (strcmp(ipa->ipa_devlogic, desc->devlogic) == 0)
	return 1;

	return 0;
}

/*---------------------------------------------------------------------------*
 *	card independend attach for ISA P&P cards
 *---------------------------------------------------------------------------*/

/* parameter and format for message producing e.g. "isic0: " */

#ifdef __FreeBSD__
#define	ISIC_FMT	"isic%d: "
#define	ISIC_PARM	dev->id_unit
#define	TERMFMT	" "
#else
#define	ISIC_FMT	"%s: "
#define	ISIC_PARM	device_xname(sc->sc_dev)
#define	TERMFMT	"\n"
#endif

static void
isic_isapnp_attach(device_t parent, device_t self, void *aux)
{
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

	struct isic_softc *sc = device_private(self);
	struct isapnp_attach_args *ipa = aux;
	const struct isic_isapnp_card_desc *desc = isic_isapnp_descriptions;
	int i;

	sc->sc_dev = self;
	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		aprint_error_dev(sc->sc_dev, "error in region allocation\n");
		return;
	}

	for (i = 0; i < NUM_DESCRIPTIONS; i++, desc++)
		if (strcmp(ipa->ipa_devlogic, desc->devlogic) == 0)
			break;
	if (i >= NUM_DESCRIPTIONS)
		panic("could not identify isic PnP device");

	/* setup parameters */
	sc->sc_cardtyp = desc->card_type;
	sc->sc_irq = ipa->ipa_irq[0].num;
	desc->allocmaps(ipa, sc);

	/* announce card name */
	printf(": %s\n", desc->name);

	/* establish interrupt handler */
	if (isa_intr_establish(ipa->ipa_ic, ipa->ipa_irq[0].num, ipa->ipa_irq[0].type,
		IPL_NET, isicintr, sc) == NULL)
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt handler\n");

	/* init card */
	desc->attach(sc);

	/* announce chip versions */
	sc->sc_isac_version = 0;
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
			return;
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
			return;
			break;
	};

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

#if defined(__NetBSD__) && __NetBSD_Version__ >= 104230000
	callout_init(&sc->sc_T3_callout, 0);
	callout_init(&sc->sc_T4_callout, 0);
#endif

	/* announce chip versions */

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

	/* init higher protocol layers and save l2 handle */
	isic_attach_bri(sc, desc->name, &isic_std_driver);
}

#if defined(ISICPNP_ELSA_QS1ISA) || defined(ISICPNP_SEDLBAUER) \
	|| defined(ISICPNP_DYNALINK) || defined(ISICPNP_SIEMENS_ISURF2)	\
	|| defined(ISICPNP_ITKIX)
static void
generic_pnp_mapalloc(struct isapnp_attach_args *ipa, struct isic_softc *sc)
{
	sc->sc_num_mappings = 1;	/* most cards have just one mapping */
	MALLOC_MAPS(sc);		/* malloc the maps */
	sc->sc_maps[0].t = ipa->ipa_iot;	/* copy the access handles */
	sc->sc_maps[0].h = ipa->ipa_io[0].h;
	sc->sc_maps[0].size = 0;	/* foreign mapping, leave it alone */
}
#endif

#ifdef ISICPNP_DRN_NGO
static void
ngo_pnp_mapalloc(struct isapnp_attach_args *ipa, struct isic_softc *sc)
{
	sc->sc_num_mappings = 2;	/* one data, one address mapping */
	MALLOC_MAPS(sc);		/* malloc the maps */
	sc->sc_maps[0].t = ipa->ipa_iot;	/* copy the access handles */
	sc->sc_maps[0].h = ipa->ipa_io[0].h;
	sc->sc_maps[0].size = 0;	/* foreign mapping, leave it alone */
		sc->sc_maps[1].t = ipa->ipa_iot;
		sc->sc_maps[1].h = ipa->ipa_io[1].h;
		sc->sc_maps[1].size = 0;
}
#endif

#if defined(ISICPNP_CRTX_S0_P) || defined(ISICPNP_TEL_S0_16_3_P)
static void
tls_pnp_mapalloc(struct isapnp_attach_args *ipa, struct isic_softc *sc)
{
	sc->sc_num_mappings = 4;	/* config, isac, 2 * hscx */
	MALLOC_MAPS(sc);		/* malloc the maps */
	sc->sc_maps[0].t = ipa->ipa_iot;	/* copy the access handles */
	sc->sc_maps[0].h = ipa->ipa_io[0].h;
	sc->sc_maps[0].size = 0;	/* foreign mapping, leave it alone */
		sc->sc_maps[1].t = ipa->ipa_iot;
		sc->sc_maps[1].h = ipa->ipa_io[0].h;
		sc->sc_maps[1].size = 0;
		sc->sc_maps[1].offset = - 0x20;
		sc->sc_maps[2].t = ipa->ipa_iot;
		sc->sc_maps[2].offset = - 0x20;
		sc->sc_maps[2].h = ipa->ipa_io[1].h;
		sc->sc_maps[2].size = 0;
		sc->sc_maps[3].t = ipa->ipa_iot;
		sc->sc_maps[3].offset = 0;
		sc->sc_maps[3].h = ipa->ipa_io[1].h;
		sc->sc_maps[3].size = 0;
}
#endif
