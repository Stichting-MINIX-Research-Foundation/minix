/*	$NetBSD: ofisa_machdep.h,v 1.3 2012/10/27 17:17:39 chs Exp $	*/

/*
 * Copyright 1998
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

int	ofisa_get_isabus_data(int, struct isabus_attach_args *);
int	ofisa_ignore_child(int pphandle, int cphandle);

#if defined(_KERNEL_OPT)
#include "opt_compat_old_ofw.h"
#endif

#ifdef COMPAT_OLD_OFW

#define	_OFISA_MD_MATCH
int	ofisa_md_match(device_t, cfdata_t, void *);

#define	_COM_OFISA_MD_MATCH
#define	_COM_OFISA_MD_INTR_FIXUP
int	com_ofisa_md_match(device_t, cfdata_t, void *);
int	com_ofisa_md_intr_fixup(device_t, device_t, void *,
	    struct ofisa_intr_desc *, int, int);

#define	_CS_OFISA_MD_MATCH
#define	_CS_OFISA_MD_REG_FIXUP
#define	_CS_OFISA_MD_INTR_FIXUP
#define	_CS_OFISA_MD_DMA_FIXUP
#define	_CS_OFISA_MD_MEDIA_FIXUP
int	cs_ofisa_md_match(device_t, cfdata_t, void *);
int	cs_ofisa_md_reg_fixup(device_t, device_t, void *,
	    struct ofisa_reg_desc *, int, int);
int	cs_ofisa_md_intr_fixup(device_t, device_t, void *,
	    struct ofisa_intr_desc *, int, int);
int	cs_ofisa_md_dma_fixup(device_t, device_t, void *,
	    struct ofisa_dma_desc *, int, int);
int	*cs_ofisa_md_media_fixup(device_t, device_t, void *,
	    int *, int *, int *);

#define	_LPT_OFISA_MD_MATCH
#define	_LPT_OFISA_MD_INTR_FIXUP
int	lpt_ofisa_md_match(device_t, cfdata_t, void *);
int	lpt_ofisa_md_intr_fixup(device_t, device_t, void *,
	    struct ofisa_intr_desc *, int, int);

#define	_WDC_OFISA_MD_MATCH
#define	_WDC_OFISA_MD_INTR_FIXUP
int	wdc_ofisa_md_match(device_t, cfdata_t, void *);
int	wdc_ofisa_md_intr_fixup(device_t, device_t, void *,
	    struct ofisa_intr_desc *, int, int);

#endif /* COMPAT_OLD_OFW */

/* The following aren't dependent on old OpenFirmware. */
#define	_CS_OFISA_MD_CFGFLAGS_FIXUP
int	cs_ofisa_md_cfgflags_fixup(device_t, device_t, void *);
