/*	$NetBSD: umass_quirks.c,v 1.96 2014/09/12 16:40:38 skrll Exp $	*/

/*
 * Copyright (c) 2001, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by MAEKAWA Masahide (gehenna@NetBSD.org).
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
__KERNEL_RCSID(0, "$NetBSD: umass_quirks.c,v 1.96 2014/09/12 16:40:38 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <dev/scsipi/scsipi_all.h> /* for scsiconf.h below */
#include <dev/scsipi/scsiconf.h> /* for quirks defines */

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/umassvar.h>
#include <dev/usb/umass_quirks.h>

Static usbd_status umass_init_insystem(struct umass_softc *);
Static usbd_status umass_init_shuttle(struct umass_softc *);

Static void umass_fixup_sony(struct umass_softc *);

/*
 * XXX
 * PLEASE NOTE that if you want quirk entries added to this table, you MUST
 * compile a kernel with USB_DEBUG, and submit a full log of the output from
 * whatever operation is "failing" with ?hcidebug=20 or higher and
 * umassdebug=0xffffff.  (It's usually helpful to also set MSGBUFSIZE to
 * something "large" unless you're using a serial console.)  Without this
 * information, the source of the problem cannot be properly analyzed, and
 * the quirk entry WILL NOT be accepted.
 * Also, when an entry is committed to this table, a concise but clear
 * description of the problem MUST accompany it.
 * - mycroft
 */
Static const struct umass_quirk umass_quirks[] = {
	/*
	 * The following 3 In-System Design adapters use a non-standard ATA
	 * over BBB protocol.  Force this protocol by quirk entries.
	 */
	{ { USB_VENDOR_INSYSTEM, USB_PRODUCT_INSYSTEM_ADAPTERV2 },
	  UMASS_WPROTO_BBB, UMASS_CPROTO_ISD_ATA,
	  0,
	  0,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},
	{ { USB_VENDOR_INSYSTEM, USB_PRODUCT_INSYSTEM_ATAPI },
	  UMASS_WPROTO_BBB, UMASS_CPROTO_ISD_ATA,
	  0,
	  0,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},
	{ { USB_VENDOR_INSYSTEM, USB_PRODUCT_INSYSTEM_DRIVEV2_5 },
	  UMASS_WPROTO_BBB, UMASS_CPROTO_ISD_ATA,
	  0,
	  0,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	{ { USB_VENDOR_INSYSTEM, USB_PRODUCT_INSYSTEM_USBCABLE },
	  UMASS_WPROTO_CBI, UMASS_CPROTO_ATAPI,
	  0,
	  0,
	  UMATCH_VENDOR_PRODUCT,
	  umass_init_insystem, NULL
	},

	{ { USB_VENDOR_SHUTTLE, USB_PRODUCT_SHUTTLE_EUSB },
	  UMASS_WPROTO_CBI_I, UMASS_CPROTO_ATAPI,
	  0,
	  0,
	  UMATCH_VENDOR_PRODUCT,
	  umass_init_shuttle, NULL
	},

	/*
	 * These work around genuine device bugs -- returning the wrong info in
	 * the CSW block.
	 */
	{ { USB_VENDOR_OLYMPUS, USB_PRODUCT_OLYMPUS_C1 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  UMASS_QUIRK_WRONG_CSWSIG,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},
	{ { USB_VENDOR_SCANLOGIC, USB_PRODUCT_SCANLOGIC_SL11R },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  UMASS_QUIRK_WRONG_CSWTAG,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},
	{ { USB_VENDOR_SHUTTLE, USB_PRODUCT_SHUTTLE_ORCA },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  UMASS_QUIRK_WRONG_CSWTAG,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},

	/*
	 * Some Sony cameras advertise a subclass code of 0xff, so we force it
	 * to the correct value iff necessary.
	 */
	{ { USB_VENDOR_SONY, USB_PRODUCT_SONY_DSC },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  UMASS_QUIRK_RBC_PAD_TO_12,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, umass_fixup_sony
	},

	/*
	 * Stupid device reports itself as SFF-8070, but actually returns a UFI
	 * interrupt descriptor.  - mycroft, 2004/06/28
	 */
	{ { USB_VENDOR_SONY, USB_PRODUCT_SONY_CLIE_40_MS },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UFI,
	  0,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},

	/*
	 * The SONY Portable GPS strage device almost hangs up when request
	 * UR_BBB_GET_MAX_LUN - disable the query logic.
	 */
	{ { USB_VENDOR_SONY, USB_PRODUCT_SONY_GPS_CS1 },
	  UMASS_WPROTO_BBB, UMASS_CPROTO_UNSPEC,
	  UMASS_QUIRK_NOGETMAXLUN,
	  0,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},

	/*
	 * The DiskOnKey does not reject commands it doesn't recognize in a
	 * sane way -- rather than STALLing the bulk pipe, it continually NAKs
	 * until we time out.  To prevent being screwed by this, for now we
	 * disable 10-byte MODE SENSE the klugy way.  - mycroft, 2003/10/16
	 */
	{ { USB_VENDOR_MSYSTEMS, USB_PRODUCT_MSYSTEMS_DISKONKEY },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NOBIGMODESENSE,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},
	{ { USB_VENDOR_MSYSTEMS, USB_PRODUCT_MSYSTEMS_DISKONKEY2 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NOBIGMODESENSE,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},
	{ { USB_VENDOR_MSYSTEMS, USB_PRODUCT_MSYSTEMS_DISKONKEY3 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NOBIGMODESENSE,
	  UMATCH_DEVCLASS_DEVSUBCLASS_DEVPROTO,
	  NULL, NULL
	},
	/* Some Sigmatel-based devices don't like all SCSI commands */
	{ { USB_VENDOR_SIGMATEL, USB_PRODUCT_SIGMATEL_MUSICSTICK },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NODOORLOCK | PQUIRK_NOSYNCCACHE,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},
	{ { USB_VENDOR_SIGMATEL, USB_PRODUCT_SIGMATEL_I_BEAD100 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0, 
	  PQUIRK_NODOORLOCK | PQUIRK_NOSYNCCACHE,
	  UMATCH_VENDOR_PRODUCT,  
	  NULL, NULL
	},
	{ { USB_VENDOR_SIGMATEL, USB_PRODUCT_SIGMATEL_I_BEAD150 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC, 
	  0,
	  PQUIRK_NODOORLOCK | PQUIRK_NOSYNCCACHE,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},
	{ { USB_VENDOR_PHILIPS, USB_PRODUCT_PHILIPS_SA235 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NODOORLOCK | PQUIRK_NOSYNCCACHE,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},
	/* Creative Nomad MuVo, NetBSD PR 30389, FreeBSD PR 53094 */
	{ { USB_VENDOR_CREATIVE, USB_PRODUCT_CREATIVE_NOMAD },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NODOORLOCK | PQUIRK_NOSYNCCACHE,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	/* iRiver iFP-[135]xx players fail on PREVENT/ALLOW, see PR 25440 */
	{ { USB_VENDOR_IRIVER, USB_PRODUCT_IRIVER_IFP_1XX },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NODOORLOCK,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},
	{ { USB_VENDOR_IRIVER, USB_PRODUCT_IRIVER_IFP_3XX },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NODOORLOCK,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},
	{ { USB_VENDOR_IRIVER, USB_PRODUCT_IRIVER_IFP_5XX },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NODOORLOCK,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	/* Meizu M6 doesn't like synchronize-cache, see PR 40442 */
	{ { USB_VENDOR_MEIZU, USB_PRODUCT_MEIZU_M6_SL },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC, 
	  0,
	  PQUIRK_NOSYNCCACHE,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	/*
	 * SanDisk Sansa Clip rejects cache sync in unconventional way.
	 * However, unlike some other devices listed in this table,
	 * this is does not cause the device firmware to stop responding.
	 */
	{ { USB_VENDOR_SANDISK, USB_PRODUCT_SANDISK_SANSA_CLIP },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC, 
	  0,
	  PQUIRK_NOSYNCCACHE,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	/* Kingston USB pendrives don't like being told to lock the door */
	{ { USB_VENDOR_KINGSTON, USB_PRODUCT_KINGSTON_DT101_II },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC, 
	  0,
	  PQUIRK_NODOORLOCK,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	{ { USB_VENDOR_KINGSTON, USB_PRODUCT_KINGSTON_DT101_G2 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC, 
	  0,
	  PQUIRK_NODOORLOCK,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	{ { USB_VENDOR_KINGSTON, USB_PRODUCT_KINGSTON_DT102_G2 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC, 
	  0,
	  PQUIRK_NODOORLOCK,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	{ { USB_VENDOR_KINGSTON, USB_PRODUCT_KINGSTON_DTMINI10 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC, 
	  0,
	  PQUIRK_NODOORLOCK,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	/* Also, some Kingston pendrives have Toshiba vendor ID */
	{ { USB_VENDOR_TOSHIBA, USB_PRODUCT_KINGSTON_DT100_G2 },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC, 
	  0,
	  PQUIRK_NODOORLOCK,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	/* HP USB pendrives don't like being told to lock the door */
	{ { USB_VENDOR_HP, USB_PRODUCT_HP_V125W },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  PQUIRK_NODOORLOCK,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},

	{ { USB_VENDOR_IODATA2, USB_PRODUCT_IODATA2_USB2SC },
	  UMASS_WPROTO_UNSPEC, UMASS_CPROTO_UNSPEC,
	  0,
	  0,
	  UMATCH_VENDOR_PRODUCT,
	  NULL, NULL
	},
};

const struct umass_quirk *
umass_lookup(u_int16_t vendor, u_int16_t product)
{
	return ((const struct umass_quirk *)
		usb_lookup(umass_quirks, vendor, product));
}

Static usbd_status
umass_init_insystem(struct umass_softc *sc)
{
	usbd_status err;

	err = usbd_set_interface(sc->sc_iface, 1);
	if (err) {
		DPRINTF(UDMASS_USB,
			("%s: could not switch to Alt Interface 1\n",
			device_xname(sc->sc_dev)));
		return (err);
	}

	return (USBD_NORMAL_COMPLETION);
}

Static usbd_status
umass_init_shuttle(struct umass_softc *sc)
{
	usb_device_request_t req;
	u_int8_t status[2];

	/* The Linux driver does this */
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = 1;
	USETW(req.wValue, 0);
	USETW(req.wIndex, sc->sc_ifaceno);
	USETW(req.wLength, sizeof(status));

	return (usbd_do_request(sc->sc_udev, &req, &status));
}

Static void
umass_fixup_sony(struct umass_softc *sc)
{
	usb_interface_descriptor_t *id;

	id = usbd_get_interface_descriptor(sc->sc_iface);
	if (id->bInterfaceSubClass == 0xff)
		sc->sc_cmd = UMASS_CPROTO_RBC;
}
