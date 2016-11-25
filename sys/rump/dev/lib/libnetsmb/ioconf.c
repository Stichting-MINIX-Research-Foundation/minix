#include "ioconf.h"
/*
 * MACHINE GENERATED: DO NOT EDIT
 *
 * ioconf.c, from "/home/boricj/Documents/Projets/minix/src/sys/rump/dev/lib/libnetsmb/NETSMB.ioconf"
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/mount.h>



static struct cfdriver * const cfdriver_ioconf_netsmb[] = {
	NULL
};



#define NORM FSTATE_NOTFOUND
#define STAR FSTATE_STAR

static struct cfdata cfdata_ioconf_netsmb[] = {
    /* driver           attachment    unit state      loc   flags  pspec */
    { NULL,		NULL,		 0,    0,    NULL,      0, NULL }
};


static const struct cfattachinit cfattach_ioconf_netsmb[] = {
	{ NULL, NULL }
};
