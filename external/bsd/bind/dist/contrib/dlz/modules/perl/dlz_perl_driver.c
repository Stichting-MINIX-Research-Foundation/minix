/*	$NetBSD: dlz_perl_driver.c,v 1.1.1.3 2014/12/10 03:34:31 christos Exp $	*/

/*
 * Copyright (C) 2002 Stichting NLnet, Netherlands, stichting@nlnet.nl.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND STICHTING NLNET
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * STICHTING NLNET BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * The development of Dynamically Loadable Zones (DLZ) for Bind 9 was
 * conceived and contributed by Rob Butler.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ROB BUTLER
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * ROB BUTLER BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (C) 2009-2012  John Eaglesham
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND JOHN EAGLESHAM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * JOHN EAGLESHAM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <EXTERN.h>
#include <perl.h>

#include <dlz_minimal.h>

#include "dlz_perl_driver.h"

/* Enable debug logging? */
#if 0
#define carp(...) 	cd->log(ISC_LOG_INFO, __VA_ARGS__);
#else
#define carp(...)
#endif

#ifndef MULTIPLICITY
/* This is a pretty terrible work-around for handling HUP/rndc reconfig, but
 * the way BIND/DLZ handles reloads causes it to create a second back end
 * before removing the first. In the case of a single global interpreter,
 * serious problems arise. We can hack around this, but it's much better to do
 * it properly and link against a perl compiled with multiplicity. */
static PerlInterpreter *global_perl = NULL;
static int global_perl_dont_free = 0;
#endif

typedef struct config_data {
	PerlInterpreter	*perl;
	char			*perl_source;
	SV				*perl_class;

	/* Functions given to us by bind9 */
	log_t *log;
	dns_sdlz_putrr_t *putrr;
	dns_sdlz_putnamedrr_t *putnamedrr;
	dns_dlz_writeablezone_t *writeable_zone;
} config_data_t;

/* Note, this code generates warnings due to lost type qualifiers.  This code
 * is (almost) verbatim from perlembed, and is known to work correctly despite
 * the warnings.
 */
EXTERN_C void xs_init (pTHX);
EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);
EXTERN_C void boot_DLZ_Perl__clientinfo (pTHX_ CV* cv);
EXTERN_C void boot_DLZ_Perl (pTHX_ CV* cv);
EXTERN_C void
xs_init(pTHX)
{
		char *file = __FILE__;
		dXSUB_SYS;

		/* DynaLoader is a special case */
		newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
		newXS("DLZ_Perl::clientinfo::bootstrap", boot_DLZ_Perl__clientinfo, file);
		newXS("DLZ_Perl::bootstrap", boot_DLZ_Perl, file);
}

/*
 * methods
 */

/*
 * remember a helper function, from the bind9 dlz_dlopen driver
 */
static void b9_add_helper(config_data_t *state,
			  const char *helper_name, void *ptr)
{
	if (strcmp(helper_name, "log") == 0)
		state->log = ptr;
	if (strcmp(helper_name, "putrr") == 0)
		state->putrr = ptr;
	if (strcmp(helper_name, "putnamedrr") == 0)
		state->putnamedrr = ptr;
	if (strcmp(helper_name, "writeable_zone") == 0)
		state->writeable_zone = ptr;
}

int dlz_version(unsigned int *flags) {
	return DLZ_DLOPEN_VERSION;
}

isc_result_t dlz_allnodes(const char *zone, void *dbdata,
			  dns_sdlzallnodes_t *allnodes)
{
	config_data_t *cd = (config_data_t *) dbdata;
	isc_result_t retval;
	int rrcount, r;
	SV *record_ref;
	SV **rr_name;
	SV **rr_type;
	SV **rr_ttl;
	SV **rr_data;
#ifdef MULTIPLICITY
	PerlInterpreter *my_perl = cd->perl;
#endif
	dSP;

	PERL_SET_CONTEXT(cd->perl);
	ENTER;
	SAVETMPS;
	
	PUSHMARK(SP);
	XPUSHs(cd->perl_class);
	XPUSHs(sv_2mortal(newSVpv(zone, 0)));
	PUTBACK;

	carp("DLZ Perl: Calling allnodes for zone %s", zone);
	rrcount = call_method("allnodes", G_ARRAY|G_EVAL);
	carp("DLZ Perl: Call to allnodes returned rrcount of %i", rrcount);

	SPAGAIN;

	if (SvTRUE(ERRSV)) {
		POPs;
		cd->log(ISC_LOG_ERROR, "DLZ Perl: allnodes for zone %s died in eval: %s", zone, SvPV_nolen(ERRSV));
		retval = ISC_R_FAILURE;
		goto CLEAN_UP_AND_RETURN;
	}

	if (!rrcount) {
		retval = ISC_R_NOTFOUND;
		goto CLEAN_UP_AND_RETURN;
	}

	retval = ISC_R_SUCCESS;
	r = 0;
	while (r++ < rrcount) {
		record_ref = POPs;
		if (
			(!SvROK(record_ref)) ||
			(SvTYPE(SvRV(record_ref)) != SVt_PVAV)
		) {
			cd->log(ISC_LOG_ERROR,
				"DLZ Perl: allnodes for zone %s "
				"returned an invalid value "
				"(expected array of arrayrefs)",
				zone);
			retval = ISC_R_FAILURE;
			break;
		}

		record_ref = SvRV(record_ref);

		rr_name = av_fetch((AV *) record_ref, 0, 0);
		rr_type = av_fetch((AV *) record_ref, 1, 0);
		rr_ttl = av_fetch((AV *) record_ref, 2, 0);
		rr_data = av_fetch((AV *) record_ref, 3, 0);

		if (rr_name == NULL || rr_type == NULL ||
		    rr_ttl == NULL || rr_data == NULL)
		{
			cd->log(ISC_LOG_ERROR,
				"DLZ Perl: allnodes for zone %s "
				"returned an array that was missing data",
				zone);
			retval = ISC_R_FAILURE;
			break;
		}

		carp("DLZ Perl: Got record %s/%s = %s",
		     SvPV_nolen(*rr_name), SvPV_nolen(*rr_type),
		     SvPV_nolen(*rr_data));
   		retval = cd->putnamedrr(allnodes,
					SvPV_nolen(*rr_name),
					SvPV_nolen(*rr_type),
					SvIV(*rr_ttl), SvPV_nolen(*rr_data));
		if (retval != ISC_R_SUCCESS) {
			cd->log(ISC_LOG_ERROR,
				"DLZ Perl: putnamedrr in allnodes "
				"for zone %s failed with code %i "
				"(did lookup return invalid record data?)",
				zone, retval);
			break;
		}
	}

CLEAN_UP_AND_RETURN:
	PUTBACK;
	FREETMPS;
	LEAVE;

	carp("DLZ Perl: Returning from allnodes, r = %i, retval = %i",
	     r, retval);

	return (retval);
}

isc_result_t
dlz_allowzonexfr(void *dbdata, const char *name, const char *client) {
	config_data_t *cd = (config_data_t *) dbdata;
	int r;
	isc_result_t retval;
#ifdef MULTIPLICITY
	PerlInterpreter *my_perl = cd->perl;
#endif
	dSP;

	PERL_SET_CONTEXT(cd->perl);
	ENTER;
	SAVETMPS;
	
	PUSHMARK(SP);
	XPUSHs(cd->perl_class);
	XPUSHs(sv_2mortal(newSVpv(name, 0)));
	XPUSHs(sv_2mortal(newSVpv(client, 0)));
	PUTBACK;

	r = call_method("allowzonexfr", G_SCALAR|G_EVAL);
	SPAGAIN;

	if (SvTRUE(ERRSV)) {
		/*
		 * On error there's an undef at the top of the stack. Pop
		 * it away so we don't leave junk on the stack for the next
		 * caller.
		 */
		POPs;
		cd->log(ISC_LOG_ERROR,
			"DLZ Perl: allowzonexfr died in eval: %s",
			SvPV_nolen(ERRSV));
		retval = ISC_R_FAILURE;
	} else if (r == 0) {
		/* Client returned nothing -- zone not found. */
	 	retval = ISC_R_NOTFOUND;
	} else if (r > 1) {
		/* Once again, clean out the stack when possible. */
		while (r--) POPi;
		cd->log(ISC_LOG_ERROR,
			"DLZ Perl: allowzonexfr returned too many parameters!");
		retval = ISC_R_FAILURE;
	} else {
		/*
		 * Client returned true/false -- we're authoritative for
		 * the zone.
		 */
		r = POPi;
		if (r)
			retval = ISC_R_SUCCESS;
		else
			retval = ISC_R_NOPERM;
	}

	PUTBACK;
	FREETMPS;
	LEAVE;
	return (retval);
}

#if DLZ_DLOPEN_VERSION < 3
isc_result_t
dlz_findzonedb(void *dbdata, const char *name)
#else
isc_result_t
dlz_findzonedb(void *dbdata, const char *name,
	       dns_clientinfomethods_t *methods,
	       dns_clientinfo_t *clientinfo)
#endif
{
	config_data_t *cd = (config_data_t *) dbdata;
	int r;
	isc_result_t retval;
#ifdef MULTIPLICITY
	PerlInterpreter *my_perl = cd->perl;
#endif

#if DLZ_DLOPEN_VERSION >= 3
	UNUSED(methods);
	UNUSED(clientinfo);
#endif

	dSP;
	carp("DLZ Perl: findzone looking for '%s'", name);

	PERL_SET_CONTEXT(cd->perl);
	ENTER;
	SAVETMPS;
	
	PUSHMARK(SP);
	XPUSHs(cd->perl_class);
	XPUSHs(sv_2mortal(newSVpv(name, 0)));
	PUTBACK;

	r = call_method("findzone", G_SCALAR|G_EVAL);
	SPAGAIN;

	if (SvTRUE(ERRSV)) {
		/*
		 * On error there's an undef at the top of the stack. Pop
		 * it away so we don't leave junk on the stack for the next
		 * caller.
		 */
		POPs;
		cd->log(ISC_LOG_ERROR,
			"DLZ Perl: findzone died in eval: %s",
			SvPV_nolen(ERRSV));
		retval = ISC_R_FAILURE;
	} else if (r == 0) {
	 	retval = ISC_R_FAILURE;
	} else if (r > 1) {
		/* Once again, clean out the stack when possible. */
		while (r--) POPi;
		cd->log(ISC_LOG_ERROR,
			"DLZ Perl: findzone returned too many parameters!");
		retval = ISC_R_FAILURE;
	} else {
		r = POPi;
		if (r)
			retval = ISC_R_SUCCESS;
		else
			retval = ISC_R_NOTFOUND;
	}

	PUTBACK;
	FREETMPS;
	LEAVE;
	return (retval);
}


#if DLZ_DLOPEN_VERSION == 1
isc_result_t
dlz_lookup(const char *zone, const char *name,
	   void *dbdata, dns_sdlzlookup_t *lookup)
#else
isc_result_t
dlz_lookup(const char *zone, const char *name,
	   void *dbdata, dns_sdlzlookup_t *lookup,
	   dns_clientinfomethods_t *methods,
	   dns_clientinfo_t *clientinfo)
#endif
{
	isc_result_t retval;
	config_data_t *cd = (config_data_t *) dbdata;
	int rrcount, r;
	dlz_perl_clientinfo_opaque opaque;
	SV *record_ref;
	SV **rr_type;
	SV **rr_ttl;
	SV **rr_data;
#ifdef MULTIPLICITY
	PerlInterpreter *my_perl = cd->perl;
#endif

#if DLZ_DLOPEN_VERSION >= 2
	UNUSED(methods);
	UNUSED(clientinfo);
#endif

	dSP;
	PERL_SET_CONTEXT(cd->perl);
	ENTER;
	SAVETMPS;

	opaque.methods = methods;
	opaque.clientinfo = clientinfo;

	PUSHMARK(SP);
	XPUSHs(cd->perl_class);
	XPUSHs(sv_2mortal(newSVpv(name, 0)));
	XPUSHs(sv_2mortal(newSVpv(zone, 0)));
	XPUSHs(sv_2mortal(newSViv((IV)&opaque)));
	PUTBACK;

	carp("DLZ Perl: Searching for name %s in zone %s", name, zone);
	rrcount = call_method("lookup", G_ARRAY|G_EVAL);
	carp("DLZ Perl: Call to lookup returned %i", rrcount);

	SPAGAIN;

	if (SvTRUE(ERRSV)) {
		POPs;
		cd->log(ISC_LOG_ERROR, "DLZ Perl: lookup died in eval: %s",
			SvPV_nolen(ERRSV));
		retval = ISC_R_FAILURE;
		goto CLEAN_UP_AND_RETURN;
	}

	if (!rrcount) {
		retval = ISC_R_NOTFOUND;
		goto CLEAN_UP_AND_RETURN;
	}

	retval = ISC_R_SUCCESS;
	r = 0;
	while (r++ < rrcount) {
		record_ref = POPs;
		if ((!SvROK(record_ref)) ||
		    (SvTYPE(SvRV(record_ref)) != SVt_PVAV))
		{
			cd->log(ISC_LOG_ERROR,
				"DLZ Perl: lookup returned an "
				"invalid value (expected array of arrayrefs)!");
			retval = ISC_R_FAILURE;
			break;
		}

		record_ref = SvRV(record_ref);

		rr_type = av_fetch((AV *) record_ref, 0, 0);
		rr_ttl = av_fetch((AV *) record_ref, 1, 0);
		rr_data = av_fetch((AV *) record_ref, 2, 0);

		if (rr_type == NULL || rr_ttl == NULL || rr_data == NULL) {
			cd->log(ISC_LOG_ERROR,
				"DLZ Perl: lookup for record %s in "
				"zone %s returned an array that was "
				"missing data", name, zone);
			retval = ISC_R_FAILURE;
			break;
		}

		carp("DLZ Perl: Got record %s = %s",
		     SvPV_nolen(*rr_type), SvPV_nolen(*rr_data));
		retval = cd->putrr(lookup, SvPV_nolen(*rr_type),
				   SvIV(*rr_ttl), SvPV_nolen(*rr_data));

		if (retval != ISC_R_SUCCESS) {
			cd->log(ISC_LOG_ERROR,
				"DLZ Perl: putrr for lookup of %s in "
				"zone %s failed with code %i "
				"(did lookup return invalid record data?)",
				name, zone, retval);
			break;
		}
	}

CLEAN_UP_AND_RETURN:
	PUTBACK;
	FREETMPS;
	LEAVE;

	carp("DLZ Perl: Returning from lookup, r = %i, retval = %i", r, retval);

	return (retval);
}

const char *
#ifdef MULTIPLICITY
missing_perl_method(const char *perl_class_name, PerlInterpreter *my_perl)
#else
missing_perl_method(const char *perl_class_name)
#endif
{
	const int BUF_LEN = 64; /* Should be big enough, right? hah */
	char full_name[BUF_LEN];
	const char *methods[] = { "new", "findzone", "lookup", NULL };
	int i = 0;

	while( methods[i] != NULL ) {
		snprintf(full_name, BUF_LEN, "%s::%s",
			 perl_class_name, methods[i]);

		if (get_cv(full_name, 0) == NULL) {
			return methods[i];
		}
		i++;
	}

	return (NULL);
}

isc_result_t
dlz_create(const char *dlzname, unsigned int argc, char *argv[],
	   void **dbdata, ...)
{
	config_data_t *cd;
	char *init_args[] = { NULL, NULL };
	char *perlrun[] = { "", NULL, "dlz perl", NULL };
	char *perl_class_name;
	int r;
	va_list ap;
	const char *helper_name;
	const char *missing_method_name;
	char *call_argv_args = NULL;
#ifdef MULTIPLICITY
	PerlInterpreter *my_perl;
#endif

	cd = malloc(sizeof(config_data_t));
	if (cd == NULL)
		return (ISC_R_NOMEMORY);

	memset(cd, 0, sizeof(config_data_t));

	/* fill in the helper functions */
	va_start(ap, dbdata);
	while ((helper_name = va_arg(ap, const char *)) != NULL) {
		b9_add_helper(cd, helper_name, va_arg(ap, void*));
	}
	va_end(ap);

	if (argc < 2) {
		cd->log(ISC_LOG_ERROR,
			"DLZ Perl '%s': Missing script argument.",
			dlzname);
		return (ISC_R_FAILURE);
	}

	if (argc < 3) {
		cd->log(ISC_LOG_ERROR,
			"DLZ Perl '%s': Missing class name argument.",
			dlzname);
		return (ISC_R_FAILURE);
	}
	perl_class_name = argv[2];

	cd->log(ISC_LOG_INFO, "DLZ Perl '%s': Loading '%s' from location '%s'",
		 dlzname, perl_class_name, argv[1], argc);

#ifndef MULTIPLICITY
	if (global_perl) {
		/*
		 * PERL_SET_CONTEXT not needed here as we're guaranteed to
		 * have an implicit context thanks to an undefined
		 * MULTIPLICITY.
		 */
		PL_perl_destruct_level = 1;
		perl_destruct(global_perl);
		perl_free(global_perl);
		global_perl = NULL;
		global_perl_dont_free = 1;
	}
#endif

	cd->perl = perl_alloc();
	if (cd->perl == NULL) {
		free(cd);
		return (ISC_R_FAILURE);
	}
#ifdef MULTIPLICITY
	my_perl = cd->perl;
#endif
	PERL_SET_CONTEXT(cd->perl);
 
	/*
	 * We will re-create the interpreter during an rndc reconfig, so we
	 * must set this variable per perlembed in order to insure we can
	 * clean up Perl at a later time.
	 */
	PL_perl_destruct_level = 1;
	perl_construct(cd->perl);
	PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
	/* Prevent crashes from clients writing to $0 */
	PL_origalen = 1;

	cd->perl_source = strdup(argv[1]);
	if (cd->perl_source == NULL) {
		free(cd);
		return (ISC_R_NOMEMORY);
	}

	perlrun[1] = cd->perl_source;
	if (perl_parse(cd->perl, xs_init, 3, perlrun, (char **)NULL)) {
		cd->log(ISC_LOG_ERROR,
			"DLZ Perl '%s': Failed to parse Perl script, aborting",
			dlzname);
		goto CLEAN_UP_PERL_AND_FAIL;
	}

	/* Let Perl know about our callbacks. */
	call_argv("DLZ_Perl::clientinfo::bootstrap",
		  G_DISCARD|G_NOARGS, &call_argv_args);
	call_argv("DLZ_Perl::bootstrap",
		  G_DISCARD|G_NOARGS, &call_argv_args);

	/*
	 * Run the script. We don't really need to do this since we have
	 * the init callback, but there's not really a downside either.
	 */
	if (perl_run(cd->perl)) {
		cd->log(ISC_LOG_ERROR,
			"DLZ Perl '%s': Script exited with an error, aborting",
			dlzname);
		goto CLEAN_UP_PERL_AND_FAIL;
	}

#ifdef MULTIPLICITY
	if (missing_method_name = missing_perl_method(perl_class_name, my_perl))
#else
	if (missing_method_name = missing_perl_method(perl_class_name))
#endif
	{
		cd->log(ISC_LOG_ERROR,
			"DLZ Perl '%s': Missing required function '%s', "
			"aborting", dlzname, missing_method_name);
		goto CLEAN_UP_PERL_AND_FAIL;
	}

	dSP;
	ENTER;
	SAVETMPS;

	PUSHMARK(SP);
	XPUSHs(sv_2mortal(newSVpv(perl_class_name, 0)));

	/* Build flattened hash of config info. */
	XPUSHs(sv_2mortal(newSVpv("log_context", 0)));
	XPUSHs(sv_2mortal(newSViv((IV)cd->log)));

	/* Argument to pass to new? */
	if (argc == 4) {
		XPUSHs(sv_2mortal(newSVpv("argv", 0)));
		XPUSHs(sv_2mortal(newSVpv(argv[3], 0)));
	}

	PUTBACK;

	r = call_method("new", G_EVAL|G_SCALAR);

	SPAGAIN;

	if (r) cd->perl_class = SvREFCNT_inc(POPs);

	PUTBACK;
	FREETMPS;
	LEAVE;

	if (SvTRUE(ERRSV)) {
		POPs;
		cd->log(ISC_LOG_ERROR,
			"DLZ Perl '%s': new died in eval: %s",
			dlzname, SvPV_nolen(ERRSV));
		goto CLEAN_UP_PERL_AND_FAIL;
	}

	if (!r || !sv_isobject(cd->perl_class)) {
		cd->log(ISC_LOG_ERROR,
			"DLZ Perl '%s': new failed to return a blessed object",
			dlzname);
		goto CLEAN_UP_PERL_AND_FAIL;
	}

	*dbdata = cd;

#ifndef MULTIPLICITY
	global_perl = cd->perl;
#endif
	return (ISC_R_SUCCESS);

CLEAN_UP_PERL_AND_FAIL:
	PL_perl_destruct_level = 1;
	perl_destruct(cd->perl);
	perl_free(cd->perl);
	free(cd->perl_source);
	free(cd);
	return (ISC_R_FAILURE);
}

void dlz_destroy(void *dbdata) {
	config_data_t *cd = (config_data_t *) dbdata;
#ifdef MULTIPLICITY
	PerlInterpreter *my_perl = cd->perl;
#endif

	cd->log(ISC_LOG_INFO, "DLZ Perl: Unloading driver.");

#ifndef MULTIPLICITY
	if (!global_perl_dont_free) {
#endif
		PERL_SET_CONTEXT(cd->perl);
		PL_perl_destruct_level = 1;
		perl_destruct(cd->perl);
		perl_free(cd->perl);
#ifndef MULTIPLICITY
		global_perl_dont_free = 0;
		global_perl = NULL;
	}
#endif

	free(cd->perl_source);
	free(cd);
}
