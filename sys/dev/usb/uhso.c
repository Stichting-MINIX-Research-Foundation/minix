/*	$NetBSD: uhso.c,v 1.17 2014/11/15 19:18:19 christos Exp $	*/

/*-
 * Copyright (c) 2009 Iain Hibbert
 * Copyright (c) 2008 Fredrik Lindberg
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *   This driver originated as the hso module for FreeBSD written by
 * Fredrik Lindberg[1]. It has been rewritten almost completely for
 * NetBSD, and to support more devices with information extracted from
 * the Linux hso driver provided by Option N.V.[2]
 *
 *   [1] http://www.shapeshifter.se/code/hso
 *   [2] http://www.pharscape.org/hso.htm
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: uhso.c,v 1.17 2014/11/15 19:18:19 christos Exp $");

#ifdef _KERNEL_OPT
#include "opt_inet.h"
#endif

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kauth.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/mbuf.h>
#include <sys/poll.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/vnode.h>
#include <sys/lwp.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/netisr.h>

#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbcdc.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/umassvar.h>

#include <dev/scsipi/scsi_disk.h>

#include "usbdevs.h"

#undef DPRINTF
#ifdef UHSO_DEBUG
/*
 * defined levels
 *	0	warnings only
 *	1	informational
 *	5	really chatty
 */
int uhso_debug = 0;

#define DPRINTF(n, ...)	do {			\
	if (uhso_debug >= (n)) {		\
		printf("%s: ", __func__);	\
		printf(__VA_ARGS__);		\
	}					\
} while (/* CONSTCOND */0)
#else
#define DPRINTF(...)	((void)0)
#endif

/*
 * When first attached, the device class will be 0 and the modem
 * will attach as UMASS until a SCSI REZERO_UNIT command is sent,
 * in which case it will detach and reattach with device class set
 * to UDCLASS_VENDOR (0xff) and provide the serial interfaces.
 *
 * If autoswitch is set (the default) this will happen automatically.
 */
Static int uhso_autoswitch = 1;

SYSCTL_SETUP(sysctl_hw_uhso_setup, "uhso sysctl setup")
{
	const struct sysctlnode *node = NULL;

	sysctl_createv(clog, 0, NULL, &node,
		CTLFLAG_PERMANENT,
		CTLTYPE_NODE, "uhso",
		NULL,
		NULL, 0,
		NULL, 0,
		CTL_HW, CTL_CREATE, CTL_EOL);

	if (node == NULL)
		return;

#ifdef UHSO_DEBUG
	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "debug",
		SYSCTL_DESCR("uhso debug level (0, 1, 5)"),
		NULL, 0,
		&uhso_debug, sizeof(uhso_debug),
		CTL_CREATE, CTL_EOL);
#endif

	sysctl_createv(clog, 0, &node, NULL,
		CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		CTLTYPE_INT, "autoswitch",
		SYSCTL_DESCR("automatically switch device into modem mode"),
		NULL, 0,
		&uhso_autoswitch, sizeof(uhso_autoswitch),
		CTL_CREATE, CTL_EOL);
}

/*
 * The uhso modems have a number of interfaces providing a variety of
 * IO ports using the bulk endpoints, or multiplexed on the control
 * endpoints. We separate the ports by function and provide each with
 * a predictable index number used to construct the device minor number.
 *
 * The Network port is configured as a network interface rather than
 * a tty as it provides raw IPv4 packets.
 */

Static const char *uhso_port_name[] = {
	"Control",
	"Diagnostic",
	"Diagnostic2",
	"Application",
	"Application2",
	"GPS",
	"GPS Control",
	"PC Smartcard",
	"Modem",
	"MSD",			/* "Modem Sharing Device" ? */
	"Voice",
	"Network",
};

#define UHSO_PORT_CONTROL	0x00
#define UHSO_PORT_DIAG		0x01
#define UHSO_PORT_DIAG2		0x02
#define UHSO_PORT_APP		0x03
#define UHSO_PORT_APP2		0x04
#define UHSO_PORT_GPS		0x05
#define UHSO_PORT_GPS_CONTROL	0x06
#define UHSO_PORT_PCSC		0x07
#define UHSO_PORT_MODEM		0x08
#define UHSO_PORT_MSD		0x09
#define UHSO_PORT_VOICE		0x0a
#define UHSO_PORT_NETWORK	0x0b

#define UHSO_PORT_MAX		__arraycount(uhso_port_name)

#define UHSO_IFACE_MUX		0x20
#define UHSO_IFACE_BULK		0x40
#define UHSO_IFACE_IFNET	0x80

/*
 * The interface specification can sometimes be deduced from the device
 * type and interface number, or some modems support a vendor specific
 * way to read config info which we can translate to the port index.
 */
Static const uint8_t uhso_spec_default[] = {
	UHSO_IFACE_IFNET | UHSO_PORT_NETWORK | UHSO_IFACE_MUX,
	UHSO_IFACE_BULK | UHSO_PORT_DIAG,
	UHSO_IFACE_BULK | UHSO_PORT_MODEM,
};

Static const uint8_t uhso_spec_icon321[] = {
	UHSO_IFACE_IFNET | UHSO_PORT_NETWORK | UHSO_IFACE_MUX,
	UHSO_IFACE_BULK | UHSO_PORT_DIAG2,
	UHSO_IFACE_BULK | UHSO_PORT_MODEM,
	UHSO_IFACE_BULK | UHSO_PORT_DIAG,
};

Static const uint8_t uhso_spec_config[] = {
	0,
	UHSO_IFACE_BULK | UHSO_PORT_DIAG,
	UHSO_IFACE_BULK | UHSO_PORT_GPS,
	UHSO_IFACE_BULK | UHSO_PORT_GPS_CONTROL,
	UHSO_IFACE_BULK | UHSO_PORT_APP,
	UHSO_IFACE_BULK | UHSO_PORT_APP2,
	UHSO_IFACE_BULK | UHSO_PORT_CONTROL,
	UHSO_IFACE_IFNET | UHSO_PORT_NETWORK,
	UHSO_IFACE_BULK | UHSO_PORT_MODEM,
	UHSO_IFACE_BULK | UHSO_PORT_MSD,
	UHSO_IFACE_BULK | UHSO_PORT_PCSC,
	UHSO_IFACE_BULK | UHSO_PORT_VOICE,
};

struct uhso_dev {
	uint16_t vendor;
	uint16_t product;
	uint16_t type;
};

#define UHSOTYPE_DEFAULT	1
#define UHSOTYPE_ICON321	2
#define UHSOTYPE_CONFIG		3

Static const struct uhso_dev uhso_devs[] = {
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_MAXHSDPA,    UHSOTYPE_DEFAULT },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_GSICON72,    UHSOTYPE_DEFAULT },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_ICON225,     UHSOTYPE_DEFAULT },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_GEHSUPA,     UHSOTYPE_DEFAULT },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_GTHSUPA,     UHSOTYPE_DEFAULT },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_GSHSUPA,     UHSOTYPE_DEFAULT },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_GE40X1,      UHSOTYPE_CONFIG },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_GE40X2,      UHSOTYPE_CONFIG },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_GE40X3,      UHSOTYPE_CONFIG },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_ICON401,     UHSOTYPE_CONFIG },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_GTM382,	     UHSOTYPE_CONFIG },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_GE40X4,      UHSOTYPE_CONFIG },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_GTHSUPAM,    UHSOTYPE_CONFIG },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_ICONEDGE,    UHSOTYPE_DEFAULT },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_MODHSXPA,    UHSOTYPE_ICON321 },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_ICON321,     UHSOTYPE_ICON321 },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_ICON322,     UHSOTYPE_ICON321 },
    { USB_VENDOR_OPTIONNV, USB_PRODUCT_OPTIONNV_ICON505,     UHSOTYPE_CONFIG },
};

#define uhso_lookup(p, v)  ((const struct uhso_dev *)usb_lookup(uhso_devs, (p), (v)))

/* IO buffer sizes */
#define UHSO_MUX_WSIZE		64
#define UHSO_MUX_RSIZE		1024
#define UHSO_BULK_WSIZE		8192
#define UHSO_BULK_RSIZE		4096
#define UHSO_IFNET_MTU		1500

/*
 * Each IO port provided by the modem can be mapped to a network
 * interface (when hp_ifp != NULL) or a tty (when hp_tp != NULL)
 * which may be multiplexed and sharing interrupt and control endpoints
 * from an interface, or using the dedicated bulk endpoints.
 */

struct uhso_port;
struct uhso_softc;

/* uhso callback functions return errno on failure */
typedef int (*uhso_callback)(struct uhso_port *);

struct uhso_port {
	struct uhso_softc      *hp_sc;		/* master softc */
	struct tty	       *hp_tp;		/* tty pointer */
	struct ifnet	       *hp_ifp;		/* ifnet pointer */
	unsigned int		hp_flags;	/* see below */
	int			hp_swflags;	/* persistent tty flags */
	int			hp_status;	/* modem status */

	/* port type specific handlers */
	uhso_callback		hp_abort;	/* abort any transfers */
	uhso_callback		hp_detach;	/* detach port completely */
	uhso_callback		hp_init;	/* init port (first open) */
	uhso_callback		hp_clean;	/* clean port (last close) */
	uhso_callback		hp_write;	/* write data */
	usbd_callback		hp_write_cb;	/* write callback */
	uhso_callback		hp_read;	/* read data */
	usbd_callback		hp_read_cb;	/* read callback */
	uhso_callback		hp_control;	/* set control lines */

	usbd_interface_handle	hp_ifh;		/* interface handle */
	unsigned int		hp_index;	/* usb request index */

	int			hp_iaddr;	/* interrupt endpoint */
	usbd_pipe_handle	hp_ipipe;	/* interrupt pipe */
	void		       *hp_ibuf;	/* interrupt buffer */
	size_t			hp_isize;	/* allocated size */

	int			hp_raddr;	/* bulk in endpoint */
	usbd_pipe_handle	hp_rpipe;	/* bulk in pipe */
	usbd_xfer_handle	hp_rxfer;	/* input xfer */
	void		       *hp_rbuf;	/* input buffer */
	size_t			hp_rlen;	/* fill length */
	size_t			hp_rsize;	/* allocated size */

	int			hp_waddr;	/* bulk out endpoint */
	usbd_pipe_handle	hp_wpipe;	/* bulk out pipe */
	usbd_xfer_handle	hp_wxfer;	/* output xfer */
	void		       *hp_wbuf;	/* output buffer */
	size_t			hp_wlen;	/* fill length */
	size_t			hp_wsize;	/* allocated size */

	struct mbuf	       *hp_mbuf;	/* partial packet */
};

/* hp_flags */
#define UHSO_PORT_MUXPIPE	__BIT(0)	/* duplicate ipipe/ibuf references */
#define UHSO_PORT_MUXREADY	__BIT(1)	/* input is ready */
#define UHSO_PORT_MUXBUSY	__BIT(2)	/* read in progress */

struct uhso_softc {
	device_t		sc_dev;		/* self */
	usbd_device_handle	sc_udev;
	int			sc_refcnt;
	struct uhso_port       *sc_port[UHSO_PORT_MAX];
};

#define UHSO_CONFIG_NO		1

int uhso_match(device_t, cfdata_t, void *);
void uhso_attach(device_t, device_t, void *);
int uhso_detach(device_t, int);

extern struct cfdriver uhso_cd;

CFATTACH_DECL_NEW(uhso, sizeof(struct uhso_softc), uhso_match, uhso_attach,
    uhso_detach, NULL);

Static int uhso_switch_mode(usbd_device_handle);
Static int uhso_get_iface_spec(struct usb_attach_arg *, uint8_t, uint8_t *);
Static usb_endpoint_descriptor_t *uhso_get_endpoint(usbd_interface_handle, int, int);

Static void uhso_mux_attach(struct uhso_softc *, usbd_interface_handle, int);
Static int  uhso_mux_abort(struct uhso_port *);
Static int  uhso_mux_detach(struct uhso_port *);
Static int  uhso_mux_init(struct uhso_port *);
Static int  uhso_mux_clean(struct uhso_port *);
Static int  uhso_mux_write(struct uhso_port *);
Static int  uhso_mux_read(struct uhso_port *);
Static int  uhso_mux_control(struct uhso_port *);
Static void uhso_mux_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);

Static void uhso_bulk_attach(struct uhso_softc *, usbd_interface_handle, int);
Static int  uhso_bulk_abort(struct uhso_port *);
Static int  uhso_bulk_detach(struct uhso_port *);
Static int  uhso_bulk_init(struct uhso_port *);
Static int  uhso_bulk_clean(struct uhso_port *);
Static int  uhso_bulk_write(struct uhso_port *);
Static int  uhso_bulk_read(struct uhso_port *);
Static int  uhso_bulk_control(struct uhso_port *);
Static void uhso_bulk_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);

Static void uhso_tty_attach(struct uhso_port *);
Static void uhso_tty_detach(struct uhso_port *);
Static void uhso_tty_read_cb(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void uhso_tty_write_cb(usbd_xfer_handle, usbd_private_handle, usbd_status);

dev_type_open(uhso_tty_open);
dev_type_close(uhso_tty_close);
dev_type_read(uhso_tty_read);
dev_type_write(uhso_tty_write);
dev_type_ioctl(uhso_tty_ioctl);
dev_type_stop(uhso_tty_stop);
dev_type_tty(uhso_tty_tty);
dev_type_poll(uhso_tty_poll);

const struct cdevsw uhso_cdevsw = {
	.d_open = uhso_tty_open,
	.d_close = uhso_tty_close,
	.d_read = uhso_tty_read,
	.d_write = uhso_tty_write,
	.d_ioctl = uhso_tty_ioctl,
	.d_stop = uhso_tty_stop,
	.d_tty = uhso_tty_tty,
	.d_poll = uhso_tty_poll,
	.d_mmap = nommap,
	.d_kqfilter = ttykqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};

Static int  uhso_tty_init(struct uhso_port *);
Static void uhso_tty_clean(struct uhso_port *);
Static int  uhso_tty_do_ioctl(struct uhso_port *, u_long, void *, int, struct lwp *);
Static void uhso_tty_start(struct tty *);
Static int  uhso_tty_param(struct tty *, struct termios *);
Static int  uhso_tty_control(struct uhso_port *, u_long, int);

#define UHSO_UNIT_MASK		TTUNIT_MASK
#define UHSO_PORT_MASK		0x0000f
#define UHSO_DIALOUT_MASK	TTDIALOUT_MASK
#define UHSO_CALLUNIT_MASK	TTCALLUNIT_MASK

#define UHSOUNIT(x)	(TTUNIT(x) >> 4)
#define UHSOPORT(x)	(TTUNIT(x) & UHSO_PORT_MASK)
#define UHSODIALOUT(x)	TTDIALOUT(x)
#define UHSOMINOR(u, p)	((((u) << 4) & UHSO_UNIT_MASK) | ((p) & UHSO_UNIT_MASK))

Static void uhso_ifnet_attach(struct uhso_softc *, usbd_interface_handle, int);
Static int  uhso_ifnet_abort(struct uhso_port *);
Static int  uhso_ifnet_detach(struct uhso_port *);
Static void uhso_ifnet_read_cb(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void uhso_ifnet_input(struct ifnet *, struct mbuf **, uint8_t *, size_t);
Static void uhso_ifnet_write_cb(usbd_xfer_handle, usbd_private_handle, usbd_status);

Static int  uhso_ifnet_ioctl(struct ifnet *, u_long, void *);
Static int  uhso_ifnet_init(struct uhso_port *);
Static void uhso_ifnet_clean(struct uhso_port *);
Static void uhso_ifnet_start(struct ifnet *);
Static int  uhso_ifnet_output(struct ifnet *, struct mbuf *, const struct sockaddr *, struct rtentry *);


/*******************************************************************************
 *
 *	USB autoconfig
 *
 */

int
uhso_match(device_t parent, cfdata_t match, void *aux)
{
	struct usb_attach_arg *uaa = aux;

	/*
	 * don't claim this device if autoswitch is disabled
	 * and it is not in modem mode already
	 */
	if (!uhso_autoswitch && uaa->class != UDCLASS_VENDOR)
		return UMATCH_NONE;

	if (uhso_lookup(uaa->vendor, uaa->product))
		return UMATCH_VENDOR_PRODUCT;

	return UMATCH_NONE;
}

void
uhso_attach(device_t parent, device_t self, void *aux)
{
	struct uhso_softc *sc = device_private(self);
	struct usb_attach_arg *uaa = aux;
	usbd_interface_handle ifh;
	char *devinfop;
	uint8_t count, i, spec;
	usbd_status status;

	DPRINTF(1, ": sc = %p, self=%p", sc, self);

	sc->sc_dev = self;
	sc->sc_udev = uaa->device;

	aprint_naive("\n");
	aprint_normal("\n");

	devinfop = usbd_devinfo_alloc(uaa->device, 0);
	aprint_normal_dev(self, "%s\n", devinfop);
	usbd_devinfo_free(devinfop);

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, sc->sc_dev);

	status = usbd_set_config_no(sc->sc_udev, UHSO_CONFIG_NO, 1);
	if (status != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(self, "failed to set configuration"
		    ", err=%s\n", usbd_errstr(status));
		return;
	}

	if (uaa->class != UDCLASS_VENDOR) {
		aprint_verbose_dev(self, "Switching device into modem mode..\n");
		if (uhso_switch_mode(uaa->device) != 0)
			aprint_error_dev(self, "modem switch failed\n");

		return;
	}

	count = 0;
	(void)usbd_interface_count(sc->sc_udev, &count);
	DPRINTF(1, "interface count %d\n", count);

	for (i = 0; i < count; i++) {
		status = usbd_device2interface_handle(sc->sc_udev, i, &ifh);
		if (status != USBD_NORMAL_COMPLETION) {
			aprint_error_dev(self,
			    "could not get interface %d: %s\n",
			    i, usbd_errstr(status));

			return;
		}

		if (!uhso_get_iface_spec(uaa, i, &spec)) {
			aprint_error_dev(self,
			    "could not get interface %d specification\n", i);

			return;
		}

		if (ISSET(spec, UHSO_IFACE_MUX))
			uhso_mux_attach(sc, ifh, UHSOPORT(spec));

		if (ISSET(spec, UHSO_IFACE_BULK))
			uhso_bulk_attach(sc, ifh, UHSOPORT(spec));

		if (ISSET(spec, UHSO_IFACE_IFNET))
			uhso_ifnet_attach(sc, ifh, UHSOPORT(spec));
	}

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

int
uhso_detach(device_t self, int flags)
{
	struct uhso_softc *sc = device_private(self);
	struct uhso_port *hp;
	devmajor_t major;
	devminor_t minor;
	unsigned int i;
	int s;

	pmf_device_deregister(self);

	for (i = 0; i < UHSO_PORT_MAX; i++) {
		hp = sc->sc_port[i];
		if (hp != NULL)
			(*hp->hp_abort)(hp);
	}

	s = splusb();
	if (sc->sc_refcnt-- > 0) {
		DPRINTF(1, "waiting for refcnt (%d)..\n", sc->sc_refcnt);
		usb_detach_waitold(sc->sc_dev);
	}
	splx(s);

	/*
	 * XXX the tty close routine increases/decreases refcnt causing
	 * XXX another usb_detach_wakeupold() does it matter, should these
	 * XXX be before the detach_wait? or before the abort?
	 */

	/* Nuke the vnodes for any open instances (calls close). */
	major = cdevsw_lookup_major(&uhso_cdevsw);
	minor = UHSOMINOR(device_unit(sc->sc_dev), 0);
	vdevgone(major, minor, minor + UHSO_PORT_MAX, VCHR);
	minor = UHSOMINOR(device_unit(sc->sc_dev), 0) | UHSO_DIALOUT_MASK;
	vdevgone(major, minor, minor + UHSO_PORT_MAX, VCHR);
	minor = UHSOMINOR(device_unit(sc->sc_dev), 0) | UHSO_CALLUNIT_MASK;
	vdevgone(major, minor, minor + UHSO_PORT_MAX, VCHR);

	for (i = 0; i < UHSO_PORT_MAX; i++) {
		hp = sc->sc_port[i];
		if (hp != NULL)
			(*hp->hp_detach)(hp);
	}

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, sc->sc_dev);

	return 0;
}

/*
 * Send SCSI REZERO_UNIT command to switch device into modem mode
 */
Static int
uhso_switch_mode(usbd_device_handle udev)
{
	umass_bbb_cbw_t	cmd;
	usb_endpoint_descriptor_t *ed;
	usbd_interface_handle ifh;
	usbd_pipe_handle pipe;
	usbd_xfer_handle xfer;
	usbd_status status;

	status = usbd_device2interface_handle(udev, 0, &ifh);
	if (status != USBD_NORMAL_COMPLETION)
		return EIO;

	ed = uhso_get_endpoint(ifh, UE_BULK, UE_DIR_OUT);
	if (ed == NULL)
		return ENODEV;

	status = usbd_open_pipe(ifh, ed->bEndpointAddress, 0, &pipe);
	if (status != USBD_NORMAL_COMPLETION)
		return EIO;

	xfer = usbd_alloc_xfer(udev);
	if (xfer == NULL)
		return ENOMEM;

	USETDW(cmd.dCBWSignature, CBWSIGNATURE);
	USETDW(cmd.dCBWTag, 1);
	USETDW(cmd.dCBWDataTransferLength, 0);
	cmd.bCBWFlags = CBWFLAGS_OUT;
	cmd.bCBWLUN = 0;
	cmd.bCDBLength = 6;

	memset(&cmd.CBWCDB, 0, CBWCDBLENGTH);
	cmd.CBWCDB[0] = SCSI_REZERO_UNIT;

	usbd_setup_xfer(xfer, pipe, NULL, &cmd, sizeof(cmd),
		USBD_SYNCHRONOUS, USBD_DEFAULT_TIMEOUT, NULL);

	status = usbd_transfer(xfer);

	usbd_abort_pipe(pipe);
	usbd_close_pipe(pipe);
	usbd_free_xfer(xfer);

	return (status == USBD_NORMAL_COMPLETION ? 0 : EIO);
}

Static int
uhso_get_iface_spec(struct usb_attach_arg *uaa, uint8_t ifnum, uint8_t *spec)
{
	const struct uhso_dev *hd;
	uint8_t config[17];
	usb_device_request_t req;
	usbd_status status;

	hd = uhso_lookup(uaa->vendor, uaa->product);
	KASSERT(hd != NULL);

	switch (hd->type) {
	case UHSOTYPE_DEFAULT:
		if (ifnum > __arraycount(uhso_spec_default))
			break;

		*spec = uhso_spec_default[ifnum];
		return 1;

	case UHSOTYPE_ICON321:
		if (ifnum > __arraycount(uhso_spec_icon321))
			break;

		*spec = uhso_spec_icon321[ifnum];
		return 1;

	case UHSOTYPE_CONFIG:
		req.bmRequestType = UT_READ_VENDOR_DEVICE;
		req.bRequest = 0x86;	/* "Config Info" */
		USETW(req.wValue, 0);
		USETW(req.wIndex, 0);
		USETW(req.wLength, sizeof(config));

		status = usbd_do_request(uaa->device, &req, config);
		if (status != USBD_NORMAL_COMPLETION)
			break;

		if (ifnum > __arraycount(config)
		    || config[ifnum] > __arraycount(uhso_spec_config))
			break;

		*spec = uhso_spec_config[config[ifnum]];

		/*
		 * Apparently some modems also have a CRC bug that is
		 * indicated by ISSET(config[16], __BIT(0)) but we dont
		 * handle it at this time.
		 */
		return 1;

	default:
		DPRINTF(0, "unknown interface type\n");
		break;
	}

	return 0;
}

Static usb_endpoint_descriptor_t *
uhso_get_endpoint(usbd_interface_handle ifh, int type, int dir)
{
	usb_endpoint_descriptor_t *ed;
	uint8_t count, i;

	count = 0;
	(void)usbd_endpoint_count(ifh, &count);

	for (i = 0; i < count; i++) {
		ed = usbd_interface2endpoint_descriptor(ifh, i);
		if (ed != NULL
		    && UE_GET_XFERTYPE(ed->bmAttributes) == type
		    && UE_GET_DIR(ed->bEndpointAddress) == dir)
			return ed;
	}

	return NULL;
}


/******************************************************************************
 *
 *	Multiplexed ports signal with the interrupt endpoint to indicate
 *  when data is available for reading, and a separate request is made on
 *  the control endpoint to read or write on each port. The offsets in the
 *  table below relate to bit numbers in the mux mask, identifying each port.
 */

Static const int uhso_mux_port[] = {
	UHSO_PORT_CONTROL,
	UHSO_PORT_APP,
	UHSO_PORT_PCSC,
	UHSO_PORT_GPS,
	UHSO_PORT_APP2,
};

Static void
uhso_mux_attach(struct uhso_softc *sc, usbd_interface_handle ifh, int index)
{
	usbd_desc_iter_t iter;
	const usb_descriptor_t *desc;
	usb_endpoint_descriptor_t *ed;
	usbd_pipe_handle pipe;
	struct uhso_port *hp;
	uint8_t *buf;
	size_t size;
	unsigned int i, mux, flags;
	int addr;
	usbd_status status;

	ed = uhso_get_endpoint(ifh, UE_INTERRUPT, UE_DIR_IN);
	if (ed == NULL) {
		aprint_error_dev(sc->sc_dev, "no interrupt endpoint\n");
		return;
	}
	addr = ed->bEndpointAddress;
	size = UGETW(ed->wMaxPacketSize);

	/*
	 * There should be an additional "Class Specific" descriptor on
	 * the mux interface containing a single byte with a bitmask of
	 * enabled ports. We need to look through the device descriptor
	 * to find it and the port index is found from the uhso_mux_port
	 * array, above.
	 */
	usb_desc_iter_init(sc->sc_udev, &iter);

	/* skip past the current interface descriptor */
	iter.cur = (const uByte *)usbd_get_interface_descriptor(ifh);
	desc = usb_desc_iter_next(&iter);

	for (;;) {
		desc = usb_desc_iter_next(&iter);
		if (desc == NULL
		    || desc->bDescriptorType == UDESC_INTERFACE) {
			mux = 0;
			break;	/* not found */
		}

		if (desc->bDescriptorType == UDESC_CS_INTERFACE
		    && desc->bLength == 3) {
			mux = ((const uint8_t *)desc)[2];
			break;
		}
	}

	DPRINTF(1, "addr=%d, size=%zd, mux=0x%02x\n", addr, size, mux);

	buf = kmem_alloc(size, KM_SLEEP);
	status = usbd_open_pipe_intr(ifh, addr, USBD_SHORT_XFER_OK, &pipe,
	    sc, buf, size, uhso_mux_intr, USBD_DEFAULT_INTERVAL);

	if (status != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(sc->sc_dev, "failed to open interrupt pipe: %s",
		    usbd_errstr(status));

		kmem_free(buf, size);
		return;
	}

	flags = 0;
	for (i = 0; i < __arraycount(uhso_mux_port); i++) {
		if (ISSET(mux, __BIT(i))) {
			if (sc->sc_port[uhso_mux_port[i]] != NULL) {
				aprint_error_dev(sc->sc_dev,
				    "mux port %d is duplicate!\n", i);

				continue;
			}

			hp = kmem_zalloc(sizeof(struct uhso_port), KM_SLEEP);
			sc->sc_port[uhso_mux_port[i]] = hp;

			hp->hp_sc = sc;
			hp->hp_index = i;
			hp->hp_ipipe = pipe;
			hp->hp_ibuf = buf;
			hp->hp_isize = size;
			hp->hp_flags = flags;
			hp->hp_abort = uhso_mux_abort;
			hp->hp_detach = uhso_mux_detach;
			hp->hp_init = uhso_mux_init;
			hp->hp_clean = uhso_mux_clean;
			hp->hp_write = uhso_mux_write;
			hp->hp_write_cb = uhso_tty_write_cb;
			hp->hp_read = uhso_mux_read;
			hp->hp_read_cb = uhso_tty_read_cb;
			hp->hp_control = uhso_mux_control;
			hp->hp_wsize = UHSO_MUX_WSIZE;
			hp->hp_rsize = UHSO_MUX_RSIZE;

			uhso_tty_attach(hp);

			aprint_normal_dev(sc->sc_dev,
			    "%s (port %d) attached as mux tty\n",
			    uhso_port_name[uhso_mux_port[i]], uhso_mux_port[i]);

			/*
			 * As the pipe handle is stored in each mux, mark
			 * secondary references so they don't get released
			 */
			flags = UHSO_PORT_MUXPIPE;
		}
	}

	if (flags == 0) {
		/* for whatever reasons, nothing was attached */
		usbd_abort_pipe(pipe);
		usbd_close_pipe(pipe);
		kmem_free(buf, size);
	}
}

Static int
uhso_mux_abort(struct uhso_port *hp)
{
	struct uhso_softc *sc = hp->hp_sc;

	DPRINTF(1, "hp=%p\n", hp);

	if (!ISSET(hp->hp_flags, UHSO_PORT_MUXPIPE))
		usbd_abort_pipe(hp->hp_ipipe);

	usbd_abort_default_pipe(sc->sc_udev);

	return (*hp->hp_clean)(hp);
}

Static int
uhso_mux_detach(struct uhso_port *hp)
{

	DPRINTF(1, "hp=%p\n", hp);

	if (!ISSET(hp->hp_flags, UHSO_PORT_MUXPIPE)) {
		DPRINTF(1, "interrupt pipe closed\n");
		usbd_abort_pipe(hp->hp_ipipe);
		usbd_close_pipe(hp->hp_ipipe);
		kmem_free(hp->hp_ibuf, hp->hp_isize);
	}

	uhso_tty_detach(hp);
	kmem_free(hp, sizeof(struct uhso_port));
	return 0;
}

Static int
uhso_mux_init(struct uhso_port *hp)
{

	DPRINTF(1, "hp=%p\n", hp);

	CLR(hp->hp_flags, UHSO_PORT_MUXBUSY | UHSO_PORT_MUXREADY);
	SET(hp->hp_status, TIOCM_DSR | TIOCM_CAR);
	return 0;
}

Static int
uhso_mux_clean(struct uhso_port *hp)
{

	DPRINTF(1, "hp=%p\n", hp);

	CLR(hp->hp_flags, UHSO_PORT_MUXREADY);
	CLR(hp->hp_status, TIOCM_DTR | TIOCM_DSR | TIOCM_CAR);
	return 0;
}

Static int
uhso_mux_write(struct uhso_port *hp)
{
	struct uhso_softc *sc = hp->hp_sc;
	usb_device_request_t req;
	usbd_status status;

	DPRINTF(5, "hp=%p, index=%d, wlen=%zd\n", hp, hp->hp_index, hp->hp_wlen);

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SEND_ENCAPSULATED_COMMAND;
	USETW(req.wValue, 0);
	USETW(req.wIndex, hp->hp_index);
	USETW(req.wLength, hp->hp_wlen);

	usbd_setup_default_xfer(hp->hp_wxfer, sc->sc_udev, hp, USBD_NO_TIMEOUT,
	    &req, hp->hp_wbuf, hp->hp_wlen, USBD_NO_COPY, hp->hp_write_cb);

	status = usbd_transfer(hp->hp_wxfer);
	if (status != USBD_IN_PROGRESS) {
		DPRINTF(0, "non-normal status %s\n", usbd_errstr(status));
		return EIO;
	}

	sc->sc_refcnt++;
	return 0;
}

Static int
uhso_mux_read(struct uhso_port *hp)
{
	struct uhso_softc *sc = hp->hp_sc;
	usb_device_request_t req;
	usbd_status status;

	CLR(hp->hp_flags, UHSO_PORT_MUXBUSY);

	if (hp->hp_rlen == 0 && !ISSET(hp->hp_flags, UHSO_PORT_MUXREADY))
		return 0;

	SET(hp->hp_flags, UHSO_PORT_MUXBUSY);
	CLR(hp->hp_flags, UHSO_PORT_MUXREADY);

	DPRINTF(5, "hp=%p, index=%d\n", hp, hp->hp_index);

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UCDC_GET_ENCAPSULATED_RESPONSE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, hp->hp_index);
	USETW(req.wLength, hp->hp_rsize);

	usbd_setup_default_xfer(hp->hp_rxfer, sc->sc_udev, hp, USBD_NO_TIMEOUT,
	    &req, hp->hp_rbuf, hp->hp_rsize, USBD_NO_COPY | USBD_SHORT_XFER_OK,
	    hp->hp_read_cb);

	status = usbd_transfer(hp->hp_rxfer);
	if (status != USBD_IN_PROGRESS) {
		DPRINTF(0, "non-normal status %s\n", usbd_errstr(status));
		CLR(hp->hp_flags, UHSO_PORT_MUXBUSY);
		return EIO;
	}

	sc->sc_refcnt++;
	return 0;
}

Static int
uhso_mux_control(struct uhso_port *hp)
{

	DPRINTF(1, "hp=%p\n", hp);

	return 0;
}

Static void
uhso_mux_intr(usbd_xfer_handle xfer, usbd_private_handle p, usbd_status status)
{
	struct uhso_softc *sc = p;
	struct uhso_port *hp;
	uint32_t cc;
	uint8_t *buf;
	unsigned int i;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(0, "non-normal status %s\n", usbd_errstr(status));
		return;
	}

	usbd_get_xfer_status(xfer, NULL, (void **)&buf, &cc, NULL);
	if (cc == 0)
		return;

	DPRINTF(5, "mux mask 0x%02x, cc=%u\n", buf[0], cc);

	for (i = 0; i < __arraycount(uhso_mux_port); i++) {
		if (!ISSET(buf[0], __BIT(i)))
			continue;

		DPRINTF(5, "mux %d port %d\n", i, uhso_mux_port[i]);
		hp = sc->sc_port[uhso_mux_port[i]];
		if (hp == NULL
		    || hp->hp_tp == NULL
		    || !ISSET(hp->hp_status, TIOCM_DTR))
			continue;

		SET(hp->hp_flags, UHSO_PORT_MUXREADY);
		if (ISSET(hp->hp_flags, UHSO_PORT_MUXBUSY))
			continue;

		uhso_mux_read(hp);
	}
}


/******************************************************************************
 *
 *	Bulk ports operate using the bulk endpoints on an interface, though
 *   the Modem port (at least) may have an interrupt endpoint that will pass
 *   CDC Notification messages with the modem status.
 */

Static void
uhso_bulk_attach(struct uhso_softc *sc, usbd_interface_handle ifh, int index)
{
	usb_endpoint_descriptor_t *ed;
	usb_interface_descriptor_t *id;
	struct uhso_port *hp;
	int in, out;

	ed = uhso_get_endpoint(ifh, UE_BULK, UE_DIR_IN);
	if (ed == NULL) {
		aprint_error_dev(sc->sc_dev, "bulk-in endpoint not found\n");
		return;
	}
	in = ed->bEndpointAddress;

	ed = uhso_get_endpoint(ifh, UE_BULK, UE_DIR_OUT);
	if (ed == NULL) {
		aprint_error_dev(sc->sc_dev, "bulk-out endpoint not found\n");
		return;
	}
	out = ed->bEndpointAddress;

	id = usbd_get_interface_descriptor(ifh);
	if (id == NULL) {
		aprint_error_dev(sc->sc_dev, "interface descriptor not found\n");
		return;
	}

	DPRINTF(1, "bulk endpoints in=%x, out=%x\n", in, out);

	if (sc->sc_port[index] != NULL) {
		aprint_error_dev(sc->sc_dev, "bulk port %d is duplicate!\n",
		    index);

		return;
	}

	hp = kmem_zalloc(sizeof(struct uhso_port), KM_SLEEP);
	sc->sc_port[index] = hp;

	hp->hp_sc = sc;
	hp->hp_ifh = ifh;
	hp->hp_index = id->bInterfaceNumber;
	hp->hp_raddr = in;
	hp->hp_waddr = out;
	hp->hp_abort = uhso_bulk_abort;
	hp->hp_detach = uhso_bulk_detach;
	hp->hp_init = uhso_bulk_init;
	hp->hp_clean = uhso_bulk_clean;
	hp->hp_write = uhso_bulk_write;
	hp->hp_write_cb = uhso_tty_write_cb;
	hp->hp_read = uhso_bulk_read;
	hp->hp_read_cb = uhso_tty_read_cb;
	hp->hp_control = uhso_bulk_control;
	hp->hp_wsize = UHSO_BULK_WSIZE;
	hp->hp_rsize = UHSO_BULK_RSIZE;

	if (index == UHSO_PORT_MODEM) {
		ed = uhso_get_endpoint(ifh, UE_INTERRUPT, UE_DIR_IN);
		if (ed != NULL) {
			hp->hp_iaddr = ed->bEndpointAddress;
			hp->hp_isize = UGETW(ed->wMaxPacketSize);
		}
	}

	uhso_tty_attach(hp);

	aprint_normal_dev(sc->sc_dev,
	    "%s (port %d) attached as bulk tty\n",
	    uhso_port_name[index], index);
}

Static int
uhso_bulk_abort(struct uhso_port *hp)
{

	DPRINTF(1, "hp=%p\n", hp);

	return (*hp->hp_clean)(hp);
}

Static int
uhso_bulk_detach(struct uhso_port *hp)
{

	DPRINTF(1, "hp=%p\n", hp);

	uhso_tty_detach(hp);
	kmem_free(hp, sizeof(struct uhso_port));
	return 0;
}

Static int
uhso_bulk_init(struct uhso_port *hp)
{
	usbd_status status;

	DPRINTF(1, "hp=%p\n", hp);

	if (hp->hp_isize > 0) {
		hp->hp_ibuf = kmem_alloc(hp->hp_isize, KM_SLEEP);

		status = usbd_open_pipe_intr(hp->hp_ifh, hp->hp_iaddr,
		    USBD_SHORT_XFER_OK, &hp->hp_ipipe, hp, hp->hp_ibuf,
		    hp->hp_isize, uhso_bulk_intr, USBD_DEFAULT_INTERVAL);

		if (status != USBD_NORMAL_COMPLETION) {
			DPRINTF(0, "interrupt pipe open failed: %s\n",
			    usbd_errstr(status));

			return EIO;
		}
	}

	status = usbd_open_pipe(hp->hp_ifh, hp->hp_raddr, 0, &hp->hp_rpipe);
	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(0, "read pipe open failed: %s\n", usbd_errstr(status));
		return EIO;
	}

	status = usbd_open_pipe(hp->hp_ifh, hp->hp_waddr, 0, &hp->hp_wpipe);
	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(0, "write pipe open failed: %s\n", usbd_errstr(status));
		return EIO;
	}

	return 0;
}

Static int
uhso_bulk_clean(struct uhso_port *hp)
{

	DPRINTF(1, "hp=%p\n", hp);

	if (hp->hp_ipipe != NULL) {
		usbd_abort_pipe(hp->hp_ipipe);
		usbd_close_pipe(hp->hp_ipipe);
		hp->hp_ipipe = NULL;
	}

	if (hp->hp_ibuf != NULL) {
		kmem_free(hp->hp_ibuf, hp->hp_isize);
		hp->hp_ibuf = NULL;
	}

	if (hp->hp_rpipe != NULL) {
		usbd_abort_pipe(hp->hp_rpipe);
		usbd_close_pipe(hp->hp_rpipe);
		hp->hp_rpipe = NULL;
	}

	if (hp->hp_wpipe != NULL) {
		usbd_abort_pipe(hp->hp_wpipe);
		usbd_close_pipe(hp->hp_wpipe);
		hp->hp_wpipe = NULL;
	}

	return 0;
}

Static int
uhso_bulk_write(struct uhso_port *hp)
{
	struct uhso_softc *sc = hp->hp_sc;
	usbd_status status;

	DPRINTF(5, "hp=%p, wlen=%zd\n", hp, hp->hp_wlen);

	usbd_setup_xfer(hp->hp_wxfer, hp->hp_wpipe, hp, hp->hp_wbuf,
	    hp->hp_wlen, USBD_NO_COPY, USBD_NO_TIMEOUT, hp->hp_write_cb);

	status = usbd_transfer(hp->hp_wxfer);
	if (status != USBD_IN_PROGRESS) {
		DPRINTF(0, "non-normal status %s\n", usbd_errstr(status));
		return EIO;
	}

	sc->sc_refcnt++;
	return 0;
}

Static int
uhso_bulk_read(struct uhso_port *hp)
{
	struct uhso_softc *sc = hp->hp_sc;
	usbd_status status;

	DPRINTF(5, "hp=%p\n", hp);

	usbd_setup_xfer(hp->hp_rxfer, hp->hp_rpipe, hp, hp->hp_rbuf,
	    hp->hp_rsize, USBD_NO_COPY | USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, hp->hp_read_cb);

	status = usbd_transfer(hp->hp_rxfer);
	if (status != USBD_IN_PROGRESS) {
		DPRINTF(0, "non-normal status %s\n", usbd_errstr(status));
		return EIO;
	}

	sc->sc_refcnt++;
	return 0;
}

Static int
uhso_bulk_control(struct uhso_port *hp)
{
	struct uhso_softc *sc = hp->hp_sc;
	usb_device_request_t req;
	usbd_status status;
	int val;

	DPRINTF(1, "hp=%p\n", hp);

	if (hp->hp_isize == 0)
		return 0;

	val = 0;
	if (ISSET(hp->hp_status, TIOCM_DTR))
		SET(val, UCDC_LINE_DTR);
	if (ISSET(hp->hp_status, TIOCM_RTS))
		SET(val, UCDC_LINE_RTS);

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	USETW(req.wValue, val);
	USETW(req.wIndex, hp->hp_index);
	USETW(req.wLength, 0);

	sc->sc_refcnt++;

	status = usbd_do_request(sc->sc_udev, &req, NULL);

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(0, "non-normal status %s\n", usbd_errstr(status));
		return EIO;
	}

	return 0;
}

Static void
uhso_bulk_intr(usbd_xfer_handle xfer, usbd_private_handle p, usbd_status status)
{
	struct uhso_port *hp = p;
	struct tty *tp = hp->hp_tp;
	usb_cdc_notification_t *msg;
	uint32_t cc;
	int s, old;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(0, "non-normal status %s\n", usbd_errstr(status));
		return;
	}

	usbd_get_xfer_status(xfer, NULL, (void **)&msg, &cc, NULL);

	if (cc < UCDC_NOTIFICATION_LENGTH
	    || msg->bmRequestType != UCDC_NOTIFICATION
	    || msg->bNotification != UCDC_N_SERIAL_STATE
	    || UGETW(msg->wValue) != 0
	    || UGETW(msg->wIndex) != hp->hp_index
	    || UGETW(msg->wLength) < 1)
		return;

	DPRINTF(5, "state=%02x\n", msg->data[0]);

	old = hp->hp_status;
	CLR(hp->hp_status, TIOCM_RNG | TIOCM_DSR | TIOCM_CAR);
	if (ISSET(msg->data[0], UCDC_N_SERIAL_RI))
		SET(hp->hp_status, TIOCM_RNG);
	if (ISSET(msg->data[0], UCDC_N_SERIAL_DSR))
		SET(hp->hp_status, TIOCM_DSR);
	if (ISSET(msg->data[0], UCDC_N_SERIAL_DCD))
		SET(hp->hp_status, TIOCM_CAR);

	if (ISSET(hp->hp_status ^ old, TIOCM_CAR)) {
		s = spltty();
		tp->t_linesw->l_modem(tp, ISSET(hp->hp_status, TIOCM_CAR));
		splx(s);
	}

	if (ISSET((hp->hp_status ^ old), TIOCM_RNG | TIOCM_DSR | TIOCM_CAR))
		DPRINTF(1, "RNG %s, DSR %s, DCD %s\n",
		    (ISSET(hp->hp_status, TIOCM_RNG) ? "on" : "off"),
		    (ISSET(hp->hp_status, TIOCM_DSR) ? "on" : "off"),
		    (ISSET(hp->hp_status, TIOCM_CAR) ? "on" : "off"));
}


/******************************************************************************
 *
 *	TTY management
 *
 */

Static void
uhso_tty_attach(struct uhso_port *hp)
{
	struct tty *tp;

	tp = tty_alloc();
	tp->t_oproc = uhso_tty_start;
	tp->t_param = uhso_tty_param;

	hp->hp_tp = tp;
	tty_attach(tp);

	DPRINTF(1, "hp=%p, tp=%p\n", hp, tp);
}

Static void
uhso_tty_detach(struct uhso_port *hp)
{

	DPRINTF(1, "hp=%p\n", hp);

	uhso_tty_clean(hp);

	tty_detach(hp->hp_tp);
	tty_free(hp->hp_tp);
	hp->hp_tp = NULL;
}

Static void
uhso_tty_write_cb(usbd_xfer_handle xfer, usbd_private_handle p, usbd_status status)
{
	struct uhso_port *hp = p;
	struct uhso_softc *sc = hp->hp_sc;
	struct tty *tp = hp->hp_tp;
	uint32_t cc;
	int s;

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(0, "non-normal status %s\n", usbd_errstr(status));

		if (status == USBD_STALLED && hp->hp_wpipe != NULL)
			usbd_clear_endpoint_stall_async(hp->hp_wpipe);
		else
			return;
	} else {
		usbd_get_xfer_status(xfer, NULL, NULL, &cc, NULL);

		DPRINTF(5, "wrote %d bytes (of %zd)\n", cc, hp->hp_wlen);
		if (cc != hp->hp_wlen)
			DPRINTF(0, "cc=%u, wlen=%zd\n", cc, hp->hp_wlen);
	}

	s = spltty();
	CLR(tp->t_state, TS_BUSY);
	tp->t_linesw->l_start(tp);
	splx(s);
}

Static void
uhso_tty_read_cb(usbd_xfer_handle xfer, usbd_private_handle p, usbd_status status)
{
	struct uhso_port *hp = p;
	struct uhso_softc *sc = hp->hp_sc;
	struct tty *tp = hp->hp_tp;
	uint8_t *cp;
	uint32_t cc;
	int s;

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(0, "non-normal status: %s\n", usbd_errstr(status));

		if (status == USBD_STALLED && hp->hp_rpipe != NULL)
			usbd_clear_endpoint_stall_async(hp->hp_rpipe);
		else
			return;

		hp->hp_rlen = 0;
	} else {
		usbd_get_xfer_status(xfer, NULL, (void **)&cp, &cc, NULL);

		hp->hp_rlen = cc;
		DPRINTF(5, "read %d bytes\n", cc);

		s = spltty();
		while (cc > 0) {
			if (tp->t_linesw->l_rint(*cp++, tp) == -1) {
				DPRINTF(0, "lost %d bytes\n", cc);
				break;
			}

			cc--;
		}
		splx(s);
	}

	(*hp->hp_read)(hp);
}


/******************************************************************************
 *
 *	TTY subsystem
 *
 */

int
uhso_tty_open(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct uhso_softc *sc;
	struct uhso_port *hp;
	struct tty *tp;
	int error, s;

	DPRINTF(1, "unit %d port %d\n", UHSOUNIT(dev), UHSOPORT(dev));

	sc = device_lookup_private(&uhso_cd, UHSOUNIT(dev));
	if (sc == NULL
	    || !device_is_active(sc->sc_dev)
	    || UHSOPORT(dev) >= UHSO_PORT_MAX)
		return ENXIO;

	hp = sc->sc_port[UHSOPORT(dev)];
	if (hp == NULL || hp->hp_tp == NULL)
		return ENXIO;

	tp = hp->hp_tp;
	if (kauth_authorize_device_tty(l->l_cred, KAUTH_DEVICE_TTY_OPEN, tp))
		return EBUSY;

	error = 0;
	s = spltty();
	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
		tp->t_dev = dev;
		error = uhso_tty_init(hp);
	}
	splx(s);

	if (error == 0) {
		error = ttyopen(tp, UHSODIALOUT(dev), ISSET(flag, O_NONBLOCK));
		if (error == 0) {
			error = tp->t_linesw->l_open(dev, tp);
		}
	}

	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0)
		uhso_tty_clean(hp);

	DPRINTF(1, "sc=%p, hp=%p, tp=%p, error=%d\n", sc, hp, tp, error);

	return error;
}

Static int
uhso_tty_init(struct uhso_port *hp)
{
	struct uhso_softc *sc = hp->hp_sc;
	struct tty *tp = hp->hp_tp;
	struct termios t;
	int error;

	DPRINTF(1, "sc=%p, hp=%p, tp=%p\n", sc, hp, tp);

	/*
	 * Initialize the termios status to the defaults.  Add in the
	 * sticky bits from TIOCSFLAGS.
	 */
	t.c_ispeed = 0;
	t.c_ospeed = TTYDEF_SPEED;
	t.c_cflag = TTYDEF_CFLAG;
	if (ISSET(hp->hp_swflags, TIOCFLAG_CLOCAL))
		SET(t.c_cflag, CLOCAL);
	if (ISSET(hp->hp_swflags, TIOCFLAG_CRTSCTS))
		SET(t.c_cflag, CRTSCTS);
	if (ISSET(hp->hp_swflags, TIOCFLAG_MDMBUF))
		SET(t.c_cflag, MDMBUF);

	/* Ensure uhso_tty_param() will do something. */
	tp->t_ospeed = 0;
	(void)uhso_tty_param(tp, &t);

	tp->t_iflag = TTYDEF_IFLAG;
	tp->t_oflag = TTYDEF_OFLAG;
	tp->t_lflag = TTYDEF_LFLAG;
	ttychars(tp);
	ttsetwater(tp);

	hp->hp_status = 0;
	error = (*hp->hp_init)(hp);
	if (error != 0)
		return error;

	hp->hp_rxfer = usbd_alloc_xfer(sc->sc_udev);
	if (hp->hp_rxfer == NULL)
		return ENOMEM;

	hp->hp_rbuf = usbd_alloc_buffer(hp->hp_rxfer, hp->hp_rsize);
	if (hp->hp_rbuf == NULL)
		return ENOMEM;

	hp->hp_wxfer = usbd_alloc_xfer(sc->sc_udev);
	if (hp->hp_wxfer == NULL)
		return ENOMEM;

	hp->hp_wbuf = usbd_alloc_buffer(hp->hp_wxfer, hp->hp_wsize);
	if (hp->hp_wbuf == NULL)
		return ENOMEM;

	/*
	 * Turn on DTR.  We must always do this, even if carrier is not
	 * present, because otherwise we'd have to use TIOCSDTR
	 * immediately after setting CLOCAL, which applications do not
	 * expect.  We always assert DTR while the port is open
	 * unless explicitly requested to deassert it.  Ditto RTS.
	 */
	uhso_tty_control(hp, TIOCMBIS, TIOCM_DTR | TIOCM_RTS);

	/* and start reading */
	error = (*hp->hp_read)(hp);
	if (error != 0)
		return error;

	return 0;
}

int
uhso_tty_close(dev_t dev, int flag, int mode, struct lwp *l)
{
	struct uhso_softc *sc = device_lookup_private(&uhso_cd, UHSOUNIT(dev));
	struct uhso_port *hp = sc->sc_port[UHSOPORT(dev)];
	struct tty *tp = hp->hp_tp;

	if (!ISSET(tp->t_state, TS_ISOPEN))
		return 0;

	DPRINTF(1, "sc=%p, hp=%p, tp=%p\n", sc, hp, tp);

	sc->sc_refcnt++;

	tp->t_linesw->l_close(tp, flag);
	ttyclose(tp);

	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0)
		uhso_tty_clean(hp);

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	return 0;
}

Static void
uhso_tty_clean(struct uhso_port *hp)
{

	DPRINTF(1, "hp=%p\n", hp);

	if (ISSET(hp->hp_status, TIOCM_DTR)
	    && ISSET(hp->hp_tp->t_cflag, HUPCL))
		uhso_tty_control(hp, TIOCMBIC, TIOCM_DTR);

	(*hp->hp_clean)(hp);

	if (hp->hp_rxfer != NULL) {
		usbd_free_xfer(hp->hp_rxfer);
		hp->hp_rxfer = NULL;
		hp->hp_rbuf = NULL;
	}

	if (hp->hp_wxfer != NULL) {
		usbd_free_xfer(hp->hp_wxfer);
		hp->hp_wxfer = NULL;
		hp->hp_wbuf = NULL;
	}
}

int
uhso_tty_read(dev_t dev, struct uio *uio, int flag)
{
	struct uhso_softc *sc = device_lookup_private(&uhso_cd, UHSOUNIT(dev));
	struct uhso_port *hp = sc->sc_port[UHSOPORT(dev)];
	struct tty *tp = hp->hp_tp;
	int error;

	if (!device_is_active(sc->sc_dev))
		return EIO;

	DPRINTF(5, "sc=%p, hp=%p, tp=%p\n", sc, hp, tp);

	sc->sc_refcnt++;

	error = tp->t_linesw->l_read(tp, uio, flag);

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	return error;
}

int
uhso_tty_write(dev_t dev, struct uio *uio, int flag)
{
	struct uhso_softc *sc = device_lookup_private(&uhso_cd, UHSOUNIT(dev));
	struct uhso_port *hp = sc->sc_port[UHSOPORT(dev)];
	struct tty *tp = hp->hp_tp;
	int error;

	if (!device_is_active(sc->sc_dev))
		return EIO;

	DPRINTF(5, "sc=%p, hp=%p, tp=%p\n", sc, hp, tp);

	sc->sc_refcnt++;

	error = tp->t_linesw->l_write(tp, uio, flag);

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	return error;
}

int
uhso_tty_ioctl(dev_t dev, u_long cmd, void *data, int flag, struct lwp *l)
{
	struct uhso_softc *sc = device_lookup_private(&uhso_cd, UHSOUNIT(dev));
	struct uhso_port *hp = sc->sc_port[UHSOPORT(dev)];
	int error;

	if (!device_is_active(sc->sc_dev))
		return EIO;

	DPRINTF(1, "sc=%p, hp=%p\n", sc, hp);

	sc->sc_refcnt++;

	error = uhso_tty_do_ioctl(hp, cmd, data, flag, l);

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	return error;
}

Static int
uhso_tty_do_ioctl(struct uhso_port *hp, u_long cmd, void *data, int flag,
    struct lwp *l)
{
	struct tty *tp = hp->hp_tp;
	int error, s;

	error = tp->t_linesw->l_ioctl(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return error;

	error = ttioctl(tp, cmd, data, flag, l);
	if (error != EPASSTHROUGH)
		return error;

	error = 0;

	s = spltty();

	switch (cmd) {
	case TIOCSDTR:
		error = uhso_tty_control(hp, TIOCMBIS, TIOCM_DTR);
		break;

	case TIOCCDTR:
		error = uhso_tty_control(hp, TIOCMBIC, TIOCM_DTR);
		break;

	case TIOCGFLAGS:
		*(int *)data = hp->hp_swflags;
		break;

	case TIOCSFLAGS:
		error = kauth_authorize_device_tty(l->l_cred,
		    KAUTH_DEVICE_TTY_PRIVSET, tp);

		if (error)
			break;

		hp->hp_swflags = *(int *)data;
		break;

	case TIOCMSET:
	case TIOCMBIS:
	case TIOCMBIC:
		error = uhso_tty_control(hp, cmd, *(int *)data);
		break;

	case TIOCMGET:
		*(int *)data = hp->hp_status;
		break;

	default:
		error = EPASSTHROUGH;
		break;
	}

	splx(s);

	return error;
}

/* this is called with tty_lock held */
void
uhso_tty_stop(struct tty *tp, int flag)
{
#if 0
	struct uhso_softc *sc = device_lookup_private(&uhso_cd, UHSOUNIT(tp->t_dev));
	struct uhso_port *hp = sc->sc_port[UHSOPORT(tp->t_dev)];
#endif
}

struct tty *
uhso_tty_tty(dev_t dev)
{
	struct uhso_softc *sc = device_lookup_private(&uhso_cd, UHSOUNIT(dev));
	struct uhso_port *hp = sc->sc_port[UHSOPORT(dev)];

	return hp->hp_tp;
}

int
uhso_tty_poll(dev_t dev, int events, struct lwp *l)
{
	struct uhso_softc *sc = device_lookup_private(&uhso_cd, UHSOUNIT(dev));
	struct uhso_port *hp = sc->sc_port[UHSOPORT(dev)];
	struct tty *tp = hp->hp_tp;
        int revents;

	if (!device_is_active(sc->sc_dev))
                return POLLHUP;

	sc->sc_refcnt++;

        revents = tp->t_linesw->l_poll(tp, events, l);

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

        return revents;
}

Static int
uhso_tty_param(struct tty *tp, struct termios *t)
{
	struct uhso_softc *sc = device_lookup_private(&uhso_cd, UHSOUNIT(tp->t_dev));
	struct uhso_port *hp = sc->sc_port[UHSOPORT(tp->t_dev)];

	if (!device_is_active(sc->sc_dev))
		return EIO;

	DPRINTF(1, "hp=%p, tp=%p, termios iflag=%x, oflag=%x, cflag=%x\n",
	    hp, tp, t->c_iflag, t->c_oflag, t->c_cflag);

	/* Check requested parameters. */
	if (t->c_ispeed != 0
	    && t->c_ispeed != t->c_ospeed)
		return EINVAL;

	/* force CLOCAL and !HUPCL for console */
	if (ISSET(hp->hp_swflags, TIOCFLAG_SOFTCAR)) {
		SET(t->c_cflag, CLOCAL);
		CLR(t->c_cflag, HUPCL);
	}

	/* If there were no changes, don't do anything.  */
	if (tp->t_ospeed == t->c_ospeed
	    && tp->t_cflag == t->c_cflag)
		return 0;

	tp->t_ispeed = 0;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	/* update tty layers idea of carrier bit */
	tp->t_linesw->l_modem(tp, ISSET(hp->hp_status, TIOCM_CAR));
	return 0;
}

/* this is called with tty_lock held */
Static void
uhso_tty_start(struct tty *tp)
{
	struct uhso_softc *sc = device_lookup_private(&uhso_cd, UHSOUNIT(tp->t_dev));
	struct uhso_port *hp = sc->sc_port[UHSOPORT(tp->t_dev)];
	int s;

	if (!device_is_active(sc->sc_dev))
		return;

	s = spltty();

	if (!ISSET(tp->t_state, TS_BUSY | TS_TIMEOUT | TS_TTSTOP)
	    && ttypull(tp) != 0) {
		hp->hp_wlen = q_to_b(&tp->t_outq, hp->hp_wbuf, hp->hp_wsize);
		if (hp->hp_wlen > 0) {
			SET(tp->t_state, TS_BUSY);
			(*hp->hp_write)(hp);
		}
	}

	splx(s);
}

Static int
uhso_tty_control(struct uhso_port *hp, u_long cmd, int bits)
{

	bits &= (TIOCM_DTR | TIOCM_RTS);
	DPRINTF(1, "cmd %s, DTR=%d, RTS=%d\n",
	    (cmd == TIOCMBIC ? "BIC" : (cmd == TIOCMBIS ? "BIS" : "SET")),
	    (bits & TIOCM_DTR) ? 1 : 0,
	    (bits & TIOCM_RTS) ? 1 : 0);

	switch (cmd) {
	case TIOCMBIC:
		CLR(hp->hp_status, bits);
		break;

	case TIOCMBIS:
		SET(hp->hp_status, bits);
		break;

	case TIOCMSET:
		CLR(hp->hp_status, TIOCM_DTR | TIOCM_RTS);
		SET(hp->hp_status, bits);
		break;
	}

	return (*hp->hp_control)(hp);
}


/******************************************************************************
 *
 *	Network Interface
 *
 */

Static void
uhso_ifnet_attach(struct uhso_softc *sc, usbd_interface_handle ifh, int index)
{
	usb_endpoint_descriptor_t *ed;
	struct uhso_port *hp;
	struct ifnet *ifp;
	int in, out;

	ed = uhso_get_endpoint(ifh, UE_BULK, UE_DIR_IN);
	if (ed == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "could not find bulk-in endpoint\n");

		return;
	}
	in = ed->bEndpointAddress;

	ed = uhso_get_endpoint(ifh, UE_BULK, UE_DIR_OUT);
	if (ed == NULL) {
		aprint_error_dev(sc->sc_dev,
		    "could not find bulk-out endpoint\n");

		return;
	}
	out = ed->bEndpointAddress;

	DPRINTF(1, "in=%d, out=%d\n", in, out);

	if (sc->sc_port[index] != NULL) {
		aprint_error_dev(sc->sc_dev,
		    "ifnet port %d is duplicate!\n", index);

		return;
	}

	hp = kmem_zalloc(sizeof(struct uhso_port), KM_SLEEP);
	sc->sc_port[index] = hp;

	ifp = if_alloc(IFT_IP);
	strlcpy(ifp->if_xname, device_xname(sc->sc_dev), IFNAMSIZ);
	ifp->if_softc = hp;
	ifp->if_mtu = UHSO_IFNET_MTU;
	ifp->if_dlt = DLT_RAW;
	ifp->if_type = IFT_IP;
	ifp->if_flags = IFF_NOARP | IFF_SIMPLEX;
	ifp->if_ioctl = uhso_ifnet_ioctl;
	ifp->if_start = uhso_ifnet_start;
	ifp->if_output = uhso_ifnet_output;
	IFQ_SET_READY(&ifp->if_snd);

	hp->hp_sc = sc;
	hp->hp_ifp = ifp;
	hp->hp_ifh = ifh;
	hp->hp_raddr = in;
	hp->hp_waddr = out;
	hp->hp_abort = uhso_ifnet_abort;
	hp->hp_detach = uhso_ifnet_detach;
	hp->hp_init = uhso_bulk_init;
	hp->hp_clean = uhso_bulk_clean;
	hp->hp_write = uhso_bulk_write;
	hp->hp_write_cb = uhso_ifnet_write_cb;
	hp->hp_read = uhso_bulk_read;
	hp->hp_read_cb = uhso_ifnet_read_cb;
	hp->hp_wsize = MCLBYTES;
	hp->hp_rsize = MCLBYTES;

	if_attach(ifp);
	if_alloc_sadl(ifp);
	bpf_attach(ifp, DLT_RAW, 0);

	aprint_normal_dev(sc->sc_dev, "%s (port %d) attached as ifnet\n",
	    uhso_port_name[index], index);
}

Static int
uhso_ifnet_abort(struct uhso_port *hp)
{
	struct ifnet *ifp = hp->hp_ifp;

	/* All ifnet IO will abort when IFF_RUNNING is not set */
	CLR(ifp->if_flags, IFF_RUNNING);

	return (*hp->hp_clean)(hp);
}

Static int
uhso_ifnet_detach(struct uhso_port *hp)
{
	struct ifnet *ifp = hp->hp_ifp;
	int s;

	s = splnet();
	bpf_detach(ifp);
	if_detach(ifp);
	splx(s);

	kmem_free(hp, sizeof(struct uhso_port));
	return 0;
}

Static void
uhso_ifnet_write_cb(usbd_xfer_handle xfer, usbd_private_handle p, usbd_status status)
{
	struct uhso_port *hp = p;
	struct uhso_softc *sc= hp->hp_sc;
	struct ifnet *ifp = hp->hp_ifp;
	uint32_t cc;
	int s;

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(0, "non-normal status %s\n", usbd_errstr(status));

		if (status == USBD_STALLED && hp->hp_wpipe != NULL)
			usbd_clear_endpoint_stall_async(hp->hp_wpipe);
		else
			return;

		ifp->if_oerrors++;
	} else {
		usbd_get_xfer_status(xfer, NULL, NULL, &cc, NULL);
		DPRINTF(5, "wrote %d bytes (of %zd)\n", cc, hp->hp_wlen);

		if (cc != hp->hp_wlen)
			DPRINTF(0, "cc=%u, wlen=%zd\n", cc, hp->hp_wlen);

		ifp->if_opackets++;
	}

	s = splnet();
	CLR(ifp->if_flags, IFF_OACTIVE);
	ifp->if_start(ifp);
	splx(s);
}

Static void
uhso_ifnet_read_cb(usbd_xfer_handle xfer, usbd_private_handle p,
    usbd_status status)
{
	struct uhso_port *hp = p;
	struct uhso_softc *sc= hp->hp_sc;
	struct ifnet *ifp = hp->hp_ifp;
	void *cp;
	uint32_t cc;

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeupold(sc->sc_dev);

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(0, "non-normal status: %s\n", usbd_errstr(status));

		if (status == USBD_STALLED && hp->hp_rpipe != NULL)
			usbd_clear_endpoint_stall_async(hp->hp_rpipe);
		else
			return;

		ifp->if_ierrors++;
		hp->hp_rlen = 0;
	} else {
		usbd_get_xfer_status(xfer, NULL, (void **)&cp, &cc, NULL);

		hp->hp_rlen = cc;
		DPRINTF(5, "read %d bytes\n", cc);

		uhso_ifnet_input(ifp, &hp->hp_mbuf, cp, cc);
	}

	(*hp->hp_read)(hp);
}

Static void
uhso_ifnet_input(struct ifnet *ifp, struct mbuf **mb, uint8_t *cp, size_t cc)
{
	struct mbuf *m;
	size_t got, len, want;
	int s;

	/*
	 * Several IP packets might be in the same buffer, we need to
	 * separate them before handing it to the ip-stack.  We might
	 * also receive partial packets which we need to defer until
	 * we get more data.
	 */
	while (cc > 0) {
		if (*mb == NULL) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				aprint_error_ifnet(ifp, "no mbufs\n");
				ifp->if_ierrors++;
				break;
			}

			MCLGET(m, M_DONTWAIT);
			if (!ISSET(m->m_flags, M_EXT)) {
				aprint_error_ifnet(ifp, "no mbuf clusters\n");
				ifp->if_ierrors++;
				m_freem(m);
				break;
			}

			got = 0;
		} else {
			m = *mb;
			*mb = NULL;
			got = m->m_pkthdr.len;
		}

		/* make sure that the incoming packet is ok */
		if (got == 0)
			mtod(m, uint8_t *)[0] = cp[0];

		want = mtod(m, struct ip *)->ip_hl << 2;
		if (mtod(m, struct ip *)->ip_v != 4
		    || want != sizeof(struct ip)) {
			aprint_error_ifnet(ifp, "bad IP header (v=%d, hl=%zd)\n",
			    mtod(m, struct ip *)->ip_v, want);

			ifp->if_ierrors++;
			m_freem(m);
			break;
		}

		/* ensure we have the IP header.. */
		if (got < want) {
			len = MIN(want - got, cc);
			memcpy(mtod(m, uint8_t *) + got, cp, len);
			got += len;
			cc -= len;
			cp += len;

			if (got < want) {
				DPRINTF(5, "waiting for IP header "
					   "(got %zd want %zd)\n", got, want);

				m->m_pkthdr.len = got;
				*mb = m;
				break;
			}
		}

		/* ..and the packet body */
		want = ntohs(mtod(m, struct ip *)->ip_len);
		if (got < want) {
			len = MIN(want - got, cc);
			memcpy(mtod(m, uint8_t *) + got, cp, len);
			got += len;
			cc -= len;
			cp += len;

			if (got < want) {
				DPRINTF(5, "waiting for IP packet "
					   "(got %zd want %zd)\n", got, want);

				m->m_pkthdr.len = got;
				*mb = m;
				break;
			}
		} else if (want > got) {
			aprint_error_ifnet(ifp, "bad IP packet (len=%zd)\n",
			    want);

			ifp->if_ierrors++;
			m_freem(m);
			break;
		}

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = got;

		s = splnet();

		bpf_mtap(ifp, m);

		if (__predict_false(!pktq_enqueue(ip_pktq, m, 0))) {
			m_freem(m);
		} else {
			ifp->if_ipackets++;
			ifp->if_ibytes += got;
		}
		splx(s);
	}
}

Static int
uhso_ifnet_ioctl(struct ifnet *ifp, u_long cmd, void *data)
{
	struct uhso_port *hp = ifp->if_softc;
	int error, s;

	s = splnet();

	switch (cmd) {
	case SIOCINITIFADDR:
		switch (((struct ifaddr *)data)->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			if (!ISSET(ifp->if_flags, IFF_RUNNING)) {
				SET(ifp->if_flags, IFF_UP);
				error = uhso_ifnet_init(hp);
				if (error != 0) {
					uhso_ifnet_clean(hp);
					break;
				}

				SET(ifp->if_flags, IFF_RUNNING);
				DPRINTF(1, "hp=%p, ifp=%p INITIFADDR\n", hp, ifp);
				break;
			}

			error = 0;
			break;
#endif

		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCSIFMTU:
		if (((struct ifreq *)data)->ifr_mtu > hp->hp_wsize) {
			error = EINVAL;
			break;
		}

		error = ifioctl_common(ifp, cmd, data);
		if (error == ENETRESET)
			error = 0;

		break;

	case SIOCSIFFLAGS:
		error = ifioctl_common(ifp, cmd, data);
		if (error != 0)
			break;

		switch (ifp->if_flags & (IFF_UP | IFF_RUNNING)) {
		case IFF_UP:
			error = uhso_ifnet_init(hp);
			if (error != 0) {
				uhso_ifnet_clean(hp);
				break;
			}

			SET(ifp->if_flags, IFF_RUNNING);
			DPRINTF(1, "hp=%p, ifp=%p RUNNING\n", hp, ifp);
			break;

		case IFF_RUNNING:
			uhso_ifnet_clean(hp);
			CLR(ifp->if_flags, IFF_RUNNING);
			DPRINTF(1, "hp=%p, ifp=%p STOPPED\n", hp, ifp);
			break;

		default:
			break;
		}
		break;

	default:
		error = ifioctl_common(ifp, cmd, data);
		break;
	}

	splx(s);

	return error;
}

/* is only called if IFF_RUNNING not set */
Static int
uhso_ifnet_init(struct uhso_port *hp)
{
	struct uhso_softc *sc = hp->hp_sc;
	int error;

	DPRINTF(1, "sc=%p, hp=%p\n", sc, hp);

	if (!device_is_active(sc->sc_dev))
		return EIO;

	error = (*hp->hp_init)(hp);
	if (error != 0)
		return error;

	hp->hp_rxfer = usbd_alloc_xfer(sc->sc_udev);
	if (hp->hp_rxfer == NULL)
		return ENOMEM;

	hp->hp_rbuf = usbd_alloc_buffer(hp->hp_rxfer, hp->hp_rsize);
	if (hp->hp_rbuf == NULL)
		return ENOMEM;

	hp->hp_wxfer = usbd_alloc_xfer(sc->sc_udev);
	if (hp->hp_wxfer == NULL)
		return ENOMEM;

	hp->hp_wbuf = usbd_alloc_buffer(hp->hp_wxfer, hp->hp_wsize);
	if (hp->hp_wbuf == NULL)
		return ENOMEM;

	error = (*hp->hp_read)(hp);
	if (error != 0)
		return error;

	return 0;
}

Static void
uhso_ifnet_clean(struct uhso_port *hp)
{

	DPRINTF(1, "hp=%p\n", hp);

	(*hp->hp_clean)(hp);

	if (hp->hp_rxfer != NULL) {
		usbd_free_xfer(hp->hp_rxfer);
		hp->hp_rxfer = NULL;
		hp->hp_rbuf = NULL;
	}

	if (hp->hp_wxfer != NULL) {
		usbd_free_xfer(hp->hp_wxfer);
		hp->hp_wxfer = NULL;
		hp->hp_wbuf = NULL;
	}
}

/* called at splnet() with IFF_OACTIVE not set */
Static void
uhso_ifnet_start(struct ifnet *ifp)
{
	struct uhso_port *hp = ifp->if_softc;
	struct mbuf *m;

	KASSERT(!ISSET(ifp->if_flags, IFF_OACTIVE));

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return;

	if (IFQ_IS_EMPTY(&ifp->if_snd)) {
		DPRINTF(5, "finished sending\n");
		return;
	}

	SET(ifp->if_flags, IFF_OACTIVE);
	IFQ_DEQUEUE(&ifp->if_snd, m);
	hp->hp_wlen = m->m_pkthdr.len;
	if (hp->hp_wlen > hp->hp_wsize) {
		aprint_error_ifnet(ifp,
		    "packet too long (%zd > %zd), truncating\n",
		    hp->hp_wlen, hp->hp_wsize);

		hp->hp_wlen = hp->hp_wsize;
	}

	bpf_mtap(ifp, m);

	m_copydata(m, 0, hp->hp_wlen, hp->hp_wbuf);
	m_freem(m);

	if ((*hp->hp_write)(hp) != 0) {
		ifp->if_oerrors++;
		CLR(ifp->if_flags, IFF_OACTIVE);
	}
}

Static int
uhso_ifnet_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
    struct rtentry *rt0)
{
	ALTQ_DECL(struct altq_pktattr pktattr);
	int error;

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return EIO;

	IFQ_CLASSIFY(&ifp->if_snd, m, dst->sa_family, &pktattr);

	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
		error = ifq_enqueue(ifp, m ALTQ_COMMA ALTQ_DECL(&pktattr));
		break;
#endif

	default:
		DPRINTF(0, "unsupported address family %d\n", dst->sa_family);
		error = EAFNOSUPPORT;
		m_freem(m);
		break;
	}

	return error;
}
