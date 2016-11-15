/*	$NetBSD: i2odpt.h,v 1.5 2008/09/08 23:36:54 gmcgarry Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andrew Doran.
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
 * Copyright (c) 1996-2000 Distributed Processing Technology Corporation
 * Copyright (c) 2000 Adaptec Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source form, with or without modification, are
 * permitted provided that redistributions of source code must retain the
 * above copyright notice, this list of conditions and the following disclaimer.
 *
 * This software is provided `as is' by Distributed Processing Technology and
 * any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose,
 * are disclaimed. In no event shall Distributed Processing Technology be
 * liable for any direct, indirect, incidental, special, exemplary or
 * consequential damages (including, but not limited to, procurement of
 * substitute goods or services; loss of use, data, or profits; or business
 * interruptions) however caused and on any theory of liability, whether in
 * contract, strict liability, or tort (including negligence or otherwise)
 * arising in any way out of the use of this driver software, even if advised
 * of the possibility of such damage.
 *
 */

#ifndef _I2O_I2ODPT_H_
#define	_I2O_I2ODPT_H_

/*
 * ================= Messages =================
 */

#define	I2O_DPT_SCSI_SCB_EXEC		I2O_SCSI_SCB_EXEC
struct i2o_dpt_scsi_scb_exec {
	u_int32_t	msgflags;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	privfunc;
	u_int32_t	tid;		/* bit 16: interpret; bit 17: phys */
	u_int32_t	flags;
	u_int32_t	scbflags;
	u_int8_t	cdb[16];
	u_int32_t	bytecount;
} __packed;

#define	I2O_DPT_FLASH_REGION_SIZE	0x0100
#define	I2O_DPT_FLASH_REGION_READ	0x0101
#define	I2O_DPT_FLASH_REGION_WRITE	0x0102
#define	I2O_DPT_FLASH_REGION_CRC	0x0103
struct i2o_dpt_flash_region {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	privfunc;
	u_int32_t	region;
	u_int32_t	regionoffset;
	u_int32_t	bytecount;
} __packed;

#define	DPT_FLASH_REGION_OP_FIRMWARE	0x00
#define	DPT_FLASH_REGION_SOFTWARE	0x01
#define	DPT_FLASH_REGION_OEM_NVRAM	0x02
#define	DPT_FLASH_REGION_SERIAL		0x03
#define	DPT_FLASH_REGION_BOOT_FIRMWARE	0x04

#define	I2O_DPT_DRIVER_PRINTF		0x0200
struct i2o_dpt_driver_printf {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	privfunc;
	u_int32_t	printbuffersize;
	u_int8_t	printbuffer[1];
} __packed;

#define	I2O_DPT_DIAG_ENABLE		0x0201
struct i2o_dpt_diag_enable {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	privfunc;
} __packed;

#define	I2O_DPT_DRIVER_GET		0x0300
struct i2o_dpt_driver_get {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	privfunc;
	u_int32_t	offset;
	u_int32_t	bytecount;

	/* SGL follows. */
} __packed;

#define	I2O_DPT_DRIVER_SET		0x0301
struct i2o_dpt_driver_set {
	u_int32_t	msgflags;
	u_int32_t	msgfunc;
	u_int32_t	msgictx;
	u_int32_t	msgtctx;
	u_int32_t	privfunc;
	u_int32_t	offset;
	u_int32_t	bytecount;

	/* SGL follows. */
} __packed;

/*
 * ================= Parameter groups =================
 */

#define	I2O_DPT_PARAM_DEVICE_INFO	0x8000
struct i2o_dpt_param_device_info {
	u_int8_t	devicetype;	/* as I2O_PARAM_SCSI_DEVICE_INFO */
	u_int8_t	flags;		/* as I2O_PARAM_SCSI_DEVICE_INFO */
	u_int16_t	bus;
	u_int32_t	identifier;
	u_int8_t	luninfo[8];
} __packed;

#define	I2O_DPT_PARAM_EXEC_IOP_BUFFERS	0x8000
struct i2o_dpt_param_exec_iop_buffers {
	u_int32_t	serialoutputoff;
	u_int32_t	serialoutputsize;
	u_int32_t	serialheadersize;
	u_int32_t	serialflagssupported;
} __packed;

#endif	/* _I2O_I2ODPT_H_ */
