#include "ioconf.h"
/*
 * MACHINE GENERATED: DO NOT EDIT
 *
 * ioconf.c, from "/home/boricj/Documents/Projets/minix/src/sys/rump/dev/lib/libucom/UCOM.ioconf"
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
static const struct cfiattrdata ucombuscf_iattrdata = {
	"ucombus", 1, {
		{ "portno", "-1", -1 },
	}
};

CFDRIVER_DECL(ucom, DV_DULL, NULL);

static const struct cfiattrdata * const u3g_attrs[] = { &ucombuscf_iattrdata, NULL };
CFDRIVER_DECL(u3g, DV_DULL, u3g_attrs);

CFDRIVER_DECL(u3ginit, DV_DULL, NULL);

static const struct cfiattrdata * const uplcom_attrs[] = { &ucombuscf_iattrdata, NULL };
CFDRIVER_DECL(uplcom, DV_DULL, uplcom_attrs);


static struct cfdriver * const cfdriver_ioconf_ucom[] = {
	&ucom_cd,
	&u3g_cd,
	&u3ginit_cd,
	&uplcom_cd,
	NULL
};

extern struct cfattach ucom_ca;
extern struct cfattach u3g_ca;
extern struct cfattach u3ginit_ca;
extern struct cfattach uplcom_ca;

/* locators */
static int loc[20] = {
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1,
};

static const struct cfparent pspec0 = {
	"usbdevif", "uhub", DVUNIT_ANY
};
static const struct cfparent pspec1 = {
	"ucombus", "uplcom", DVUNIT_ANY
};
static const struct cfparent pspec2 = {
	"usbifif", "uhub", DVUNIT_ANY
};
static const struct cfparent pspec3 = {
	"ucombus", "u3g", DVUNIT_ANY
};

#define NORM FSTATE_NOTFOUND
#define STAR FSTATE_STAR

static struct cfdata cfdata_ioconf_ucom[] = {
    /* driver           attachment    unit state      loc   flags  pspec */
/*  0: ucom* at uplcom? portno -1 */
    { "ucom",		"ucom",		 0, STAR, loc+ 18,      0, &pspec1 },
/*  1: ucom* at u3g? portno -1 */
    { "ucom",		"ucom",		 0, STAR, loc+ 19,      0, &pspec3 },
/*  2: u3g* at uhub? port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    { "u3g",		"u3g",		 0, STAR, loc+  0,      0, &pspec2 },
/*  3: u3ginit* at uhub? port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    { "u3ginit",	"u3ginit",	 0, STAR, loc+  6,      0, &pspec0 },
/*  4: uplcom* at uhub? port -1 configuration -1 interface -1 vendor -1 product -1 release -1 */
    { "uplcom",		"uplcom",	 0, STAR, loc+ 12,      0, &pspec0 },
    { NULL,		NULL,		 0,    0,    NULL,      0, NULL }
};

static struct cfattach * const ucom_cfattachinit[] = {
	&ucom_ca, NULL
};
static struct cfattach * const u3g_cfattachinit[] = {
	&u3g_ca, NULL
};
static struct cfattach * const u3ginit_cfattachinit[] = {
	&u3ginit_ca, NULL
};
static struct cfattach * const uplcom_cfattachinit[] = {
	&uplcom_ca, NULL
};

static const struct cfattachinit cfattach_ioconf_ucom[] = {
	{ "ucom", ucom_cfattachinit },
	{ "u3g", u3g_cfattachinit },
	{ "u3ginit", u3ginit_cfattachinit },
	{ "uplcom", uplcom_cfattachinit },
	{ NULL, NULL }
};
