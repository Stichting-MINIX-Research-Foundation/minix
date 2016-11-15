/*	$NetBSD: aac.c,v 1.44 2012/10/27 17:18:18 chs Exp $	*/

/*-
 * Copyright (c) 2002, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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

/*-
 * Copyright (c) 2001 Scott Long
 * Copyright (c) 2001 Adaptec, Inc.
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2000 Niklas Hallqvist
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

/*
 * Driver for the Adaptec 'FSA' family of PCI/SCSI RAID adapters.
 *
 * TODO:
 *
 * o Management interface.
 * o Look again at some of the portability issues.
 * o Handle various AIFs (e.g., notification that a container is going away).
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: aac.c,v 1.44 2012/10/27 17:18:18 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <sys/bus.h>

#include <dev/ic/aacreg.h>
#include <dev/ic/aacvar.h>
#include <dev/ic/aac_tables.h>

#include "locators.h"

static int	aac_new_intr(void *);
static int	aac_alloc_commands(struct aac_softc *);
#ifdef notyet
static void	aac_free_commands(struct aac_softc *);
#endif
static int	aac_check_firmware(struct aac_softc *);
static void	aac_describe_controller(struct aac_softc *);
static int	aac_dequeue_fib(struct aac_softc *, int, u_int32_t *,
				struct aac_fib **);
static int	aac_enqueue_fib(struct aac_softc *, int, struct aac_ccb *);
static int	aac_enqueue_response(struct aac_softc *, int, struct aac_fib *);
static void	aac_host_command(struct aac_softc *);
static void	aac_host_response(struct aac_softc *);
static int	aac_init(struct aac_softc *);
static int	aac_print(void *, const char *);
static void	aac_shutdown(void *);
static void	aac_startup(struct aac_softc *);
static int	aac_sync_command(struct aac_softc *, u_int32_t, u_int32_t,
				 u_int32_t, u_int32_t, u_int32_t, u_int32_t *);
static int	aac_sync_fib(struct aac_softc *, u_int32_t, u_int32_t, void *,
			     u_int16_t, void *, u_int16_t *);

#ifdef AAC_DEBUG
static void	aac_print_fib(struct aac_softc *, struct aac_fib *, const char *);
#endif

/*
 * Adapter-space FIB queue manipulation.
 *
 * Note that the queue implementation here is a little funky; neither the PI or
 * CI will ever be zero.  This behaviour is a controller feature.
 */
static struct {
	int	size;
	int	notify;
} const aac_qinfo[] = {
	{ AAC_HOST_NORM_CMD_ENTRIES, AAC_DB_COMMAND_NOT_FULL },
	{ AAC_HOST_HIGH_CMD_ENTRIES, 0 },
	{ AAC_ADAP_NORM_CMD_ENTRIES, AAC_DB_COMMAND_READY },
	{ AAC_ADAP_HIGH_CMD_ENTRIES, 0 },
	{ AAC_HOST_NORM_RESP_ENTRIES, AAC_DB_RESPONSE_NOT_FULL },
	{ AAC_HOST_HIGH_RESP_ENTRIES, 0 },
	{ AAC_ADAP_NORM_RESP_ENTRIES, AAC_DB_RESPONSE_READY },
	{ AAC_ADAP_HIGH_RESP_ENTRIES, 0 }
};

#ifdef AAC_DEBUG
int	aac_debug = AAC_DEBUG;
#endif

MALLOC_DEFINE(M_AACBUF, "aacbuf", "Buffers for aac(4)");

static void	*aac_sdh;

extern struct	cfdriver aac_cd;

int
aac_attach(struct aac_softc *sc)
{
	struct aac_attach_args aaca;
	int i, rv;
	int locs[AACCF_NLOCS];

	SIMPLEQ_INIT(&sc->sc_ccb_free);
	SIMPLEQ_INIT(&sc->sc_ccb_queue);
	SIMPLEQ_INIT(&sc->sc_ccb_complete);

	/*
	 * Disable interrupts before we do anything.
	 */
	AAC_MASK_INTERRUPTS(sc);

	/*
	 * Initialise the adapter.
	 */
	if (aac_check_firmware(sc))
		return (EINVAL);

	if ((rv = aac_init(sc)) != 0)
		return (rv);

	if (sc->sc_quirks & AAC_QUIRK_NEW_COMM) {
		rv = sc->sc_intr_set(sc, aac_new_intr, sc);
		if (rv)
			return (rv);
	}

	aac_startup(sc);

	/*
	 * Print a little information about the controller.
	 */
	aac_describe_controller(sc);

	/*
	 * Attach devices.
	 */
	for (i = 0; i < AAC_MAX_CONTAINERS; i++) {
		if (!sc->sc_hdr[i].hd_present)
			continue;
		aaca.aaca_unit = i;

		locs[AACCF_UNIT] = i;

		config_found_sm_loc(sc->sc_dv, "aac", locs, &aaca,
				    aac_print, config_stdsubmatch);
	}

	/*
	 * Enable interrupts, and register our shutdown hook.
	 */
	sc->sc_flags |= AAC_ONLINE;
	AAC_UNMASK_INTERRUPTS(sc);
	if (aac_sdh != NULL)
		shutdownhook_establish(aac_shutdown, NULL);
	return (0);
}

static int
aac_alloc_commands(struct aac_softc *sc)
{
	struct aac_fibmap *fm;
	struct aac_ccb *ac;
	bus_addr_t fibpa;
	int size, nsegs;
	int i, error;
	int state;

	if (sc->sc_total_fibs + sc->sc_max_fibs_alloc > sc->sc_max_fibs)
		return ENOMEM;

	fm = malloc(sizeof(struct aac_fibmap), M_AACBUF, M_NOWAIT|M_ZERO);
	if (fm == NULL)
		return ENOMEM;

	size = sc->sc_max_fibs_alloc * sc->sc_max_fib_size;

	state = 0;
	error = bus_dmamap_create(sc->sc_dmat, size, 1, size,
	    0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &fm->fm_fibmap);
	if (error != 0) {
		aprint_error_dev(sc->sc_dv, "cannot create fibs dmamap (%d)\n",
		    error);
		goto bail_out;
	}
	state++;
	error = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0,
	    &fm->fm_fibseg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dv, "can't allocate fibs structure (%d)\n",
		    error);
		goto bail_out;
	}
	state++;
	error = bus_dmamem_map(sc->sc_dmat, &fm->fm_fibseg, nsegs, size,
	    (void **)&fm->fm_fibs, 0);
	if (error != 0) {
		aprint_error_dev(sc->sc_dv, "can't map fibs structure (%d)\n",
		    error);
		goto bail_out;
	}
	state++;
	error = bus_dmamap_load(sc->sc_dmat, fm->fm_fibmap, fm->fm_fibs,
	    size, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		aprint_error_dev(sc->sc_dv, "cannot load fibs dmamap (%d)\n",
		    error);
		goto bail_out;
	}

	fm->fm_ccbs = sc->sc_ccbs + sc->sc_total_fibs;
	fibpa = fm->fm_fibseg.ds_addr;

	memset(fm->fm_fibs, 0, size);
	for (i = 0; i < sc->sc_max_fibs_alloc; i++) {
		ac = fm->fm_ccbs + i;

		error = bus_dmamap_create(sc->sc_dmat, AAC_MAX_XFER(sc),
		    sc->sc_max_sgs, AAC_MAX_XFER(sc), 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &ac->ac_dmamap_xfer);
		if (error) {
			while (--i >= 0) {
				ac = fm->fm_ccbs + i;
				bus_dmamap_destroy(sc->sc_dmat,
				    ac->ac_dmamap_xfer);
				sc->sc_total_fibs--;
			}
			aprint_error_dev(sc->sc_dv, "cannot create ccb dmamap (%d)",
			    error);
			goto bail_out;
		}

		ac->ac_fibmap = fm;
		ac->ac_fib = (struct aac_fib *)
		    ((char *) fm->fm_fibs + i * sc->sc_max_fib_size);
		ac->ac_fibphys = fibpa + i * sc->sc_max_fib_size;
		aac_ccb_free(sc, ac);
		sc->sc_total_fibs++;
	}

	TAILQ_INSERT_TAIL(&sc->sc_fibmap_tqh, fm, fm_link);

	return 0;
bail_out:
	if (state > 3)
		bus_dmamap_unload(sc->sc_dmat, fm->fm_fibmap);
	if (state > 2)
		bus_dmamem_unmap(sc->sc_dmat, (void *) fm->fm_fibs, size);
	if (state > 1)
		bus_dmamem_free(sc->sc_dmat, &fm->fm_fibseg, 1);

	bus_dmamap_destroy(sc->sc_dmat, fm->fm_fibmap);

	free(fm, M_AACBUF);

	return error;
}

#ifdef notyet
static void
aac_free_commands(struct aac_softc *sc)
{
}
#endif

/*
 * Print autoconfiguration message for a sub-device.
 */
static int
aac_print(void *aux, const char *pnp)
{
	struct aac_attach_args *aaca;

	aaca = aux;

	if (pnp != NULL)
		aprint_normal("block device at %s", pnp);
	aprint_normal(" unit %d", aaca->aaca_unit);
	return (UNCONF);
}

/*
 * Look up a text description of a numeric error code and return a pointer to
 * same.
 */
const char *
aac_describe_code(const struct aac_code_lookup *table, u_int32_t code)
{
	int i;

	for (i = 0; table[i].string != NULL; i++)
		if (table[i].code == code)
			return (table[i].string);

	return (table[i + 1].string);
}

/*
 * snprintb(3) format string for the adapter options.
 */
static const char *optfmt = 
    "\20\1SNAPSHOT\2CLUSTERS\3WCACHE\4DATA64\5HOSTTIME\6RAID50"
    "\7WINDOW4GB"
    "\10SCSIUPGD\11SOFTERR\12NORECOND\13SGMAP64\14ALARM\15NONDASD";

static void
aac_describe_controller(struct aac_softc *sc)
{
	u_int8_t fmtbuf[256];
	u_int8_t tbuf[AAC_FIB_DATASIZE];
	u_int16_t bufsize;
	struct aac_adapter_info *info;
	u_int8_t arg;

	arg = 0;
	if (aac_sync_fib(sc, RequestAdapterInfo, 0, &arg, sizeof(arg), &tbuf,
	    &bufsize)) {
		aprint_error_dev(sc->sc_dv, "RequestAdapterInfo failed\n");
		return;
	}
	if (bufsize != sizeof(*info)) {
		aprint_error_dev(sc->sc_dv,
		    "RequestAdapterInfo returned wrong data size (%d != %zu)\n",
		    bufsize, sizeof(*info));
		return;
	}
	info = (struct aac_adapter_info *)&tbuf[0];

	aprint_normal_dev(sc->sc_dv, "%s at %dMHz, %dMB mem (%dMB cache), %s\n",
	    aac_describe_code(aac_cpu_variant, le32toh(info->CpuVariant)),
	    le32toh(info->ClockSpeed),
	    le32toh(info->TotalMem) / (1024 * 1024),
	    le32toh(info->BufferMem) / (1024 * 1024),
	    aac_describe_code(aac_battery_platform,
			      le32toh(info->batteryPlatform)));

	aprint_verbose_dev(sc->sc_dv, "Kernel %d.%d-%d [Build %d], ",
	    info->KernelRevision.external.comp.major,
	    info->KernelRevision.external.comp.minor,
	    info->KernelRevision.external.comp.dash,
	    info->KernelRevision.buildNumber);

	aprint_verbose("Monitor %d.%d-%d [Build %d], S/N %6X\n",
	    info->MonitorRevision.external.comp.major,
	    info->MonitorRevision.external.comp.minor,
	    info->MonitorRevision.external.comp.dash,
	    info->MonitorRevision.buildNumber,
	    ((u_int32_t)info->SerialNumber & 0xffffff));

	snprintb(fmtbuf, sizeof(fmtbuf), optfmt, sc->sc_supported_options);
	aprint_verbose_dev(sc->sc_dv, "Controller supports: %s\n", fmtbuf);

	/* Save the kernel revision structure for later use. */
	sc->sc_revision = info->KernelRevision;
}

/*
 * Retrieve the firmware version numbers.  Dell PERC2/QC cards with firmware
 * version 1.x are not compatible with this driver.
 */
static int
aac_check_firmware(struct aac_softc *sc)
{
	u_int32_t major, minor, opts, atusize = 0, status = 0;
	u_int32_t calcsgs;

	if ((sc->sc_quirks & AAC_QUIRK_PERC2QC) != 0) {
		if (aac_sync_command(sc, AAC_MONKER_GETKERNVER, 0, 0, 0, 0,
		    NULL)) {
			aprint_error_dev(sc->sc_dv, "error reading firmware version\n");
			return (1);
		}

		/* These numbers are stored as ASCII! */
		major = (AAC_GET_MAILBOX(sc, 1) & 0xff) - 0x30;
		minor = (AAC_GET_MAILBOX(sc, 2) & 0xff) - 0x30;
		if (major == 1) {
			aprint_error_dev(sc->sc_dv, 
			    "firmware version %d.%d not supported.\n",
			    major, minor);
			return (1);
		}
	}

	if (aac_sync_command(sc, AAC_MONKER_GETINFO, 0, 0, 0, 0, &status)) {
		if (status != AAC_SRB_STS_INVALID_REQUEST) {
			aprint_error_dev(sc->sc_dv, "GETINFO failed, status 0x%08x\n", status);
			return (1);
		}
	} else {
		opts = AAC_GET_MAILBOX(sc, 1);
		atusize = AAC_GET_MAILBOX(sc, 2);
		sc->sc_supported_options = opts;

		if (((opts & AAC_SUPPORTED_4GB_WINDOW) != 0) &&
		    ((sc->sc_quirks & AAC_QUIRK_NO4GB) == 0) )
			sc->sc_quirks |= AAC_QUIRK_4GB_WINDOW;

		if (((opts & AAC_SUPPORTED_SGMAP_HOST64) != 0) &&
		    (sizeof(bus_addr_t) > 4)) {
			aprint_normal_dev(sc->sc_dv, "Enabling 64-bit address support\n");
			sc->sc_quirks |= AAC_QUIRK_SG_64BIT;
		}
		if ((opts & AAC_SUPPORTED_NEW_COMM) &&
		    (sc->sc_if.aif_send_command != NULL)) {
			sc->sc_quirks |= AAC_QUIRK_NEW_COMM;
		}
		if (opts & AAC_SUPPORTED_64BIT_ARRAYSIZE)
			sc->sc_quirks |= AAC_QUIRK_ARRAY_64BIT;
	}

	sc->sc_max_fibs = (sc->sc_quirks & AAC_QUIRK_256FIBS) ? 256 : 512;

	if (   (sc->sc_quirks & AAC_QUIRK_NEW_COMM)
	    && (sc->sc_regsize < atusize)) {
		aprint_error_dev(sc->sc_dv, "Not enabling new comm i/f -- "
			     "atusize 0x%08x, regsize 0x%08x\n",
			     atusize,
			     (uint32_t) sc->sc_regsize);
		sc->sc_quirks &= ~AAC_QUIRK_NEW_COMM;
	}
#if 0
	if (sc->sc_quirks & AAC_QUIRK_NEW_COMM) {
		aprint_error_dev(sc->sc_dv, "Not enabling new comm i/f -- "
			     "driver not ready yet\n");
		sc->sc_quirks &= ~AAC_QUIRK_NEW_COMM;
	}
#endif

	sc->sc_max_fib_size = sizeof(struct aac_fib);
	sc->sc_max_sectors = 128;	/* 64KB */
	if (sc->sc_quirks & AAC_QUIRK_SG_64BIT)
		sc->sc_max_sgs = (sc->sc_max_fib_size
					- sizeof(struct aac_blockwrite64)
					+ sizeof(struct aac_sg_table64))
				      / sizeof(struct aac_sg_table64);
	else
		sc->sc_max_sgs = (sc->sc_max_fib_size
					- sizeof(struct aac_blockwrite)
					+ sizeof(struct aac_sg_table))
				      / sizeof(struct aac_sg_table);

	if (!aac_sync_command(sc, AAC_MONKER_GETCOMMPREF, 0, 0, 0, 0, NULL)) {
		u_int32_t	opt1, opt2, opt3;
		u_int32_t	tmpval;

		opt1 = AAC_GET_MAILBOX(sc, 1);
		opt2 = AAC_GET_MAILBOX(sc, 2);
		opt3 = AAC_GET_MAILBOX(sc, 3);
		if (!opt1 || !opt2 || !opt3) {
			aprint_verbose_dev(sc->sc_dv, "GETCOMMPREF appears untrustworthy."
			    "  Ignoring.\n");
		} else {
			sc->sc_max_fib_size = le32toh(opt1) & 0xffff;
			sc->sc_max_sectors = (le32toh(opt1) >> 16) << 1;
			tmpval = (le32toh(opt2) >> 16);
			if (tmpval < sc->sc_max_sgs) {
				sc->sc_max_sgs = tmpval;
			}
			tmpval = (le32toh(opt3) & 0xffff);
			if (tmpval < sc->sc_max_fibs) {
				sc->sc_max_fibs = tmpval;
			}
		}
	}
	if (sc->sc_max_fib_size > PAGE_SIZE)
		sc->sc_max_fib_size = PAGE_SIZE;

	if (sc->sc_quirks & AAC_QUIRK_SG_64BIT)
		calcsgs = (sc->sc_max_fib_size
			   - sizeof(struct aac_blockwrite64)
			   + sizeof(struct aac_sg_table64))
			      / sizeof(struct aac_sg_table64);
	else
		calcsgs = (sc->sc_max_fib_size
			   - sizeof(struct aac_blockwrite)
			   + sizeof(struct aac_sg_table))
			      / sizeof(struct aac_sg_table);

	if (calcsgs < sc->sc_max_sgs) {
		sc->sc_max_sgs = calcsgs;
	}

	sc->sc_max_fibs_alloc = PAGE_SIZE / sc->sc_max_fib_size;

	if (sc->sc_max_fib_size > sizeof(struct aac_fib)) {
		sc->sc_quirks |= AAC_QUIRK_RAW_IO;
		aprint_debug_dev(sc->sc_dv, "Enable raw I/O\n");
	}
	if ((sc->sc_quirks & AAC_QUIRK_RAW_IO) &&
	    (sc->sc_quirks & AAC_QUIRK_ARRAY_64BIT)) {
		sc->sc_quirks |= AAC_QUIRK_LBA_64BIT;
		aprint_normal_dev(sc->sc_dv, "Enable 64-bit array support\n");
	}

	return (0);
}

static int
aac_init(struct aac_softc *sc)
{
	int nsegs, i, rv, state, norm, high;
	struct aac_adapter_init	*ip;
	u_int32_t code, qoff;

	state = 0;

	/*
	 * First wait for the adapter to come ready.
	 */
	for (i = 0; i < AAC_BOOT_TIMEOUT * 1000; i++) {
		code = AAC_GET_FWSTATUS(sc);
		if ((code & AAC_SELF_TEST_FAILED) != 0) {
			aprint_error_dev(sc->sc_dv, "FATAL: selftest failed\n");
			return (ENXIO);
		}
		if ((code & AAC_KERNEL_PANIC) != 0) {
			aprint_error_dev(sc->sc_dv, "FATAL: controller kernel panic\n");
			return (ENXIO);
		}
		if ((code & AAC_UP_AND_RUNNING) != 0)
			break;
		DELAY(1000);
	}
	if (i == AAC_BOOT_TIMEOUT * 1000) {
		aprint_error_dev(sc->sc_dv, 
		    "FATAL: controller not coming ready, status %x\n",
		    code);
		return (ENXIO);
	}

	sc->sc_aif_fib = malloc(sizeof(struct aac_fib), M_AACBUF,
	    M_NOWAIT | M_ZERO);
	if (sc->sc_aif_fib == NULL) {
		aprint_error_dev(sc->sc_dv, "cannot alloc fib structure\n");
		return (ENOMEM);
	}
	if ((rv = bus_dmamap_create(sc->sc_dmat, sizeof(*sc->sc_common), 1,
	    sizeof(*sc->sc_common), 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &sc->sc_common_dmamap)) != 0) {
		aprint_error_dev(sc->sc_dv, "cannot create common dmamap\n");
		goto bail_out;
	}
	state++;
	if ((rv = bus_dmamem_alloc(sc->sc_dmat, sizeof(*sc->sc_common),
	    PAGE_SIZE, 0, &sc->sc_common_seg, 1, &nsegs,
	    BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dv, "can't allocate common structure\n");
		goto bail_out;
	}
	state++;
	if ((rv = bus_dmamem_map(sc->sc_dmat, &sc->sc_common_seg, nsegs,
	    sizeof(*sc->sc_common), (void **)&sc->sc_common, 0)) != 0) {
		aprint_error_dev(sc->sc_dv, "can't map common structure\n");
		goto bail_out;
	}
	state++;
	if ((rv = bus_dmamap_load(sc->sc_dmat, sc->sc_common_dmamap,
	    sc->sc_common, sizeof(*sc->sc_common), NULL,
	    BUS_DMA_NOWAIT)) != 0) {
		aprint_error_dev(sc->sc_dv, "cannot load common dmamap\n");
		goto bail_out;
	}
	state++;

	memset(sc->sc_common, 0, sizeof(*sc->sc_common));

	TAILQ_INIT(&sc->sc_fibmap_tqh);
	sc->sc_ccbs = malloc(sizeof(struct aac_ccb) * sc->sc_max_fibs, M_AACBUF,
	    M_NOWAIT | M_ZERO);
	if (sc->sc_ccbs == NULL) {
		aprint_error_dev(sc->sc_dv, "memory allocation failure getting ccbs\n");
		rv = ENOMEM;
		goto bail_out;
	}
	state++;
	while (sc->sc_total_fibs < AAC_PREALLOCATE_FIBS(sc)) {
		if (aac_alloc_commands(sc) != 0)
			break;
	}
	if (sc->sc_total_fibs == 0)
		goto bail_out;

	/*
	 * Fill in the init structure.  This tells the adapter about the
	 * physical location of various important shared data structures.
	 */
	ip = &sc->sc_common->ac_init;
	ip->InitStructRevision = htole32(AAC_INIT_STRUCT_REVISION);
	if (sc->sc_quirks & AAC_QUIRK_RAW_IO)
		ip->InitStructRevision = htole32(AAC_INIT_STRUCT_REVISION_4);
	ip->MiniPortRevision = htole32(AAC_INIT_STRUCT_MINIPORT_REVISION);

	ip->AdapterFibsPhysicalAddress = htole32(sc->sc_common_seg.ds_addr +
	    offsetof(struct aac_common, ac_fibs));
	ip->AdapterFibsVirtualAddress = 0;
	ip->AdapterFibsSize =
	    htole32(AAC_ADAPTER_FIBS * sizeof(struct aac_fib));
	ip->AdapterFibAlign = htole32(sizeof(struct aac_fib));

	ip->PrintfBufferAddress = htole32(sc->sc_common_seg.ds_addr +
	    offsetof(struct aac_common, ac_printf));
	ip->PrintfBufferSize = htole32(AAC_PRINTF_BUFSIZE);

	/*
	 * The adapter assumes that pages are 4K in size, except on some
	 * broken firmware versions that do the page->byte conversion twice,
	 * therefore 'assuming' that this value is in 16MB units (2^24).
	 * Round up since the granularity is so high.
	 */
	ip->HostPhysMemPages = ctob(physmem) / AAC_PAGE_SIZE;
	if (sc->sc_quirks & AAC_QUIRK_BROKEN_MMAP) {
		ip->HostPhysMemPages = 
		    (ip->HostPhysMemPages + AAC_PAGE_SIZE) / AAC_PAGE_SIZE;
	}
	ip->HostElapsedSeconds = 0;	/* reset later if invalid */

	ip->InitFlags = 0;
	if (sc->sc_quirks & AAC_QUIRK_NEW_COMM) {
		ip->InitFlags = htole32(AAC_INITFLAGS_NEW_COMM_SUPPORTED);
		aprint_normal_dev(sc->sc_dv, "New comm. interface enabled\n");
	}

	ip->MaxIoCommands = htole32(sc->sc_max_fibs);
	ip->MaxIoSize = htole32(sc->sc_max_sectors << 9);
	ip->MaxFibSize = htole32(sc->sc_max_fib_size);

	/*
	 * Initialise FIB queues.  Note that it appears that the layout of
	 * the indexes and the segmentation of the entries is mandated by
	 * the adapter, which is only told about the base of the queue index
	 * fields.
	 *
	 * The initial values of the indices are assumed to inform the
	 * adapter of the sizes of the respective queues.
	 *
	 * The Linux driver uses a much more complex scheme whereby several
	 * header records are kept for each queue.  We use a couple of
	 * generic list manipulation functions which 'know' the size of each
	 * list by virtue of a table.
	 */
	qoff = offsetof(struct aac_common, ac_qbuf) + AAC_QUEUE_ALIGN;
	qoff &= ~(AAC_QUEUE_ALIGN - 1);
	sc->sc_queues = (struct aac_queue_table *)((uintptr_t)sc->sc_common + qoff);
	ip->CommHeaderAddress = htole32(sc->sc_common_seg.ds_addr +
	    ((char *)sc->sc_queues - (char *)sc->sc_common));
	memset(sc->sc_queues, 0, sizeof(struct aac_queue_table));

	norm = htole32(AAC_HOST_NORM_CMD_ENTRIES);
	high = htole32(AAC_HOST_HIGH_CMD_ENTRIES);

	sc->sc_queues->qt_qindex[AAC_HOST_NORM_CMD_QUEUE][AAC_PRODUCER_INDEX] =
	    norm;
	sc->sc_queues->qt_qindex[AAC_HOST_NORM_CMD_QUEUE][AAC_CONSUMER_INDEX] =
	    norm;
	sc->sc_queues->qt_qindex[AAC_HOST_HIGH_CMD_QUEUE][AAC_PRODUCER_INDEX] =
	    high;
	sc->sc_queues->qt_qindex[AAC_HOST_HIGH_CMD_QUEUE][AAC_CONSUMER_INDEX] =
	    high;

	norm = htole32(AAC_ADAP_NORM_CMD_ENTRIES);
	high = htole32(AAC_ADAP_HIGH_CMD_ENTRIES);

	sc->sc_queues->qt_qindex[AAC_ADAP_NORM_CMD_QUEUE][AAC_PRODUCER_INDEX] =
	    norm;
	sc->sc_queues->qt_qindex[AAC_ADAP_NORM_CMD_QUEUE][AAC_CONSUMER_INDEX] =
	    norm;
	sc->sc_queues->qt_qindex[AAC_ADAP_HIGH_CMD_QUEUE][AAC_PRODUCER_INDEX] =
	    high;
	sc->sc_queues->qt_qindex[AAC_ADAP_HIGH_CMD_QUEUE][AAC_CONSUMER_INDEX] =
	    high;

	norm = htole32(AAC_HOST_NORM_RESP_ENTRIES);
	high = htole32(AAC_HOST_HIGH_RESP_ENTRIES);

	sc->sc_queues->
	    qt_qindex[AAC_HOST_NORM_RESP_QUEUE][AAC_PRODUCER_INDEX] = norm;
	sc->sc_queues->
	    qt_qindex[AAC_HOST_NORM_RESP_QUEUE][AAC_CONSUMER_INDEX] = norm;
	sc->sc_queues->
	    qt_qindex[AAC_HOST_HIGH_RESP_QUEUE][AAC_PRODUCER_INDEX] = high;
	sc->sc_queues->
	    qt_qindex[AAC_HOST_HIGH_RESP_QUEUE][AAC_CONSUMER_INDEX] = high;

	norm = htole32(AAC_ADAP_NORM_RESP_ENTRIES);
	high = htole32(AAC_ADAP_HIGH_RESP_ENTRIES);

	sc->sc_queues->
	    qt_qindex[AAC_ADAP_NORM_RESP_QUEUE][AAC_PRODUCER_INDEX] = norm;
	sc->sc_queues->
	    qt_qindex[AAC_ADAP_NORM_RESP_QUEUE][AAC_CONSUMER_INDEX] = norm;
	sc->sc_queues->
	    qt_qindex[AAC_ADAP_HIGH_RESP_QUEUE][AAC_PRODUCER_INDEX] = high;
	sc->sc_queues->
	    qt_qindex[AAC_ADAP_HIGH_RESP_QUEUE][AAC_CONSUMER_INDEX] = high;

	sc->sc_qentries[AAC_HOST_NORM_CMD_QUEUE] =
	    &sc->sc_queues->qt_HostNormCmdQueue[0];
	sc->sc_qentries[AAC_HOST_HIGH_CMD_QUEUE] =
	    &sc->sc_queues->qt_HostHighCmdQueue[0];
	sc->sc_qentries[AAC_ADAP_NORM_CMD_QUEUE] =
	    &sc->sc_queues->qt_AdapNormCmdQueue[0];
	sc->sc_qentries[AAC_ADAP_HIGH_CMD_QUEUE] =
	    &sc->sc_queues->qt_AdapHighCmdQueue[0];
	sc->sc_qentries[AAC_HOST_NORM_RESP_QUEUE] =
	    &sc->sc_queues->qt_HostNormRespQueue[0];
	sc->sc_qentries[AAC_HOST_HIGH_RESP_QUEUE] =
	    &sc->sc_queues->qt_HostHighRespQueue[0];
	sc->sc_qentries[AAC_ADAP_NORM_RESP_QUEUE] =
	    &sc->sc_queues->qt_AdapNormRespQueue[0];
	sc->sc_qentries[AAC_ADAP_HIGH_RESP_QUEUE] =
	    &sc->sc_queues->qt_AdapHighRespQueue[0];

	/*
	 * Do controller-type-specific initialisation
	 */
	switch (sc->sc_hwif) {
	case AAC_HWIF_I960RX:
		AAC_SETREG4(sc, AAC_RX_ODBR, ~0);
		break;
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_common_dmamap, 0,
	    sizeof(*sc->sc_common),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/*
	 * Give the init structure to the controller.
	 */
	if (aac_sync_command(sc, AAC_MONKER_INITSTRUCT,
	    sc->sc_common_seg.ds_addr + offsetof(struct aac_common, ac_init),
	    0, 0, 0, NULL)) {
		aprint_error_dev(sc->sc_dv, "error establishing init structure\n");
		rv = EIO;
		goto bail_out;
	}

	return (0);

 bail_out:
 	if (state > 4)
 		free(sc->sc_ccbs, M_AACBUF);
 	if (state > 3)
 		bus_dmamap_unload(sc->sc_dmat, sc->sc_common_dmamap);
	if (state > 2)
		bus_dmamem_unmap(sc->sc_dmat, (void *)sc->sc_common,
		    sizeof(*sc->sc_common));
	if (state > 1)
		bus_dmamem_free(sc->sc_dmat, &sc->sc_common_seg, 1);
	if (state > 0)
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_common_dmamap);

	free(sc->sc_aif_fib, M_AACBUF);

	return (rv);
}

/*
 * Probe for containers, create disks.
 */
static void
aac_startup(struct aac_softc *sc)
{
	struct aac_mntinfo mi;
	struct aac_mntinforesponse mir;
	struct aac_drive *hd;
	u_int16_t rsize;
	size_t ersize;
	int i;

	/*
	 * Loop over possible containers.
	 */
	hd = sc->sc_hdr;

	for (i = 0; i < AAC_MAX_CONTAINERS; i++, hd++) {
		/*
		 * Request information on this container.
		 */
		memset(&mi, 0, sizeof(mi));
		/* use 64-bit LBA if enabled */
		if (sc->sc_quirks & AAC_QUIRK_LBA_64BIT) {
			mi.Command = htole32(VM_NameServe64);
			ersize = sizeof(mir);
		} else {
			mi.Command = htole32(VM_NameServe);
			ersize = sizeof(mir) - sizeof(mir.MntTable[0].CapacityHigh);
		}
		mi.MntType = htole32(FT_FILESYS);
		mi.MntCount = htole32(i);
		if (aac_sync_fib(sc, ContainerCommand, 0, &mi, sizeof(mi), &mir,
		    &rsize)) {
			aprint_error_dev(sc->sc_dv, "error probing container %d\n", i);
			continue;
		}
		if (rsize != ersize) {
			aprint_error_dev(sc->sc_dv, "container info response wrong size "
			    "(%d should be %zu)\n", rsize, ersize);
			continue;
		}

		/*
		 * Check container volume type for validity.  Note that many
		 * of the possible types may never show up.
		 */
		if (le32toh(mir.Status) != ST_OK ||
		    le32toh(mir.MntTable[0].VolType) == CT_NONE)
			continue;

		hd->hd_present = 1;
		hd->hd_size = le32toh(mir.MntTable[0].Capacity);
		if (sc->sc_quirks & AAC_QUIRK_LBA_64BIT)
			hd->hd_size += (u_int64_t)
			    le32toh(mir.MntTable[0].CapacityHigh) << 32;
		hd->hd_devtype = le32toh(mir.MntTable[0].VolType);
		hd->hd_size &= ~0x1f;
		sc->sc_nunits++;
	}
}

static void
aac_shutdown(void *cookie)
{
	struct aac_softc *sc;
	struct aac_close_command cc;
	u_int32_t i;

	for (i = 0; i < aac_cd.cd_ndevs; i++) {
		if ((sc = device_lookup_private(&aac_cd, i)) == NULL)
			continue;
		if ((sc->sc_flags & AAC_ONLINE) == 0)
			continue;

		AAC_MASK_INTERRUPTS(sc);

		/*
		 * Send a Container shutdown followed by a HostShutdown FIB
		 * to the controller to convince it that we don't want to
		 * talk to it anymore.  We've been closed and all I/O
		 * completed already
		 */
		memset(&cc, 0, sizeof(cc));
		cc.Command = htole32(VM_CloseAll);
		cc.ContainerId = 0xffffffff;
		if (aac_sync_fib(sc, ContainerCommand, 0, &cc, sizeof(cc),
		    NULL, NULL)) {
			aprint_error_dev(sc->sc_dv, "unable to halt controller\n");
			continue;
		}

		/*
		 * Note that issuing this command to the controller makes it
		 * shut down but also keeps it from coming back up without a
		 * reset of the PCI bus.
		 */
		if (aac_sync_fib(sc, FsaHostShutdown, AAC_FIBSTATE_SHUTDOWN,
		    &i, sizeof(i), NULL, NULL))
			aprint_error_dev(sc->sc_dv, "unable to halt controller\n");

		sc->sc_flags &= ~AAC_ONLINE;
	}
}

static int
aac_new_intr(void *cookie)
{
	struct aac_softc *sc;
	u_int32_t index, fast;
	struct aac_ccb *ac;
	struct aac_fib *fib;
	struct aac_fibmap *fm;
	int i;

	sc = (struct aac_softc *) cookie;

	for (;;) {
		index = AAC_GET_OUTB_QUEUE(sc);
		if (index == 0xffffffff)
			index = AAC_GET_OUTB_QUEUE(sc);
		if (index == 0xffffffff)
			break;
		if (index & 2) {
			if (index == 0xfffffffe) {
				/* XXX This means that the controller wants
				 * more work.  Ignore it for now.
				 */
				continue;
			}
			/* AIF */
			index &= ~2;
			fib = sc->sc_aif_fib;
			for (i = 0; i < sizeof(struct aac_fib)/4; i++) {
				((u_int32_t*)fib)[i] =
				    AAC_GETREG4(sc, index + i*4);
			}
#ifdef notyet
			aac_handle_aif(sc, &fib);
#endif

			AAC_SET_OUTB_QUEUE(sc, index);
			AAC_CLEAR_ISTATUS(sc, AAC_DB_RESPONSE_READY);
		} else {
			fast = index & 1;
			ac = sc->sc_ccbs + (index >> 2);
			fib = ac->ac_fib;
			fm = ac->ac_fibmap;
			if (fast) {
				bus_dmamap_sync(sc->sc_dmat, fm->fm_fibmap,
				    (char *)fib - (char *)fm->fm_fibs,
				    sc->sc_max_fib_size,
				    BUS_DMASYNC_POSTWRITE |
				    BUS_DMASYNC_POSTREAD);
				fib->Header.XferState |=
				    htole32(AAC_FIBSTATE_DONEADAP);
				*((u_int32_t *)(fib->data)) =
				    htole32(AAC_ERROR_NORMAL);
			}
			ac->ac_flags |= AAC_CCB_COMPLETED;

			if (ac->ac_intr != NULL)
				(*ac->ac_intr)(ac);
			else
				wakeup(ac);
		}
	}

	/*
	 * Try to submit more commands.
	 */
	if (! SIMPLEQ_EMPTY(&sc->sc_ccb_queue))
		aac_ccb_enqueue(sc, NULL);

	return 1;
}

/*
 * Take an interrupt.
 */
int
aac_intr(void *cookie)
{
	struct aac_softc *sc;
	u_int16_t reason;
	int claimed;

	sc = cookie;
	claimed = 0;

	AAC_DPRINTF(AAC_D_INTR, ("aac_intr(%p) ", sc));

	reason = AAC_GET_ISTATUS(sc);
	AAC_CLEAR_ISTATUS(sc, reason);

	AAC_DPRINTF(AAC_D_INTR, ("istatus 0x%04x ", reason));

	/*
	 * Controller wants to talk to the log.  XXX Should we defer this?
	 */
	if ((reason & AAC_DB_PRINTF) != 0) {
		if (sc->sc_common->ac_printf[0] == '\0')
			sc->sc_common->ac_printf[0] = ' ';
		printf("%s: WARNING: adapter logged message:\n",
			device_xname(sc->sc_dv));
		printf("%s:     %.*s", device_xname(sc->sc_dv),
			AAC_PRINTF_BUFSIZE, sc->sc_common->ac_printf);
		sc->sc_common->ac_printf[0] = '\0';
		AAC_QNOTIFY(sc, AAC_DB_PRINTF);
		claimed = 1;
	}

	/*
	 * Controller has a message for us?
	 */
	if ((reason & AAC_DB_COMMAND_READY) != 0) {
		aac_host_command(sc);
		claimed = 1;
	}

	/*
	 * Controller has a response for us?
	 */
	if ((reason & AAC_DB_RESPONSE_READY) != 0) {
		aac_host_response(sc);
		claimed = 1;
	}

	/*
	 * Spurious interrupts that we don't use - reset the mask and clear
	 * the interrupts.
	 */
	if ((reason & (AAC_DB_SYNC_COMMAND | AAC_DB_COMMAND_NOT_FULL |
            AAC_DB_RESPONSE_NOT_FULL)) != 0) {
		AAC_UNMASK_INTERRUPTS(sc);
		AAC_CLEAR_ISTATUS(sc, AAC_DB_SYNC_COMMAND |
		    AAC_DB_COMMAND_NOT_FULL | AAC_DB_RESPONSE_NOT_FULL);
		claimed = 1;
	}

	return (claimed);
}

/*
 * Handle notification of one or more FIBs coming from the controller.
 */
static void
aac_host_command(struct aac_softc *sc)
{
	struct aac_fib *fib;
	u_int32_t fib_size;

	for (;;) {
		if (aac_dequeue_fib(sc, AAC_HOST_NORM_CMD_QUEUE, &fib_size,
		    &fib))
			break;	/* nothing to do */

		bus_dmamap_sync(sc->sc_dmat, sc->sc_common_dmamap,
		    (char *)fib - (char *)sc->sc_common, sizeof(*fib),
		    BUS_DMASYNC_POSTREAD);

		switch (le16toh(fib->Header.Command)) {
		case AifRequest:
#ifdef notyet
			aac_handle_aif(sc,
			    (struct aac_aif_command *)&fib->data[0]);
#endif
			AAC_PRINT_FIB(sc, fib);
			break;
		default:
			aprint_error_dev(sc->sc_dv, "unknown command from controller\n");
			AAC_PRINT_FIB(sc, fib);
			break;
		}

		bus_dmamap_sync(sc->sc_dmat, sc->sc_common_dmamap,
		    (char *)fib - (char *)sc->sc_common, sizeof(*fib),
		    BUS_DMASYNC_PREREAD);

		if ((fib->Header.XferState == 0) ||
		    (fib->Header.StructType != AAC_FIBTYPE_TFIB)) {
			break; // continue; ???
		}

		/* XXX reply to FIBs requesting responses ?? */

		/* Return the AIF/FIB to the controller */
		if (le32toh(fib->Header.XferState) & AAC_FIBSTATE_FROMADAP) {
			u_int16_t	size;

			fib->Header.XferState |=
				htole32(AAC_FIBSTATE_DONEHOST);
			*(u_int32_t*)fib->data = htole32(ST_OK);

			/* XXX Compute the Size field? */
			size = le16toh(fib->Header.Size);
			if (size > sizeof(struct aac_fib)) {
				size = sizeof(struct aac_fib);
				fib->Header.Size = htole16(size);
			}

			/*
			 * Since we didn't generate this command, it can't
			 * go through the normal process.
			 */
			aac_enqueue_response(sc,
					AAC_ADAP_NORM_RESP_QUEUE, fib);
		}
	}
}

/*
 * Handle notification of one or more FIBs completed by the controller
 */
static void
aac_host_response(struct aac_softc *sc)
{
	struct aac_ccb *ac;
	struct aac_fib *fib;
	u_int32_t fib_size;

	/*
	 * Look for completed FIBs on our queue.
	 */
	for (;;) {
		if (aac_dequeue_fib(sc, AAC_HOST_NORM_RESP_QUEUE, &fib_size,
		    &fib))
			break;	/* nothing to do */

		if ((fib->Header.SenderData & 0x80000000) == 0) {
			/* Not valid; not sent by us. */
			AAC_PRINT_FIB(sc, fib);
		} else {
			ac = (struct aac_ccb *)(sc->sc_ccbs +
			    (fib->Header.SenderData & 0x7fffffff));
			fib->Header.SenderData = 0;
			SIMPLEQ_INSERT_TAIL(&sc->sc_ccb_complete, ac, ac_chain);
		}
	}

	/*
	 * Deal with any completed commands.
	 */
	while ((ac = SIMPLEQ_FIRST(&sc->sc_ccb_complete)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_ccb_complete, ac_chain);
		ac->ac_flags |= AAC_CCB_COMPLETED;

		if (ac->ac_intr != NULL)
			(*ac->ac_intr)(ac);
		else
			wakeup(ac);
	}

	/*
	 * Try to submit more commands.
	 */
	if (! SIMPLEQ_EMPTY(&sc->sc_ccb_queue))
		aac_ccb_enqueue(sc, NULL);
}

/*
 * Send a synchronous command to the controller and wait for a result.
 */
static int
aac_sync_command(struct aac_softc *sc, u_int32_t command, u_int32_t arg0,
		 u_int32_t arg1, u_int32_t arg2, u_int32_t arg3, u_int32_t *sp)
{
	int i;
	u_int32_t status;
	int s;

	s = splbio();

	/* Populate the mailbox. */
	AAC_SET_MAILBOX(sc, command, arg0, arg1, arg2, arg3);

	/* Ensure the sync command doorbell flag is cleared. */
	AAC_CLEAR_ISTATUS(sc, AAC_DB_SYNC_COMMAND);

	/* ... then set it to signal the adapter. */
	AAC_QNOTIFY(sc, AAC_DB_SYNC_COMMAND);
	DELAY(AAC_SYNC_DELAY);

	/* Spin waiting for the command to complete. */
	for (i = 0; i < AAC_IMMEDIATE_TIMEOUT * 1000; i++) {
		if (AAC_GET_ISTATUS(sc) & AAC_DB_SYNC_COMMAND)
			break;
		DELAY(1000);
	}
	if (i == AAC_IMMEDIATE_TIMEOUT * 1000) {
		splx(s);
		return (EIO);
	}

	/* Clear the completion flag. */
	AAC_CLEAR_ISTATUS(sc, AAC_DB_SYNC_COMMAND);

	/* Get the command status. */
	status = AAC_GET_MAILBOXSTATUS(sc);
	splx(s);
	if (sp != NULL)
		*sp = status;

	return (0);	/* XXX Check command return status? */
}

/*
 * Send a synchronous FIB to the controller and wait for a result.
 */
static int
aac_sync_fib(struct aac_softc *sc, u_int32_t command, u_int32_t xferstate,
	     void *data, u_int16_t datasize, void *result,
	     u_int16_t *resultsize)
{
	struct aac_fib *fib;
	u_int32_t fibpa, status;

	fib = &sc->sc_common->ac_sync_fib;
	fibpa = sc->sc_common_seg.ds_addr +
	    offsetof(struct aac_common, ac_sync_fib);

	if (datasize > AAC_FIB_DATASIZE)
		return (EINVAL);

	/*
	 * Set up the sync FIB.
	 */
	fib->Header.XferState = htole32(AAC_FIBSTATE_HOSTOWNED |
	    AAC_FIBSTATE_INITIALISED | AAC_FIBSTATE_EMPTY | xferstate);
	fib->Header.Command = htole16(command);
	fib->Header.StructType = AAC_FIBTYPE_TFIB;
	fib->Header.Size = htole16(sizeof(*fib) + datasize);
	fib->Header.SenderSize = htole16(sizeof(*fib));
	fib->Header.SenderFibAddress = 0; /* not needed */
	fib->Header.ReceiverFibAddress = htole32(fibpa);

	/*
	 * Copy in data.
	 */
	if (data != NULL) {
		memcpy(fib->data, data, datasize);
		fib->Header.XferState |=
		    htole32(AAC_FIBSTATE_FROMHOST | AAC_FIBSTATE_NORM);
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_common_dmamap,
	    (char *)fib - (char *)sc->sc_common, sizeof(*fib),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	/*
	 * Give the FIB to the controller, wait for a response.
	 */
	if (aac_sync_command(sc, AAC_MONKER_SYNCFIB, fibpa, 0, 0, 0, &status))
		return (EIO);
	if (status != 1) {
		printf("%s: syncfib command %04x status %08x\n",
			device_xname(sc->sc_dv), command, status);
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_common_dmamap,
	    (char *)fib - (char *)sc->sc_common, sizeof(*fib),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);

	/*
	 * Copy out the result
	 */
	if (result != NULL) {
		*resultsize = le16toh(fib->Header.Size) - sizeof(fib->Header);
		memcpy(result, fib->data, *resultsize);
	}

	return (0);
}

struct aac_ccb *
aac_ccb_alloc(struct aac_softc *sc, int flags)
{
	struct aac_ccb *ac;
	int s;

	AAC_DPRINTF(AAC_D_QUEUE, ("aac_ccb_alloc(%p, 0x%x) ", sc, flags));

	s = splbio();
	ac = SIMPLEQ_FIRST(&sc->sc_ccb_free);
	if (ac == NULL) {
		if (aac_alloc_commands(sc)) {
			splx(s);
			return NULL;
		}
		ac = SIMPLEQ_FIRST(&sc->sc_ccb_free);
	}
#ifdef DIAGNOSTIC
	if (ac == NULL)
		panic("aac_ccb_get: no free CCBS");
#endif
	SIMPLEQ_REMOVE_HEAD(&sc->sc_ccb_free, ac_chain);
	splx(s);

	ac->ac_flags = flags;
	return (ac);
}

void
aac_ccb_free(struct aac_softc *sc, struct aac_ccb *ac)
{
	int s;

	AAC_DPRINTF(AAC_D_QUEUE, ("aac_ccb_free(%p, %p) ", sc, ac));

	ac->ac_flags = 0;
	ac->ac_intr = NULL;
	ac->ac_fib->Header.XferState = htole32(AAC_FIBSTATE_EMPTY);
	ac->ac_fib->Header.StructType = AAC_FIBTYPE_TFIB;
	ac->ac_fib->Header.Flags = 0;
	ac->ac_fib->Header.SenderSize = htole16(sc->sc_max_fib_size);

#ifdef AAC_DEBUG
	/*
	 * These are duplicated in aac_ccb_submit() to cover the case where
	 * an intermediate stage may have destroyed them.  They're left
	 * initialised here for debugging purposes only.
	 */
	ac->ac_fib->Header.SenderFibAddress =
	    htole32(((u_int32_t) (ac - sc->sc_ccbs)) << 2);
	ac->ac_fib->Header.ReceiverFibAddress = htole32(ac->ac_fibphys);
#endif

	s = splbio();
	SIMPLEQ_INSERT_HEAD(&sc->sc_ccb_free, ac, ac_chain);
	splx(s);
}

int
aac_ccb_map(struct aac_softc *sc, struct aac_ccb *ac)
{
	int error;

	AAC_DPRINTF(AAC_D_QUEUE, ("aac_ccb_map(%p, %p) ", sc, ac));

#ifdef DIAGNOSTIC
	if ((ac->ac_flags & AAC_CCB_MAPPED) != 0)
		panic("aac_ccb_map: already mapped");
#endif

	error = bus_dmamap_load(sc->sc_dmat, ac->ac_dmamap_xfer, ac->ac_data,
	    ac->ac_datalen, NULL, BUS_DMA_NOWAIT | BUS_DMA_STREAMING |
	    ((ac->ac_flags & AAC_CCB_DATA_IN) ? BUS_DMA_READ : BUS_DMA_WRITE));
	if (error) {
		printf("%s: aac_ccb_map: ", device_xname(sc->sc_dv));
		if (error == EFBIG)
			printf("more than %d DMA segs\n", sc->sc_max_sgs);
		else
			printf("error %d loading DMA map\n", error);
		return (error);
	}

	bus_dmamap_sync(sc->sc_dmat, ac->ac_dmamap_xfer, 0, ac->ac_datalen,
	    (ac->ac_flags & AAC_CCB_DATA_IN) ? BUS_DMASYNC_PREREAD :
	    BUS_DMASYNC_PREWRITE);

#ifdef DIAGNOSTIC
	ac->ac_flags |= AAC_CCB_MAPPED;
#endif
	return (0);
}

void
aac_ccb_unmap(struct aac_softc *sc, struct aac_ccb *ac)
{

	AAC_DPRINTF(AAC_D_QUEUE, ("aac_ccb_unmap(%p, %p) ", sc, ac));

#ifdef DIAGNOSTIC
	if ((ac->ac_flags & AAC_CCB_MAPPED) == 0)
		panic("aac_ccb_unmap: not mapped");
#endif

	bus_dmamap_sync(sc->sc_dmat, ac->ac_dmamap_xfer, 0, ac->ac_datalen,
	    (ac->ac_flags & AAC_CCB_DATA_IN) ? BUS_DMASYNC_POSTREAD :
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, ac->ac_dmamap_xfer);

#ifdef DIAGNOSTIC
	ac->ac_flags &= ~AAC_CCB_MAPPED;
#endif
}

void
aac_ccb_enqueue(struct aac_softc *sc, struct aac_ccb *ac)
{
	int s;

	AAC_DPRINTF(AAC_D_QUEUE, ("aac_ccb_enqueue(%p, %p) ", sc, ac));

	s = splbio();

	if (ac != NULL)
		SIMPLEQ_INSERT_TAIL(&sc->sc_ccb_queue, ac, ac_chain);

	while ((ac = SIMPLEQ_FIRST(&sc->sc_ccb_queue)) != NULL) {
		if (aac_ccb_submit(sc, ac))
			break;
		SIMPLEQ_REMOVE_HEAD(&sc->sc_ccb_queue, ac_chain);
	}

	splx(s);
}

int
aac_ccb_submit(struct aac_softc *sc, struct aac_ccb *ac)
{
	struct aac_fibmap *fm;
	u_int32_t acidx;

	AAC_DPRINTF(AAC_D_QUEUE, ("aac_ccb_submit(%p, %p) ", sc, ac));

	acidx = (u_int32_t) (ac - sc->sc_ccbs);
	/* Fix up the address values. */
	ac->ac_fib->Header.SenderFibAddress = htole32(acidx << 2);
	ac->ac_fib->Header.ReceiverFibAddress = htole32(ac->ac_fibphys);

	/* Save a pointer to the command for speedy reverse-lookup. */
	ac->ac_fib->Header.SenderData = acidx | 0x80000000;

	fm = ac->ac_fibmap;
	bus_dmamap_sync(sc->sc_dmat, fm->fm_fibmap,
	    (char *)ac->ac_fib - (char *)fm->fm_fibs, sc->sc_max_fib_size,
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	/* Put the FIB on the outbound queue. */
	if (sc->sc_quirks & AAC_QUIRK_NEW_COMM) {
		int count = 10000000L;
		while (AAC_SEND_COMMAND(sc, ac) != 0) {
			if (--count == 0) {
				panic("aac: fixme!");
				return EAGAIN;
			}
			DELAY(5);
		}
		return 0;
	} else {
		return (aac_enqueue_fib(sc, AAC_ADAP_NORM_CMD_QUEUE, ac));
	}
}

int
aac_ccb_poll(struct aac_softc *sc, struct aac_ccb *ac, int timo)
{
	int rv, s;

	AAC_DPRINTF(AAC_D_QUEUE, ("aac_ccb_poll(%p, %p, %d) ", sc, ac, timo));

	s = splbio();

	if ((rv = aac_ccb_submit(sc, ac)) != 0) {
		splx(s);
		return (rv);
	}

	for (timo *= 1000; timo != 0; timo--) {
		if (sc->sc_quirks & AAC_QUIRK_NEW_COMM)
			aac_new_intr(sc);
		else
			aac_intr(sc);
		if ((ac->ac_flags & AAC_CCB_COMPLETED) != 0)
			break;
		DELAY(100);
	}

	splx(s);
	return (timo == 0);
}

/*
 * Atomically insert an entry into the nominated queue, returns 0 on success
 * or EBUSY if the queue is full.
 *
 * XXX Note that it would be more efficient to defer notifying the
 * controller in the case where we may be inserting several entries in rapid
 * succession, but implementing this usefully is difficult.
 */
static int
aac_enqueue_fib(struct aac_softc *sc, int queue, struct aac_ccb *ac)
{
	u_int32_t fib_size, fib_addr, pi, ci;

	fib_size = le16toh(ac->ac_fib->Header.Size);
	fib_addr = le32toh(ac->ac_fib->Header.ReceiverFibAddress);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_common_dmamap,
	    (char *)sc->sc_common->ac_qbuf - (char *)sc->sc_common,
	    sizeof(sc->sc_common->ac_qbuf),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);

	/* Get the producer/consumer indices.  */
	pi = le32toh(sc->sc_queues->qt_qindex[queue][AAC_PRODUCER_INDEX]);
	ci = le32toh(sc->sc_queues->qt_qindex[queue][AAC_CONSUMER_INDEX]);

	/* Wrap the queue? */
	if (pi >= aac_qinfo[queue].size)
		pi = 0;

	/* Check for queue full. */
	if ((pi + 1) == ci)
		return (EAGAIN);

	/* Populate queue entry. */
	(sc->sc_qentries[queue] + pi)->aq_fib_size = htole32(fib_size);
	(sc->sc_qentries[queue] + pi)->aq_fib_addr = htole32(fib_addr);

	/* Update producer index. */
	sc->sc_queues->qt_qindex[queue][AAC_PRODUCER_INDEX] = htole32(pi + 1);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_common_dmamap,
	    (char *)sc->sc_common->ac_qbuf - (char *)sc->sc_common,
	    sizeof(sc->sc_common->ac_qbuf),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	/* Notify the adapter if we know how. */
	if (aac_qinfo[queue].notify != 0)
		AAC_QNOTIFY(sc, aac_qinfo[queue].notify);

	return (0);
}

/*
 * Atomically remove one entry from the nominated queue, returns 0 on success
 * or ENOENT if the queue is empty.
 */
static int
aac_dequeue_fib(struct aac_softc *sc, int queue, u_int32_t *fib_size,
		struct aac_fib **fib_addr)
{
	struct aac_fibmap *fm;
	struct aac_ccb *ac;
	u_int32_t pi, ci, idx;
	int notify;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_common_dmamap,
	    (char *)sc->sc_common->ac_qbuf - (char *)sc->sc_common,
	    sizeof(sc->sc_common->ac_qbuf),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);

	/* Get the producer/consumer indices. */
	pi = le32toh(sc->sc_queues->qt_qindex[queue][AAC_PRODUCER_INDEX]);
	ci = le32toh(sc->sc_queues->qt_qindex[queue][AAC_CONSUMER_INDEX]);

	/* Check for queue empty. */
	if (ci == pi)
		return (ENOENT);

	notify = 0;
	if (ci == pi + 1)
		notify = 1;

	/* Wrap the queue? */
	if (ci >= aac_qinfo[queue].size)
		ci = 0;

	/* Fetch the entry. */
	*fib_size = le32toh((sc->sc_qentries[queue] + ci)->aq_fib_size);

	switch (queue) {
	case AAC_HOST_NORM_CMD_QUEUE:
	case AAC_HOST_HIGH_CMD_QUEUE:
		idx = le32toh((sc->sc_qentries[queue] + ci)->aq_fib_addr);
		idx /= sizeof(struct aac_fib);
		*fib_addr = &sc->sc_common->ac_fibs[idx];
		break;
	case AAC_HOST_NORM_RESP_QUEUE:
	case AAC_HOST_HIGH_RESP_QUEUE:
		idx = le32toh((sc->sc_qentries[queue] + ci)->aq_fib_addr);
		ac = sc->sc_ccbs + (idx >> 2);
		*fib_addr = ac->ac_fib;
		if (idx & 0x01) {
			fm = ac->ac_fibmap;
			bus_dmamap_sync(sc->sc_dmat, fm->fm_fibmap,
			    (char *)ac->ac_fib - (char *)fm->fm_fibs,
			    sc->sc_max_fib_size,
			    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);
			ac->ac_fib->Header.XferState |=
				htole32(AAC_FIBSTATE_DONEADAP);
			*((u_int32_t*)(ac->ac_fib->data)) =
				htole32(AAC_ERROR_NORMAL);
		}
		break;
	default:
		panic("Invalid queue in aac_dequeue_fib()");
		break;
	}

	/* Update consumer index. */
	sc->sc_queues->qt_qindex[queue][AAC_CONSUMER_INDEX] = ci + 1;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_common_dmamap,
	    (char *)sc->sc_common->ac_qbuf - (char *)sc->sc_common,
	    sizeof(sc->sc_common->ac_qbuf),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	/* If we have made the queue un-full, notify the adapter. */
	if (notify && (aac_qinfo[queue].notify != 0))
		AAC_QNOTIFY(sc, aac_qinfo[queue].notify);

	return (0);
}

/*
 * Put our response to an adapter-initiated fib (AIF) on the response queue.
 */
static int
aac_enqueue_response(struct aac_softc *sc, int queue, struct aac_fib *fib)
{
	u_int32_t fib_size, fib_addr, pi, ci;

	fib_size = le16toh(fib->Header.Size);
	fib_addr = fib->Header.SenderFibAddress;
	fib->Header.ReceiverFibAddress = fib_addr;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_common_dmamap,
	    (char *)sc->sc_common->ac_qbuf - (char *)sc->sc_common,
	    sizeof(sc->sc_common->ac_qbuf),
	    BUS_DMASYNC_POSTWRITE | BUS_DMASYNC_POSTREAD);

	/* Get the producer/consumer indices.  */
	pi = le32toh(sc->sc_queues->qt_qindex[queue][AAC_PRODUCER_INDEX]);
	ci = le32toh(sc->sc_queues->qt_qindex[queue][AAC_CONSUMER_INDEX]);

	/* Wrap the queue? */
	if (pi >= aac_qinfo[queue].size)
		pi = 0;

	/* Check for queue full. */
	if ((pi + 1) == ci)
		return (EAGAIN);

	/* Populate queue entry. */
	(sc->sc_qentries[queue] + pi)->aq_fib_size = htole32(fib_size);
	(sc->sc_qentries[queue] + pi)->aq_fib_addr = htole32(fib_addr);

	/* Update producer index. */
	sc->sc_queues->qt_qindex[queue][AAC_PRODUCER_INDEX] = htole32(pi + 1);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_common_dmamap,
	    (char *)sc->sc_common->ac_qbuf - (char *)sc->sc_common,
	    sizeof(sc->sc_common->ac_qbuf),
	    BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD);

	/* Notify the adapter if we know how. */
	if (aac_qinfo[queue].notify != 0)
		AAC_QNOTIFY(sc, aac_qinfo[queue].notify);

	return (0);
}

#ifdef AAC_DEBUG
/*
 * Print a FIB
 */
static void
aac_print_fib(struct aac_softc *sc, struct aac_fib *fib,
    const char *caller)
{
	struct aac_blockread *br;
	struct aac_blockwrite *bw;
	struct aac_sg_table *sg;
	char tbuf[512];
	int i;

	printf("%s: FIB @ %p\n", caller, fib);
	snprintb(tbuf, sizeof(tbuf),
	    "\20"
	    "\1HOSTOWNED"
	    "\2ADAPTEROWNED"
	    "\3INITIALISED"
	    "\4EMPTY"
	    "\5FROMPOOL"
	    "\6FROMHOST"
	    "\7FROMADAP"
	    "\10REXPECTED"
	    "\11RNOTEXPECTED"
	    "\12DONEADAP"
	    "\13DONEHOST"
	    "\14HIGH"
	    "\15NORM"
	    "\16ASYNC"
	    "\17PAGEFILEIO"
	    "\20SHUTDOWN"
	    "\21LAZYWRITE"
	    "\22ADAPMICROFIB"
	    "\23BIOSFIB"
	    "\24FAST_RESPONSE"
	    "\25APIFIB\n", le32toh(fib->Header.XferState));

	printf("  XferState       %s\n", tbuf);
	printf("  Command         %d\n", le16toh(fib->Header.Command));
	printf("  StructType      %d\n", fib->Header.StructType);
	printf("  Flags           0x%x\n", fib->Header.Flags);
	printf("  Size            %d\n", le16toh(fib->Header.Size));
	printf("  SenderSize      %d\n", le16toh(fib->Header.SenderSize));
	printf("  SenderAddress   0x%x\n",
	    le32toh(fib->Header.SenderFibAddress));
	printf("  ReceiverAddress 0x%x\n",
	    le32toh(fib->Header.ReceiverFibAddress));
	printf("  SenderData      0x%x\n", fib->Header.SenderData);

	switch (fib->Header.Command) {
	case ContainerCommand: {
		br = (struct aac_blockread *)fib->data;
		bw = (struct aac_blockwrite *)fib->data;
		sg = NULL;

		if (le32toh(br->Command) == VM_CtBlockRead) {
			printf("  BlockRead: container %d  0x%x/%d\n",
			    le32toh(br->ContainerId), le32toh(br->BlockNumber),
			    le32toh(br->ByteCount));
			sg = &br->SgMap;
		}
		if (le32toh(bw->Command) == VM_CtBlockWrite) {
			printf("  BlockWrite: container %d  0x%x/%d (%s)\n",
			    le32toh(bw->ContainerId), le32toh(bw->BlockNumber),
			    le32toh(bw->ByteCount),
			    le32toh(bw->Stable) == CSTABLE ?
			    "stable" : "unstable");
			sg = &bw->SgMap;
		}
		if (sg != NULL) {
			printf("  %d s/g entries\n", le32toh(sg->SgCount));
			for (i = 0; i < le32toh(sg->SgCount); i++)
				printf("  0x%08x/%d\n",
				    le32toh(sg->SgEntry[i].SgAddress),
				    le32toh(sg->SgEntry[i].SgByteCount));
		}
		break;
	}
	default:
		// dump first 32 bytes of fib->data
		printf("  Raw data:");
		for (i = 0; i < 32; i++)
			printf(" %02x", fib->data[i]);
		printf("\n");
		break;
	}
}
#endif /* AAC_DEBUG */
