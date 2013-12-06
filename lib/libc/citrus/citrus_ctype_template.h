/*	$NetBSD: citrus_ctype_template.h,v 1.36 2013/05/28 16:57:56 joerg Exp $	*/

/*-
 * Copyright (c)2002 Citrus Project,
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


/*
 * CAUTION: THIS IS NOT STANDALONE FILE
 *
 * function templates of ctype encoding handler for each encodings.
 *
 * you need to define the macros below:
 *
 *   _FUNCNAME(method) :
 *     It should convine the real function name for the method.
 *      e.g. _FUNCNAME(mbrtowc) should be expanded to
 *             _EUC_ctype_mbrtowc
 *           for EUC locale.
 *
 *   _CEI_TO_STATE(cei, method) :
 *     It should be expanded to the pointer of the method-internal state
 *     structures.
 *     e.g. _CEI_TO_STATE(cei, mbrtowc) might be expanded to
 *             (cei)->states.s_mbrtowc
 *     This structure may use if the function is called as
 *           mbrtowc(&wc, s, n, NULL);
 *     Such individual structures are needed by:
 *           mblen
 *           mbrlen
 *           mbrtowc
 *           mbtowc
 *           mbsrtowcs
 *           mbsnrtowcs
 *           wcrtomb
 *           wcsrtombs
 *           wcsnrtombs
 *           wctomb
 *     These need to be keeped in the ctype encoding information structure,
 *     pointed by "cei".
 *
 *   _ENCODING_INFO :
 *     It should be expanded to the name of the encoding information structure.
 *     e.g. For EUC encoding, this macro is expanded to _EUCInfo.
 *     Encoding information structure need to contain the common informations
 *     for the codeset.
 *
 *   _ENCODING_STATE :
 *     It should be expanded to the name of the encoding state structure.
 *     e.g. For EUC encoding, this macro is expanded to _EUCState.
 *     Encoding state structure need to contain the context-dependent states,
 *     which are "unpacked-form" of mbstate_t type and keeped during sequent
 *     calls of mb/wc functions,
 *
 *   _ENCODING_IS_STATE_DEPENDENT :
 *     If the encoding is state dependent, this should be expanded to
 *     non-zero integral value.  Otherwise, 0.
 *
 *   _STATE_NEEDS_EXPLICIT_INIT(ps) :
 *     some encodings, states needs some explicit initialization.
 *     (ie. initialization with memset isn't enough.)
 *     If the encoding state pointed by "ps" needs to be initialized
 *     explicitly, return non-zero. Otherwize, 0.
 *
 */


/* prototypes */

__BEGIN_DECLS
static void _FUNCNAME(init_state)(_ENCODING_INFO * __restrict,
				  _ENCODING_STATE * __restrict);
static void _FUNCNAME(pack_state)(_ENCODING_INFO * __restrict,
				  void * __restrict,
				  const _ENCODING_STATE * __restrict);
static void _FUNCNAME(unpack_state)(_ENCODING_INFO * __restrict,
				    _ENCODING_STATE * __restrict,
				    const void * __restrict);
#if _ENCODING_IS_STATE_DEPENDENT
static int _FUNCNAME(put_state_reset)(_ENCODING_INFO * __restrict,
				      char * __restrict, size_t,
				      _ENCODING_STATE * __restrict,
				      size_t * __restrict);
#endif

/*
 * standard form of mbrtowc_priv.
 *
 * note (differences from real mbrtowc):
 *   - 3rd parameter is not "const char *s" but "const char **s".
 *     after the call of the function, *s will point the first byte of
 *     the next character.
 *   - additional 4th parameter is the size of src buffer.
 *   - 5th parameter is unpacked encoding-dependent state structure.
 *   - additional 6th parameter is the storage to be stored
 *     the return value in the real mbrtowc context.
 *   - return value means "errno" in the real mbrtowc context.
 */

static int _FUNCNAME(mbrtowc_priv)(_ENCODING_INFO * __restrict,
				   wchar_t * __restrict,
				   const char ** __restrict,
				   size_t, _ENCODING_STATE * __restrict,
				   size_t * __restrict);

/*
 * standard form of wcrtomb_priv.
 *
 * note (differences from real wcrtomb):
 *   - additional 3th parameter is the size of src buffer.
 *   - 5th parameter is unpacked encoding-dependent state structure.
 *   - additional 6th parameter is the storage to be stored
 *     the return value in the real mbrtowc context.
 *   - return value means "errno" in the real wcrtomb context.
 *   - caller should ensure that 2nd parameter isn't NULL.
 *     (XXX inconsist with mbrtowc_priv)
 */

static int _FUNCNAME(wcrtomb_priv)(_ENCODING_INFO * __restrict,
				   char * __restrict, size_t, wchar_t,
				   _ENCODING_STATE * __restrict,
				   size_t * __restrict);
__END_DECLS


/*
 * macros
 */

#define _TO_CEI(_cl_)	((_CTYPE_INFO*)(_cl_))


/*
 * templates
 */

/* internal routines */

static __inline int
_FUNCNAME(mbtowc_priv)(_ENCODING_INFO * __restrict ei,
		       wchar_t * __restrict pwc,  const char * __restrict s,
		       size_t n, _ENCODING_STATE * __restrict psenc,
		       int * __restrict nresult)
{
	_ENCODING_STATE state;
	size_t nr;
	int err = 0;

	_DIAGASSERT(ei != NULL);
	_DIAGASSERT(psenc != NULL);

	if (s == NULL) {
		_FUNCNAME(init_state)(ei, psenc);
		*nresult = _ENCODING_IS_STATE_DEPENDENT;
		return (0);
	}

	state = *psenc;
	err = _FUNCNAME(mbrtowc_priv)(ei, pwc, (const char **)&s, n, psenc, &nr);
	if (nr == (size_t)-2)
		err = EILSEQ;
	if (err) {
		/* In error case, we should restore the state. */
		*psenc = state;
		*nresult = -1;
		return (err);
	}

	*nresult = (int)nr;

	return (0);
}

static int
_FUNCNAME(mbsrtowcs_priv)(_ENCODING_INFO * __restrict ei,
			  wchar_t * __restrict pwcs,
			  const char ** __restrict s,
			  size_t n, _ENCODING_STATE * __restrict psenc,
			  size_t * __restrict nresult)
{
	int err, cnt;
	size_t siz;
	const char *s0;
	size_t mbcurmax;

	_DIAGASSERT(nresult != 0);
	_DIAGASSERT(ei != NULL);
	_DIAGASSERT(psenc != NULL);
	_DIAGASSERT(s != NULL);
	_DIAGASSERT(*s != NULL);

	/* if pwcs is NULL, ignore n */
	if (pwcs == NULL)
		n = 1; /* arbitrary >0 value */

	err = cnt = 0;
	s0 = *s; /* to keep *s unchanged for now, use copy instead. */
	mbcurmax = _ENCODING_MB_CUR_MAX(ei);
	while (n > 0) {
		err = _FUNCNAME(mbrtowc_priv)(ei, pwcs, &s0, mbcurmax,
					      psenc, &siz);
		if (siz == (size_t)-2)
			err = EILSEQ;
		if (err) {
			cnt = -1;
			goto bye;
		}
		switch (siz) {
		case 0:
			if (pwcs) {
				_FUNCNAME(init_state)(ei, psenc);
			}
			s0 = 0;
			goto bye;
		default:
			if (pwcs) {
				pwcs++;
				n--;
			}
			cnt++;
			break;
		}
	}
bye:
	if (pwcs)
		*s = s0;

	*nresult = (size_t)cnt;

	return err;
}

static int
_FUNCNAME(mbsnrtowcs_priv)(_ENCODING_INFO * __restrict ei,
			  wchar_t * __restrict pwcs,
			  const char ** __restrict s, size_t in,
			  size_t n, _ENCODING_STATE * __restrict psenc,
			  size_t * __restrict nresult)
{
	int err;
	size_t cnt, siz;
	const char *s0, *se;

	_DIAGASSERT(nresult != 0);
	_DIAGASSERT(ei != NULL);
	_DIAGASSERT(psenc != NULL);
	_DIAGASSERT(s != NULL);
	_DIAGASSERT(*s != NULL);

	/* if pwcs is NULL, ignore n */
	if (pwcs == NULL)
		n = 1; /* arbitrary >0 value */

	err = 0;
	cnt = 0;
	se = *s + in;
	s0 = *s; /* to keep *s unchanged for now, use copy instead. */
	while (s0 < se && n > 0) {
		err = _FUNCNAME(mbrtowc_priv)(ei, pwcs, &s0, se - s0,
					      psenc, &siz);
		if (err) {
			cnt = (size_t)-1;
			goto bye;
		}
		if (siz == (size_t)-2) {
			s0 = se;
			goto bye;
		}
		switch (siz) {
		case 0:
			if (pwcs) {
				_FUNCNAME(init_state)(ei, psenc);
			}
			s0 = 0;
			goto bye;
		default:
			if (pwcs) {
				pwcs++;
				n--;
			}
			cnt++;
			break;
		}
	}
bye:
	if (pwcs)
		*s = s0;

	*nresult = cnt;

	return err;
}

static int
_FUNCNAME(wcsrtombs_priv)(_ENCODING_INFO * __restrict ei, char * __restrict s,
			  const wchar_t ** __restrict pwcs,
			  size_t n, _ENCODING_STATE * __restrict psenc,
			  size_t * __restrict nresult)
{
	int err;
	char buf[MB_LEN_MAX];
	size_t cnt, siz;
	const wchar_t* pwcs0;
#if _ENCODING_IS_STATE_DEPENDENT
	_ENCODING_STATE state;
#endif

	pwcs0 = *pwcs;

	cnt = 0;
	if (!s)
		n = 1;

	while (n > 0) {
#if _ENCODING_IS_STATE_DEPENDENT
		state = *psenc;
#endif
		err = _FUNCNAME(wcrtomb_priv)(ei, buf, sizeof(buf),
					      *pwcs0, psenc, &siz);
		if (siz == (size_t)-1) {
			*nresult = siz;
			return (err);
		}

		if (s) {
			if (n < siz) {
#if _ENCODING_IS_STATE_DEPENDENT
				*psenc = state;
#endif
				break;
			}
			memcpy(s, buf, siz);
			s += siz;
			n -= siz;
		}
		cnt += siz;
		if (!*pwcs0) {
			if (s) {
				_FUNCNAME(init_state)(ei, psenc);
			}
			pwcs0 = 0;
			cnt--; /* don't include terminating null */
			break;
		}
		pwcs0++;
	}
	if (s)
		*pwcs = pwcs0;

	*nresult = cnt;
	return (0);
}

static int
_FUNCNAME(wcsnrtombs_priv)(_ENCODING_INFO * __restrict ei, char * __restrict s,
			  const wchar_t ** __restrict pwcs, size_t in,
			  size_t n, _ENCODING_STATE * __restrict psenc,
			  size_t * __restrict nresult)
{
	int cnt = 0, err;
	char buf[MB_LEN_MAX];
	size_t siz;
	const wchar_t* pwcs0;
#if _ENCODING_IS_STATE_DEPENDENT
	_ENCODING_STATE state;
#endif

	pwcs0 = *pwcs;

	if (!s)
		n = 1;

	while (in > 0 && n > 0) {
#if _ENCODING_IS_STATE_DEPENDENT
		state = *psenc;
#endif
		err = _FUNCNAME(wcrtomb_priv)(ei, buf, sizeof(buf),
					      *pwcs0, psenc, &siz);
		if (siz == (size_t)-1) {
			*nresult = siz;
			return (err);
		}

		if (s) {
			if (n < siz) {
#if _ENCODING_IS_STATE_DEPENDENT
				*psenc = state;
#endif
				break;
			}
			memcpy(s, buf, siz);
			s += siz;
			n -= siz;
		}
		cnt += siz;
		if (!*pwcs0) {
			if (s) {
				_FUNCNAME(init_state)(ei, psenc);
			}
			pwcs0 = 0;
			cnt--; /* don't include terminating null */
			break;
		}
		pwcs0++;
		--in;
	}
	if (s)
		*pwcs = pwcs0;

	*nresult = (size_t)cnt;
	return (0);
}


/* ----------------------------------------------------------------------
 * templates for public functions
 */

#define _RESTART_BEGIN(_func_, _cei_, _pspriv_, _pse_)			\
do {									\
	_ENCODING_STATE _state;						\
	do {								\
		if (_pspriv_ == NULL) {					\
			_pse_ = &_CEI_TO_STATE(_cei_, _func_);		\
			if (_STATE_NEEDS_EXPLICIT_INIT(_pse_))		\
			    _FUNCNAME(init_state)(_CEI_TO_EI(_cei_),	\
							(_pse_));	\
		} else {						\
			_pse_ = &_state;				\
			_FUNCNAME(unpack_state)(_CEI_TO_EI(_cei_),	\
						_pse_, _pspriv_);	\
		}							\
	} while (/*CONSTCOND*/0)

#define _RESTART_END(_func_, _cei_, _pspriv_, _pse_)			\
	if (_pspriv_ != NULL) {						\
		_FUNCNAME(pack_state)(_CEI_TO_EI(_cei_), _pspriv_,	\
				      _pse_);				\
	}								\
} while (/*CONSTCOND*/0)

int
_FUNCNAME(ctype_getops)(_citrus_ctype_ops_rec_t *ops, size_t lenops,
			uint32_t expected_version)
{
	if (expected_version<_CITRUS_CTYPE_ABI_VERSION || lenops<sizeof(*ops))
		return (EINVAL);

	memcpy(ops, &_FUNCNAME(ctype_ops), sizeof(_FUNCNAME(ctype_ops)));

	return (0);
}

static int
_FUNCNAME(ctype_init)(void ** __restrict cl,
		      void * __restrict var, size_t lenvar, size_t lenps)
{
	_CTYPE_INFO *cei;

	_DIAGASSERT(cl != NULL);

	/* sanity check to avoid overruns */
	if (sizeof(_ENCODING_STATE) > lenps)
		return (EINVAL);

	cei = calloc(1, sizeof(_CTYPE_INFO));
	if (cei == NULL)
		return (ENOMEM);

	*cl = (void *)cei;

	return _FUNCNAME(encoding_module_init)(_CEI_TO_EI(cei), var, lenvar);
}

static void
_FUNCNAME(ctype_uninit)(void *cl)
{
	if (cl) {
		_FUNCNAME(encoding_module_uninit)(_CEI_TO_EI(_TO_CEI(cl)));
		free(cl);
	}
}

static unsigned
/*ARGSUSED*/
_FUNCNAME(ctype_get_mb_cur_max)(void *cl)
{
	return _ENCODING_MB_CUR_MAX(_CEI_TO_EI(_TO_CEI(cl)));
}

static int
_FUNCNAME(ctype_mblen)(void * __restrict cl,
		       const char * __restrict s, size_t n,
		       int * __restrict nresult)
{
	_ENCODING_STATE *psenc;
	_ENCODING_INFO *ei;

	_DIAGASSERT(cl != NULL);

	psenc = &_CEI_TO_STATE(_TO_CEI(cl), mblen);
	ei = _CEI_TO_EI(_TO_CEI(cl));
	if (_STATE_NEEDS_EXPLICIT_INIT(psenc))
		_FUNCNAME(init_state)(ei, psenc);
	return _FUNCNAME(mbtowc_priv)(ei, NULL, s, n, psenc, nresult);
}

static int
_FUNCNAME(ctype_mbrlen)(void * __restrict cl, const char * __restrict s,
			size_t n, void * __restrict pspriv,
			size_t * __restrict nresult)
{
	_ENCODING_STATE *psenc;
	_ENCODING_INFO *ei;
	int err = 0;

	_DIAGASSERT(cl != NULL);

	ei = _CEI_TO_EI(_TO_CEI(cl));
	_RESTART_BEGIN(mbrlen, _TO_CEI(cl), pspriv, psenc);
	if (s == NULL) {
		_FUNCNAME(init_state)(ei, psenc);
		*nresult = 0;
	} else {
		err = _FUNCNAME(mbrtowc_priv)(ei, NULL, (const char **)&s, n,
		    (void *)psenc, nresult);
	}
	_RESTART_END(mbrlen, _TO_CEI(cl), pspriv, psenc);

	return (err);
}

static int
_FUNCNAME(ctype_mbrtowc)(void * __restrict cl, wchar_t * __restrict pwc,
			 const char * __restrict s, size_t n,
			 void * __restrict pspriv, size_t * __restrict nresult)
{
	_ENCODING_STATE *psenc;
	_ENCODING_INFO *ei;
	int err = 0;

	_DIAGASSERT(cl != NULL);

	ei = _CEI_TO_EI(_TO_CEI(cl));
	_RESTART_BEGIN(mbrtowc, _TO_CEI(cl), pspriv, psenc);
	if (s == NULL) {
		_FUNCNAME(init_state)(ei, psenc);
		*nresult = 0;
	} else {
		err = _FUNCNAME(mbrtowc_priv)(ei, pwc, (const char **)&s, n,
		    (void *)psenc, nresult);
	}
	_RESTART_END(mbrtowc, _TO_CEI(cl), pspriv, psenc);

	return (err);
}

static int
/*ARGSUSED*/
_FUNCNAME(ctype_mbsinit)(void * __restrict cl, const void * __restrict pspriv,
			 int * __restrict nresult)
{
	_ENCODING_STATE state;

	if (pspriv == NULL) {
		*nresult = 1;
		return (0);
	}

	_FUNCNAME(unpack_state)(_CEI_TO_EI(_TO_CEI(cl)), &state, pspriv);

	*nresult = (state.chlen == 0); /* XXX: FIXME */

	return (0);
}

static int
_FUNCNAME(ctype_mbsrtowcs)(void * __restrict cl, wchar_t * __restrict pwcs,
			   const char ** __restrict s, size_t n,
			   void * __restrict pspriv,
			   size_t * __restrict nresult)
{
	_ENCODING_STATE *psenc;
	_ENCODING_INFO *ei;
	int err = 0;

	_DIAGASSERT(cl != NULL);

	ei = _CEI_TO_EI(_TO_CEI(cl));
	_RESTART_BEGIN(mbsrtowcs, _TO_CEI(cl), pspriv, psenc);
	err = _FUNCNAME(mbsrtowcs_priv)(ei, pwcs, s, n, psenc, nresult);
	_RESTART_END(mbsrtowcs, _TO_CEI(cl), pspriv, psenc);

	return (err);
}

static int __used
_FUNCNAME(ctype_mbsnrtowcs)(_citrus_ctype_rec_t * __restrict cc, wchar_t * __restrict pwcs,
			   const char ** __restrict s, size_t in, size_t n,
			   void * __restrict pspriv,
			   size_t * __restrict nresult)
{
	void *cl = cc->cc_closure;
	_ENCODING_STATE *psenc;
	_ENCODING_INFO *ei;
	int err = 0;

	_DIAGASSERT(cl != NULL);

	ei = _CEI_TO_EI(_TO_CEI(cl));
	_RESTART_BEGIN(mbsnrtowcs, _TO_CEI(cl), pspriv, psenc);
	err = _FUNCNAME(mbsnrtowcs_priv)(ei, pwcs, s, in, n, psenc, nresult);
	_RESTART_END(mbsnrtowcs, _TO_CEI(cl), pspriv, psenc);

	return (err);
}

static int
_FUNCNAME(ctype_mbstowcs)(void * __restrict cl, wchar_t * __restrict pwcs,
			  const char * __restrict s, size_t n,
			  size_t * __restrict nresult)
{
	int err;
	_ENCODING_STATE state;
	_ENCODING_INFO *ei;

	_DIAGASSERT(cl != NULL);

	ei = _CEI_TO_EI(_TO_CEI(cl));
	_FUNCNAME(init_state)(ei, &state);
	err = _FUNCNAME(mbsrtowcs_priv)(ei, pwcs, (const char **)&s, n,
					&state, nresult);
	if (*nresult == (size_t)-2) {
		err = EILSEQ;
		*nresult = (size_t)-1;
	}

	return (err);
}

static int
_FUNCNAME(ctype_mbtowc)(void * __restrict cl, wchar_t * __restrict pwc,
			const char * __restrict s, size_t n,
			int * __restrict nresult)
{
	_ENCODING_STATE *psenc;
	_ENCODING_INFO *ei;

	_DIAGASSERT(cl != NULL);

	psenc = &_CEI_TO_STATE(_TO_CEI(cl), mbtowc);
	ei = _CEI_TO_EI(_TO_CEI(cl));
	if (_STATE_NEEDS_EXPLICIT_INIT(psenc))
		_FUNCNAME(init_state)(ei, psenc);
	return _FUNCNAME(mbtowc_priv)(ei, pwc, s, n, psenc, nresult);
}

static int
_FUNCNAME(ctype_wcrtomb)(void * __restrict cl, char * __restrict s, wchar_t wc,
			 void * __restrict pspriv, size_t * __restrict nresult)
{
	_ENCODING_STATE *psenc;
	char buf[MB_LEN_MAX];
	int err = 0;
	size_t sz;
#if _ENCODING_IS_STATE_DEPENDENT
	size_t rsz = 0;
#endif

	_DIAGASSERT(cl != NULL);

	if (s == NULL) {
		/*
		 * use internal buffer.
		 */
		s = buf;
		wc = L'\0'; /* SUSv3 */
	}

	_RESTART_BEGIN(wcrtomb, _TO_CEI(cl), pspriv, psenc);
	sz = _ENCODING_MB_CUR_MAX(_CEI_TO_EI(_TO_CEI(cl)));
#if _ENCODING_IS_STATE_DEPENDENT
	if (wc == L'\0') {
		/* reset state */
		err = _FUNCNAME(put_state_reset)(_CEI_TO_EI(_TO_CEI(cl)), s,
						 sz, psenc, &rsz);
		if (err) {
			*nresult = -1;
			goto quit;
		}
		s += rsz;
		sz -= rsz;
	}
#endif
	err = _FUNCNAME(wcrtomb_priv)(_CEI_TO_EI(_TO_CEI(cl)), s, sz,
				      wc, psenc, nresult);
#if _ENCODING_IS_STATE_DEPENDENT
	if (err == 0)
		*nresult += rsz;
quit:
#endif
	if (err == E2BIG)
		err = EINVAL;
	_RESTART_END(wcrtomb, _TO_CEI(cl), pspriv, psenc);

	return err;
}

static int
/*ARGSUSED*/
_FUNCNAME(ctype_wcsrtombs)(void * __restrict cl, char * __restrict s,
			   const wchar_t ** __restrict pwcs, size_t n,
			   void * __restrict pspriv,
			   size_t * __restrict nresult)
{
	_ENCODING_STATE *psenc;
	_ENCODING_INFO *ei;
	int err = 0;

	_DIAGASSERT(cl != NULL);

	ei = _CEI_TO_EI(_TO_CEI(cl));
	_RESTART_BEGIN(wcsrtombs, _TO_CEI(cl), pspriv, psenc);
	err = _FUNCNAME(wcsrtombs_priv)(ei, s, pwcs, n, psenc, nresult);
	_RESTART_END(wcsrtombs, _TO_CEI(cl), pspriv, psenc);

	return err;
}

static int __used
/*ARGSUSED*/
_FUNCNAME(ctype_wcsnrtombs)(_citrus_ctype_rec_t * __restrict cc,
			   char * __restrict s,
			   const wchar_t ** __restrict pwcs, size_t in,
			   size_t n, void * __restrict pspriv,
			   size_t * __restrict nresult)
{
	void *cl = cc->cc_closure;
	_ENCODING_STATE *psenc;
	_ENCODING_INFO *ei;
	int err = 0;

	_DIAGASSERT(cl != NULL);

	ei = _CEI_TO_EI(_TO_CEI(cl));
	_RESTART_BEGIN(wcsnrtombs, _TO_CEI(cl), pspriv, psenc);
	err = _FUNCNAME(wcsnrtombs_priv)(ei, s, pwcs, in, n, psenc, nresult);
	_RESTART_END(wcsnrtombs, _TO_CEI(cl), pspriv, psenc);

	return err;
}

static int
/*ARGSUSED*/
_FUNCNAME(ctype_wcstombs)(void * __restrict cl, char * __restrict s,
			  const wchar_t * __restrict pwcs, size_t n,
			  size_t * __restrict nresult)
{
	_ENCODING_STATE state;
	_ENCODING_INFO *ei;
	int err;

	_DIAGASSERT(cl != NULL);

	ei = _CEI_TO_EI(_TO_CEI(cl));
	_FUNCNAME(init_state)(ei, &state);
	err = _FUNCNAME(wcsrtombs_priv)(ei, s, (const wchar_t **)&pwcs, n,
					&state, nresult);

	return err;
}

static int
_FUNCNAME(ctype_wctomb)(void * __restrict cl, char * __restrict s, wchar_t wc,
			int * __restrict nresult)
{
	_ENCODING_STATE *psenc;
	_ENCODING_INFO *ei;
	size_t nr, sz;
#if _ENCODING_IS_STATE_DEPENDENT
	size_t rsz = 0;
#endif
	int err = 0;

	_DIAGASSERT(cl != NULL);

	ei = _CEI_TO_EI(_TO_CEI(cl));
	psenc = &_CEI_TO_STATE(_TO_CEI(cl), wctomb);
	if (_STATE_NEEDS_EXPLICIT_INIT(psenc))
		_FUNCNAME(init_state)(ei, psenc);
	if (s == NULL) {
		_FUNCNAME(init_state)(ei, psenc);
		*nresult = _ENCODING_IS_STATE_DEPENDENT;
		return 0;
	}
	sz = _ENCODING_MB_CUR_MAX(_CEI_TO_EI(_TO_CEI(cl)));
#if _ENCODING_IS_STATE_DEPENDENT
	if (wc == L'\0') {
		/* reset state */
		err = _FUNCNAME(put_state_reset)(_CEI_TO_EI(_TO_CEI(cl)), s,
						 sz, psenc, &rsz);
		if (err) {
			*nresult = -1; /* XXX */
			return 0;
		}
		s += rsz;
		sz -= rsz;
	}
#endif
	err = _FUNCNAME(wcrtomb_priv)(ei, s, sz, wc, psenc, &nr);
#if _ENCODING_IS_STATE_DEPENDENT
	if (err == 0)
		*nresult = (int)(nr + rsz);
	else
#endif
	*nresult = (int)nr;

	return 0;
}

static int
/*ARGSUSED*/
_FUNCNAME(ctype_btowc)(_citrus_ctype_rec_t * __restrict cc,
		       int c, wint_t * __restrict wcresult)
{
	_ENCODING_STATE state;
	_ENCODING_INFO *ei;
	char mb;
	char const *s;
	wchar_t wc;
	size_t nr;
	int err;

	_DIAGASSERT(cc != NULL && cc->cc_closure != NULL);

	if (c == EOF) {
		*wcresult = WEOF;
		return 0;
	}
	ei = _CEI_TO_EI(_TO_CEI(cc->cc_closure));
	_FUNCNAME(init_state)(ei, &state);
	mb = (char)(unsigned)c;
	s = &mb;
	err = _FUNCNAME(mbrtowc_priv)(ei, &wc, &s, 1, &state, &nr);
	if (!err && (nr == 0 || nr == 1))
		*wcresult = (wint_t)wc;
	else
		*wcresult = WEOF;

	return 0;
}

static int
/*ARGSUSED*/
_FUNCNAME(ctype_wctob)(_citrus_ctype_rec_t * __restrict cc,
		       wint_t wc, int * __restrict cresult)
{
	_ENCODING_STATE state;
	_ENCODING_INFO *ei;
	char buf[MB_LEN_MAX];
	size_t nr;
	int err;

	_DIAGASSERT(cc != NULL && cc->cc_closure != NULL);

	if (wc == WEOF) {
		*cresult = EOF;
		return 0;
	}
	ei = _CEI_TO_EI(_TO_CEI(cc->cc_closure));
	_FUNCNAME(init_state)(ei, &state);
	err = _FUNCNAME(wcrtomb_priv)(ei, buf, _ENCODING_MB_CUR_MAX(ei),
				      (wchar_t)wc, &state, &nr);
	if (!err && nr == 1)
		*cresult = buf[0];
	else
		*cresult = EOF;

	return 0;
}
