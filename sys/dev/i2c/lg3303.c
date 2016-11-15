/* $NetBSD: lg3303.c,v 1.9 2015/03/07 14:16:51 jmcneill Exp $ */

/*-
 * Copyright 2007 Jason Harmening
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
 *
 */

#include <sys/param.h>
__KERNEL_RCSID(0, "$NetBSD: lg3303.c,v 1.9 2015/03/07 14:16:51 jmcneill Exp $");

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/module.h>
#include <sys/bitops.h>

#include <dev/i2c/i2cvar.h>
#include <dev/i2c/lg3303var.h>
#include <dev/dtv/dtvif.h>
#include <dev/dtv/dtv_math.h>

#define REG_TOP_CONTROL         0x00
#define REG_IRQ_MASK            0x01
#define REG_IRQ_STATUS          0x02
#define REG_VSB_CARRIER_FREQ0   0x16
#define REG_VSB_CARRIER_FREQ1   0x17
#define REG_VSB_CARRIER_FREQ2   0x18
#define REG_VSB_CARRIER_FREQ3   0x19
#define REG_CARRIER_MSEQAM1     0x1a
#define REG_CARRIER_MSEQAM2     0x1b
#define REG_CARRIER_LOCK        0x1c
#define REG_TIMING_RECOVERY     0x1d
#define REG_AGC_DELAY0          0x2a
#define REG_AGC_DELAY1          0x2b
#define REG_AGC_DELAY2          0x2c
#define REG_AGC_RF_BANDWIDTH0   0x2d
#define REG_AGC_RF_BANDWIDTH1   0x2e
#define REG_AGC_RF_BANDWIDTH2   0x2f
#define REG_AGC_LOOP_BANDWIDTH0 0x30
#define REG_AGC_LOOP_BANDWIDTH1 0x31
#define REG_AGC_FUNC_CTRL1      0x32
#define REG_AGC_FUNC_CTRL2      0x33
#define REG_AGC_FUNC_CTRL3      0x34
#define REG_AGC_RFIF_ACC0       0x39
#define REG_AGC_RFIF_ACC1       0x3a
#define REG_AGC_RFIF_ACC2       0x3b
#define REG_AGC_STATUS          0x3f
#define REG_SYNC_STATUS_VSB     0x43
#define REG_DEMUX_CONTROL       0x66
#define REG_EQPH_ERR0           0x6e
#define REG_EQ_ERR1             0x6f
#define REG_EQ_ERR2             0x70
#define REG_PH_ERR1             0x71
#define REG_PH_ERR2             0x72
#define REG_PACKET_ERR_COUNTER1 0x8b
#define REG_PACKET_ERR_COUNTER2 0x8c

#define LG3303_DEFAULT_DELAY 250000

static int	lg3303_reset(struct lg3303 *);
static int	lg3303_init(struct lg3303 *);

struct lg3303 *
lg3303_open(device_t parent, i2c_tag_t i2c, i2c_addr_t addr, int flags)
{
	struct lg3303 *lg;

	lg = kmem_alloc(sizeof(*lg), KM_SLEEP);
	if (lg == NULL)
		return NULL;
	lg->parent = parent;
	lg->i2c = i2c;
	lg->i2c_addr = addr;
	lg->current_modulation = -1;
	lg->flags = flags;

	if (lg3303_init(lg) != 0) {
		kmem_free(lg, sizeof(*lg));
		return NULL;
	}

	device_printf(lg->parent, "lg3303: found @ 0x%02x\n", addr);

	return lg;
}

void
lg3303_close(struct lg3303 *lg)
{
	kmem_free(lg, sizeof(*lg));
}

static int
lg3303_write(struct lg3303 *lg, uint8_t *buf, size_t len)
{
	unsigned int i;
	uint8_t *p = buf;
	int error;

	for (i = 0; i < len - 1; i += 2) {
		error = iic_exec(lg->i2c, I2C_OP_WRITE_WITH_STOP, lg->i2c_addr,
		    p, 2, NULL, 0, 0);
		if (error)
			return error;
		p += 2;
	}

	return 0;
}

static int
lg3303_read(struct lg3303 *lg, uint8_t reg, uint8_t *buf, size_t len)
{
	int error;

	error = iic_exec(lg->i2c, I2C_OP_WRITE, lg->i2c_addr, 
	    &reg, sizeof(reg), NULL, 0, 0);
	if (error)
		return error;
	return iic_exec(lg->i2c, I2C_OP_READ, lg->i2c_addr,
	    NULL, 0, buf, len, 0);
}

static int
lg3303_reset(struct lg3303 *lg)
{
	uint8_t buffer[] = {REG_IRQ_STATUS, 0x00};
	int error = lg3303_write(lg, buffer, 2);
	if (error == 0) {
		buffer[1] = 0x01;
		error = lg3303_write(lg, buffer, 2);
	}
	return error;
}
      
static int
lg3303_init(struct lg3303 *lg)
{
	//static uint8_t init_data[] = {0x4c, 0x14, 0x87, 0xf3};
	static uint8_t init_data[] = {0x4c, 0x14};
	size_t len;
	int error;

#if notyet
	if (clock_polarity == DVB_IFC_POS_POL)
		len = 4;
	else
#endif
	len = 2;

	error = lg3303_write(lg, init_data, len);
	if (error == 0)
      		lg3303_reset(lg);

	return error;
}

int
lg3303_set_modulation(struct lg3303 *lg, fe_modulation_t modulation)
{
	int error;
	static uint8_t vsb_data[] = {
		0x04, 0x00,
		0x0d, 0x40,
		0x0e, 0x87,
		0x0f, 0x8e,
		0x10, 0x01,
		0x47, 0x8b
	};
	static uint8_t qam_data[] = {
		0x04, 0x00,
		0x0d, 0x00,
		0x0e, 0x00,
		0x0f, 0x00,
		0x10, 0x00,
		0x51, 0x63,
		0x47, 0x66,
		0x48, 0x66,
		0x4d, 0x1a,
		0x49, 0x08,
		0x4a, 0x9b   
	};
	uint8_t top_ctrl[] = {REG_TOP_CONTROL, 0x00};

	error = lg3303_reset(lg);
	if (error)
		return error;

	if (lg->flags & LG3303_CFG_SERIAL_INPUT)
		top_ctrl[1] = 0x40;  

	switch (modulation) {
	case VSB_8:
		top_ctrl[1] |= 0x03;
		error = lg3303_write(lg, vsb_data, sizeof(vsb_data));
		if (error)
			return error;
		break;
	case QAM_256:
		top_ctrl[1] |= 0x01;
		/* FALLTHROUGH */
	case QAM_64:
		error = lg3303_write(lg, qam_data, sizeof(qam_data));
		if (error)
			return error;
		break;
	default:
		device_printf(lg->parent,
		    "lg3303: unsupported modulation type (%d)\n",
		    modulation);
		return EINVAL;
	}
	error = lg3303_write(lg, top_ctrl, sizeof(top_ctrl));
	if (error)
		return error;
	lg->current_modulation = modulation;
	lg3303_reset(lg);

	return error;
}

fe_status_t
lg3303_get_dtv_status(struct lg3303 *lg)
{
	uint8_t reg = 0, value = 0x00;
	fe_status_t festatus = 0;
	int error = 0;

	error = lg3303_read(lg, 0x58, &value, sizeof(value));
	if (error)
		return 0;

	if (value & 0x01)
		festatus |= FE_HAS_SIGNAL;

	error = lg3303_read(lg, REG_CARRIER_LOCK, &value, sizeof(value));
	if (error)
		return 0;

	switch (lg->current_modulation) {
	case VSB_8:
		if (value & 0x80)
			festatus |= FE_HAS_CARRIER;
		reg = 0x38;
		break;
	case QAM_64:
	case QAM_256:
		if ((value & 0x07) == 0x07)
			festatus |= FE_HAS_CARRIER;
		reg = 0x8a;
		break;
	default:
		device_printf(lg->parent,
		    "lg3303: unsupported modulation type (%d)\n",
		    lg->current_modulation);
		return 0;
	}

	if ((festatus & FE_HAS_CARRIER) == 0)
		return festatus;

	error = lg3303_read(lg, reg, &value, sizeof(value));
	if (!error && (value & 0x01))
		festatus |= FE_HAS_LOCK;

	if (festatus & FE_HAS_LOCK)
		festatus |= (FE_HAS_SYNC | FE_HAS_VITERBI);

	return festatus;
}

uint16_t
lg3303_get_snr(struct lg3303 *lg)
{
	int64_t noise, snr_const;
	uint8_t buffer[5];
	int64_t snr;
	int error;

	switch (lg->current_modulation) {
	case VSB_8:
		error = lg3303_read(lg, REG_EQPH_ERR0, buffer, sizeof(buffer));
		if (error)
			return 0;
		noise = ((buffer[0] & 7) << 16) | (buffer[3] << 8) | buffer[4];
		snr_const = 73957994;	/* log10(2560) * pow(2,24) */
		break;
	case QAM_64:
	case QAM_256:
		error = lg3303_read(lg, REG_CARRIER_MSEQAM1, buffer, 2);
		if (error)
			return 0;
		noise = (buffer[0] << 8) | buffer[1];
		if (lg->current_modulation == QAM_64)
			snr_const = 97939837;	/* log10(688128) * pow(2,24) */
		else
			snr_const = 98026066;	/* log10(696320) * pow(2,24) */
		break;
	default:
		device_printf(lg->parent,
		    "lg3303: unsupported modulation type (%d)\n",
		    lg->current_modulation);
		return 0;
	}

	if (noise == 0)
		return 0;
	snr = dtv_intlog10(noise);
	if (snr > snr_const)
		return 0;
	return (10 * (snr_const - snr)) >> 16;
}

uint16_t
lg3303_get_signal_strength(struct lg3303 *lg)
{
	return ((uint32_t)lg3303_get_snr(lg) << 16) / 8960;
}

uint32_t
lg3303_get_ucblocks(struct lg3303 *lg)
{
	uint8_t buffer[2];
	int error;

	error = lg3303_read(lg, REG_PACKET_ERR_COUNTER1, buffer, sizeof(buffer));
	if (error)
		return 0;

	return (buffer[0] << 8) | buffer[1];
}

MODULE(MODULE_CLASS_DRIVER, lg3303, "i2cexec,dtv_math");

static int
lg3303_modcmd(modcmd_t cmd, void *opaque)
{
	if (cmd == MODULE_CMD_INIT || cmd == MODULE_CMD_FINI)
		return 0;
	return ENOTTY;
}
