/* $NetBSD: hdafg_dd.h,v 1.1 2015/03/28 14:09:59 jmcneill Exp $ */

/*
 * Copyright (c) 2011 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _HDAFG_DD_H
#define _HDAFG_DD_H

#include "hdmireg.h"
#include "eldreg.h"
#include "ceareg.h"

#define	HDAFG_DD_MONITOR_NAME_LEN	32
#define	HDAFG_DD_MAX_SAD		15

struct hdafg_dd_info {
	struct eld_baseline_block eld;
	char		monitor[HDAFG_DD_MONITOR_NAME_LEN];
	size_t		nsad;
	struct cea_sad	sad[HDAFG_DD_MAX_SAD];
};

int	hdafg_dd_parse_info(uint8_t *, size_t, struct hdafg_dd_info *);
void	hdafg_dd_hdmi_ai_cksum(struct hdmi_audio_infoframe *);


#endif /* !_HDAFG_DD_H */
