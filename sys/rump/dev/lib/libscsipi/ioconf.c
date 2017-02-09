#include "ioconf.h"
/*
 * MACHINE GENERATED: DO NOT EDIT
 *
 * ioconf.c, from "/home/boricj/Documents/Projets/minix/src/sys/rump/dev/lib/libscsipi/SCSIPI.ioconf"
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
static const struct cfiattrdata scsibuscf_iattrdata = {
	"scsibus", 2, {
		{ "target", "-1", -1 },
		{ "lun", "-1", -1 },
	}
};
static const struct cfiattrdata atapibuscf_iattrdata = {
	"atapibus", 1, {
		{ "drive", "-1", -1 },
	}
};

static const struct cfiattrdata * const scsibus_attrs[] = { &scsibuscf_iattrdata, NULL };
CFDRIVER_DECL(scsibus, DV_DULL, scsibus_attrs);

static const struct cfiattrdata * const atapibus_attrs[] = { &atapibuscf_iattrdata, NULL };
CFDRIVER_DECL(atapibus, DV_DULL, atapibus_attrs);

CFDRIVER_DECL(cd, DV_DISK, NULL);

CFDRIVER_DECL(sd, DV_DISK, NULL);


static struct cfdriver * const cfdriver_ioconf_scsipi[] = {
	&scsibus_cd,
	&atapibus_cd,
	&cd_cd,
	&sd_cd,
	NULL
};

extern struct cfattach scsibus_ca;
extern struct cfattach atapibus_ca;
extern struct cfattach cd_ca;
extern struct cfattach sd_ca;

/* locators */
static int loc[7] = {
	-1, -1, -1, -1, -1, -1, -1,
};

static const struct cfparent pspec0 = {
	"scsi", NULL, 0
};
static const struct cfparent pspec1 = {
	"scsibus", "scsibus", DVUNIT_ANY
};
static const struct cfparent pspec2 = {
	"atapi", NULL, 0
};
static const struct cfparent pspec3 = {
	"atapibus", "atapibus", DVUNIT_ANY
};

#define NORM FSTATE_NOTFOUND
#define STAR FSTATE_STAR

static struct cfdata cfdata_ioconf_scsipi[] = {
    /* driver           attachment    unit state      loc   flags  pspec */
/*  0: scsibus* at scsi? channel -1 */
    { "scsibus",	"scsibus",	 0, STAR, loc+  4,      0, &pspec0 },
/*  1: atapibus* at atapi? */
    { "atapibus",	"atapibus",	 0, STAR,    NULL,      0, &pspec2 },
/*  2: cd* at scsibus? target -1 lun -1 */
    { "cd",		"cd",		 0, STAR, loc+  0,      0, &pspec1 },
/*  3: cd* at atapibus? drive -1 */
    { "cd",		"cd",		 0, STAR, loc+  5,      0, &pspec3 },
/*  4: sd* at scsibus? target -1 lun -1 */
    { "sd",		"sd",		 0, STAR, loc+  2,      0, &pspec1 },
/*  5: sd* at atapibus? drive -1 */
    { "sd",		"sd",		 0, STAR, loc+  6,      0, &pspec3 },
    { NULL,		NULL,		 0,    0,    NULL,      0, NULL }
};

static struct cfattach * const scsibus_cfattachinit[] = {
	&scsibus_ca, NULL
};
static struct cfattach * const atapibus_cfattachinit[] = {
	&atapibus_ca, NULL
};
static struct cfattach * const cd_cfattachinit[] = {
	&cd_ca, NULL
};
static struct cfattach * const sd_cfattachinit[] = {
	&sd_ca, NULL
};

static const struct cfattachinit cfattach_ioconf_scsipi[] = {
	{ "scsibus", scsibus_cfattachinit },
	{ "atapibus", atapibus_cfattachinit },
	{ "cd", cd_cfattachinit },
	{ "sd", sd_cfattachinit },
	{ NULL, NULL }
};
