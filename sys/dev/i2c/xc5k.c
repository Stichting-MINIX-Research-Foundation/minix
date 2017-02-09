/* $NetBSD: xc5k.c,v 1.6 2015/03/07 14:16:51 jmcneill Exp $ */

/*-
 * Copyright (c) 2010 Jared D. McNeill <jmcneill@invisible.ca>
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
 * Xceive XC5000
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: xc5k.c,v 1.6 2015/03/07 14:16:51 jmcneill Exp $");

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

#include <dev/i2c/xc5kreg.h>
#include <dev/i2c/xc5kvar.h>

#define	XC5K_FIRMWARE_DRVNAME	"xc5k"
#define	XC5K_FIRMWARE_IMGNAME	"dvb-fe-xc5000-1.6.114.fw"

#define	XC5K_FREQ_MIN		1000000
#define	XC5K_FREQ_MAX		1023000000

static kmutex_t xc5k_firmware_lock;

static int	xc5k_reset(struct xc5k *);
static int	xc5k_read_2(struct xc5k *, uint16_t, uint16_t *);
static int	xc5k_write_buffer(struct xc5k *, const uint8_t *, size_t);
static int	xc5k_firmware_open(struct xc5k *);
static int	xc5k_firmware_upload(struct xc5k *, const uint8_t *, size_t);

static int
xc5k_reset(struct xc5k *xc)
{
	int error = 0;

	if (xc->reset)
		error = xc->reset(xc->reset_priv);

	return error;
}

static int
xc5k_firmware_upload(struct xc5k *xc, const uint8_t *fw, size_t fwlen)
{
	const uint8_t *p;
	uint8_t cmd[64];
	unsigned int i;
	uint16_t len, rem;
	size_t wrlen;
	int error;

	for (i = 0; i < fwlen - 1;) {
		len = (fw[i] << 8) | fw[i + 1];
		i += 2;
		if (len == 0xffff)
			break;

		/* reset command */
		if (len == 0x0000) {
			error = xc5k_reset(xc);
			if (error)
				return error;
			continue;
		}

		/* delay command */
		if (len & 0x8000) {
			delay((len & 0x7fff) * 1000);
			continue;
		}

		if (i + len >= fwlen)
			break;

		cmd[0] = fw[i];
		cmd[1] = fw[i + 1];
		p = &fw[i + 2];
		rem = len - 2;
		while (rem > 0) {
			wrlen = min(rem, __arraycount(cmd) - 2);
			memcpy(&cmd[2], p, wrlen);
			error = xc5k_write_buffer(xc, cmd, wrlen + 2);
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
xc5k_firmware_open(struct xc5k *xc)
{
	firmware_handle_t fwh;
	uint16_t product_id, xcversion, xcbuild;
	uint8_t *fw = NULL;
	size_t fwlen;
	int error;

	mutex_enter(&xc5k_firmware_lock);
	
	error = xc5k_read_2(xc, XC5K_REG_PRODUCT_ID, &product_id);
	if (error || product_id != XC5K_PRODUCT_ID_NOFW)
		goto done;

	error = firmware_open(XC5K_FIRMWARE_DRVNAME,
	    XC5K_FIRMWARE_IMGNAME, &fwh);
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

	aprint_normal_dev(xc->parent, "xc5k: loading firmware '%s/%s'\n",
	    XC5K_FIRMWARE_DRVNAME, XC5K_FIRMWARE_IMGNAME);
	error = xc5k_firmware_upload(xc, fw, fwlen);
	if (!error) {
		xc5k_read_2(xc, XC5K_REG_VERSION, &xcversion);
		xc5k_read_2(xc, XC5K_REG_BUILD, &xcbuild);
		if (!error)
			aprint_normal_dev(xc->parent,
			    "xc5k: hw %d.%d, fw %d.%d.%d\n",
			    (xcversion >> 12) & 0xf,
			    (xcversion >> 8) & 0xf,
			    (xcversion >> 4) & 0xf,
			    xcversion & 0xf,
			    xcbuild);
	}

done:
	if (fw)
		firmware_free(fw, fwlen);
	mutex_exit(&xc5k_firmware_lock);

	if (error)
		aprint_error_dev(xc->parent,
		    "xc5k: couldn't open firmware '%s/%s' (error=%d)\n",
		    XC5K_FIRMWARE_DRVNAME, XC5K_FIRMWARE_IMGNAME, error);

	return error;
}

static int
xc5k_read_2(struct xc5k *xc, uint16_t reg, uint16_t *val)
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
xc5k_write_buffer(struct xc5k *xc, const uint8_t *data, size_t datalen)
{
	return iic_exec(xc->i2c, I2C_OP_WRITE, xc->i2c_addr,
	    data, datalen, NULL, 0, 0);
}

static int
xc5k_write_2(struct xc5k *xc, uint16_t reg, uint16_t val)
{
	uint8_t data[4];
	uint16_t busy;
	int error, retry;

	data[0] = reg >> 8;
	data[1] = reg & 0xff;
	data[2] = val >> 8;
	data[3] = val & 0xff;
	error = xc5k_write_buffer(xc, data, sizeof(data));
	if (error)
		return error;

	retry = 1000;
	while (--retry > 0) {
		error = xc5k_read_2(xc, XC5K_REG_BUSY, &busy);
		if (error || !busy)
			break;
		delay(5000);
	}

	return error;
}

struct xc5k *
xc5k_open(device_t parent, i2c_tag_t i2c, i2c_addr_t addr,
    xc5k_reset_cb reset, void *reset_priv, unsigned int if_freq,
    fe_type_t fe_type)
{
	struct xc5k *xc;
	uint16_t product_id;

	xc = kmem_alloc(sizeof(*xc), KM_SLEEP);
	if (xc == NULL)
		return NULL;
	xc->parent = parent;
	xc->i2c = i2c;
	xc->i2c_addr = addr;
	xc->reset = reset;
	xc->reset_priv = reset_priv;
	xc->if_freq = if_freq;
	xc->fe_type = fe_type;

	if (xc5k_read_2(xc, XC5K_REG_PRODUCT_ID, &product_id))
		goto failed;

	aprint_debug_dev(parent, "xc5k: product=0x%04x\n", product_id);

	if (product_id != XC5K_PRODUCT_ID_NOFW && product_id != XC5K_PRODUCT_ID)
		goto failed;

	if (xc5k_firmware_open(xc))
		goto failed;
	if (xc5k_write_2(xc, XC5K_REG_INIT, 0))
		goto failed;
	delay(100000);

	if (xc5k_read_2(xc, XC5K_REG_PRODUCT_ID, &product_id))
		goto failed;

	aprint_debug_dev(parent, "xc5k: product=0x%04x\n", product_id);

	return xc;

failed:
	kmem_free(xc, sizeof(*xc));
	return NULL;
}

void
xc5k_close(struct xc5k *xc)
{
	kmem_free(xc, sizeof(*xc));
}

int
xc5k_tune_video(struct xc5k *xc, struct xc5k_params *params)
{
	uint16_t amode, vmode;
	uint16_t lock, freq;
	int retry;

	switch (params->standard) {
	case VIDEO_STANDARD_NTSC_M:
	case VIDEO_STANDARD_NTSC_M_JP:
	case VIDEO_STANDARD_NTSC_M_KR:
		amode = XC5K_AUDIO_MODE_BTSC;
		vmode = XC5K_VIDEO_MODE_BTSC;
		break;
	default:
		return EINVAL;
	}

	if (xc5k_write_2(xc, XC5K_REG_SIGNAL_SOURCE, params->signal_source))
		return EIO;
	if (xc5k_write_2(xc, XC5K_REG_VIDEO_MODE, vmode))
		return EIO;
	if (xc5k_write_2(xc, XC5K_REG_AUDIO_MODE, amode))
		return EIO;
	if (xc5k_write_2(xc, XC5K_REG_OUTAMP, XC5K_OUTAMP_ANALOG))
		return EIO;
	freq = (params->frequency * 62500) / 15625;
#ifdef XC5K_DEBUG
	printf("xc5k_tune_video: frequency=%u (%u Hz)\n", params->frequency,
	    params->frequency * 62500);
	printf("           freq=%u\n", freq);
#endif
	if (xc5k_write_2(xc, XC5K_REG_FINER_FREQ, freq))
		return EIO;

	retry = 100;
	while (--retry > 0) {
		if (xc5k_read_2(xc, XC5K_REG_LOCK, &lock))
			return EIO;
#ifdef XC5K_DEBUG
		printf("xc5k_tune_video: lock=0x%04x\n", lock);
#endif
		if (lock == 1)
			break;
		delay(5000);
	}

	return 0;
}

int
xc5k_tune_dtv(struct xc5k *xc, const struct dvb_frontend_parameters *params)
{
	uint16_t amode, vmode;
	uint32_t freq, ifout;
	int signal_source;
	fe_modulation_t modulation;

	if (xc->fe_type == FE_ATSC)
		modulation = params->u.vsb.modulation;
	else if (xc->fe_type == FE_QAM)
		modulation = params->u.qam.modulation;
	else
		return EINVAL;

	switch (modulation) {
	case VSB_8:
	case VSB_16:
		signal_source = XC5K_SIGNAL_SOURCE_AIR;
		switch (xc->fe_type) {
		case FE_ATSC:
			amode = XC5K_AUDIO_MODE_DTV6;
			vmode = XC5K_VIDEO_MODE_DTV6;
			freq = params->frequency - 1750000;
			break;
		default:
			return EINVAL;
		}
		break;
	case QAM_16:
	case QAM_32:
	case QAM_64:
	case QAM_128:
	case QAM_256:
		signal_source = XC5K_SIGNAL_SOURCE_CABLE;
		switch (xc->fe_type) {
		case FE_ATSC:
			amode = XC5K_AUDIO_MODE_DTV6;
			vmode = XC5K_VIDEO_MODE_DTV6;
			freq = params->frequency - 1750000;
			break;
		case FE_QAM:
			amode = XC5K_AUDIO_MODE_DTV78;
			vmode = XC5K_VIDEO_MODE_DTV78;
			freq = params->frequency - 2750000;
			break;
		default:
			return EINVAL;
		}
		break;
	default:
		return EINVAL;
	}

	if (freq > XC5K_FREQ_MAX || freq < XC5K_FREQ_MIN)
		return ERANGE;

	if (xc5k_write_2(xc, XC5K_REG_SIGNAL_SOURCE, signal_source))
		return EIO;
	if (xc5k_write_2(xc, XC5K_REG_VIDEO_MODE, vmode))
		return EIO;
	if (xc5k_write_2(xc, XC5K_REG_AUDIO_MODE, amode))
		return EIO;
	ifout = ((xc->if_freq / 1000) * 1024) / 1000;
	if (xc5k_write_2(xc, XC5K_REG_IF_OUT, ifout))
		return EIO;
	if (xc5k_write_2(xc, XC5K_REG_OUTAMP, XC5K_OUTAMP_DIGITAL))
		return EIO;
	freq = (uint16_t)(freq / 15625);
	if (xc5k_write_2(xc, XC5K_REG_FINER_FREQ, freq))
		return EIO;

	return 0;
}

fe_status_t
xc5k_get_status(struct xc5k *xc)
{
	uint16_t lock_status;
	fe_status_t festatus = 0;

	if (xc5k_read_2(xc, XC5K_REG_LOCK, &lock_status))
		return 0;
	if (lock_status & XC5K_LOCK_LOCKED) {
		festatus |= FE_HAS_LOCK;
		if ((lock_status & XC5K_LOCK_NOSIGNAL) == 0)
			festatus |= FE_HAS_SIGNAL;
	}

	return festatus;
}

MODULE(MODULE_CLASS_DRIVER, xc5k, "i2cexec");

static int
xc5k_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
		mutex_init(&xc5k_firmware_lock, MUTEX_DEFAULT, IPL_NONE);
		return 0;
	case MODULE_CMD_FINI:
		mutex_destroy(&xc5k_firmware_lock);
		return 0;
	default:
		return ENOTTY;
	}
}
