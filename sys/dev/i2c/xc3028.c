/* $NetBSD: xc3028.c,v 1.7 2015/03/07 14:16:51 jmcneill Exp $ */

/*-
 * Copyright (c) 2011 Jared D. McNeill <jmcneill@invisible.ca>
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

/*
 * Xceive XC3028L
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: xc3028.c,v 1.7 2015/03/07 14:16:51 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/module.h>

#include <dev/firmload.h>
#include <dev/i2c/i2cvar.h>

#include <dev/i2c/xc3028reg.h>
#include <dev/i2c/xc3028var.h>

#define	XC3028_FIRMWARE_DRVNAME	"xc3028"

#define	XC3028_FREQ_MIN		1000000
#define	XC3028_FREQ_MAX		1023000000

#define	XC3028_FW_BASE		(1 << 0)
#define	XC3028_FW_D2633		(1 << 4)
#define	XC3028_FW_DTV6		(1 << 5)
#define	XC3028_FW_QAM		(1 << 6)
#define	XC3028_FW_ATSC		(1 << 16)
#define	XC3028_FW_LG60		(1 << 18)
#define	XC3028_FW_F6MHZ		(1 << 27)
#define	XC3028_FW_SCODE		(1 << 29)
#define	XC3028_FW_HAS_IF	(1 << 30)

#define	XC3028_FW_DEFAULT	(XC3028_FW_ATSC|XC3028_FW_D2633|XC3028_FW_DTV6)

static kmutex_t	xc3028_firmware_lock;

static int	xc3028_reset(struct xc3028 *);
static int	xc3028_read_2(struct xc3028 *, uint16_t, uint16_t *);
static int	xc3028_write_buffer(struct xc3028 *, const uint8_t *, size_t);
static int	xc3028_firmware_open(struct xc3028 *);
static int	xc3028_firmware_parse(struct xc3028 *, const uint8_t *, size_t);
static int	xc3028_firmware_upload(struct xc3028 *, struct xc3028_fw *);
static int	xc3028_scode_upload(struct xc3028 *, struct xc3028_fw *);
static void	xc3028_dump_fw(struct xc3028 *, struct xc3028_fw *,
		    const char *);

static const char *
xc3028_name(struct xc3028 *xc)
{
	if (xc->type == XC3028L)
		return "xc3028l";
	else
		return "xc3028";
}

static const char *
xc3028_firmware_name(struct xc3028 *xc)
{
	if (xc->type == XC3028L)
		return "xc3028L-v36.fw";
	else
		return "xc3028-v27.fw";
}

static int
xc3028_reset(struct xc3028 *xc)
{
	int error = 0;

	if (xc->reset)
		error = xc->reset(xc->reset_priv);

	return error;
}

static struct xc3028_fw *
xc3028_get_basefw(struct xc3028 *xc)
{
	struct xc3028_fw *fw;
	unsigned int i;

	for (i = 0; i < xc->nfw; i++) {
		fw = &xc->fw[i];
		if (fw->type == XC3028_FW_BASE)
			return fw;
	}

	return NULL;
}

static struct xc3028_fw *
xc3028_get_stdfw(struct xc3028 *xc)
{
	struct xc3028_fw *fw;
	unsigned int i;

	for (i = 0; i < xc->nfw; i++) {
		fw = &xc->fw[i];
		if (fw->type == (XC3028_FW_D2633|XC3028_FW_DTV6|XC3028_FW_ATSC))
			return fw;
	}

	return NULL;
}

static struct xc3028_fw *
xc3028_get_scode(struct xc3028 *xc)
{
	struct xc3028_fw *fw;
	unsigned int i;

	for (i = 0; i < xc->nfw; i++) {
		fw = &xc->fw[i];
		if (fw->type ==
		    (XC3028_FW_DTV6|XC3028_FW_QAM|XC3028_FW_ATSC|XC3028_FW_LG60|
		     XC3028_FW_F6MHZ|XC3028_FW_SCODE|XC3028_FW_HAS_IF) &&
		    fw->int_freq == 6200)
			return fw;
	}

	return NULL;
}

static int
xc3028_firmware_open(struct xc3028 *xc)
{
	firmware_handle_t fwh;
	struct xc3028_fw *basefw, *stdfw, *scode;
	uint8_t *fw = NULL;
	uint16_t xcversion = 0;
	size_t fwlen;
	int error;

	mutex_enter(&xc3028_firmware_lock);

	error = firmware_open(XC3028_FIRMWARE_DRVNAME,
	    xc3028_firmware_name(xc), &fwh);
	if (error)
		goto done;
	fwlen = firmware_get_size(fwh);
	fw = firmware_malloc(fwlen);
	if (fw == NULL) {
		firmware_close(fwh);
		error = ENOMEM;
		goto done;
	}
	error = firmware_read(fwh, 0, fw, fwlen);
	firmware_close(fwh);
	if (error)
		goto done;

	device_printf(xc->parent, "%s: loading firmware '%s/%s'\n",
	    xc3028_name(xc), XC3028_FIRMWARE_DRVNAME, xc3028_firmware_name(xc));
	error = xc3028_firmware_parse(xc, fw, fwlen);
	if (!error) {
		basefw = xc3028_get_basefw(xc);
		stdfw = xc3028_get_stdfw(xc);
		scode = xc3028_get_scode(xc);
		if (basefw && stdfw) {
			xc3028_reset(xc);
			xc3028_dump_fw(xc, basefw, "base");
			error = xc3028_firmware_upload(xc, basefw);
			if (error)
				return error;
			xc3028_dump_fw(xc, stdfw, "std");
			error = xc3028_firmware_upload(xc, stdfw);
			if (error)
				return error;
			if (scode) {
				xc3028_dump_fw(xc, scode, "scode");
				error = xc3028_scode_upload(xc, scode);
				if (error)
					return error;
			}
		} else
			error = ENODEV;
	}
	if (!error) {
		xc3028_read_2(xc, XC3028_REG_VERSION, &xcversion);

		device_printf(xc->parent, "%s: hw %d.%d, fw %d.%d\n",
		    xc3028_name(xc),
		    (xcversion >> 12) & 0xf, (xcversion >> 8) & 0xf,
		    (xcversion >> 4) & 0xf, (xcversion >> 0) & 0xf);
	}

done:
	if (fw)
		firmware_free(fw, fwlen);
	mutex_exit(&xc3028_firmware_lock);

	if (error)
		aprint_error_dev(xc->parent,
		    "%s: couldn't open firmware '%s/%s' (error=%d)\n",
		    xc3028_name(xc), XC3028_FIRMWARE_DRVNAME,
		    xc3028_firmware_name(xc), error);

	return error;
}

static const char *xc3028_fw_types[] = {
	"BASE",
	"F8MHZ",
	"MTS",
	"D2620",
	"D2633",
	"DTV6",
	"QAM",
	"DTV7",
	"DTV78",
	"DTV8",
	"FM",
	"INPUT1",
	"LCD",
	"NOGD",
	"INIT1",
	"MONO",
	"ATSC",
	"IF",
	"LG60",
	"ATI638",
	"OREN538",
	"OREN36",
	"TOYOTA388",
	"TOYOTA794",
	"DIBCOM52",
	"ZARLINK456",
	"CHINA",
	"F6MHZ",
	"INPUT2",
	"SCODE",
	"HAS_IF",
};

static void
xc3028_dump_fw(struct xc3028 *xc, struct xc3028_fw *xcfw, const char *type)
{
	unsigned int i;

	device_printf(xc->parent, "%s: %s:", xc3028_name(xc), type);
	if (xcfw == NULL) {
		printf(" <none>\n");
		return;
	}
	for (i = 0; i < __arraycount(xc3028_fw_types); i++) {
		if (xcfw->type & (1 << i))
			printf(" %s", xc3028_fw_types[i]);
	}
	if (xcfw->type & (1 << 30))
		printf("_%d", xcfw->int_freq);
	if (xcfw->id)
		printf(" id=%" PRIx64, xcfw->id);
	printf(" size=%u\n", xcfw->data_size);
}

static int
xc3028_firmware_parse(struct xc3028 *xc, const uint8_t *fw, size_t fwlen)
{
	const uint8_t *p = fw, *endp = p + fwlen;
	char fwname[32 + 1];
	uint16_t fwver, narr;
	unsigned int index;
	struct xc3028_fw *xcfw;

	if (fwlen < 36)
		return EINVAL;

	/* first 32 bytes are the firmware name string */
	memset(fwname, 0, sizeof(fwname));
	memcpy(fwname, p, sizeof(fwname) - 1);
	p += (sizeof(fwname) - 1);

	fwver = le16dec(p);
	p += sizeof(fwver);
	narr = le16dec(p);
	p += sizeof(narr);

	aprint_debug_dev(xc->parent, "%s: fw type %s, ver %d.%d, %d images\n",
	    xc3028_name(xc), fwname, fwver >> 8, fwver & 0xff, narr);

	xc->fw = kmem_zalloc(sizeof(*xc->fw) * narr, KM_SLEEP);
	if (xc->fw == NULL)
		return ENOMEM;
	xc->nfw = narr;

	for (index = 0; index < xc->nfw && p < endp; index++) {
		xcfw = &xc->fw[index];

		if (endp - p < 16)
			goto corrupt;

		xcfw->type = le32dec(p);
		p += sizeof(xcfw->type);

		xcfw->id = le64dec(p);
		p += sizeof(xcfw->id);

		if (xcfw->type & XC3028_FW_HAS_IF) {
			xcfw->int_freq = le16dec(p);
			p += sizeof(xcfw->int_freq);
			if ((uint32_t)(endp - p) < sizeof(xcfw->data_size))
				goto corrupt;
		}

		xcfw->data_size = le32dec(p);
		p += sizeof(xcfw->data_size);

		if (xcfw->data_size == 0 ||
		    xcfw->data_size > (uint32_t)(endp - p))
			goto corrupt;
		xcfw->data = kmem_alloc(xcfw->data_size, KM_SLEEP);
		if (xcfw->data == NULL)
			goto corrupt;
		memcpy(xcfw->data, p, xcfw->data_size);
		p += xcfw->data_size;
	}

	return 0;

corrupt:
	aprint_error_dev(xc->parent, "%s: fw image corrupt\n", xc3028_name(xc));
	for (index = 0; index < xc->nfw; index++) {
		if (xc->fw[index].data)
			kmem_free(xc->fw[index].data, xc->fw[index].data_size);
	}
	kmem_free(xc->fw, sizeof(*xc->fw) * xc->nfw);
	xc->nfw = 0;

	return ENXIO;
}

static int
xc3028_firmware_upload(struct xc3028 *xc, struct xc3028_fw *xcfw)
{
	const uint8_t *fw = xcfw->data, *p;
	uint32_t fwlen = xcfw->data_size;
	uint8_t cmd[64];
	unsigned int i;
	uint16_t len, rem;
	size_t wrlen;
	int error;

	for (i = 0; i < fwlen - 2;) {
		len = le16dec(&fw[i]);
		i += 2;
		if (len == 0xffff)
			break;

		/* reset command */
		if (len == 0x0000) {
			error = xc3028_reset(xc);
			if (error)
				return error;
			continue;
		}
		/* reset clk command */
		if (len == 0xff00) {
			continue;
		}
		/* delay command */
		if (len & 0x8000) {
			delay((len & 0x7fff) * 1000);
			continue;
		}

		if (i + len > fwlen) {
			printf("weird len, i=%u len=%u fwlen=%u'\n", i, len, fwlen);
			return EINVAL;
		}

		cmd[0] = fw[i];
		p = &fw[i + 1];
		rem = len - 1;
		while (rem > 0) {
			wrlen = min(rem, __arraycount(cmd) - 1);
			memcpy(&cmd[1], p, wrlen);
			error = xc3028_write_buffer(xc, cmd, wrlen + 1);
			if (error)
				return error;
			p += wrlen;
			rem -= wrlen;
		}
		i += len;
	}

	return 0;
}

static int
xc3028_scode_upload(struct xc3028 *xc, struct xc3028_fw *xcfw)
{
	static uint8_t scode_init[] = {	0xa0, 0x00, 0x00, 0x00 };
	static uint8_t scode_fini[] = { 0x00, 0x8c };
	int error;

	if (xcfw->data_size < 12)
		return EINVAL;
	error = xc3028_write_buffer(xc, scode_init, sizeof(scode_init));
	if (error)
		return error;
	error = xc3028_write_buffer(xc, xcfw->data, 12);
	if (error)
		return error;
	error = xc3028_write_buffer(xc, scode_fini, sizeof(scode_fini));
	if (error)
		return error;

	return 0;
}

static int
xc3028_read_2(struct xc3028 *xc, uint16_t reg, uint16_t *val)
{
	uint8_t cmd[2], resp[2];
	int error;

	cmd[0] = reg >> 8;
	cmd[1] = reg & 0xff;
	error = iic_exec(xc->i2c, I2C_OP_WRITE, xc->i2c_addr,
	    cmd, sizeof(cmd), NULL, 0, 0);
	if (error)
		return error;
	resp[0] = resp[1] = 0;
	error = iic_exec(xc->i2c, I2C_OP_READ, xc->i2c_addr,
	    NULL, 0, resp, sizeof(resp), 0);
	if (error)
		return error;

	*val = (resp[0] << 8) | resp[1];

	return 0;
}

static int
xc3028_write_buffer(struct xc3028 *xc, const uint8_t *data, size_t datalen)
{
	return iic_exec(xc->i2c, I2C_OP_WRITE_WITH_STOP, xc->i2c_addr,
	    data, datalen, NULL, 0, 0);
}

#if notyet
static int
xc3028_write_2(struct xc3028 *xc, uint16_t reg, uint16_t val)
{
	uint8_t data[4];

	data[0] = reg >> 8;
	data[1] = reg & 0xff;
	data[2] = val >> 8;
	data[3] = val & 0xff;

	return xc3028_write_buffer(xc, data, sizeof(data));
}
#endif

struct xc3028 *
xc3028_open(device_t parent, i2c_tag_t i2c, i2c_addr_t addr,
    xc3028_reset_cb reset, void *reset_priv, enum xc3028_type type)
{
	struct xc3028 *xc;

	xc = kmem_alloc(sizeof(*xc), KM_SLEEP);
	if (xc == NULL)
		return NULL;
	xc->parent = parent;
	xc->i2c = i2c;
	xc->i2c_addr = addr;
	xc->reset = reset;
	xc->reset_priv = reset_priv;
	xc->type = type;

	if (xc3028_firmware_open(xc)) {
		aprint_error_dev(parent, "%s: fw open failed\n",
		    xc3028_name(xc));
		goto failed;
	}

	return xc;

failed:
	kmem_free(xc, sizeof(*xc));
	return NULL;
}

void
xc3028_close(struct xc3028 *xc)
{
	unsigned int index;

	if (xc->fw) {
		for (index = 0; index < xc->nfw; index++) {
			if (xc->fw[index].data)
				kmem_free(xc->fw[index].data,
				    xc->fw[index].data_size);
		}
		kmem_free(xc->fw, sizeof(*xc->fw) * xc->nfw);
	}
	kmem_free(xc, sizeof(*xc));
}

int
xc3028_tune_dtv(struct xc3028 *xc, const struct dvb_frontend_parameters *params)
{
	static uint8_t freq_init[] = { 0x80, 0x02, 0x00, 0x00 };
	uint8_t freq_buf[4];
	uint32_t div, offset = 0;
	int error;

	if (params->u.vsb.modulation == VSB_8) {
		offset = 1750000;
	} else {
		return EINVAL;
	}

	div = (params->frequency - offset + 15625 / 2) / 15625;
	
	error = xc3028_write_buffer(xc, freq_init, sizeof(freq_init));
	if (error)
		return error;
	delay(10000);

	freq_buf[0] = (div >> 24) & 0xff;
	freq_buf[1] = (div >> 16) & 0xff;
	freq_buf[2] = (div >> 8) & 0xff;
	freq_buf[3] = (div >> 0) & 0xff;
	error = xc3028_write_buffer(xc, freq_buf, sizeof(freq_buf));
	if (error)
		return error;
	delay(100000);

	return 0;
}

MODULE(MODULE_CLASS_DRIVER, xc3028, "i2cexec");

static int
xc3028_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
		mutex_init(&xc3028_firmware_lock, MUTEX_DEFAULT, IPL_NONE);
		return 0;
	case MODULE_CMD_FINI:
		mutex_destroy(&xc3028_firmware_lock);
		return 0;
	default:
		return ENOTTY;
	}
}
