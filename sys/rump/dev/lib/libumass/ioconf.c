#include "ioconf.h"
/*
 * MACHINE GENERATED: DO NOT EDIT
 *
 * ioconf.c, from "/home/boricj/Documents/Projets/minix/src/sys/rump/dev/lib/libumass/UMASS.ioconf"
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/mount.h>

static const struct cfiattrdata atapicf_iattrdata = {
	"atapi", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata scsicf_iattrdata = {
	"scsi", 1, {
		{ "channel", "-1", -1 },
	}
};
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
static const struct cfiattrdata ata_hlcf_iattrdata = {
	"ata_hl", 1, {
		{ "drive", "-1", -1 },
	}
};

static const struct cfiattrdata * const umass_attrs[] = { &ata_hlcf_iattrdata, &atapicf_iattrdata, &scsicf_iattrdata, NULL };
CFDRIVER_DECL(umass, DV_DULL, umass_attrs);


static struct cfdriver * const cfdriver_ioconf_umass[] = {
	&umass_cd,
	NULL
};

extern struct cfattach umass_ca;

/* locators */
static int loc[6] = {
	-1, -1, -1, -1, -1, -1,
};

static const struct cfparent pspec0 = {
	"usbifif", "uhub", DVUNIT_ANY
};

#define NORM FSTATE_NOTFOUND
#define STAR FSTATE_STAR

static struct cfdata cfdata_ioconf_umass[] = {
    /* driver           attachment    unit state      loc   flags  pspec */
/*  0: umass* at uhub? port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    { "umass",		"umass",	 0, STAR, loc+  0,      0, &pspec0 },
    { NULL,		NULL,		 0,    0,    NULL,      0, NULL }
};

static struct cfattach * const umass_cfattachinit[] = {
	&umass_ca, NULL
};

static const struct cfattachinit cfattach_ioconf_umass[] = {
	{ "umass", umass_cfattachinit },
	{ NULL, NULL }
};
