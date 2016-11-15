/* $NetBSD: fdc_acpi.c,v 1.43 2015/04/13 16:33:23 riastradh Exp $ */

/*
 * Copyright (c) 2002 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * ACPI attachment for the PC Floppy Controller driver, based on
 * sys/arch/i386/pnpbios/fdc_pnpbios.c by Jason R. Thorpe
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fdc_acpi.c,v 1.43 2015/04/13 16:33:23 riastradh Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/systm.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/isa/isadmavar.h>
#include <dev/isa/fdcvar.h>
#include <dev/isa/fdvar.h>
#include <dev/isa/fdreg.h>

#include <dev/acpi/fdc_acpireg.h>

#define _COMPONENT          ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME            ("fdc_acpi")

static int	fdc_acpi_match(device_t, cfdata_t, void *);
static void	fdc_acpi_attach(device_t, device_t, void *);

struct fdc_acpi_softc {
	struct fdc_softc sc_fdc;
	bus_space_handle_t sc_baseioh;
	struct acpi_devnode *sc_node;	/* ACPI devnode */
};

static int	fdc_acpi_enumerate(struct fdc_acpi_softc *);
static void	fdc_acpi_getknownfds(struct fdc_acpi_softc *);

static const struct fd_type *fdc_acpi_nvtotype(const char *, int, int);

CFATTACH_DECL_NEW(fdc_acpi, sizeof(struct fdc_acpi_softc), fdc_acpi_match,
    fdc_acpi_attach, NULL, NULL);

/*
 * Supported device IDs
 */

static const char * const fdc_acpi_ids[] = {
	"PNP07??",	/* PC standard floppy disk controller */
	NULL
};

/*
 * fdc_acpi_match: autoconf(9) match routine
 */
static int
fdc_acpi_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	return acpi_match_hid(aa->aa_node->ad_devinfo, fdc_acpi_ids);
}

/*
 * fdc_acpi_attach: autoconf(9) attach routine
 */
static void
fdc_acpi_attach(device_t parent, device_t self, void *aux)
{
	struct fdc_acpi_softc *asc = device_private(self);
	struct fdc_softc *sc = &asc->sc_fdc;
	struct acpi_attach_args *aa = aux;
	struct acpi_io *io, *ctlio;
	struct acpi_irq *irq;
	struct acpi_drq *drq;
	struct acpi_resources res;
	ACPI_STATUS rv;

	sc->sc_dev = self;
	sc->sc_ic = aa->aa_ic;
	asc->sc_node = aa->aa_node;

	/* parse resources */
	rv = acpi_resource_parse(sc->sc_dev, aa->aa_node->ad_handle, "_CRS",
	    &res, &acpi_resource_parse_ops_default);
	if (ACPI_FAILURE(rv))
		return;

	/* find our i/o registers */
	io = acpi_res_io(&res, 0);
	if (io == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "unable to find i/o register resource\n");
		goto out;
	}

	/* find our IRQ */
	irq = acpi_res_irq(&res, 0);
	if (irq == NULL) {
		aprint_error_dev(sc->sc_dev, "unable to find irq resource\n");
		goto out;
	}

	/* find our DRQ */
	drq = acpi_res_drq(&res, 0);
	if (drq == NULL) {
		aprint_error_dev(sc->sc_dev, "unable to find drq resource\n");
		goto out;
	}
	sc->sc_drq = drq->ar_drq;

	sc->sc_iot = aa->aa_iot;
	if (bus_space_map(sc->sc_iot, io->ar_base, io->ar_length,
		    0, &asc->sc_baseioh)) {
		aprint_error_dev(sc->sc_dev, "can't map i/o space\n");
		goto out;
	}

	switch (io->ar_length) {
	case 4:
		sc->sc_ioh = asc->sc_baseioh;
		break;
	case 6:
		if (bus_space_subregion(sc->sc_iot, asc->sc_baseioh, 2, 4,
		    &sc->sc_ioh)) {
			aprint_error_dev(sc->sc_dev,
			    "unable to subregion i/o space\n");
			goto out;
		}
		break;
	default:
		aprint_error_dev(sc->sc_dev,
		    "unknown size: %d of io mapping\n", io->ar_length);
		goto out;
	}

	/*
	 * omitting the controller I/O port. (One has to exist for there to
	 * be a working fdc). Just try and force the mapping in.
	 */
	ctlio = acpi_res_io(&res, 1);
	if (ctlio == NULL) {
		if (bus_space_map(sc->sc_iot, io->ar_base + io->ar_length + 1,
		    1, 0, &sc->sc_fdctlioh)) {
			aprint_error_dev(sc->sc_dev,
			    "unable to force map ctl i/o space\n");
			goto out;
		}
		aprint_verbose_dev(sc->sc_dev,
		    "ctl io %x did't probe. Forced attach\n",
		    io->ar_base + io->ar_length + 1);
	} else {
		if (bus_space_map(sc->sc_iot, ctlio->ar_base, ctlio->ar_length,
		    0, &sc->sc_fdctlioh)) {
			aprint_error_dev(sc->sc_dev,
			    "unable to map ctl i/o space\n");
			goto out;
		}
	}

	sc->sc_ih = isa_intr_establish(aa->aa_ic, irq->ar_irq,
	    (irq->ar_type == ACPI_EDGE_SENSITIVE) ? IST_EDGE : IST_LEVEL,
	    IPL_BIO, fdcintr, sc);

	/* Setup direct configuration of floppy drives */
	sc->sc_present = fdc_acpi_enumerate(asc);
	if (sc->sc_present >= 0) {
		sc->sc_known = 1;
		fdc_acpi_getknownfds(asc);
	} else {
		/*
		 * XXX if there is no _FDE control method, attempt to
		 * probe without pnp
		 */
		aprint_debug_dev(sc->sc_dev,
		    "unable to enumerate, attempting normal probe\n");
	}

	fdcattach(sc);

 out:
	acpi_resource_cleanup(&res);
}

static int
fdc_acpi_enumerate(struct fdc_acpi_softc *asc)
{
	struct fdc_softc *sc = &asc->sc_fdc;
	ACPI_OBJECT *fde;
	ACPI_BUFFER abuf;
	ACPI_STATUS rv;
	uint32_t *p;
	int i, drives = -1;

	rv = acpi_eval_struct(asc->sc_node->ad_handle, "_FDE", &abuf);

	if (ACPI_FAILURE(rv)) {
		aprint_normal_dev(sc->sc_dev, "failed to evaluate _FDE: %s\n",
		    AcpiFormatException(rv));
		return drives;
	}
	fde = abuf.Pointer;
	if (fde->Type != ACPI_TYPE_BUFFER) {
		aprint_error_dev(sc->sc_dev, "expected BUFFER, got %u\n",
		    fde->Type);
		goto out;
	}
	if (fde->Buffer.Length < 5 * sizeof(uint32_t)) {
		aprint_error_dev(sc->sc_dev,
		    "expected buffer len of %lu, got %u\n",
		    (unsigned long)(5 * sizeof(uint32_t)), fde->Buffer.Length);
		goto out;
	}

	p = (uint32_t *)fde->Buffer.Pointer;

	/*
	 * Indexes 0 through 3 are each uint32_t booleans. True if a drive
	 * is present.
	 */
	drives = 0;
	for (i = 0; i < 4; i++) {
		if (p[i]) drives |= (1 << i);
		aprint_normal_dev(sc->sc_dev, "drive %d %sattached\n", i,
		    p[i] ? "" : "not ");
	}

	/*
	 * p[4] reports tape presence. Possible values:
	 * 	0	- Unknown if device is present
	 *	1	- Device is present
	 *	2	- Device is never present
	 *	>2	- Reserved
	 *
	 * we don't currently use this.
	 */

out:
	ACPI_FREE(abuf.Pointer);
	return drives;
}

static void
fdc_acpi_getknownfds(struct fdc_acpi_softc *asc)
{
	struct fdc_softc *sc = &asc->sc_fdc;
	ACPI_OBJECT *fdi, *e;
	ACPI_BUFFER abuf;
	ACPI_STATUS rv;
	int i;

	for (i = 0; i < 4; i++) {
		if ((sc->sc_present & (1 << i)) == 0)
			continue;
		rv = acpi_eval_struct(asc->sc_node->ad_handle, "_FDI", &abuf);
		if (ACPI_FAILURE(rv)) {
			aprint_normal_dev(sc->sc_dev,
			    "failed to evaluate _FDI: %s on drive %d\n",
			    AcpiFormatException(rv), i);
			/* XXX if _FDI fails, assume 1.44MB floppy */
			sc->sc_knownfds[i] = &fdc_acpi_fdtypes[0];
			continue;
		}
		fdi = abuf.Pointer;
		if (fdi->Type != ACPI_TYPE_PACKAGE) {
			aprint_error_dev(sc->sc_dev,
			    "expected PACKAGE, got %u\n", fdi->Type);
			goto out;
		}
		e = fdi->Package.Elements;
		sc->sc_knownfds[i] = fdc_acpi_nvtotype(
		    device_xname(sc->sc_dev),
		    e[1].Integer.Value, e[0].Integer.Value);

		/* if fdc_acpi_nvtotype returns NULL, don't attach drive */
		if (!sc->sc_knownfds[i])
			sc->sc_present &= ~(1 << i);

out:
		ACPI_FREE(abuf.Pointer);
	}
}

static const struct fd_type *
fdc_acpi_nvtotype(const char *fdc, int nvraminfo, int drive)
{
	int type;

	type = (drive == 0 ? nvraminfo : nvraminfo << 4) & 0xf0;
	switch (type) {
	case ACPI_FDC_DISKETTE_NONE:
		return NULL;
	case ACPI_FDC_DISKETTE_12M:
		return &fdc_acpi_fdtypes[1];
	case ACPI_FDC_DISKETTE_TYPE5:
	case ACPI_FDC_DISKETTE_TYPE6:
	case ACPI_FDC_DISKETTE_144M:
		return &fdc_acpi_fdtypes[0];
	case ACPI_FDC_DISKETTE_360K:
		return &fdc_acpi_fdtypes[3];
	case ACPI_FDC_DISKETTE_720K:
		return &fdc_acpi_fdtypes[4];
	default:
		aprint_normal("%s: drive %d: unknown device type 0x%x\n",
		    fdc, drive, type);
		return NULL;
	}
}
