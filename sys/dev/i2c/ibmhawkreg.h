/* $NetBSD: ibmhawkreg.h,v 1.1 2011/02/14 08:50:39 hannken Exp $ */

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
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

#define IBMHAWK_MAX_CPU		2
#define IBMHAWK_MAX_VOLTAGE	7
#define IBMHAWK_MAX_FAN		6

typedef union ibmhawk_response {
	struct {
		uint8_t unknown1;
		uint8_t numcpus;
		uint8_t unknown2[5];
		uint8_t numfans;
	} resp_equip;
#define IHR_EQUIP		0x51
#define ihr_numcpus		resp_equip.numcpus
#define ihr_numfans		resp_equip.numfans
	struct {
		uint8_t unknown[4];
		uint16_t rpm[IBMHAWK_MAX_FAN];
	} resp_fan;
#define IHR_FANRPM		0xa2
#define ihr_fanrpm		resp_fan.rpm
	struct {
		char name[16];
	} resp_name;
#define IHR_NAME		0x14
#define ihr_name		resp_name.name
	struct {
		uint8_t unknown1[4];
		uint8_t ambient;
		uint8_t cpu[IBMHAWK_MAX_CPU];
		uint8_t unknown2[2];
	} resp_temp;
#define IHR_TEMP		0xa4
#define ihr_t_ambient		resp_temp.ambient
#define ihr_t_cpu		resp_temp.cpu
	struct {
		uint8_t	warn_reset;
		uint8_t warn;
		uint8_t soft;
		uint8_t hard;
	} resp_temp_thresh;
#define IHR_TEMP_THR		0xe0
#define ihr_t_warn_thr		resp_temp_thresh.warn
#define ihr_t_soft_thr		resp_temp_thresh.soft
	struct {
		uint8_t unknown[2];
		uint16_t voltage[IBMHAWK_MAX_VOLTAGE];
	} resp_volt;
#define IHR_VOLT		0xa6
#define ihr_v_voltage		resp_volt.voltage
	struct {
		uint16_t voltage[IBMHAWK_MAX_VOLTAGE*2];
	} resp_volt_thresh;
#define IHR_VOLT_THR		0xea
#define ihr_v_voltage_thr	resp_volt_thresh.voltage
} ibmhawk_response_t __packed;
