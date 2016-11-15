/*	$NetBSD: acpi_pci_link.c,v 1.22 2014/09/14 19:54:05 mrg Exp $	*/

/*-
 * Copyright (c) 2002 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_pci_link.c,v 1.22 2014/09/14 19:54:05 mrg Exp $");

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/reboot.h>
#include <sys/systm.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/pci/pcireg.h>

#include "opt_acpi.h"


#define _COMPONENT          ACPI_BUS_COMPONENT
ACPI_MODULE_NAME            ("acpi_pci_link")

MALLOC_DECLARE(M_ACPI);

#define NUM_ISA_INTERRUPTS	16
#define NUM_ACPI_INTERRUPTS	256

#define PCI_INVALID_IRQ	255
#define PCI_INTERRUPT_VALID(x) ((x) != PCI_INVALID_IRQ && (x) != 0)

#define ACPI_SERIAL_BEGIN(x)
#define ACPI_SERIAL_END(x)

/*
 * An ACPI PCI link device may contain multiple links.  Each link has its
 * own ACPI resource.  _PRT entries specify which link is being used via
 * the Source Index.
 *
 * XXX: A note about Source Indices and DPFs:  Currently we assume that
 * the DPF start and end tags are not counted towards the index that
 * Source Index corresponds to.  Also, we assume that when DPFs are in use
 * they various sets overlap in terms of Indices.  Here's an example
 * resource list indicating these assumptions:
 *
 * Resource		Index
 * --------		-----
 * I/O Port		0
 * Start DPF		-
 * IRQ			1
 * MemIO		2
 * Start DPF		-
 * IRQ			1
 * MemIO		2
 * End DPF		-
 * DMA Channel		3
 *
 * The XXX is because I'm not sure if this is a valid assumption to make.
 */

/* States during DPF processing. */
#define	DPF_OUTSIDE	0
#define	DPF_FIRST	1
#define	DPF_IGNORE	2

struct link;

struct acpi_pci_link_softc {
	int	pl_num_links;
	int	pl_crs_bad;
	struct link *pl_links;
	char pl_name[32];
	ACPI_HANDLE pl_handle;
	TAILQ_ENTRY(acpi_pci_link_softc) pl_list;
};

static TAILQ_HEAD(, acpi_pci_link_softc) acpi_pci_linkdevs =
    TAILQ_HEAD_INITIALIZER(acpi_pci_linkdevs);


struct link {
	struct acpi_pci_link_softc *l_sc;
	uint8_t	l_bios_irq;
	uint8_t	l_irq;
	uint8_t l_trig;
	uint8_t l_pol;
	uint8_t	l_initial_irq;
	int	l_res_index;
	int	l_num_irqs;
	int	*l_irqs;
	int	l_references;
	int	l_dev_count;
	pcitag_t *l_devices;
	int	l_routed:1;
	int	l_isa_irq:1;
	ACPI_RESOURCE l_prs_template;
};

struct link_count_request {
	int	in_dpf;
	int	count;
};

struct link_res_request {
	struct acpi_pci_link_softc *sc;
	int	in_dpf;
	int	res_index;
	int	link_index;
};

static int pci_link_interrupt_weights[NUM_ACPI_INTERRUPTS];
static int pci_link_bios_isa_irqs;

static ACPI_STATUS acpi_count_irq_resources(ACPI_RESOURCE *, void *);
static ACPI_STATUS link_add_crs(ACPI_RESOURCE *, void *);
static ACPI_STATUS link_add_prs(ACPI_RESOURCE *, void *);
static int link_valid_irq(struct link *, int);
static void acpi_pci_link_dump(struct acpi_pci_link_softc *);
static int acpi_pci_link_attach(struct acpi_pci_link_softc *);
static uint8_t acpi_pci_link_search_irq(struct acpi_pci_link_softc *, int, int,
					int);
static struct link *acpi_pci_link_lookup(struct acpi_pci_link_softc *, int);
static ACPI_STATUS acpi_pci_link_srs(struct acpi_pci_link_softc *,
				     ACPI_BUFFER *);
static ACPI_STATUS acpi_AppendBufferResource(ACPI_BUFFER *, ACPI_RESOURCE *);

static ACPI_STATUS
acpi_count_irq_resources(ACPI_RESOURCE *res, void *context)
{
	struct link_count_request *req;

	req = (struct link_count_request *)context;
	switch (res->Type) {
	case ACPI_RESOURCE_TYPE_START_DEPENDENT:
		switch (req->in_dpf) {
		case DPF_OUTSIDE:
			/* We've started the first DPF. */
			req->in_dpf = DPF_FIRST;
			break;
		case DPF_FIRST:
			/* We've started the second DPF. */
			req->in_dpf = DPF_IGNORE;
			break;
		}
		break;
	case ACPI_RESOURCE_TYPE_END_DEPENDENT:
		/* We are finished with DPF parsing. */
		KASSERT(req->in_dpf != DPF_OUTSIDE);
		req->in_dpf = DPF_OUTSIDE;
		break;
	case ACPI_RESOURCE_TYPE_IRQ:
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		/*
		 * Don't count resources if we are in a DPF set that we are
		 * ignoring.
		 */
		if (req->in_dpf != DPF_IGNORE)
			req->count++;
	}
	return (AE_OK);
}

static ACPI_STATUS
link_add_crs(ACPI_RESOURCE *res, void *context)
{
	struct link_res_request *req;
	struct link *link;

	req = (struct link_res_request *)context;
	switch (res->Type) {
	case ACPI_RESOURCE_TYPE_START_DEPENDENT:
		switch (req->in_dpf) {
		case DPF_OUTSIDE:
			/* We've started the first DPF. */
			req->in_dpf = DPF_FIRST;
			break;
		case DPF_FIRST:
			/* We've started the second DPF. */
			panic(
		"%s: Multiple dependent functions within a current resource",
			    __func__);
			break;
		}
		break;
	case ACPI_RESOURCE_TYPE_END_DEPENDENT:
		/* We are finished with DPF parsing. */
		KASSERT(req->in_dpf != DPF_OUTSIDE);
		req->in_dpf = DPF_OUTSIDE;
		break;
	case ACPI_RESOURCE_TYPE_IRQ:
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		KASSERT(req->link_index < req->sc->pl_num_links);
		link = &req->sc->pl_links[req->link_index];
		link->l_res_index = req->res_index;
		req->link_index++;
		req->res_index++;

		/*
		 * Only use the current value if there's one IRQ.  Some
		 * systems return multiple IRQs (which is nonsense for _CRS)
		 * when the link hasn't been programmed.
		 */
		if (res->Type == ACPI_RESOURCE_TYPE_IRQ) {
			if (res->Data.Irq.InterruptCount == 1) {
				link->l_irq = res->Data.Irq.Interrupts[0];
				link->l_trig = res->Data.Irq.Triggering;
				link->l_pol = res->Data.Irq.Polarity;
			}
		} else if (res->Data.ExtendedIrq.InterruptCount == 1) {
			link->l_irq = res->Data.ExtendedIrq.Interrupts[0];
			link->l_trig = res->Data.ExtendedIrq.Triggering;
			link->l_pol = res->Data.ExtendedIrq.Polarity;
		}

		/*
		 * An IRQ of zero means that the link isn't routed.
		 */
		if (link->l_irq == 0)
			link->l_irq = PCI_INVALID_IRQ;
		break;
	default:
		req->res_index++;
	}
	return (AE_OK);
}

/*
 * Populate the set of possible IRQs for each device.
 */
static ACPI_STATUS
link_add_prs(ACPI_RESOURCE *res, void *context)
{
	struct link_res_request *req;
	struct link *link;
	uint8_t *irqs = NULL;
	uint32_t *ext_irqs = NULL;
	int i, is_ext_irq = 1;

	req = (struct link_res_request *)context;
	switch (res->Type) {
	case ACPI_RESOURCE_TYPE_START_DEPENDENT:
		switch (req->in_dpf) {
		case DPF_OUTSIDE:
			/* We've started the first DPF. */
			req->in_dpf = DPF_FIRST;
			break;
		case DPF_FIRST:
			/* We've started the second DPF. */
			req->in_dpf = DPF_IGNORE;
			break;
		}
		break;
	case ACPI_RESOURCE_TYPE_END_DEPENDENT:
		/* We are finished with DPF parsing. */
		KASSERT(req->in_dpf != DPF_OUTSIDE);
		req->in_dpf = DPF_OUTSIDE;
		break;
	case ACPI_RESOURCE_TYPE_IRQ:
		is_ext_irq = 0;
		/* fall through */
	case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
		/*
		 * Don't parse resources if we are in a DPF set that we are
		 * ignoring.
		 */
		if (req->in_dpf == DPF_IGNORE)
			break;

		KASSERT(req->link_index < req->sc->pl_num_links);
		link = &req->sc->pl_links[req->link_index];
		if (link->l_res_index == -1) {
			KASSERT(req->sc->pl_crs_bad);
			link->l_res_index = req->res_index;
		}
		req->link_index++;
		req->res_index++;

		/*
		 * Stash a copy of the resource for later use when
		 * doing _SRS.
		 *
		 * Note that in theory res->Length may exceed the size
		 * of ACPI_RESOURCE, due to variable length lists in
		 * subtypes.  However, all uses of l_prs_template only
		 * rely on lists lengths of zero or one, for which
		 * sizeof(ACPI_RESOURCE) is sufficient space anyway.
		 * We cannot read longer than Length bytes, in case we
		 * read off the end of mapped memory.  So we read
		 * whichever length is shortest, Length or
		 * sizeof(ACPI_RESOURCE).
		 */
		KASSERT(res->Length >= ACPI_RS_SIZE_MIN);

		memset(&link->l_prs_template, 0, sizeof(link->l_prs_template));
		memcpy(&link->l_prs_template, res,
		       MIN(res->Length, sizeof(link->l_prs_template)));

		if (is_ext_irq) {
			link->l_num_irqs =
			    res->Data.ExtendedIrq.InterruptCount;
			link->l_trig = res->Data.ExtendedIrq.Triggering;
			link->l_pol = res->Data.ExtendedIrq.Polarity;
			ext_irqs = res->Data.ExtendedIrq.Interrupts;
		} else {
			link->l_num_irqs = res->Data.Irq.InterruptCount;
			link->l_trig = res->Data.Irq.Triggering;
			link->l_pol = res->Data.Irq.Polarity;
			irqs = res->Data.Irq.Interrupts;
		}
		if (link->l_num_irqs == 0)
			break;

		/*
		 * Save a list of the valid IRQs.  Also, if all of the
		 * valid IRQs are ISA IRQs, then mark this link as
		 * routed via an ISA interrupt.
		 */
		link->l_isa_irq = TRUE;
		link->l_irqs = malloc(sizeof(int) * link->l_num_irqs,
		    M_ACPI, M_WAITOK | M_ZERO);
		for (i = 0; i < link->l_num_irqs; i++) {
			if (is_ext_irq) {
				link->l_irqs[i] = ext_irqs[i];
				if (ext_irqs[i] >= NUM_ISA_INTERRUPTS)
					link->l_isa_irq = FALSE;
			} else {
				link->l_irqs[i] = irqs[i];
				if (irqs[i] >= NUM_ISA_INTERRUPTS)
					link->l_isa_irq = FALSE;
			}
		}
		break;
	default:
		if (req->in_dpf == DPF_IGNORE)
			break;
		if (req->sc->pl_crs_bad)
			aprint_normal("%s: Warning: possible resource %d "
			       "will be lost during _SRS\n", req->sc->pl_name,
			       req->res_index);
		req->res_index++;
	}
	return (AE_OK);
}

static int
link_valid_irq(struct link *link, int irq)
{
	int i;

	/* Invalid interrupts are never valid. */
	if (!PCI_INTERRUPT_VALID(irq))
		return (FALSE);

	/* Any interrupt in the list of possible interrupts is valid. */
	for (i = 0; i < link->l_num_irqs; i++)
		if (link->l_irqs[i] == irq)
			 return (TRUE);

	/*
	 * For links routed via an ISA interrupt, if the SCI is routed via
	 * an ISA interrupt, the SCI is always treated as a valid IRQ.
	 */
	if (link->l_isa_irq && AcpiGbl_FADT.SciInterrupt == irq &&
	    irq < NUM_ISA_INTERRUPTS)
		return (TRUE);

	/* If the interrupt wasn't found in the list it is not valid. */
	return (FALSE);
}

void
acpi_pci_link_state(void)
{
	struct acpi_pci_link_softc *sc;

	TAILQ_FOREACH(sc, &acpi_pci_linkdevs, pl_list) {
		acpi_pci_link_dump(sc);
	}
}

static void
acpi_pci_link_dump(struct acpi_pci_link_softc *sc)
{
	struct link *link;
	int i, j;

	printf("Link Device %s:\n", sc->pl_name);
	printf("Index  IRQ  Rtd  Ref  IRQs\n");
	for (i = 0; i < sc->pl_num_links; i++) {
		link = &sc->pl_links[i];
		printf("%5d  %3d   %c   %3d ", i, link->l_irq,
		    link->l_routed ? 'Y' : 'N',  link->l_references);
		if (link->l_num_irqs == 0)
			printf(" none");
		else for (j = 0; j < link->l_num_irqs; j++)
			printf(" %d", link->l_irqs[j]);
		printf(" polarity %u trigger %u\n", link->l_pol, link->l_trig);
	}
	printf("\n");
}

static int
acpi_pci_link_attach(struct acpi_pci_link_softc *sc)
{
	struct link_count_request creq;
	struct link_res_request rreq;
	ACPI_STATUS status;
	int i;

	ACPI_SERIAL_BEGIN(pci_link);

	/*
	 * Count the number of current resources so we know how big of
	 * a link array to allocate.  On some systems, _CRS is broken,
	 * so for those systems try to derive the count from _PRS instead.
	 */
	creq.in_dpf = DPF_OUTSIDE;
	creq.count = 0;
	status = AcpiWalkResources(sc->pl_handle, "_CRS",
	    acpi_count_irq_resources, &creq);
	sc->pl_crs_bad = ACPI_FAILURE(status);
	if (sc->pl_crs_bad) {
		creq.in_dpf = DPF_OUTSIDE;
		creq.count = 0;
		status = AcpiWalkResources(sc->pl_handle, "_PRS",
		    acpi_count_irq_resources, &creq);
		if (ACPI_FAILURE(status)) {
			aprint_error("%s: Unable to parse _CRS or _PRS: %s\n",
			    sc->pl_name, AcpiFormatException(status));
			ACPI_SERIAL_END(pci_link);
			return (ENXIO);
		}
	}
	sc->pl_num_links = creq.count;
	if (creq.count == 0) {
		ACPI_SERIAL_END(pci_link);
		return (0);
	}
	sc->pl_links = malloc(sizeof(struct link) * sc->pl_num_links,
	    M_ACPI, M_WAITOK | M_ZERO);

	/* Initialize the child links. */
	for (i = 0; i < sc->pl_num_links; i++) {
		sc->pl_links[i].l_irq = PCI_INVALID_IRQ;
		sc->pl_links[i].l_bios_irq = PCI_INVALID_IRQ;
		sc->pl_links[i].l_sc = sc;
		sc->pl_links[i].l_isa_irq = FALSE;
		sc->pl_links[i].l_res_index = -1;
		sc->pl_links[i].l_dev_count = 0;
		sc->pl_links[i].l_devices = NULL;
	}

	/* Try to read the current settings from _CRS if it is valid. */
	if (!sc->pl_crs_bad) {
		rreq.in_dpf = DPF_OUTSIDE;
		rreq.link_index = 0;
		rreq.res_index = 0;
		rreq.sc = sc;
		status = AcpiWalkResources(sc->pl_handle, "_CRS",
		    link_add_crs, &rreq);
		if (ACPI_FAILURE(status)) {
			aprint_error("%s: Unable to parse _CRS: %s\n",
			    sc->pl_name, AcpiFormatException(status));
			goto fail;
		}
	}

	/*
	 * Try to read the possible settings from _PRS.  Note that if the
	 * _CRS is toast, we depend on having a working _PRS.  However, if
	 * _CRS works, then it is ok for _PRS to be missing.
	 */
	rreq.in_dpf = DPF_OUTSIDE;
	rreq.link_index = 0;
	rreq.res_index = 0;
	rreq.sc = sc;
	status = AcpiWalkResources(sc->pl_handle, "_PRS",
	    link_add_prs, &rreq);
	if (ACPI_FAILURE(status) &&
	    (status != AE_NOT_FOUND || sc->pl_crs_bad)) {
		aprint_error("%s: Unable to parse _PRS: %s\n",
		    sc->pl_name, AcpiFormatException(status));
		goto fail;
	}
	if (boothowto & AB_VERBOSE) {
		aprint_normal("%s: Links after initial probe:\n", sc->pl_name);
		acpi_pci_link_dump(sc);
	}

	/* Verify initial IRQs if we have _PRS. */
	if (status != AE_NOT_FOUND)
		for (i = 0; i < sc->pl_num_links; i++)
			if (!link_valid_irq(&sc->pl_links[i],
			    sc->pl_links[i].l_irq))
				sc->pl_links[i].l_irq = PCI_INVALID_IRQ;
	if (boothowto & AB_VERBOSE) {
		printf("%s: Links after initial validation:\n", sc->pl_name);
		acpi_pci_link_dump(sc);
	}

	/* Save initial IRQs. */
	for (i = 0; i < sc->pl_num_links; i++)
		sc->pl_links[i].l_initial_irq = sc->pl_links[i].l_irq;

	/*
	 * Try to disable this link.  If successful, set the current IRQ to
	 * zero and flags to indicate this link is not routed.  If we can't
	 * run _DIS (i.e., the method doesn't exist), assume the initial
	 * IRQ was routed by the BIOS.
	 */
#ifndef ACPI__DIS_IS_BROKEN
	if (ACPI_SUCCESS(AcpiEvaluateObject(sc->pl_handle, "_DIS", NULL,
	    NULL)))
		for (i = 0; i < sc->pl_num_links; i++)
			sc->pl_links[i].l_irq = PCI_INVALID_IRQ;
	else
#endif
		for (i = 0; i < sc->pl_num_links; i++)
			if (PCI_INTERRUPT_VALID(sc->pl_links[i].l_irq))
				sc->pl_links[i].l_routed = TRUE;
	if (boothowto & AB_VERBOSE) {
		printf("%s: Links after disable:\n", sc->pl_name);
		acpi_pci_link_dump(sc);
	}
	ACPI_SERIAL_END(pci_link);
	return (0);
fail:
	ACPI_SERIAL_END(pci_link);
	for (i = 0; i < sc->pl_num_links; i++) {
		if (sc->pl_links[i].l_irqs != NULL)
			free(sc->pl_links[i].l_irqs, M_ACPI);
		if (sc->pl_links[i].l_devices != NULL)
			free(sc->pl_links[i].l_devices, M_ACPI);
	}
	free(sc->pl_links, M_ACPI);
	return (ENXIO);
}

static void
acpi_pci_link_add_functions(struct acpi_pci_link_softc *sc, struct link *link,
    int bus, int device, int pin)
{
	uint32_t value;
	uint8_t func, maxfunc, ipin;
	pcitag_t tag;

	tag = pci_make_tag(acpi_softc->sc_pc, bus, device, 0);
	/* See if we have a valid device at function 0. */
	value = pci_conf_read(acpi_softc->sc_pc, tag,  PCI_BHLC_REG);
	if (PCI_HDRTYPE_TYPE(value) > PCI_HDRTYPE_PCB)
		return;
	if (PCI_HDRTYPE_MULTIFN(value))
		maxfunc = 7;
	else
		maxfunc = 0;

	/* Scan all possible functions at this device. */
	for (func = 0; func <= maxfunc; func++) {
		tag = pci_make_tag(acpi_softc->sc_pc, bus, device, func);
		value = pci_conf_read(acpi_softc->sc_pc, tag, PCI_ID_REG);
		if (PCI_VENDOR(value) == 0xffff)
			continue;
		value = pci_conf_read(acpi_softc->sc_pc, tag,
		    PCI_INTERRUPT_REG);
		ipin = PCI_INTERRUPT_PIN(value);
		/*
		 * See if it uses the pin in question.  Note that the passed
		 * in pin uses 0 for A, .. 3 for D whereas the intpin
		 * register uses 0 for no interrupt, 1 for A, .. 4 for D.
		 */
		if (ipin != pin + 1)
			continue;

		link->l_devices = realloc(link->l_devices,
		    sizeof(pcitag_t) * (link->l_dev_count + 1),
		    M_ACPI, M_WAITOK);
		link->l_devices[link->l_dev_count] = tag;
		++link->l_dev_count;
	}
}

static uint8_t
acpi_pci_link_search_irq(struct acpi_pci_link_softc *sc, int bus, int device,
			 int pin)
{
	uint32_t value;
	uint8_t func, maxfunc, ipin, iline;
	pcitag_t tag;

	tag = pci_make_tag(acpi_softc->sc_pc, bus, device, 0);
	/* See if we have a valid device at function 0. */
	value = pci_conf_read(acpi_softc->sc_pc, tag,  PCI_BHLC_REG);
	if (PCI_HDRTYPE_TYPE(value) > PCI_HDRTYPE_PCB)
		return (PCI_INVALID_IRQ);
	if (PCI_HDRTYPE_MULTIFN(value))
		maxfunc = 7;
	else
		maxfunc = 0;

	/* Scan all possible functions at this device. */
	for (func = 0; func <= maxfunc; func++) {
		tag = pci_make_tag(acpi_softc->sc_pc, bus, device, func);
		value = pci_conf_read(acpi_softc->sc_pc, tag, PCI_ID_REG);
		if (PCI_VENDOR(value) == 0xffff)
			continue;
		value = pci_conf_read(acpi_softc->sc_pc, tag,
		    PCI_INTERRUPT_REG);
		ipin = PCI_INTERRUPT_PIN(value);
		iline = PCI_INTERRUPT_LINE(value);

		/*
		 * See if it uses the pin in question.  Note that the passed
		 * in pin uses 0 for A, .. 3 for D whereas the intpin
		 * register uses 0 for no interrupt, 1 for A, .. 4 for D.
		 */
		if (ipin != pin + 1)
			continue;
		aprint_verbose(
		    "%s: ACPI: Found matching pin for %d.%d.INT%c"
	            " at func %d: %d\n",
			    sc->pl_name, bus, device, pin + 'A', func, iline);
		if (PCI_INTERRUPT_VALID(iline))
			return (iline);
	}
	return (PCI_INVALID_IRQ);
}

/*
 * Find the link structure that corresponds to the resource index passed in
 * via 'source_index'.
 */
static struct link *
acpi_pci_link_lookup(struct acpi_pci_link_softc *sc, int source_index)
{
	int i;

	for (i = 0; i < sc->pl_num_links; i++)
		if (sc->pl_links[i].l_res_index == source_index)
			return (&sc->pl_links[i]);
	return (NULL);
}

void
acpi_pci_link_add_reference(void *v, int index, int bus, int slot, int pin)
{
	struct acpi_pci_link_softc *sc = v;
	struct link *link;
	uint8_t bios_irq;

	/* Bump the reference count. */
	ACPI_SERIAL_BEGIN(pci_link);
	link = acpi_pci_link_lookup(sc, index);
	if (link == NULL) {
		printf("%s: apparently invalid index %d\n", sc->pl_name, index);
		ACPI_SERIAL_END(pci_link);
		return;
	}
	link->l_references++;
	acpi_pci_link_add_functions(sc, link, bus, slot, pin);
	if (link->l_routed)
		pci_link_interrupt_weights[link->l_irq]++;

	/*
	 * The BIOS only routes interrupts via ISA IRQs using the ATPICs
	 * (8259As).  Thus, if this link is routed via an ISA IRQ, go
	 * look to see if the BIOS routed an IRQ for this link at the
	 * indicated (bus, slot, pin).  If so, we prefer that IRQ for
	 * this link and add that IRQ to our list of known-good IRQs.
	 * This provides a good work-around for link devices whose _CRS
	 * method is either broken or bogus.  We only use the value
	 * returned by _CRS if we can't find a valid IRQ via this method
	 * in fact.
	 *
	 * If this link is not routed via an ISA IRQ (because we are using
	 * APIC for example), then don't bother looking up the BIOS IRQ
	 * as if we find one it won't be valid anyway.
	 */
	if (!link->l_isa_irq) {
		ACPI_SERIAL_END(pci_link);
		return;
	}

	/* Try to find a BIOS IRQ setting from any matching devices. */
	bios_irq = acpi_pci_link_search_irq(sc, bus, slot, pin);
	if (!PCI_INTERRUPT_VALID(bios_irq)) {
		ACPI_SERIAL_END(pci_link);
		return;
	}

	/* Validate the BIOS IRQ. */
	if (!link_valid_irq(link, bios_irq)) {
		printf("%s: BIOS IRQ %u for %d.%d.INT%c is invalid\n",
		    sc->pl_name, bios_irq, (int)bus, slot, pin + 'A');
	} else if (!PCI_INTERRUPT_VALID(link->l_bios_irq)) {
		link->l_bios_irq = bios_irq;
		if (bios_irq < NUM_ISA_INTERRUPTS)
			pci_link_bios_isa_irqs |= (1 << bios_irq);
		if (bios_irq != link->l_initial_irq &&
		    PCI_INTERRUPT_VALID(link->l_initial_irq))
			printf(
			    "%s: BIOS IRQ %u does not match initial IRQ %u\n",
			    sc->pl_name, bios_irq, link->l_initial_irq);
	} else if (bios_irq != link->l_bios_irq)
		printf(
	    "%s: BIOS IRQ %u for %d.%d.INT%c does not match "
	    "previous BIOS IRQ %u\n",
		    sc->pl_name, bios_irq, (int)bus, slot, pin + 'A',
		    link->l_bios_irq);
	ACPI_SERIAL_END(pci_link);
}

static ACPI_STATUS
acpi_pci_link_srs_from_crs(struct acpi_pci_link_softc *sc, ACPI_BUFFER *srsbuf)
{
	ACPI_RESOURCE *resource, *end, newres, *resptr;
	ACPI_BUFFER crsbuf;
	ACPI_STATUS status;
	struct link *link;
	int i, in_dpf;

	/* Fetch the _CRS. */
	crsbuf.Pointer = NULL;
	crsbuf.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
	status = AcpiGetCurrentResources(sc->pl_handle, &crsbuf);
	if (ACPI_SUCCESS(status) && crsbuf.Pointer == NULL)
		status = AE_NO_MEMORY;
	if (ACPI_FAILURE(status)) {
		aprint_verbose("%s: Unable to fetch current resources: %s\n",
		    sc->pl_name, AcpiFormatException(status));
		return (status);
	}

	/* Fill in IRQ resources via link structures. */
	srsbuf->Pointer = NULL;
	link = sc->pl_links;
	i = 0;
	in_dpf = DPF_OUTSIDE;
	resource = (ACPI_RESOURCE *)crsbuf.Pointer;
	end = (ACPI_RESOURCE *)((char *)crsbuf.Pointer + crsbuf.Length);
	for (;;) {
		switch (resource->Type) {
		case ACPI_RESOURCE_TYPE_START_DEPENDENT:
			switch (in_dpf) {
			case DPF_OUTSIDE:
				/* We've started the first DPF. */
				in_dpf = DPF_FIRST;
				break;
			case DPF_FIRST:
				/* We've started the second DPF. */
				panic(
		"%s: Multiple dependent functions within a current resource",
				    __func__);
				break;
			}
			resptr = NULL;
			break;
		case ACPI_RESOURCE_TYPE_END_DEPENDENT:
			/* We are finished with DPF parsing. */
			KASSERT(in_dpf != DPF_OUTSIDE);
			in_dpf = DPF_OUTSIDE;
			resptr = NULL;
			break;
		case ACPI_RESOURCE_TYPE_IRQ:
			newres = link->l_prs_template;
			resptr = &newres;
			resptr->Data.Irq.InterruptCount = 1;
			if (PCI_INTERRUPT_VALID(link->l_irq)) {
				KASSERT(link->l_irq < NUM_ISA_INTERRUPTS);
				resptr->Data.Irq.Interrupts[0] = link->l_irq;
				resptr->Data.Irq.Triggering = link->l_trig;
				resptr->Data.Irq.Polarity = link->l_pol;
			} else
				resptr->Data.Irq.Interrupts[0] = 0;
			link++;
			i++;
			break;
		case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
			newres = link->l_prs_template;
			resptr = &newres;
			resptr->Data.ExtendedIrq.InterruptCount = 1;
			if (PCI_INTERRUPT_VALID(link->l_irq)) {
				resptr->Data.ExtendedIrq.Interrupts[0] =
				    link->l_irq;
				resptr->Data.ExtendedIrq.Triggering =
				    link->l_trig;
				resptr->Data.ExtendedIrq.Polarity = link->l_pol;
			} else
				resptr->Data.ExtendedIrq.Interrupts[0] = 0;
			link++;
			i++;
			break;
		default:
			resptr = resource;
		}
		if (resptr != NULL) {
			status = acpi_AppendBufferResource(srsbuf, resptr);
			if (ACPI_FAILURE(status)) {
				printf("%s: Unable to build resources: %s\n",
				    sc->pl_name, AcpiFormatException(status));
				if (srsbuf->Pointer != NULL)
					ACPI_FREE(srsbuf->Pointer);
				ACPI_FREE(crsbuf.Pointer);
				return (status);
			}
		}
		if (resource->Type == ACPI_RESOURCE_TYPE_END_TAG)
			break;
		resource = ACPI_NEXT_RESOURCE(resource);
		if (resource >= end)
			break;
	}
	ACPI_FREE(crsbuf.Pointer);
	return (AE_OK);
}

static ACPI_STATUS
acpi_pci_link_srs_from_links(struct acpi_pci_link_softc *sc,
    ACPI_BUFFER *srsbuf)
{
	ACPI_RESOURCE newres;
	ACPI_STATUS status;
	struct link *link;
	int i;

	/* Start off with an empty buffer. */
	srsbuf->Pointer = NULL;
	link = sc->pl_links;
	for (i = 0; i < sc->pl_num_links; i++) {

		/* Add a new IRQ resource from each link. */
		link = &sc->pl_links[i];
		newres = link->l_prs_template;
		if (newres.Type == ACPI_RESOURCE_TYPE_IRQ) {

			/* Build an IRQ resource. */
			newres.Data.Irq.InterruptCount = 1;
			if (PCI_INTERRUPT_VALID(link->l_irq)) {
				KASSERT(link->l_irq < NUM_ISA_INTERRUPTS);
				newres.Data.Irq.Interrupts[0] = link->l_irq;
				newres.Data.Irq.Triggering = link->l_trig;
				newres.Data.Irq.Polarity = link->l_pol;
			} else
				newres.Data.Irq.Interrupts[0] = 0;
		} else {

			/* Build an ExtIRQ resuorce. */
			newres.Data.ExtendedIrq.InterruptCount = 1;
			if (PCI_INTERRUPT_VALID(link->l_irq)) {
				newres.Data.ExtendedIrq.Interrupts[0] =
				    link->l_irq;
				newres.Data.ExtendedIrq.Triggering =
				    link->l_trig;
				newres.Data.ExtendedIrq.Polarity =
				    link->l_pol;
			} else {
				newres.Data.ExtendedIrq.Interrupts[0] = 0;
			}
		}

		/* Add the new resource to the end of the _SRS buffer. */
		status = acpi_AppendBufferResource(srsbuf, &newres);
		if (ACPI_FAILURE(status)) {
			printf("%s: Unable to build resources: %s\n",
			    sc->pl_name, AcpiFormatException(status));
			if (srsbuf->Pointer != NULL)
				ACPI_FREE(srsbuf->Pointer);
			return (status);
		}
	}
	return (AE_OK);
}

static ACPI_STATUS
acpi_pci_link_srs(struct acpi_pci_link_softc *sc, ACPI_BUFFER *srsbuf)
{
	ACPI_STATUS status;

	if (sc->pl_crs_bad)
		status = acpi_pci_link_srs_from_links(sc, srsbuf);
	else
		status = acpi_pci_link_srs_from_crs(sc, srsbuf);

	if (ACPI_FAILURE(status))
		printf("%s: Unable to find link srs : %s\n",
		    sc->pl_name, AcpiFormatException(status));

	/* Write out new resources via _SRS. */
	return AcpiSetCurrentResources(sc->pl_handle, srsbuf);
}

static ACPI_STATUS
acpi_pci_link_route_irqs(struct acpi_pci_link_softc *sc, int *irq, int *pol,
			 int *trig)
{
	ACPI_RESOURCE *resource, *end;
	ACPI_BUFFER srsbuf;
	ACPI_STATUS status;
	struct link *link;
	int i, is_ext = 0;

	status = acpi_pci_link_srs(sc, &srsbuf);
	if (ACPI_FAILURE(status)) {
		printf("%s: _SRS failed: %s\n",
		    sc->pl_name, AcpiFormatException(status));
		return (status);
	}
	/*
	 * Perform acpi_config_intr() on each IRQ resource if it was just
	 * routed for the first time.
	 */
	link = sc->pl_links;
	i = 0;
	resource = (ACPI_RESOURCE *)srsbuf.Pointer;
	end = (ACPI_RESOURCE *)((char *)srsbuf.Pointer + srsbuf.Length);
	for (;;) {
		if (resource->Type == ACPI_RESOURCE_TYPE_END_TAG)
			break;
		switch (resource->Type) {
		case ACPI_RESOURCE_TYPE_EXTENDED_IRQ:
			is_ext = 1;
			/* FALLTHROUGH */
		case ACPI_RESOURCE_TYPE_IRQ:
			/*
			 * Only configure the interrupt and update the
			 * weights if this link has a valid IRQ and was
			 * previously unrouted.
			 */
			if (!link->l_routed &&
			    PCI_INTERRUPT_VALID(link->l_irq)) {
				*trig = is_ext ?
				    resource->Data.ExtendedIrq.Triggering :
				    resource->Data.Irq.Triggering;
				*pol = is_ext ?
				    resource->Data.ExtendedIrq.Polarity :
				    resource->Data.Irq.Polarity;
				*irq = is_ext ?
				    resource->Data.ExtendedIrq.Interrupts[0] :
				    resource->Data.Irq.Interrupts[0];
				link->l_routed = TRUE;
				pci_link_interrupt_weights[link->l_irq] +=
				    link->l_references;
			}
			link++;
			i++;
			break;
		}
		resource = ACPI_NEXT_RESOURCE(resource);
		if (resource >= end)
			break;
	}
	ACPI_FREE(srsbuf.Pointer);
	return (AE_OK);
}

/*
 * Pick an IRQ to use for this unrouted link.
 */
static uint8_t
acpi_pci_link_choose_irq(struct acpi_pci_link_softc *sc, struct link *link)
{
	u_int8_t best_irq, pos_irq;
	int best_weight, pos_weight, i;

	KASSERT(!link->l_routed);
	KASSERT(!PCI_INTERRUPT_VALID(link->l_irq));

	/*
	 * If we have a valid BIOS IRQ, use that.  We trust what the BIOS
	 * says it routed over what _CRS says the link thinks is routed.
	 */
	if (PCI_INTERRUPT_VALID(link->l_bios_irq))
		return (link->l_bios_irq);

	/*
	 * If we don't have a BIOS IRQ but do have a valid IRQ from _CRS,
	 * then use that.
	 */
	if (PCI_INTERRUPT_VALID(link->l_initial_irq))
		return (link->l_initial_irq);

	/*
	 * Ok, we have no useful hints, so we have to pick from the
	 * possible IRQs.  For ISA IRQs we only use interrupts that
	 * have already been used by the BIOS.
	 */
	best_irq = PCI_INVALID_IRQ;
	best_weight = INT_MAX;
	for (i = 0; i < link->l_num_irqs; i++) {
		pos_irq = link->l_irqs[i];
		if (pos_irq < NUM_ISA_INTERRUPTS &&
		    (pci_link_bios_isa_irqs & 1 << pos_irq) == 0)
			continue;
		pos_weight = pci_link_interrupt_weights[pos_irq];
		if (pos_weight < best_weight) {
			best_weight = pos_weight;
			best_irq = pos_irq;
		}
	}

	/*
	 * If this is an ISA IRQ, try using the SCI if it is also an ISA
	 * interrupt as a fallback.
	 */
	if (link->l_isa_irq && !PCI_INTERRUPT_VALID(best_irq)) {
		pos_irq = AcpiGbl_FADT.SciInterrupt;
		pos_weight = pci_link_interrupt_weights[pos_irq];
		if (pos_weight < best_weight) {
			best_weight = pos_weight;
			best_irq = pos_irq;
		}
	}

	if (PCI_INTERRUPT_VALID(best_irq)) {
		aprint_verbose("%s: Picked IRQ %u with weight %d\n",
		    sc->pl_name, best_irq, best_weight);
	} else
		printf("%s: Unable to choose an IRQ\n", sc->pl_name);
	return (best_irq);
}

int
acpi_pci_link_route_interrupt(void *v, int index, int *irq, int *pol, int *trig)
{
	struct acpi_pci_link_softc *sc = v;
	struct link *link;
	int i;
	pcireg_t reg;

	ACPI_SERIAL_BEGIN(pci_link);
	link = acpi_pci_link_lookup(sc, index);
	if (link == NULL)
		panic("%s: apparently invalid index %d", __func__, index);

	/*
	 * If this link device is already routed to an interrupt, just return
	 * the interrupt it is routed to.
	 */
	if (link->l_routed) {
		KASSERT(PCI_INTERRUPT_VALID(link->l_irq));
		ACPI_SERIAL_END(pci_link);
		*irq = link->l_irq;
		*pol = link->l_pol;
		*trig = link->l_trig;
		return (link->l_irq);
	}

	/* Choose an IRQ if we need one. */
	if (PCI_INTERRUPT_VALID(link->l_irq)) {
		*irq = link->l_irq;
		*pol = link->l_pol;
		*trig = link->l_trig;
		goto done;
	}

	link->l_irq = acpi_pci_link_choose_irq(sc, link);

	/*
	 * Try to route the interrupt we picked.  If it fails, then
	 * assume the interrupt is not routed.
	 */
	if (!PCI_INTERRUPT_VALID(link->l_irq))
		goto done;

	acpi_pci_link_route_irqs(sc, irq, pol, trig);
	if (!link->l_routed) {
		link->l_irq = PCI_INVALID_IRQ;
		goto done;
	}

	link->l_pol = *pol;
	link->l_trig = *trig;
	for (i = 0; i < link->l_dev_count; ++i) {
		reg = pci_conf_read(acpi_softc->sc_pc, link->l_devices[i],
		    PCI_INTERRUPT_REG);
		reg &= ~(PCI_INTERRUPT_LINE_MASK << PCI_INTERRUPT_LINE_SHIFT);
		reg |= link->l_irq << PCI_INTERRUPT_LINE_SHIFT;
		pci_conf_write(acpi_softc->sc_pc, link->l_devices[i],
		    PCI_INTERRUPT_REG, reg);
	}

done:
	ACPI_SERIAL_END(pci_link);

	return (link->l_irq);
}

/*
 * This is gross, but we abuse the identify routine to perform one-time
 * SYSINIT() style initialization for the driver.
 */
static void
acpi_pci_link_init(struct acpi_pci_link_softc *sc)
{
	ACPI_BUFFER buf;

	/*
	 * If the SCI is an ISA IRQ, add it to the bitmask of known good
	 * ISA IRQs.
	 *
	 * XXX: If we are using the APIC, the SCI might have been
	 * rerouted to an APIC pin in which case this is invalid.  However,
	 * if we are using the APIC, we also shouldn't be having any PCI
	 * interrupts routed via ISA IRQs, so this is probably ok.
	 */
	if (AcpiGbl_FADT.SciInterrupt < NUM_ISA_INTERRUPTS)
		pci_link_bios_isa_irqs |= (1 << AcpiGbl_FADT.SciInterrupt);

	buf.Length = sizeof (sc->pl_name);
	buf.Pointer = sc->pl_name;

	if (ACPI_FAILURE(AcpiGetName(sc->pl_handle, ACPI_SINGLE_NAME, &buf)))
		snprintf(sc->pl_name, sizeof (sc->pl_name), "%s",
		    "ACPI link device");

	acpi_pci_link_attach(sc);
}

void *
acpi_pci_link_devbyhandle(ACPI_HANDLE handle)
{
	struct acpi_pci_link_softc *sc;

	TAILQ_FOREACH(sc, &acpi_pci_linkdevs, pl_list) {
		if (sc->pl_handle == handle)
			return sc;
	}

	sc = malloc(sizeof (*sc), M_ACPI, M_NOWAIT | M_ZERO);
	if (sc == NULL)
		return NULL;

	sc->pl_handle = handle;

	acpi_pci_link_init(sc);

	TAILQ_INSERT_TAIL(&acpi_pci_linkdevs, sc, pl_list);

	return (void *)sc;
}

void
acpi_pci_link_resume(void)
{
	struct acpi_pci_link_softc *sc;
	ACPI_BUFFER srsbuf;

	TAILQ_FOREACH(sc, &acpi_pci_linkdevs, pl_list) {
		ACPI_SERIAL_BEGIN(pci_link);
		if (ACPI_SUCCESS(acpi_pci_link_srs(sc, &srsbuf)))
			ACPI_FREE(srsbuf.Pointer);
		ACPI_SERIAL_END(pci_link);
	}
}

ACPI_HANDLE
acpi_pci_link_handle(void *v)
{
	struct acpi_pci_link_softc *sc = v;

	return sc->pl_handle;
}

char *
acpi_pci_link_name(void *v)
{
	struct acpi_pci_link_softc *sc = v;

	return sc->pl_name;
}


/*
 * Append an ACPI_RESOURCE to an ACPI_BUFFER.
 *
 * Given a pointer to an ACPI_RESOURCE structure, expand the ACPI_BUFFER
 * provided to contain it.  If the ACPI_BUFFER is empty, allocate a sensible
 * backing block.  If the ACPI_RESOURCE is NULL, return an empty set of
 * resources.
 */
#define ACPI_INITIAL_RESOURCE_BUFFER_SIZE	512

static ACPI_STATUS
acpi_AppendBufferResource(ACPI_BUFFER *buf, ACPI_RESOURCE *res)
{
	ACPI_RESOURCE	*rp;
	void		*newp;

	/* Initialise the buffer if necessary. */
	if (buf->Pointer == NULL) {
	buf->Length = ACPI_INITIAL_RESOURCE_BUFFER_SIZE;
	if ((buf->Pointer = ACPI_ALLOCATE(buf->Length)) == NULL)
		return (AE_NO_MEMORY);
	rp = (ACPI_RESOURCE *)buf->Pointer;
	rp->Type =  ACPI_RESOURCE_TYPE_END_TAG;
	rp->Length = 0;
	}

	if (res == NULL)
		return (AE_OK);

	/*
	 * Scan the current buffer looking for the terminator.
	 * This will either find the terminator or hit the end
	 * of the buffer and return an error.
	 */
	rp = (ACPI_RESOURCE *)buf->Pointer;
	for (;;) {
		/* Range check, don't go outside the buffer */
		if (rp >= (ACPI_RESOURCE *)((u_int8_t *)buf->Pointer +
		    buf->Length))
			return (AE_BAD_PARAMETER);
		if (rp->Type ==  ACPI_RESOURCE_TYPE_END_TAG || rp->Length == 0)
			break;
		rp = ACPI_NEXT_RESOURCE(rp);
	}

	/*
	 * Check the size of the buffer and expand if required.
	 *
	 * Required size is:
	 *	size of existing resources before terminator + 
	 *	size of new resource and header +
	 * 	size of terminator.
	 *
	 * Note that this loop should really only run once, unless
	 * for some reason we are stuffing a *really* huge resource.
	 */
	while ((((u_int8_t *)rp - (u_int8_t *)buf->Pointer) + 
	    res->Length + ACPI_RS_SIZE_NO_DATA +
	    ACPI_RS_SIZE_MIN) >= buf->Length) {
		if ((newp = ACPI_ALLOCATE(buf->Length * 2)) == NULL)
			return (AE_NO_MEMORY);
		memcpy(newp, buf->Pointer, buf->Length);
		rp = (ACPI_RESOURCE *)((u_int8_t *)newp +
		   ((u_int8_t *)rp - (u_int8_t *)buf->Pointer));
		ACPI_FREE(buf->Pointer);
		buf->Pointer = newp;
		buf->Length += buf->Length;
	}

	/* Insert the new resource. */
	memcpy(rp, res, res->Length);

	/* And add the terminator. */
	rp = ACPI_NEXT_RESOURCE(rp);
	rp->Type =  ACPI_RESOURCE_TYPE_END_TAG;
	rp->Length = 0;

	return (AE_OK);
}
