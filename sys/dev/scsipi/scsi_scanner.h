/*	$NetBSD: scsi_scanner.h,v 1.11 2005/12/11 12:23:50 christos Exp $	*/

/*
 * Copyright (c) 1995 Kenneth Stailey.  All rights reserved.
 *   modified for configurable scanner support by Joachim Koenig
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
 *	This product includes software developed by Kenneth Stailey.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * SCSI scanner interface description
 */

/* SCSI scanner commands */
#define GET_IMAGE_STATUS	0x0f
#define READ_10			0x28
#define WRITE_10		0x2a
#define OBJECT_POSITION		0x31
#define GET_BUFFER_STATUS	0x34

/* generic scanner command formats */

struct scsi_rw_scanner {
#define	READ			0x08
#define WRITE			0x0a
	u_int8_t opcode;
	u_int8_t byte2;
#define	SRW_FIXED		0x01
	u_int8_t len[3];
	u_int8_t control;
};

struct scsi_set_window {
#define SET_WINDOW		0x24 /* set params of image area and windows */
#define GET_WINDOW		0x25
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t reserved[4];
	u_int8_t len[3];
	u_int8_t control;
};

struct scsi_window_header {
	u_int8_t reserved[6];
	u_int8_t len[2];
};

struct scsi_window_data {
	u_int8_t window_id;	    /* must be zero */
	u_int8_t res1:7;
	u_int8_t auto_bit:1;
	u_int8_t x_res[2];
	u_int8_t y_res[2];
	u_int8_t x_org[4];
	u_int8_t y_org[4];
	u_int8_t width[4];
	u_int8_t length[4];
	u_int8_t brightness;
	u_int8_t threshold;
	u_int8_t contrast;
	u_int8_t image_comp;	    /* image composition (data type) */
	u_int8_t bits_per_pixel;
	u_int8_t halftone_pattern[2];
	u_int8_t rif:1;		    /* reverse image format (mono negative) */
	u_int8_t res2:4;
	u_int8_t pad_type:3;
	u_int8_t bit_ordering[2];
	u_int8_t compression_type;
	u_int8_t compression_arg;
	u_int8_t res3[6];
};

/* mustek scsi commands */

#define MUSTEK_SET_WINDOW	0x04	/* set image area and windows */
#define MUSTEK_ADF		0x10	/* ADF and backtracking selection */
#define MUSTEK_LUT		0x55	/* look up table download */
