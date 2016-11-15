/*	$NetBSD: i2c_exec.c,v 1.10 2015/03/07 14:16:51 jmcneill Exp $	*/

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: i2c_exec.c,v 1.10 2015/03/07 14:16:51 jmcneill Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/module.h>
#include <sys/event.h>
#include <sys/conf.h>

#define	_I2C_PRIVATE
#include <dev/i2c/i2cvar.h>

static uint8_t	iic_smbus_crc8(uint16_t);
static uint8_t	iic_smbus_pec(int, uint8_t *, uint8_t *);

static int	i2cexec_modcmd(modcmd_t, void *);

/*
 * iic_exec:
 *
 *	Simplified I2C client interface engine.
 *
 *	This and the SMBus routines are the preferred interface for
 *	client access to I2C/SMBus, since many automated controllers
 *	do not provide access to the low-level primitives of the I2C
 *	bus protocol.
 */
int
iic_exec(i2c_tag_t tag, i2c_op_t op, i2c_addr_t addr, const void *vcmd,
    size_t cmdlen, void *vbuf, size_t buflen, int flags)
{
	const uint8_t *cmd = vcmd;
	uint8_t *buf = vbuf;
	int error;
	size_t len;

	if ((flags & I2C_F_PEC) && cmdlen > 0 && tag->ic_exec != NULL) {
		uint8_t data[33]; /* XXX */
		uint8_t b[3];

		b[0] = addr << 1;
		b[1] = cmd[0];

		switch (buflen) {
		case 0:
			data[0] = iic_smbus_pec(2, b, NULL);
			buflen++;
			break;
		case 1:
			b[2] = buf[0];
			data[0] = iic_smbus_pec(3, b, NULL);
			data[1] = b[2];
			buflen++;
			break;
		case 2:
			break;
		default:
			KASSERT(buflen+1 < sizeof(data));
			memcpy(data, vbuf, buflen);
			data[buflen] = iic_smbus_pec(2, b, data);
			buflen++;
			break;
		}

		return ((*tag->ic_exec)(tag->ic_cookie, op, addr, cmd,
					cmdlen, data, buflen, flags));
	}

	/*
	 * Defer to the controller if it provides an exec function.  Use
	 * it if it does.
	 */
	if (tag->ic_exec != NULL)
		return ((*tag->ic_exec)(tag->ic_cookie, op, addr, cmd,
					cmdlen, buf, buflen, flags));

	if ((len = cmdlen) != 0) {
		if ((error = iic_initiate_xfer(tag, addr, flags)) != 0)
			goto bad;
		while (len--) {
			if ((error = iic_write_byte(tag, *cmd++, flags)) != 0)
				goto bad;
		}
	} else if (buflen == 0) {
		/*
		 * This is a quick_read()/quick_write() command with
		 * neither command nor data bytes
		 */
		if (I2C_OP_STOP_P(op))
			flags |= I2C_F_STOP;
		if (I2C_OP_READ_P(op))
			flags |= I2C_F_READ;
		if ((error = iic_initiate_xfer(tag, addr, flags)) != 0)
			goto bad;
	}

	if (I2C_OP_READ_P(op))
		flags |= I2C_F_READ;

	len = buflen;
	while (len--) {
		if (len == 0 && I2C_OP_STOP_P(op))
			flags |= I2C_F_STOP;
		if (I2C_OP_READ_P(op)) {
			/* Send REPEATED START. */
			if ((len + 1) == buflen &&
			    (error = iic_initiate_xfer(tag, addr, flags)) != 0)
				goto bad;
			/* NACK on last byte. */
			if (len == 0)
				flags |= I2C_F_LAST;
			if ((error = iic_read_byte(tag, buf++, flags)) != 0)
				goto bad;
		} else  {
			/* Maybe send START. */
			if ((len + 1) == buflen && cmdlen == 0 &&
			    (error = iic_initiate_xfer(tag, addr, flags)) != 0)
				goto bad;
			if ((error = iic_write_byte(tag, *buf++, flags)) != 0)
				goto bad;
		}
	}

	return (0);
 bad:
	iic_send_stop(tag, flags);
	return (error);
}

/*
 * iic_smbus_write_byte:
 *
 *	Perform an SMBus "write byte" operation.
 */
int
iic_smbus_write_byte(i2c_tag_t tag, i2c_addr_t addr, uint8_t cmd,
    uint8_t val, int flags)
{

	return (iic_exec(tag, I2C_OP_WRITE_WITH_STOP, addr, &cmd, 1,
			 &val, 1, flags));
}

/*
 * iic_smbus_write_word:
 *
 *	Perform an SMBus "write word" operation.
 */
int
iic_smbus_write_word(i2c_tag_t tag, i2c_addr_t addr, uint8_t cmd,
    uint16_t val, int flags)
{
	uint8_t vbuf[2];

	vbuf[0] = val & 0xff;
	vbuf[1] = (val >> 8) & 0xff;

	return (iic_exec(tag, I2C_OP_WRITE_WITH_STOP, addr, &cmd, 1,
			 vbuf, 2, flags));
}

/*
 * iic_smbus_read_byte:
 *
 *	Perform an SMBus "read byte" operation.
 */
int
iic_smbus_read_byte(i2c_tag_t tag, i2c_addr_t addr, uint8_t cmd,
    uint8_t *valp, int flags)
{

	return (iic_exec(tag, I2C_OP_READ_WITH_STOP, addr, &cmd, 1,
			 valp, 1, flags));
}

/*
 * iic_smbus_read_word:
 *
 *	Perform an SMBus "read word" operation.
 */
int
iic_smbus_read_word(i2c_tag_t tag, i2c_addr_t addr, uint8_t cmd,
    uint16_t *valp, int flags)
{

	return (iic_exec(tag, I2C_OP_READ_WITH_STOP, addr, &cmd, 1,
			 (uint8_t *)valp, 2, flags));
}

/*
 * iic_smbus_receive_byte:
 *
 *	Perform an SMBus "receive byte" operation.
 */
int
iic_smbus_receive_byte(i2c_tag_t tag, i2c_addr_t addr, uint8_t *valp,
    int flags)
{

	return (iic_exec(tag, I2C_OP_READ_WITH_STOP, addr, NULL, 0,
			 valp, 1, flags));
}

/*
 * iic_smbus_send_byte:
 *
 *	Perform an SMBus "send byte" operation.
 */
int
iic_smbus_send_byte(i2c_tag_t tag, i2c_addr_t addr, uint8_t val, int flags)
{

	return (iic_exec(tag, I2C_OP_WRITE_WITH_STOP, addr, NULL, 0,
			 &val, 1, flags));
}

/*
 * iic_smbus_quick_read:
 *
 *	Perform an SMBus "quick read" operation.
 */
int
iic_smbus_quick_read(i2c_tag_t tag, i2c_addr_t addr, int flags)
{

	return (iic_exec(tag, I2C_OP_READ_WITH_STOP, addr, NULL, 0,
			 NULL, 0, flags));
}

/*
 * iic_smbus_quick_write:
 *
 *	Perform an SMBus "quick write" operation.
 */
int
iic_smbus_quick_write(i2c_tag_t tag, i2c_addr_t addr, int flags)
{

	return (iic_exec(tag, I2C_OP_WRITE_WITH_STOP, addr, NULL, 0,
			 NULL, 0, flags));
}

/*
 * iic_smbus_block_read:
 *
 *	Perform an SMBus "block read" operation.
 */
int
iic_smbus_block_read(i2c_tag_t tag, i2c_addr_t addr, uint8_t cmd,
    uint8_t *vbuf, size_t buflen, int flags)
{

	return (iic_exec(tag, I2C_OP_READ_BLOCK, addr, &cmd, 1,
			 vbuf, buflen, flags));
}

/*
 * iic_smbus_block_write:
 *
 *	Perform an SMBus "block write" operation.
 */
int
iic_smbus_block_write(i2c_tag_t tag, i2c_addr_t addr, uint8_t cmd,
    uint8_t *vbuf, size_t buflen, int flags)
{

	return (iic_exec(tag, I2C_OP_WRITE_BLOCK, addr, &cmd, 1,
			 vbuf, buflen, flags));
}

/*
 * iic_smbus_crc8
 *
 *	Private helper for calculating packet error checksum
 */
static uint8_t
iic_smbus_crc8(uint16_t data)
{
	int i;

	for (i = 0; i < 8; i++) {
		if (data & 0x8000)
			data = data ^ (0x1070U << 3);
		data = data << 1; 
	}

	return (uint8_t)(data >> 8);
}

/*
 * iic_smbus_pec
 *
 *	Private function for calculating packet error checking on SMBus
 *	packets.
 */
static uint8_t
iic_smbus_pec(int count, uint8_t *s, uint8_t *r)
{
	int i;
	uint8_t crc = 0;

	for (i = 0; i < count; i++)
		crc = iic_smbus_crc8((crc ^ s[i]) << 8);
	if (r != NULL)
		for (i = 0; i <= r[0]; i++)
			crc = iic_smbus_crc8((crc ^ r[i]) << 8);

	return crc;
}

MODULE(MODULE_CLASS_MISC, i2cexec, NULL);

static int
i2cexec_modcmd(modcmd_t cmd, void *opaque)
{
	switch (cmd) {
	case MODULE_CMD_INIT:
	case MODULE_CMD_FINI:
		return 0;
		break;
	default:
		return ENOTTY;
	}
}
