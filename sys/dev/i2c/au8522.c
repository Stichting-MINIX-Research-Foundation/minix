/* $NetBSD: au8522.c,v 1.7 2015/03/07 14:16:51 jmcneill Exp $ */

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
 * Auvitek AU8522
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: au8522.c,v 1.7 2015/03/07 14:16:51 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kmem.h>
#include <sys/module.h>

#include <dev/dtv/dtvio.h>

#include <dev/i2c/i2cvar.h>

#include <dev/i2c/au8522reg.h>
#include <dev/i2c/au8522var.h>
#include <dev/i2c/au8522mod.h>

static int	au8522_reset(struct au8522 *);
static int	au8522_read_1(struct au8522 *, uint16_t, uint8_t *);
static int	au8522_write_1(struct au8522 *, uint16_t, uint8_t);
static int	au8522_set_vinput(struct au8522 *, au8522_vinput_t);
static int	au8522_set_ainput(struct au8522 *, au8522_ainput_t);
static void	au8522_set_common(struct au8522 *, au8522_vinput_t);

static int
au8522_reset(struct au8522 *au)
{
	return au8522_write_1(au, 0xa4, 1 << 5);
}

static int
au8522_read_1(struct au8522 *au, uint16_t reg, uint8_t *val)
{
	uint8_t cmd[2];
	int error;

	cmd[0] = (reg >> 8) | 0x40;
	cmd[1] = reg & 0xff;
	error = iic_exec(au->i2c, I2C_OP_WRITE, au->i2c_addr,
	    cmd, sizeof(cmd), NULL, 0, 0);
	if (error)
		return error;
	return iic_exec(au->i2c, I2C_OP_READ, au->i2c_addr,
		    NULL, 0, val, sizeof(*val), 0);
}

static int
au8522_write_1(struct au8522 *au, uint16_t reg, uint8_t val)
{
	uint8_t data[3];

	data[0] = (reg >> 8) | 0x80;
	data[1] = reg & 0xff;
	data[2] = val;
	return iic_exec(au->i2c, I2C_OP_WRITE, au->i2c_addr,
	    data, sizeof(data), NULL, 0, 0);
}

static int
au8522_set_vinput(struct au8522 *au, au8522_vinput_t vi)
{
	switch (vi) {
	case AU8522_VINPUT_CVBS:
		au8522_write_1(au, AU8522_REG_MODCLKCTL, AU8522_MODCLKCTL_CVBS);
		au8522_write_1(au, AU8522_REG_PGACTL, 0x00);
		au8522_write_1(au, AU8522_REG_CLAMPCTL, 0x0e);
		au8522_write_1(au, AU8522_REG_PGACTL, 0x10);
		au8522_write_1(au, AU8522_REG_INPUTCTL,
		    AU8522_INPUTCTL_CVBS_CH1);

		au8522_set_common(au, vi);

		au8522_write_1(au, AU8522_REG_SYSMODCTL0,
		    AU8522_SYSMODCTL0_CVBS);
		break;
	case AU8522_VINPUT_SVIDEO:
		au8522_write_1(au, AU8522_REG_MODCLKCTL,
		    AU8522_MODCLKCTL_SVIDEO);
		au8522_write_1(au, AU8522_REG_INPUTCTL,
		    AU8522_INPUTCTL_SVIDEO_CH13);
		au8522_write_1(au, AU8522_REG_CLAMPCTL, 0x00);

		au8522_set_common(au, vi);

		au8522_write_1(au, AU8522_REG_SYSMODCTL0,
		    AU8522_SYSMODCTL0_CVBS);

		break;
	case AU8522_VINPUT_CVBS_TUNER:
		au8522_write_1(au, AU8522_REG_MODCLKCTL, AU8522_MODCLKCTL_CVBS);
		au8522_write_1(au, AU8522_REG_PGACTL, 0x00);
		au8522_write_1(au, AU8522_REG_CLAMPCTL, 0x0e);
		au8522_write_1(au, AU8522_REG_PGACTL, 0x10);
		au8522_write_1(au, AU8522_REG_INPUTCTL,
		    AU8522_INPUTCTL_CVBS_CH4_SIF);

		au8522_set_common(au, vi);

		au8522_write_1(au, AU8522_REG_SYSMODCTL0,
		    AU8522_SYSMODCTL0_CVBS);

		break;
	default:
		return EINVAL;
	}

	return 0;
}

static void
au8522_set_common(struct au8522 *au, au8522_vinput_t vi)
{
	au8522_write_1(au, AU8522_REG_INTMASK, 0x00);
	au8522_write_1(au, AU8522_REG_VIDEOMODE, vi == AU8522_VINPUT_SVIDEO ?
	    AU8522_VIDEOMODE_SVIDEO : AU8522_VIDEOMODE_CVBS);
	au8522_write_1(au, AU8522_REG_TV_PGA, AU8522_TV_PGA_CVBS);
}

static int
au8522_set_ainput(struct au8522 *au, au8522_ainput_t ai)
{
	/* mute during mode change */
	au8522_write_1(au, AU8522_REG_AUDIO_VOL_L, 0x00);
	au8522_write_1(au, AU8522_REG_AUDIO_VOL_R, 0x00);
	au8522_write_1(au, AU8522_REG_AUDIO_VOL, 0x00);

	switch (ai) {
	case AU8522_AINPUT_SIF:
		au8522_write_1(au, AU8522_REG_SYSMODCTL0,
		    AU8522_SYSMODCTL0_CVBS);
		au8522_write_1(au, AU8522_REG_AUDIO_MODE, 0x82);
		au8522_write_1(au, AU8522_REG_SYSMODCTL1,
		    AU8522_SYSMODCTL1_I2S);
		au8522_write_1(au, AU8522_REG_AUDIO_FREQ, 0x03);
		au8522_write_1(au, AU8522_REG_I2S_CTL2, 0xc2);
		/* unmute */
		au8522_write_1(au, AU8522_REG_AUDIO_VOL_L, 0x7f);
		au8522_write_1(au, AU8522_REG_AUDIO_VOL_R, 0x7f);
		au8522_write_1(au, AU8522_REG_AUDIO_VOL, 0xff);
		break;
	case AU8522_AINPUT_NONE:
		au8522_write_1(au, AU8522_REG_USBEN, 0x00);
		au8522_write_1(au, AU8522_REG_AUDIO_VOL_L, 0x7f);
		au8522_write_1(au, AU8522_REG_AUDIO_VOL_R, 0x7f);
		au8522_write_1(au, AU8522_REG_AUDIO_MODE, 0x40);
		au8522_write_1(au, AU8522_REG_SYSMODCTL1,
		    AU8522_SYSMODCTL1_SVIDEO);
		au8522_write_1(au, AU8522_REG_AUDIO_FREQ, 0x03);
		au8522_write_1(au, AU8522_REG_I2S_CTL2, 0x02);
		au8522_write_1(au, AU8522_REG_SYSMODCTL0,
		    AU8522_SYSMODCTL0_CVBS);
		break;
	default:
		return EINVAL;
	}
	return 0;
}

static int
au8522_set_if(struct au8522 *au)
{
	uint8_t ifinit[3];
	unsigned int n;

	switch (au->if_freq) {
	case 6000000:	/* 6MHz */
		ifinit[0] = 0xfb;
		ifinit[1] = 0x8e;
		ifinit[2] = 0x39;
		break;
	default:
		aprint_error_dev(au->parent, "au8522: unsupported if freq %dHz\n", au->if_freq);
		return EINVAL;
	}

	for (n = 0; n < __arraycount(ifinit); n++)
		au8522_write_1(au, 0x80b5 + n, ifinit[n]);

	return 0;
}

struct au8522 *
au8522_open(device_t parent, i2c_tag_t i2c, i2c_addr_t addr, unsigned int if_freq)
{
	struct au8522 *au;

	au = kmem_alloc(sizeof(*au), KM_SLEEP);
	if (au == NULL)
		return NULL;
	au->parent = parent;
	au->i2c = i2c;
	au->i2c_addr = addr;
	au->current_modulation = -1;
	au->if_freq = if_freq;

	if (au8522_reset(au))
		goto failed;
	if (au8522_write_1(au, AU8522_REG_TUNERCTL, AU8522_TUNERCTL_EN))
		goto failed;

	return au;

failed:
	kmem_free(au, sizeof(*au));
	return NULL;
}

void
au8522_close(struct au8522 *au)
{
	kmem_free(au, sizeof(*au));
}

void
au8522_enable(struct au8522 *au, bool enable)
{
	if (enable) {
		au8522_write_1(au, AU8522_REG_SYSMODCTL0,
		    AU8522_SYSMODCTL0_RESET);
		delay(1000);
		au8522_write_1(au, AU8522_REG_SYSMODCTL0,
		    AU8522_SYSMODCTL0_CVBS);
	} else {
		au8522_write_1(au, AU8522_REG_SYSMODCTL0,
		    AU8522_SYSMODCTL0_DISABLE);
	}
}

void
au8522_set_input(struct au8522 *au, au8522_vinput_t vi, au8522_ainput_t ai)
{
	au8522_reset(au);

	if (vi != AU8522_VINPUT_UNCONF)
		au8522_set_vinput(au, vi);
	if (ai != AU8522_AINPUT_UNCONF)
		au8522_set_ainput(au, ai);
}

int
au8522_get_signal(struct au8522 *au)
{
	uint8_t status;

	if (au8522_read_1(au, AU8522_REG_STATUS, &status))
		return 0;

#ifdef AU8522_DEBUG
	printf("au8522: status=0x%02x\n", status);
#endif
	return (status & AU8522_STATUS_LOCK) == AU8522_STATUS_LOCK ? 1 : 0;
}

void
au8522_set_audio(struct au8522 *au, bool onoff)
{
	if (onoff) {
		au8522_write_1(au, AU8522_REG_AUDIO_VOL_L, 0x7f);
		au8522_write_1(au, AU8522_REG_AUDIO_VOL_R, 0x7f);
		au8522_write_1(au, AU8522_REG_AUDIO_VOL, 0xff);
	} else {
		au8522_write_1(au, AU8522_REG_AUDIO_VOL_L, 0x00);
		au8522_write_1(au, AU8522_REG_AUDIO_VOL_R, 0x00);
		au8522_write_1(au, AU8522_REG_AUDIO_VOL, 0x00);
	}
}

int
au8522_set_modulation(struct au8522 *au, fe_modulation_t modulation)
{
	const struct au8522_modulation_table *modtab = NULL;
	size_t modtablen;
	unsigned int n;

	switch (modulation) {
	case VSB_8:
		modtab = au8522_modulation_8vsb;
		modtablen = __arraycount(au8522_modulation_8vsb);
		break;
	case QAM_64:
		modtab = au8522_modulation_qam64;
		modtablen = __arraycount(au8522_modulation_qam64);
		break;
	case QAM_256:
		modtab = au8522_modulation_qam256;
		modtablen = __arraycount(au8522_modulation_qam256);
		break;
	default:
		return EINVAL;
	}

	for (n = 0; n < modtablen; n++)
		au8522_write_1(au, modtab[n].reg, modtab[n].val);

	au8522_set_if(au);

	au->current_modulation = modulation;

	return 0;
}

void
au8522_set_gate(struct au8522 *au, bool onoff)
{
	au8522_write_1(au, AU8522_REG_TUNERCTL, onoff ? AU8522_TUNERCTL_EN : 0);
}

fe_status_t
au8522_get_dtv_status(struct au8522 *au)
{
	fe_status_t status = 0;
	uint8_t val;

	switch (au->current_modulation) {
	case VSB_8:
		if (au8522_read_1(au, 0x4088, &val))
			return 0;
		if ((val & 0x03) == 0x03) {
			status |= FE_HAS_SIGNAL;
			status |= FE_HAS_CARRIER;
			status |= FE_HAS_VITERBI;
		}
		break;
	case QAM_64:
	case QAM_256:
		if (au8522_read_1(au, 0x4541, &val))
			return 0;
		if (val & 0x80) {
			status |= FE_HAS_VITERBI;
		}
		if (val & 0x20) {
			status |= FE_HAS_SIGNAL;
			status |= FE_HAS_CARRIER;
		}
		break;
	default:
		break;
	}

	if (status & FE_HAS_VITERBI) {
		status |= FE_HAS_SYNC;
		status |= FE_HAS_LOCK;
	}

	return status;
}

uint16_t
au8522_get_snr(struct au8522 *au)
{
	const struct au8522_snr_table *snrtab = NULL;
	uint16_t snrreg;
	uint8_t val;
	size_t snrtablen;
	unsigned int n;

	switch (au->current_modulation) {
	case VSB_8:
		snrtab = au8522_snr_8vsb;
		snrtablen = __arraycount(au8522_snr_8vsb);
		snrreg = AU8522_REG_SNR_VSB;
		break;
	case QAM_64:
		snrtab = au8522_snr_qam64;
		snrtablen = __arraycount(au8522_snr_qam64);
		snrreg = AU8522_REG_SNR_QAM;
		break;
	case QAM_256:
		snrtab = au8522_snr_qam256;
		snrtablen = __arraycount(au8522_snr_qam256);
		snrreg = AU8522_REG_SNR_QAM;
		break;
	default:
		return 0;
	}

	if (au8522_read_1(au, snrreg, &val))
		return 0;

	for (n = 0; n < snrtablen; n++) {
		if (val < snrtab[n].val)
			return snrtab[n].snr;
	}

	return 0;
}

MODULE(MODULE_CLASS_DRIVER, au8522, "i2cexec");

static int
au8522_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
		return 0;
	case MODULE_CMD_FINI:
		return 0;
	default:
		return ENOTTY;
	}
}
