#include "ioconf.h"
/*
 * MACHINE GENERATED: DO NOT EDIT
 *
 * ioconf.c, from "/home/boricj/Documents/Projets/minix/src/sys/rump/dev/lib/libwscons/WSCONS.ioconf"
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/mount.h>

static const struct cfiattrdata wskbddevcf_iattrdata = {
	"wskbddev", 2, {
		{ "console", "-1", -1 },
		{ "mux", "1", 1 },
	}
};
static const struct cfiattrdata wsmousedevcf_iattrdata = {
	"wsmousedev", 1, {
		{ "mux", "0", 0 },
	}
};

CFDRIVER_DECL(wskbd, DV_DULL, NULL);

CFDRIVER_DECL(wsmouse, DV_DULL, NULL);


static struct cfdriver * const cfdriver_ioconf_wscons[] = {
	&wskbd_cd,
	&wsmouse_cd,
	NULL
};

extern struct cfattach wskbd_ca;
extern struct cfattach wsmouse_ca;

/* locators */
static int loc[3] = {
	-1, 1, 0,
};

static const struct cfparent pspec0 = {
	"wsmousedev", "ums", DVUNIT_ANY
};
static const struct cfparent pspec1 = {
	"wskbddev", "ukbd", DVUNIT_ANY
};

#define NORM FSTATE_NOTFOUND
#define STAR FSTATE_STAR

static struct cfdata cfdata_ioconf_wscons[] = {
    /* driver           attachment    unit state      loc   flags  pspec */
/*  0: wskbd* at ukbd? console -1 mux 1 */
    { "wskbd",		"wskbd",	 0, STAR, loc+  0,      0, &pspec1 },
/*  1: wsmouse* at ums? mux 0 */
    { "wsmouse",	"wsmouse",	 0, STAR, loc+  2,      0, &pspec0 },
    { NULL,		NULL,		 0,    0,    NULL,      0, NULL }
};

static struct cfattach * const wskbd_cfattachinit[] = {
	&wskbd_ca, NULL
};
static struct cfattach * const wsmouse_cfattachinit[] = {
	&wsmouse_ca, NULL
};

static const struct cfattachinit cfattach_ioconf_wscons[] = {
	{ "wskbd", wskbd_cfattachinit },
	{ "wsmouse", wsmouse_cfattachinit },
	{ NULL, NULL }
};
