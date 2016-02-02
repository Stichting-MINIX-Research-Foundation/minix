/*	$NetBSD: dev_verbose.h,v 1.1 2014/09/21 14:30:22 christos Exp $ */

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef _DEV_DEV_VERBOSE_H_
#define	_DEV_DEV_VERBOSE_H_

const char *dev_findvendor(char *, size_t, const char *, size_t, 
	const uint16_t *, size_t, uint16_t);
const char *dev_findproduct(char *, size_t, const char *, size_t, 
	const uint16_t *, size_t, uint16_t, uint16_t);

#define DEV_VERBOSE_COMMON_DEFINE(tag)					\
static const char *							\
tag ## _findvendor_real(char *buf, size_t len, uint16_t vendor)		\
{									\
	return dev_findvendor(buf, len, tag ## _words,			\
	    __arraycount(tag ## _words), tag ## _vendors,		\
	    __arraycount(tag ## _vendors), vendor);			\
}									\
									\
static const char *							\
tag ## _findproduct_real(char *buf, size_t len, uint16_t vendor,	\
    uint16_t product)							\
{									\
	return dev_findproduct(buf, len, tag ## _words,			\
	    __arraycount(tag ## _words), tag ## _products,		\
	    __arraycount(tag ## _products), vendor, product);		\
}									\

#ifdef _KERNEL

#define DEV_VERBOSE_MODULE_DEFINE(tag, deps)				\
DEV_VERBOSE_COMMON_DEFINE(tag)						\
extern int tag ## verbose_loaded;					\
									\
static int								\
tag ## verbose_modcmd(modcmd_t cmd, void *arg)				\
{									\
	static const char *(*saved_findvendor)(char *, size_t,		\
	    uint16_t);							\
	static const char *(*saved_findproduct)(char *, size_t,		\
	    uint16_t, uint16_t);					\
									\
	switch (cmd) {							\
	case MODULE_CMD_INIT:						\
		saved_findvendor = tag ## _findvendor;			\
		saved_findproduct = tag ## _findproduct;		\
		tag ## _findvendor = tag ## _findvendor_real;		\
		tag ## _findproduct = tag ## _findproduct_real;		\
		tag ## verbose_loaded = 1;				\
		return 0;						\
	case MODULE_CMD_FINI:						\
		tag ## _findvendor = saved_findvendor;			\
		tag ## _findproduct = saved_findproduct;		\
		tag ## verbose_loaded = 0;				\
		return 0;						\
	default:							\
		return ENOTTY;						\
	}								\
}									\
MODULE(MODULE_CLASS_MISC, tag ## verbose, deps)

#endif /* KERNEL */

#define DEV_VERBOSE_DECLARE(tag)					\
extern const char * (*tag ## _findvendor)(char *, size_t, uint16_t);	\
extern const char * (*tag ## _findproduct)(char *, size_t, uint16_t, uint16_t)

#if defined(_KERNEL)
#define DEV_VERBOSE_DEFINE(tag)						\
int tag ## verbose_loaded = 0;						\
									\
static void								\
tag ## _load_verbose(void)						\
{									\
									\
	if (tag ## verbose_loaded == 0)					\
		module_autoload(# tag "verbose", MODULE_CLASS_MISC);	\
}									\
									\
static const char *							\
tag ## _findvendor_stub(char *buf, size_t len, uint16_t vendor)		\
{									\
									\
	tag ## _load_verbose();						\
	if (tag ## verbose_loaded)					\
		return tag ## _findvendor(buf, len, vendor);		\
	else {								\
		snprintf(buf, len, "vendor %4.4x", vendor);		\
		return NULL;						\
	}								\
}									\
									\
static const char *							\
tag ## _findproduct_stub(char *buf, size_t len, uint16_t vendor,	\
    uint16_t product)							\
{									\
									\
	tag ## _load_verbose();						\
	if (tag ## verbose_loaded)					\
		return tag ## _findproduct(buf, len, vendor, product);	\
	else {								\
		snprintf(buf, len, "product %4.4x", product);		\
		return NULL;						\
	}								\
}									\
									\
const char *(*tag ## _findvendor)(char *, size_t, uint16_t) = 		\
    tag ## _findvendor_stub;						\
const char *(*tag ## _findproduct)(char *, size_t, uint16_t, uint16_t) =\
    tag ## _findproduct_stub;						\

#else

#define DEV_VERBOSE_DEFINE(tag)						\
DEV_VERBOSE_COMMON_DEFINE(tag)						\
const char *(*tag ## _findvendor)(char *, size_t, uint16_t) = 		\
    tag ## _findvendor_real;						\
const char *(*tag ## _findproduct)(char *, size_t, uint16_t, uint16_t) =\
    tag ## _findproduct_real;						\

#endif /* _KERNEL */

#endif /* _DEV_DEV_VERBOSE_H_ */
