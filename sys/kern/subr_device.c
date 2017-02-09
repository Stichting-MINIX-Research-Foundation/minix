/*	$NetBSD: subr_device.c,v 1.3 2015/03/09 15:35:11 pooka Exp $	*/

/*
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: subr_device.c,v 1.3 2015/03/09 15:35:11 pooka Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

/* Root device. */
device_t			root_device;

/*
 * Accessor functions for the device_t type.
 */
devclass_t
device_class(device_t dev)
{

	return dev->dv_class;
}

cfdata_t
device_cfdata(device_t dev)
{

	return dev->dv_cfdata;
}

cfdriver_t
device_cfdriver(device_t dev)
{

	return dev->dv_cfdriver;
}

cfattach_t
device_cfattach(device_t dev)
{

	return dev->dv_cfattach;
}

int
device_unit(device_t dev)
{

	return dev->dv_unit;
}

const char *
device_xname(device_t dev)
{

	return dev->dv_xname;
}

device_t
device_parent(device_t dev)
{

	return dev->dv_parent;
}

bool
device_activation(device_t dev, devact_level_t level)
{
	int active_flags;

	active_flags = DVF_ACTIVE;
	switch (level) {
	case DEVACT_LEVEL_FULL:
		active_flags |= DVF_CLASS_SUSPENDED;
		/*FALLTHROUGH*/
	case DEVACT_LEVEL_DRIVER:
		active_flags |= DVF_DRIVER_SUSPENDED;
		/*FALLTHROUGH*/
	case DEVACT_LEVEL_BUS:
		active_flags |= DVF_BUS_SUSPENDED;
		break;
	}

	return (dev->dv_flags & active_flags) == DVF_ACTIVE;
}

bool
device_is_active(device_t dev)
{
	int active_flags;

	active_flags = DVF_ACTIVE;
	active_flags |= DVF_CLASS_SUSPENDED;
	active_flags |= DVF_DRIVER_SUSPENDED;
	active_flags |= DVF_BUS_SUSPENDED;

	return (dev->dv_flags & active_flags) == DVF_ACTIVE;
}

bool
device_is_enabled(device_t dev)
{
	return (dev->dv_flags & DVF_ACTIVE) == DVF_ACTIVE;
}

bool
device_has_power(device_t dev)
{
	int active_flags;

	active_flags = DVF_ACTIVE | DVF_BUS_SUSPENDED;

	return (dev->dv_flags & active_flags) == DVF_ACTIVE;
}

int
device_locator(device_t dev, u_int locnum)
{

	KASSERT(dev->dv_locators != NULL);
	return dev->dv_locators[locnum];
}

void *
device_private(device_t dev)
{

	/*
	 * The reason why device_private(NULL) is allowed is to simplify the
	 * work of a lot of userspace request handlers (i.e., c/bdev
	 * handlers) which grab cfdriver_t->cd_units[n].
	 * It avoids having them test for it to be NULL and only then calling
	 * device_private.
	 */
	return dev == NULL ? NULL : dev->dv_private;
}

prop_dictionary_t
device_properties(device_t dev)
{

	return dev->dv_properties;
}

/*
 * device_is_a:
 *
 *	Returns true if the device is an instance of the specified
 *	driver.
 */
bool
device_is_a(device_t dev, const char *dname)
{

	return strcmp(dev->dv_cfdriver->cd_name, dname) == 0;
}
