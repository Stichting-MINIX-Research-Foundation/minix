/*	$NetBSD: ugenhc_at_mainbus.c,v 1.3 2010/03/26 15:51:55 pooka Exp $	*/

#include <sys/param.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/stat.h>

#include "ioconf.c"

#include "rump_private.h"
#include "rump_dev_private.h"

void tty_init(void);

RUMP_COMPONENT(RUMP_COMPONENT_DEV)
{

	config_init_component(cfdriver_ioconf_ugenhc,
	    cfattach_ioconf_ugenhc, cfdata_ioconf_ugenhc);
}
