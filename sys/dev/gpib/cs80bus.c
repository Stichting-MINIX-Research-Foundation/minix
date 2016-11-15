/*	$NetBSD: cs80bus.c,v 1.16 2012/10/27 17:18:16 chs Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gregory McGarry.
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
__KERNEL_RCSID(0, "$NetBSD: cs80bus.c,v 1.16 2012/10/27 17:18:16 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/endian.h>
#include <sys/malloc.h>

#include <dev/gpib/gpibvar.h>
#include <dev/gpib/cs80busvar.h>

#ifndef DEBUG
#define DEBUG
#endif

#ifdef DEBUG
int cs80busdebug = 0xff;
#define DBG_FOLLOW	0x01
#define DBG_STATUS	0x02
#define DBG_FAIL	0x04
#define DPRINTF(mask, str)	if (cs80busdebug & (mask)) printf str
#else
#define DPRINTF(mask, str)	/* nothing */
#endif

#include "locators.h"
#define	cs80buscf_slave		cf_loc[CS80BUSCF_SLAVE]
#define	cs80buscf_punit		cf_loc[CS80BUSCF_PUNIT]

int	cs80busmatch(device_t, cfdata_t, void *);
void	cs80busattach(device_t, device_t, void *);

CFATTACH_DECL_NEW(cs80bus, sizeof(struct cs80bus_softc),
	cs80busmatch, cs80busattach, NULL, NULL);

static int	cs80bus_alloc(struct cs80bus_softc *, int, int);
static int	cs80bussearch(device_t, cfdata_t,
			      const int *, void *);
static int	cs80busprint(void *, const char *);

/*
 * HP's CS80/SS80 command set can be found on `newer' devices, while
 * the HP's Amigo command set is used on before-you-were-born
 * devices.  Devices that respond to CS80/SS80 (and probably Amigo, too)
 * are tagged with a 16-bit ID.
 *
 * CS80/SS80 has a 2-level addressing scheme; slave, the analog
 * of a SCSI ID, and punit, the analog of a SCSI LUN.  Unforunately,
 * IDs are on a per-slave basis; punits are often used for disk
 * drives that have an accompanying tape drive on the second punit.
 *
 * We treat CS80/SS80 as an indirect bus.  However, since we are given
 * some ID information, it is unreasonable to disallow cloning of
 * CS80/SS80 devices.
 *
 * To deal with all of this, we use the semi-twisted scheme
 * in cs80bus_attach_children().  For each GPIB slave, we loop
 * through all of the possibly-configured children, allowing
 * them to modify the punit parameter (but NOT the slave!).
 *
 */

int
cs80busmatch(device_t parent, cfdata_t match, void *aux)
{

	return (1);
}

void
cs80busattach(device_t parent, device_t self, void *aux)
{
	struct cs80bus_softc *sc = device_private(self);
	struct gpib_attach_args *ga = aux;
	struct cs80bus_attach_args ca;
	int slave;
	u_int16_t id;

	printf("\n");

	sc->sc_dev = self;
	sc->sc_ic = ga->ga_ic;

	for (slave = 0; slave < 8; slave++) {

		if (gpib_isalloc(device_private(device_parent(sc->sc_dev)), slave))
			continue;

		if (gpibrecv(sc->sc_ic, GPIB_BROADCAST_ADDR,
		    slave, &id, 2) != 2)
			continue;

		BE16TOH(id);

		DPRINTF(DBG_STATUS, ("cs80busattach: found id 0x%x\n", id));

		if ((id & 0x200) == 0)
			continue;

		ca.ca_ic = sc->sc_ic;
		ca.ca_slave = slave;
		ca.ca_id = id;

		(void)config_search_ia(cs80bussearch, sc->sc_dev, "cs80bus", &ca);
	}
}

int
cs80bussearch(device_t parent, cfdata_t cf, const int *ldesc, void *aux)
{
	struct cs80bus_softc *sc = device_private(parent);
	struct cs80bus_attach_args *ca = aux;

	/*
	 * Set punit if operator specified one in the kernel
	 * configuration file.
	 */
	if (cf->cs80buscf_punit != CS80BUSCF_PUNIT_DEFAULT &&
	    cf->cs80buscf_punit < CS80BUS_NPUNITS)
		ca->ca_punit = cf->cs80buscf_punit;
	else
		/* default punit */
		ca->ca_punit = 0;

	DPRINTF(DBG_FOLLOW, ("cs80bussearch: id=0x%x slave=%d punit=%d\n",
	    ca->ca_id, ca->ca_slave, ca->ca_punit));

	if (config_match(parent, cf, ca) > 0) {

		DPRINTF(DBG_FOLLOW,
		    ("cs80bussearch: got id=0x%x slave=%d punit %d\n",
		    ca->ca_id, ca->ca_slave, ca->ca_punit));

		/*
		 * The device probe has succeeded, and filled in
		 * the punit information.  Make sure the configuration
		 * allows for this slave/punit combination.
		 */
		if (cf->cs80buscf_slave != CS80BUSCF_SLAVE_DEFAULT &&
		    cf->cs80buscf_slave != ca->ca_slave)
			goto out;
		if (cf->cs80buscf_punit != CS80BUSCF_PUNIT_DEFAULT &&
		    cf->cs80buscf_punit != ca->ca_punit)
			goto out;

		/*
		 * Allocate the device's address from the bus's
		 * resource map.
		 */
		if (cs80bus_alloc(sc, ca->ca_slave, ca->ca_punit))
			goto out;

		/*
		 * This device is allowed; attach it.
		 */
		config_attach(parent, cf, ca, cs80busprint);
	}
out:
	return (0);
}

int
cs80busprint(void *aux, const char *pnp)
{
	struct cs80bus_attach_args *ca = aux;

	printf(" slave %d punit %d", ca->ca_slave, ca->ca_punit);
	return (UNCONF);
}

static int
cs80bus_alloc(struct cs80bus_softc *sc, int slave, int punit)
{

	DPRINTF(DBG_FOLLOW, ("cs80bus_alloc: sc=%p\n", sc));

	if (slave >= CS80BUS_NSLAVES || punit >= CS80BUS_NPUNITS)
		panic("cs80bus_alloc: device address out of range");

	gpib_alloc(device_private(device_parent(sc->sc_dev)), slave);

	if (sc->sc_rmap[slave][punit] == 0) {
		sc->sc_rmap[slave][punit] = 1;
		return (0);
	}
	return (1);
}



/*
 *  CS80/SS80 (secondary) command functions
 */

int
cs80describe(void *v, int slave, int punit, struct cs80_description *csd)
{
	struct cs80bus_softc *sc = v;
	struct cs80_describecmd desc;
	u_int8_t stat;

	DPRINTF(DBG_FOLLOW, ("cs80describe: sc=%p slave=%d\n", sc, slave));

        /*
         * Note command is always issued to unit 0.
         */

        desc.c_unit = CS80CMD_SUNIT(0);
        desc.c_vol = CS80CMD_SVOL(0);
	desc.c_cmd = CS80CMD_DESC;
        (void) gpibsend(sc->sc_ic, slave, CS80CMD_SCMD, &desc, sizeof(desc));
        (void) gpibrecv(sc->sc_ic, slave, CS80CMD_EXEC, csd, sizeof(*csd));
        (void) gpibrecv(sc->sc_ic, slave, CS80CMD_QSTAT, &stat, 1);
	if (stat != 0) {
		DPRINTF(DBG_FAIL, ("cs80describe: failed, stat=0x%x\n", stat));
		return (1);
	}
	BE16TOH(csd->d_iuw);
	BE16TOH(csd->d_cmaxxfr);
	BE16TOH(csd->d_sectsize);
	BE16TOH(csd->d_blocktime);
	BE16TOH(csd->d_uavexfr);
	BE16TOH(csd->d_retry);
	BE16TOH(csd->d_access);
	BE32TOH(csd->d_maxcylhead);
	BE16TOH(csd->d_maxsect);
	BE16TOH(csd->d_maxvsecth);
	BE32TOH(csd->d_maxvsectl);

	return (0);
}

int
cs80reset(void *v, int slave, int punit)
{
	struct cs80bus_softc *sc = v;
	struct cs80_clearcmd clear;
	struct cs80_srcmd sr;
	struct cs80_ssmcmd ssm;

	DPRINTF(DBG_FOLLOW, ("cs80reset: sc=%p slave=%d punit=%d\n", sc,
	    slave, punit));

	clear.c_unit = CS80CMD_SUNIT(punit);
	clear.c_cmd = CS80CMD_CLEAR;
	if (cs80send(sc, slave, punit, CS80CMD_TCMD, &clear, sizeof(clear))) {
		DPRINTF(DBG_FAIL, ("cs80reset: CLEAR failed\n"));
		return (1);
	}

	sr.c_unit = CS80CMD_SUNIT(15);		/* XXX */
	sr.c_nop = CS80CMD_NOP;
	sr.c_cmd = CS80CMD_SREL;
	sr.c_param = 0xc0;			/* XXX */
	if (cs80send(sc, slave, punit, CS80CMD_SCMD, &sr, sizeof(sr))) {
		DPRINTF(DBG_FAIL, ("cs80reset: SREL failed\n"));
		return (1);
	}

	ssm.c_unit = CS80CMD_SUNIT(punit);
	ssm.c_cmd = CS80CMD_SSM;
	ssm.c_refm = htobe16(REF_MASK);
	ssm.c_fefm = htobe16(FEF_MASK);
	ssm.c_aefm = htobe16(AEF_MASK);
	ssm.c_iefm = htobe16(IEF_MASK);
	if (cs80send(sc, slave, punit, CS80CMD_SCMD, &ssm, sizeof(ssm))) {
		DPRINTF(DBG_FAIL, ("cs80reset: SSM failed\n"));
		return (1);
	}

	return (0);
}

int
cs80status(void *v, int slave, int punit, struct cs80_stat *css)
{
	struct cs80bus_softc *sc = v;
	struct cs80_statuscmd rs;
	u_int8_t stat;

	rs.c_unit = CS80CMD_SUNIT(punit);
	rs.c_sram = CS80CMD_SRAM;
	rs.c_param = 0;		/* single vector (i.e. sector number) */
	rs.c_cmd = CS80CMD_STATUS;
	memset((void *)css, 0, sizeof(*css));
	(void) gpibsend(sc->sc_ic, slave, CS80CMD_SCMD, &rs, sizeof(rs));
	(void) gpibrecv(sc->sc_ic, slave, CS80CMD_EXEC, css, sizeof(*css));
	(void) gpibrecv(sc->sc_ic, slave, CS80CMD_QSTAT, &stat, 1);
	if (stat != 0) {
		DPRINTF(DBG_FAIL, ("cs80status: failed, stat=0x%x\n", stat));
		return (1);
	}
	BE16TOH(css->c_ref);
	BE16TOH(css->c_fef);
	BE16TOH(css->c_aef);
	BE16TOH(css->c_ief);
	BE32TOH(css->c_blk);

	return (0);
}

int
cs80setoptions(void *v, int slave, int punit, u_int8_t options)
{
	struct cs80bus_softc *sc = v;
	struct cs80_soptcmd opt;

	opt.c_unit = CS80CMD_SUNIT(punit);
	opt.c_nop = CS80CMD_NOP;
	opt.c_opt = CS80CMD_SOPT;
	opt.c_param = options;
	if (cs80send(sc, slave, punit, CS80CMD_SCMD, &opt, sizeof(opt))) {
		DPRINTF(DBG_FAIL, ("cs80setoptions: failed\n"));
		return (1);
	}

	return (0);
}

int
cs80send(void *v, int slave, int punit, int cmd, void *ptr, int cnt)
{
	struct cs80bus_softc *sc = v;
	u_int8_t *buf = ptr;
	u_int8_t stat;

	DPRINTF(DBG_FOLLOW,
	    ("cs80send: sc=%p slave=%d punit=%d cmd=%d ptr=%p cnt=%d\n", sc,
	    slave, punit, cmd, buf, cnt));

	if (gpibsend(sc->sc_ic, slave, cmd, buf, cnt) != cnt) {
		DPRINTF(DBG_FAIL, ("cs80send: SCMD failed\n"));
		return (1);
	}
	if (gpibswait(sc->sc_ic, slave)) {
		DPRINTF(DBG_FAIL, ("cs80send: wait failed\n"));
		return (1);
	}
	if (gpibrecv(sc->sc_ic, slave, CS80CMD_QSTAT, &stat, 1) != 1) {
		DPRINTF(DBG_FAIL, ("cs80send: QSTAT failed\n"));
		return (1);
	}
	if (stat != 0) {
		DPRINTF(DBG_FAIL, ("cs80send: failed, stat=0x%x\n", stat));
		return (1);
	}

	return (0);
}
