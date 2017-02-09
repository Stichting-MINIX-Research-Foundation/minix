#include "ioconf.h"
/*
 * MACHINE GENERATED: DO NOT EDIT
 *
 * ioconf.c, from "/home/boricj/Documents/Projets/minix/src/sys/rump/dev/lib/libubt/UBT.ioconf"
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/mount.h>

static const struct cfiattrdata usbififcf_iattrdata = {
	"usbifif", 6, {
		{ "port", "-1", -1 },
		{ "configuration", "-1", -1 },
		{ "interface", "-1", -1 },
		{ "vendor", "-1", -1 },
		{ "product", "-1", -1 },
		{ "release", "-1", -1 },
	}
};
static const struct cfiattrdata usbdevifcf_iattrdata = {
	"usbdevif", 6, {
		{ "port", "-1", -1 },
		{ "configuration", "-1", -1 },
		{ "interface", "-1", -1 },
		{ "vendor", "-1", -1 },
		{ "product", "-1", -1 },
		{ "release", "-1", -1 },
	}
};
static const struct cfiattrdata btbuscf_iattrdata = {
	"btbus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata bthubcf_iattrdata = {
	"bthub", 0, {
		{ NULL, NULL, 0 },
	}
};

static const struct cfiattrdata * const ubt_attrs[] = { &btbuscf_iattrdata, NULL };
CFDRIVER_DECL(ubt, DV_DULL, ubt_attrs);

static const struct cfiattrdata * const bthub_attrs[] = { &bthubcf_iattrdata, NULL };
CFDRIVER_DECL(bthub, DV_DULL, bthub_attrs);


static struct cfdriver * const cfdriver_ioconf_ubt[] = {
	&ubt_cd,
	&bthub_cd,
	NULL
};

extern struct cfattach ubt_ca;
extern struct cfattach bthub_ca;

/* locators */
static int loc[6] = {
	-1, -1, -1, -1, -1, -1,
};

static const struct cfparent pspec0 = {
	"usbdevif", "uhub", DVUNIT_ANY
};
static const struct cfparent pspec1 = {
	"btbus", "ubt", DVUNIT_ANY
};

#define NORM FSTATE_NOTFOUND
#define STAR FSTATE_STAR

static struct cfdata cfdata_ioconf_ubt[] = {
    /* driver           attachment    unit state      loc   flags  pspec */
/*  0: ubt* at uhub? port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    { "ubt",		"ubt",		 0, STAR, loc+  0,      0, &pspec0 },
/*  1: bthub* at ubt? */
    { "bthub",		"bthub",	 0, STAR,    NULL,      0, &pspec1 },
    { NULL,		NULL,		 0,    0,    NULL,      0, NULL }
};

static struct cfattach * const ubt_cfattachinit[] = {
	&ubt_ca, NULL
};
static struct cfattach * const bthub_cfattachinit[] = {
	&bthub_ca, NULL
};

static const struct cfattachinit cfattach_ioconf_ubt[] = {
	{ "ubt", ubt_cfattachinit },
	{ "bthub", bthub_cfattachinit },
	{ NULL, NULL }
};
