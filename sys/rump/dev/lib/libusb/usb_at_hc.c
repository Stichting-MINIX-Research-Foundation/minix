/*	$NetBSD: usb_at_hc.c,v 1.1 2015/05/20 11:53:08 pooka Exp $	*/

#include <sys/param.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/stat.h>

#include "ioconf.c"

#include "rump_private.h"
#include "rump_dev_private.h"

RUMP_COMPONENT(RUMP_COMPONENT_DEV)
{

	config_init_component(cfdriver_ioconf_usb,
	    cfattach_ioconf_usb, cfdata_ioconf_usb);
}
