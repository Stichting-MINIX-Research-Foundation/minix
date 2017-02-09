/*	$NetBSD: azalia_codec.c,v 1.79 2011/11/23 23:07:35 jmcneill Exp $	*/

/*-
 * Copyright (c) 2005, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by TAMURA Kent
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: azalia_codec.c,v 1.79 2011/11/23 23:07:35 jmcneill Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/null.h>
#include <sys/systm.h>

#include <dev/pci/azalia.h>

#ifdef MAX_VOLUME_255
# define MIXER_DELTA(n)	(AUDIO_MAX_GAIN / (n))
#else
# define MIXER_DELTA(n)	(1)
#endif
#define AZ_CLASS_INPUT		0
#define AZ_CLASS_OUTPUT		1
#define AZ_CLASS_RECORD		2
#define AZ_CLASS_PLAYBACK	3
#define AZ_CLASS_MIXER		4
#define AzaliaCplayback	"playback"
#define AzaliaCmixer	"mix"
#define AzaliaNfront	"front"
#define AzaliaNclfe	"clfe"
#define AzaliaNside	"side"
#define AzaliaNdigital	"spdif"
#define ENUM_OFFON	.un.e={2, {{{AudioNoff, 0}, 0}, {{AudioNon, 0}, 1}}}
#define ENUM_IO		.un.e={2, {{{AudioNinput, 0}, 0}, {{AudioNoutput, 0}, 1}}}
#define ENUM_V		.un.v={{ "", 0 }, 0, 0 }
#define AZ_MIXER_CLASSES	\
	{{AZ_CLASS_INPUT, {AudioCinputs, 0}, AUDIO_MIXER_CLASS, \
	AZ_CLASS_INPUT, 0, 0,  ENUM_V }, 0, 0, }, \
	{{AZ_CLASS_OUTPUT, {AudioCoutputs, 0}, AUDIO_MIXER_CLASS, \
	AZ_CLASS_OUTPUT, 0, 0, ENUM_V }, 0, 0, }, \
	{{AZ_CLASS_RECORD, {AudioCrecord, 0}, AUDIO_MIXER_CLASS, \
	AZ_CLASS_RECORD, 0, 0, ENUM_V }, 0, 0, }, \
	{{AZ_CLASS_PLAYBACK, {AzaliaCplayback, 0}, AUDIO_MIXER_CLASS, \
	AZ_CLASS_PLAYBACK, 0, 0, ENUM_V }, 0, 0, }, \
	{{AZ_CLASS_MIXER, {AzaliaCmixer, 0}, AUDIO_MIXER_CLASS, \
	AZ_CLASS_MIXER, 0, 0, ENUM_V }, 0, 0, }
#define AZ_MIXER_SPDIF(cl, nid)	\
	{{0, {AzaliaNdigital, 0}, AUDIO_MIXER_SET, cl, 0, 0,		\
	.un.s={6, {{{"v", 0}, CORB_DCC_V},	{{"vcfg", 0}, CORB_DCC_VCFG},	\
		   {{"pre", 0}, CORB_DCC_PRE}, {{"copy", 0}, CORB_DCC_COPY},	\
		   {{"pro", 0}, CORB_DCC_PRO}, {{"l", 0}, CORB_DCC_L}}}},	\
		    nid, MI_TARGET_SPDIF},				\
	{{0, {AzaliaNdigital".cc", 0}, AUDIO_MIXER_VALUE, cl, 0, 0,	\
       .un.v={{"", 0}, 1, 1}}, nid, MI_TARGET_SPDIF_CC}


static int	generic_codec_init_dacgroup(codec_t *);
static int	generic_codec_add_dacgroup(codec_t *, int, uint32_t);
static int	generic_codec_find_pin(const codec_t *, int, int, uint32_t);
static int	generic_codec_find_dac(const codec_t *, int, int);

static int	generic_mixer_init(codec_t *);
static int	generic_mixer_autoinit(codec_t *);
static int	generic_mixer_init_widget(const codec_t *, widget_t *, nid_t);

static int	generic_mixer_fix_indexes(codec_t *);
static int	generic_mixer_default(codec_t *);
static int	generic_mixer_pin_sense(codec_t *);
static int	generic_mixer_widget_name(const codec_t *, widget_t *);
static int	generic_mixer_create_virtual(codec_t *);
static int	generic_mixer_delete(codec_t *);
static void	generic_mixer_cat_names
	(char *, size_t, const char *, const char *, const char *);
static int	generic_mixer_ensure_capacity(codec_t *, size_t);
static int	generic_mixer_get(const codec_t *, nid_t, int, mixer_ctrl_t *);
static int	generic_mixer_set(codec_t *, nid_t, int, const mixer_ctrl_t *);
static int	generic_mixer_pinctrl(codec_t *, nid_t, uint32_t);
static u_char	generic_mixer_from_device_value
	(const codec_t *, nid_t, int, uint32_t );
static uint32_t	generic_mixer_to_device_value
	(const codec_t *, nid_t, int, u_char);
static uint32_t	generic_mixer_max(const codec_t *, nid_t, int);
static bool	generic_mixer_validate_value
	(const codec_t *, nid_t, int, u_char);
static int	generic_set_port(codec_t *, mixer_ctrl_t *);
static int	generic_get_port(codec_t *, mixer_ctrl_t *);

static int	alc260_init_dacgroup(codec_t *);
static int	alc260_mixer_init(codec_t *);
static int	alc260_set_port(codec_t *, mixer_ctrl_t *);
static int	alc260_get_port(codec_t *, mixer_ctrl_t *);
static int	alc260_unsol_event(codec_t *, int);
static int	alc262_init_widget(const codec_t *, widget_t *, nid_t);
static int	alc268_init_dacgroup(codec_t *);
static int	alc662_init_dacgroup(codec_t *);
static int	alc861_init_dacgroup(codec_t *);
static int	alc861vdgr_init_dacgroup(codec_t *);
static int	alc880_init_dacgroup(codec_t *);
static int	alc880_mixer_init(codec_t *);
static int	alc882_init_dacgroup(codec_t *);
static int	alc882_mixer_init(codec_t *);
static int	alc882_set_port(codec_t *, mixer_ctrl_t *);
static int	alc882_get_port(codec_t *, mixer_ctrl_t *);
static int	alc883_init_dacgroup(codec_t *);
static int 	alc883_mixer_init(codec_t *);
static int	alc885_init_dacgroup(codec_t *);
static int	alc888_init_dacgroup(codec_t *);
static int	alc888_init_widget(const codec_t *, widget_t *, nid_t);
static int	alc888_mixer_init(codec_t *);
static int	ad1981hd_init_widget(const codec_t *, widget_t *, nid_t);
static int	ad1981hd_mixer_init(codec_t *);
static int	ad1983_mixer_init(codec_t *);
static int	ad1983_unsol_event(codec_t *, int);
static int	ad1984_init_dacgroup(codec_t *);
static int	ad1984_init_widget(const codec_t *, widget_t *, nid_t);
static int	ad1984_mixer_init(codec_t *);
static int	ad1984_unsol_event(codec_t *, int);
static int	ad1986a_init_dacgroup(codec_t *);
static int	ad1986a_mixer_init(codec_t *);
static int	ad1988_init_dacgroup(codec_t *);
static int	cmi9880_init_dacgroup(codec_t *);
static int	cmi9880_mixer_init(codec_t *);
static int	stac9221_init_dacgroup(codec_t *);
static int	stac9221_mixer_init(codec_t *);
static int	stac9221_gpio_unmute(codec_t *, int);
static int	stac9200_mixer_init(codec_t *);
static int	stac9200_unsol_event(codec_t *, int);
static int	atihdmi_init_dacgroup(codec_t *);


int
azalia_codec_init_vtbl(codec_t *this)
{
	size_t extra_size;

	/**
	 * We can refer this->vid and this->subid.
	 */
	DPRINTF(("%s: vid=%08x subid=%08x\n", __func__, this->vid, this->subid));
	extra_size = 0;
	this->name = NULL;
	this->init_dacgroup = generic_codec_init_dacgroup;
	this->mixer_init = generic_mixer_init;
	this->mixer_delete = generic_mixer_delete;
	this->set_port = generic_set_port;
	this->get_port = generic_get_port;
	this->unsol_event = NULL;
	switch (this->vid) {
	case 0x10027919:
	case 0x1002793c:
		this->name = "ATI RS600 HDMI";
		this->init_dacgroup = atihdmi_init_dacgroup;
		break;
	case 0x1002791a:
		this->name = "ATI RS690/780 HDMI";
		this->init_dacgroup = atihdmi_init_dacgroup;
		break;
	case 0x1002aa01:
		this->name = "ATI R600 HDMI";
		this->init_dacgroup = atihdmi_init_dacgroup;
		break;
	case 0x10ec0260:
		this->name = "Realtek ALC260";
		this->mixer_init = alc260_mixer_init;
		this->init_dacgroup = alc260_init_dacgroup;
		this->set_port = alc260_set_port;
		this->unsol_event = alc260_unsol_event;
		extra_size = 1;
		break;
	case 0x10ec0262:
		this->name = "Realtek ALC262";
		this->init_widget = alc262_init_widget;
		break;
	case 0x10ec0268:
		this->name = "Realtek ALC268";
		this->init_dacgroup = alc268_init_dacgroup;
		this->mixer_init = generic_mixer_autoinit;
		this->init_widget = generic_mixer_init_widget;
		break;
	case 0x10ec0269:
		this->name = "Realtek ALC269";
		this->mixer_init = generic_mixer_autoinit;
		this->init_widget = generic_mixer_init_widget;
		break;
	case 0x10ec0662:
		this->name = "Realtek ALC662-GR";
		this->init_dacgroup = alc662_init_dacgroup;
		this->mixer_init = generic_mixer_autoinit;
		this->init_widget = generic_mixer_init_widget;
		break;
	case 0x10ec0663:
		this->name = "Realtek ALC663";
		this->init_dacgroup = alc662_init_dacgroup;
		this->mixer_init = generic_mixer_autoinit;
		this->init_widget = generic_mixer_init_widget;
		break;
	case 0x10ec0861:
		this->name = "Realtek ALC861";
		this->init_dacgroup = alc861_init_dacgroup;
		break;
	case 0x10ec0862:
		this->name = "Realtek ALC861-VD-GR";
		this->init_dacgroup = alc861vdgr_init_dacgroup;
		break;
	case 0x10ec0880:
		this->name = "Realtek ALC880";
		this->init_dacgroup = alc880_init_dacgroup;
		this->mixer_init = alc880_mixer_init;
		break;
	case 0x10ec0882:
		this->name = "Realtek ALC882";
		this->init_dacgroup = alc882_init_dacgroup;
		this->mixer_init = alc882_mixer_init;
		this->get_port = alc882_get_port;
		this->set_port = alc882_set_port;
		break;
	case 0x10ec0883:
		/* ftp://209.216.61.149/pc/audio/ALC883_DataSheet_1.3.pdf */
		this->name = "Realtek ALC883";
		this->init_dacgroup = alc883_init_dacgroup;
		this->mixer_init = alc883_mixer_init;
		this->get_port = alc882_get_port;
		this->set_port = alc882_set_port;
		break;
	case 0x10ec0885:
		this->name = "Realtek ALC885";
		this->init_dacgroup = alc885_init_dacgroup;
		this->mixer_init = generic_mixer_autoinit;
		this->init_widget = generic_mixer_init_widget;
		break;
	case 0x10ec0888:
		this->name = "Realtek ALC888";
		this->init_dacgroup = alc888_init_dacgroup;
		this->init_widget = alc888_init_widget;
		this->mixer_init = alc888_mixer_init;
		break;
	case 0x11d41981:
		/* http://www.analog.com/en/prod/0,2877,AD1981HD,00.html */
		this->name = "Analog Devices AD1981HD";
		this->init_widget = ad1981hd_init_widget;
		this->mixer_init = ad1981hd_mixer_init;
		break;
	case 0x11d41983:
		/* http://www.analog.com/en/prod/0,2877,AD1983,00.html */
		this->name = "Analog Devices AD1983";
		this->mixer_init = ad1983_mixer_init;
		this->unsol_event = ad1983_unsol_event;
		break;
	case 0x11d41984:
		/* http://www.analog.com/en/prod/0,2877,AD1984,00.html */
		this->name = "Analog Devices AD1984";
		this->init_dacgroup = ad1984_init_dacgroup;
		this->init_widget = ad1984_init_widget;
		this->mixer_init = ad1984_mixer_init;
		this->unsol_event = ad1984_unsol_event;
		break;
	case 0x11d4194a:
		/* http://www.analog.com/static/imported-files/data_sheets/AD1984A.pdf */
		this->name = "Analog Devices AD1984A";
		this->init_dacgroup = ad1984_init_dacgroup;
		this->init_widget = ad1984_init_widget;
		this->mixer_init = ad1984_mixer_init;
		this->unsol_event = ad1984_unsol_event;
		break;
	case 0x11d41986:
		/* http://www.analog.com/en/prod/0,2877,AD1986A,00.html */
		this->name = "Analog Devices AD1986A";
		this->init_dacgroup = ad1986a_init_dacgroup;
		this->mixer_init = ad1986a_mixer_init;
		break;
	case 0x11d41988:
		/* http://www.analog.com/en/prod/0,2877,AD1988A,00.html */
		this->name = "Analog Devices AD1988A";
		this->init_dacgroup = ad1988_init_dacgroup;
		break;
	case 0x11d4198b:
		/* http://www.analog.com/en/prod/0,2877,AD1988B,00.html */
		this->name = "Analog Devices AD1988B";
		this->init_dacgroup = ad1988_init_dacgroup;
		break;
	case 0x434d4980:
		this->name = "CMedia CMI9880";
		this->init_dacgroup = cmi9880_init_dacgroup;
		this->mixer_init = cmi9880_mixer_init;
		break;
	case 0x83847612:
		/* http://www.idt.com/products/getDoc.cfm?docID=17122893 */
		this->name = "Sigmatel STAC9230X";
		break;
	case 0x83847613:
		/* http://www.idt.com/products/getDoc.cfm?docID=17122893 */
		this->name = "Sigmatel STAC9230D";
		break;
	case 0x83847614:
		/* http://www.idt.com/products/getDoc.cfm?docID=17122893 */
		this->name = "Sigmatel STAC9229X";
		break;
	case 0x83847615:
		/* http://www.idt.com/products/getDoc.cfm?docID=17122893 */
		this->name = "Sigmatel STAC9229D";
		break;
	case 0x83847616:
		/* http://www.idt.com/products/getDoc.cfm?docID=17122893 */
		this->name = "Sigmatel STAC9228X";
		break;
	case 0x83847617:
		/* http://www.idt.com/products/getDoc.cfm?docID=17122893 */
		this->name = "Sigmatel STAC9228D";
		break;
	case 0x83847618:
		/* http://www.idt.com/products/getDoc.cfm?docID=17122893 */
		this->name = "Sigmatel STAC9227X";
		break;
	case 0x83847619:
		/* http://www.idt.com/products/getDoc.cfm?docID=17122893 */
		this->name = "Sigmatel STAC9227D";
		break;
	case 0x83847680:
		this->name = "Sigmatel STAC9221";
		this->init_dacgroup = stac9221_init_dacgroup;
		this->mixer_init = stac9221_mixer_init;
		break;
	case 0x83847683:
		this->name = "Sigmatel STAC9221D";
		this->init_dacgroup = stac9221_init_dacgroup;
		break;
	case 0x83847690:
		/* http://www.idt.com/products/getDoc.cfm?docID=17812077 */
		this->name = "Sigmatel STAC9200";
		this->mixer_init = stac9200_mixer_init;
		this->unsol_event = stac9200_unsol_event;
		break;
	case 0x83847691:
		/* http://www.idt.com/products/getDoc.cfm?docID=17812077 */
		this->name = "Sigmatel STAC9200D";
		this->mixer_init = stac9200_mixer_init;
		this->unsol_event = stac9200_unsol_event;
		break;
	}
	if (extra_size > 0) {
		this->szextra = sizeof(uint32_t) * extra_size;
		this->extra = kmem_zalloc(this->szextra, KM_SLEEP);
		if (this->extra == NULL) {
			aprint_error_dev(this->dev, "Not enough memory\n");
			return ENOMEM;
		}
	}
	return 0;
}

/* ----------------------------------------------------------------
 * functions for generic codecs
 * ---------------------------------------------------------------- */

static int
generic_codec_init_dacgroup(codec_t *this)
{
	int i, j, assoc, group;

	/*
	 * grouping DACs
	 *   [0] the lowest assoc DACs
	 *   [1] the lowest assoc digital outputs
	 *   [2] the 2nd assoc DACs
	 *      :
	 */
	this->dacs.ngroups = 0;
	for (assoc = 0; assoc < CORB_CD_ASSOCIATION_MAX; assoc++) {
		generic_codec_add_dacgroup(this, assoc, 0);
		generic_codec_add_dacgroup(this, assoc, COP_AWCAP_DIGITAL);
	}

	/* find DACs which do not connect with any pins by default */
	DPRINTF(("%s: find non-connected DACs\n", __func__));
	FOR_EACH_WIDGET(this, i) {
		bool found;

		if (this->w[i].type != COP_AWTYPE_AUDIO_OUTPUT)
			continue;
		found = FALSE;
		for (group = 0; group < this->dacs.ngroups; group++) {
			for (j = 0; j < this->dacs.groups[group].nconv; j++) {
				if (i == this->dacs.groups[group].conv[j]) {
					found = TRUE;
					group = this->dacs.ngroups;
					break;
				}
			}
		}
		if (found)
			continue;
		if (this->dacs.ngroups >= 32)
			break;
		this->dacs.groups[this->dacs.ngroups].nconv = 1;
		this->dacs.groups[this->dacs.ngroups].conv[0] = i;
		this->dacs.ngroups++;
	}
	this->dacs.cur = 0;

	/* enumerate ADCs */
	this->adcs.ngroups = 0;
	FOR_EACH_WIDGET(this, i) {
		if (this->w[i].type != COP_AWTYPE_AUDIO_INPUT)
			continue;
		this->adcs.groups[this->adcs.ngroups].nconv = 1;
		this->adcs.groups[this->adcs.ngroups].conv[0] = i;
		this->adcs.ngroups++;
		if (this->adcs.ngroups >= 32)
			break;
	}
	this->adcs.cur = 0;
	return 0;
}

static int
generic_codec_add_dacgroup(codec_t *this, int assoc, uint32_t digital)
{
	int i, j, n, dac, seq;

	n = 0;
	for (seq = 0 ; seq < CORB_CD_SEQUENCE_MAX; seq++) {
		i = generic_codec_find_pin(this, assoc, seq, digital);
		if (i < 0)
			continue;
		dac = generic_codec_find_dac(this, i, 0);
		if (dac < 0)
			continue;
		/* duplication check */
		for (j = 0; j < n; j++) {
			if (this->dacs.groups[this->dacs.ngroups].conv[j] == dac)
				break;
		}
		if (j < n)	/* this group already has <dac> */
			continue;
		this->dacs.groups[this->dacs.ngroups].conv[n++] = dac;
		DPRINTF(("%s: assoc=%d seq=%d ==> g=%d n=%d\n",
			 __func__, assoc, seq, this->dacs.ngroups, n-1));
	}
	if (n <= 0)		/* no such DACs */
		return 0;
	this->dacs.groups[this->dacs.ngroups].nconv = n;

	/* check if the same combination is already registered */
	for (i = 0; i < this->dacs.ngroups; i++) {
		if (n != this->dacs.groups[i].nconv)
			continue;
		for (j = 0; j < n; j++) {
			if (this->dacs.groups[this->dacs.ngroups].conv[j] !=
			    this->dacs.groups[i].conv[j])
				break;
		}
		if (j >= n) /* matched */
			return 0;
	}
	/* found no equivalent group */
	this->dacs.ngroups++;
	return 0;
}

static int
generic_codec_find_pin(const codec_t *this, int assoc, int seq, uint32_t digital)
{
	int i;

	FOR_EACH_WIDGET(this, i) {
		if (this->w[i].type != COP_AWTYPE_PIN_COMPLEX)
			continue;
		if ((this->w[i].d.pin.cap & COP_PINCAP_OUTPUT) == 0)
			continue;
		if ((this->w[i].widgetcap & COP_AWCAP_DIGITAL) != digital)
			continue;
		if (this->w[i].d.pin.association != assoc)
			continue;
		if (this->w[i].d.pin.sequence == seq) {
			return i;
		}
	}
	return -1;
}

static int
generic_codec_find_dac(const codec_t *this, int index, int depth)
{
	const widget_t *w;
	int i, j, ret;

	w = &this->w[index];
	if (w->type == COP_AWTYPE_AUDIO_OUTPUT) {
		DPRINTF(("%s: DAC: nid=0x%x index=%d\n",
		    __func__, w->nid, index));
		return index;
	}
	if (++depth > 50) {
		return -1;
	}
	if (w->selected >= 0) {
		j = w->connections[w->selected];
		if (VALID_WIDGET_NID(j, this)) {
			ret = generic_codec_find_dac(this, j, depth);
			if (ret >= 0) {
				DPRINTF(("%s: DAC path: nid=0x%x index=%d\n",
				    __func__, w->nid, index));
				return ret;
			}
		}
	}
	for (i = 0; i < w->nconnections; i++) {
		j = w->connections[i];
		if (!VALID_WIDGET_NID(j, this))
			continue;
		ret = generic_codec_find_dac(this, j, depth);
		if (ret >= 0) {
			DPRINTF(("%s: DAC path: nid=0x%x index=%d\n",
			    __func__, w->nid, index));
			return ret;
		}
	}
	return -1;
}

/* ----------------------------------------------------------------
 * Generic mixer functions
 * ---------------------------------------------------------------- */

#define GMIDPRINTF(x)	do {} while (0/*CONSTCOND*/)

static int
generic_mixer_init(codec_t *this)
{
	/*
	 * pin		"<color>%2.2x"
	 * audio output	"dac%2.2x"
	 * audio input	"adc%2.2x"
	 * mixer	"mixer%2.2x"
	 * selector	"sel%2.2x"
	 */
	mixer_item_t *m;
	int err, i, j, k;

	this->maxmixers = 10;
	this->nmixers = 0;
	this->szmixers = sizeof(mixer_item_t) * this->maxmixers;
	this->mixers = kmem_zalloc(this->szmixers, KM_SLEEP);
	if (this->mixers == NULL) {
		aprint_error_dev(this->dev, "out of memory in %s\n", __func__);
		return ENOMEM;
	}

	/* register classes */
	DPRINTF(("%s: register classes\n", __func__));
	m = &this->mixers[AZ_CLASS_INPUT];
	m->devinfo.index = AZ_CLASS_INPUT;
	strlcpy(m->devinfo.label.name, AudioCinputs,
	    sizeof(m->devinfo.label.name));
	m->devinfo.type = AUDIO_MIXER_CLASS;
	m->devinfo.mixer_class = AZ_CLASS_INPUT;
	m->devinfo.next = AUDIO_MIXER_LAST;
	m->devinfo.prev = AUDIO_MIXER_LAST;
	m->nid = 0;

	m = &this->mixers[AZ_CLASS_OUTPUT];
	m->devinfo.index = AZ_CLASS_OUTPUT;
	strlcpy(m->devinfo.label.name, AudioCoutputs,
	    sizeof(m->devinfo.label.name));
	m->devinfo.type = AUDIO_MIXER_CLASS;
	m->devinfo.mixer_class = AZ_CLASS_OUTPUT;
	m->devinfo.next = AUDIO_MIXER_LAST;
	m->devinfo.prev = AUDIO_MIXER_LAST;
	m->nid = 0;

	m = &this->mixers[AZ_CLASS_RECORD];
	m->devinfo.index = AZ_CLASS_RECORD;
	strlcpy(m->devinfo.label.name, AudioCrecord,
	    sizeof(m->devinfo.label.name));
	m->devinfo.type = AUDIO_MIXER_CLASS;
	m->devinfo.mixer_class = AZ_CLASS_RECORD;
	m->devinfo.next = AUDIO_MIXER_LAST;
	m->devinfo.prev = AUDIO_MIXER_LAST;
	m->nid = 0;

	m = &this->mixers[AZ_CLASS_PLAYBACK];
	m->devinfo.index = AZ_CLASS_PLAYBACK;
	strlcpy(m->devinfo.label.name, AzaliaCplayback,
	    sizeof(m->devinfo.label.name));
	m->devinfo.type = AUDIO_MIXER_CLASS;
	m->devinfo.mixer_class = AZ_CLASS_PLAYBACK;
	m->devinfo.next = AUDIO_MIXER_LAST;
	m->devinfo.prev = AUDIO_MIXER_LAST;
	m->nid = 0;

	m = &this->mixers[AZ_CLASS_MIXER];
	m->devinfo.index = AZ_CLASS_MIXER;
	strlcpy(m->devinfo.label.name, AzaliaCmixer,
	    sizeof(m->devinfo.label.name));
	m->devinfo.type = AUDIO_MIXER_CLASS;
	m->devinfo.mixer_class = AZ_CLASS_MIXER;
	m->devinfo.next = AUDIO_MIXER_LAST;
	m->devinfo.prev = AUDIO_MIXER_LAST;
	m->nid = 0;

	this->nmixers = AZ_CLASS_MIXER + 1;

#define MIXER_REG_PROLOG	\
	mixer_devinfo_t *d; \
	err = generic_mixer_ensure_capacity(this, this->nmixers + 1); \
	if (err) \
		return err; \
	m = &this->mixers[this->nmixers]; \
	d = &m->devinfo; \
	m->nid = i

	FOR_EACH_WIDGET(this, i) {
		const widget_t *w;

		w = &this->w[i];

		/* skip unconnected pins */
		if (w->type == COP_AWTYPE_PIN_COMPLEX) {
			uint8_t conn =
			    (w->d.pin.config & CORB_CD_PORT_MASK) >> 30;
			if (conn == 1)	/* no physical connection */
				continue;
		}

		/* selector */
		if (w->type != COP_AWTYPE_AUDIO_MIXER &&
		    w->type != COP_AWTYPE_POWER && w->nconnections >= 2) {
			MIXER_REG_PROLOG;
			GMIDPRINTF(("%s: selector %s\n", __func__, w->name));
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.source", w->name);
			d->type = AUDIO_MIXER_ENUM;
			if (w->type == COP_AWTYPE_AUDIO_MIXER)
				d->mixer_class = AZ_CLASS_RECORD;
			else if (w->type == COP_AWTYPE_AUDIO_SELECTOR)
				d->mixer_class = AZ_CLASS_INPUT;
			else
				d->mixer_class = AZ_CLASS_OUTPUT;
			m->target = MI_TARGET_CONNLIST;
			for (j = 0, k = 0; j < w->nconnections && k < 32; j++) {
				uint8_t conn;

				if (!VALID_WIDGET_NID(w->connections[j], this))
					continue;
				/* skip unconnected pins */
				PIN_STATUS(&this->w[w->connections[j]],
				    conn);
				if (conn == 1)
					continue;
				GMIDPRINTF(("%s: selector %d=%s\n", __func__, j,
				    this->w[w->connections[j]].name));
				d->un.e.member[k].ord = j;
				strlcpy(d->un.e.member[k].label.name,
				    this->w[w->connections[j]].name,
				    MAX_AUDIO_DEV_LEN);
				k++;
			}
			d->un.e.num_mem = k;
			this->nmixers++;
		}

		/* output mute */
		if (w->widgetcap & COP_AWCAP_OUTAMP &&
		    w->outamp_cap & COP_AMPCAP_MUTE) {
			MIXER_REG_PROLOG;
			GMIDPRINTF(("%s: output mute %s\n", __func__, w->name));
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.mute", w->name);
			d->type = AUDIO_MIXER_ENUM;
			if (w->type == COP_AWTYPE_AUDIO_MIXER)
				d->mixer_class = AZ_CLASS_MIXER;
			else if (w->type == COP_AWTYPE_AUDIO_SELECTOR)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else if (w->type == COP_AWTYPE_PIN_COMPLEX)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else
				d->mixer_class = AZ_CLASS_INPUT;
			m->target = MI_TARGET_OUTAMP;
			d->un.e.num_mem = 2;
			d->un.e.member[0].ord = 0;
			strlcpy(d->un.e.member[0].label.name, AudioNoff,
			    MAX_AUDIO_DEV_LEN);
			d->un.e.member[1].ord = 1;
			strlcpy(d->un.e.member[1].label.name, AudioNon,
			    MAX_AUDIO_DEV_LEN);
			this->nmixers++;
		}

		/* output gain */
		if (w->widgetcap & COP_AWCAP_OUTAMP
		    && COP_AMPCAP_NUMSTEPS(w->outamp_cap)) {
			MIXER_REG_PROLOG;
			GMIDPRINTF(("%s: output gain %s\n", __func__, w->name));
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s", w->name);
			d->type = AUDIO_MIXER_VALUE;
			if (w->type == COP_AWTYPE_AUDIO_MIXER)
				d->mixer_class = AZ_CLASS_MIXER;
			else if (w->type == COP_AWTYPE_AUDIO_SELECTOR)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else if (w->type == COP_AWTYPE_PIN_COMPLEX)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else
				d->mixer_class = AZ_CLASS_INPUT;
			m->target = MI_TARGET_OUTAMP;
			d->un.v.num_channels = WIDGET_CHANNELS(w);
#ifdef MAX_VOLUME_255
			d->un.v.units.name[0] = 0;
#else
			snprintf(d->un.v.units.name, sizeof(d->un.v.units.name),
			    "0.25x%ddB", COP_AMPCAP_STEPSIZE(w->outamp_cap)+1);
#endif
			d->un.v.delta =
			    MIXER_DELTA(COP_AMPCAP_NUMSTEPS(w->outamp_cap));
			this->nmixers++;
		}

		/* input mute */
		if (w->widgetcap & COP_AWCAP_INAMP &&
		    w->inamp_cap & COP_AMPCAP_MUTE) {
			GMIDPRINTF(("%s: input mute %s\n", __func__, w->name));
			if (w->type != COP_AWTYPE_AUDIO_SELECTOR &&
			    w->type != COP_AWTYPE_AUDIO_MIXER) {
				MIXER_REG_PROLOG;
				snprintf(d->label.name, sizeof(d->label.name),
				    "%s.mute", w->name);
				d->type = AUDIO_MIXER_ENUM;
				if (w->type == COP_AWTYPE_PIN_COMPLEX)
					d->mixer_class = AZ_CLASS_OUTPUT;
				else if (w->type == COP_AWTYPE_AUDIO_INPUT)
					d->mixer_class = AZ_CLASS_RECORD;
				else
					d->mixer_class = AZ_CLASS_INPUT;
				m->target = 0;
				d->un.e.num_mem = 2;
				d->un.e.member[0].ord = 0;
				strlcpy(d->un.e.member[0].label.name,
				    AudioNoff, MAX_AUDIO_DEV_LEN);
				d->un.e.member[1].ord = 1;
				strlcpy(d->un.e.member[1].label.name,
				    AudioNon, MAX_AUDIO_DEV_LEN);
				this->nmixers++;
			} else {
				uint8_t conn;

				for (j = 0; j < w->nconnections; j++) {
					MIXER_REG_PROLOG;
					if (!VALID_WIDGET_NID(w->connections[j], this))
						continue;

					/* skip unconnected pins */
					PIN_STATUS(&this->w[w->connections[j]],
					    conn);
					if (conn == 1)
						continue;

					GMIDPRINTF(("%s: input mute %s.%s\n", __func__,
					    w->name, this->w[w->connections[j]].name));
					generic_mixer_cat_names(
					    d->label.name,
					    sizeof(d->label.name),
					    w->name, this->w[w->connections[j]].name,
					    "mute");
					d->type = AUDIO_MIXER_ENUM;
					if (w->type == COP_AWTYPE_PIN_COMPLEX)
						d->mixer_class = AZ_CLASS_OUTPUT;
					else if (w->type == COP_AWTYPE_AUDIO_INPUT)
						d->mixer_class = AZ_CLASS_RECORD;
					else if (w->type == COP_AWTYPE_AUDIO_MIXER)
						d->mixer_class = AZ_CLASS_MIXER;
					else
						d->mixer_class = AZ_CLASS_INPUT;
					m->target = j;
					d->un.e.num_mem = 2;
					d->un.e.member[0].ord = 0;
					strlcpy(d->un.e.member[0].label.name,
					    AudioNoff, MAX_AUDIO_DEV_LEN);
					d->un.e.member[1].ord = 1;
					strlcpy(d->un.e.member[1].label.name,
					    AudioNon, MAX_AUDIO_DEV_LEN);
					this->nmixers++;
				}
			}
		}

		/* input gain */
		if (w->widgetcap & COP_AWCAP_INAMP
		    && COP_AMPCAP_NUMSTEPS(w->inamp_cap)) {
			GMIDPRINTF(("%s: input gain %s\n", __func__, w->name));
			if (w->type != COP_AWTYPE_AUDIO_SELECTOR &&
			    w->type != COP_AWTYPE_AUDIO_MIXER) {
				MIXER_REG_PROLOG;
				snprintf(d->label.name, sizeof(d->label.name),
				    "%s", w->name);
				d->type = AUDIO_MIXER_VALUE;
				if (w->type == COP_AWTYPE_PIN_COMPLEX)
					d->mixer_class = AZ_CLASS_OUTPUT;
				else if (w->type == COP_AWTYPE_AUDIO_INPUT)
					d->mixer_class = AZ_CLASS_RECORD;
				else
					d->mixer_class = AZ_CLASS_INPUT;
				m->target = 0;
				d->un.v.num_channels = WIDGET_CHANNELS(w);
#ifdef MAX_VOLUME_255
				d->un.v.units.name[0] = 0;
#else
				snprintf(d->un.v.units.name,
				    sizeof(d->un.v.units.name), "0.25x%ddB",
				    COP_AMPCAP_STEPSIZE(w->inamp_cap)+1);
#endif
				d->un.v.delta =
				    MIXER_DELTA(COP_AMPCAP_NUMSTEPS(w->inamp_cap));
				this->nmixers++;
			} else {
				uint8_t conn;

				for (j = 0; j < w->nconnections; j++) {
					MIXER_REG_PROLOG;
					if (!VALID_WIDGET_NID(w->connections[j], this))
						continue;
					/* skip unconnected pins */
					PIN_STATUS(&this->w[w->connections[j]],
					    conn);
					if (conn == 1)
						continue;
					GMIDPRINTF(("%s: input gain %s.%s\n", __func__,
					    w->name, this->w[w->connections[j]].name));
					snprintf(d->label.name, sizeof(d->label.name),
					    "%s.%s", w->name,
					    this->w[w->connections[j]].name);
					d->type = AUDIO_MIXER_VALUE;
					if (w->type == COP_AWTYPE_PIN_COMPLEX)
						d->mixer_class = AZ_CLASS_OUTPUT;
					else if (w->type == COP_AWTYPE_AUDIO_INPUT)
						d->mixer_class = AZ_CLASS_RECORD;
					else if (w->type == COP_AWTYPE_AUDIO_MIXER)
						d->mixer_class = AZ_CLASS_MIXER;
					else
						d->mixer_class = AZ_CLASS_INPUT;
					m->target = j;
					d->un.v.num_channels = WIDGET_CHANNELS(w);
#ifdef MAX_VOLUME_255
					d->un.v.units.name[0] = 0;
#else
					snprintf(d->un.v.units.name,
					    sizeof(d->un.v.units.name), "0.25x%ddB",
					    COP_AMPCAP_STEPSIZE(w->inamp_cap)+1);
#endif
					d->un.v.delta =
					    MIXER_DELTA(COP_AMPCAP_NUMSTEPS(w->inamp_cap));
					this->nmixers++;
				}
			}
		}

		/* pin direction */
		if (w->type == COP_AWTYPE_PIN_COMPLEX &&
		    w->d.pin.cap & COP_PINCAP_OUTPUT &&
		    w->d.pin.cap & COP_PINCAP_INPUT) {
			MIXER_REG_PROLOG;
			GMIDPRINTF(("%s: pin dir %s\n", __func__, w->name));
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.dir", w->name);
			d->type = AUDIO_MIXER_ENUM;
			d->mixer_class = AZ_CLASS_OUTPUT;
			m->target = MI_TARGET_PINDIR;
			d->un.e.num_mem = 2;
			d->un.e.member[0].ord = 0;
			strlcpy(d->un.e.member[0].label.name, AudioNinput,
			    MAX_AUDIO_DEV_LEN);
			d->un.e.member[1].ord = 1;
			strlcpy(d->un.e.member[1].label.name, AudioNoutput,
			    MAX_AUDIO_DEV_LEN);
			this->nmixers++;
		}

		/* pin headphone-boost */
		if (w->type == COP_AWTYPE_PIN_COMPLEX &&
		    w->d.pin.cap & COP_PINCAP_HEADPHONE) {
			MIXER_REG_PROLOG;
			GMIDPRINTF(("%s: hpboost %s\n", __func__, w->name));
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.boost", w->name);
			d->type = AUDIO_MIXER_ENUM;
			d->mixer_class = AZ_CLASS_OUTPUT;
			m->target = MI_TARGET_PINBOOST;
			d->un.e.num_mem = 2;
			d->un.e.member[0].ord = 0;
			strlcpy(d->un.e.member[0].label.name, AudioNoff,
			    MAX_AUDIO_DEV_LEN);
			d->un.e.member[1].ord = 1;
			strlcpy(d->un.e.member[1].label.name, AudioNon,
			    MAX_AUDIO_DEV_LEN);
			this->nmixers++;
		}

		if (w->type == COP_AWTYPE_PIN_COMPLEX &&
		    w->d.pin.cap & COP_PINCAP_EAPD) {
			MIXER_REG_PROLOG;
			GMIDPRINTF(("%s: eapd %s\n", __func__, w->name));
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.eapd", w->name);
			d->type = AUDIO_MIXER_ENUM;
			d->mixer_class = AZ_CLASS_OUTPUT;
			m->target = MI_TARGET_EAPD;
			d->un.e.num_mem = 2;
			d->un.e.member[0].ord = 0;
			strlcpy(d->un.e.member[0].label.name, AudioNoff,
			    MAX_AUDIO_DEV_LEN);
			d->un.e.member[1].ord = 1;
			strlcpy(d->un.e.member[1].label.name, AudioNon,
			    MAX_AUDIO_DEV_LEN);
			this->nmixers++;
		}

		if (w->type == COP_AWTYPE_PIN_COMPLEX &&
		    w->d.pin.cap & COP_PINCAP_BALANCE) {
			MIXER_REG_PROLOG;
			GMIDPRINTF(("%s: balance %s\n", __func__, w->name));
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.balance", w->name);
			d->type = AUDIO_MIXER_ENUM;
			if (w->type == COP_AWTYPE_PIN_COMPLEX)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else if (w->type == COP_AWTYPE_AUDIO_INPUT)
				d->mixer_class = AZ_CLASS_RECORD;
			else
				d->mixer_class = AZ_CLASS_INPUT;
			m->target = MI_TARGET_BALANCE;
			d->un.e.num_mem = 2;
			d->un.e.member[0].ord = 0;
			strlcpy(d->un.e.member[0].label.name, AudioNoff,
			    MAX_AUDIO_DEV_LEN);
			d->un.e.member[1].ord = 1;
			strlcpy(d->un.e.member[1].label.name, AudioNon,
			    MAX_AUDIO_DEV_LEN);
			this->nmixers++;
		}

		if (w->widgetcap & COP_AWCAP_LRSWAP) {
			MIXER_REG_PROLOG;
			GMIDPRINTF(("%s: lrswap %s\n", __func__, w->name));
			snprintf(d->label.name, sizeof(d->label.name),
			    "%s.lrswap", w->name);
			d->type = AUDIO_MIXER_ENUM;
			if (w->type == COP_AWTYPE_PIN_COMPLEX)
				d->mixer_class = AZ_CLASS_OUTPUT;
			else if (w->type == COP_AWTYPE_AUDIO_INPUT)
				d->mixer_class = AZ_CLASS_RECORD;
			else
				d->mixer_class = AZ_CLASS_INPUT;
			m->target = MI_TARGET_LRSWAP;
			d->un.e.num_mem = 2;
			d->un.e.member[0].ord = 0;
			strlcpy(d->un.e.member[0].label.name, AudioNoff,
			    MAX_AUDIO_DEV_LEN);
			d->un.e.member[1].ord = 1;
			strlcpy(d->un.e.member[1].label.name, AudioNon,
			    MAX_AUDIO_DEV_LEN);
			this->nmixers++;
		}

		/* volume knob */
		if (w->type == COP_AWTYPE_VOLUME_KNOB &&
		    w->d.volume.cap & COP_VKCAP_DELTA) {
			MIXER_REG_PROLOG;
			GMIDPRINTF(("%s: volume knob %s\n", __func__, w->name));
			strlcpy(d->label.name, w->name, sizeof(d->label.name));
			d->type = AUDIO_MIXER_VALUE;
			d->mixer_class = AZ_CLASS_OUTPUT;
			m->target = MI_TARGET_VOLUME;
			d->un.v.num_channels = 1;
			d->un.v.units.name[0] = 0;
			d->un.v.delta =
			    MIXER_DELTA(COP_VKCAP_NUMSTEPS(w->d.volume.cap));
			this->nmixers++;
		}
	}

	/* if the codec has multiple DAC groups, create "playback.mode" */
	if (this->dacs.ngroups > 1) {
		MIXER_REG_PROLOG;
		GMIDPRINTF(("%s: create playback.mode\n", __func__));
		strlcpy(d->label.name, AudioNmode, sizeof(d->label.name));
		d->type = AUDIO_MIXER_ENUM;
		d->mixer_class = AZ_CLASS_PLAYBACK;
		m->target = MI_TARGET_DAC;
		for (i = 0; i < this->dacs.ngroups && i < 32; i++) {
			d->un.e.member[i].ord = i;
			for (j = 0; j < this->dacs.groups[i].nconv; j++) {
				if (j * 2 >= MAX_AUDIO_DEV_LEN)
					break;
				snprintf(d->un.e.member[i].label.name + j*2,
				    MAX_AUDIO_DEV_LEN - j*2, "%2.2x",
				    this->dacs.groups[i].conv[j]);
			}
		}
		d->un.e.num_mem = i;
		this->nmixers++;
	}

	/* if the codec has multiple ADC groups, create "record.mode" */
	if (this->adcs.ngroups > 1) {
		MIXER_REG_PROLOG;
		GMIDPRINTF(("%s: create record.mode\n", __func__));
		strlcpy(d->label.name, AudioNmode, sizeof(d->label.name));
		d->type = AUDIO_MIXER_ENUM;
		d->mixer_class = AZ_CLASS_RECORD;
		m->target = MI_TARGET_ADC;
		for (i = 0; i < this->adcs.ngroups && i < 32; i++) {
			d->un.e.member[i].ord = i;
			for (j = 0; j < this->adcs.groups[i].nconv; j++) {
				if (j * 2 >= MAX_AUDIO_DEV_LEN)
					break;
				snprintf(d->un.e.member[i].label.name + j*2,
				    MAX_AUDIO_DEV_LEN - j*2, "%2.2x",
				    this->adcs.groups[i].conv[j]);
			}
		}
		d->un.e.num_mem = i;
		this->nmixers++;
	}

	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);
	return 0;
}

static void
generic_mixer_cat_names(char *dst, size_t dstsize,
		  const char *str1, const char *str2, const char *str3)
{
	const char *last2;
	size_t len1, len2, len3, total;

	len1 = strlen(str1);
	len2 = strlen(str2);
	len3 = strlen(str3);
	total = len1 + 1 + len2 + 1 + len3 + 1;
	if (total - (len3 - 1) <= dstsize) {
		snprintf(dst, dstsize, "%s.%s.%s", str1, str2, str3);
		return;
	}
	last2 = len2 > 2 ? str2 + len2 - 2 : str2;
	if (len2 > 4) {
		snprintf(dst, dstsize, "%s.%.2s%s.%s", str1, str2, last2, str3);
		return;
	}
	snprintf(dst, dstsize, "%s.%s.%s", str1, last2, str3);
}

static int
generic_mixer_ensure_capacity(codec_t *this, size_t newsize)
{
	size_t newmax;
	size_t newsz;
	void *newbuf;

	if (this->maxmixers >= newsize)
		return 0;
	newmax = this->maxmixers + 10;
	if (newmax < newsize)
		newmax = newsize;
	newsz = sizeof(mixer_item_t) * newmax;
	newbuf = kmem_zalloc(newsz, KM_SLEEP);
	if (newbuf == NULL) {
		aprint_error_dev(this->dev, "out of memory in %s\n", __func__);
		return ENOMEM;
	}
	memcpy(newbuf, this->mixers, this->szmixers);
	kmem_free(this->mixers, this->szmixers);
	this->mixers = newbuf;
	this->szmixers = newsize;
	this->maxmixers = newmax;
	return 0;
}

static int
generic_mixer_fix_indexes(codec_t *this)
{
	int i;
	mixer_devinfo_t *d;

	for (i = 0; i < this->nmixers; i++) {
		d = &this->mixers[i].devinfo;
#ifdef DIAGNOSTIC
		if (d->index != 0 && d->index != i)
			aprint_error("%s: index mismatch %d %d\n", __func__,
			    d->index, i);
#endif
		d->index = i;
		if (d->prev == 0)
			d->prev = AUDIO_MIXER_LAST;
		if (d->next == 0)
			d->next = AUDIO_MIXER_LAST;
	}
	return 0;
}

static int
generic_mixer_default(codec_t *this)
{
	int i;
	mixer_item_t *m;
	/* unmute all */
	DPRINTF(("%s: unmute\n", __func__));
	for (i = 0; i < this->nmixers; i++) {
		mixer_ctrl_t mc;

		m = &this->mixers[i];
		if (!IS_MI_TARGET_INAMP(m->target) &&
		    m->target != MI_TARGET_OUTAMP)
			continue;
		if (m->devinfo.type != AUDIO_MIXER_ENUM)
			continue;
		mc.dev = i;
		mc.type = AUDIO_MIXER_ENUM;
		mc.un.ord = 0;
		generic_mixer_set(this, m->nid, m->target, &mc);
	}

	/*
	 * For bidirectional pins, make the default `output'
	 */
	DPRINTF(("%s: process bidirectional pins\n", __func__));
	for (i = 0; i < this->nmixers; i++) {
		mixer_ctrl_t mc;

		m = &this->mixers[i];
		if (m->target != MI_TARGET_PINDIR)
			continue;
		mc.dev = i;
		mc.type = AUDIO_MIXER_ENUM;
		mc.un.ord = 1;	/* output */
		generic_mixer_set(this, m->nid, m->target, &mc);
	}

	/* set unextreme volume */
	DPRINTF(("%s: set volume\n", __func__));
	for (i = 0; i < this->nmixers; i++) {
		mixer_ctrl_t mc;

		m = &this->mixers[i];
		if (!IS_MI_TARGET_INAMP(m->target) &&
		    m->target != MI_TARGET_OUTAMP &&
		    m->target != MI_TARGET_VOLUME)
			continue;
		if (m->devinfo.type != AUDIO_MIXER_VALUE)
			continue;
		mc.dev = i;
		mc.type = AUDIO_MIXER_VALUE;
		mc.un.value.num_channels = 1;
		mc.un.value.level[0] = AUDIO_MAX_GAIN / 2;
		if (m->target != MI_TARGET_VOLUME &&
		    WIDGET_CHANNELS(&this->w[m->nid]) == 2) {
			mc.un.value.num_channels = 2;
			mc.un.value.level[1] = AUDIO_MAX_GAIN / 2;
		}
		generic_mixer_set(this, m->nid, m->target, &mc);
	}

	return 0;
}

static int
generic_mixer_pin_sense(codec_t *this)
{
	typedef enum {
		PIN_DIR_IN,
		PIN_DIR_OUT,
		PIN_DIR_MIC
	} pintype_t;
	const widget_t *w;
	int i;

	FOR_EACH_WIDGET(this, i) {
		pintype_t pintype = PIN_DIR_IN;

		w = &this->w[i];
		if (w->type != COP_AWTYPE_PIN_COMPLEX)
			continue;
		if (!(w->d.pin.cap & COP_PINCAP_INPUT))
			pintype = PIN_DIR_OUT;
		if (!(w->d.pin.cap & COP_PINCAP_OUTPUT))
			pintype = PIN_DIR_IN;

		switch (w->d.pin.device) {
		case CORB_CD_LINEOUT:
		case CORB_CD_SPEAKER:
		case CORB_CD_HEADPHONE:
		case CORB_CD_SPDIFOUT:
		case CORB_CD_DIGITALOUT:
			pintype = PIN_DIR_OUT;
			break;
		case CORB_CD_CD:
		case CORB_CD_LINEIN:
			pintype = PIN_DIR_IN;
			break;
		case CORB_CD_MICIN:
			pintype = PIN_DIR_MIC;
			break;
		}

		switch (pintype) {
		case PIN_DIR_IN:
			this->comresp(this, w->nid,
			    CORB_SET_PIN_WIDGET_CONTROL,
			    CORB_PWC_INPUT, NULL);
			break;
		case PIN_DIR_OUT:
			this->comresp(this, w->nid,
			    CORB_SET_PIN_WIDGET_CONTROL,
			    CORB_PWC_OUTPUT, NULL);
			break;
		case PIN_DIR_MIC:
			this->comresp(this, w->nid,
			    CORB_SET_PIN_WIDGET_CONTROL,
			    CORB_PWC_INPUT|CORB_PWC_VREF_80, NULL);
			break;
		}

		if (w->d.pin.cap & COP_PINCAP_EAPD) {
			uint32_t result;
			int err;

			err = this->comresp(this, w->nid,
			    CORB_GET_EAPD_BTL_ENABLE, 0, &result);
			if (err)
				continue;
			result &= 0xff;
			result |= CORB_EAPD_EAPD;
			err = this->comresp(this, w->nid,
			    CORB_SET_EAPD_BTL_ENABLE, result, &result);
			if (err)
				continue;
		}
	}

	return 0;
}

static int
generic_mixer_widget_name(const codec_t *this, widget_t *w)
{
	const char *name = NULL, *grossloc = "", *geoloc = "";
	uint8_t grosslocval, geolocval;

	if (w->type != COP_AWTYPE_PIN_COMPLEX)
		return 0;

	switch (w->d.pin.device) {
	case CORB_CD_LINEOUT:	name = "lineout";	break;
	case CORB_CD_SPEAKER:	name = "spkr";		break;
	case CORB_CD_HEADPHONE:	name = "hp";		break;
	case CORB_CD_CD:	name = AudioNcd;	break;
	case CORB_CD_SPDIFOUT:	name = "spdifout";	break;
	case CORB_CD_DIGITALOUT: name = "digout";	break;
	case CORB_CD_MODEMLINE:	name = "modemline";	break;
	case CORB_CD_MODEMHANDSET: name = "modemhset";	break;
	case CORB_CD_LINEIN:	name = "linein";	break;
	case CORB_CD_AUX:	name = AudioNaux;	break;
	case CORB_CD_MICIN:	name = AudioNmicrophone; break;
	case CORB_CD_TELEPHONY:	name = "telephony";	break;
	case CORB_CD_SPDIFIN:	name = "spdifin";	break;
	case CORB_CD_DIGITALIN:	name = "digin";		break;
	case CORB_CD_DEVICE_OTHER: name = "reserved";	break;
	default:		name = "unused";	break;
	}

	grosslocval = ((w->d.pin.config & CORB_CD_LOCATION_MASK) >> 24) & 0xf;
	geolocval = (w->d.pin.config & CORB_CD_LOCATION_MASK) >> 28;

	switch (geolocval) {
	case 0x00:	/* external on primary chassis */
	case 0x10:	/* external on separate chassis */
		geoloc = (geolocval == 0x00 ? "" : "d");
		switch (grosslocval) {
		case 0x00:	grossloc = "";		break;	/* N/A */
		case 0x01:	grossloc = "";		break;	/* rear */
		case 0x02:	grossloc = ".front";	break;	/* front */
		case 0x03:	grossloc = ".left";	break;	/* left */
		case 0x04:	grossloc = ".right";	break;	/* right */
		case 0x05:	grossloc = ".top";	break;	/* top */
		case 0x06:	grossloc = ".bottom";	break;	/* bottom */
		case 0x07:	grossloc = ".rearpnl";	break;	/* rear panel */
		case 0x08:	grossloc = ".drivebay";	break;	/* drive bay */
		default:	grossloc = "";		break;
		}
		break;
	case 0x01:
		geoloc = "i";
		switch (grosslocval) {
		case 0x00:	grossloc = "";		break;	/* N/A */
		case 0x07:	grossloc = ".riser";	break;	/* riser */
		case 0x08:	grossloc = ".hdmi";	break;	/* hdmi */
		default:	grossloc = "";		break;
		}
		break;
	default:
		geoloc = "o";
		switch (grosslocval) {
		case 0x00:	grossloc = "";		break;	/* N/A */
		case 0x06:	grossloc = ".bottom";	break;	/* bottom */
		case 0x07:	grossloc = ".lidin";	break;	/* lid inside */
		case 0x08:	grossloc = ".lidout";	break;	/* lid outside */
		default:	grossloc = "";		break;
		}
		break;
	}

	snprintf(w->name, sizeof(w->name), "%s%s%s", geoloc, name, grossloc);
	return 0;
}

static int
generic_mixer_create_virtual(codec_t *this)
{
	mixer_item_t *m;
	mixer_devinfo_t *d;
	convgroup_t *cgdac = &this->dacs.groups[0];
	convgroup_t *cgadc = &this->adcs.groups[0];
	int i, err, mdac, madc, mmaster;

	/* Clear mixer indexes, to make generic_mixer_fix_index happy */
	for (i = 0; i < this->nmixers; i++) {
		d = &this->mixers[i].devinfo;
		d->index = d->prev = d->next = 0;
	}

	mdac = madc = mmaster = -1;
	for (i = 0; i < this->nmixers; i++) {
		if (this->mixers[i].devinfo.type != AUDIO_MIXER_VALUE)
			continue;
		if (mdac < 0 && this->dacs.ngroups > 0 && cgdac->nconv > 0) {
			if (this->mixers[i].nid == cgdac->conv[0])
				mdac = mmaster = i;
		}
		if (madc < 0 && this->adcs.ngroups > 0 && cgadc->nconv > 0) {
			if (this->mixers[i].nid == cgadc->conv[0])
				madc = i;
		}
	}

	if (mdac == -1) {
		/*
		 * no volume mixer found on the DAC; enumerate peer widgets
		 * and try to find a volume mixer on them
		 */
		widget_t *w;
		int j;
		FOR_EACH_WIDGET(this, i) {
			w = &this->w[i];
			for (j = 0; j < w->nconnections; j++)
				if (w->connections[j] == cgdac->conv[0])
					break;

			if (j == w->nconnections)
				continue;

			for (j = 0; j < this->nmixers; j++) {
				if (this->mixers[j].devinfo.type !=
				    AUDIO_MIXER_VALUE)
					continue;
				if (this->mixers[j].nid == w->nid) {
					mdac = mmaster = j;
					break;
				}
			}

			if (mdac == -1)
				break;
		}
	}

	if (mdac >= 0) {
		err = generic_mixer_ensure_capacity(this, this->nmixers + 1);
		if (err)
			return err;
		m = &this->mixers[this->nmixers];
		d = &m->devinfo;
		memcpy(m, &this->mixers[mmaster], sizeof(*m));
		d->mixer_class = AZ_CLASS_OUTPUT;
		snprintf(d->label.name, sizeof(d->label.name), AudioNmaster);
		this->nmixers++;

		err = generic_mixer_ensure_capacity(this, this->nmixers + 1);
		if (err)
			return err;
		m = &this->mixers[this->nmixers];
		d = &m->devinfo;
		memcpy(m, &this->mixers[mdac], sizeof(*m));
		d->mixer_class = AZ_CLASS_INPUT;
		snprintf(d->label.name, sizeof(d->label.name), AudioNdac);
		this->nmixers++;
	}

	if (madc >= 0) {
		err = generic_mixer_ensure_capacity(this, this->nmixers + 1);
		if (err)
			return err;
		m = &this->mixers[this->nmixers];
		d = &m->devinfo;
		memcpy(m, &this->mixers[madc], sizeof(*m));
		d->mixer_class = AZ_CLASS_RECORD;
		snprintf(d->label.name, sizeof(d->label.name), AudioNvolume);
		this->nmixers++;
	}

	generic_mixer_fix_indexes(this);

	return 0;
}

static int
generic_mixer_autoinit(codec_t *this)
{
	generic_mixer_init(this);
	generic_mixer_create_virtual(this);
	generic_mixer_pin_sense(this);

	return 0;
}

static int
generic_mixer_init_widget(const codec_t *this, widget_t *w, nid_t nid)
{
	return generic_mixer_widget_name(this, w);
}

static int
generic_mixer_delete(codec_t *this)
{
	if (this->mixers == NULL)
		return 0;
	kmem_free(this->mixers, this->szmixers);
	this->mixers = NULL;
	return 0;
}

/**
 * @param mc	mc->type must be set by the caller before the call
 */
static int
generic_mixer_get(const codec_t *this, nid_t nid, int target, mixer_ctrl_t *mc)
{
	uint32_t result;
	nid_t n;
	int err;

	/* inamp mute */
	if (IS_MI_TARGET_INAMP(target) && mc->type == AUDIO_MIXER_ENUM) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		    CORB_GAGM_INPUT | CORB_GAGM_LEFT |
		    MI_TARGET_INAMP(target), &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_GAGM_MUTE ? 1 : 0;
	}

	/* inamp gain */
	else if (IS_MI_TARGET_INAMP(target) && mc->type == AUDIO_MIXER_VALUE) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		      CORB_GAGM_INPUT | CORB_GAGM_LEFT |
		      MI_TARGET_INAMP(target), &result);
		if (err)
			return err;
		mc->un.value.level[0] = generic_mixer_from_device_value(this,
		    nid, target, CORB_GAGM_GAIN(result));
		if (this->w[nid].type == COP_AWTYPE_AUDIO_SELECTOR ||
		    this->w[nid].type == COP_AWTYPE_AUDIO_MIXER) {
			n = this->w[nid].connections[MI_TARGET_INAMP(target)];
#ifdef AZALIA_DEBUG
			if (!VALID_WIDGET_NID(n, this)) {
				DPRINTF(("%s: invalid target: nid=%d nconn=%d index=%d\n",
				   __func__, nid, this->w[nid].nconnections,
				   MI_TARGET_INAMP(target)));
				return EINVAL;
			}
#endif
		} else {
			n = nid;
		}
		mc->un.value.num_channels = WIDGET_CHANNELS(&this->w[n]);
		if (mc->un.value.num_channels == 2) {
			err = this->comresp(this, nid,
			    CORB_GET_AMPLIFIER_GAIN_MUTE, CORB_GAGM_INPUT |
			    CORB_GAGM_RIGHT | MI_TARGET_INAMP(target),
			    &result);
			if (err)
				return err;
			mc->un.value.level[1] = generic_mixer_from_device_value
			    (this, nid, target, CORB_GAGM_GAIN(result));
		}
	}

	/* outamp mute */
	else if (target == MI_TARGET_OUTAMP && mc->type == AUDIO_MIXER_ENUM) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		    CORB_GAGM_OUTPUT | CORB_GAGM_LEFT | 0, &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_GAGM_MUTE ? 1 : 0;
	}

	/* outamp gain */
	else if (target == MI_TARGET_OUTAMP && mc->type == AUDIO_MIXER_VALUE) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		      CORB_GAGM_OUTPUT | CORB_GAGM_LEFT | 0, &result);
		if (err)
			return err;
		mc->un.value.level[0] = generic_mixer_from_device_value(this,
		    nid, target, CORB_GAGM_GAIN(result));
		mc->un.value.num_channels = WIDGET_CHANNELS(&this->w[nid]);
		if (mc->un.value.num_channels == 2) {
			err = this->comresp(this, nid,
			    CORB_GET_AMPLIFIER_GAIN_MUTE,
			    CORB_GAGM_OUTPUT | CORB_GAGM_RIGHT | 0, &result);
			if (err)
				return err;
			mc->un.value.level[1] = generic_mixer_from_device_value
			    (this, nid, target, CORB_GAGM_GAIN(result));
		}
	}

	/* selection */
	else if (target == MI_TARGET_CONNLIST) {
		err = this->comresp(this, nid,
		    CORB_GET_CONNECTION_SELECT_CONTROL, 0, &result);
		if (err)
			return err;
		result = CORB_CSC_INDEX(result);
		if (!VALID_WIDGET_NID(this->w[nid].connections[result], this))
			mc->un.ord = -1;
		else
			mc->un.ord = result;
	}

	/* pin I/O */
	else if (target == MI_TARGET_PINDIR) {
		err = this->comresp(this, nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_PWC_OUTPUT ? 1 : 0;
	}

	/* pin headphone-boost */
	else if (target == MI_TARGET_PINBOOST) {
		err = this->comresp(this, nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_PWC_HEADPHONE ? 1 : 0;
	}

	/* DAC group selection */
	else if (target == MI_TARGET_DAC) {
		mc->un.ord = this->dacs.cur;
	}

	/* ADC selection */
	else if (target == MI_TARGET_ADC) {
		mc->un.ord = this->adcs.cur;
	}

	/* Volume knob */
	else if (target == MI_TARGET_VOLUME) {
		err = this->comresp(this, nid, CORB_GET_VOLUME_KNOB,
		    0, &result);
		if (err)
			return err;
		mc->un.value.level[0] = generic_mixer_from_device_value(this,
		    nid, target, CORB_VKNOB_VOLUME(result));
		mc->un.value.num_channels = 1;
	}

	/* S/PDIF */
	else if (target == MI_TARGET_SPDIF) {
		err = this->comresp(this, nid, CORB_GET_DIGITAL_CONTROL,
		    0, &result);
		if (err)
			return err;
		mc->un.mask = result & 0xff & ~(CORB_DCC_DIGEN | CORB_DCC_NAUDIO);
	} else if (target == MI_TARGET_SPDIF_CC) {
		err = this->comresp(this, nid, CORB_GET_DIGITAL_CONTROL,
		    0, &result);
		if (err)
			return err;
		mc->un.value.num_channels = 1;
		mc->un.value.level[0] = CORB_DCC_CC(result);
	}

	/* EAPD */
	else if (target == MI_TARGET_EAPD) {
		err = this->comresp(this, nid,
		    CORB_GET_EAPD_BTL_ENABLE, 0, &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_EAPD_EAPD ? 1 : 0;
	}

	/* Balanced I/O */
	else if (target == MI_TARGET_BALANCE) {
		err = this->comresp(this, nid,
		    CORB_GET_EAPD_BTL_ENABLE, 0, &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_EAPD_BTL ? 1 : 0;
	}

	/* LR-Swap */
	else if (target == MI_TARGET_LRSWAP) {
		err = this->comresp(this, nid,
		    CORB_GET_EAPD_BTL_ENABLE, 0, &result);
		if (err)
			return err;
		mc->un.ord = result & CORB_EAPD_LRSWAP ? 1 : 0;
	}

	else {
		aprint_error_dev(this->dev, "internal error in %s: target=%x\n",
		    __func__, target);
		return -1;
	}
	return 0;
}

static int
generic_mixer_set(codec_t *this, nid_t nid, int target, const mixer_ctrl_t *mc)
{
	uint32_t result, value;
	int err;

	/* inamp mute */
	if (IS_MI_TARGET_INAMP(target) && mc->type == AUDIO_MIXER_ENUM) {
		/* We have to set stereo mute separately to keep each gain value. */
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		    CORB_GAGM_INPUT | CORB_GAGM_LEFT |
		    MI_TARGET_INAMP(target), &result);
		if (err)
			return err;
		value = CORB_AGM_INPUT | CORB_AGM_LEFT |
		    (target << CORB_AGM_INDEX_SHIFT) |
		    CORB_GAGM_GAIN(result);
		if (mc->un.ord)
			value |= CORB_AGM_MUTE;
		err = this->comresp(this, nid, CORB_SET_AMPLIFIER_GAIN_MUTE,
		    value, &result);
		if (err)
			return err;
		if (WIDGET_CHANNELS(&this->w[nid]) == 2) {
			err = this->comresp(this, nid,
			    CORB_GET_AMPLIFIER_GAIN_MUTE, CORB_GAGM_INPUT |
			    CORB_GAGM_RIGHT | MI_TARGET_INAMP(target),
			    &result);
			if (err)
				return err;
			value = CORB_AGM_INPUT | CORB_AGM_RIGHT |
			    (target << CORB_AGM_INDEX_SHIFT) |
			    CORB_GAGM_GAIN(result);
			if (mc->un.ord)
				value |= CORB_AGM_MUTE;
			err = this->comresp(this, nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, value, &result);
			if (err)
				return err;
		}
	}

	/* inamp gain */
	else if (IS_MI_TARGET_INAMP(target) && mc->type == AUDIO_MIXER_VALUE) {
		if (mc->un.value.num_channels < 1)
			return EINVAL;
		if (!generic_mixer_validate_value(this, nid, target,
		    mc->un.value.level[0]))
			return EINVAL;
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		      CORB_GAGM_INPUT | CORB_GAGM_LEFT |
		      MI_TARGET_INAMP(target), &result);
		if (err)
			return err;
		value = generic_mixer_to_device_value(this, nid, target,
		    mc->un.value.level[0]);
		value = CORB_AGM_INPUT | CORB_AGM_LEFT |
		    (target << CORB_AGM_INDEX_SHIFT) |
		    (result & CORB_GAGM_MUTE ? CORB_AGM_MUTE : 0) |
		    (value & CORB_AGM_GAIN_MASK);
		err = this->comresp(this, nid, CORB_SET_AMPLIFIER_GAIN_MUTE,
		    value, &result);
		if (err)
			return err;
		if (mc->un.value.num_channels >= 2 &&
		    WIDGET_CHANNELS(&this->w[nid]) == 2) {
			if (!generic_mixer_validate_value(this, nid, target,
			    mc->un.value.level[1]))
				return EINVAL;
			err = this->comresp(this, nid,
			      CORB_GET_AMPLIFIER_GAIN_MUTE, CORB_GAGM_INPUT |
			      CORB_GAGM_RIGHT | MI_TARGET_INAMP(target),
			      &result);
			if (err)
				return err;
			value = generic_mixer_to_device_value(this, nid, target,
			    mc->un.value.level[1]);
			value = CORB_AGM_INPUT | CORB_AGM_RIGHT |
			    (target << CORB_AGM_INDEX_SHIFT) |
			    (result & CORB_GAGM_MUTE ? CORB_AGM_MUTE : 0) |
			    (value & CORB_AGM_GAIN_MASK);
			err = this->comresp(this, nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, value, &result);
			if (err)
				return err;
		}
	}

	/* outamp mute */
	else if (target == MI_TARGET_OUTAMP && mc->type == AUDIO_MIXER_ENUM) {
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		    CORB_GAGM_OUTPUT | CORB_GAGM_LEFT, &result);
		if (err)
			return err;
		value = CORB_AGM_OUTPUT | CORB_AGM_LEFT | CORB_GAGM_GAIN(result);
		if (mc->un.ord)
			value |= CORB_AGM_MUTE;
		err = this->comresp(this, nid, CORB_SET_AMPLIFIER_GAIN_MUTE,
		    value, &result);
		if (err)
			return err;
		if (WIDGET_CHANNELS(&this->w[nid]) == 2) {
			err = this->comresp(this, nid,
			    CORB_GET_AMPLIFIER_GAIN_MUTE,
			    CORB_GAGM_OUTPUT | CORB_GAGM_RIGHT, &result);
			if (err)
				return err;
			value = CORB_AGM_OUTPUT | CORB_AGM_RIGHT |
			    CORB_GAGM_GAIN(result);
			if (mc->un.ord)
				value |= CORB_AGM_MUTE;
			err = this->comresp(this, nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, value, &result);
			if (err)
				return err;
		}
	}

	/* outamp gain */
	else if (target == MI_TARGET_OUTAMP && mc->type == AUDIO_MIXER_VALUE) {
		if (mc->un.value.num_channels < 1)
			return EINVAL;
		if (!generic_mixer_validate_value(this, nid, target,
		    mc->un.value.level[0]))
			return EINVAL;
		err = this->comresp(this, nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
		      CORB_GAGM_OUTPUT | CORB_GAGM_LEFT, &result);
		if (err)
			return err;
		value = generic_mixer_to_device_value(this, nid, target,
		    mc->un.value.level[0]);
		value = CORB_AGM_OUTPUT | CORB_AGM_LEFT |
		    (result & CORB_GAGM_MUTE ? CORB_AGM_MUTE : 0) |
		    (value & CORB_AGM_GAIN_MASK);
		err = this->comresp(this, nid, CORB_SET_AMPLIFIER_GAIN_MUTE,
		    value, &result);
		if (err)
			return err;
		if (mc->un.value.num_channels >= 2 &&
		    WIDGET_CHANNELS(&this->w[nid]) == 2) {
			if (!generic_mixer_validate_value(this, nid, target,
			    mc->un.value.level[1]))
				return EINVAL;
			err = this->comresp(this, nid,
			      CORB_GET_AMPLIFIER_GAIN_MUTE, CORB_GAGM_OUTPUT |
			      CORB_GAGM_RIGHT, &result);
			if (err)
				return err;
			value = generic_mixer_to_device_value(this, nid, target,
			    mc->un.value.level[1]);
			value = CORB_AGM_OUTPUT | CORB_AGM_RIGHT |
			    (result & CORB_GAGM_MUTE ? CORB_AGM_MUTE : 0) |
			    (value & CORB_AGM_GAIN_MASK);
			err = this->comresp(this, nid,
			    CORB_SET_AMPLIFIER_GAIN_MUTE, value, &result);
			if (err)
				return err;
		}
	}

	/* selection */
	else if (target == MI_TARGET_CONNLIST) {
		if (mc->un.ord < 0 ||
		    mc->un.ord >= this->w[nid].nconnections ||
		    !VALID_WIDGET_NID(this->w[nid].connections[mc->un.ord], this))
			return EINVAL;
		err = this->comresp(this, nid,
		    CORB_SET_CONNECTION_SELECT_CONTROL, mc->un.ord, &result);
		if (err)
			return err;
	}

	/* pin I/O */
	else if (target == MI_TARGET_PINDIR) {
		if (mc->un.ord >= 2)
			return EINVAL;
		if (mc->un.ord == 0) {
			return generic_mixer_pinctrl(this, nid, CORB_PWC_INPUT);
		} else {
			return generic_mixer_pinctrl(
			    this, nid, CORB_PWC_OUTPUT);
		}
	}

	/* pin headphone-boost */
	else if (target == MI_TARGET_PINBOOST) {
		if (mc->un.ord >= 2)
			return EINVAL;
		err = this->comresp(this, nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err)
			return err;
		if (mc->un.ord == 0) {
			result &= ~CORB_PWC_HEADPHONE;
		} else {
			result |= CORB_PWC_HEADPHONE;
		}
		err = this->comresp(this, nid,
		    CORB_SET_PIN_WIDGET_CONTROL, result, &result);
		if (err)
			return err;
	}

	/* DAC group selection */
	else if (target == MI_TARGET_DAC) {
		if (this->running)
			return EBUSY;
		if (mc->un.ord >= this->dacs.ngroups)
			return EINVAL;
		return azalia_codec_construct_format(this,
		    mc->un.ord, this->adcs.cur);
	}

	/* ADC selection */
	else if (target == MI_TARGET_ADC) {
		if (this->running)
			return EBUSY;
		if (mc->un.ord >= this->adcs.ngroups)
			return EINVAL;
		return azalia_codec_construct_format(this,
		    this->dacs.cur, mc->un.ord);
	}

	/* Volume knob */
	else if (target == MI_TARGET_VOLUME) {
		if (mc->un.value.num_channels != 1)
			return EINVAL;
		if (!generic_mixer_validate_value(this, nid,
		    target, mc->un.value.level[0]))
			return EINVAL;
		value = generic_mixer_to_device_value(this, nid, target,
		     mc->un.value.level[0]) | CORB_VKNOB_DIRECT;
		err = this->comresp(this, nid, CORB_SET_VOLUME_KNOB,
		   value, &result);
		if (err)
			return err;
	}

	/* S/PDIF */
	else if (target == MI_TARGET_SPDIF) {
		err = this->comresp(this, nid, CORB_GET_DIGITAL_CONTROL,
		    0, &result);
		result &= CORB_DCC_DIGEN | CORB_DCC_NAUDIO;
		result |= mc->un.mask & 0xff & ~CORB_DCC_DIGEN;
		err = this->comresp(this, nid, CORB_SET_DIGITAL_CONTROL_L,
		    result, NULL);
		if (err)
			return err;
	} else if (target == MI_TARGET_SPDIF_CC) {
		if (mc->un.value.num_channels != 1)
			return EINVAL;
		if (mc->un.value.level[0] > 127)
			return EINVAL;
		err = this->comresp(this, nid, CORB_SET_DIGITAL_CONTROL_H,
		    mc->un.value.level[0], NULL);
		if (err)
			return err;
	}

	/* EAPD */
	else if (target == MI_TARGET_EAPD) {
		if (mc->un.ord >= 2)
			return EINVAL;
		err = this->comresp(this, nid,
		    CORB_GET_EAPD_BTL_ENABLE, 0, &result);
		if (err)
			return err;
		result &= 0xff;
		if (mc->un.ord == 0) {
			result &= ~CORB_EAPD_EAPD;
		} else {
			result |= CORB_EAPD_EAPD;
		}
		err = this->comresp(this, nid,
		    CORB_SET_EAPD_BTL_ENABLE, result, &result);
		if (err)
			return err;
	}

	/* Balanced I/O */
	else if (target == MI_TARGET_BALANCE) {
		if (mc->un.ord >= 2)
			return EINVAL;
		err = this->comresp(this, nid,
		    CORB_GET_EAPD_BTL_ENABLE, 0, &result);
		if (err)
			return err;
		result &= 0xff;
		if (mc->un.ord == 0) {
			result &= ~CORB_EAPD_BTL;
		} else {
			result |= CORB_EAPD_BTL;
		}
		err = this->comresp(this, nid,
		    CORB_SET_EAPD_BTL_ENABLE, result, &result);
		if (err)
			return err;
	}

	/* LR-Swap */
	else if (target == MI_TARGET_LRSWAP) {
		if (mc->un.ord >= 2)
			return EINVAL;
		err = this->comresp(this, nid,
		    CORB_GET_EAPD_BTL_ENABLE, 0, &result);
		if (err)
			return err;
		result &= 0xff;
		if (mc->un.ord == 0) {
			result &= ~CORB_EAPD_LRSWAP;
		} else {
			result |= CORB_EAPD_LRSWAP;
		}
		err = this->comresp(this, nid,
		    CORB_SET_EAPD_BTL_ENABLE, result, &result);
		if (err)
			return err;
	}

	else {
		aprint_error_dev(this->dev, "internal error in %s: target=%x\n",
		    __func__, target);
		return -1;
	}
	return 0;
}

static int
generic_mixer_pinctrl(codec_t *this, nid_t nid, uint32_t value)
{
	int err;
	uint32_t result;

	err = this->comresp(this, nid, CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
	if (err)
		return err;
	result &= ~(CORB_PWC_OUTPUT | CORB_PWC_INPUT);
	result |= value & (CORB_PWC_OUTPUT | CORB_PWC_INPUT);
	return this->comresp(this, nid,
	    CORB_SET_PIN_WIDGET_CONTROL, result, NULL);
}

static u_char
generic_mixer_from_device_value(const codec_t *this, nid_t nid, int target,
    uint32_t dv)
{
#ifdef MAX_VOLUME_255
	uint32_t dmax;

	if (IS_MI_TARGET_INAMP(target))
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].inamp_cap);
	else if (target == MI_TARGET_OUTAMP)
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].outamp_cap);
	else if (target == MI_TARGET_VOLUME)
		dmax = COP_VKCAP_NUMSTEPS(this->w[nid].d.volume.cap);
	else {
		printf("unknown target: %d\n", target);
		dmax = 127;
	}
	return dv * MIXER_DELTA(dmax);
#else
	return dv;
#endif
}

static uint32_t
generic_mixer_to_device_value(const codec_t *this, nid_t nid, int target,
    u_char uv)
{
#ifdef MAX_VOLUME_255
	uint32_t dmax;

	if (IS_MI_TARGET_INAMP(target))
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].inamp_cap);
	else if (target == MI_TARGET_OUTAMP)
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].outamp_cap);
	else if (target == MI_TARGET_VOLUME)
		dmax = COP_VKCAP_NUMSTEPS(this->w[nid].d.volume.cap);
	else {
		printf("unknown target: %d\n", target);
		dmax = 127;
	}
	return uv / MIXER_DELTA(dmax);
#else
	return uv;
#endif
}

static uint32_t
generic_mixer_max(const codec_t *this, nid_t nid,
    int target)
{
#ifdef MAX_VOLUME_255
	return AUDIO_MAX_GAIN;
#else
	uint32_t dmax;

	if (IS_MI_TARGET_INAMP(target))
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].inamp_cap);
	else if (target == MI_TARGET_OUTAMP)
		dmax = COP_AMPCAP_NUMSTEPS(this->w[nid].outamp_cap);
	else if (target == MI_TARGET_VOLUME)
		dmax = COP_VKCAP_NUMSTEPS(this->w[nid].d.volume.cap);
	return dmax;
#endif
}

static bool
generic_mixer_validate_value(const codec_t *this, nid_t nid,
    int target, u_char uv)
{
#ifdef MAX_VOLUME_255
	return TRUE;
#else
	return uv <= generic_mixer_max(this, nid, target);
#endif
}

static int
generic_set_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	if (mc->type != m->devinfo.type)
		return EINVAL;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;	/* nothing to do */
	return generic_mixer_set(this, m->nid, m->target, mc);
}

static int
generic_get_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	mc->type = m->devinfo.type;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;	/* nothing to do */
	return generic_mixer_get(this, m->nid, m->target, mc);
}


/* ----------------------------------------------------------------
 * Realtek ALC260
 *
 * Fujitsu LOOX T70M/T
 *	Internal Speaker: 0x10
 *	Front Headphone: 0x14
 *	Front mic: 0x12
 * ---------------------------------------------------------------- */

#define ALC260_FUJITSU_ID	0x132610cf
#define ALC260_EVENT_HP		0
#define ALC260_EXTRA_MASTER	0
static const mixer_item_t alc260_mixer_items[] = {
	AZ_MIXER_CLASSES,
	{{0, {AudioNmaster, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, 3}}, 0x08, MI_TARGET_OUTAMP}, /* and 0x09, 0x0a(mono) */
	{{0, {AudioNmaster".mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x0f, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x10, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x10, MI_TARGET_PINBOOST},
	{{0, {AudioNmono".mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x11, MI_TARGET_OUTAMP},
	{{0, {"mic1.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x12, MI_TARGET_OUTAMP},
	{{0, {"mic1", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x12, MI_TARGET_PINDIR},
	{{0, {"mic2.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x13, MI_TARGET_OUTAMP},
	{{0, {"mic2", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x13, MI_TARGET_PINDIR},
	{{0, {"line1.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {"line1", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x14, MI_TARGET_PINDIR},
	{{0, {"line2.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_OUTAMP},
	{{0, {"line2", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x15, MI_TARGET_PINDIR},

	{{0, {AudioNdac".mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)}, /* and 0x09, 0x0a(mono) */
	{{0, {"mic1.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {"mic1", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {"mic2.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(1)},
	{{0, {"mic2", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(1)},
	{{0, {"line1.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(2)},
	{{0, {"line1", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(2)},
	{{0, {"line2.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(3)},
	{{0, {"line2", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(3)},
	{{0, {AudioNcd".mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(4)},
	{{0, {AudioNcd, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(4)},
	{{0, {AudioNspeaker".mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(5)},
	{{0, {AudioNspeaker, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(5)},

	{{0, {"adc04.source", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={5, {{{"mic1", 0}, 0}, {{"mic2", 0}, 1}, {{"line1", 0}, 2},
		     {{"line2", 0}, 3}, {{AudioNcd, 0}, 4}}}},
	 0x04, MI_TARGET_CONNLIST},
	{{0, {"adc04.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  ENUM_OFFON}, 0x04, MI_TARGET_INAMP(0)},
	{{0, {"adc04", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD, 0, 0,
	  .un.v={{"", 0}, 2, MIXER_DELTA(35)}}, 0x04, MI_TARGET_INAMP(0)},
	{{0, {"adc05.source", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={6, {{{"mic1", 0}, 0}, {{"mic2", 0}, 1}, {{"line1", 0}, 2},
		     {{"line2", 0}, 3}, {{AudioNcd, 0}, 4}, {{AudioNmixerout, 0}, 5}}}},
	 0x05, MI_TARGET_CONNLIST},
	{{0, {"adc05.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  ENUM_OFFON}, 0x05, MI_TARGET_INAMP(0)},
	{{0, {"adc05", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD, 0, 0,
	  .un.v={{"", 0}, 2, MIXER_DELTA(35)}}, 0x05, MI_TARGET_INAMP(0)},

	{{0, {AudioNmode, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_PLAYBACK, 0, 0,
	  .un.e={2, {{{"analog", 0}, 0}, {{AzaliaNdigital, 0}, 1}}}}, 0, MI_TARGET_DAC},
	{{0, {AudioNmode, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={3, {{{"adc04", 0}, 0}, {{"adc05", 0}, 1}, {{AzaliaNdigital, 0}, 2}}}},
	 0, MI_TARGET_ADC},
};

static const mixer_item_t alc260_loox_mixer_items[] = {
	AZ_MIXER_CLASSES,

	{{0, {AudioNmaster, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, 3}}, 0x08, MI_TARGET_OUTAMP}, /* and 0x09, 0x0a(mono) */
	{{0, {AudioNmaster".mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x10, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x10, MI_TARGET_PINBOOST},
	{{0, {AudioNheadphone".mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_PINBOOST},

	{{0, {AudioNdac".mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)}, /* and 0x09, 0x0a(mono) */
	{{0, {AudioNmicrophone".mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {AudioNmicrophone, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {AudioNcd".mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(4)},
	{{0, {AudioNcd, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(4)},
	{{0, {AudioNspeaker".mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(5)},
	{{0, {AudioNspeaker, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(65)}}, 0x07, MI_TARGET_INAMP(5)},

	{{0, {"adc04.source", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={2, {{{AudioNmicrophone, 0}, 0}, {{AudioNcd, 0}, 4}}}}, 0x04, MI_TARGET_CONNLIST},
	{{0, {"adc04.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  ENUM_OFFON}, 0x04, MI_TARGET_INAMP(0)},
	{{0, {"adc04", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD, 0, 0,
	  .un.v={{"", 0}, 2, MIXER_DELTA(35)}}, 0x04, MI_TARGET_INAMP(0)},
	{{0, {"adc05.source", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={3, {{{AudioNmicrophone, 0}, 0}, {{AudioNcd, 0}, 4}, {{AudioNmixerout, 0}, 5}}}},
	 0x05, MI_TARGET_CONNLIST},
	{{0, {"adc05.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  ENUM_OFFON}, 0x05, MI_TARGET_INAMP(0)},
	{{0, {"adc05", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD, 0, 0,
	  .un.v={{"", 0}, 2, MIXER_DELTA(35)}}, 0x05, MI_TARGET_INAMP(0)},

	{{0, {AudioNmode, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_PLAYBACK, 0, 0,
	  .un.e={2, {{{"analog", 0}, 0}, {{AzaliaNdigital, 0}, 1}}}}, 0, MI_TARGET_DAC},
	{{0, {AudioNmode, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={3, {{{"adc04", 0}, 0}, {{"adc05", 0}, 1}, {{AzaliaNdigital, 0}, 2}}}},
	 0, MI_TARGET_ADC},
};

static int
alc260_mixer_init(codec_t *this)
{
	const mixer_item_t *mi;
	mixer_ctrl_t mc;
	uint32_t value;

	switch (this->subid) {
	case ALC260_FUJITSU_ID:
		this->nmixers = __arraycount(alc260_loox_mixer_items);
		mi = alc260_loox_mixer_items;
		break;
	default:
		this->nmixers = __arraycount(alc260_mixer_items);
		mi = alc260_mixer_items;
	}
	this->szmixers = sizeof(mixer_item_t) * this->nmixers;
	this->mixers = kmem_alloc(this->szmixers, KM_SLEEP);
	if (this->mixers == NULL) {
		aprint_error_dev(this->dev, "out of memory in %s\n", __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, mi, sizeof(mixer_item_t) * this->nmixers);
	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);

	mc.dev = -1;		/* no need for generic_mixer_set() */
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 1;		/* pindir: output */
	generic_mixer_set(this, 0x0f, MI_TARGET_PINDIR, &mc); /* lineout */
	generic_mixer_set(this, 0x10, MI_TARGET_PINDIR, &mc); /* headphones */
	mc.un.ord = 0;		/* pindir: input */
	generic_mixer_set(this, 0x12, MI_TARGET_PINDIR, &mc); /* mic1 */
	generic_mixer_set(this, 0x13, MI_TARGET_PINDIR, &mc); /* mic2 */
	generic_mixer_set(this, 0x14, MI_TARGET_PINDIR, &mc); /* line1 */
	generic_mixer_set(this, 0x15, MI_TARGET_PINDIR, &mc); /* line2 */
	mc.un.ord = 0;		/* mute: off */
	generic_mixer_set(this, 0x08, MI_TARGET_INAMP(0), &mc);
	generic_mixer_set(this, 0x08, MI_TARGET_INAMP(1), &mc);
	generic_mixer_set(this, 0x09, MI_TARGET_INAMP(0), &mc);
	generic_mixer_set(this, 0x09, MI_TARGET_INAMP(1), &mc);
	generic_mixer_set(this, 0x0a, MI_TARGET_INAMP(0), &mc);
	generic_mixer_set(this, 0x0a, MI_TARGET_INAMP(1), &mc);
	if (this->subid == ALC260_FUJITSU_ID) {
		mc.un.ord = 1;	/* pindir: output */
		generic_mixer_set(this, 0x14, MI_TARGET_PINDIR, &mc); /* line1 */
		mc.un.ord = 4;	/* connlist: cd */
		generic_mixer_set(this, 0x05, MI_TARGET_CONNLIST, &mc);
		/* setup a unsolicited event for the headphones */
		this->comresp(this, 0x14, CORB_SET_UNSOLICITED_RESPONSE,
		    CORB_UNSOL_ENABLE | ALC260_EVENT_HP, NULL);
		this->extra[ALC260_EXTRA_MASTER] = 0; /* unmute */
		/* If the headphone presents, mute the internal speaker */
		this->comresp(this, 0x14, CORB_GET_PIN_SENSE, 0, &value);
		mc.un.ord = value & CORB_PS_PRESENCE ? 1 : 0;
		generic_mixer_set(this, 0x10, MI_TARGET_OUTAMP, &mc);
		this->get_port = alc260_get_port;
	}
	return 0;
}

static int
alc260_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{1, {0x02}},	/* analog 2ch */
		 {1, {0x03}}}};	/* digital */
	static const convgroupset_t adcs = {
		-1, 3,
		{{1, {0x04}},	/* analog 2ch */
		 {1, {0x05}},	/* analog 2ch */
		 {1, {0x06}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

static int
alc260_set_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;
	mixer_ctrl_t mc2;
	uint32_t value;
	int err;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	if (mc->type != m->devinfo.type)
		return EINVAL;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if (m->nid == 0x08 && m->target == MI_TARGET_OUTAMP) {
		DPRINTF(("%s: hook for outputs.master\n", __func__));
		err = generic_mixer_set(this, m->nid, m->target, mc);
		if (!err) {
			generic_mixer_set(this, 0x09, m->target, mc);
			mc2 = *mc;
			mc2.un.value.num_channels = 1;
			mc2.un.value.level[0] = (mc2.un.value.level[0]
			    + mc2.un.value.level[1]) / 2;
			generic_mixer_set(this, 0x0a, m->target, &mc2);
		}
		return err;
	} else if (m->nid == 0x08 && m->target == MI_TARGET_INAMP(0)) {
		DPRINTF(("%s: hook for inputs.dac.mute\n", __func__));
		err = generic_mixer_set(this, m->nid, m->target, mc);
		if (!err) {
			generic_mixer_set(this, 0x09, m->target, mc);
			generic_mixer_set(this, 0x0a, m->target, mc);
		}
		return err;
	} else if (m->nid == 0x04 &&
		   m->target == MI_TARGET_CONNLIST &&
		   m->devinfo.un.e.num_mem == 2) {
		if (1 <= mc->un.ord && mc->un.ord <= 3)
			return EINVAL;
	} else if (m->nid == 0x05 &&
		   m->target == MI_TARGET_CONNLIST &&
		   m->devinfo.un.e.num_mem == 3) {
		if (1 <= mc->un.ord && mc->un.ord <= 3)
			return EINVAL;
	} else if (this->subid == ALC260_FUJITSU_ID && m->nid == 0x10 &&
		   m->target == MI_TARGET_OUTAMP) {
		if (mc->un.ord != 0 && mc->un.ord != 1)
			return EINVAL;
		this->extra[ALC260_EXTRA_MASTER] = mc->un.ord;
		err = this->comresp(this, 0x14, CORB_GET_PIN_SENSE, 0, &value);
		if (err)
			return err;
		if (!(value & CORB_PS_PRESENCE)) {
			return generic_mixer_set(this, m->nid, m->target, mc);
		}
		return 0;
	}
	return generic_mixer_set(this, m->nid, m->target, mc);
}

static int
alc260_get_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	mc->type = m->devinfo.type;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if (this->subid == ALC260_FUJITSU_ID && m->nid == 0x10 &&
	    m->target == MI_TARGET_OUTAMP) {
		mc->un.ord = this->extra[ALC260_EXTRA_MASTER];
		return 0;
	}
	return generic_mixer_get(this, m->nid, m->target, mc);
}

static int
alc260_unsol_event(codec_t *this, int tag)
{
	int err;
	uint32_t value;
	mixer_ctrl_t mc;

	switch (tag) {
	case ALC260_EVENT_HP:
		err = this->comresp(this, 0x14, CORB_GET_PIN_SENSE, 0, &value);
		if (err)
			break;
		mc.dev = -1;
		mc.type = AUDIO_MIXER_ENUM;
		if (value & CORB_PS_PRESENCE) {
			DPRINTF(("%s: headphone has been inserted.\n", __func__));
			mc.un.ord = 1; /* mute */
			generic_mixer_set(this, 0x10, MI_TARGET_OUTAMP, &mc);
		} else {
			DPRINTF(("%s: headphone has been pulled out.\n", __func__));
			mc.un.ord = this->extra[ALC260_EXTRA_MASTER];
			generic_mixer_set(this, 0x10, MI_TARGET_OUTAMP, &mc);
		}
		break;
	default:
		printf("%s: unknown tag: %d\n", __func__, tag);
	}
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC262
 * ---------------------------------------------------------------- */

static int
alc262_init_widget(const codec_t *this, widget_t *w, nid_t nid)
{
	switch (nid) {
	case 0x0c:
		strlcpy(w->name, AudioNmaster, sizeof(w->name));
		break;
	}

	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC268
 * ---------------------------------------------------------------- */

static int
alc268_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 1,
		{{2, {0x02, 0x03}}}}; /* analog 4ch */
	static const convgroupset_t adcs = {
		-1, 1,
		{{2, {0x08, 0x07}}}};	/* analog 4ch */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC662-GR
 * ---------------------------------------------------------------- */

static int
alc662_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 1,
		{{3, {0x02, 0x03, 0x04}}}}; /* analog 6ch */
	static const convgroupset_t adcs = {
		-1, 1,
		{{2, {0x09, 0x08}}}};	/* analog 4ch */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC861
 * ---------------------------------------------------------------- */

static int
alc861_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x03, 0x04, 0x05, 0x06}}, /* analog 8ch */
		 {1, {0x07}}}};	/* digital */
	static const convgroupset_t adcs = {
		-1, 1,
		{{1, {0x08}}}};	/* analog 2ch */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC861-VD-GR
 * ---------------------------------------------------------------- */

static int
alc861vdgr_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}}}};	/* digital */
	static const convgroupset_t adcs = {
		-1, 1,
		{{1, {0x09}}}};	/* analog 2ch */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC880
 * ---------------------------------------------------------------- */

static const mixer_item_t alc880_mixer_items[] = {
	AZ_MIXER_CLASSES,

	{{0, {AudioNsurround"."AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={5, {{{"mic1", 0}, 0}, {{"mic2", 0}, 1}, {{"line1", 0}, 2},
			   {{"line2", 0}, 3}, {{AudioNcd, 0}, 4}}}}, 0x08, MI_TARGET_CONNLIST},
	{{0, {AudioNsurround"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(35)}}, 0x08, MI_TARGET_INAMP(0)},

	{{0, {AzaliaNfront"."AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={5, {{{"mic1", 0}, 0}, {{"mic2", 0}, 1}, {{"line1", 0}, 2},
			   {{"line2", 0}, 3}, {{AudioNcd, 0}, 4},
			   {{AudioNmixerout, 0}, 5}}}}, 0x09, MI_TARGET_CONNLIST},
	{{0, {AzaliaNfront"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(35)}}, 0x09, MI_TARGET_INAMP(0)},

	{{0, {"mic1."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(0)},
	{{0, {"mic1", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(65)}}, 0x0b, MI_TARGET_INAMP(0)},
	{{0, {"mic2."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(1)},
	{{0, {"mic2", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(65)}}, 0x0b, MI_TARGET_INAMP(1)},
	{{0, {"line1."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(2)},
	{{0, {"line1", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(65)}}, 0x0b, MI_TARGET_INAMP(2)},
	{{0, {"line2."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(3)},
	{{0, {"line2", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(65)}}, 0x0b, MI_TARGET_INAMP(3)},
	{{0, {AudioNcd"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(4)},
	{{0, {AudioNcd, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(65)}}, 0x0b, MI_TARGET_INAMP(4)},
	{{0, {AudioNspeaker"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(5)},
	{{0, {AudioNspeaker, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 1, MIXER_DELTA(65)}}, 0x0b, MI_TARGET_INAMP(5)},

	{{0, {AudioNmaster, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(64)}}, 0x0c, MI_TARGET_OUTAMP},
	{{0, {AzaliaNfront".dac.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront".mixer.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_INAMP(1)},
	{{0, {AudioNsurround, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(64)}}, 0x0d, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(64)}}, 0x0e, MI_TARGET_OUTAMP},
	{{0, {AzaliaNside, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(64)}}, 0x0f, MI_TARGET_OUTAMP},
#if 0	/* The followings are useless. */
	{{0, {AudioNsurround".dac.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0d, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround".mixer.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0d, MI_TARGET_INAMP(1)},
	{{0, {AzaliaNclfe".dac.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0e, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNclfe".mixer.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0e, MI_TARGET_INAMP(1)},
	{{0, {AzaliaNside".dac.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0f, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNside".mixer.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0f, MI_TARGET_INAMP(1)},
#endif

	{{0, {AudioNmaster"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_PINBOOST},
	{{0, {AudioNsurround"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_OUTAMP},
	{{0, {AudioNsurround".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_PINBOOST},
	{{0, {AzaliaNclfe"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x16, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x16, MI_TARGET_PINBOOST},
	{{0, {AzaliaNside"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x17, MI_TARGET_OUTAMP},
	{{0, {AzaliaNside".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x17, MI_TARGET_PINBOOST},

	{{0, {"mic2."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x19, MI_TARGET_OUTAMP},
	{{0, {"mic2.boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x19, MI_TARGET_PINBOOST},
	{{0, {"mic2.dir", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x19, MI_TARGET_PINDIR},
	{{0, {"line1."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1a, MI_TARGET_OUTAMP},
	{{0, {"line1.boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1a, MI_TARGET_PINBOOST},
	{{0, {"line1.dir", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x1a, MI_TARGET_PINDIR},
	{{0, {"line2."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_OUTAMP},
	{{0, {"line2.boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_PINBOOST},
	{{0, {"line2.dir", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x1b, MI_TARGET_PINDIR},

	{{0, {AudioNmode, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_PLAYBACK, 0, 0,
	  .un.e={2, {{{"analog", 0}, 0}, {{AzaliaNdigital, 0}, 1}}}}, 0, MI_TARGET_DAC},
	{{0, {AudioNmode, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={2, {{{"analog", 0}, 0}, {{AzaliaNdigital, 0}, 1}}}}, 0, MI_TARGET_ADC},
};

static int
alc880_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = __arraycount(alc880_mixer_items);
	this->szmixers = sizeof(alc880_mixer_items);
	this->mixers = kmem_alloc(this->szmixers, KM_SLEEP);
	if (this->mixers == NULL) {
		aprint_error_dev(this->dev, "out of memory in %s\n", __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, alc880_mixer_items, sizeof(alc880_mixer_items));
	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);

	mc.dev = -1;
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 0;		/* mute: off */
	generic_mixer_set(this, 0x0c, MI_TARGET_INAMP(0), &mc);	/* dac->front */
	generic_mixer_set(this, 0x0c, MI_TARGET_INAMP(1), &mc);	/* mixer->front */
	generic_mixer_set(this, 0x0d, MI_TARGET_INAMP(0), &mc);	/* dac->surround */
	generic_mixer_set(this, 0x0e, MI_TARGET_INAMP(0), &mc);	/* dac->clfe */
	generic_mixer_set(this, 0x0f, MI_TARGET_INAMP(0), &mc);	/* dac->side */
	mc.un.ord = 1;		/* mute: on */
	generic_mixer_set(this, 0x0d, MI_TARGET_INAMP(1), &mc);	/* mixer->surround */
	generic_mixer_set(this, 0x0e, MI_TARGET_INAMP(1), &mc);	/* mixer->clfe */
	generic_mixer_set(this, 0x0f, MI_TARGET_INAMP(1), &mc);	/* mixer->side */
	mc.un.ord = 1;		/* pindir: output */
	generic_mixer_set(this, 0x14, MI_TARGET_PINDIR, &mc); /* front */
	generic_mixer_set(this, 0x15, MI_TARGET_PINDIR, &mc); /* surround */
	generic_mixer_set(this, 0x16, MI_TARGET_PINDIR, &mc); /* clfe */
	generic_mixer_set(this, 0x17, MI_TARGET_PINDIR, &mc); /* side */
	generic_mixer_set(this, 0x19, MI_TARGET_PINDIR, &mc); /* mic2 */
	generic_mixer_set(this, 0x1b, MI_TARGET_PINDIR, &mc); /* line2 */
	mc.un.ord = 0;		/* pindir: input */
	generic_mixer_set(this, 0x18, MI_TARGET_PINDIR, &mc);	/* mic1 */
	generic_mixer_set(this, 0x1a, MI_TARGET_PINDIR, &mc);	/* line1 */
	return 0;
}

static int
alc880_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}}}};	/* digital */
	static const convgroupset_t adcs = {
		-1, 2,
		{{2, {0x08, 0x09}}, /* analog 4ch */
		 {1, {0x0a}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC882
 * ---------------------------------------------------------------- */

static const mixer_item_t alc882_mixer_items[] = {
	AZ_MIXER_CLASSES,

	/* 0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x14,0x15,0x16,0x17 */
	{{0, {"mic1."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(0)},
	{{0, {"mic1", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(0)},
	{{0, {"mic2."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(1)},
	{{0, {"mic2", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(1)},
	{{0, {AudioNline"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(2)},
	{{0, {AudioNline, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(2)},
	{{0, {AudioNcd"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(4)},
	{{0, {AudioNcd, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(4)},
	{{0, {AudioNspeaker"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(5)},
	{{0, {AudioNspeaker, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 1, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(5)},

	{{0, {AudioNmaster, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0c, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_PINBOOST},
	{{0, {AudioNheadphone"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_PINBOOST},
	{{0, {AzaliaNfront".dac.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront".mixer.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_INAMP(1)},

	{{0, {AudioNsurround, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0d, MI_TARGET_OUTAMP},
	{{0, {AudioNsurround"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_OUTAMP},
	{{0, {AudioNsurround".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_PINBOOST},

	{{0, {AzaliaNclfe, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0e, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x16, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x16, MI_TARGET_PINBOOST},

	{{0, {AzaliaNside, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0f, MI_TARGET_OUTAMP},
	{{0, {AzaliaNside"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x17, MI_TARGET_OUTAMP},
	{{0, {AzaliaNside".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x17, MI_TARGET_PINBOOST},

	/* 0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x14,0x15,0x16,0x17,0xb */
#define ALC882_MIC1	0x001
#define ALC882_MIC2	0x002
#define ALC882_LINE	0x004
#define ALC882_CD	0x010
#define ALC882_BEEP	0x020
#define ALC882_MIX	0x400
#define ALC882_MASK	0x437
	{{0, {AzaliaNfront"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x07, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront"."AudioNsource, 0}, AUDIO_MIXER_SET, AZ_CLASS_RECORD,
	  0, 0, .un.s={6, {{{"mic1", 0}, ALC882_MIC1}, {{"mic2", 0}, ALC882_MIC2},
			   {{AudioNline, 0}, ALC882_LINE}, {{AudioNcd, 0}, ALC882_CD},
			   {{AudioNspeaker, 0}, ALC882_BEEP},
			   {{AudioNmixerout, 0}, ALC882_MIX}}}}, 0x24, -1},
	{{0, {AudioNsurround"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround"."AudioNsource, 0}, AUDIO_MIXER_SET, AZ_CLASS_RECORD,
	  0, 0, .un.s={6, {{{"mic1", 0}, ALC882_MIC1}, {{"mic2", 0}, ALC882_MIC2},
			   {{AudioNline, 0}, ALC882_LINE}, {{AudioNcd, 0}, ALC882_CD},
			   {{AudioNspeaker, 0}, ALC882_BEEP},
			   {{AudioNmixerout, 0}, ALC882_MIX}}}}, 0x23, -1},
	{{0, {AzaliaNclfe"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNclfe, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNclfe"."AudioNsource, 0}, AUDIO_MIXER_SET, AZ_CLASS_RECORD,
	  0, 0, .un.s={6, {{{"mic1", 0}, ALC882_MIC1}, {{"mic2", 0}, ALC882_MIC2},
			   {{AudioNline, 0}, ALC882_LINE}, {{AudioNcd, 0}, ALC882_CD},
			   {{AudioNspeaker, 0}, ALC882_BEEP},
			   {{AudioNmixerout, 0}, ALC882_MIX}}}}, 0x22, -1},

	{{0, {AudioNmode, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_PLAYBACK, 0, 0,
	  .un.e={2, {{{"analog", 0}, 0}, {{AzaliaNdigital, 0}, 1}}}}, 0, MI_TARGET_DAC},
	{{0, {AudioNmode, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={2, {{{"analog", 0}, 0}, {{AzaliaNdigital, 0}, 1}}}}, 0, MI_TARGET_ADC},
/*	AZ_MIXER_SPDIF(AZ_CLASS_PLAYBACK, 0x06),*/
};

static int
alc882_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = __arraycount(alc882_mixer_items);
	this->szmixers = sizeof(alc882_mixer_items);
	this->mixers = kmem_alloc(this->szmixers, KM_SLEEP);
	if (this->mixers == NULL) {
		aprint_error_dev(this->dev, "out of memory in %s\n", __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, alc882_mixer_items, sizeof(alc882_mixer_items));
	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);

	mc.dev = -1;
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 1;		/* pindir: output */
	generic_mixer_set(this, 0x14, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x1b, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x15, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x16, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x17, MI_TARGET_PINDIR, &mc);
	mc.un.ord = 0;		/* [0] 0x0c */
	generic_mixer_set(this, 0x14, MI_TARGET_CONNLIST, &mc);
	generic_mixer_set(this, 0x1b, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 1;		/* [1] 0x0d */
	generic_mixer_set(this, 0x15, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 2;		/* [2] 0x0e */
	generic_mixer_set(this, 0x16, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 2;		/* [3] 0x0fb */
	generic_mixer_set(this, 0x17, MI_TARGET_CONNLIST, &mc);

	mc.un.ord = 0;		/* pindir: input */
	generic_mixer_set(this, 0x18, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x19, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x1a, MI_TARGET_PINDIR, &mc);
	/* XXX: inamp for 18/19/1a */

	mc.un.ord = 0;		/* unmute */
	generic_mixer_set(this, 0x24, MI_TARGET_INAMP(0), &mc);
	generic_mixer_set(this, 0x23, MI_TARGET_INAMP(1), &mc);
	generic_mixer_set(this, 0x22, MI_TARGET_INAMP(2), &mc);
	generic_mixer_set(this, 0x0d, MI_TARGET_INAMP(0), &mc);
	generic_mixer_set(this, 0x0e, MI_TARGET_INAMP(0), &mc);
	generic_mixer_set(this, 0x0f, MI_TARGET_INAMP(0), &mc);
	mc.un.ord = 1;		/* mute */
	generic_mixer_set(this, 0x0d, MI_TARGET_INAMP(1), &mc);
	generic_mixer_set(this, 0x0e, MI_TARGET_INAMP(1), &mc);
	generic_mixer_set(this, 0x0f, MI_TARGET_INAMP(1), &mc);
	return 0;
}

static int
alc882_init_dacgroup(codec_t *this)
{
#if 0				/* makes no sense to support for 0x25 node */
	static const convgroupset_t dacs = {
		-1, 3,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}},	/* digital */
		 {1, {0x25}}}};	/* another analog */
#else
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}}}};	/* digital */
#endif
	static const convgroupset_t adcs = {
		-1, 2,
		{{3, {0x07, 0x08, 0x09}}, /* analog 6ch */
		 {1, {0x0a}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

static int
alc882_set_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;
	mixer_ctrl_t mc2;
	uint32_t mask, bit;
	int i, err;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	if (mc->type != m->devinfo.type)
		return EINVAL;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if ((m->nid == 0x22 || m->nid == 0x23 || m->nid == 0x24)
	    && m->target == -1) {
		DPRINTF(("%s: hook for record.*.source\n", __func__));
		mc2.dev = -1;
		mc2.type = AUDIO_MIXER_ENUM;
		bit = 1;
		mask = mc->un.mask & ALC882_MASK;
		for (i = 0; i < this->w[m->nid].nconnections && i < 32; i++) {
			mc2.un.ord = (mask & bit) ? 0 : 1;
			err = generic_mixer_set(this, m->nid,
			    MI_TARGET_INAMP(i), &mc2);
			if (err)
				return err;
			bit = bit << 1;
		}
		return 0;
	}
	return generic_mixer_set(this, m->nid, m->target, mc);
}

static int
alc882_get_port(codec_t *this, mixer_ctrl_t *mc)
{
	const mixer_item_t *m;
	uint32_t mask, bit, result;
	int i, err;

	if (mc->dev >= this->nmixers)
		return ENXIO;
	m = &this->mixers[mc->dev];
	mc->type = m->devinfo.type;
	if (mc->type == AUDIO_MIXER_CLASS)
		return 0;
	if ((m->nid == 0x22 || m->nid == 0x23 || m->nid == 0x24)
	    && m->target == -1) {
		DPRINTF(("%s: hook for record.*.source\n", __func__));
		mask = 0;
		bit = 1;
		for (i = 0; i < this->w[m->nid].nconnections && i < 32; i++) {
			err = this->comresp(this, m->nid, CORB_GET_AMPLIFIER_GAIN_MUTE,
				      CORB_GAGM_INPUT | CORB_GAGM_LEFT |
				      i, &result);
			if (err)
				return err;
			if ((result & CORB_GAGM_MUTE) == 0)
				mask |= bit;
			bit = bit << 1;
		}
		mc->un.mask = mask & ALC882_MASK;
		return 0;
	}
	return generic_mixer_get(this, m->nid, m->target, mc);
}

/* ----------------------------------------------------------------
 * Realtek ALC883
 * ALC882 without adc07 and mix24.
 * ---------------------------------------------------------------- */

static int
alc883_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}}}};	/* digital */
		/* don't support for 0x25 dac */
	static const convgroupset_t adcs = {
		-1, 2,
		{{2, {0x08, 0x09}},	/* analog 4ch */
		 {1, {0x0a}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

static const mixer_item_t alc883_mixer_items[] = {
	AZ_MIXER_CLASSES,

	/* 0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x14,0x15,0x16,0x17 */
	{{0, {"mic1."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(0)},
	{{0, {"mic1", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(0)},
	{{0, {"mic2."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(1)},
	{{0, {"mic2", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(1)},
	{{0, {AudioNline"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(2)},
	{{0, {AudioNline, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(2)},
	{{0, {AudioNcd"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(4)},
	{{0, {AudioNcd, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(4)},
	{{0, {AudioNspeaker"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_INAMP(5)},
	{{0, {AudioNspeaker, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 1, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_INAMP(5)},

	{{0, {AudioNmaster, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0c, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_PINBOOST},
	{{0, {AudioNheadphone"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_PINBOOST},
	{{0, {AzaliaNfront".dac.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront".mixer.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_INAMP(1)},
	{{0, {AudioNsurround, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0d, MI_TARGET_OUTAMP},
	{{0, {AudioNsurround"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_OUTAMP},
	{{0, {AudioNsurround".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_PINBOOST},
	{{0, {AudioNsurround".dac.mut", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0d, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround".mixer.m", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0d, MI_TARGET_INAMP(1)},

	{{0, {AzaliaNclfe, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0e, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x16, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x16, MI_TARGET_PINBOOST},
	{{0, {AzaliaNclfe".dac.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0e, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNclfe".mixer.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0e, MI_TARGET_INAMP(1)},

	{{0, {AzaliaNside, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0f, MI_TARGET_OUTAMP},
	{{0, {AzaliaNside"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x17, MI_TARGET_OUTAMP},
	{{0, {AzaliaNside".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x17, MI_TARGET_PINBOOST},
	{{0, {AzaliaNside".dac.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0f, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNside".mixer.mute", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0f, MI_TARGET_INAMP(1)},

	{{0, {AudioNsurround"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround"."AudioNsource, 0}, AUDIO_MIXER_SET, AZ_CLASS_RECORD,
	  0, 0, .un.s={6, {{{"mic1", 0}, ALC882_MIC1}, {{"mic2", 0}, ALC882_MIC2},
			   {{AudioNline, 0}, ALC882_LINE}, {{AudioNcd, 0}, ALC882_CD},
			   {{AudioNspeaker, 0}, ALC882_BEEP},
			   {{AudioNmixerout, 0}, ALC882_MIX}}}}, 0x23, -1},

	{{0, {AzaliaNclfe"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNclfe, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNclfe"."AudioNsource, 0}, AUDIO_MIXER_SET, AZ_CLASS_RECORD,
	  0, 0, .un.s={6, {{{"mic1", 0}, ALC882_MIC1}, {{"mic2", 0}, ALC882_MIC2},
			   {{AudioNline, 0}, ALC882_LINE}, {{AudioNcd, 0}, ALC882_CD},
			   {{AudioNspeaker, 0}, ALC882_BEEP},
			   {{AudioNmixerout, 0}, ALC882_MIX}}}}, 0x22, -1},

	{{0, {"usingdac", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT, 0, 0,
	  .un.e={2, {{{"analog", 0}, 0}, {{"digital", 0}, 1}}}}, 0, MI_TARGET_DAC},
	{{0, {"usingadc", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD, 0, 0,
	  .un.e={2, {{{"analog", 0}, 0}, {{"digital", 0}, 1}}}}, 0, MI_TARGET_ADC},
};

int
alc883_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = __arraycount(alc883_mixer_items);
	this->szmixers = sizeof(alc883_mixer_items);
	this->mixers = kmem_alloc(this->szmixers, KM_SLEEP);
	if (this->mixers == NULL) {
		aprint_error_dev(this->dev, "out of memory in %s\n", __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, alc883_mixer_items, sizeof(alc883_mixer_items));
	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);

	mc.dev = -1;
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 1;		/* pindir: output */
	generic_mixer_set(this, 0x14, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x1b, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x15, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x16, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x17, MI_TARGET_PINDIR, &mc);
	mc.un.ord = 0;		/* [0] 0x0c */
	generic_mixer_set(this, 0x14, MI_TARGET_CONNLIST, &mc);
	generic_mixer_set(this, 0x1b, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 1;		/* [1] 0x0d */
	generic_mixer_set(this, 0x15, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 2;		/* [2] 0x0e */
	generic_mixer_set(this, 0x16, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 2;		/* [3] 0x0fb */
	generic_mixer_set(this, 0x17, MI_TARGET_CONNLIST, &mc);

	mc.un.ord = 0;		/* pindir: input */
	generic_mixer_set(this, 0x18, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x19, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x1a, MI_TARGET_PINDIR, &mc);
	/* XXX: inamp for 18/19/1a */

	mc.un.ord = 0;		/* unmute */
	generic_mixer_set(this, 0x23, MI_TARGET_INAMP(1), &mc);
	generic_mixer_set(this, 0x22, MI_TARGET_INAMP(2), &mc);
	return 0;
}
/* ----------------------------------------------------------------
 * Realtek ALC885
 * ---------------------------------------------------------------- */

static int
alc885_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}}}};	/* digital */
		/* don't support for 0x25 dac */
	static const convgroupset_t adcs = {
		-1, 2,
		{{3, {0x07, 0x08, 0x09}},	/* analog 6ch */
		 {1, {0x0a}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Realtek ALC888
 * ---------------------------------------------------------------- */

static int
alc888_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x06}}}};	/* digital */
		/* don't support for 0x25 dac */
		/* ALC888S has another SPDIF-out 0x10 */
	static const convgroupset_t adcs = {
		-1, 2,
		{{2, {0x08, 0x09}},	/* analog 4ch */
		 {1, {0x0a}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

static int
alc888_init_widget(const codec_t *this, widget_t *w, nid_t nid)
{
	switch (nid) {
	case 0x0c:
		strlcpy(w->name, AudioNmaster, sizeof(w->name));
		break;
	}
	return 0;
}

static int
alc888_mixer_init(codec_t *this)
{
	mixer_item_t *m = NULL;
	mixer_devinfo_t *d;
	int err, i, mdac_index = -1;

	err = generic_mixer_init(this);
	if (err)
		return err;

	/* Clear mixer indexes, to make generic_mixer_fix_indexes happy */
	for (i = 0; i < this->nmixers; i++) {
		d = &this->mixers[i].devinfo;
		d->index = d->prev = d->next = 0;
	}

	/* We're looking for front l/r mixer, which we know is nid 0x0c */
	for (i = 0; i < this->nmixers; i++)
		if (this->mixers[i].nid == 0x0c) {
			mdac_index = i;
			break;
		}
	if (mdac_index >= 0) {
		/*
		 * ALC888 doesn't have a master mixer, so create a fake
		 * inputs.dac that mirrors outputs.master
		 */
		err = generic_mixer_ensure_capacity(this, this->nmixers + 1);
		if (err)
			return err;

		m = &this->mixers[this->nmixers];
		d = &m->devinfo;
		memcpy(m, &this->mixers[mdac_index], sizeof(*m));
		d->mixer_class = AZ_CLASS_INPUT;
		snprintf(d->label.name, sizeof(d->label.name), AudioNdac);
		this->nmixers++;
	}

	/* Recreate mixer indexes and defaults after making a mess of things */
	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);

	return 0;
}

/* ----------------------------------------------------------------
 * Analog Devices AD1981HD
 * ---------------------------------------------------------------- */

#define AD1981HD_THINKPAD	0x201017aa

static const mixer_item_t ad1981hd_mixer_items[] = {
	AZ_MIXER_CLASSES,

	{{0, {AzaliaNdigital "." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{"os", 0}, 0}, {{"adc", 0}, 1}}}}, 0x02, MI_TARGET_CONNLIST},

	{{0, {"lineout." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{AudioNdac, 0}, 0}, {{AudioNmixerout, 0}, 1}}}},
	 0x05, MI_TARGET_CONNLIST},
	{{0, {"lineout." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x05, MI_TARGET_OUTAMP},
	{{0, {"lineout", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(63)}}, 0x05, MI_TARGET_OUTAMP},
	{{0, {"lineout", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(3)}}, 0x05, MI_TARGET_INAMP(0)},
	{{0, {"lineout.dir", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x05, MI_TARGET_PINDIR},
	{{0, {"lineout.boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x05, MI_TARGET_PINBOOST},
	{{0, {"lineout.eapd", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x05, MI_TARGET_EAPD},

	{{0, {AudioNheadphone ".src", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{AudioNdac, 0}, 0}, {{AudioNmixerout, 0}, 1}}}},
	 0x06, MI_TARGET_CONNLIST},
	{{0, {AudioNheadphone "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x06, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(63)}}, 0x06, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone ".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x06, MI_TARGET_PINBOOST},

	{{0, {AudioNmono "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_OUTAMP},
	{{0, {AudioNmono, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 1, MIXER_DELTA(63)}}, 0x07, MI_TARGET_OUTAMP},

	{{0, {AudioNmicrophone, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(3)}}, 0x08, MI_TARGET_INAMP(0)},

	{{0, {"linein." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{AudioNdac, 0}, 0}, {{AudioNmixerout, 0}, 1}}}},
	 0x09, MI_TARGET_CONNLIST},
	{{0, {"linein." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x09, MI_TARGET_OUTAMP},
	{{0, {"linein", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(63)}}, 0x09, MI_TARGET_OUTAMP},
	{{0, {"linein", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(3)}}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {"linein.dir", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x09, MI_TARGET_PINDIR},

	{{0, {AudioNmono "." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={6, {{{AudioNdac, 0}, 0}, {{"mixedmic", 0}, 1},
			   {{"linein", 0}, 2}, {{AudioNmixerout, 0}, 3},
			   {{"lineout", 0}, 4}, {{"mic2", 0}, 5}}}},
	 0x0b, MI_TARGET_CONNLIST},

	{{0, {"beep." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, .un.e={2, {{{"digitalbeep", 0}, 0}, {{"beep", 0}, 1}}}},
	 0x0d, MI_TARGET_CONNLIST},
	{{0, {"beep." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0d, MI_TARGET_OUTAMP},
	{{0, {"beep", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 1, MIXER_DELTA(15)}}, 0x0d, MI_TARGET_OUTAMP},

	{{0, {AudioNdac "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x11, MI_TARGET_OUTAMP},
	{{0, {AudioNdac, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x11, MI_TARGET_OUTAMP},

	{{0, {AudioNmicrophone "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x12, MI_TARGET_OUTAMP},
	{{0, {AudioNmicrophone, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x12, MI_TARGET_OUTAMP},

	{{0, {"linein." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x13, MI_TARGET_OUTAMP},
	{{0, {"linein", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x13, MI_TARGET_OUTAMP},

	{{0, {AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={8, {{{"mixedmic", 0}, 0}, {{"linein", 0}, 1},
			   {{AudioNmixerout, 0}, 2}, {{AudioNmono, 0}, 3},
			   {{AudioNcd, 0}, 4}, {{"lineout", 0}, 5},
			   {{"mic2", 0}, 6}, {{AudioNaux, 0}, 7}}}},
	 0x15, MI_TARGET_CONNLIST},
	{{0, {AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(15)}}, 0x15, MI_TARGET_OUTAMP},

	{{0, {"mic2." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{AudioNdac, 0}, 0}, {{AudioNmixerout, 0}, 1}}}},
	 0x18, MI_TARGET_CONNLIST},
	{{0, {"mic2." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x18, MI_TARGET_OUTAMP},
	{{0, {"mic2", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(63)}}, 0x18, MI_TARGET_OUTAMP},
	{{0, {"mic2", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(3)}}, 0x18, MI_TARGET_INAMP(0)},
	{{0, {"mic2.dir", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x18, MI_TARGET_PINDIR},

	{{0, {"lineout." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x1a, MI_TARGET_OUTAMP},
	{{0, {"lineout", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x1a, MI_TARGET_OUTAMP},

	{{0, {AudioNaux "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_OUTAMP},
	{{0, {AudioNaux, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x1b, MI_TARGET_OUTAMP},

	{{0, {"mic2." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x1c, MI_TARGET_OUTAMP},
	{{0, {"mic2", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x1c, MI_TARGET_OUTAMP},

	{{0, {AudioNcd "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x1d, MI_TARGET_OUTAMP},
	{{0, {AudioNcd, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x1d, MI_TARGET_OUTAMP},

	{{0, {"mixedmic.mute1", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x1e, MI_TARGET_OUTAMP},
	{{0, {"mixedmic.mute2", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x1f, MI_TARGET_OUTAMP},

	{{0, {AudioNmode, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_PLAYBACK, 0, 0,
	  .un.e={2, {{{"analog", 0}, 0}, {{AzaliaNdigital, 0}, 1}}}}, 0, MI_TARGET_DAC},
};

static int
ad1981hd_init_widget(const codec_t *this, widget_t *w, nid_t nid)
{
	switch (nid) {
	case 0x05:
		strlcpy(w->name, AudioNline "out", sizeof(w->name));
		break;
	case 0x06:
		strlcpy(w->name, "hp", sizeof(w->name));
		break;
	case 0x07:
		strlcpy(w->name, AudioNmono, sizeof(w->name));
		break;
	case 0x08:
		strlcpy(w->name, AudioNmicrophone, sizeof(w->name));
		break;
	case 0x09:
		strlcpy(w->name, AudioNline "in", sizeof(w->name));
		break;
	case 0x0d:
		strlcpy(w->name, "beep", sizeof(w->name));
		break;
	case 0x17:
		strlcpy(w->name, AudioNaux, sizeof(w->name));
		break;
	case 0x18:
		strlcpy(w->name, AudioNmicrophone "2", sizeof(w->name));
		break;
	case 0x19:
		strlcpy(w->name, AudioNcd, sizeof(w->name));
		break;
	case 0x1d:
		strlcpy(w->name, AudioNcd, sizeof(w->name));
		break;
	}
	return 0;
}

static int
ad1981hd_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = __arraycount(ad1981hd_mixer_items);
	this->szmixers = sizeof(ad1981hd_mixer_items);
	this->mixers = kmem_alloc(this->szmixers, KM_SLEEP);
	if (this->mixers == NULL) {
		aprint_error_dev(this->dev, "out of memory in %s\n", __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, ad1981hd_mixer_items, sizeof(ad1981hd_mixer_items));
	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);

	if (this->subid == AD1981HD_THINKPAD) {
		mc.dev = -1;
		mc.type = AUDIO_MIXER_ENUM;
		mc.un.ord = 1;
		generic_mixer_set(this, 0x09, MI_TARGET_PINDIR, &mc);
		generic_mixer_set(this, 0x05, MI_TARGET_EAPD, &mc);
		mc.type = AUDIO_MIXER_VALUE;
		mc.un.value.num_channels = 2;
		mc.un.value.level[0] = AUDIO_MAX_GAIN;
		mc.un.value.level[1] = AUDIO_MAX_GAIN;
		generic_mixer_set(this, 0x1a, MI_TARGET_VOLUME, &mc);
	}
	return 0;
}

/* ----------------------------------------------------------------
 * Analog Devices AD1983
 * ---------------------------------------------------------------- */

static const mixer_item_t ad1983_mixer_items[] = {
	AZ_MIXER_CLASSES,

	{{0, {AzaliaNdigital "." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{"os", 0}, 0}, {{"adc", 0}, 1}}}}, 0x02, MI_TARGET_CONNLIST},

	{{0, {AudioNspeaker "." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{AudioNdac, 0}, 0}, {{AudioNmixerout, 0}, 1}}}},
	 0x05, MI_TARGET_CONNLIST},
	{{0, {AudioNspeaker "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x05, MI_TARGET_OUTAMP},
	{{0, {AudioNspeaker, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(64)}}, 0x05, MI_TARGET_OUTAMP},

	{{0, {AudioNheadphone ".src", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{AudioNdac, 0}, 0}, {{AudioNmixerout, 0}, 1}}}},
	 0x06, MI_TARGET_CONNLIST},
	{{0, {AudioNheadphone "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x06, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(64)}}, 0x06, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone ".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x06, MI_TARGET_PINBOOST},

	{{0, {AudioNmono "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_OUTAMP},
	{{0, {AudioNmono, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 1, MIXER_DELTA(64)}}, 0x07, MI_TARGET_OUTAMP},

	{{0, {AudioNmono "." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={4, {{{AudioNdac, 0}, 0}, {{AudioNmicrophone, 0}, 1},
			   {{AudioNline, 0}, 2}, {{AudioNmixerout, 0}, 3}}}},
	 0x0b, MI_TARGET_CONNLIST},

	{{0, {AudioNmicrophone "." AudioNpreamp "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x0c, MI_TARGET_OUTAMP},
	{{0, {AudioNmicrophone "." AudioNpreamp, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(4)}}, 0x0c, MI_TARGET_OUTAMP},
	{{0, {AudioNmicrophone "." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, .un.e={2, {{{AudioNmicrophone, 0}, 0}, {{AudioNline, 0}, 1}}}},
	 0x0c, MI_TARGET_CONNLIST},

	{{0, {AudioNline "." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, .un.e={2, {{{AudioNline, 0}, 0}, {{AudioNmicrophone, 0}, 1}}}},
	 0x0d, MI_TARGET_CONNLIST},

	{{0, {"beep." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x10, MI_TARGET_OUTAMP},
	{{0, {"beep", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 1, MIXER_DELTA(16)}}, 0x10, MI_TARGET_OUTAMP},

	{{0, {AudioNdac "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x11, MI_TARGET_OUTAMP},
	{{0, {AudioNdac, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(32)}}, 0x11, MI_TARGET_OUTAMP},

	{{0, {AudioNmicrophone "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x12, MI_TARGET_OUTAMP},
	{{0, {AudioNmicrophone, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(32)}}, 0x12, MI_TARGET_OUTAMP},

	{{0, {AudioNline "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x13, MI_TARGET_OUTAMP},
	{{0, {AudioNline, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(32)}}, 0x13, MI_TARGET_OUTAMP},

	{{0, {AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={4, {{{AudioNmicrophone, 0}, 0}, {{AudioNline, 0}, 1},
			   {{AudioNmixerout, 0}, 2}, {{AudioNmono, 0}, 3}}}},
	 0x14, MI_TARGET_CONNLIST},
	{{0, {AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {AudioNvolume, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(16)}}, 0x14, MI_TARGET_OUTAMP},

	{{0, {AudioNmode, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_PLAYBACK, 0, 0,
	  .un.e={2, {{{"analog", 0}, 0}, {{AzaliaNdigital, 0}, 1}}}}, 0, MI_TARGET_DAC},
};

static int
ad1983_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = __arraycount(ad1983_mixer_items);
	this->szmixers = sizeof(ad1983_mixer_items);
	this->mixers = kmem_alloc(this->szmixers, KM_SLEEP);
	if (this->mixers == NULL) {
		aprint_error_dev(this->dev, "out of memory in %s\n", __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, ad1983_mixer_items, sizeof(ad1983_mixer_items));
	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);

#define AD198X_EVENT_HP		1
#define AD198X_EVENT_SPEAKER	2

	mc.dev = -1;		/* no need for generic_mixer_set() */
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 1;  /* connlist: mixerout */
	generic_mixer_set(this, 0x05, MI_TARGET_CONNLIST, &mc);
	generic_mixer_set(this, 0x06, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 3;  /* connlist: mixerout */
	generic_mixer_set(this, 0x0b, MI_TARGET_CONNLIST, &mc);

	/* setup a unsolicited event for the headphones and speaker */
	this->comresp(this, 0x05, CORB_SET_UNSOLICITED_RESPONSE,
            CORB_UNSOL_ENABLE | AD198X_EVENT_SPEAKER, NULL);
	this->comresp(this, 0x06, CORB_SET_UNSOLICITED_RESPONSE,
            CORB_UNSOL_ENABLE | AD198X_EVENT_HP, NULL);
	ad1983_unsol_event(this, AD198X_EVENT_SPEAKER);
	ad1983_unsol_event(this, AD198X_EVENT_HP);
	return 0;
}

static int
ad1983_unsol_event(codec_t *this, int tag)
{
	int err;
	uint32_t value;
	mixer_ctrl_t mc;

	mc.dev = -1;
	mc.type = AUDIO_MIXER_ENUM;

	switch (tag) {
	case AD198X_EVENT_HP:
		err = this->comresp(this, 0x06, CORB_GET_PIN_SENSE, 0, &value);
		if (err)
			break;
		if (value & CORB_PS_PRESENCE) {
			DPRINTF(("%s: headphone has been inserted.\n", __func__));
			mc.un.ord = 1; /* mute */
			generic_mixer_set(this, 0x05, MI_TARGET_OUTAMP, &mc);
			generic_mixer_set(this, 0x07, MI_TARGET_OUTAMP, &mc);
		} else {
			DPRINTF(("%s: headphone has been pulled out.\n", __func__));
			mc.un.ord = 0; /* unmute */
			generic_mixer_set(this, 0x05, MI_TARGET_OUTAMP, &mc);
			/* if no speaker unmute internal mono */
			err = this->comresp(this, 0x05, CORB_GET_PIN_SENSE, 0, &value);
			if (err)
				break;
			if (!(value & CORB_PS_PRESENCE))
				generic_mixer_set(this, 0x07, MI_TARGET_OUTAMP, &mc);
		}
		break;
	case AD198X_EVENT_SPEAKER:
		err = this->comresp(this, 0x05, CORB_GET_PIN_SENSE, 0, &value);
		if (err)
			break;
		if (value & CORB_PS_PRESENCE) {
			DPRINTF(("%s: speaker has been inserted.\n", __func__));
			mc.un.ord = 1; /* mute */
			generic_mixer_set(this, 0x07, MI_TARGET_OUTAMP, &mc);
		} else {
			DPRINTF(("%s: speaker has been pulled out.\n", __func__));
			/* if no headphones unmute internal mono */
			err = this->comresp(this, 0x06, CORB_GET_PIN_SENSE, 0, &value);
			if (err)
				break;
			if (!(value & CORB_PS_PRESENCE)) {
				mc.un.ord = 0; /* unmute */
				generic_mixer_set(this, 0x07, MI_TARGET_OUTAMP, &mc);
			}
		}
		break;
	default:
		printf("%s: unknown tag: %d\n", __func__, tag);
	}
	return 0;
}

/* ----------------------------------------------------------------
 * Analog Devices AD1984
 * ---------------------------------------------------------------- */

#define AD1984_THINKPAD			0x20ac17aa
#define AD1984_DELL_OPTIPLEX_755	0x02111028
#define AD1984A_DELL_OPTIPLEX_760	0x027f1028

static int
ad1984_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{2, {0x04, 0x03}},	/* analog 4ch */
		 {1, {0x02}}}};		/* digital */
	static const convgroupset_t adcs = {
		-1, 3,
		{{2, {0x08, 0x09}},	/* analog 4ch */
		 {1, {0x06}},		/* digital */
		 {1, {0x05}}}}; 	/* digital */
	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

static int
ad1984_mixer_init(codec_t *this)
{
	int err;

	err = generic_mixer_autoinit(this);
	if (err)
		return err;

	if (this->subid == AD1984_DELL_OPTIPLEX_755 ||
	    this->subid == AD1984A_DELL_OPTIPLEX_760) {
		/* setup a unsolicited event for the headphones and speaker */
		this->comresp(this, 0x12, CORB_SET_UNSOLICITED_RESPONSE,
			      CORB_UNSOL_ENABLE | AD198X_EVENT_SPEAKER, NULL);
		this->comresp(this, 0x11, CORB_SET_UNSOLICITED_RESPONSE,
			      CORB_UNSOL_ENABLE | AD198X_EVENT_HP, NULL);
		ad1984_unsol_event(this, AD198X_EVENT_SPEAKER);
		ad1984_unsol_event(this, AD198X_EVENT_HP);
	}

	return 0;
}

static int
ad1984_init_widget(const codec_t *this, widget_t *w, nid_t nid)
{
	switch (nid) {
	case 0x07:
		strlcpy(w->name, "hp", sizeof(w->name));
		break;
	case 0x0a:
		strlcpy(w->name, "spkr", sizeof(w->name));
		break;
	case 0x0b:
		strlcpy(w->name, AudioNaux, sizeof(w->name));
		break;
	case 0x0c:
		strlcpy(w->name, "adc08", sizeof(w->name));
		break;
	case 0x0d:
		strlcpy(w->name, "adc09", sizeof(w->name));
		break;
	case 0x0e:
		strlcpy(w->name, AudioNmono "sel", sizeof(w->name));
		break;
	case 0x0f:
		strlcpy(w->name, AudioNaux "sel", sizeof(w->name));
		break;
	case 0x10:
		strlcpy(w->name, "beep", sizeof(w->name));
		break;
	case 0x1e:
		strlcpy(w->name, AudioNmono, sizeof(w->name));
		break;
	case 0x22:
		strlcpy(w->name, "hp" "sel", sizeof(w->name));
		break;
	case 0x23:
		strlcpy(w->name, "dock" "sel", sizeof(w->name));
		break;
	case 0x24:
		strlcpy(w->name, "dock", sizeof(w->name));
		break;
	case 0x25:
		strlcpy(w->name, "dock.pre", sizeof(w->name));
		break;
	default:
		return generic_mixer_init_widget(this, w, nid);
	}

	return 0;
}

static int
ad1984_unsol_event(codec_t *this, int tag)
{
	int err;
	uint32_t value;
	mixer_ctrl_t mc;

	mc.dev = -1;
	mc.type = AUDIO_MIXER_ENUM;

	switch (tag) {
	case AD198X_EVENT_HP:
		err = this->comresp(this, 0x11, CORB_GET_PIN_SENSE, 0, &value);
		if (err)
			break;
		if (value & CORB_PS_PRESENCE) {
			DPRINTF(("%s: headphone has been inserted.\n", __func__));
			mc.un.ord = 1; /* mute */
			generic_mixer_set(this, 0x12, MI_TARGET_OUTAMP, &mc);
			generic_mixer_set(this, 0x13, MI_TARGET_OUTAMP, &mc);
		} else {
			DPRINTF(("%s: headphone has been pulled out.\n", __func__));
			mc.un.ord = 0; /* unmute */
			generic_mixer_set(this, 0x12, MI_TARGET_OUTAMP, &mc);
			/* if no speaker unmute internal mono */
			err = this->comresp(this, 0x12, CORB_GET_PIN_SENSE, 0, &value);
			if (err)
				break;
			if (!(value & CORB_PS_PRESENCE))
				generic_mixer_set(this, 0x13, MI_TARGET_OUTAMP, &mc);
		}
		break;
	case AD198X_EVENT_SPEAKER:
		err = this->comresp(this, 0x12, CORB_GET_PIN_SENSE, 0, &value);
		if (err)
			break;
		if (value & CORB_PS_PRESENCE) {
			DPRINTF(("%s: speaker has been inserted.\n", __func__));
			mc.un.ord = 1; /* mute */
			generic_mixer_set(this, 0x13, MI_TARGET_OUTAMP, &mc);
		} else {
			DPRINTF(("%s: speaker has been pulled out.\n", __func__));
			/* if no headphones unmute internal mono */
			err = this->comresp(this, 0x11, CORB_GET_PIN_SENSE, 0, &value);
			if (err)
				break;
			if (!(value & CORB_PS_PRESENCE)) {
				mc.un.ord = 0; /* unmute */
				generic_mixer_set(this, 0x13, MI_TARGET_OUTAMP, &mc);
			}
		}
		break;
	default:
		printf("%s: unknown tag: %d\n", __func__, tag);
	}
	return 0;
}

/* ----------------------------------------------------------------
 * Analog Devices AD1986A
 * ---------------------------------------------------------------- */

static const mixer_item_t ad1986a_mixer_items[] = {
	AZ_MIXER_CLASSES,

	{{0, {AzaliaNdigital "." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{"hdaudio", 0}, 0}, {{"recordout", 0}, 1}}}}, 0x02, MI_TARGET_CONNLIST},

#if 0
	/* fix to mixerout (default) */
	{{0, {AudioNheadphone ".src", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={3, {{{AudioNmixerout, 0}, 0}, {{"surrounddac", 0}, 1},
			  {{"clfedac", 0}, 2}}}}, 0x0a, MI_TARGET_CONNLIST},
#endif
	{{0, {AudioNheadphone "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1a, MI_TARGET_OUTAMP},
	{{0, {AudioNheadphone "." "boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1a, MI_TARGET_PINBOOST},
	{{0, {AudioNheadphone, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x1a, MI_TARGET_OUTAMP},

#if 0
	/* fix to mixerout (default) */
	{{0, {AudioNline "." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{AudioNmixerout, 0}, 0}, {{"surrounddac", 0}, 1}}}},
	  0x0b, MI_TARGET_CONNLIST},
#endif
	{{0, {AudioNline "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_OUTAMP},
	{{0, {AudioNline "." "boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_PINBOOST},
	{{0, {AudioNline "." "eapd", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1b, MI_TARGET_EAPD},
	{{0, {AudioNline, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x1b, MI_TARGET_OUTAMP},

#if 0
	/* fix to the surround DAC (default) */
	{{0, {AudioNsurround "." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{"surrounddac", 0}, 0}, {{AudioNmixerout, 0}, 1}}}},
	  0x0c, MI_TARGET_CONNLIST},
	/* unmute */
	{{0, {AudioNsurround "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1c, MI_TARGET_OUTAMP},
	/* fix to output */
	{{0, {AudioNsurround "." "dir", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x1c, MI_TARGET_PINDIR},
	/* fix to maximum */
	{{0, {AudioNsurround, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x1c, MI_TARGET_OUTAMP},
#endif
	{{0, {AudioNsurround "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x04, MI_TARGET_OUTAMP},
	{{0, {AudioNsurround, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x04, MI_TARGET_OUTAMP},

#if 0
	/* fix to the clfe DAC (default) */
	{{0, {AzaliaNclfe "." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{"clfedac", 0}, 0}, {{"mixeroutmono", 0}, 1}}}},
	  0x0d, MI_TARGET_CONNLIST},
	/* unmute */
	{{0, {AzaliaNclfe "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1d, MI_TARGET_OUTAMP},
	/* fix to output */
	{{0, {AzaliaNclfe "." "dir", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_IO}, 0x1d, MI_TARGET_PINDIR},
	/* fix to maximum */
	{{0, {AzaliaNclfe, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x1d, MI_TARGET_OUTAMP},
#endif
	{{0, {AzaliaNclfe "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x05, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x05, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe "." "lrswap", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1d, MI_TARGET_LRSWAP},

	{{0, {AudioNmono "." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{AudioNmixerout, 0}, 0}, {{AudioNmicrophone, 0}, 1}}}},
	  0x0e, MI_TARGET_CONNLIST},
	{{0, {AudioNmono "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x1e, MI_TARGET_OUTAMP},
	{{0, {AudioNmono, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x1e, MI_TARGET_OUTAMP},

	/* Front DAC */
	{{0, {AudioNdac "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x03, MI_TARGET_OUTAMP},
	{{0, {AudioNdac, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x03, MI_TARGET_OUTAMP},

#if 0
	/* 0x09: 5.1 -> Stereo Downmix */
	{{0, {"downmix" "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x09, MI_TARGET_OUTAMP},
#endif

#if 0
	/* mic source is mic jack (default) */
	{{0, {AudioNmicrophone "." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, .un.e={8, {{{AudioNmicrophone, 0}, 0}, {{AudioNline, 0}, 1},
			   {{AzaliaNclfe, 0}, 2}, {{AzaliaNclfe "2", 0}, 3},
			   {{"micclfe", 0}, 4}, {{"micline", 0}, 5},
			   {{"clfeline", 0}, 6}, {{"miclineclfe", 0}, 7}}}},
	  0x0f, MI_TARGET_CONNLIST},
#endif
	{{0, {AudioNmicrophone "." "gain", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(3)}}, 0x0f, MI_TARGET_OUTAMP},
	{{0, {AudioNmicrophone "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x13, MI_TARGET_OUTAMP},
	{{0, {AudioNmicrophone, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x13, MI_TARGET_OUTAMP},

#if 0
	/* line source is line jack (default) */
	{{0, {AudioNline "." AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, .un.e={3, {{{AudioNline, 0}, 0}, {{AudioNsurround, 0}, 1},
			   {{AudioNmicrophone, 0}, 2}}}}, 0x10, MI_TARGET_CONNLIST},
#endif
	{{0, {AudioNline "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x17, MI_TARGET_OUTAMP},
	{{0, {AudioNline, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x17, MI_TARGET_OUTAMP},

	{{0, {"phone" "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {"phone", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x14, MI_TARGET_OUTAMP},

	{{0, {AudioNcd "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x15, MI_TARGET_OUTAMP},
	{{0, {AudioNcd, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x15, MI_TARGET_OUTAMP},

	{{0, {AudioNaux "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x16, MI_TARGET_OUTAMP},
	{{0, {AudioNaux, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x16, MI_TARGET_OUTAMP},

	{{0, {AudioNspeaker "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x18, MI_TARGET_OUTAMP},
	{{0, {AudioNspeaker, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(15)}}, 0x18, MI_TARGET_OUTAMP},


	/* 0x11: inputs.sel11.source=sel0f  [ sel0f mix2b ]	XXXX
		 inputs.sel11.lrswap=off  [ off on ]		XXXX */

	{{0, {AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={7, {{{AudioNmicrophone, 0}, 0}, {{AudioNcd, 0}, 1},
			   {{AudioNaux, 0}, 2}, {{AudioNline, 0}, 3},
			   {{AudioNmixerout, 0}, 4}, {{"mixeroutmono", 0}, 5},
			   {{"phone", 0}, 6}}}},
	  0x12, MI_TARGET_CONNLIST},
	{{0, {AudioNvolume "." AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x12, MI_TARGET_OUTAMP},
	{{0, {AudioNvolume, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(15)}}, 0x12, MI_TARGET_OUTAMP},

	{{0, {AudioNmode, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_PLAYBACK, 0, 0,
	  .un.e={2, {{{"analog", 0}, 0}, {{AzaliaNdigital, 0}, 1}}}}, 0, MI_TARGET_DAC},
};

static int
ad1986a_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = __arraycount(ad1986a_mixer_items);
	this->szmixers = sizeof(ad1986a_mixer_items);
	this->mixers = kmem_alloc(this->szmixers, KM_SLEEP);
	if (this->mixers == NULL) {
		aprint_error_dev(this->dev, "out of memory in %s\n", __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, ad1986a_mixer_items, sizeof(ad1986a_mixer_items));
	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);

	mc.dev = -1;
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 0;		/* unmute */
	generic_mixer_set(this, 0x1c, MI_TARGET_OUTAMP, &mc);
	mc.un.ord = 1;		/* dir: output */
	generic_mixer_set(this, 0x1c, MI_TARGET_PINDIR, &mc);
	mc.type = AUDIO_MIXER_VALUE;
	mc.un.value.num_channels = 2;
	mc.un.value.level[0] = AUDIO_MAX_GAIN;
	mc.un.value.level[1] = AUDIO_MAX_GAIN;
	generic_mixer_set(this, 0x1c, MI_TARGET_VOLUME, &mc);
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 0;		/* unmute */
	generic_mixer_set(this, 0x1d, MI_TARGET_OUTAMP, &mc);
	mc.un.ord = 1;		/* dir: output */
	generic_mixer_set(this, 0x1d, MI_TARGET_PINDIR, &mc);
	mc.type = AUDIO_MIXER_VALUE;
	mc.un.value.num_channels = 2;
	mc.un.value.level[0] = AUDIO_MAX_GAIN;
	mc.un.value.level[1] = AUDIO_MAX_GAIN;
	generic_mixer_set(this, 0x1d, MI_TARGET_VOLUME, &mc);
	return 0;
}

static int
ad1986a_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{6, {0x03, 0x04, 0x05}},	/* analog 6ch */
		 {1, {0x02}}}};			/* digital */
	static const convgroupset_t adcs = {
		-1, 1,
		{{1, {0x06}}}};			/* analog 2ch */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Analog Devices AD1988A/AD1988B
 * ---------------------------------------------------------------- */

static int
ad1988_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 3,
		{{4, {0x04, 0x05, 0x06, 0x0a}},	/* analog 8ch */
		 {1, {0x02}},	/* digital */
		 {1, {0x03}}}};	/* another analog */
	static const convgroupset_t adcs = {
		-1, 2,
		{{2, {0x08, 0x09, 0x0f}}, /* analog 6ch */
		 {1, {0x07}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * CMedia CMI9880
 * ---------------------------------------------------------------- */

static const mixer_item_t cmi9880_mixer_items[] = {
	AZ_MIXER_CLASSES,

	{{0, {AudioNmaster"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x03, MI_TARGET_OUTAMP},
	{{0, {AudioNsurround"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x04, MI_TARGET_OUTAMP},
	{{0, {AzaliaNclfe"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x05, MI_TARGET_OUTAMP},
	{{0, {AzaliaNside"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x06, MI_TARGET_OUTAMP},
	{{0, {AzaliaNdigital"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x07, MI_TARGET_OUTAMP},

	{{0, {AzaliaNfront"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(30)}}, 0x08, MI_TARGET_INAMP(0)},
	{{0, {AzaliaNfront"."AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={4, {{{AudioNmicrophone, 0}, 5}, {{AudioNcd, 0}, 6},
			   {{"line1", 0}, 7}, {{"line2", 0}, 8}}}},
	 0x08, MI_TARGET_CONNLIST},
	{{0, {AudioNsurround"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, ENUM_OFFON}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_RECORD,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(30)}}, 0x09, MI_TARGET_INAMP(0)},
	{{0, {AudioNsurround"."AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={4, {{{AudioNmicrophone, 0}, 5}, {{AudioNcd, 0}, 6},
			   {{"line1", 0}, 7}, {{"line2", 0}, 8}}}},
	 0x09, MI_TARGET_CONNLIST},

	{{0, {AudioNspeaker"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, ENUM_OFFON}, 0x23, MI_TARGET_OUTAMP},
	{{0, {AudioNspeaker, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_INPUT,
	  0, 0, .un.v={{"", 0}, 1, MIXER_DELTA(15)}}, 0x23, MI_TARGET_OUTAMP}
};

static int
cmi9880_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;

	this->nmixers = __arraycount(cmi9880_mixer_items);
	this->szmixers = sizeof(cmi9880_mixer_items);
	this->mixers = kmem_alloc(this->szmixers, KM_SLEEP);
	if (this->mixers == NULL) {
		aprint_error_dev(this->dev, "out of memory in %s\n", __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, cmi9880_mixer_items, sizeof(cmi9880_mixer_items));
	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);

	mc.dev = -1;
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 5;		/* record.front.source=mic */
	generic_mixer_set(this, 0x08, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 7;		/* record.surround.source=line1 */
	generic_mixer_set(this, 0x09, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 1;		/* pindir: output */
	generic_mixer_set(this, 0x0b, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x0c, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x0d, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x0e, MI_TARGET_PINDIR, &mc);
	generic_mixer_set(this, 0x0f, MI_TARGET_PINDIR, &mc);
	mc.un.ord = 0;		/* front DAC -> headphones */
	generic_mixer_set(this, 0x0f, MI_TARGET_CONNLIST, &mc);
	mc.un.ord = 0;		/* pindir: input */
	generic_mixer_set(this, 0x10, MI_TARGET_PINDIR, &mc);	/* mic */
	generic_mixer_set(this, 0x13, MI_TARGET_PINDIR, &mc);	/* SPDIF-in */
	generic_mixer_set(this, 0x1f, MI_TARGET_PINDIR, &mc);	/* line1 */
	generic_mixer_set(this, 0x20, MI_TARGET_PINDIR, &mc);	/* line2 */
	return 0;
}

static int
cmi9880_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 2,
		{{4, {0x03, 0x04, 0x05, 0x06}}, /* analog 8ch */
		 {1, {0x07}}}};	/* digital */
	static const convgroupset_t adcs = {
		-1, 2,
		{{2, {0x08, 0x09}}, /* analog 4ch */
		 {1, {0x0a}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

/* ----------------------------------------------------------------
 * Sigmatel STAC9221 and STAC9221D
 * ---------------------------------------------------------------- */

#define STAC9221_MAC	0x76808384

static int
stac9221_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 3,
		{{4, {0x02, 0x03, 0x04, 0x05}}, /* analog 8ch */
		 {1, {0x08}},	/* digital */
		 {1, {0x1a}}}};	/* another digital? */
	static const convgroupset_t adcs = {
		-1, 2,
		{{2, {0x06, 0x07}}, /* analog 4ch */
		 {1, {0x09}}}};	/* digital */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

static int
stac9221_mixer_init(codec_t *this)
{
	int err;

	err = generic_mixer_init(this);
	if (err)
		return err;
	if (this->subid == STAC9221_MAC) {
		stac9221_gpio_unmute(this, 0);
		stac9221_gpio_unmute(this, 1);
	}
	return 0;
}

static int
stac9221_gpio_unmute(codec_t *this, int pin)
{
	uint32_t data, mask, dir;

	this->comresp(this, this->audiofunc, CORB_GET_GPIO_DATA, 0, &data);
	this->comresp(this, this->audiofunc,
	    CORB_GET_GPIO_ENABLE_MASK, 0, &mask);
	this->comresp(this, this->audiofunc, CORB_GET_GPIO_DIRECTION, 0, &dir);
	data &= ~(1 << pin);
	mask |= 1 << pin;
	dir |= 1 << pin;
	this->comresp(this, this->audiofunc, 0x7e7, 0, NULL);
	this->comresp(this, this->audiofunc,
	    CORB_SET_GPIO_ENABLE_MASK, mask, NULL);
	this->comresp(this, this->audiofunc,
	    CORB_SET_GPIO_DIRECTION, dir, NULL);
	DELAY(1000);
	this->comresp(this, this->audiofunc, CORB_SET_GPIO_DATA, data, NULL);
	return 0;
}

/* ----------------------------------------------------------------
 * Sigmatel STAC9200 and STAC9200D
 * ---------------------------------------------------------------- */

static const mixer_item_t stac9200_mixer_items[] = {
	AZ_MIXER_CLASSES,

	{{0, {AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={3, {{{AudioNdac, 0}, 0}, {{AzaliaNdigital"-in", 0}, 1},
			   {{"selector", 0}, 2}}}}, 0x07, MI_TARGET_CONNLIST},
	{{0, {AzaliaNdigital"."AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, .un.e={2, {{{AudioNdac, 0}, 0}, {{"selector", 0}, 1}}}},
	 0x09, MI_TARGET_CONNLIST}, /* AudioNdac is not accurate name */
	{{0, {"selector."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x0a, MI_TARGET_OUTAMP},
	{{0, {"selector", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(15)}}, 0x0a, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x0b, MI_TARGET_OUTAMP},
	{{0, {AudioNmaster, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 2, MIXER_DELTA(31)}}, 0x0b, MI_TARGET_OUTAMP},
	{{0, {"selector."AudioNsource, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_INPUT,
	  0, 0, .un.e={3, {{{"mic1", 0}, 0}, {{"mic2", 0}, 1}, {{AudioNcd, 0}, 4}}}},
	 0x0c, MI_TARGET_CONNLIST},
	{{0, {AudioNheadphone".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x0d, MI_TARGET_PINBOOST},
	{{0, {AudioNspeaker".boost", 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x0e, MI_TARGET_PINBOOST},
	{{0, {AudioNmono"."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x11, MI_TARGET_OUTAMP},
	{{0, {AudioNmono, 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 1, MIXER_DELTA(31)}}, 0x11, MI_TARGET_OUTAMP},
	{{0, {"beep."AudioNmute, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_OUTPUT,
	  0, 0, ENUM_OFFON}, 0x14, MI_TARGET_OUTAMP},
	{{0, {"beep", 0}, AUDIO_MIXER_VALUE, AZ_CLASS_OUTPUT,
	  0, 0, .un.v={{"", 0}, 1, MIXER_DELTA(3)}}, 0x14, MI_TARGET_OUTAMP},
	{{0, {AudioNmode, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_PLAYBACK,
	  0, 0, .un.e={2, {{{"analog", 0}, 0}, {{AzaliaNdigital, 0}, 1}}}},
	 0, MI_TARGET_DAC},
	{{0, {AudioNmode, 0}, AUDIO_MIXER_ENUM, AZ_CLASS_RECORD,
	  0, 0, .un.e={2, {{{"analog", 0}, 0}, {{AzaliaNdigital, 0}, 1}}}},
	 0, MI_TARGET_ADC},
};

static int
stac9200_mixer_init(codec_t *this)
{
	mixer_ctrl_t mc;
	uint32_t value;

	this->nmixers = __arraycount(stac9200_mixer_items);
	this->szmixers = sizeof(stac9200_mixer_items);
	this->mixers = kmem_alloc(this->szmixers, KM_SLEEP);
	if (this->mixers == NULL) {
		aprint_error_dev(this->dev, "out of memory in %s\n", __func__);
		return ENOMEM;
	}
	memcpy(this->mixers, stac9200_mixer_items, sizeof(stac9200_mixer_items));
	generic_mixer_fix_indexes(this);
	generic_mixer_default(this);

	mc.dev = -1;		/* no need for generic_mixer_set() */
	mc.type = AUDIO_MIXER_ENUM;
	mc.un.ord = 1;		/* pindir: output */
	generic_mixer_set(this, 0x0d, MI_TARGET_PINDIR, &mc); /* headphones */
	generic_mixer_set(this, 0x0e, MI_TARGET_PINDIR, &mc); /* speaker */
	mc.un.ord = 0;		/* pindir: input */
	generic_mixer_set(this, 0x0f, MI_TARGET_PINDIR, &mc); /* mic2 */
	generic_mixer_set(this, 0x10, MI_TARGET_PINDIR, &mc); /* mic1 */
	mc.type = AUDIO_MIXER_VALUE;
	mc.un.value.num_channels = 2;
	mc.un.value.level[0] = generic_mixer_max(this, 0x0c, MI_TARGET_OUTAMP);
	mc.un.value.level[1] = mc.un.value.level[0];
	generic_mixer_set(this, 0x0c, MI_TARGET_OUTAMP, &mc);

#define STAC9200_DELL_INSPIRON6400_ID	0x01bd1028
#define STAC9200_DELL_INSPIRON9400_ID	0x01cd1028
#define STAC9200_DELL_640M_ID		0x01d81028
#define STAC9200_DELL_LATITUDE_D420_ID	0x01d61028
#define STAC9200_DELL_LATITUDE_D430_ID	0x02011028

#define STAC9200_EVENT_HP	0
#define STAC9200_NID_HP		0x0d
#define STAC9200_NID_SPEAKER	0x0e

	switch (this->subid) {
	case STAC9200_DELL_INSPIRON6400_ID:
	case STAC9200_DELL_INSPIRON9400_ID:
	case STAC9200_DELL_640M_ID:
	case STAC9200_DELL_LATITUDE_D420_ID:
	case STAC9200_DELL_LATITUDE_D430_ID:
		/* Does every DELL model have the same pin configuration?
		 * I'm not sure. */

		/* setup a unsolicited event for the headphones */
		this->comresp(this, STAC9200_NID_HP, CORB_SET_UNSOLICITED_RESPONSE,
		    CORB_UNSOL_ENABLE | STAC9200_EVENT_HP, NULL);
		/* If the headphone presents, mute the internal speaker */
		this->comresp(this, STAC9200_NID_HP, CORB_GET_PIN_SENSE, 0, &value);
		if (value & CORB_PS_PRESENCE) {
			generic_mixer_pinctrl(this, STAC9200_NID_SPEAKER, 0);
		} else {
			generic_mixer_pinctrl(this,
			    STAC9200_NID_SPEAKER, CORB_PWC_OUTPUT);
		}
		break;
	default:
		break;
	}
	return 0;
}

static int
stac9200_unsol_event(codec_t *this, int tag)
{
	int err;
	uint32_t value;

	switch (tag) {
	case STAC9200_EVENT_HP:
		err = this->comresp(this, STAC9200_NID_HP,
		    CORB_GET_PIN_SENSE, 0, &value);
		if (err)
			break;
		if (value & CORB_PS_PRESENCE) {
			DPRINTF(("%s: headphone has been inserted.\n", __func__));
			generic_mixer_pinctrl(this, STAC9200_NID_SPEAKER, 0);
		} else {
			DPRINTF(("%s: headphone has been pulled out.\n", __func__));
			generic_mixer_pinctrl(this, STAC9200_NID_SPEAKER, CORB_PWC_OUTPUT);
		}
		break;
	default:
		printf("%s: unknown tag: %d\n", __func__, tag);
	}
	return 0;
}

/* ----------------------------------------------------------------
 * ATI HDMI
 * ---------------------------------------------------------------- */

static int
atihdmi_init_dacgroup(codec_t *this)
{
	static const convgroupset_t dacs = {
		-1, 1,
		{{1, {0x02}}}};	/* digital */
	static const convgroupset_t adcs = {
		-1, 0,
		{{0, {0x00}}}}; /* no recording */

	this->dacs = dacs;
	this->adcs = adcs;
	return 0;
}

