/*	$NetBSD: resolve-test.c,v 1.2 2017/01/28 21:31:50 christos Exp $	*/

/*
 * Copyright (c) 1995 - 2016 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <config.h>

#include <krb5/roken.h>
#include <krb5/getarg.h>
#include <assert.h>
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
#include <krb5/resolve.h>

static int srv_rr_order = 1;
static int loop_integer = 1;
static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"srv-rr-order", 0,
     arg_negative_flag,                 &srv_rr_order,
     "do not test SRV RR ordering", NULL },
    {"loop",	0,	arg_integer,	&loop_integer,
     "loop resolving", NULL },
    {"version",	0,	arg_flag,	&version_flag,
     "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,
     NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "dns-record resource-record-type");
    exit (ret);
}

#define NUMRRS 16

static
int
test_rk_dns_srv_order(size_t run)
{
    struct rk_dns_reply reply;
    struct rk_resource_record rrs[NUMRRS];
    struct rk_resource_record *rr;
    struct rk_srv_record srvs[NUMRRS];
    size_t i, prio0;
    int fail = 0;

    (void) memset(&reply, 0, sizeof(reply));
    (void) memset(srvs, 0, sizeof(srvs));
    (void) memset(rrs, 0, sizeof(rrs));

    /* Test with two equal weight zero SRV records */
    rrs[0].type = rk_ns_t_srv;
    rrs[0].u.srv = &srvs[0];
    rrs[0].next = &rrs[1];
    srvs[0].priority = 10;
    srvs[0].weight = 0;

    rrs[1].type = rk_ns_t_srv;
    rrs[1].u.srv = &srvs[1];
    rrs[1].next = NULL;
    srvs[1].priority = 10;
    srvs[1].weight = 0;
    reply.head = &rrs[0];

    rk_dns_srv_order(&reply);
    assert(reply.head != NULL);
    printf("%p %p\n", &rrs[0], rrs[0].next);

    /*
     * Test four priority groups with priority 1--5 and weigths 0--3 in the
     * first two groups, and 1--4 in the last two groups.  Test multiple zero
     * weights, by further coercing the weight to zero if <= run/2.
     */
    for (i = 0; i < NUMRRS; i++) {
        rrs[i].type = rk_ns_t_srv;
        rrs[i].u.srv = &srvs[i];
        srvs[i].priority = 1 + i / 4;
        srvs[i].weight = i % 4 + i / 8;
        if (srvs[i].weight <= run/2)
            srvs[i].weight = 0;
    }
    /* Shuffle the RRs */
    for (i = 0; i < NUMRRS - 1; i++) {
        struct rk_resource_record tmp;
	size_t j = rk_random() % (NUMRRS - i);

        if (j > 0) {
            tmp = rrs[i+j];
            rrs[i+j] = rrs[i];
            rrs[i] = tmp;
        }
    }
    for (i = 0; i < NUMRRS; i++)
        rrs[i].next = &rrs[i + 1];
    rrs[i - 1].next = NULL;
    reply.head = &rrs[0];

    for (i = 0, rr = reply.head; i < NUMRRS; i++) {
        if (rr == NULL)
            break;
        printf("SRV RR order run %lu input: prio %lu weight %lu\n",
               (unsigned long)run, (unsigned long)rr->u.srv->priority,
               (unsigned long)rr->u.srv->weight);
        rr = rr->next;
    }

    rk_dns_srv_order(&reply);
    assert(reply.head != NULL);

    /*
     * After sorting, ensure monotone priority ordering with jumps by 1 at
     * group boundaries.
     */
    prio0 = 0;
    for (i = 0, rr = reply.head; i < NUMRRS; i++) {
        if (rr == NULL)
            break;
        if (rr->u.srv->priority < prio0 ||
	    (rr->u.srv->priority != prio0 &&
	     (i % 4 != 0 || rr->u.srv->priority > prio0 + 1))) {
            printf("SRV RR order run %lu failed\n", run);
            fail = 1;
        }
	prio0 = rr->u.srv->priority;
        printf("SRV RR order run %lu output: prio %lu weight %lu\n",
               (unsigned long)run, (unsigned long)rr->u.srv->priority,
               (unsigned long)rr->u.srv->weight);
        rr = rr->next;
    }
    assert(i == NUMRRS);

    return fail;
}

int
main(int argc, char **argv)
{
    struct rk_dns_reply *r;
    struct rk_resource_record *rr;
    int optidx = 0, i, exit_code = 0;

    setprogname (argv[0]);
    rk_random_init();

    if (getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if (version_flag) {
	printf("some version\n");
	exit(0);
    }

    argc -= optidx;
    argv += optidx;

    if (argc != 2 && argc != 0 && !srv_rr_order)
	usage(1);

    if (srv_rr_order) {
        exit_code += test_rk_dns_srv_order(0);
        exit_code += test_rk_dns_srv_order(1);
        exit_code += test_rk_dns_srv_order(2);
        exit_code += test_rk_dns_srv_order(3);
        exit_code += test_rk_dns_srv_order(4);
        exit_code += test_rk_dns_srv_order(5);
    }

    if (srv_rr_order && argc == 0)
        exit(exit_code ? 1 : 0);

    if (argc != 2)
        usage(1);

    for (i = 0; i < loop_integer; i++) {

	r = rk_dns_lookup(argv[0], argv[1]);
	if (r == NULL) {
	    printf("No reply.\n");
	    exit_code = 1;
	    break;
	}
	if (r->q.type == rk_ns_t_srv)
	    rk_dns_srv_order(r);

	for (rr = r->head; rr;rr=rr->next) {
	    printf("%-30s %-5s %-6d ", rr->domain, rk_dns_type_to_string(rr->type), rr->ttl);
	    switch (rr->type) {
	    case rk_ns_t_ns:
	    case rk_ns_t_cname:
	    case rk_ns_t_ptr:
		printf("%s\n", (char*)rr->u.data);
		break;
	    case rk_ns_t_a:
		printf("%s\n", inet_ntoa(*rr->u.a));
		break;
	    case rk_ns_t_mx:
	    case rk_ns_t_afsdb: {
		printf("%d %s\n", rr->u.mx->preference, rr->u.mx->domain);
		break;
	    }
	    case rk_ns_t_srv: {
		struct rk_srv_record *srv = rr->u.srv;
		printf("%d %d %d %s\n", srv->priority, srv->weight,
		       srv->port, srv->target);
		break;
	    }
	    case rk_ns_t_txt: {
		printf("%s\n", rr->u.txt);
		break;
	    }
	    case rk_ns_t_sig: {
		struct rk_sig_record *sig = rr->u.sig;
		const char *type_string = rk_dns_type_to_string (sig->type);

		printf("type %u (%s), algorithm %u, labels %u, orig_ttl %u, "
		       "sig_expiration %u, sig_inception %u, key_tag %u, "
		       "signer %s\n",
		       sig->type, type_string ? type_string : "",
		       sig->algorithm, sig->labels, sig->orig_ttl,
		       sig->sig_expiration, sig->sig_inception, sig->key_tag,
		       sig->signer);
		break;
	    }
	    case rk_ns_t_key: {
		struct rk_key_record *key = rr->u.key;

		printf("flags %u, protocol %u, algorithm %u\n",
		       key->flags, key->protocol, key->algorithm);
		break;
	    }
	    case rk_ns_t_sshfp: {
		struct rk_sshfp_record *sshfp = rr->u.sshfp;
		size_t j;

		printf ("alg %u type %u length %lu data ", sshfp->algorithm,
			sshfp->type,  (unsigned long)sshfp->sshfp_len);
		for (j = 0; j < sshfp->sshfp_len; j++)
		    printf("%02X", sshfp->sshfp_data[j]);
		printf("\n");

		break;
	    }
	    case rk_ns_t_ds: {
		struct rk_ds_record *ds = rr->u.ds;
		size_t j;

		printf("key tag %u alg %u type %u length %lu data ",
		       ds->key_tag, ds->algorithm, ds->digest_type,
		       (unsigned long)ds->digest_len);
		for (j = 0; j < ds->digest_len; j++)
		    printf("%02X", ds->digest_data[j]);
		printf("\n");

		break;
	    }
	    default:
		printf("\n");
		break;
	    }
	}
	rk_dns_free_data(r);
    }

    return exit_code ? 1 : 0;
}
