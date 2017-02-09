/*	$NetBSD: hil_gpib.c,v 1.12 2012/10/27 17:18:16 chs Exp $	*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hil_gpib.c,v 1.12 2012/10/27 17:18:16 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <dev/gpib/gpibvar.h>

#ifdef DEBUG
int     hildebug = 0;
#define HDB_FOLLOW      0x01
#define HDB_MMAP	0x02
#define HDB_MASK	0x04
#define HDB_CONFIG      0x08
#define HDB_KEYBOARD    0x10
#define HDB_IDMODULE    0x20
#define HDB_EVENTS      0x80
#define DPRINTF(mask, str)	if (hildebug & (mask)) printf str
#else
#define DPRINTF(mask, str)	/* nothing */
#endif

struct  hil_softc {
	gpib_chipset_tag_t sc_ic;
	gpib_handle_t sc_hdl;

	int	sc_address;		 /* GPIB address */
	int     sc_flags;
#define HILF_ALIVE	0x01
#define HILF_OPEN	0x02
#define HILF_UIO	0x04
#define HILF_TIMO	0x08
#define HILF_DELAY	0x10
};

int     hilmatch(device_t, cfdata_t, void *);
void    hilattach(device_t, device_t, void *);

const struct cfattach hil_ca = {
	sizeof(struct hil_softc), hilmatch, hilattach,
};

void	hilcallback(void *, int);
void	hilstart(void *);

int
hilmatch(device_t parent, cfdata_t match, void *aux)
{
	struct gpib_attach_args *ga = aux;
	u_int8_t *cmd = "SE;";
	u_int8_t stat;

	if (gpibsend(ga->ga_ic, ga->ga_address, -1, cmd, 3) != 3)
		return (0);
	if (gpibrecv(ga->ga_ic, ga->ga_address, -1, &stat, 1) != 1)
		return (0);
	printf("hilmatch: enable status byte 0x%x\n", stat);
	return (1);
}

void
hilattach(device_t parent, device_t self, void *aux)
{
	struct hil_softc *sc = device_private(self);
	struct gpib_attach_args *ga = aux;

	printf("\n");

	sc->sc_ic = ga->ga_ic;
	sc->sc_address = ga->ga_address;

	if (gpibregister(sc->sc_ic, sc->sc_address, hilcallback, sc,
	    &sc->sc_hdl)) {
		aprint_error_dev(self, "can't register callback\n");
		return;
	}

	sc->sc_flags = HILF_ALIVE;
}

void
hilcallback(void *v, int action)
{
	struct hil_softc *sc = v;

	DPRINTF(HDB_FOLLOW, ("hilcallback: v=%p, action=%d\n", v, action));

	switch (action) {
	case GPIBCBF_START:
		hilstart(sc);
	case GPIBCBF_INTR:
		/* no-op */
		break;
#ifdef DEBUG
	default:
		DPRINTF(HDB_FOLLOW, ("hilcallback: unknown action %d\n",
		    action));
		break;
#endif
	}
}

void
hilstart(void *v)
{
	struct hil_softc *sc = v;

	DPRINTF(HDB_FOLLOW, ("hilstart\n"));

	sc->sc_flags &= ~HILF_DELAY;
}
