/*	$NetBSD: pcmcia_cis_quirks.c,v 1.35 2013/09/14 13:13:33 joerg Exp $	*/

/*
 * Copyright (c) 1998 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pcmcia_cis_quirks.c,v 1.35 2013/09/14 13:13:33 joerg Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>

#include <dev/pcmcia/pcmciadevs.h>
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciachip.h>
#include <dev/pcmcia/pcmciavar.h>

/* There are cards out there whose CIS flat-out lies.  This file
   contains struct pcmcia_function chains for those devices. */

/* these structures are just static templates which are then copied
   into "live" allocated structures */

static const struct pcmcia_function pcmcia_3cxem556_func0 = {
	.number = 0,				/* function number */
	.function = PCMCIA_FUNCTION_NETWORK,
	.last_config_index = 0x07,		/* last cfe number */
	.ccr_base = 0x800,			/* ccr_base */
	.ccr_mask = 0x63,			/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_3cxem556_func0_cfe0 = {
	.number = 0x07,			/* cfe number */
	.flags = PCMCIA_CFE_IO8 | PCMCIA_CFE_IO16 | PCMCIA_CFE_IRQLEVEL,
	.iftype = PCMCIA_IFTYPE_IO,
	.num_iospace = 1,		/* num_iospace */
	.iomask = 4,			/* iomask */
	.iospace = { { .length = 0x0010, .start = 0 } },	/* iospace */
	.irqmask = 0xffff,		/* irqmask */
};

static const struct pcmcia_function pcmcia_3cxem556_func1 = {
	.number = 1,			/* function number */
	.function = PCMCIA_FUNCTION_SERIAL,
	.last_config_index = 0x27,	/* last cfe number */
	.ccr_base = 0x900,		/* ccr_base */
	.ccr_mask = 0x63,		/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_3cxem556_func1_cfe0 = {
	.number = 0x27,			/* cfe number */
	.flags = PCMCIA_CFE_IO8 | PCMCIA_CFE_IRQLEVEL,
	.iftype = PCMCIA_IFTYPE_IO,
	.num_iospace = 1,		/* num_iospace */
	.iomask = 3,			/* iomask */
	.iospace = { { .length = 0x0008, .start = 0 } },	/* iospace */
	.irqmask = 0xffff,		/* irqmask */
};

static const struct pcmcia_function pcmcia_3ccfem556bi_func0 = {
	.number = 0,			/* function number */
	.function = PCMCIA_FUNCTION_NETWORK,
	.last_config_index = 0x07,	/* last cfe number */
	.ccr_base = 0x1000,		/* ccr_base */
	.ccr_mask = 0x267,		/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_3ccfem556bi_func0_cfe0 = {
	.number = 0x07,		/* cfe number */
	.flags = PCMCIA_CFE_IO8 | PCMCIA_CFE_IO16 | PCMCIA_CFE_IRQLEVEL,
	.iftype = PCMCIA_IFTYPE_IO,
	.num_iospace = 1,	/* num_iospace */
	.iomask = 5,			/* iomask */
	.iospace = { { .length = 0x0020, .start = 0 } },	/* iospace */
};

static const struct pcmcia_function pcmcia_3ccfem556bi_func1 = {
	.number = 1,			/* function number */
	.function = PCMCIA_FUNCTION_SERIAL,
	.last_config_index = 0x27,	/* last cfe number */
	.ccr_base = 0x1100,		/* ccr_base */
	.ccr_mask = 0x277,		/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_3ccfem556bi_func1_cfe0 = {
	.number = 0x27,		/* cfe number */
	.flags = PCMCIA_CFE_IO8 | PCMCIA_CFE_IRQLEVEL,
	.iftype = PCMCIA_IFTYPE_IO,
	.num_iospace = 1,	/* num_iospace */
	.iomask = 3,		/* iomask */
	.iospace = { { .length = 0x0008, .start = 0 } },	/* iospace */
	.irqmask = 0xffff,	/* irqmask */
};

static const struct pcmcia_function pcmcia_sveclancard_func0 = {
	.number = 0,			/* function number */
	.function = PCMCIA_FUNCTION_NETWORK,
	.last_config_index = 0x1,	/* last cfe number */
	.ccr_base = 0x100,		/* ccr_base */
	.ccr_mask = 0x1,		/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_sveclancard_func0_cfe0 = {
	.number = 0x1,		/* cfe number */
	.flags = PCMCIA_CFE_MWAIT_REQUIRED | PCMCIA_CFE_RDYBSY_ACTIVE |
	    PCMCIA_CFE_WP_ACTIVE | PCMCIA_CFE_BVD_ACTIVE | PCMCIA_CFE_IO16,
	.iftype = PCMCIA_IFTYPE_IO,
	.num_iospace = 1,	/* num_iospace */
	.iomask = 5,		/* iomask */
	.iospace = { { .length = 0x20, .start = 0x300 } },	/* iospace */
	.irqmask = 0xdeb8,	/* irqmask */
};

static const struct pcmcia_function pcmcia_ndc_nd5100_func0 = {
	.number = 0,			/* function number */
	.function = PCMCIA_FUNCTION_NETWORK,
	.last_config_index = 0x23,	/* last cfe number */
	.ccr_base = 0x3f8,		/* ccr_base */
	.ccr_mask = 0x3,		/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_ndc_nd5100_func0_cfe0 = {
	.number = 0x20,			/* cfe number */
	.flags = PCMCIA_CFE_MWAIT_REQUIRED | PCMCIA_CFE_IO16 |
	    PCMCIA_CFE_IRQLEVEL,
	.iftype = PCMCIA_IFTYPE_IO,
	.num_iospace = 1,		/* num_iospace */
	.iomask = 5,			/* iomask */
	.iospace = { { .length = 0x20, .start = 0x300 } },	/* iospace */
	.irqmask = 0xdeb8,		/* irqmask */
};

static const struct pcmcia_function pcmcia_emtac_a2424i_func0 = {
	.number = 0,			/* function number */
	.function = PCMCIA_FUNCTION_NETWORK,
	.last_config_index = 0x21,	/* last cfe number */
	.ccr_base = 0x3e0,		/* ccr_base */
	.ccr_mask = 0x1,		/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_emtac_a2424i_func0_cfe0 = {
	.number = 0x21,		/* cfe number */
	.flags = PCMCIA_CFE_IO16 | PCMCIA_CFE_IRQLEVEL | PCMCIA_CFE_IRQPULSE,
	.iftype = PCMCIA_IFTYPE_IO,
	.num_iospace = 1,	/* num_iospace */
	.iomask = 6,		/* iomask */
	.iospace = { { .length = 0x40, .start = 0x100 } },	/* iospace */
	.irqmask = 0xffff,	/* irqmask */
};

static const struct pcmcia_function pcmcia_fujitsu_j181_func0 = {
	.number = 0,			/* function number */
	.function = PCMCIA_FUNCTION_NETWORK,
	.last_config_index = 0x21,	/* last cfe number */
	.ccr_base = 0xfe0,		/* ccr_base */
	.ccr_mask = 0xf,		/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_fujitsu_j181_func0_cfe0 = {
	.number = 0xc,			/* cfe number */
	.flags = PCMCIA_CFE_MWAIT_REQUIRED | PCMCIA_CFE_WP_ACTIVE |
	    PCMCIA_CFE_IO8 | PCMCIA_CFE_IO16 | PCMCIA_CFE_IRQLEVEL |
	    PCMCIA_CFE_IRQPULSE,
	.iftype = PCMCIA_IFTYPE_IO,
	.num_iospace = 1,		/* num_iospace */
	.iomask = 10,			/* iomask */
	.iospace = { { .length = 0x20, .start = 0x140 } },	/* iospace */
	.irqmask = 0xffff,		/* irqmask */
};

static const struct pcmcia_function pcmcia_necinfrontia_ax420n_func0 = {
	.number = 0,			/* function number */
	.function = PCMCIA_FUNCTION_SERIAL,
	.last_config_index = 0x38,	/* last cfe number */
	.ccr_base = 0x200,		/* ccr_base */
	.ccr_mask = 0x1f,		/* ccr_mask */
};

static const struct pcmcia_config_entry pcmcia_necinfrontia_ax420n_func0_cfe0 = {
	.number = 0x25,			/* cfe number */
	.flags = PCMCIA_CFE_RDYBSY_ACTIVE | PCMCIA_CFE_IO8 |
		 PCMCIA_CFE_IRQLEVEL | PCMCIA_CFE_POWERDOWN |
		 PCMCIA_CFE_AUDIO,
	.iftype = PCMCIA_IFTYPE_IO,
	.num_iospace = 1,		/* num_iospace */
	.iomask = 10,			/* iomask */
	.iospace = { { .length = 0x8, .start = 0x3f8 } },	/* iospace */
	.irqmask = 0x86bc,		/* irqmask */
};

static const struct pcmcia_cis_quirk pcmcia_cis_quirks[] = {
	{ PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CXEM556,
	  PCMCIA_CIS_INVALID,
	  &pcmcia_3cxem556_func0, &pcmcia_3cxem556_func0_cfe0 },
	{ PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CXEM556,
	  PCMCIA_CIS_INVALID,
	  &pcmcia_3cxem556_func1, &pcmcia_3cxem556_func1_cfe0 },
	{ PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CXEM556INT,
	  PCMCIA_CIS_INVALID,
	  &pcmcia_3cxem556_func0, &pcmcia_3cxem556_func0_cfe0 },
	{ PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CXEM556INT,
	  PCMCIA_CIS_INVALID,
	  &pcmcia_3cxem556_func1, &pcmcia_3cxem556_func1_cfe0 },
	{ PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CCFEM556BI,
	  PCMCIA_CIS_INVALID,
	  &pcmcia_3ccfem556bi_func0, &pcmcia_3ccfem556bi_func0_cfe0 },
	{ PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CCFEM556BI,
	  PCMCIA_CIS_INVALID,
	  &pcmcia_3ccfem556bi_func1, &pcmcia_3ccfem556bi_func1_cfe0 },
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_SVEC_LANCARD,
	  &pcmcia_sveclancard_func0, &pcmcia_sveclancard_func0_cfe0 },
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_NDC_ND5100_E,
	  &pcmcia_ndc_nd5100_func0, &pcmcia_ndc_nd5100_func0_cfe0 },
	{ PCMCIA_VENDOR_EMTAC, PCMCIA_PRODUCT_EMTAC_WLAN,
	  PCMCIA_CIS_INVALID,
	  &pcmcia_emtac_a2424i_func0, &pcmcia_emtac_a2424i_func0_cfe0 },
	{ PCMCIA_VENDOR_INVALID, PCMCIA_PRODUCT_INVALID,
	  PCMCIA_CIS_FUJITSU_FMV_J181,
	  &pcmcia_fujitsu_j181_func0, &pcmcia_fujitsu_j181_func0_cfe0 },
	{ PCMCIA_VENDOR_NECINFRONTIA, PCMCIA_PRODUCT_NECINFRONTIA_AX420N,
	  PCMCIA_CIS_INVALID,
	  &pcmcia_necinfrontia_ax420n_func0,
	  &pcmcia_necinfrontia_ax420n_func0_cfe0 },
};

static const int pcmcia_cis_nquirks =
   sizeof(pcmcia_cis_quirks) / sizeof(pcmcia_cis_quirks[0]);

void
pcmcia_check_cis_quirks(struct pcmcia_softc *sc)
{
	int wiped = 0;
	size_t i, j;
	struct pcmcia_function *pf;
	const struct pcmcia_function *pf_last;
	struct pcmcia_config_entry *cfe;
	struct pcmcia_card *card = &sc->card;
	const struct pcmcia_cis_quirk *quirk;

	pf = NULL;
	pf_last = NULL;

	for (i = 0; i < pcmcia_cis_nquirks; i++) {
		quirk = &pcmcia_cis_quirks[i];

		if (card->manufacturer == quirk->manufacturer &&
		    card->manufacturer != PCMCIA_VENDOR_INVALID &&
		    card->product == quirk->product &&
		    card->product != PCMCIA_PRODUCT_INVALID)
			goto match;

		for (j = 0; j < 2; j++)
			if (card->cis1_info[j] == NULL ||
			    quirk->cis1_info[j] == NULL ||
			    strcmp(card->cis1_info[j],
			    quirk->cis1_info[j]) != 0)
				goto nomatch;

match:
		if (!wiped) {
			if (pcmcia_verbose) {
				printf("%s: using CIS quirks for ",
				    device_xname(sc->dev));
				for (j = 0; j < 4; j++) {
					if (card->cis1_info[j] == NULL)
						break;
					if (j)
						printf(", ");
					printf("%s", card->cis1_info[j]);
				}
				printf("\n");
			}
			pcmcia_free_pf(&card->pf_head);
			wiped = 1;
		}

		if (pf_last != quirk->pf) {
			/*
			 * XXX: a driver which still calls pcmcia_card_attach
			 * very early attach stage should be fixed instead.
			 */
			pf = malloc(sizeof(*pf), M_DEVBUF,
			    cold ? M_NOWAIT : M_WAITOK);
			if (pf == NULL)
				panic("pcmcia_check_cis_quirks: malloc pf");
			*pf = *quirk->pf;
			SIMPLEQ_INIT(&pf->cfe_head);
			SIMPLEQ_INSERT_TAIL(&card->pf_head, pf, pf_list);
			pf_last = quirk->pf;
		}

		/*
		 * XXX: see above.
		 */
		cfe = malloc(sizeof(*cfe), M_DEVBUF,
		    cold ? M_NOWAIT : M_WAITOK);
		if (cfe == NULL)
			panic("pcmcia_check_cis_quirks: malloc cfe");
		*cfe = *quirk->cfe;
		KASSERT(pf != NULL);
		SIMPLEQ_INSERT_TAIL(&pf->cfe_head, cfe, cfe_list);

nomatch:;
	}
}
