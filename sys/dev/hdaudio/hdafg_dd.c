/* $NetBSD: hdafg_dd.c,v 1.1 2015/03/28 14:09:59 jmcneill Exp $ */

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

/*
 * HD audio Digital Display support
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: hdafg_dd.c,v 1.1 2015/03/28 14:09:59 jmcneill Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/bus.h>

#include "hdaudioreg.h"
#include "hdaudiovar.h"
#include "hdafg_dd.h"

int
hdafg_dd_parse_info(uint8_t *data, size_t datalen, struct hdafg_dd_info *hdi)
{
	struct eld_baseline_block *block = &hdi->eld;
	unsigned int i;

	printf("hdafg_dd_parse_info: datalen=%u\n", (unsigned int)datalen);

	memset(hdi, 0, sizeof(*hdi));

	if (datalen < sizeof(block->header)) {
		printf(" no room for header\n");
		return EINVAL;
	}

	memcpy(&block->header, data, sizeof(block->header));
	data += sizeof(block->header);
	datalen -= sizeof(block->header);

	if (datalen < block->header.baseline_eld_len * 4 ||
	    datalen < sizeof(*block) - sizeof(block->header)) {
		printf(" ack!\n");
		return EINVAL;
	}

	datalen = block->header.baseline_eld_len * 4;

	memcpy(&block->flags[0], data, sizeof(*block) - sizeof(block->header));
	data += sizeof(*block) - sizeof(block->header);
	datalen -= sizeof(*block) - sizeof(block->header);

	if (datalen < ELD_MNL(block)) {
		printf(" MNL=%u\n", ELD_MNL(block));
		return EINVAL;
	}

	memcpy(hdi->monitor, data, ELD_MNL(block));
	data += ELD_MNL(block);
	datalen -= ELD_MNL(block);

	if (datalen != ELD_SAD_COUNT(block) * sizeof(hdi->sad[0])) {
		printf(" datalen %u sadcount %u sizeof sad %u\n",
		    (unsigned int)datalen,
		    ELD_SAD_COUNT(block),
		    (unsigned int)sizeof(hdi->sad[0]));
		return EINVAL;
	}
	hdi->nsad = ELD_SAD_COUNT(block);
	for (i = 0; i < hdi->nsad; i++) {
		memcpy(&hdi->sad[i], data, sizeof(hdi->sad[i]));
		data += sizeof(hdi->sad[i]);
		datalen -= sizeof(hdi->sad[i]);
	}

	printf("datalen = %u\n", (unsigned int)datalen);
	KASSERT(datalen == 0);

	return 0;
}

void
hdafg_dd_hdmi_ai_cksum(struct hdmi_audio_infoframe *hdmi)
{
	uint8_t *dip = (uint8_t *)hdmi, c = 0;
	int i;

	hdmi->checksum = 0;
	for (i = 0; i < sizeof(*hdmi); i++)
		c += dip[i];
	hdmi->checksum = -c;
}
