/*	$NetBSD: autoconf.c,v 1.8 2013/09/20 16:49:00 pooka Exp $	*/

/*
 * Copyright (c) 2009 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: autoconf.c,v 1.8 2013/09/20 16:49:00 pooka Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>

static int	mainbus_match(device_t, cfdata_t, void *);
static void	mainbus_attach(device_t, device_t, void *);
static int	mainbus_search(device_t, cfdata_t, const int *, void *);

struct mainbus_softc {
	int mb_nada;
};

/*
 * Initial lists as required by autoconf(9).  The data from ioconf.c
 * is patched in by rump_mainbus_init().
 */
const struct cfattachinit cfattachinit[] = {
	{ NULL, NULL },
};
struct cfdata cfdata[] = {
	{ NULL, NULL, 0, FSTATE_NOTFOUND, NULL, 0, NULL}, /* replaced by init */
	{ NULL, NULL, 0, FSTATE_NOTFOUND, NULL, 0, NULL},
};
struct cfdriver * const cfdriver_list_initial[] = {
	NULL,
};

#include "ioconf.c"

CFATTACH_DECL_NEW(mainbus, sizeof(struct mainbus_softc),
	mainbus_match, mainbus_attach, NULL, NULL);

const short cfroots[] = {
	0, /* mainbus */
	-1
};

/* actually used */
#define MAXPDEVS 256
struct pdevinit pdevinit[MAXPDEVS] = {{NULL,0}, }; /* XXX: static limit */
static int pdev_total = 0;

#include "rump_dev_private.h"

void
rump_pdev_add(void (*pdev_attach)(int), int pdev_count)
{
	struct pdevinit *pdev_new;

	KASSERT(cold);

	pdev_new = &pdevinit[pdev_total];
	pdev_new->pdev_attach = pdev_attach;
	pdev_new->pdev_count = pdev_count;

	pdev_total++;
	KASSERT(pdev_total < MAXPDEVS);
}

void
rump_pdev_finalize()
{

	rump_pdev_add(NULL, 0);
}

static int
mainbus_match(device_t parent, cfdata_t match, void *aux)
{

	return 1;
}

static void
mainbus_attach(device_t parent, device_t self, void *aux)
{

	aprint_normal("\n");
	config_search_ia(mainbus_search, self, "mainbus", NULL);
}

static int
mainbus_search(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct mainbus_attach_args maa;

	maa.maa_unit = cf->cf_unit;
	if (config_match(parent, cf, &maa) > 0)
		config_attach(parent, cf, &maa, NULL);

	return 0;
}

void
rump_mainbus_init(void)
{

	/* replace cfdata[0] to a state expected by autoconf(9) */
	memcpy(&cfdata[0], &cfdata_ioconf_mainbus[0], sizeof(cfdata[0]));
}

void
rump_mainbus_attach(void)
{
	const struct cfattachinit *cfai = &cfattach_ioconf_mainbus[0];

	config_cfdata_attach(cfdata, 0);
	config_cfdriver_attach(cfdriver_ioconf_mainbus[0]);
	config_cfattach_attach(cfai->cfai_name, cfai->cfai_list[0]);
}
