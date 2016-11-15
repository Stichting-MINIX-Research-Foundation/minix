/*	$NetBSD: fmv.c,v 1.10 2008/04/12 06:27:01 tsutsui Exp $	*/

/*
 * All Rights Reserved, Copyright (C) Fujitsu Limited 1995
 *
 * This software may be used, modified, copied, distributed, and sold, in
 * both source and binary form provided that the above copyright, these
 * terms and the following disclaimer are retained.  The name of the author
 * and/or the contributor may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND THE CONTRIBUTOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE CONTRIBUTOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION.
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions copyright (C) 1993, David Greenman.  This software may be used,
 * modified, copied, distributed, and sold, in both source and binary form
 * provided that the above copyright and these terms are retained.  Under no
 * circumstances is the author responsible for the proper functioning of this
 * software, nor does the author assume any responsibility for damages
 * incurred with its use.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fmv.c,v 1.10 2008/04/12 06:27:01 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>

#include <dev/ic/mb86960reg.h>
#include <dev/ic/mb86960var.h>
#include <dev/ic/fmvreg.h>
#include <dev/ic/fmvvar.h>

#ifdef FMV_DEBUG
#define DPRINTF	printf
#else
#define DPRINTF	while (/* CONSTCOND */0) printf
#endif

/*
 * Determine type and ethernet address.
 */
int
fmv_detect(bus_space_tag_t iot, bus_space_handle_t ioh, uint8_t *enaddr)
{
	int model, id, type;

	/* Get our station address from EEPROM. */
	bus_space_read_region_1(iot, ioh, FE_FMV4, enaddr, ETHER_ADDR_LEN);

	/* Make sure we got a valid station address. */
	if ((enaddr[0] & 0x03) != 0x00 ||
	    (enaddr[0] == 0x00 && enaddr[1] == 0x00 && enaddr[2] == 0x00)) {
		DPRINTF("%s: invalid ethernet address\n", __func__);
		return 0;
	}

	/* Determine the card type. */
	model = bus_space_read_1(iot, ioh, FE_FMV0) & FE_FMV0_MODEL;
	id    = bus_space_read_1(iot, ioh, FE_FMV1) & FE_FMV1_CARDID_REV;

	switch (model) {
	case FE_FMV0_MODEL_FMV181:
		type = FE_TYPE_FMV181;
		if (id == FE_FMV1_CARDID_REV_A)
			type = FE_TYPE_FMV181A;
		break;
	case FE_FMV0_MODEL_FMV182:
		type = FE_TYPE_FMV182;
		if (id == FE_FMV1_CARDID_REV_A)
			type = FE_TYPE_FMV182A;
		else if (id == FE_FMV1_CARDID_PNP)
			type = FE_TYPE_FMV184;
		break;
	case FE_FMV0_MODEL_FMV183:
		type = FE_TYPE_FMV183;
		break;
	default:
		type = 0;
		DPRINTF("%s: unknown card\n", __func__);
		break;
	}

	return type;
}

void
fmv_attach(struct mb86960_softc *sc)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	const char *typestr;
	int type;
	uint8_t myea[ETHER_ADDR_LEN];

	iot = sc->sc_bst;
	ioh = sc->sc_bsh;

	/* Determine the card type. */
	type = fmv_detect(iot, ioh, myea);
	switch (type) {
	case FE_TYPE_FMV181:
		typestr = "FMV-181";
		break;
	case FE_TYPE_FMV181A:
		typestr = "FMV-181A";
		break;
	case FE_TYPE_FMV182:
		typestr = "FMV-182";
		break;
	case FE_TYPE_FMV182A:
		typestr = "FMV-182A";
		break;
	case FE_TYPE_FMV183:
		typestr = "FMV-183";
		break;
	case FE_TYPE_FMV184:
		typestr = "FMV-184";
		break;
	default:
	  	/* Unknown card type: maybe a new model, but... */
		aprint_normal("\n");
		panic("%s: unknown FMV-18x card", device_xname(sc->sc_dev));
	}

	aprint_normal(": %s Ethernet\n", typestr);

	/* This interface is always enabled. */
	sc->sc_stat |= FE_STAT_ENABLED;

	/*
	 * Minimum initialization of the hardware.
	 * We write into registers; hope I/O ports have no
	 * overlap with other boards.
	 */

	/* Initialize ASIC. */
	bus_space_write_1(iot, ioh, FE_FMV3, 0);
	bus_space_write_1(iot, ioh, FE_FMV10, 0);

	/* Wait for a while.  I'm not sure this is necessary.  FIXME */
	delay(200);

	/*
	 * Do generic MB86960 attach.
	 */
	mb86960_attach(sc, myea);

	/* Is this really needs to be done here? XXX */
	/* Turn the "master interrupt control" flag of ASIC on. */
	bus_space_write_1(iot, ioh, FE_FMV3, FE_FMV3_ENABLE_FLAG);

	mb86960_config(sc, NULL, 0, 0);
}
