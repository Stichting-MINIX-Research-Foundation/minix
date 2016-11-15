/* $NetBSD: smbus_acpi.c,v 1.13 2010/07/29 11:03:09 jruoho Exp $ */

/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Goyette
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

/*
 * ACPI SMBus Controller driver
 *
 * See http://smbus.org/specs/smbus_cmi10.pdf for specifications
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: smbus_acpi.c,v 1.13 2010/07/29 11:03:09 jruoho Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/callout.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include <dev/i2c/i2cvar.h>

#define _COMPONENT		ACPI_BUS_COMPONENT
ACPI_MODULE_NAME		("smbus_acpi")

/*
 * ACPI SMBus CMI protocol codes.
 */
#define	ACPI_SMBUS_RD_QUICK	0x03
#define	ACPI_SMBUS_RCV_BYTE	0x05
#define	ACPI_SMBUS_RD_BYTE	0x07
#define	ACPI_SMBUS_RD_WORD	0x09
#define	ACPI_SMBUS_RD_BLOCK	0x0B
#define	ACPI_SMBUS_WR_QUICK	0x02
#define	ACPI_SMBUS_SND_BYTE	0x04
#define	ACPI_SMBUS_WR_BYTE	0x06
#define	ACPI_SMBUS_WR_WORD	0x08
#define	ACPI_SMBUS_WR_BLOCK	0x0A
#define	ACPI_SMBUS_PROCESS_CALL	0x0C

struct acpi_smbus_softc {
	struct acpi_devnode 	*sc_devnode;
	struct callout		sc_callout;
	struct i2c_controller	sc_i2c_tag;
	device_t		sc_dv;
	kmutex_t		sc_i2c_mutex;
	int			sc_poll_alert;
};

static int	acpi_smbus_match(device_t, cfdata_t, void *);
static void	acpi_smbus_attach(device_t, device_t, void *);
static int	acpi_smbus_detach(device_t, int);
static int	acpi_smbus_poll_alert(ACPI_HANDLE, int *);
static int	acpi_smbus_acquire_bus(void *, int);
static void	acpi_smbus_release_bus(void *, int);
static int	acpi_smbus_exec(void *, i2c_op_t, i2c_addr_t, const void *,
				size_t, void *, size_t, int);
static void	acpi_smbus_alerts(struct acpi_smbus_softc *);
static void	acpi_smbus_tick(void *);
static void	acpi_smbus_notify_handler(ACPI_HANDLE, uint32_t, void *);

struct SMB_UDID {
	uint8_t		dev_cap;
	uint8_t		vers_rev;
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	interface;
	uint16_t	subsys_vendor;
	uint16_t	subsys_device;
	uint8_t		reserved[4];
};

struct SMB_DEVICE {
	uint8_t		slave_addr;
	uint8_t		reserved;
	struct SMB_UDID	dev_id;
};

struct SMB_INFO {
	uint8_t		struct_ver;
	uint8_t		spec_ver;
	uint8_t		hw_cap;
	uint8_t		poll_int;
	uint8_t		dev_count;
	struct SMB_DEVICE device[1];
};

static const char * const smbus_acpi_ids[] = {
	"SMBUS01",	/* SMBus CMI v1.0 */
	NULL
};

CFATTACH_DECL_NEW(acpismbus, sizeof(struct acpi_smbus_softc),
    acpi_smbus_match, acpi_smbus_attach, acpi_smbus_detach, NULL);

static int
acpi_smbus_match(device_t parent, cfdata_t match, void *aux)
{
	struct acpi_attach_args *aa = aux;

	if (aa->aa_node->ad_type != ACPI_TYPE_DEVICE)
		return 0;

	if (acpi_match_hid(aa->aa_node->ad_devinfo, smbus_acpi_ids) == 0)
		return 0;

	return acpi_smbus_poll_alert(aa->aa_node->ad_handle, NULL);
}

static void
acpi_smbus_attach(device_t parent, device_t self, void *aux)
{
	struct acpi_smbus_softc *sc = device_private(self);
	struct acpi_attach_args *aa = aux;
	struct i2cbus_attach_args iba;

	aprint_naive("\n");

	sc->sc_devnode = aa->aa_node;
	sc->sc_dv = self;
	sc->sc_poll_alert = 2;

	/* Attach I2C bus. */
	mutex_init(&sc->sc_i2c_mutex, MUTEX_DEFAULT, IPL_NONE);

	sc->sc_i2c_tag.ic_cookie = sc;
	sc->sc_i2c_tag.ic_acquire_bus = acpi_smbus_acquire_bus;
	sc->sc_i2c_tag.ic_release_bus = acpi_smbus_release_bus;
	sc->sc_i2c_tag.ic_exec = acpi_smbus_exec;

	(void)acpi_smbus_poll_alert(aa->aa_node->ad_handle,&sc->sc_poll_alert);

	/* If failed, fall-back to polling. */
	if (acpi_register_notify(sc->sc_devnode,
		acpi_smbus_notify_handler) != true)
		sc->sc_poll_alert = 2;

	callout_init(&sc->sc_callout, 0);
	callout_setfunc(&sc->sc_callout, acpi_smbus_tick, self);

	if (sc->sc_poll_alert != 0) {
		aprint_debug(": alert_poll %d sec", sc->sc_poll_alert);
		callout_schedule(&sc->sc_callout, sc->sc_poll_alert * hz);
	}

	aprint_normal("\n");

	(void)memset(&iba, 0, sizeof(iba));
	(void)pmf_device_register(self, NULL, NULL);

	iba.iba_tag = &sc->sc_i2c_tag;

	(void)config_found_ia(self, "i2cbus", &iba, iicbus_print);
}

static int
acpi_smbus_detach(device_t self, int flags)
{
	struct acpi_smbus_softc *sc = device_private(self);

	pmf_device_deregister(self);
	acpi_deregister_notify(sc->sc_devnode);

	callout_halt(&sc->sc_callout, NULL);
	callout_destroy(&sc->sc_callout);

	mutex_destroy(&sc->sc_i2c_mutex);

	return 0;
}

static int
acpi_smbus_poll_alert(ACPI_HANDLE hdl, int *alert)
{
	struct SMB_INFO *info;
	ACPI_BUFFER smi_buf;
	ACPI_OBJECT *e, *p;
	ACPI_STATUS rv;

	/*
	 * Retrieve polling interval for SMBus Alerts.
	 */
	rv = acpi_eval_struct(hdl, "_SBI", &smi_buf);

	if (ACPI_FAILURE(rv))
		return 0;

	p = smi_buf.Pointer;

	if (p->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	if (p->Package.Count == 0) {
		rv = AE_LIMIT;
		goto out;
	}

	e = p->Package.Elements;

	if (e[0].Type != ACPI_TYPE_INTEGER) {
		rv = AE_TYPE;
		goto out;
	}

	/* Verify CMI version. */
	if (e[0].Integer.Value != 0x10) {
		rv = AE_SUPPORT;
		goto out;
	}

	if (alert != NULL) {

		if (p->Package.Count < 2)
			goto out;

		if (e[1].Type != ACPI_TYPE_BUFFER)
			goto out;

		info = (struct SMB_INFO *)(e[1].Buffer.Pointer);
		*alert = info->poll_int;
	}

out:
	if (smi_buf.Pointer != NULL)
		ACPI_FREE(smi_buf.Pointer);

	return (ACPI_FAILURE(rv)) ? 0 : 1;
}

static int
acpi_smbus_acquire_bus(void *cookie, int flags)
{
        struct acpi_smbus_softc *sc = cookie;

        mutex_enter(&sc->sc_i2c_mutex);

        return 0;
}

static void
acpi_smbus_release_bus(void *cookie, int flags)
{
        struct acpi_smbus_softc *sc = cookie;

        mutex_exit(&sc->sc_i2c_mutex);
}
static int
acpi_smbus_exec(void *cookie, i2c_op_t op, i2c_addr_t addr,
	const void *cmdbuf, size_t cmdlen, void *buf, size_t len, int flags)
{
        struct acpi_smbus_softc *sc = cookie;
	const uint8_t *c = cmdbuf;
	uint8_t *b = buf, *xb;
	const char *path;
	ACPI_OBJECT_LIST args;
	ACPI_OBJECT arg[5];
	ACPI_OBJECT *p, *e;
	ACPI_BUFFER smbuf;
	ACPI_STATUS rv;
	int i, r, xlen;

	/*
	 *	arg[0] : protocol
	 *	arg[1] : slave address
	 *	arg[2] : command
	 *	arg[3] : data length
	 *	arg[4] : data
	 */
	for (i = r = 0; i < __arraycount(arg); i++)
		arg[i].Type = ACPI_TYPE_INTEGER;

	args.Pointer = arg;

	smbuf.Pointer = NULL;
	smbuf.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

	arg[1].Integer.Value = addr;

	if (I2C_OP_READ_P(op)) {

		path = "_SBR";
		args.Count = 3;

		switch (len) {

		case 0:
			arg[0].Integer.Value = (cmdlen != 0) ?
			    ACPI_SMBUS_RCV_BYTE : ACPI_SMBUS_RD_QUICK;

			arg[2].Integer.Value = 0;
			break;

		case 1:
			arg[0].Integer.Value = ACPI_SMBUS_RD_BYTE;
			arg[2].Integer.Value = *c;
			break;

		case 2:
			arg[0].Integer.Value = ACPI_SMBUS_RD_WORD;
			arg[2].Integer.Value = *c;
			break;

		default:
			arg[0].Integer.Value = ACPI_SMBUS_RD_BLOCK;
			arg[2].Integer.Value = *c;
			break;
		}

	} else {

		path = "_SBW";
		args.Count = 5;

		arg[3].Integer.Value = len;

		switch (len) {

		case 0:
			if (cmdlen == 0) {
				arg[2].Integer.Value = 0;
				arg[0].Integer.Value = ACPI_SMBUS_WR_QUICK;
			} else {
				arg[2].Integer.Value = *c;
				arg[0].Integer.Value = ACPI_SMBUS_SND_BYTE;
			}

			arg[4].Integer.Value = 0;
			break;

		case 1:
			arg[0].Integer.Value = ACPI_SMBUS_WR_BYTE;
			arg[2].Integer.Value = *c;
			arg[4].Integer.Value = *b;
			break;

		case 2:
			arg[0].Integer.Value = ACPI_SMBUS_WR_WORD;
			arg[2].Integer.Value = *c;
			arg[4].Integer.Value = *b++;
			arg[4].Integer.Value += (*b--) << 8;
			break;

		default:
			arg[0].Integer.Value = ACPI_SMBUS_WR_BLOCK;
			arg[2].Integer.Value = *c;
			arg[4].Type = ACPI_TYPE_BUFFER;
			arg[4].Buffer.Pointer = buf;
			arg[4].Buffer.Length = (len < 32) ? len : 32;
			break;
		}
	}

	rv = AcpiEvaluateObject(sc->sc_devnode->ad_handle, path, &args,&smbuf);

	if (ACPI_FAILURE(rv))
		goto out;

	p = smbuf.Pointer;

	if (p->Type != ACPI_TYPE_PACKAGE) {
		rv = AE_TYPE;
		goto out;
	}

	if (p->Package.Count < 1) {
		rv = AE_LIMIT;
		goto out;
	}

	e = p->Package.Elements;

	if (e->Type != ACPI_TYPE_INTEGER) {
		rv = AE_TYPE;
		goto out;
	}

	ACPI_DEBUG_PRINT((ACPI_DB_DEBUG_OBJECT,
		"return status: %"PRIu64"\n", e[0].Integer.Value));

	if (e[0].Integer.Value != 0) {
		rv = AE_BAD_VALUE;
		goto out;
	}

	/*
	 * For read operations, copy data to user buffer.
	 */
	if (I2C_OP_READ_P(op)) {

		if (p->Package.Count < 3) {
			rv = AE_LIMIT;
			goto out;
		}

		if (e[1].Type != ACPI_TYPE_INTEGER) {
			rv = AE_TYPE;
			goto out;
		}

		xlen = e[1].Integer.Value;

		if (xlen > len)
			xlen = len;

		switch (e[2].Type) {

		case ACPI_TYPE_BUFFER:

			if (xlen == 0) {
				rv = AE_LIMIT;
				goto out;
			}

			xb = e[2].Buffer.Pointer;

			if (xb == NULL) {
				rv = AE_NULL_OBJECT;
				goto out;
			}

			(void)memcpy(b, xb, xlen);
			break;

		case ACPI_TYPE_INTEGER:

			if (xlen > 0)
				*b++ = e[2].Integer.Value & 0xff;

			if (xlen > 1)
				*b = e[2].Integer.Value >> 8;

			break;

		default:
			rv = AE_TYPE;
			goto out;
		}
	}

out:
	if (smbuf.Pointer != NULL)
		ACPI_FREE(smbuf.Pointer);

	if (ACPI_SUCCESS(rv))
		return 0;

	ACPI_DEBUG_PRINT((ACPI_DB_DEBUG_OBJECT, "failed to "
		"evaluate %s: %s\n", path, AcpiFormatException(rv)));

	return 1;
}

/*
 * Whether triggered by periodic polling or a Notify(),
 * retrieve all pending SMBus device alerts.
 */
static void
acpi_smbus_alerts(struct acpi_smbus_softc *sc)
{
	const ACPI_HANDLE hdl = sc->sc_devnode->ad_handle;
	ACPI_OBJECT *e, *p;
	ACPI_BUFFER alert;
	ACPI_STATUS rv;
	int status = 0;
	uint8_t addr;

	do {
		rv = acpi_eval_struct(hdl, "_SBA", &alert);

		if (ACPI_FAILURE(rv)) {
			status = 1;
			goto done;
		}

		p = alert.Pointer;

		if (p->Type == ACPI_TYPE_PACKAGE && p->Package.Count >= 2) {

			status = 1;

			e = p->Package.Elements;

			if (e[0].Type == ACPI_TYPE_INTEGER)
				status = e[0].Integer.Value;

			if (status == 0 && e[1].Type == ACPI_TYPE_INTEGER) {
				addr = e[1].Integer.Value;

				aprint_debug_dev(sc->sc_dv,
				    "alert for 0x%x\n", addr);

				(void)iic_smbus_intr(&sc->sc_i2c_tag);
			}
		}
done:
		if (alert.Pointer != NULL)
			ACPI_FREE(alert.Pointer);

	} while (status == 0);
}

static void
acpi_smbus_tick(void *opaque)
{
	device_t dv = opaque;
	struct acpi_smbus_softc *sc = device_private(dv);

	acpi_smbus_alerts(sc);

	callout_schedule(&sc->sc_callout, sc->sc_poll_alert * hz);
}

static void
acpi_smbus_notify_handler(ACPI_HANDLE hdl, uint32_t notify, void *opaque)
{
	device_t dv = opaque;
	struct acpi_smbus_softc *sc = device_private(dv);

	aprint_debug_dev(dv, "received notify message 0x%x\n", notify);

	acpi_smbus_alerts(sc);
}
