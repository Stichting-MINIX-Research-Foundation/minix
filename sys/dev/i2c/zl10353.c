/*	$NetBSD: zl10353.c,v 1.4 2015/03/07 14:16:51 jmcneill Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jukka Ruohonen.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: zl10353.c,v 1.4 2015/03/07 14:16:51 jmcneill Exp $");

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/module.h>

#include <dev/dtv/dtvif.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/zl10353reg.h>
#include <dev/i2c/zl10353var.h>

/*
 * Zarlink ZL10353 demodulator (now known as Intel CE623x).
 *
 * An incomplete datasheet:
 *
 *	Intel Corporation: CE6230 - COFDM demodulator with
 *	USB interface for PC-TV. Data Sheet, Revision 1.1.
 *	March 29, 2007.
 */

static int	zl10353_probe(struct zl10353 *);
static int	zl10353_read(struct zl10353 *, uint8_t, uint8_t *);
static int	zl10353_write(struct zl10353 *, uint8_t, uint8_t);
static int	zl10353_reset(struct zl10353 *, bool);
static int	zl10353_set_agc(struct zl10353 *,
			const struct dvb_frontend_parameters *);
static int	zl10353_set_bw(struct zl10353 *, fe_bandwidth_t);
static int	zl10353_set_rate(struct zl10353 *);
static int	zl10353_set_freq(struct zl10353 *);
static int	zl10353_set_tps(struct zl10353 *,
			const struct dvb_frontend_parameters *);
static int	zl10353_get_guard(fe_guard_interval_t, uint16_t *);
static int	zl10353_get_mode(fe_transmit_mode_t, uint16_t *);
static int	zl10353_get_fec(fe_code_rate_t, bool, uint16_t *);
static int	zl10353_get_modulation(fe_modulation_t, uint16_t *);
static int	zl10353_get_hier(fe_hierarchy_t, uint16_t *);

struct zl10353 *
zl10353_open(device_t parent, i2c_tag_t i2c, i2c_addr_t addr)
{
	struct zl10353 *zl;

	zl = kmem_zalloc(sizeof(*zl), KM_SLEEP);

	if (zl == NULL)
		return NULL;

	zl->zl_i2c = i2c;
	zl->zl_i2c_addr = addr;
	zl->zl_parent = parent;

	zl->zl_freq = ZL10353_DEFAULT_INPUT_FREQ;
	zl->zl_clock = ZL10353_DEFAULT_CLOCK_MHZ;

	if (zl10353_reset(zl, true) != 0) {
		zl10353_close(zl);
		return NULL;
	}

	if (zl10353_probe(zl) != 0) {
		zl10353_close(zl);
		return NULL;
	}

	return zl;
}

void
zl10353_close(struct zl10353 *zl)
{
	kmem_free(zl, sizeof(*zl));
}

static int
zl10353_probe(struct zl10353 *zl)
{
	uint8_t val;
	int rv;

	if ((rv = zl10353_read(zl, ZL10353_REG_ID, &val)) != 0)
		return rv;

	switch (val) {

	case ZL10353_ID_CE6230:
		zl->zl_name = "Intel CE6230";
		break;

	case ZL10353_ID_CE6231:
		zl->zl_name = "Intel CE6231";
		break;

	case ZL10353_ID_ZL10353:
		zl->zl_name = "Zarlink ZL10353";
		break;

	default:
		aprint_error_dev(zl->zl_parent, "unknown chip 0x%02x\n", val);
		return ENOTSUP;
	}

	aprint_verbose_dev(zl->zl_parent, "found %s at i2c "
	    "addr 0x%02x\n",  zl->zl_name, zl->zl_i2c_addr);

	return 0;
}

static int
zl10353_read(struct zl10353 *zl, uint8_t reg, uint8_t *valp)
{
	static const i2c_op_t op = I2C_OP_READ;
	int rv;

	if ((rv = iic_acquire_bus(zl->zl_i2c, 0)) != 0)
		return rv;

	rv = iic_exec(zl->zl_i2c, op, zl->zl_i2c_addr, &reg, 1, valp, 1, 0);
	iic_release_bus(zl->zl_i2c, 0);

	return rv;
}

static int
zl10353_write(struct zl10353 *zl, uint8_t reg, uint8_t val)
{
	static const i2c_op_t op = I2C_OP_WRITE;
	const uint8_t cmd[2] = { reg, val };
	int rv;

	if ((rv = iic_acquire_bus(zl->zl_i2c, 0)) != 0)
		return rv;

	rv = iic_exec(zl->zl_i2c, op, zl->zl_i2c_addr, cmd, 2, NULL, 0, 0);
	iic_release_bus(zl->zl_i2c, 0);

	return rv;
}

static int
zl10353_reset(struct zl10353 *zl, bool hard)
{
	size_t i = 0, len = 5;
	int rv;

	static const struct {
		uint8_t		reg;
		uint8_t		val;
	} reset[] = {
		{ 0x50, 0x03 },	/* Hard */
		{ 0x03, 0x44 },
		{ 0x44, 0x46 },
		{ 0x46, 0x15 },
		{ 0x15, 0x0f },
		{ 0x55, 0x80 },	/* Soft */
		{ 0xea, 0x01 },
		{ 0xea, 0x00 },
	};

	if (hard != true) {
		len = __arraycount(reset);
		i = 5;
	}

	while (i < len) {

		if ((rv = zl10353_write(zl, reset[i].reg, reset[i].val)) != 0)
			return rv;
		else {
			delay(100);
		}

		i++;

	}

	return 0;
}

void
zl10353_get_devinfo(struct dvb_frontend_info *fi)
{

	fi->type = FE_OFDM;

	fi->frequency_min = 174000000;
	fi->frequency_max = 862000000;

	fi->frequency_tolerance = 0;
	fi->frequency_stepsize = 166667;

	fi->caps |= FE_CAN_FEC_1_2 | FE_CAN_FEC_2_3 | FE_CAN_FEC_3_4 |
		    FE_CAN_FEC_5_6 | FE_CAN_FEC_7_8 | FE_CAN_QPSK |
	            FE_CAN_QAM_16  | FE_CAN_QAM_64  | FE_CAN_RECOVER |
		    FE_CAN_MUTE_TS;

	fi->caps |= FE_CAN_FEC_AUTO | FE_CAN_TRANSMISSION_MODE_AUTO |
		    FE_CAN_QAM_AUTO | FE_CAN_GUARD_INTERVAL_AUTO |
		    FE_CAN_HIERARCHY_AUTO;
}

int
zl10353_set_params(struct zl10353 *zl,const struct dvb_frontend_parameters *fp)
{
	int rv;

	/* 1. Soft reset. */
	if ((rv = zl10353_reset(zl, false)) != 0)
		return rv;

	/* 2. Set AGC. */
	if ((rv = zl10353_set_agc(zl, fp)) != 0)
		return rv;

	/* 3. Set bandwidth. */
	if ((rv = zl10353_set_bw(zl, fp->u.ofdm.bandwidth)) != 0)
		return rv;

	/* 4. Set nominal rate. */
	if ((rv = zl10353_set_rate(zl)) != 0)
		return rv;

	/* 5. Set input frequency. */
	if ((rv = zl10353_set_freq(zl)) != 0)
		return rv;

	/* 6. Set TPS parameters. */
	if ((rv = zl10353_set_tps(zl, fp)) != 0)
		return rv;

	return 0;
}

int
zl10353_set_fsm(struct zl10353 *zl)
{
	return zl10353_write(zl, ZL10353_REG_FSM, ZL10353_FSM_START);
}

int
zl10353_set_gate(void *aux, bool enable)
{
	uint8_t val = ZL10353_GATE_DISABLE;

	if (enable != false)
		val = ZL10353_GATE_ENABLE;

	return zl10353_write(aux, ZL10353_REG_GATE, val);
}

static int
zl10353_set_agc(struct zl10353 *zl, const struct dvb_frontend_parameters *fp)
{
	const struct dvb_ofdm_parameters *ofdm = &fp->u.ofdm;
	uint8_t val = ZL10353_AGC_TARGET_DEFAULT;
	int rv;

	if ((rv = zl10353_write(zl, ZL10353_REG_AGC_TARGET, val)) != 0)
		return rv;
	else {
		val = 0;
	}

	if (ofdm->guard_interval != GUARD_INTERVAL_AUTO)
		val |= ZL10353_AGC_CTRL_GUARD_NONAUTO;

	if (ofdm->transmission_mode != TRANSMISSION_MODE_AUTO)
		val |= ZL10353_AGC_CTRL_MODE_NONAUTO;

	return zl10353_write(zl, ZL10353_REG_AGC_CTRL, val);
}

static int
zl10353_set_bw(struct zl10353 *zl, fe_bandwidth_t bw)
{
	uint8_t val[3];
	int rv;

	switch (bw) {

	case BANDWIDTH_6_MHZ:
		val[0] = ZL10353_BW_1_6_MHZ;
		val[1] = ZL10353_BW_2_6_MHZ;
		val[2] = ZL10353_BW_3_6_MHZ;
		zl->zl_bw = 6;
		break;

	case BANDWIDTH_7_MHZ:
		val[0] = ZL10353_BW_1_7_MHZ;
		val[1] = ZL10353_BW_2_7_MHZ;
		val[2] = ZL10353_BW_3_7_MHZ;
		zl->zl_bw = 7;
		break;

	case BANDWIDTH_8_MHZ:
		val[0] = ZL10353_BW_1_8_MHZ;
		val[1] = ZL10353_BW_2_8_MHZ;
		val[2] = ZL10353_BW_3_8_MHZ;
		zl->zl_bw = 8;
		break;

	default:
		zl->zl_bw = 0;
		return EINVAL;
	}

	if ((rv = zl10353_write(zl, ZL10353_REG_BW_1, val[0])) != 0)
		return rv;

	if ((rv = zl10353_write(zl, ZL10353_REG_BW_2, val[1])) != 0)
		return rv;

	if ((rv = zl10353_write(zl, ZL10353_REG_BW_3, val[2])) != 0)
		return rv;

	return 0;
}

static int
zl10353_set_rate(struct zl10353 *zl)
{
	static const uint64_t c = 1497965625;
	uint64_t val;
	int rv;

	KASSERT(zl->zl_bw >= 6 && zl->zl_bw <= 8);
	KASSERT(zl->zl_clock > 0 && zl->zl_freq > 0);

	val = zl->zl_bw * c;
	val += zl->zl_clock >> 1;
	val /= zl->zl_clock;
	val = val & 0xffff;

	if ((rv = zl10353_write(zl, ZL10353_REG_RATE_1, val >> 8)) != 0)
		return rv;

	return zl10353_write(zl, ZL10353_REG_RATE_2, val & 0xff);
}

static int
zl10353_set_freq(struct zl10353 *zl)
{
	const uint16_t val = zl->zl_freq;
	int rv;

	if ((rv = zl10353_write(zl, ZL10353_REG_FREQ_1, val >> 8)) != 0)
		return rv;

	return zl10353_write(zl, ZL10353_REG_FREQ_2, val & 0xff);
}

static int
zl10353_set_tps(struct zl10353 *zl, const struct dvb_frontend_parameters *fp)
{
	const struct dvb_ofdm_parameters *ofdm = &fp->u.ofdm;
	uint16_t val = 0;
	int rv;

	if ((rv = zl10353_get_guard(ofdm->guard_interval, &val)) != 0)
		goto fail;

	if ((rv = zl10353_get_mode(ofdm->transmission_mode, &val)) != 0)
		goto fail;

	if ((rv = zl10353_get_fec(ofdm->code_rate_HP, true, &val)) != 0)
		goto fail;

	if ((rv = zl10353_get_fec(ofdm->code_rate_LP, false, &val)) != 0)
		goto fail;

	if ((rv = zl10353_get_modulation(ofdm->constellation, &val)) != 0)
		goto fail;

	if ((rv = zl10353_get_hier(ofdm->hierarchy_information, &val)) != 0)
		goto fail;

	if ((rv = zl10353_write(zl, ZL10353_REG_TPS_1, val >> 8)) != 0)
		goto fail;

	if ((rv = zl10353_write(zl, ZL10353_REG_TPS_2, val & 0xff)) != 0)
		goto fail;

	return 0;

fail:
	aprint_error_dev(zl->zl_parent, "failed to set "
	    "tps for %s (err %d)\n", zl->zl_name, rv);

	return rv;
}

static int
zl10353_get_guard(fe_guard_interval_t fg, uint16_t *valp)
{

	switch (fg) {

	case GUARD_INTERVAL_1_4:
		*valp |= ZL10353_TPS_GUARD_1_4;
		break;

	case GUARD_INTERVAL_1_8:
		*valp |= ZL10353_TPS_GUARD_1_8;
		break;

	case GUARD_INTERVAL_1_16:
		*valp |= ZL10353_TPS_GUARD_1_16;
		break;

	case GUARD_INTERVAL_1_32:
		*valp |= ZL10353_TPS_GUARD_1_32;
		break;

	case GUARD_INTERVAL_AUTO:
		*valp |= ZL10353_TPS_GUARD_AUTO;
		break;

	default:
		return EINVAL;
	}

	return 0;
}

static int
zl10353_get_mode(fe_transmit_mode_t fm, uint16_t *valp)
{

	switch (fm) {

	case TRANSMISSION_MODE_2K:
		*valp |= ZL10353_TPS_MODE_2K;
		break;

	case TRANSMISSION_MODE_8K:
		*valp |= ZL10353_TPS_MODE_8K;
		break;

	case TRANSMISSION_MODE_AUTO:
		*valp |= ZL10353_TPS_MODE_AUTO;
		break;

	default:
		return EINVAL;
	}

	return 0;
}

static int
zl10353_get_fec(fe_code_rate_t fc, bool hp, uint16_t *valp)
{
	uint16_t hpval = 0, lpval = 0;

	switch (fc) {

	case FEC_1_2:
		hpval = ZL10353_TPS_HP_FEC_1_2;
		lpval = ZL10353_TPS_LP_FEC_1_2;
		break;

	case FEC_2_3:
		hpval = ZL10353_TPS_HP_FEC_2_3;
		lpval = ZL10353_TPS_LP_FEC_2_3;
		break;

	case FEC_3_4:
		hpval = ZL10353_TPS_HP_FEC_3_4;
		lpval = ZL10353_TPS_LP_FEC_3_4;
		break;

	case FEC_5_6:
		hpval = ZL10353_TPS_HP_FEC_5_6;
		lpval = ZL10353_TPS_LP_FEC_5_6;
		break;

	case FEC_7_8:
		hpval = ZL10353_TPS_HP_FEC_7_8;
		lpval = ZL10353_TPS_LP_FEC_7_8;
		break;

	case FEC_AUTO:
		hpval = ZL10353_TPS_HP_FEC_AUTO;
		lpval = ZL10353_TPS_LP_FEC_AUTO;
		break;

	case FEC_NONE:
		return EOPNOTSUPP;

	default:
		return EINVAL;
	}

	*valp |= (hp != false) ? hpval : lpval;

	return 0;
}

static int
zl10353_get_modulation(fe_modulation_t fm, uint16_t *valp)
{

	switch (fm) {

	case QPSK:
		*valp |= ZL10353_TPS_MODULATION_QPSK;
		break;

	case QAM_16:
		*valp |= ZL10353_TPS_MODULATION_QAM_16;
		break;

	case QAM_64:
		*valp |= ZL10353_TPS_MODULATION_QAM_64;
		break;

	case QAM_AUTO:
		*valp |= ZL10353_TPS_MODULATION_QAM_AUTO;
		break;

	default:
		return EINVAL;
	}

	return 0;
}

static int
zl10353_get_hier(fe_hierarchy_t fh, uint16_t *valp)
{

	switch (fh) {

	case HIERARCHY_1:
		*valp |= ZL10353_TPS_HIERARCHY_1;
		break;

	case HIERARCHY_2:
		*valp |= ZL10353_TPS_HIERARCHY_2;
		break;

	case HIERARCHY_4:
		*valp |= ZL10353_TPS_HIERARCHY_4;
		break;

	case HIERARCHY_NONE:
		*valp |= ZL10353_TPS_HIERARCHY_NONE;
		break;

	case HIERARCHY_AUTO:
		*valp |= ZL10353_TPS_HIERARCHY_AUTO;
		break;

	default:
		return EINVAL;
	}

	return 0;
}

fe_status_t
zl10353_get_status(struct zl10353 *zl)
{
	const uint8_t lock = FE_HAS_CARRIER | FE_HAS_VITERBI | FE_HAS_SYNC;
	fe_status_t fs = 0;
	uint8_t val;

	if (zl10353_read(zl, ZL10353_REG_STATUS_LOCK, &val) == 0) {

		if ((val & ZL10353_STATUS_LOCK_ON) != 0)
			fs |= FE_HAS_LOCK;

		if ((val & ZL10353_STATUS_LOCK_CARRIER) != 0)
			fs |= FE_HAS_CARRIER;

		if ((val & ZL10353_STATUS_LOCK_VITERBI) != 0)
			fs |= FE_HAS_VITERBI;
	}

	if (zl10353_read(zl, ZL10353_REG_STATUS_SYNC, &val) == 0) {

		if ((val & ZL10353_STATUS_SYNC_ON) != 0)
			fs |= FE_HAS_SYNC;
	}

	if (zl10353_read(zl, ZL10353_REG_STATUS_SIGNAL, &val) == 0) {

		if ((val & ZL10353_STATUS_SIGNAL_ON) != 0)
			fs |= FE_HAS_SIGNAL;
	}

	return ((fs & lock) != lock) ? fs & ~FE_HAS_LOCK : fs;
};

uint16_t
zl10353_get_signal_strength(struct zl10353 *zl)
{
	uint8_t val1, val2;

	if (zl10353_read(zl, ZL10353_REG_SIGSTR_1, &val1) != 0)
		return 0;

	if (zl10353_read(zl, ZL10353_REG_SIGSTR_2, &val2) != 0)
		return 0;

	return val1 << 10 | val2 << 2 | 0x03;
}

uint16_t
zl10353_get_snr(struct zl10353 *zl)
{
	uint8_t val;

	if (zl10353_read(zl, ZL10353_REG_SNR, &val) != 0)
		return 0;

	return (val << 8) | val;
}

MODULE(MODULE_CLASS_DRIVER, zl10353, "i2cexec");

static int
zl10353_modcmd(modcmd_t cmd, void *aux)
{

	if (cmd != MODULE_CMD_INIT && cmd != MODULE_CMD_FINI)
		return ENOTTY;

	return 0;
}
