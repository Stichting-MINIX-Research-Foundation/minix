#include "ioconf.h"
/*
 * MACHINE GENERATED: DO NOT EDIT
 *
 * ioconf.c, from "/home/boricj/Documents/Projets/minix/src/sys/rump/dev/lib/libugenhc/UGENHC.ioconf"
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/mount.h>

static const struct cfiattrdata mainbuscf_iattrdata = {
	"mainbus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata usbuscf_iattrdata = {
	"usbus", 0, {
		{ NULL, NULL, 0 },
	}
};
static const struct cfiattrdata pcibuscf_iattrdata = {
	"pcibus", 1, {
		{ "bus", "-1", -1 },
	}
};

static const struct cfiattrdata * const ugenhc_attrs[] = { &usbuscf_iattrdata, NULL };
CFDRIVER_DECL(ugenhc, DV_DULL, ugenhc_attrs);


static struct cfdriver * const cfdriver_ioconf_ugenhc[] = {
	&ugenhc_cd,
	NULL
};

extern struct cfattach ugenhc_ca;

static const struct cfparent pspec0 = {
	"mainbus", "mainbus", DVUNIT_ANY
};

#define NORM FSTATE_NOTFOUND
#define STAR FSTATE_STAR

static struct cfdata cfdata_ioconf_ugenhc[] = {
    /* driver           attachment    unit state      loc   flags  pspec */
/*  0: ugenhc0 at mainbus? */
    { "ugenhc",		"ugenhc",	 0, NORM,    NULL,      0, &pspec0 },
/*  1: ugenhc1 at mainbus? */
    { "ugenhc",		"ugenhc",	 1, NORM,    NULL,      0, &pspec0 },
/*  2: ugenhc2 at mainbus? */
    { "ugenhc",		"ugenhc",	 2, NORM,    NULL,      0, &pspec0 },
/*  3: ugenhc3 at mainbus? */
    { "ugenhc",		"ugenhc",	 3, NORM,    NULL,      0, &pspec0 },
    { NULL,		NULL,		 0,    0,    NULL,      0, NULL }
};

static struct cfattach * const ugenhc_cfattachinit[] = {
	&ugenhc_ca, NULL
};

static const struct cfattachinit cfattach_ioconf_ugenhc[] = {
	{ "ugenhc", ugenhc_cfattachinit },
	{ NULL, NULL }
};
