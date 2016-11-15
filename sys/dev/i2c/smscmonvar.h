/* $NetBSD: smscmonvar.h,v 1.1 2010/02/22 03:50:56 pgoyette Exp $ */

/*
 * Copyright (c) 2009 Takahiro Hayashi
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

#ifndef _DEV_I2C_SMSCMONVAR_H_
#define _DEV_I2C_SMSCMONVAR_H_

#define SMSCMON_ADDR		0x2c
#define SMSCMON_ADDR_MASK	0x7e

#define SMSCMON_REG_COMPANY	0x3e
#define SMSCMON_REG_STEPPING	0x3f
#define SMSCMON_REG_CONFIG	0x40

#define SMSC_CID_47M192		0x55	/* LPC47M192/LPC47M997 */
#define SMSC_REV_47M192		0x20

#define SMSCMON_NUM_SENSORS	12


struct smscmon_sc {
	device_t	sc_dev;
	i2c_tag_t	sc_tag;
	i2c_addr_t	sc_addr;

	uint8_t	(*smscmon_readreg)(struct smscmon_sc *, int);
	void	(*smscmon_writereg)(struct smscmon_sc *, int, int);

	struct sysmon_envsys *sc_sme;
	envsys_data_t sensors[SMSCMON_NUM_SENSORS];
	int		numsensors;
	struct smscmon_sensor *smscmon_sensors;
};

struct smscmon_sensor {
	const char	*desc;
	enum envsys_units type;
	uint8_t 	reg;
	void		(*refresh)(struct smscmon_sc *, envsys_data_t *);
	int		vmin;		/* value in uV for 0x00 */
	int		vmax;		/* value in uV for 0xff */
};

#endif /* _DEV_I2C_SMSCVAR_H_ */
