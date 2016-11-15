/* $NetBSD: ibmhawkvar.h,v 1.1 2011/02/14 08:50:39 hannken Exp $ */

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

#define IBMHAWK_T_AMBIENT	0
#define IBMHAWK_T_CPU		1
#define IBMHAWK_V_VOLTAGE	(IBMHAWK_T_CPU+IBMHAWK_MAX_CPU)
#define IBMHAWK_F_FAN		(IBMHAWK_V_VOLTAGE+IBMHAWK_MAX_VOLTAGE)
#define IBMHAWK_MAX_SENSOR	(IBMHAWK_F_FAN+IBMHAWK_MAX_FAN)

struct ibmhawk_softc {
	device_t sc_dev;
	i2c_tag_t sc_tag;
	i2c_addr_t sc_addr;
	int sc_numcpus;
	int sc_numfans;
	int sc_refresh;
	struct sysmon_envsys *sc_sme;
	struct ibmhawk_sensordata {
		envsys_data_t ihs_edata;
		uint32_t ihs_warnmin;
		uint32_t ihs_warnmax;
	} sc_sensordata[IBMHAWK_MAX_SENSOR];
};
