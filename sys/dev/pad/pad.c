/* $NetBSD: pad.c,v 1.23 2015/07/10 21:58:56 nat Exp $ */

/*-
 * Copyright (c) 2007 Jared D. McNeill <jmcneill@invisible.ca>
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
__KERNEL_RCSID(0, "$NetBSD: pad.c,v 1.23 2015/07/10 21:58:56 nat Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/condvar.h>
#include <sys/select.h>
#include <sys/audioio.h>
#include <sys/vnode.h>
#include <sys/module.h>
#include <sys/atomic.h>

#include <dev/audio_if.h>
#include <dev/audiovar.h>
#include <dev/auconv.h>
#include <dev/auvolconv.h>

#include <dev/pad/padvar.h>

#define PADUNIT(x)	minor(x)

extern struct cfdriver pad_cd;

typedef struct pad_block {
	uint8_t		*pb_ptr;
	int		pb_len;
} pad_block_t;

enum {
	PAD_OUTPUT_CLASS,
	PAD_INPUT_CLASS,
	PAD_OUTPUT_MASTER_VOLUME,
	PAD_INPUT_DAC_VOLUME,
	PAD_ENUM_LAST,
};

static int	pad_match(device_t, cfdata_t, void *);
static void	pad_attach(device_t, device_t, void *);
static int	pad_detach(device_t, int);
static void	pad_childdet(device_t, device_t);

static int	pad_query_encoding(void *, struct audio_encoding *);
static int	pad_set_params(void *, int, int,
				audio_params_t *, audio_params_t *,
				stream_filter_list_t *, stream_filter_list_t *);
static int	pad_start_output(void *, void *, int,
				    void (*)(void *), void *);
static int	pad_start_input(void *, void *, int,
				   void (*)(void *), void *);
static int	pad_halt_output(void *);
static int	pad_halt_input(void *);
static int	pad_getdev(void *, struct audio_device *);
static int	pad_set_port(void *, mixer_ctrl_t *);
static int	pad_get_port(void *, mixer_ctrl_t *);
static int	pad_query_devinfo(void *, mixer_devinfo_t *);
static int	pad_get_props(void *);
static int	pad_round_blocksize(void *, int, int, const audio_params_t *);
static void	pad_get_locks(void *, kmutex_t **, kmutex_t **);

static stream_filter_t *pad_swvol_filter_le(struct audio_softc *,
    const audio_params_t *, const audio_params_t *);
static stream_filter_t *pad_swvol_filter_be(struct audio_softc *,
    const audio_params_t *, const audio_params_t *);
static void	pad_swvol_dtor(stream_filter_t *);

static const struct audio_hw_if pad_hw_if = {
	.query_encoding = pad_query_encoding,
	.set_params = pad_set_params,
	.start_output = pad_start_output,
	.start_input = pad_start_input,
	.halt_output = pad_halt_output,
	.halt_input = pad_halt_input,
	.getdev = pad_getdev,
	.set_port = pad_set_port,
	.get_port = pad_get_port,
	.query_devinfo = pad_query_devinfo,
	.get_props = pad_get_props,
	.round_blocksize = pad_round_blocksize,
	.get_locks = pad_get_locks,
};

#define PAD_NFORMATS	1
static const struct audio_format pad_formats[PAD_NFORMATS] = {
	{ NULL, AUMODE_PLAY|AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	  2, AUFMT_STEREO, 1, { 44100 } },
};

extern void	padattach(int);

static int		pad_add_block(pad_softc_t *, uint8_t *, int);
static int		pad_get_block(pad_softc_t *, pad_block_t *, int);

dev_type_open(pad_open);
dev_type_close(pad_close);
dev_type_read(pad_read);

const struct cdevsw pad_cdevsw = {
	.d_open = pad_open,
	.d_close = pad_close,
	.d_read = pad_read,
	.d_write = nowrite,
	.d_ioctl = noioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = nopoll,
	.d_mmap = nommap,
	.d_kqfilter = nokqfilter,
	.d_discard = nodiscard,
	.d_flag = D_OTHER | D_MPSAFE,
};

CFATTACH_DECL2_NEW(pad, sizeof(pad_softc_t), pad_match, pad_attach, pad_detach,
    NULL, NULL, pad_childdet);

void
padattach(int n)
{
	int i, err;
	cfdata_t cf;

	aprint_debug("pad: requested %d units\n", n);

	err = config_cfattach_attach(pad_cd.cd_name, &pad_ca);
	if (err) {
		aprint_error("%s: couldn't register cfattach: %d\n",
		    pad_cd.cd_name, err);
		config_cfdriver_detach(&pad_cd);
		return;
	}

	for (i = 0; i < n; i++) {
		cf = kmem_alloc(sizeof(struct cfdata), KM_SLEEP);
		if (cf == NULL) {
			aprint_error("%s: couldn't allocate cfdata\n",
			    pad_cd.cd_name);
			continue;
		}
		cf->cf_name = pad_cd.cd_name;
		cf->cf_atname = pad_cd.cd_name;
		cf->cf_unit = i;
		cf->cf_fstate = FSTATE_STAR;

		(void)config_attach_pseudo(cf);
	}

	return;
}

static int
pad_add_block(pad_softc_t *sc, uint8_t *blk, int blksize)
{
	int l;

	if (sc->sc_open == 0)
		return EIO;

	KASSERT(mutex_owned(&sc->sc_lock));

	if (sc->sc_buflen + blksize > PAD_BUFSIZE)
		return ENOBUFS;

	if (sc->sc_wpos + blksize <= PAD_BUFSIZE)
		memcpy(sc->sc_audiobuf + sc->sc_wpos, blk, blksize);
	else {
		l = PAD_BUFSIZE - sc->sc_wpos;
		memcpy(sc->sc_audiobuf + sc->sc_wpos, blk, l);
		memcpy(sc->sc_audiobuf, blk + l, blksize - l);
	}

	sc->sc_wpos += blksize;
	if (sc->sc_wpos > PAD_BUFSIZE)
		sc->sc_wpos -= PAD_BUFSIZE;

	sc->sc_buflen += blksize;

	return 0;
}

static int
pad_get_block(pad_softc_t *sc, pad_block_t *pb, int blksize)
{
	int l;

	KASSERT(mutex_owned(&sc->sc_lock));
	KASSERT(pb != NULL);

	if (sc->sc_buflen < blksize)
		return ERESTART;

	pb->pb_ptr = (sc->sc_audiobuf + sc->sc_rpos);
	if (sc->sc_rpos + blksize < PAD_BUFSIZE) {
		pb->pb_len = blksize;
		sc->sc_rpos += blksize;
	} else {
		l = PAD_BUFSIZE - sc->sc_rpos;
		pb->pb_len = l;
		sc->sc_rpos = 0;
	}
	sc->sc_buflen -= pb->pb_len;

	return 0;
}

static int
pad_match(device_t parent, cfdata_t data, void *opaque)
{

	return 1;
}

static void
pad_childdet(device_t self, device_t child)
{
	pad_softc_t *sc = device_private(self);

	sc->sc_audiodev = NULL;
}

static void
pad_attach(device_t parent, device_t self, void *opaque)
{
	pad_softc_t *sc = device_private(self);

	aprint_normal_dev(self, "outputs: 44100Hz, 16-bit, stereo\n");

	sc->sc_dev = self;
	sc->sc_open = 0;
	if (auconv_create_encodings(pad_formats, PAD_NFORMATS,
	    &sc->sc_encodings) != 0) {
		aprint_error_dev(self, "couldn't create encodings\n");
		return;
	}

	cv_init(&sc->sc_condvar, device_xname(self));
	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_NONE);

	sc->sc_swvol = 255;
	sc->sc_buflen = 0;
	sc->sc_rpos = sc->sc_wpos = 0;
	sc->sc_audiodev = (void *)audio_attach_mi(&pad_hw_if, sc, sc->sc_dev);

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");

	return;
}

static int
pad_detach(device_t self, int flags)
{
	pad_softc_t *sc = device_private(self);
	int cmaj, mn, rc;

	cmaj = cdevsw_lookup_major(&pad_cdevsw);
	mn = device_unit(self);
	vdevgone(cmaj, mn, mn, VCHR);

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;

	pmf_device_deregister(self);

	mutex_destroy(&sc->sc_lock);
	mutex_destroy(&sc->sc_intr_lock);
	cv_destroy(&sc->sc_condvar);

	auconv_delete_encodings(sc->sc_encodings);

	return 0;
}

int
pad_open(dev_t dev, int flags, int fmt, struct lwp *l)
{
	pad_softc_t *sc;

	sc = device_lookup_private(&pad_cd, PADUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	if (atomic_swap_uint(&sc->sc_open, 1) != 0) {
		return EBUSY;
	}

	return 0;
}

int
pad_close(dev_t dev, int flags, int fmt, struct lwp *l)
{
	pad_softc_t *sc;

	sc = device_lookup_private(&pad_cd, PADUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	KASSERT(sc->sc_open > 0);
	sc->sc_open = 0;

	return 0;
}

int
pad_read(dev_t dev, struct uio *uio, int flags)
{
	pad_softc_t *sc;
	pad_block_t pb;
	void (*intr)(void *);
	void *intrarg;
	int err;

	sc = device_lookup_private(&pad_cd, PADUNIT(dev));
	if (sc == NULL)
		return ENXIO;

	err = 0;

	mutex_enter(&sc->sc_lock);
	intr = sc->sc_intr;
	intrarg = sc->sc_intrarg;

	kpreempt_disable();
	while (uio->uio_resid > 0 && !err) {
		err = pad_get_block(sc, &pb, min(uio->uio_resid, PAD_BLKSIZE));
		if (!err) {
			mutex_exit(&sc->sc_lock);
			err = uiomove(pb.pb_ptr, pb.pb_len, uio);
			mutex_enter(&sc->sc_lock);
			continue;
		}

		if (intr) {
			mutex_enter(&sc->sc_intr_lock);
			(*intr)(intrarg);
			mutex_exit(&sc->sc_intr_lock);
			intr = sc->sc_intr;
			intrarg = sc->sc_intrarg;
			err = 0;
			continue;
		}
		err = cv_wait_sig(&sc->sc_condvar, &sc->sc_lock);
		if (err != 0) {
			mutex_exit(&sc->sc_lock);
			kpreempt_enable();
			return err;
		}
		intr = sc->sc_intr;
		intrarg = sc->sc_intrarg;
	}

	if (intr) {
		mutex_enter(&sc->sc_intr_lock);
		(*intr)(intrarg);
		mutex_exit(&sc->sc_intr_lock);
	}
	mutex_exit(&sc->sc_lock);
	kpreempt_enable();

	return err;
}

static int
pad_query_encoding(void *opaque, struct audio_encoding *ae)
{
	pad_softc_t *sc;

	sc = (pad_softc_t *)opaque;

	KASSERT(mutex_owned(&sc->sc_lock));

	return auconv_query_encoding(sc->sc_encodings, ae);
}

static int
pad_set_params(void *opaque, int setmode, int usemode,
    audio_params_t *play, audio_params_t *rec,
    stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
	pad_softc_t *sc __diagused;

	sc = (pad_softc_t *)opaque;

	KASSERT(mutex_owned(&sc->sc_lock));

	if (auconv_set_converter(pad_formats, PAD_NFORMATS, AUMODE_PLAY,
	    play, true, pfil) < 0)
		return EINVAL;
	if (auconv_set_converter(pad_formats, PAD_NFORMATS, AUMODE_RECORD,
	    rec, true, rfil) < 0)
		return EINVAL;

	if (pfil->req_size > 0)
		play = &pfil->filters[0].param;
	switch (play->encoding) {
	case AUDIO_ENCODING_SLINEAR_LE:
		if (play->precision == 16 && play->validbits == 16)
			pfil->prepend(pfil, pad_swvol_filter_le, play);
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		if (play->precision == 16 && play->validbits == 16)
			pfil->prepend(pfil, pad_swvol_filter_be, play);
		break;
	default:
		break;
	}

	return 0;
}

static int
pad_start_output(void *opaque, void *block, int blksize,
    void (*intr)(void *), void *intrarg)
{
	pad_softc_t *sc;
	int err;

	sc = (pad_softc_t *)opaque;

	KASSERT(mutex_owned(&sc->sc_lock));

	sc->sc_intr = intr;
	sc->sc_intrarg = intrarg;
	sc->sc_blksize = blksize;

	err = pad_add_block(sc, block, blksize);

	cv_broadcast(&sc->sc_condvar);

	return err;
}

static int
pad_start_input(void *opaque, void *block, int blksize,
    void (*intr)(void *), void *intrarg)
{
	pad_softc_t *sc __diagused;

	sc = (pad_softc_t *)opaque;

	KASSERT(mutex_owned(&sc->sc_lock));

	return EOPNOTSUPP;
}

static int
pad_halt_output(void *opaque)
{
	pad_softc_t *sc;

	sc = (pad_softc_t *)opaque;

	KASSERT(mutex_owned(&sc->sc_lock));

	sc->sc_intr = NULL;
	sc->sc_intrarg = NULL;
	sc->sc_buflen = 0;
	sc->sc_rpos = sc->sc_wpos = 0;

	return 0;
}

static int
pad_halt_input(void *opaque)
{
	pad_softc_t *sc __diagused;

	sc = (pad_softc_t *)opaque;

	KASSERT(mutex_owned(&sc->sc_lock));

	return 0;
}

static int
pad_getdev(void *opaque, struct audio_device *ret)
{
	strlcpy(ret->name, "Virtual Audio", sizeof(ret->name));
	strlcpy(ret->version, osrelease, sizeof(ret->version));
	strlcpy(ret->config, "pad", sizeof(ret->config));

	return 0;
}

static int
pad_set_port(void *opaque, mixer_ctrl_t *mc)
{
	pad_softc_t *sc;

	sc = (pad_softc_t *)opaque;

	KASSERT(mutex_owned(&sc->sc_lock));

	switch (mc->dev) {
	case PAD_OUTPUT_MASTER_VOLUME:
	case PAD_INPUT_DAC_VOLUME:
		sc->sc_swvol = mc->un.value.level[AUDIO_MIXER_LEVEL_MONO];
		return 0;
	}

	return ENXIO;
}

static int
pad_get_port(void *opaque, mixer_ctrl_t *mc)
{
	pad_softc_t *sc;

	sc = (pad_softc_t *)opaque;

	KASSERT(mutex_owned(&sc->sc_lock));

	switch (mc->dev) {
	case PAD_OUTPUT_MASTER_VOLUME:
	case PAD_INPUT_DAC_VOLUME:
		mc->un.value.level[AUDIO_MIXER_LEVEL_MONO] = sc->sc_swvol;
		return 0;
	}

	return ENXIO;
}

static int
pad_query_devinfo(void *opaque, mixer_devinfo_t *di)
{
	pad_softc_t *sc __diagused;

	sc = (pad_softc_t *)opaque;

	KASSERT(mutex_owned(&sc->sc_lock));

	switch (di->index) {
	case PAD_OUTPUT_CLASS:
		di->mixer_class = PAD_OUTPUT_CLASS;
		strcpy(di->label.name, AudioCoutputs);
		di->type = AUDIO_MIXER_CLASS;
		di->next = di->prev = AUDIO_MIXER_LAST;
		return 0;
	case PAD_INPUT_CLASS:
		di->mixer_class = PAD_INPUT_CLASS;
		strcpy(di->label.name, AudioCinputs);
		di->type = AUDIO_MIXER_CLASS;
		di->next = di->prev = AUDIO_MIXER_LAST;
		return 0;
	case PAD_OUTPUT_MASTER_VOLUME:
		di->mixer_class = PAD_OUTPUT_CLASS;
		strcpy(di->label.name, AudioNmaster);
		di->type = AUDIO_MIXER_VALUE;
		di->next = di->prev = AUDIO_MIXER_LAST;
		di->un.v.num_channels = 1;
		strcpy(di->un.v.units.name, AudioNvolume);
		return 0;
	case PAD_INPUT_DAC_VOLUME:
		di->mixer_class = PAD_INPUT_CLASS;
		strcpy(di->label.name, AudioNdac);
		di->type = AUDIO_MIXER_VALUE;
		di->next = di->prev = AUDIO_MIXER_LAST;
		di->un.v.num_channels = 1;
		strcpy(di->un.v.units.name, AudioNvolume);
		return 0;
	}

	return ENXIO;
}

static int
pad_get_props(void *opaque)
{
	pad_softc_t *sc __diagused;

	sc = (pad_softc_t *)opaque;

	KASSERT(mutex_owned(&sc->sc_lock));

	return 0;
}

static int
pad_round_blocksize(void *opaque, int blksize, int mode,
    const audio_params_t *p)
{
	pad_softc_t *sc __diagused;

	sc = (pad_softc_t *)opaque;
	KASSERT(mutex_owned(&sc->sc_lock));

	return PAD_BLKSIZE;
}

static void
pad_get_locks(void *opaque, kmutex_t **intr, kmutex_t **thread)
{
	pad_softc_t *sc;

	sc = (pad_softc_t *)opaque;

	*intr = &sc->sc_intr_lock;
	*thread = &sc->sc_lock;
}

static stream_filter_t *
pad_swvol_filter_le(struct audio_softc *asc,
    const audio_params_t *from, const audio_params_t *to)
{
	auvolconv_filter_t *this;
	device_t dev = audio_get_device(asc);
	struct pad_softc *sc = device_private(dev);

	this = kmem_alloc(sizeof(auvolconv_filter_t), KM_SLEEP);
	this->base.base.fetch_to = auvolconv_slinear16_le_fetch_to;
	this->base.dtor = pad_swvol_dtor;
	this->base.set_fetcher = stream_filter_set_fetcher;
	this->base.set_inputbuffer = stream_filter_set_inputbuffer;
	this->vol = &sc->sc_swvol;

	return (stream_filter_t *)this;
}

static stream_filter_t *
pad_swvol_filter_be(struct audio_softc *asc,
    const audio_params_t *from, const audio_params_t *to)
{
	auvolconv_filter_t *this;
	device_t dev = audio_get_device(asc);
	struct pad_softc *sc = device_private(dev);

	this = kmem_alloc(sizeof(auvolconv_filter_t), KM_SLEEP);
	this->base.base.fetch_to = auvolconv_slinear16_be_fetch_to;
	this->base.dtor = pad_swvol_dtor;
	this->base.set_fetcher = stream_filter_set_fetcher;
	this->base.set_inputbuffer = stream_filter_set_inputbuffer;
	this->vol = &sc->sc_swvol;

	return (stream_filter_t *)this;
}

static void
pad_swvol_dtor(stream_filter_t *this)
{
	if (this)
		kmem_free(this, sizeof(auvolconv_filter_t));
}

#ifdef _MODULE

MODULE(MODULE_CLASS_DRIVER, pad, NULL);

static const struct cfiattrdata audiobuscf_iattrdata = {
	"audiobus", 0, { { NULL, NULL, 0 }, }
};
static const struct cfiattrdata * const pad_attrs[] = {
	&audiobuscf_iattrdata, NULL
};

CFDRIVER_DECL(pad, DV_DULL, pad_attrs);
extern struct cfattach pad_ca;
static int padloc[] = { -1, -1 };

static struct cfdata pad_cfdata[] = {
	{
		.cf_name = "pad",
		.cf_atname = "pad",
		.cf_unit = 0,
		.cf_fstate = FSTATE_STAR,
		.cf_loc = padloc,
		.cf_flags = 0,
		.cf_pspec = NULL,
	},
	{ NULL, NULL, 0, 0, NULL, 0, NULL }
};

static int
pad_modcmd(modcmd_t cmd, void *arg)
{
	devmajor_t cmajor = NODEVMAJOR, bmajor = NODEVMAJOR;
	int error;

	switch (cmd) {
	case MODULE_CMD_INIT:
		error = config_cfdriver_attach(&pad_cd);
		if (error) {
			return error;
		}

		error = config_cfattach_attach(pad_cd.cd_name, &pad_ca);
		if (error) {
			config_cfdriver_detach(&pad_cd);
			aprint_error("%s: unable to register cfattach\n",
				pad_cd.cd_name);

			return error;
		}

		error = config_cfdata_attach(pad_cfdata, 1);
		if (error) {
			config_cfattach_detach(pad_cd.cd_name, &pad_ca);
			config_cfdriver_detach(&pad_cd);
			aprint_error("%s: unable to register cfdata\n",
				pad_cd.cd_name);

			return error;
		}

		error = devsw_attach(pad_cd.cd_name, NULL, &bmajor, &pad_cdevsw, &cmajor);
		if (error) {
			error = config_cfdata_detach(pad_cfdata);
			if (error) {
				return error;
			}
			config_cfattach_detach(pad_cd.cd_name, &pad_ca);
			config_cfdriver_detach(&pad_cd);
			aprint_error("%s: unable to register devsw\n",
				pad_cd.cd_name);

			return error;
		}

		(void)config_attach_pseudo(pad_cfdata);

		return 0;
	case MODULE_CMD_FINI:
		error = config_cfdata_detach(pad_cfdata);
		if (error) {
			return error;
		}

		config_cfattach_detach(pad_cd.cd_name, &pad_ca);
		config_cfdriver_detach(&pad_cd);
		devsw_detach(NULL, &pad_cdevsw);

		return 0;
	default:
		return ENOTTY;
	}
}

#endif
