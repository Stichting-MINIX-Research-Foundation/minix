/*	$NetBSD: t_resolver.c,v 1.11 2014/12/10 04:37:53 christos Exp $	*/

/*
 * Copyright (C) 2011-2014  Internet Systems Consortium, Inc. ("ISC")
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

/* Id: t_resolver.c,v 1.3 2011/02/03 12:18:11 tbox Exp  */

#include <config.h>

#include <ctype.h>
#include <stdlib.h>

#include <isc/mem.h>
#include <isc/util.h>
#include <isc/string.h>
#include <isc/timer.h>
#include <isc/task.h>

#include <dns/dispatch.h>
#include <dns/resolver.h>
#include <dns/result.h>
#include <dns/view.h>

#include <tests/t_api.h>

char *progname;

#define CHECK(x) RUNTIME_CHECK(ISC_R_SUCCESS == (x))


isc_mem_t *mctx = NULL;
isc_timermgr_t *timer_manager = NULL;
isc_socketmgr_t *socket_manager = NULL;
isc_taskmgr_t *task_manager = NULL;
dns_dispatchmgr_t *dispatch_manager = NULL;
dns_view_t *view = NULL;
dns_dispatch_t *dispatch_v4 = NULL;

static void
setup_create_dispatch_v4(void)
{
	isc_sockaddr_t local_address;
	isc_sockaddr_any(&local_address);

	CHECK(dns_dispatch_getudp(dispatch_manager, socket_manager,
				  task_manager, &local_address,
				  4096, 100, 100, 100, 500, 0, 0,
				  &dispatch_v4));
}
static void
setup(void) {
	/* 1 */ CHECK(isc_mem_create(0, 0, &mctx));
	/* 2 */ CHECK(isc_timermgr_create(mctx, &timer_manager));
	/* 3 */ CHECK(isc_taskmgr_create(mctx, 1, 0, &task_manager));
	/* 4 */ CHECK(isc_socketmgr_create(mctx, &socket_manager));
	/* 5 */ CHECK(dns_dispatchmgr_create(mctx, NULL, &dispatch_manager));
	/* 6 */ CHECK(dns_view_create(mctx, dns_rdataclass_in, "testview", &view));
	/* 7 */ setup_create_dispatch_v4();
}

static void
teardown(void) {
	/* 7 */ dns_dispatch_detach(&dispatch_v4);
	/* 6 */ dns_view_detach(&view);
	/* 5 */ dns_dispatchmgr_destroy(&dispatch_manager);
	/* 4 */ isc_socketmgr_destroy(&socket_manager);
	/* 3 */ isc_taskmgr_destroy(&task_manager);
	/* 2 */ isc_timermgr_destroy(&timer_manager);
	/* 1 */ isc_mem_destroy(&mctx);
}

static isc_result_t
make_resolver(dns_resolver_t **resolverp) {
	isc_result_t result;

	result = dns_resolver_create(view,
			    task_manager, 1, 1,
			    socket_manager,
			    timer_manager,
			    0, /* unsigned int options, */
			    dispatch_manager,
			    dispatch_v4,
			    NULL, /* dns_dispatch_t *dispatchv6, */
			    resolverp);

	return (result);
}

static void
destroy_resolver(dns_resolver_t **resolverp) {
	dns_resolver_shutdown(*resolverp);
	dns_resolver_detach(resolverp);
}

static void
test_dns_resolver_create(void) {
	dns_resolver_t *resolver = NULL;

	t_assert("test_dns_resolver_create", 1, T_REQUIRED, "%s",
		 "a resolver can be created successfully");
	setup();
	CHECK(make_resolver(&resolver));

	destroy_resolver(&resolver);
	teardown();

	t_result(T_PASS);
}

static void
test_dns_resolver_gettimeout(void) {
	dns_resolver_t *resolver = NULL;
	int test_result;
	unsigned int timeout;

	t_assert("test_dns_resolver_gettimeout", 1, T_REQUIRED, "%s",
		 "The default timeout is returned from _gettimeout()");
	setup();
	CHECK(make_resolver(&resolver));

	timeout = dns_resolver_gettimeout(resolver);
	t_info("The default timeout is %d second%s\n", timeout, (timeout == 1 ? "" : "s"));
	test_result = (timeout > 0) ? T_PASS : T_FAIL;

	destroy_resolver(&resolver);
	teardown();

	t_result(test_result);
}

static void
test_dns_resolver_settimeout(void) {
	dns_resolver_t *resolver = NULL;
	int test_result;
	unsigned int default_timeout, timeout;

	t_assert("test_dns_resolver_settimeout", 1, T_REQUIRED, "%s",
		 "_settimeout() can change the timeout to a non-default");
	setup();
	CHECK(make_resolver(&resolver));

	default_timeout = dns_resolver_gettimeout(resolver);
	t_info("The default timeout is %d second%s\n", default_timeout,
	       (default_timeout == 1 ? "" : "s"));

	dns_resolver_settimeout(resolver, default_timeout + 1);
	timeout = dns_resolver_gettimeout(resolver);
	t_info("The new timeout is %d second%s\n", timeout,
	       (timeout == 1 ? "" : "s"));
	test_result = (timeout == default_timeout + 1) ? T_PASS : T_FAIL;

	destroy_resolver(&resolver);
	teardown();

	t_result(test_result);
}

static void
test_dns_resolver_settimeout_to_default(void) {
	dns_resolver_t *resolver = NULL;
	int test_result;
	unsigned int default_timeout, timeout;

	t_assert("test_dns_resolver_settimeout_to_default", 1, T_REQUIRED, "%s",
		 "_settimeout() can change the timeout back to a default value"
		 " by specifying 0 as the timeout.");
	setup();
	CHECK(make_resolver(&resolver));

	default_timeout = dns_resolver_gettimeout(resolver);
	t_info("The default timeout is %d second%s\n", default_timeout,
	       (default_timeout == 1 ? "" : "s"));

	dns_resolver_settimeout(resolver, default_timeout - 1);
	timeout = dns_resolver_gettimeout(resolver);
	t_info("The new timeout is %d second%s\n", timeout,
	       (timeout == 1 ? "" : "s"));

	dns_resolver_settimeout(resolver, 0);
	timeout = dns_resolver_gettimeout(resolver);
	test_result = (timeout == default_timeout) ? T_PASS : T_FAIL;

	destroy_resolver(&resolver);
	teardown();

	t_result(test_result);
}

static void
test_dns_resolver_settimeout_over_maximum(void) {
	dns_resolver_t *resolver = NULL;
	int test_result;
	unsigned int timeout;

	t_assert("test_dns_resolver_settimeout_over_maximum", 1, T_REQUIRED, "%s",
		 "_settimeout() cannot set the value larger than the maximum.");
	setup();
	CHECK(make_resolver(&resolver));

	dns_resolver_settimeout(resolver, 4000000);
	timeout = dns_resolver_gettimeout(resolver);
	t_info("The new timeout is %d second%s\n", timeout,
	       (timeout == 1 ? "" : "s"));

	test_result = (timeout < 4000000 && timeout > 0) ? T_PASS : T_FAIL;

	destroy_resolver(&resolver);
	teardown();

	t_result(test_result);
}


testspec_t T_testlist[] = {
	{ (PFV) test_dns_resolver_create,	"dns_resolver_create"		},
	{ (PFV) test_dns_resolver_settimeout,	"dns_resolver_settimeout"	},
	{ (PFV) test_dns_resolver_gettimeout,	"dns_resolver_gettimeout"	},
	{ (PFV) test_dns_resolver_settimeout_to_default, "test_dns_resolver_settimeout_to_default" },
	{ (PFV) test_dns_resolver_settimeout_over_maximum, "test_dns_resolver_settimeout_over_maximum" },
	{ (PFV) 0,	NULL }
};

#ifdef WIN32
int
main(int argc, char **argv) {
	t_settests(T_testlist);
	return (t_main(argc, argv));
}
#endif
