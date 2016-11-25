#include "ioconf.h"
/*
 * MACHINE GENERATED: DO NOT EDIT
 *
 * ioconf.c, from "/home/boricj/Documents/Projets/minix/src/sys/rump/dev/lib/libulpt/ULPT.ioconf"
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

CFDRIVER_DECL(ulpt, DV_DULL, NULL);


static struct cfdriver * const cfdriver_ioconf_ulpt[] = {
	&ulpt_cd,
	NULL
};

extern struct cfattach ulpt_ca;

/* locators */
static int loc[6] = {
	-1, -1, -1, -1, -1, -1,
};

static const struct cfparent pspec0 = {
	"usbifif", "uhub", DVUNIT_ANY
};

#define NORM FSTATE_NOTFOUND
#define STAR FSTATE_STAR

static struct cfdata cfdata_ioconf_ulpt[] = {
    /* driver           attachment    unit state      loc   flags  pspec */
/*  0: ulpt* at uhub? port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    { "ulpt",		"ulpt",		 0, STAR, loc+  0,      0, &pspec0 },
    { NULL,		NULL,		 0,    0,    NULL,      0, NULL }
};

static struct cfattach * const ulpt_cfattachinit[] = {
	&ulpt_ca, NULL
};

static const struct cfattachinit cfattach_ioconf_ulpt[] = {
	{ "ulpt", ulpt_cfattachinit },
	{ NULL, NULL }
};
