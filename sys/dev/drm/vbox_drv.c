/* $NetBSD: vbox_drv.c,v 1.2 2011/08/28 17:18:31 jmcneill Exp $ */

/*
 * Copyright (c) 2011 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vbox_drv.c,v 1.2 2011/08/28 17:18:31 jmcneill Exp $");

#include "drmP.h"
#include "drm.h"

static drm_pci_id_list_t vboxdrm_pciidlist[] = {
	{ 0x80ee, 0xbeef, 0, "VirtualBox Video" },
	{ 0, 0, 0, NULL },
};

static int
vboxdrm_driver_load(struct drm_device *dev, unsigned long flags)
{
	return drm_vblank_init(dev, 1);
}

static void
vboxdrm_configure(struct drm_device *dev)
{
	dev->driver->buf_priv_size = 1;
	dev->driver->load = vboxdrm_driver_load;
	dev->driver->name = "vbox";
	dev->driver->desc = "VirtualBox Video";
	dev->driver->date = "20110130";
	dev->driver->major = 1;
	dev->driver->minor = 0;
	dev->driver->patchlevel = 0;
}

static int
vboxdrm_match(device_t parent, cfdata_t match, void *opaque)
{
	struct pci_attach_args *pa = opaque;

	return drm_probe(pa, vboxdrm_pciidlist);
}

static void
vboxdrm_attach(device_t parent, device_t self, void *opaque)
{
	struct pci_attach_args *pa = opaque;
	struct drm_device *dev = device_private(self);

	pmf_device_register(self, NULL, NULL);

	dev->driver = kmem_zalloc(sizeof(struct drm_driver_info), KM_SLEEP);
	if (dev->driver == NULL) {
		aprint_error_dev(self, "couldn't allocate memory\n");
		return;
	}

	vboxdrm_configure(dev);
	drm_attach(self, pa, vboxdrm_pciidlist);
}

static int
vboxdrm_detach(device_t self, int flags)
{
	struct drm_device *dev = device_private(self);
	int error;

	pmf_device_deregister(self);

	error = drm_detach(self, flags);
	kmem_free(dev->driver, sizeof(struct drm_driver_info));

	return error;
}

CFATTACH_DECL_NEW(
    vboxdrm,
    sizeof(struct drm_device),
    vboxdrm_match,
    vboxdrm_attach,
    vboxdrm_detach,
    NULL
);

MODULE(MODULE_CLASS_DRIVER, vboxdrm, "drm");

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
vboxdrm_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
#ifdef _MODULE
	case MODULE_CMD_INIT:
		return config_init_component(cfdriver_ioconf_vboxdrm,
		    cfattach_ioconf_vboxdrm, cfdata_ioconf_vboxdrm);
	case MODULE_CMD_FINI:
		return config_fini_component(cfdriver_ioconf_vboxdrm,
		    cfattach_ioconf_vboxdrm, cfdata_ioconf_vboxdrm);
#else
	case MODULE_CMD_INIT:
	case MODULE_CMD_FINI:
		return 0;
#endif
	default:
		return ENOTTY;
	}
}
