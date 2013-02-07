/*
 * Copyright (C) 2004, 2007-2009  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: txt_16.c,v 1.47 2009-12-04 22:06:37 tbox Exp $ */

/* Reviewed: Thu Mar 16 15:40:00 PST 2000 by bwelling */

#ifndef RDATA_GENERIC_TXT_16_C
#define RDATA_GENERIC_TXT_16_C

#define RRTYPE_TXT_ATTRIBUTES (0)

static inline isc_result_t
fromtext_txt(ARGS_FROMTEXT) {
	isc_token_t token;
	int strings;

	REQUIRE(type == 16);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(origin);
	UNUSED(options);
	UNUSED(callbacks);

	strings = 0;
	for (;;) {
		RETERR(isc_lex_getmastertoken(lexer, &token,
					      isc_tokentype_qstring,
					      ISC_TRUE));
		if (token.type != isc_tokentype_qstring &&
		    token.type != isc_tokentype_string)
			break;
		RETTOK(txt_fromtext(&token.value.as_textregion, target));
		strings++;
	}
	/* Let upper layer handle eol/eof. */
	isc_lex_ungettoken(lexer, &token);
	return (strings == 0 ? ISC_R_UNEXPECTEDEND : ISC_R_SUCCESS);
}

static inline isc_result_t
totext_txt(ARGS_TOTEXT) {
	isc_region_t region;

	UNUSED(tctx);

	REQUIRE(rdata->type == 16);

	dns_rdata_toregion(rdata, &region);

	while (region.length > 0) {
		RETERR(txt_totext(&region, target));
		if (region.length > 0)
			RETERR(str_totext(" ", target));
	}

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_txt(ARGS_FROMWIRE) {
	isc_result_t result;

	REQUIRE(type == 16);

	UNUSED(type);
	UNUSED(dctx);
	UNUSED(rdclass);
	UNUSED(options);

	do {
		result = txt_fromwire(source, target);
		if (result != ISC_R_SUCCESS)
			return (result);
	} while (!buffer_empty(source));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_txt(ARGS_TOWIRE) {
	isc_region_t region;

	REQUIRE(rdata->type == 16);

	UNUSED(cctx);

	isc_buffer_availableregion(target, &region);
	if (region.length < rdata->length)
		return (ISC_R_NOSPACE);

	memcpy(region.base, rdata->data, rdata->length);
	isc_buffer_add(target, rdata->length);
	return (ISC_R_SUCCESS);
}

static inline int
compare_txt(ARGS_COMPARE) {
	isc_region_t r1;
	isc_region_t r2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == 16);

	dns_rdata_toregion(rdata1, &r1);
	dns_rdata_toregion(rdata2, &r2);
	return (isc_region_compare(&r1, &r2));
}

static inline isc_result_t
fromstruct_txt(ARGS_FROMSTRUCT) {
	dns_rdata_txt_t *txt = source;
	isc_region_t region;
	isc_uint8_t length;

	REQUIRE(type == 16);
	REQUIRE(source != NULL);
	REQUIRE(txt->common.rdtype == type);
	REQUIRE(txt->common.rdclass == rdclass);
	REQUIRE(txt->txt != NULL && txt->txt_len != 0);

	UNUSED(type);
	UNUSED(rdclass);

	region.base = txt->txt;
	region.length = txt->txt_len;
	while (region.length > 0) {
		length = uint8_fromregion(&region);
		isc_region_consume(&region, 1);
		if (region.length < length)
			return (ISC_R_UNEXPECTEDEND);
		isc_region_consume(&region, length);
	}

	return (mem_tobuffer(target, txt->txt, txt->txt_len));
}

static inline isc_result_t
tostruct_txt(ARGS_TOSTRUCT) {
	dns_rdata_txt_t *txt = target;
	isc_region_t r;

	REQUIRE(rdata->type == 16);
	REQUIRE(target != NULL);

	txt->common.rdclass = rdata->rdclass;
	txt->common.rdtype = rdata->type;
	ISC_LINK_INIT(&txt->common, link);

	dns_rdata_toregion(rdata, &r);
	txt->txt_len = r.length;
	txt->txt = mem_maybedup(mctx, r.base, r.length);
	if (txt->txt == NULL)
		return (ISC_R_NOMEMORY);

	txt->offset = 0;
	txt->mctx = mctx;
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_txt(ARGS_FREESTRUCT) {
	dns_rdata_txt_t *txt = source;

	REQUIRE(source != NULL);
	REQUIRE(txt->common.rdtype == 16);

	if (txt->mctx == NULL)
		return;

	if (txt->txt != NULL)
		isc_mem_free(txt->mctx, txt->txt);
	txt->mctx = NULL;
}

static inline isc_result_t
additionaldata_txt(ARGS_ADDLDATA) {
	REQUIRE(rdata->type == 16);

	UNUSED(rdata);
	UNUSED(add);
	UNUSED(arg);

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
digest_txt(ARGS_DIGEST) {
	isc_region_t r;

	REQUIRE(rdata->type == 16);

	dns_rdata_toregion(rdata, &r);

	return ((digest)(arg, &r));
}

static inline isc_boolean_t
checkowner_txt(ARGS_CHECKOWNER) {

	REQUIRE(type == 16);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline isc_boolean_t
checknames_txt(ARGS_CHECKNAMES) {

	REQUIRE(rdata->type == 16);

	UNUSED(rdata);
	UNUSED(owner);
	UNUSED(bad);

	return (ISC_TRUE);
}

static inline isc_result_t
casecompare_txt(ARGS_COMPARE) {
	return (compare_txt(rdata1, rdata2));
}

#endif	/* RDATA_GENERIC_TXT_16_C */
