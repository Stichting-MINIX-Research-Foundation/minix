/*	$NetBSD: pcmcia_cis.c,v 1.55 2009/05/12 14:42:19 cegger Exp $	*/

/*
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
__KERNEL_RCSID(0, "$NetBSD: pcmcia_cis.c,v 1.55 2009/05/12 14:42:19 cegger Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciachip.h>
#include <dev/pcmcia/pcmciavar.h>

#ifdef PCMCIACISDEBUG
int	pcmciacis_debug = 0;
#define	DPRINTF(arg) if (pcmciacis_debug) printf arg
#else
#define	DPRINTF(arg)
#endif

#define	PCMCIA_CIS_SIZE		1024

struct cis_state {
	int	count;
	int	gotmfc;
	struct pcmcia_config_entry temp_cfe;
	struct pcmcia_config_entry *default_cfe;
	struct pcmcia_card *card;
	struct pcmcia_function *pf;
};

int	pcmcia_parse_cis_tuple(struct pcmcia_tuple *, void *);
static void create_pf(struct cis_state *);

static void decode_end(struct pcmcia_tuple *, struct cis_state *);
static void decode_longlink_mfc(struct pcmcia_tuple *, struct cis_state *);
static void decode_device(struct pcmcia_tuple *, struct cis_state *);
static void decode_vers_1(struct pcmcia_tuple *, struct cis_state *);
static void decode_manfid(struct pcmcia_tuple *, struct cis_state *);
static void decode_funcid(struct pcmcia_tuple *, struct cis_state *);
static void decode_funce(struct pcmcia_tuple *, struct cis_state *);
static void decode_config(struct pcmcia_tuple *, struct cis_state *);
static void decode_cftable_entry(struct pcmcia_tuple *, struct cis_state *);


static void
create_pf(struct cis_state *state)
{
	state->pf = malloc(sizeof(*state->pf), M_DEVBUF, M_NOWAIT|M_ZERO);
	state->pf->number = state->count++;
	state->pf->last_config_index = -1;
	SIMPLEQ_INIT(&state->pf->cfe_head);
	SIMPLEQ_INSERT_TAIL(&state->card->pf_head, state->pf, pf_list);
}

void
pcmcia_free_pf(struct pcmcia_function_head *pfhead)
{
	struct pcmcia_function *pf, *npf;
	struct pcmcia_config_entry *cfe, *ncfe;

	for (pf = SIMPLEQ_FIRST(pfhead); pf != NULL; pf = npf) {
		npf = SIMPLEQ_NEXT(pf, pf_list);
		for (cfe = SIMPLEQ_FIRST(&pf->cfe_head); cfe != NULL;
		    cfe = ncfe) {
			ncfe = SIMPLEQ_NEXT(cfe, cfe_list);
			free(cfe, M_DEVBUF);
		}
		free(pf, M_DEVBUF);
	}

	SIMPLEQ_INIT(pfhead);
}

void
pcmcia_read_cis(struct pcmcia_softc *sc)
{
	struct cis_state state;

	memset(&state, 0, sizeof state);

	state.card = &sc->card;

	state.card->error = 0;
	state.card->cis1_major = -1;
	state.card->cis1_minor = -1;
	state.card->cis1_info[0] = NULL;
	state.card->cis1_info[1] = NULL;
	state.card->cis1_info[2] = NULL;
	state.card->cis1_info[3] = NULL;
	state.card->manufacturer = PCMCIA_VENDOR_INVALID;
	state.card->product = PCMCIA_PRODUCT_INVALID;
	SIMPLEQ_INIT(&state.card->pf_head);

	state.pf = NULL;

	if (pcmcia_scan_cis(sc->dev, pcmcia_parse_cis_tuple,
	    &state) == -1)
		state.card->error++;
}

int
pcmcia_scan_cis(device_t dev,
	int (*fct)(struct pcmcia_tuple *, void *),
	void *arg)
{
	struct pcmcia_softc *sc = device_private(dev);
	pcmcia_chipset_tag_t pct;
	pcmcia_chipset_handle_t pch;
	int window;
	struct pcmcia_mem_handle pcmh;
	struct pcmcia_tuple tuple;
	int longlink_present;
	int longlink_common;
	u_long longlink_addr;
	int mfc_count;
	int mfc_index;
	struct {
		int	common;
		u_long	addr;
	} mfc[256 / 5];
	int ret;

	ret = 0;

	pct = sc->pct;
	pch = sc->pch;

	/* allocate some memory */

	if (pcmcia_chip_mem_alloc(pct, pch, PCMCIA_CIS_SIZE, &pcmh)) {
#ifdef DIAGNOSTIC
		aprint_error_dev(sc->dev,
				 "can't alloc memory to read attributes\n");
#endif
		return -1;
	}
	/* initialize state for the primary tuple chain */
	if (pcmcia_chip_mem_map(pct, pch, PCMCIA_MEM_ATTR, 0,
	    PCMCIA_CIS_SIZE, &pcmh, &tuple.ptr, &window)) {
		pcmcia_chip_mem_free(pct, pch, &pcmh);
#ifdef DIAGNOSTIC
		aprint_error_dev(sc->dev,
				 "can't map memory to read attributes\n");
#endif
		return -1;
	}
	tuple.memt = pcmh.memt;
	tuple.memh = pcmh.memh;

	DPRINTF(("cis mem map %x\n", (unsigned int) tuple.memh));

	tuple.mult = 2;

	longlink_present = 1;
	longlink_common = 1;
	longlink_addr = 0;

	mfc_count = 0;
	mfc_index = 0;

	DPRINTF(("%s: CIS tuple chain:\n", device_xname(sc->dev)));

	while (1) {
		DELAY(1000);

		while (1) {
			/*
			 * Perform boundary check for insane cards.
			 * If CIS is too long, simulate CIS end.
			 * (This check may not be sufficient for
			 * malicious cards.)
			 */
			if (tuple.mult * tuple.ptr >= PCMCIA_CIS_SIZE - 1
			    - 32 /* ad hoc value */ ) {
				DPRINTF(("CISTPL_END (too long CIS)\n"));
				tuple.code = PCMCIA_CISTPL_END;
				goto cis_end;
			}

			/* get the tuple code */

			tuple.code = pcmcia_cis_read_1(&tuple, tuple.ptr);

			/* two special-case tuples */

			if (tuple.code == PCMCIA_CISTPL_NULL) {
				DPRINTF((" 00\nCISTPL_NONE\n"));
				tuple.ptr++;
				continue;
			} else if (tuple.code == PCMCIA_CISTPL_END) {
				DPRINTF((" ff\nCISTPL_END\n"));
			cis_end:
				/* Call the function for the END tuple, since
				   the CIS semantics depend on it */
				if ((*fct) (&tuple, arg)) {
					pcmcia_chip_mem_unmap(pct, pch,
							      window);
					ret = 1;
					goto done;
				}
				tuple.ptr++;
				break;
			}

			/* now all the normal tuples */

			tuple.length = pcmcia_cis_read_1(&tuple, tuple.ptr + 1);
#ifdef PCMCIACISDEBUG
			/* print the tuple */
			{
				int i;

				DPRINTF((" %02x %02x", tuple.code,
				    tuple.length));

				for (i = 0; i < tuple.length; i++) {
					DPRINTF((" %02x",
					    pcmcia_tuple_read_1(&tuple, i)));
					if ((i % 16) == 13)
						DPRINTF(("\n"));
				}
				if ((i % 16) != 14)
					DPRINTF(("\n"));
			}
#endif
			switch (tuple.code) {
			case PCMCIA_CISTPL_LONGLINK_A:
			case PCMCIA_CISTPL_LONGLINK_C:
				if (tuple.length < 4) {
					DPRINTF(("CISTPL_LONGLINK_%s too "
					    "short %d\n",
					    longlink_common ? "C" : "A",
					    tuple.length));
					break;
				}
				longlink_present = 1;
				longlink_common = (tuple.code ==
				    PCMCIA_CISTPL_LONGLINK_C) ? 1 : 0;
				longlink_addr = pcmcia_tuple_read_4(&tuple, 0);
				DPRINTF(("CISTPL_LONGLINK_%s %lx\n",
				    longlink_common ? "C" : "A",
				    longlink_addr));
				break;
			case PCMCIA_CISTPL_NO_LINK:
				longlink_present = 0;
				DPRINTF(("CISTPL_NO_LINK\n"));
				break;
			case PCMCIA_CISTPL_CHECKSUM:
				if (tuple.length < 5) {
					DPRINTF(("CISTPL_CHECKSUM too "
					    "short %d\n", tuple.length));
					break;
				} {
					int16_t offset;
					u_long addr, length;
					u_int cksum, sum;
					int i;

					*((u_int16_t *) & offset) =
					    pcmcia_tuple_read_2(&tuple, 0);
					length = pcmcia_tuple_read_2(&tuple, 2);
					cksum = pcmcia_tuple_read_1(&tuple, 4);

					addr = tuple.ptr + offset;

					DPRINTF(("CISTPL_CHECKSUM addr=%lx "
					    "len=%lx cksum=%x",
					    addr, length, cksum));

					/*
					 * XXX do more work to deal with
					 * distant regions
					 */
					if ((addr >= PCMCIA_CIS_SIZE) ||
#if 0
					    ((addr + length) < 0) ||
#endif
					    ((addr + length) >=
					      PCMCIA_CIS_SIZE)) {
						DPRINTF((" skipped, "
						    "too distant\n"));
						break;
					}
					sum = 0;
					for (i = 0; i < length; i++)
						sum +=
						    bus_space_read_1(tuple.memt,
						    tuple.memh,
						    addr + tuple.mult * i);
					if (cksum != (sum & 0xff)) {
						DPRINTF((" failed sum=%x\n",
						    sum));
						aprint_error_dev(sc->dev,
						 "CIS checksum failed\n");
#if 0
						/*
						 * XXX Some working cards have
						 * XXX bad checksums!!
						 */
						ret = -1;
#endif
					} else {
						DPRINTF((" ok\n"));
					}
				}
				break;
			case PCMCIA_CISTPL_LONGLINK_MFC:
				if (tuple.length < 1) {
					DPRINTF(("CISTPL_LONGLINK_MFC too "
					    "short %d\n", tuple.length));
					break;
				}
				if (((tuple.length - 1) % 5) != 0) {
					DPRINTF(("CISTPL_LONGLINK_MFC bogus "
					    "length %d\n", tuple.length));
					break;
				}
				/*
				 * this is kind of ad hoc, as I don't have
				 * any real documentation
				 */
				{
					int i, tmp_count;

					/*
					 * put count into tmp var so that
					 * if we have to bail (because it's
					 * a bogus count) it won't be
					 * remembered for later use.
					 */
					tmp_count =
					    pcmcia_tuple_read_1(&tuple, 0);
					DPRINTF(("CISTPL_LONGLINK_MFC %d",
					    tmp_count));

					/*
					 * make _sure_ it's the right size;
					 * if too short, it may be a weird
					 * (unknown/undefined) format
					 */
					if (tuple.length != (tmp_count*5 + 1)) {
						DPRINTF((" bogus length %d\n",
						    tuple.length));
						break;
					}

#ifdef PCMCIACISDEBUG	/* maybe enable all the time? */
					/*
					 * sanity check for a programming
					 * error which is difficult to find
					 * when debugging.
					 */
					if (tmp_count >
					    howmany(sizeof mfc, sizeof mfc[0]))
						panic("CISTPL_LONGLINK_MFC mfc "
						    "count would blow stack");
#endif

					mfc_count = tmp_count;
					for (i = 0; i < mfc_count; i++) {
						mfc[i].common =
						    (pcmcia_tuple_read_1(&tuple,
						    1 + 5 * i) ==
						    PCMCIA_MFC_MEM_COMMON) ?
						    1 : 0;
						mfc[i].addr =
						    pcmcia_tuple_read_4(&tuple,
						    1 + 5 * i + 1);
						DPRINTF((" %s:%lx",
						    mfc[i].common ? "common" :
						    "attr", mfc[i].addr));
					}
					DPRINTF(("\n"));
				}
				/*
				 * for LONGLINK_MFC, fall through to the
				 * function.  This tuple has structural and
				 * semantic content.
				 */
			default:
				{
					if ((*fct) (&tuple, arg)) {
						pcmcia_chip_mem_unmap(pct,
						    pch, window);
						ret = 1;
						goto done;
					}
				}
				break;
			}	/* switch */
			/* skip to the next tuple */
			tuple.ptr += 2 + tuple.length;
		}

		/*
		 * the chain is done.  Clean up and move onto the next one,
		 * if any.  The loop is here in the case that there is an MFC
		 * card with no longlink (which defaults to existing, == 0).
		 * In general, this means that if one pointer fails, it will
		 * try the next one, instead of just bailing.
		 */

		while (1) {
			pcmcia_chip_mem_unmap(pct, pch, window);

			if (longlink_present) {
				/*
				 * if the longlink is to attribute memory,
				 * then it is unindexed.  That is, if the
				 * link value is 0x100, then the actual
				 * memory address is 0x200.  This means that
				 * we need to multiply by 2 before calling
				 * mem_map, and then divide the resulting ptr
				 * by 2 after.
				 */

				if (!longlink_common)
					longlink_addr *= 2;

				pcmcia_chip_mem_map(pct, pch, longlink_common ?
				    (PCMCIA_WIDTH_MEM8 | PCMCIA_MEM_COMMON) :
				    PCMCIA_MEM_ATTR,
				    longlink_addr, PCMCIA_CIS_SIZE,
				    &pcmh, &tuple.ptr, &window);

				tuple.memt = pcmh.memt;
				tuple.memh = pcmh.memh;

				if (!longlink_common)
					tuple.ptr /= 2;

				DPRINTF(("cis mem map %x\n",
				    (unsigned int) tuple.memh));

				tuple.mult = longlink_common ? 1 : 2;
				longlink_present = 0;
				longlink_common = 1;
				longlink_addr = 0;
			} else if (mfc_count && (mfc_index < mfc_count)) {
				if (!mfc[mfc_index].common)
					mfc[mfc_index].addr *= 2;

				pcmcia_chip_mem_map(pct, pch,
				    mfc[mfc_index].common ?
				    (PCMCIA_WIDTH_MEM8 | PCMCIA_MEM_COMMON) :
				    PCMCIA_MEM_ATTR,
				    mfc[mfc_index].addr, PCMCIA_CIS_SIZE,
				    &pcmh, &tuple.ptr, &window);

				if (!mfc[mfc_index].common)
					tuple.ptr /= 2;

				DPRINTF(("cis mem map %x\n",
				    (unsigned int) tuple.memh));

				/* set parse state, and point at the next one */

				tuple.mult = mfc[mfc_index].common ? 1 : 2;

				mfc_index++;
			} else {
				goto done;
			}

			/* make sure that the link is valid */
			tuple.code = pcmcia_cis_read_1(&tuple, tuple.ptr);
			if (tuple.code != PCMCIA_CISTPL_LINKTARGET) {
				DPRINTF(("CISTPL_LINKTARGET expected, "
				    "code %02x observed\n", tuple.code));
				continue;
			}
			tuple.length = pcmcia_cis_read_1(&tuple, tuple.ptr + 1);
			if (tuple.length < 3) {
				DPRINTF(("CISTPL_LINKTARGET too short %d\n",
				    tuple.length));
				continue;
			}
			if ((pcmcia_tuple_read_1(&tuple, 0) != 'C') ||
			    (pcmcia_tuple_read_1(&tuple, 1) != 'I') ||
			    (pcmcia_tuple_read_1(&tuple, 2) != 'S')) {
				DPRINTF(("CISTPL_LINKTARGET magic "
				    "%02x%02x%02x incorrect\n",
				    pcmcia_tuple_read_1(&tuple, 0),
				    pcmcia_tuple_read_1(&tuple, 1),
				    pcmcia_tuple_read_1(&tuple, 2)));
				continue;
			}
			tuple.ptr += 2 + tuple.length;

			break;
		}
	}

	pcmcia_chip_mem_unmap(pct, pch, window);

done:
	/* Last, free the allocated memory block */
	pcmcia_chip_mem_free(pct, pch, &pcmh);

	return (ret);
}

/* XXX this is incredibly verbose.  Not sure what trt is */

void
pcmcia_print_cis(struct pcmcia_softc *sc)
{
	struct pcmcia_card *card = &sc->card;
	struct pcmcia_function *pf;
	struct pcmcia_config_entry *cfe;
	int i;

	printf("%s: CIS version ", device_xname(sc->dev));
	if (card->cis1_major == 4) {
		if (card->cis1_minor == 0)
			printf("PCMCIA 1.0\n");
		else if (card->cis1_minor == 1)
			printf("PCMCIA 2.0 or 2.1\n");
	} else if (card->cis1_major >= 5)
		printf("PC Card Standard %d.%d\n", card->cis1_major, card->cis1_minor);
	else
		printf("unknown (major=%d, minor=%d)\n",
		    card->cis1_major, card->cis1_minor);

	printf("%s: CIS info: ", device_xname(sc->dev));
	for (i = 0; i < 4; i++) {
		if (card->cis1_info[i] == NULL)
			break;
		if (i)
			printf(", ");
		printf("%s", card->cis1_info[i]);
	}
	printf("\n");

	printf("%s: Manufacturer code 0x%x, product 0x%x\n",
	       device_xname(sc->dev), card->manufacturer, card->product);

	SIMPLEQ_FOREACH(pf, &card->pf_head, pf_list) {
		printf("%s: function %d: ", device_xname(sc->dev), pf->number);

		switch (pf->function) {
		case PCMCIA_FUNCTION_UNSPEC:
			printf("unspecified");
			break;
		case PCMCIA_FUNCTION_MULTIFUNCTION:
			printf("multi-function");
			break;
		case PCMCIA_FUNCTION_MEMORY:
			printf("memory");
			break;
		case PCMCIA_FUNCTION_SERIAL:
			printf("serial port");
			break;
		case PCMCIA_FUNCTION_PARALLEL:
			printf("parallel port");
			break;
		case PCMCIA_FUNCTION_DISK:
			printf("fixed disk");
			switch (pf->pf_funce_disk_interface) {
			case PCMCIA_TPLFE_DDI_PCCARD_ATA:
				printf("(ata)");
				break;
			default:
				break;
			}
			break;
		case PCMCIA_FUNCTION_VIDEO:
			printf("video adapter");
			break;
		case PCMCIA_FUNCTION_NETWORK:
			printf("network adapter");
			break;
		case PCMCIA_FUNCTION_AIMS:
			printf("auto incrementing mass storage");
			break;
		case PCMCIA_FUNCTION_SCSI:
			printf("SCSI bridge");
			break;
		case PCMCIA_FUNCTION_SECURITY:
			printf("Security services");
			break;
		case PCMCIA_FUNCTION_INSTRUMENT:
			printf("Instrument");
			break;
		default:
			printf("unknown (%d)", pf->function);
			break;
		}

		printf(", ccr addr %lx mask %lx\n", pf->ccr_base, pf->ccr_mask);

		SIMPLEQ_FOREACH(cfe, &pf->cfe_head, cfe_list) {
			printf("%s: function %d, config table entry %d: ",
			    device_xname(sc->dev), pf->number, cfe->number);

			switch (cfe->iftype) {
			case PCMCIA_IFTYPE_MEMORY:
				printf("memory card");
				break;
			case PCMCIA_IFTYPE_IO:
				printf("I/O card");
				break;
			default:
				printf("card type unknown");
				break;
			}

			printf("; irq mask %x", cfe->irqmask);

			if (cfe->num_iospace) {
				printf("; iomask %lx, iospace", cfe->iomask);

				for (i = 0; i < cfe->num_iospace; i++) {
					printf(" %lx", cfe->iospace[i].start);
					if (cfe->iospace[i].length)
						printf("-%lx",
						    cfe->iospace[i].start +
						    cfe->iospace[i].length - 1);
				}
			}
			if (cfe->num_memspace) {
				printf("; memspace");

				for (i = 0; i < cfe->num_memspace; i++) {
					printf(" %lx",
					    cfe->memspace[i].cardaddr);
					if (cfe->memspace[i].length)
						printf("-%lx",
						    cfe->memspace[i].cardaddr +
						    cfe->memspace[i].length - 1);
					if (cfe->memspace[i].hostaddr)
						printf("@%lx",
						    cfe->memspace[i].hostaddr);
				}
			}
			if (cfe->maxtwins)
				printf("; maxtwins %d", cfe->maxtwins);

			printf(";");

			if (cfe->flags & PCMCIA_CFE_MWAIT_REQUIRED)
				printf(" mwait_required");
			if (cfe->flags & PCMCIA_CFE_RDYBSY_ACTIVE)
				printf(" rdybsy_active");
			if (cfe->flags & PCMCIA_CFE_WP_ACTIVE)
				printf(" wp_active");
			if (cfe->flags & PCMCIA_CFE_BVD_ACTIVE)
				printf(" bvd_active");
			if (cfe->flags & PCMCIA_CFE_IO8)
				printf(" io8");
			if (cfe->flags & PCMCIA_CFE_IO16)
				printf(" io16");
			if (cfe->flags & PCMCIA_CFE_IRQSHARE)
				printf(" irqshare");
			if (cfe->flags & PCMCIA_CFE_IRQPULSE)
				printf(" irqpulse");
			if (cfe->flags & PCMCIA_CFE_IRQLEVEL)
				printf(" irqlevel");
			if (cfe->flags & PCMCIA_CFE_POWERDOWN)
				printf(" powerdown");
			if (cfe->flags & PCMCIA_CFE_READONLY)
				printf(" readonly");
			if (cfe->flags & PCMCIA_CFE_AUDIO)
				printf(" audio");

			printf("\n");
		}
	}

	if (card->error)
		printf("%s: %d errors found while parsing CIS\n",
		    device_xname(sc->dev), card->error);
}

int
pcmcia_parse_cis_tuple(struct pcmcia_tuple *tuple, void *arg)
{
	struct cis_state *state = arg;

	switch (tuple->code) {
	case PCMCIA_CISTPL_END:
		decode_end(tuple, state);
		break;

	case PCMCIA_CISTPL_LONGLINK_MFC:
		decode_longlink_mfc(tuple, state);
		break;

	case PCMCIA_CISTPL_DEVICE:
	case PCMCIA_CISTPL_DEVICE_A:
		decode_device(tuple, state);
		break;

	case PCMCIA_CISTPL_VERS_1:
		decode_vers_1(tuple, state);
		break;

	case PCMCIA_CISTPL_MANFID:
		decode_manfid(tuple, state);
		break;

	case PCMCIA_CISTPL_FUNCID:
		decode_funcid(tuple, state);
		break;

	case PCMCIA_CISTPL_FUNCE:
		decode_funce(tuple, state);
		break;

	case PCMCIA_CISTPL_CONFIG:
		decode_config(tuple, state);
		break;

	case PCMCIA_CISTPL_CFTABLE_ENTRY:
		decode_cftable_entry(tuple, state);
		break;
	default:
		DPRINTF(("unhandled CISTPL %x\n", tuple->code));
		break;
	}

	return (0);
}

static void
decode_end(struct pcmcia_tuple *tuple, struct cis_state *state)
{
	/* if we've seen a LONGLINK_MFC, and this is the first
	 * END after it, reset the function list.
	 *
	 * XXX This might also be the right place to start a
	 * new function, but that assumes that a function
	 * definition never crosses any longlink, and I'm not
	 * sure about that.  This is probably safe for MFC
	 * cards, but what we have now isn't broken, so I'd
	 * rather not change it.
	 */
	if (state->gotmfc == 1) {
		state->gotmfc = 2;
		state->count = 0;
		state->pf = NULL;

		pcmcia_free_pf(&state->card->pf_head);
	}
}

static void
decode_longlink_mfc(struct pcmcia_tuple *tuple,
    struct cis_state *state)
{
	/*
	 * this tuple's structure was dealt with in scan_cis.  here,
	 * record the fact that the MFC tuple was seen, so that
	 * functions declared before the MFC link can be cleaned
	 * up.
	 */
	if (state->gotmfc == 0) {
		state->gotmfc = 1;
	} else {
		DPRINTF(("got LONGLINK_MFC again!"));
	}
}

static void
decode_device(struct pcmcia_tuple *tuple,
    struct cis_state *state)
{
#ifdef PCMCIACISDEBUG
	u_int reg, dtype, dspeed;

	reg = pcmcia_tuple_read_1(tuple, 0);
	dtype = reg & PCMCIA_DTYPE_MASK;
	dspeed = reg & PCMCIA_DSPEED_MASK;

	DPRINTF(("CISTPL_DEVICE%s type=",
	(tuple->code == PCMCIA_CISTPL_DEVICE) ? "" : "_A"));
	switch (dtype) {
	case PCMCIA_DTYPE_NULL:
		DPRINTF(("null"));
		break;
	case PCMCIA_DTYPE_ROM:
		DPRINTF(("rom"));
		break;
	case PCMCIA_DTYPE_OTPROM:
		DPRINTF(("otprom"));
		break;
	case PCMCIA_DTYPE_EPROM:
		DPRINTF(("eprom"));
		break;
	case PCMCIA_DTYPE_EEPROM:
		DPRINTF(("eeprom"));
		break;
	case PCMCIA_DTYPE_FLASH:
		DPRINTF(("flash"));
		break;
	case PCMCIA_DTYPE_SRAM:
		DPRINTF(("sram"));
		break;
	case PCMCIA_DTYPE_DRAM:
		DPRINTF(("dram"));
		break;
	case PCMCIA_DTYPE_FUNCSPEC:
		DPRINTF(("funcspec"));
		break;
	case PCMCIA_DTYPE_EXTEND:
		DPRINTF(("extend"));
		break;
	default:
		DPRINTF(("reserved"));
		break;
	}
	DPRINTF((" speed="));
	switch (dspeed) {
	case PCMCIA_DSPEED_NULL:
		DPRINTF(("null"));
		break;
	case PCMCIA_DSPEED_250NS:
		DPRINTF(("250ns"));
		break;
	case PCMCIA_DSPEED_200NS:
		DPRINTF(("200ns"));
		break;
	case PCMCIA_DSPEED_150NS:
		DPRINTF(("150ns"));
		break;
	case PCMCIA_DSPEED_100NS:
		DPRINTF(("100ns"));
		break;
	case PCMCIA_DSPEED_EXT:
		DPRINTF(("ext"));
		break;
	default:
		DPRINTF(("reserved"));
		break;
	}
	DPRINTF(("\n"));
#endif
}

static void
decode_vers_1(struct pcmcia_tuple *tuple, struct cis_state *state)
{
	int start, i, ch, count;

	if (tuple->length < 6) {
		DPRINTF(("CISTPL_VERS_1 too short %d\n",
		    tuple->length));
		return;
	}
	state->card->cis1_major = pcmcia_tuple_read_1(tuple, 0);
	state->card->cis1_minor = pcmcia_tuple_read_1(tuple, 1);

	for (count = 0, start = 0, i = 0;
	    (count < 4) && ((i + 4) < 256); i++) {
		ch = pcmcia_tuple_read_1(tuple, 2 + i);
		if (ch == 0xff) {
			if (i > start) {
				state->card->cis1_info_buf[i] = 0;
				state->card->cis1_info[count] =
				    state->card->cis1_info_buf + start;
			}
			break;
		}
		state->card->cis1_info_buf[i] = ch;
		if (ch == 0) {
			state->card->cis1_info[count] =
			    state->card->cis1_info_buf + start;
			start = i + 1;
			count++;
		}
	}
	DPRINTF(("CISTPL_VERS_1\n"));
}

static void
decode_manfid(struct pcmcia_tuple *tuple, struct cis_state *state)
{
	if (tuple->length < 4) {
		DPRINTF(("CISTPL_MANFID too short %d\n",
		    tuple->length));
		return;
	}
	state->card->manufacturer = pcmcia_tuple_read_2(tuple, 0);
	state->card->product = pcmcia_tuple_read_2(tuple, 2);
	DPRINTF(("CISTPL_MANFID\n"));
}

static void
decode_funcid(struct pcmcia_tuple *tuple, struct cis_state *state)
{
	if (tuple->length < 1) {
		DPRINTF(("CISTPL_FUNCID too short %d\n",
		    tuple->length));
		return;
	}
	if (state->pf) {
		if (state->pf->function == PCMCIA_FUNCTION_UNSPEC) {
			/*
			 * This looks like a opportunistic function
			 * created by a CONFIG tuple.  Just keep it.
			 */
		} else {
			/*
			 * A function is being defined, end it.
			 */
			state->pf = NULL;
		}
	}
	if (state->pf == NULL)
		create_pf(state);
	state->pf->function = pcmcia_tuple_read_1(tuple, 0);

	DPRINTF(("CISTPL_FUNCID\n"));
}

static void
decode_funce(struct pcmcia_tuple *tuple, struct cis_state *state)
{
	struct pcmcia_function *pf = state->pf;
	int type = pcmcia_tuple_read_1(tuple, 0);

	if (state->pf == NULL || state->pf->function <= 0) {
		DPRINTF(("CISTPL_FUNCE is not followed by "
		    "valid CISTPL_FUNCID\n"));
		return;
	}
	if (tuple->length < 2)
		return;
	switch (pf->function) {
	case PCMCIA_FUNCTION_DISK:
		if (type == PCMCIA_TPLFE_TYPE_DISK_DEVICE_INTERFACE) {
			pf->pf_funce_disk_interface
			    = pcmcia_tuple_read_1(tuple, 1);
		}
		break;
	case PCMCIA_FUNCTION_NETWORK:
		if (type == PCMCIA_TPLFE_TYPE_LAN_NID) {
			int i;
			int len = pcmcia_tuple_read_1(tuple, 1);
			if (tuple->length < 2 + len || len > 8) {
				/* tuple length not enough or nid too long */
				break;
			}
			for (i = 0; i < len; ++i) {
				pf->pf_funce_lan_nid[i]
				    = pcmcia_tuple_read_1(tuple, 2 + i);
			}
			pf->pf_funce_lan_nidlen = len;
		}
		break;
	default:
		break;
	}

	return;
}

static void
decode_config(struct pcmcia_tuple *tuple, struct cis_state *state)
{
	u_int reg, rasz, rmsz, rfsz;
	int i;
	/* most of these are educated guesses */
	static const struct pcmcia_config_entry init_cfe = {
		.number	= -1,
		.flags	= PCMCIA_CFE_RDYBSY_ACTIVE | PCMCIA_CFE_WP_ACTIVE |
			  PCMCIA_CFE_BVD_ACTIVE,
		.iftype	= PCMCIA_IFTYPE_MEMORY,
	};

	if (tuple->length < 3) {
		DPRINTF(("CISTPL_CONFIG too short %d\n", tuple->length));
		return;
	} 
	reg = pcmcia_tuple_read_1(tuple, 0);
	rasz = 1 + ((reg & PCMCIA_TPCC_RASZ_MASK) >>
	    PCMCIA_TPCC_RASZ_SHIFT);
	rmsz = 1 + ((reg & PCMCIA_TPCC_RMSZ_MASK) >>
	    PCMCIA_TPCC_RMSZ_SHIFT);
	rfsz = ((reg & PCMCIA_TPCC_RFSZ_MASK) >>
	    PCMCIA_TPCC_RFSZ_SHIFT);

	if (tuple->length < (rasz + rmsz + rfsz)) {
		DPRINTF(("CISTPL_CONFIG (%d,%d,%d) too short %d\n",
		    rasz, rmsz, rfsz, tuple->length));
		return;
	}
	if (state->pf == NULL) {
		create_pf(state);
		state->pf->function = PCMCIA_FUNCTION_UNSPEC;
	}
	state->pf->last_config_index =
	    pcmcia_tuple_read_1(tuple, 1);

	state->pf->ccr_base = 0;
	for (i = 0; i < rasz; i++)
		state->pf->ccr_base |= ((pcmcia_tuple_read_1(tuple, 2 + i)) <<
		    (i * 8));

	state->pf->ccr_mask = 0;
	for (i = 0; i < rmsz; i++)
		state->pf->ccr_mask |= ((pcmcia_tuple_read_1(tuple,
		    2 + rasz + i)) << (i * 8));

	/* skip the reserved area and subtuples */

	/* reset the default cfe for each cfe list */
	state->temp_cfe = init_cfe;
	state->default_cfe = &state->temp_cfe;
	DPRINTF(("CISTPL_CONFIG\n"));
}

static void
decode_cftable_entry(struct pcmcia_tuple *tuple, struct cis_state *state)
{
	int idx, i, j;
	u_int reg, reg2;
	u_int intface, def, num;
	u_int power, timing, iospace, irq, memspace, misc;
	struct pcmcia_config_entry *cfe;

	idx = 0;

	reg = pcmcia_tuple_read_1(tuple, idx);
	idx++;
	intface = reg & PCMCIA_TPCE_INDX_INTFACE;
	def = reg & PCMCIA_TPCE_INDX_DEFAULT;
	num = reg & PCMCIA_TPCE_INDX_NUM_MASK;

	/*
	 * this is a little messy.  Some cards have only a
	 * cfentry with the default bit set.  So, as we go
	 * through the list, we add new indexes to the queue,
	 * and keep a pointer to the last one with the
	 * default bit set.  if we see a record with the same
	 * index, as the default, we stash the default and
	 * replace the queue entry. otherwise, we just add
	 * new entries to the queue, pointing the default ptr
	 * at them if the default bit is set.  if we get to
	 * the end with the default pointer pointing at a
	 * record which hasn't had a matching index, that's
	 * ok; it just becomes a cfentry like any other.
	 */

	/*
	 * if the index in the cis differs from the default
	 * cis, create new entry in the queue and start it
	 * with the current default
	 */
	if (state->default_cfe == NULL) {
		DPRINTF(("CISTPL_CFTABLE_ENTRY with no "
		    "default\n"));
		return;
	}
	if (num != state->default_cfe->number) {
		cfe = malloc(sizeof(*cfe), M_DEVBUF, M_NOWAIT);
		if (cfe == NULL) {
			printf("Cannot allocate cfe entry\n");
			return;
		}

		*cfe = *state->default_cfe;

		SIMPLEQ_INSERT_TAIL(&state->pf->cfe_head, cfe, cfe_list);

		cfe->number = num;

		/*
		 * if the default bit is set in the cis, then
		 * point the new default at whatever is being
		 * filled in
		 */
		if (def)
			state->default_cfe = cfe;
	} else {
		/*
		 * the cis index matches the default index,
		 * fill in the default cfentry.  It is
		 * assumed that the cfdefault index is in the
		 * queue.  For it to be otherwise, the cis
		 * index would have to be -1 (initial
		 * condition) which is not possible, or there
		 * would have to be a preceding cis entry
		 * which had the same cis index and had the
		 * default bit unset. Neither condition
		 * should happen.  If it does, this cfentry
		 * is lost (written into temp space), which
		 * is an acceptable failure mode.
		 */

		cfe = state->default_cfe;

		/*
		 * if the cis entry does not have the default
		 * bit set, copy the default out of the way
		 * first.
		 */
		if (!def) {
			state->temp_cfe = *state->default_cfe;
			state->default_cfe = &state->temp_cfe;
		}
	}

	if (intface) {
		reg = pcmcia_tuple_read_1(tuple, idx);
		idx++;
		cfe->flags &= ~(PCMCIA_CFE_MWAIT_REQUIRED
		    | PCMCIA_CFE_RDYBSY_ACTIVE
		    | PCMCIA_CFE_WP_ACTIVE
		    | PCMCIA_CFE_BVD_ACTIVE);
		if (reg & PCMCIA_TPCE_IF_MWAIT)
			cfe->flags |= PCMCIA_CFE_MWAIT_REQUIRED;
		if (reg & PCMCIA_TPCE_IF_RDYBSY)
			cfe->flags |= PCMCIA_CFE_RDYBSY_ACTIVE;
		if (reg & PCMCIA_TPCE_IF_WP)
			cfe->flags |= PCMCIA_CFE_WP_ACTIVE;
		if (reg & PCMCIA_TPCE_IF_BVD)
			cfe->flags |= PCMCIA_CFE_BVD_ACTIVE;
		cfe->iftype = reg & PCMCIA_TPCE_IF_IFTYPE;
	}
	reg = pcmcia_tuple_read_1(tuple, idx);
	idx++;

	power = reg & PCMCIA_TPCE_FS_POWER_MASK;
	timing = reg & PCMCIA_TPCE_FS_TIMING;
	iospace = reg & PCMCIA_TPCE_FS_IOSPACE;
	irq = reg & PCMCIA_TPCE_FS_IRQ;
	memspace = reg & PCMCIA_TPCE_FS_MEMSPACE_MASK;
	misc = reg & PCMCIA_TPCE_FS_MISC;

	if (power) {
		/* skip over power, don't save */
		/* for each parameter selection byte */
		for (i = 0; i < power; i++) {
			reg = pcmcia_tuple_read_1(tuple, idx);
			idx++;
			/* for each bit */
			for (j = 0; j < 7; j++) {
				/* if the bit is set */
				if ((reg >> j) & 0x01) {
					/* skip over bytes */
					do {
						reg2 = pcmcia_tuple_read_1(tuple, idx);
						idx++;
						/*
						 * until
						 * non-
						 * extension
						 * byte
						 */
					} while (reg2 & 0x80);
				}
			}
		}
	}
	if (timing) {
		/* skip over timing, don't save */
		reg = pcmcia_tuple_read_1(tuple, idx);
		idx++;

		if ((reg & PCMCIA_TPCE_TD_RESERVED_MASK) !=
		    PCMCIA_TPCE_TD_RESERVED_MASK)
			idx++;
		if ((reg & PCMCIA_TPCE_TD_RDYBSY_MASK) !=
		    PCMCIA_TPCE_TD_RDYBSY_MASK)
			idx++;
		if ((reg & PCMCIA_TPCE_TD_WAIT_MASK) !=
		    PCMCIA_TPCE_TD_WAIT_MASK)
			idx++;
	}
	if (iospace) {
		if (tuple->length <= idx) {
			DPRINTF(("ran out of space before TCPE_IO\n"));
			goto abort_cfe;
		}

		reg = pcmcia_tuple_read_1(tuple, idx);
		idx++;

		cfe->flags &=
		    ~(PCMCIA_CFE_IO8 | PCMCIA_CFE_IO16);
		if (reg & PCMCIA_TPCE_IO_BUSWIDTH_8BIT)
			cfe->flags |= PCMCIA_CFE_IO8;
		if (reg & PCMCIA_TPCE_IO_BUSWIDTH_16BIT)
			cfe->flags |= PCMCIA_CFE_IO16;
		cfe->iomask =
		    reg & PCMCIA_TPCE_IO_IOADDRLINES_MASK;

		if (reg & PCMCIA_TPCE_IO_HASRANGE) {
			reg = pcmcia_tuple_read_1(tuple, idx);
			idx++;

			cfe->num_iospace = 1 + (reg &
			    PCMCIA_TPCE_IO_RANGE_COUNT);

			if (cfe->num_iospace >
			    (sizeof(cfe->iospace) /
			     sizeof(cfe->iospace[0]))) {
				DPRINTF(("too many io "
				    "spaces %d",
				    cfe->num_iospace));
				state->card->error++;
				return;
			}
			for (i = 0; i < cfe->num_iospace; i++) {
				switch (reg & PCMCIA_TPCE_IO_RANGE_ADDRSIZE_MASK) {
				case PCMCIA_TPCE_IO_RANGE_ADDRSIZE_NONE:
					cfe->iospace[i].start =
					    0;
					break;
				case PCMCIA_TPCE_IO_RANGE_ADDRSIZE_ONE:
					cfe->iospace[i].start =
						pcmcia_tuple_read_1(tuple, idx);
					idx++;
					break;
				case PCMCIA_TPCE_IO_RANGE_ADDRSIZE_TWO:
					cfe->iospace[i].start =
						pcmcia_tuple_read_2(tuple, idx);
					idx += 2;
					break;
				case PCMCIA_TPCE_IO_RANGE_ADDRSIZE_FOUR:
					cfe->iospace[i].start =
						pcmcia_tuple_read_4(tuple, idx);
					idx += 4;
					break;
				}
				switch (reg &
					PCMCIA_TPCE_IO_RANGE_LENGTHSIZE_MASK) {
				case PCMCIA_TPCE_IO_RANGE_LENGTHSIZE_NONE:
					cfe->iospace[i].length =
					    0;
					break;
				case PCMCIA_TPCE_IO_RANGE_LENGTHSIZE_ONE:
					cfe->iospace[i].length =
						pcmcia_tuple_read_1(tuple, idx);
					idx++;
					break;
				case PCMCIA_TPCE_IO_RANGE_LENGTHSIZE_TWO:
					cfe->iospace[i].length =
						pcmcia_tuple_read_2(tuple, idx);
					idx += 2;
					break;
				case PCMCIA_TPCE_IO_RANGE_LENGTHSIZE_FOUR:
					cfe->iospace[i].length =
						pcmcia_tuple_read_4(tuple, idx);
					idx += 4;
					break;
				}
				cfe->iospace[i].length++;
			}
		} else {
			cfe->num_iospace = 1;
			cfe->iospace[0].start = 0;
			cfe->iospace[0].length =
			    (1 << cfe->iomask);
		}
	}
	if (irq) {
		if (tuple->length <= idx) {
			DPRINTF(("ran out of space before TCPE_IR\n"));
			goto abort_cfe;
		}

		reg = pcmcia_tuple_read_1(tuple, idx);
		idx++;

		cfe->flags &= ~(PCMCIA_CFE_IRQSHARE
		    | PCMCIA_CFE_IRQPULSE
		    | PCMCIA_CFE_IRQLEVEL);
		if (reg & PCMCIA_TPCE_IR_SHARE)
			cfe->flags |= PCMCIA_CFE_IRQSHARE;
		if (reg & PCMCIA_TPCE_IR_PULSE)
			cfe->flags |= PCMCIA_CFE_IRQPULSE;
		if (reg & PCMCIA_TPCE_IR_LEVEL)
			cfe->flags |= PCMCIA_CFE_IRQLEVEL;

		if (reg & PCMCIA_TPCE_IR_HASMASK) {
			/*
			 * it's legal to ignore the
			 * special-interrupt bits, so I will
			 */

			cfe->irqmask =
			    pcmcia_tuple_read_2(tuple, idx);
			idx += 2;
		} else {
			cfe->irqmask =
			    (1 << (reg & PCMCIA_TPCE_IR_IRQ));
		}
	}
	if (memspace) {
		int lengthsize;
		int cardaddrsize;
		int hostaddrsize;

		if (tuple->length <= idx) {
			DPRINTF(("ran out of space before TCPE_MS\n"));
			goto abort_cfe;
		}

		switch (memspace) {
#ifdef notdef	/* This is 0 */
		case PCMCIA_TPCE_FS_MEMSPACE_NONE:
			cfe->num_memspace = 0;
			break;
#endif

		case PCMCIA_TPCE_FS_MEMSPACE_LENGTH:
			cfe->num_memspace = 1;
			cfe->memspace[0].length = 256 *
			    pcmcia_tuple_read_2(tuple, idx);
			idx += 2;
			cfe->memspace[0].cardaddr = 0;
			cfe->memspace[0].hostaddr = 0;
			break;

		case PCMCIA_TPCE_FS_MEMSPACE_LENGTHADDR:
			cfe->num_memspace = 1;
			cfe->memspace[0].length = 256 *
			    pcmcia_tuple_read_2(tuple, idx);
			idx += 2;
			cfe->memspace[0].cardaddr = 256 *
			    pcmcia_tuple_read_2(tuple, idx);
			idx += 2;
			cfe->memspace[0].hostaddr =
			    cfe->memspace[0].cardaddr;
			break;

		default:
			reg = pcmcia_tuple_read_1(tuple, idx);
			idx++;

			cfe->num_memspace = (reg & PCMCIA_TPCE_MS_COUNT)
			    + 1;

			if (cfe->num_memspace >
			    (sizeof(cfe->memspace) /
				sizeof(cfe->memspace[0]))) {
				DPRINTF(("too many mem spaces %d",
				    cfe->num_memspace));
				state->card->error++;
				return;
			}
			lengthsize = 
			    ((reg & PCMCIA_TPCE_MS_LENGTH_SIZE_MASK) >>
				PCMCIA_TPCE_MS_LENGTH_SIZE_SHIFT);
			cardaddrsize =
			    ((reg &
				PCMCIA_TPCE_MS_CARDADDR_SIZE_MASK) >>
				  PCMCIA_TPCE_MS_CARDADDR_SIZE_SHIFT);
			hostaddrsize =
			    (reg & PCMCIA_TPCE_MS_HOSTADDR) ?
			    cardaddrsize : 0;

			if (lengthsize == 0) {
				DPRINTF(("cfe memspace "
				    "lengthsize == 0"));
				state->card->error++;
			}
			for (i = 0; i < cfe->num_memspace; i++) {
				if (lengthsize) {
					cfe->memspace[i].length = 256 *
					    pcmcia_tuple_read_n(tuple,
						lengthsize, idx);
					idx += lengthsize;
				} else {
					cfe->memspace[i].length = 0;
				}
				if (cfe->memspace[i].length == 0) {
					DPRINTF(("cfe->memspace" 
					    "[%d].length == 0", i));
					state->card->error++;
				}
				if (cardaddrsize) {
					cfe->memspace[i].cardaddr =
					    256 * 
					    pcmcia_tuple_read_n(tuple,
						cardaddrsize, idx);
					idx += cardaddrsize;
				} else {
					cfe->memspace[i].cardaddr = 0;
				}
				if (hostaddrsize) {
					cfe->memspace[i].hostaddr =
					    256 *
					    pcmcia_tuple_read_n(tuple,
						hostaddrsize, idx);
					idx += hostaddrsize;
				} else {
					cfe->memspace[i].hostaddr = 0;
				}
			}
		}
	}

	if (misc) {
		if (tuple->length <= idx) {
			DPRINTF(("ran out of space before TCPE_MI\n"));
			goto abort_cfe;
		}

		reg = pcmcia_tuple_read_1(tuple, idx);
		idx++;

		cfe->flags &= ~(PCMCIA_CFE_POWERDOWN
		    | PCMCIA_CFE_READONLY
		    | PCMCIA_CFE_AUDIO);
		if (reg & PCMCIA_TPCE_MI_PWRDOWN)
			cfe->flags |= PCMCIA_CFE_POWERDOWN;
		if (reg & PCMCIA_TPCE_MI_READONLY)
			cfe->flags |= PCMCIA_CFE_READONLY;
		if (reg & PCMCIA_TPCE_MI_AUDIO)
			cfe->flags |= PCMCIA_CFE_AUDIO;
		cfe->maxtwins = reg & PCMCIA_TPCE_MI_MAXTWINS;

		while (reg & PCMCIA_TPCE_MI_EXT) {
			reg = pcmcia_tuple_read_1(tuple, idx);
			idx++;
		}
	}

	/* skip all the subtuples */
abort_cfe:
	DPRINTF(("CISTPL_CFTABLE_ENTRY\n"));
}
