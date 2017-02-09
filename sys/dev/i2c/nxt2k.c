/* $NetBSD: nxt2k.c,v 1.4 2015/03/07 14:16:51 jmcneill Exp $ */

/*
 * Copyright (c) 2008, 2011 Jonathan A. Kollasch
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: nxt2k.c,v 1.4 2015/03/07 14:16:51 jmcneill Exp $");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/condvar.h>
#include <sys/mutex.h>
#include <sys/module.h>

#include <dev/firmload.h>

#include <dev/i2c/nxt2kvar.h>

struct nxt2k {
	device_t        parent;
	i2c_tag_t       tag;
	i2c_addr_t      addr;
	kcondvar_t      cv;
	kmutex_t        mtx;
	bool            loaded; /* firmware is loaded? */
};

static int nxt2k_init(struct nxt2k *);

static int nxt2k_writedata(struct nxt2k *, uint8_t, uint8_t *, size_t);
static int nxt2k_readdata(struct nxt2k *, uint8_t, uint8_t *, size_t);
static int nxt2k_writereg(struct nxt2k *, uint8_t, uint8_t *, size_t);
static int nxt2k_readreg(struct nxt2k*, uint8_t, uint8_t *, size_t);

static int nxt2k4_init(struct nxt2k *);
static bool nxt2k4_load_firmware(struct nxt2k *);
static void nxt2k4_mc_init(struct nxt2k *);
static void nxt2k_mc_start(struct nxt2k *);
static void nxt2k_mc_stop(struct nxt2k *);
static void nxt2k_agc_reset(struct nxt2k *);
static uint16_t nxt2k_crc_ccit(uint16_t, uint8_t);

static int
nxt2k_writedata(struct nxt2k *nxt, uint8_t reg, uint8_t *data, size_t len)
{
	uint8_t buffer[384];
	int error;

	KASSERT((len + 1) <= 384);

	if (iic_acquire_bus(nxt->tag, I2C_F_POLL) != 0)
		return false;

	buffer[0] = reg;
	memcpy(&buffer[1], data, len);
	
	error = iic_exec(nxt->tag, I2C_OP_WRITE_WITH_STOP, nxt->addr,
			 buffer, len + 1, NULL, 0, I2C_F_POLL);
	
	iic_release_bus(nxt->tag, I2C_F_POLL);

	return error;
}

static int
nxt2k_readdata(struct nxt2k *nxt, uint8_t reg, uint8_t *data, size_t len)
{
	int error;

	if (iic_acquire_bus(nxt->tag, I2C_F_POLL) != 0)
		return false;

	error = iic_exec(nxt->tag, I2C_OP_READ_WITH_STOP, nxt->addr,
			 &reg, 1, data, len, I2C_F_POLL);

	iic_release_bus(nxt->tag, I2C_F_POLL);

	return error;
}

static int
nxt2k_writereg(struct nxt2k *nxt, uint8_t reg, uint8_t *data, size_t len)
{
	uint8_t attr, len2, buf;

	nxt2k_writedata(nxt, 0x35, &reg, 1);

	nxt2k_writedata(nxt, 0x36, data, len);

	attr = 0x02;
	if (reg & 0x80) {
		attr = attr << 1;
		if (reg & 0x04)
			attr = attr >> 1;
	}
	len2 = ((attr << 4) | 0x10) | len;
	buf = 0x80;

	nxt2k_writedata(nxt, 0x34, &len2, 1);

	nxt2k_writedata(nxt, 0x21, &buf, 1);

	nxt2k_readdata(nxt, 0x21, &buf, 1);

	if (buf == 0)
		return 0;

	return -1;
}

static int
nxt2k_readreg(struct nxt2k *nxt, uint8_t reg, uint8_t *data, size_t len)
{
	uint8_t buf, len2, attr;
	unsigned int i;

	nxt2k_writedata(nxt, 0x35, &reg, 1);

	attr = 0x02;
	if (reg & 0x80) {
		attr = attr << 1;
		if (reg & 0x04)
			attr = attr >> 1;
	}

	len2 = (attr << 4) | len;
	nxt2k_writedata(nxt, 0x34, &len2, 1);
	
	buf = 0x80;
	nxt2k_writedata(nxt, 0x21, &buf, 1);

	for(i = 0; i < len; i++) {
		nxt2k_readdata(nxt, 0x36+i, &data[i], 1);
	}

	return 0;
}

static void
nxt2k_agc_reset(struct nxt2k *nxt)
{
	uint8_t byte;
	nxt2k_readreg(nxt, 0x08, &byte, 1);
	byte = 0x08;
	nxt2k_writereg(nxt, 0x08, &byte, 1);
	byte = 0x00;
	nxt2k_writereg(nxt, 0x08, &byte, 1);
	return;
}

static void
nxt2k_mc_stop(struct nxt2k *nxt)
{
	int counter;
	uint8_t stopval, buf;

	/* 2k4 */
	stopval = 0x10;

	buf = 0x80;
	nxt2k_writedata(nxt, 0x22, &buf, 1);

	for(counter = 0; counter < 20; counter++) {
		nxt2k_readdata(nxt, 0x31, &buf, 1);
		if (buf & stopval)
			return;
		mutex_enter(&nxt->mtx);
		cv_timedwait(&nxt->cv, &nxt->mtx, mstohz(10));
		mutex_exit(&nxt->mtx);
	}

	printf("%s timeout\n", __func__);
	
	return;
}

static void
nxt2k_mc_start(struct nxt2k *nxt)
{
	uint8_t buf;
	
	buf = 0x00;
	nxt2k_writedata(nxt, 0x22, &buf, 1);
}

static void
nxt2k4_mc_init(struct nxt2k *nxt)
{
	uint8_t byte;
	uint8_t data[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xC0};
	int counter;

	byte = 0x00;
	nxt2k_writedata(nxt, 0x2b, &byte, 1);
	byte = 0x70;
	nxt2k_writedata(nxt, 0x34, &byte, 1);
	byte = 0x04;
	nxt2k_writedata(nxt, 0x35, &byte, 1);

	nxt2k_writedata(nxt, 0x36, data, 9);

	byte = 0x80;
	nxt2k_writedata(nxt, 0x21, &byte, 1);

	for(counter = 0; counter < 20; counter++) {
		nxt2k_readdata(nxt, 0x21, &byte, 1);
		if ( byte == 0 )
			return;
		mutex_enter(&nxt->mtx);
		cv_timedwait(&nxt->cv, &nxt->mtx, mstohz(25));
		mutex_exit(&nxt->mtx);
	}

	printf("%s timeout\n", __func__);

	return;
}

/* CRC-CCIT */
static uint16_t
nxt2k_crc_ccit(uint16_t crc, uint8_t byte)
{
	int i;
	uint16_t input;
	
	input = byte << 8;

	for(i = 0; i < 8; i++) {
		if ((crc ^ input) & 0x8000)
			crc = (crc << 1) ^ 0x1021;
		else
			crc = (crc << 1);
		input = input << 1;
	}
	return crc;
}

static bool
nxt2k4_load_firmware(struct nxt2k *nxt)
{
	firmware_handle_t fh;
	uint8_t *blob;
	size_t fwsize;
	size_t position;
	int error;
	uint16_t crc;

	error = firmware_open("nxt2k", "dvb-fe-nxt2004.fw", &fh);
	if (error != 0) {
		printf("nxt2k firmware_open fail %d\n", error);
		return 0;
	}

	fwsize = firmware_get_size(fh);
	printf("fwsize %zd\n", fwsize);
	blob = firmware_malloc(fwsize);
	if ( blob == NULL ) {
		printf("nxt2k firmware_malloc fail\n");
		firmware_close(fh);
		return -1;
	}

	error = firmware_read(fh, 0, blob, fwsize);
	if (error != 0) {
		printf("nxt2k firmware_read fail %d\n", error);
		firmware_free(blob, fwsize);
		firmware_close(fh);
		return -1;
	}

	/* calculate CRC */	
	crc = 0;
	for(position = 0; position < fwsize; position++) {
		crc = nxt2k_crc_ccit(crc, blob[position]);
	}
	printf("nxt2k firmware crc is %02x\n", crc);

	uint16_t rambase;
	uint8_t buf[3];

	rambase = 0x1000;

	/* hold the micro in reset while loading firmware */
	buf[0] = 0x80;
	nxt2k_writedata(nxt, 0x2b, buf, 1);

	buf[0] = rambase >> 8;
	buf[1] = rambase & 0xFF;
	buf[2] = 0x81;
	/* write starting address */
	nxt2k_writedata(nxt, 0x29, buf, 3);

	position = 0;

	size_t xfercnt;

	while ( position < fwsize ) {
		xfercnt = fwsize - position > 255 ? 255 : fwsize - position;
		nxt2k_writedata(nxt, 0x2c, &blob[position], xfercnt);
		position += xfercnt;
	}

	/* write crc */
        buf[0] = crc >> 8;
        buf[1] = crc & 0xFF;
	nxt2k_writedata(nxt, 0x2c, buf, 2);

	/* do a read to stop things */
	nxt2k_readdata(nxt, 0x2c, buf, 1);
	
	/* set transfer mode to complete */
	buf[0] = 0x80;
	nxt2k_writedata(nxt, 0x2b, buf, 1);
	
	firmware_free(blob, fwsize);
	firmware_close(fh);

	return 1;
}

static int 
nxt2k4_init(struct nxt2k *nxt)
{
	int success;
	uint8_t buf[3];

	buf[0] = 0x00;
	nxt2k_writedata(nxt, 0x1e, buf, 1);

	/* try to load firmware */
	nxt->loaded = nxt2k4_load_firmware(nxt);
	if (nxt->loaded == false)
		return ECANCELED;

	/* ensure transfer is complete */
	buf[0] = 0x01;
	nxt2k_writedata(nxt, 0x19, buf, 1);

	nxt2k4_mc_init(nxt);
	nxt2k_mc_stop(nxt);
	nxt2k_mc_stop(nxt);
	nxt2k4_mc_init(nxt);
	nxt2k_mc_stop(nxt);

	buf[0] = 0xff;
	nxt2k_writereg(nxt, 0x08, buf, 1);
	buf[0] = 0x00;
	nxt2k_writereg(nxt, 0x08, buf, 1);

	buf[0] = 0xD7;
	nxt2k_writedata(nxt, 0xd7, buf, 1);

	buf[0] = 0x07;
	buf[1] = 0xfe;
	nxt2k_writedata(nxt, 0x35, buf, 2);
	buf[0] = 0x12;
	nxt2k_writedata(nxt, 0x34, buf, 1);
	buf[0] = 0x80;
	nxt2k_writedata(nxt, 0x21, buf, 1);

	buf[0] = 0x21;
	nxt2k_writedata(nxt, 0x0a, buf, 1);

	buf[0] = 0x01;
	nxt2k_writereg(nxt, 0x80, buf, 1);

	/* fec mpeg mode */
	buf[0] = 0x7E;
	buf[1] = 0x00;
	nxt2k_writedata(nxt, 0xe9, buf, 2);

	/* mux selection */
	buf[0] = 0x00;
	nxt2k_writedata(nxt, 0xcc, buf, 1);

	/* */
	nxt2k_readreg(nxt, 0x80, buf, 1);
	buf[0] = 0x00;
	nxt2k_writereg(nxt, 0x80, buf, 1);

	/* soft reset? */	
	nxt2k_readreg(nxt, 0x08, buf, 1);
	buf[0] = 0x10;
	nxt2k_writereg(nxt, 0x08, buf, 1);
	nxt2k_readreg(nxt, 0x08, buf, 1);
	buf[0] = 0x00;
	nxt2k_writereg(nxt, 0x08, buf, 1);
	
	/* */
	nxt2k_readreg(nxt, 0x80, buf, 1);
	buf[0] = 0x01;
	nxt2k_writereg(nxt, 0x80, buf, 1);
	buf[0] = 0x70;
	nxt2k_writereg(nxt, 0x81, buf, 1);
	buf[0] = 0x31; buf[1] = 0x5E; buf[2] = 0x66;
	nxt2k_writereg(nxt, 0x82, buf, 3);

	nxt2k_readreg(nxt, 0x88, buf, 1);
	buf[0] = 0x11;
	nxt2k_writereg(nxt, 0x88, buf, 1);
	nxt2k_readreg(nxt, 0x80, buf, 1);
	buf[0] = 0x40;
	nxt2k_writereg(nxt, 0x80, buf, 1);

	nxt2k_readdata(nxt, 0x10, buf, 1);
	buf[0] = 0x10;
	nxt2k_writedata(nxt, 0x10, buf, 1);
	nxt2k_readdata(nxt, 0x0a, buf, 1);
	buf[0] = 0x21;
	nxt2k_writedata(nxt, 0x0a, buf, 1);
	
	nxt2k4_mc_init(nxt);

	buf[0] = 0x21;
	nxt2k_writedata(nxt, 0x0a, buf, 1);
	buf[0] = 0x7e;
	nxt2k_writedata(nxt, 0xe9, buf, 1);
	buf[0] = 0x00;
	nxt2k_writedata(nxt, 0xea, buf, 1);

	nxt2k_readreg(nxt, 0x80, buf, 1);
	buf[0] = 0x00;
	nxt2k_writereg(nxt, 0x80, buf, 1);
	nxt2k_readreg(nxt, 0x80, buf, 1);
	buf[0] = 0x00;
	nxt2k_writereg(nxt, 0x80, buf, 1);

	nxt2k_readreg(nxt, 0x08, buf, 1);
	buf[0] = 0x10;
	nxt2k_writereg(nxt, 0x08, buf, 1);
	nxt2k_readreg(nxt, 0x08, buf, 1);
	buf[0] = 0x00;
	nxt2k_writereg(nxt, 0x08, buf, 1);

	nxt2k_readreg(nxt, 0x80, buf, 1);
	buf[0] = 0x04;
	nxt2k_writereg(nxt, 0x80, buf, 1);
	buf[0] = 0x00;
	nxt2k_writereg(nxt, 0x81, buf, 1);
	buf[0] = 0x80; buf[1] = 0x00; buf[2] = 0x00;
	nxt2k_writereg(nxt, 0x82, buf, 3);

	nxt2k_readreg(nxt, 0x88, buf, 1);
	buf[0] = 0x11;
	nxt2k_writereg(nxt, 0x88, buf, 1);

	nxt2k_readreg(nxt, 0x80, buf, 1);
	buf[0] = 0x44;
	nxt2k_writereg(nxt, 0x80, buf, 1);

	/* init tuner */
	nxt2k_readdata(nxt, 0x10, buf, 1);
	buf[0] = 0x12;
	nxt2k_writedata(nxt, 0x10, buf,1);
	buf[0] = 0x04;
	nxt2k_writedata(nxt, 0x13, buf,1);
	buf[0] = 0x00;
	nxt2k_writedata(nxt, 0x16, buf,1);
	buf[0] = 0x04;
	nxt2k_writedata(nxt, 0x14, buf,1);
	buf[0] = 0x00;
	nxt2k_writedata(nxt, 0x14, buf,1);
	nxt2k_writedata(nxt, 0x17, buf,1);
	nxt2k_writedata(nxt, 0x14, buf,1);
	nxt2k_writedata(nxt, 0x17, buf,1);

	success = 1;
	return success;
}

uint16_t
nxt2k_get_signal(struct nxt2k *nxt)
{
	uint16_t temp;
	uint8_t b[2];
	
	b[0] = 0x00;
	nxt2k_writedata(nxt, 0xa1, b, 1);

	nxt2k_readreg(nxt, 0xa6, b, 2);

	temp = (b[0] << 8) | b[1];

	printf("a6: %04hx\n", temp);

	return 0x7fff - temp * 16;
}

uint16_t
nxt2k_get_snr(struct nxt2k *nxt)
{
	uint32_t tsnr;
	uint16_t temp, temp2;
	uint8_t b[2];

	b[0] = 0x00;
	nxt2k_writedata(nxt, 0xa1, b, 1);

	nxt2k_readreg(nxt, 0xa6, b, 2);
	
	temp = (b[0] << 8) | b[1];

	temp2 = 0x7fff - temp;

	printf("snr temp2: %04hx\n", temp2);

	if (temp2 > 0x7f00)
		tsnr = 1000*24+(1000*(30-24)*(temp2-0x7f00)/(0x7fff-0x7f00));
	else if ( temp2 > 0x7ec0)
		tsnr = 1000*18+(1000*(24-18)*(temp2-0x7ec0)/(0x7f00-0x7ec0));
	else if ( temp2 > 0x7c00)
		tsnr = 1000*12+(1000*(18-12)*(temp2-0x7c00)/(0x7ec0-0x7c00));
	else
		tsnr = 1000*0+(1000*(12-0)*(temp2-0)/(0x7c00-0));

	printf("snr tsnr: %08x\n", tsnr);

	return ((tsnr * 0xffff)/32000);
}

fe_status_t
nxt2k_get_dtv_status(struct nxt2k *nxt)
{
	uint8_t reg;
	fe_status_t status = 0;

	nxt2k_readdata(nxt, 0x31, &reg, 1);
	if (reg & 0x20) {
		status = FE_HAS_SIGNAL | FE_HAS_CARRIER | FE_HAS_VITERBI |
		    FE_HAS_SYNC | FE_HAS_LOCK;
	}

	return status;
}

#if notyet
int
nxt2k_fe_read_ucblocks(struct nxt2k *nxt, uint32_t *ucblk)
{
	uint8_t reg[3];
	
	nxt2k_readreg(nxt, 0xe6, reg, 3);
	*ucblk = reg[2];

	return 0;
}

int
nxt2k_fe_read_ber(struct nxt2k *nxt, uint32_t *ber)
{
	uint8_t reg[3];

	nxt2k_readreg(nxt, 0xe6, reg, 3);

	*ber = ((reg[0] << 8) + reg[1]) * 8;

	return 0;
}
#endif

static int
nxt2k_fe_set_frontend(struct nxt2k *nxt, fe_modulation_t modulation)
{
	uint8_t buf[5];

	if (nxt->loaded != true)
		nxt2k4_init(nxt);

	nxt2k_mc_stop(nxt);

	{ /* 2k4 */
	/* make sure demod is set to digital */
	buf[0] = 0x04;
	nxt2k_writedata(nxt, 0x14, buf, 1);
	buf[0] = 0x00;
	nxt2k_writedata(nxt, 0x17, buf, 1);
	}

	/* QAM/VSB punctured/non-punctured goes here */

	/* tune in */
	/* maybe ensure tuner managed to tune in? */

	/* tuning done, reset agc */
	nxt2k_agc_reset(nxt);

	/* set target power level */
	switch (modulation) {
	case VSB_8:
		buf[0] = 0x70;
		break;
	case QAM_256:
	case QAM_64:
		buf[0] = 0x74;
		break;
	default:
		return EINVAL;
		/* NOTREACHED */
	}
	nxt2k_writedata(nxt, 0x42, buf, 1);

	/* configure sdm */
	buf[0] = 0x07; /* 2k4 */
	nxt2k_writedata(nxt, 0x57, buf, 1);

	/* write sdm1 input */
        buf[0] = 0x10;
        buf[1] = 0x00;
	nxt2k_writedata(nxt, 0x58, buf, 2); /* 2k4 */

	/* write sdmx input */
	switch (modulation) {
	case VSB_8:
		buf[0] = 0x60;
		break;
	case QAM_256:
		buf[0] = 0x64;
		break;
	case QAM_64:
		buf[0] = 0x68;
		break;
	default:
		return EINVAL;
		/* NOTREACHED */
	}
        buf[1] = 0x00;
	nxt2k_writedata(nxt, 0x5c, buf, 2); /* 2k4 */

	/* write adc power lpf fc */
	buf[0] = 0x05;
	nxt2k_writedata(nxt, 0x43, buf, 1);
	
	{ /* 2k4 */
	buf[0] = 0x00;
	buf[1] = 0x00;
	nxt2k_writedata(nxt, 0x46, buf, 2);
	}

	/* write accumulator2 input */
	buf[0] = 0x80;
	buf[1] = 0x00;
	nxt2k_writedata(nxt, 0x4b, buf, 2); /* 2k4 */

	/* write kg1 */
	buf[0] = 0x00;
	nxt2k_writedata(nxt, 0x4d, buf, 1);

	/* write sdm12 lpf fc */
	buf[0] = 0x44;
	nxt2k_writedata(nxt, 0x55, buf, 1);

	/* write agc control reg */
	buf[0] = 0x04;
	nxt2k_writedata(nxt, 0x41, buf, 1);
	
	{ /* 2k4 */
	nxt2k_readreg(nxt, 0x80, buf, 1);
	buf[0] = 0x24;
	nxt2k_writereg(nxt, 0x80, buf, 1);
	
	/* soft reset? */
	nxt2k_readreg(nxt, 0x08, buf, 1);
	buf[0] = 0x10;
	nxt2k_writereg(nxt, 0x08, buf, 1);
	nxt2k_readreg(nxt, 0x08, buf, 1);
	buf[0] = 0x00;
	nxt2k_writereg(nxt, 0x08, buf, 1);
	
	nxt2k_readreg(nxt, 0x80, buf, 1);
	buf[0] = 0x04;
	nxt2k_writereg(nxt, 0x80, buf, 1);

	buf[0] = 0x00;
	nxt2k_writereg(nxt, 0x81, buf, 1);

	buf[0] = 0x80; buf[1] = 0x00; buf[2] = 0x00;
	nxt2k_writereg(nxt, 0x82, buf, 3);
	
	nxt2k_readreg(nxt, 0x88, buf, 1);
	buf[0] = 0x11;
	nxt2k_writereg(nxt, 0x88, buf, 1);

	nxt2k_readreg(nxt, 0x80, buf, 1);
	buf[0] = 0x44;
	nxt2k_writereg(nxt, 0x80, buf, 1);
	}

	/* write agc ucgp0 */
	switch (modulation) {
	case VSB_8:
		buf[0] = 0x00;
		break;
	case QAM_64:
		buf[0] = 0x02;
		break;
	case QAM_256:
		buf[0] = 0x03;
		break;
	default:
		return EINVAL;
		/* NOTREACHED */
	}
	nxt2k_writedata(nxt, 0x30, buf, 1);
	
	/* write agc control reg */
	buf[0] = 0x00;
	nxt2k_writedata(nxt, 0x41, buf, 1);

	/* write accumulator2 input */
	buf[0] = 0x80;
	buf[1] = 0x00;
	{ /* 2k4 */
	nxt2k_writedata(nxt, 0x49, buf, 2);
	nxt2k_writedata(nxt, 0x4b, buf, 2);
	}
	
	/* write agc control reg */
	buf[0] = 0x04;
	nxt2k_writedata(nxt, 0x41, buf, 1);
	
	nxt2k_mc_start(nxt);
	
	{ /* 2k4 */
	nxt2k4_mc_init(nxt);
	buf[0] = 0xf0;
	buf[1] = 0x00;
	nxt2k_writedata(nxt, 0x5c, buf, 2);
	}

	/* "adjacent channel detection" code would go here */

	return 0;
}

static int
nxt2k_init(struct nxt2k *nxt)
{
	int ret = 0;

	printf("%s\n", __func__);

	if (nxt->loaded != 1)
		ret = nxt2k4_init(nxt);

	return ret;
}


struct nxt2k *
nxt2k_open(device_t parent, i2c_tag_t tag, i2c_addr_t addr, unsigned int if_freq)
{
	struct nxt2k *nxt;
	int e;
	uint8_t b[5];

	nxt = kmem_alloc(sizeof(*nxt), KM_SLEEP);
	if (nxt == NULL)
		return NULL;

	nxt->parent = parent;
	nxt->tag = tag;
	nxt->addr = addr;

	/* read chip ids */
	e = nxt2k_readdata(nxt, 0x00, b, 5);

	if (e) {
		printf("%s read failed %d\n", __func__, e);
		kmem_free(nxt, sizeof(*nxt));
		return NULL;
	}

	if (b[0] != 0x05) {
		printf("%s unsupported %02x %02x %02x %02x %02x\n",
		    __func__, b[0], b[1], b[2], b[3], b[4]);
		kmem_free(nxt, sizeof(*nxt));
		return NULL;
	}

	mutex_init(&nxt->mtx, MUTEX_DEFAULT, IPL_NONE);
	cv_init(&nxt->cv, "nxtpl");

	nxt->loaded = false;

	return nxt;
}

void
nxt2k_close(struct nxt2k *nxt)
{
	kmem_free(nxt, sizeof(*nxt));
}

void
nxt2k_enable(struct nxt2k *nxt, bool enable)
{
	if (enable == true)
		nxt2k_init(nxt);
}

int
nxt2k_set_modulation(struct nxt2k *nxt, fe_modulation_t modulation)
{
	return nxt2k_fe_set_frontend(nxt, modulation);
}

MODULE(MODULE_CLASS_DRIVER, nxt2k, "i2cexec");

static int
nxt2k_modcmd(modcmd_t cmd, void *opaque)
{
	if (cmd == MODULE_CMD_INIT || cmd == MODULE_CMD_FINI)
		return 0;
	return ENOTTY;
}
