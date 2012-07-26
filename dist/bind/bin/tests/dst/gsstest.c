/*
 * Copyright (C) 2006, 2007, 2009-2011  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: gsstest.c,v 1.14.12.2 2011-03-28 05:14:18 marka Exp $ */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <isc/app.h>
#include <isc/base64.h>
#include <isc/entropy.h>
#include <isc/log.h>
#include <isc/mem.h>
#include <isc/sockaddr.h>
#include <isc/socket.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/dispatch.h>
#include <dns/fixedname.h>
#include <dns/keyvalues.h>
#include <dns/log.h>
#include <dns/message.h>
#include <dns/name.h>
#include <dns/request.h>
#include <dns/result.h>
#include <dns/tkey.h>
#include <dns/tsig.h>
#include <dns/view.h>

#include <dns/dnssec.h>
#include <dns/events.h>
#include <dns/masterdump.h>
#include <dns/rdataset.h>
#include <dns/resolver.h>
#include <dns/types.h>

#include <dst/result.h>

#ifdef GSSAPI
#include ISC_PLATFORM_GSSAPIHEADER

struct dst_context {
	unsigned int magic;
	dst_key_t *key;
	isc_mem_t *mctx;
	void *opaque;
};

#define CHECK(str, x) { \
	if ((x) != ISC_R_SUCCESS) { \
		fprintf(stderr, "I:%d:%s: %s\n", __LINE__, (str), isc_result_totext(x)); \
		goto end; \
	} \
}

static char contextname[512];
static char gssid[512];
static char serveraddress[512];
static dns_fixedname_t servername, gssname;

static isc_mem_t *mctx;
static dns_requestmgr_t *requestmgr;
static isc_sockaddr_t address;

static dns_tsig_keyring_t *ring;
static dns_tsigkey_t *tsigkey = NULL;
static gss_ctx_id_t gssctx;
static gss_ctx_id_t *gssctxp = &gssctx;

#define RUNCHECK(x) RUNTIME_CHECK((x) == ISC_R_SUCCESS)

#define PORT 53
#define TIMEOUT 30

static void initctx1(isc_task_t *task, isc_event_t *event);
static void sendquery(isc_task_t *task, isc_event_t *event);
static void setup();

static void
console(isc_task_t *task, isc_event_t *event)
{
	char buf[32];
	int c;

	isc_event_t *ev = NULL;

	isc_event_free(&event);

	for (;;) {
		printf("\nCommand => ");
		c = scanf("%s", buf);

		if (c == EOF || strcmp(buf, "quit") == 0) {
			isc_app_shutdown();
			return;
		}

		if (strcmp(buf, "initctx") == 0) {
			ev = isc_event_allocate(mctx, (void *)1, 1, initctx1,
						NULL, sizeof(*event));
			isc_task_send(task, &ev);
			return;
		}

		if (strcmp(buf, "query") == 0) {
			ev = isc_event_allocate(mctx, (void *)1, 1, sendquery,
						NULL, sizeof(*event));
			isc_task_send(task, &ev);
			return;
		}

		printf("Unknown command\n");
	}
}

static void
recvresponse(isc_task_t *task, isc_event_t *event) {
	dns_requestevent_t *reqev = (dns_requestevent_t *)event;
	isc_result_t result, result2;
	dns_message_t *query, *response = NULL;
	isc_buffer_t outtoken;
	isc_buffer_t outbuf;
	char output[10 * 1024];

	unsigned char array[DNS_NAME_MAXTEXT + 1];
	isc_buffer_init(&outtoken, array, sizeof(array));

	UNUSED(task);

	REQUIRE(reqev != NULL);

	if (reqev->result != ISC_R_SUCCESS) {
		fprintf(stderr, "I:request event result: %s\n",
			isc_result_totext(reqev->result));
		goto end;
	}

	query = reqev->ev_arg;

	response = NULL;
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE, &response);
	CHECK("dns_message_create", result);

	printf("\nReceived Response:\n");

	result2 = dns_request_getresponse(reqev->request, response,
					 DNS_MESSAGEPARSE_PRESERVEORDER);
	isc_buffer_init(&outbuf, output, sizeof(output));
	result = dns_message_totext(response, &dns_master_style_debug, 0,
				    &outbuf);
	CHECK("dns_message_totext", result);
	printf("%.*s\n", (int)isc_buffer_usedlength(&outbuf),
	       (char *)isc_buffer_base(&outbuf));

	CHECK("dns_request_getresponse", result2);

	if (response)
		dns_message_destroy(&response);

end:
	if (query)
		dns_message_destroy(&query);

	if (reqev->request)
		dns_request_destroy(&reqev->request);

	isc_event_free(&event);

	event = isc_event_allocate(mctx, (void *)1, 1, console, NULL,
				   sizeof(*event));
	isc_task_send(task, &event);
	return;
}


static void
sendquery(isc_task_t *task, isc_event_t *event)
{
	dns_request_t *request = NULL;
	dns_message_t *message = NULL;
	dns_name_t *qname = NULL;
	dns_rdataset_t *qrdataset = NULL;
	isc_result_t result;
	dns_fixedname_t queryname;
	isc_buffer_t buf;
	isc_buffer_t outbuf;
	char output[10 * 1024];
	static char host[256];
	int c;

	isc_event_free(&event);

	printf("Query => ");
	c = scanf("%s", host);
	if (c == EOF)
		return;

	dns_fixedname_init(&queryname);
	isc_buffer_init(&buf, host, strlen(host));
	isc_buffer_add(&buf, strlen(host));
	result = dns_name_fromtext(dns_fixedname_name(&queryname), &buf,
				   dns_rootname, 0, NULL);
	CHECK("dns_name_fromtext", result);

	result = dns_message_create(mctx, DNS_MESSAGE_INTENTRENDER, &message);

	message->opcode = dns_opcode_query;
	message->rdclass = dns_rdataclass_in;
	message->id = (unsigned short)(random() & 0xFFFF);

	result = dns_message_gettempname(message, &qname);
	if (result != ISC_R_SUCCESS)
		goto end;

	result = dns_message_gettemprdataset(message, &qrdataset);
	if (result != ISC_R_SUCCESS)
		goto end;

	dns_name_init(qname, NULL);
	dns_name_clone(dns_fixedname_name(&queryname), qname);
	dns_rdataset_init(qrdataset);
	dns_rdataset_makequestion(qrdataset, dns_rdataclass_in,
				  dns_rdatatype_a);
	ISC_LIST_APPEND(qname->list, qrdataset, link);
	dns_message_addname(message, qname, DNS_SECTION_QUESTION);

	result = dns_request_create(requestmgr, message, &address, 0, tsigkey,
				    TIMEOUT, task, recvresponse,
		message, &request);
	CHECK("dns_request_create", result);

	printf("Submitting query:\n");
	isc_buffer_init(&outbuf, output, sizeof(output));
	result = dns_message_totext(message, &dns_master_style_debug, 0,
				    &outbuf);
	CHECK("dns_message_totext", result);
	printf("%.*s\n", (int)isc_buffer_usedlength(&outbuf),
	       (char *)isc_buffer_base(&outbuf));

	return;

	end:
		if (qname != NULL)
			dns_message_puttempname(message, &qname);
		if (qrdataset != NULL)
			dns_message_puttemprdataset(message, &qrdataset);
		if (message != NULL)
			dns_message_destroy(&message);
}

static void
initctx2(isc_task_t *task, isc_event_t *event) {
	dns_requestevent_t *reqev = (dns_requestevent_t *)event;
	isc_result_t result;
	dns_message_t *query, *response = NULL;
	isc_buffer_t outtoken;
	unsigned char array[DNS_NAME_MAXTEXT + 1];
	dns_rdataset_t *rdataset;
	dns_rdatatype_t qtype;
	dns_name_t *question_name;

	UNUSED(task);

	REQUIRE(reqev != NULL);

	if (reqev->result != ISC_R_SUCCESS) {
		fprintf(stderr, "I:request event result: %s\n",
			isc_result_totext(reqev->result));
		goto end;
	}

	query = reqev->ev_arg;

	response = NULL;
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE, &response);
	CHECK("dns_message_create", result);

	result = dns_request_getresponse(reqev->request, response,
					 DNS_MESSAGEPARSE_PRESERVEORDER);
	CHECK("dns_request_getresponse", result);

	if (response->rcode != dns_rcode_noerror) {
		result = ISC_RESULTCLASS_DNSRCODE + response->rcode;
		fprintf(stderr, "I:response rcode: %s\n",
			isc_result_totext(result));
		goto end;
	}

	printf("Received token from server, calling gss_init_sec_context()\n");
	isc_buffer_init(&outtoken, array, DNS_NAME_MAXTEXT + 1);
	result = dns_tkey_processgssresponse(query, response,
					     dns_fixedname_name(&gssname),
					     &gssctx, &outtoken,
					     &tsigkey, ring, NULL);
	gssctx = *gssctxp;
	CHECK("dns_tkey_processgssresponse", result);
	printf("Context accepted\n");

	question_name = NULL;
	dns_message_currentname(response, DNS_SECTION_ANSWER, &question_name);
	rdataset = ISC_LIST_HEAD(question_name->list);
	INSIST(rdataset != NULL);
	qtype = rdataset->type;
	if (qtype == dns_rdatatype_tkey) {
		printf("Received TKEY response from server\n");
		printf("Context completed\n");
	} else {
		printf("Did not receive TKEY response from server\n");
		printf("Context not completed\n");
		dns_tsigkey_detach(&tsigkey);
		tsigkey = NULL;
	}

	if (response)
		dns_message_destroy(&response);

end:
	if (query)
		dns_message_destroy(&query);

	if (reqev->request)
		dns_request_destroy(&reqev->request);

	isc_event_free(&event);

	event = isc_event_allocate(mctx, (void *)1, 1, console, NULL,
				   sizeof(*event));
	isc_task_send(task, &event);
	return;
}

static void
initctx1(isc_task_t *task, isc_event_t *event) {
	isc_result_t result;
	isc_buffer_t buf;
	dns_message_t *query;
	dns_request_t *request;
	int c;

	isc_event_free(&event);

	printf("Initctx - GSS name => ");
	c = scanf("%s", gssid);
	if (c == EOF)
		return;

	sprintf(contextname, "gsstest.context.%d.", (int)time(NULL));

	printf("Initctx - context name we're using: %s\n", contextname);

	printf("Negotiating GSSAPI context: ");
	printf("%s", gssid);
	printf("\n");

	/*
	 * Setup a GSSAPI context with the server
	 */
	dns_fixedname_init(&servername);
	isc_buffer_init(&buf, contextname, strlen(contextname));
	isc_buffer_add(&buf, strlen(contextname));
	result = dns_name_fromtext(dns_fixedname_name(&servername), &buf,
				   dns_rootname, 0, NULL);
	CHECK("dns_name_fromtext", result);

	/* Make name happen */
	dns_fixedname_init(&gssname);
	isc_buffer_init(&buf, gssid, strlen(gssid));
	isc_buffer_add(&buf, strlen(gssid));
	result = dns_name_fromtext(dns_fixedname_name(&gssname), &buf,
				   dns_rootname, 0, NULL);
	CHECK("dns_name_fromtext", result);

	query = NULL;
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTRENDER, &query);
	CHECK("dns_message_create", result);

	printf("Calling gss_init_sec_context()\n");
	gssctx = GSS_C_NO_CONTEXT;
	result = dns_tkey_buildgssquery(query, dns_fixedname_name(&servername),
					dns_fixedname_name(&gssname),
					NULL, 36000, &gssctx, ISC_TRUE,
					mctx, NULL);
	CHECK("dns_tkey_buildgssquery", result);

	printf("Sending context token to server\n");
	request = NULL;
	result = dns_request_create(requestmgr, query, &address, 0, NULL,
				    TIMEOUT, task, initctx2, query, &request);
	CHECK("dns_request_create", result);

	return;
end:
	event = isc_event_allocate(mctx, (void *)1, 1, console, NULL,
				   sizeof(*event));
	isc_task_send(task, &event);return;
}

static void
setup(void)
{
	struct in_addr inaddr;
	int c;

	for (;;) {
		printf("Server IP => ");
		c = scanf("%s", serveraddress);

		if (c == EOF || strcmp(serveraddress, "quit") == 0) {
			isc_app_shutdown();
			return;
		}

		if (inet_pton(AF_INET, serveraddress, &inaddr) == 1) {
			isc_sockaddr_fromin(&address, &inaddr, PORT);
			return;
		}

	}
}

int
main(int argc, char *argv[]) {
	isc_taskmgr_t *taskmgr;
	isc_timermgr_t *timermgr;
	isc_socketmgr_t *socketmgr;
	isc_socket_t *sock;
	unsigned int attrs, attrmask;
	isc_sockaddr_t bind_any;
	dns_dispatchmgr_t *dispatchmgr;
	dns_dispatch_t *dispatchv4;
	dns_view_t *view;
	isc_entropy_t *ectx;
	isc_task_t *task;
	isc_log_t *lctx = NULL;
	isc_logconfig_t *lcfg = NULL;
	isc_logdestination_t destination;

	UNUSED(argv);
	UNUSED(argc);

	RUNCHECK(isc_app_start());

	dns_result_register();

	mctx = NULL;
	RUNCHECK(isc_mem_create(0, 0, &mctx));

	RUNCHECK(isc_log_create(mctx, &lctx, &lcfg));
	isc_log_setcontext(lctx);
	dns_log_init(lctx);
	dns_log_setcontext(lctx);

	/*
	 * Create and install the default channel.
	 */
	destination.file.stream = stderr;
	destination.file.name = NULL;
	destination.file.versions = ISC_LOG_ROLLNEVER;
	destination.file.maximum_size = 0;
	RUNCHECK(isc_log_createchannel(lcfg, "_default",
				       ISC_LOG_TOFILEDESC,
				       ISC_LOG_DYNAMIC,
				       &destination, ISC_LOG_PRINTTIME));
	RUNCHECK(isc_log_usechannel(lcfg, "_default", NULL, NULL));

	isc_log_setdebuglevel(lctx, 9);

	ectx = NULL;
	RUNCHECK(isc_entropy_create(mctx, &ectx));
	RUNCHECK(isc_entropy_createfilesource(ectx, "/dev/urandom"));

	RUNCHECK(dst_lib_init(mctx, ectx, ISC_ENTROPY_GOODONLY));

	taskmgr = NULL;
	RUNCHECK(isc_taskmgr_create(mctx, 1, 0, &taskmgr));
	task = NULL;
	RUNCHECK(isc_task_create(taskmgr, 0, &task));
	timermgr = NULL;
	RUNCHECK(isc_timermgr_create(mctx, &timermgr));
	socketmgr = NULL;
	RUNCHECK(isc_socketmgr_create(mctx, &socketmgr));
	dispatchmgr = NULL;
	RUNCHECK(dns_dispatchmgr_create(mctx, ectx, &dispatchmgr));
	isc_sockaddr_any(&bind_any);
	attrs = DNS_DISPATCHATTR_UDP |
		DNS_DISPATCHATTR_MAKEQUERY |
		DNS_DISPATCHATTR_IPV4;
	attrmask = DNS_DISPATCHATTR_UDP |
		   DNS_DISPATCHATTR_TCP |
		   DNS_DISPATCHATTR_IPV4 |
		   DNS_DISPATCHATTR_IPV6;
	dispatchv4 = NULL;
	RUNCHECK(dns_dispatch_getudp(dispatchmgr, socketmgr, taskmgr,
					  &bind_any, 4096, 4, 2, 3, 5,
					  attrs, attrmask, &dispatchv4));
	requestmgr = NULL;
	RUNCHECK(dns_requestmgr_create(mctx, timermgr, socketmgr, taskmgr,
					    dispatchmgr, dispatchv4, NULL,
					    &requestmgr));

	ring = NULL;
	RUNCHECK(dns_tsigkeyring_create(mctx, &ring));

	view = NULL;
	RUNCHECK(dns_view_create(mctx, 0, "_test", &view));
	dns_view_setkeyring(view, ring);

	sock = NULL;
	RUNCHECK(isc_socket_create(socketmgr, PF_INET, isc_sockettype_udp,
				   &sock));

	setup();

	RUNCHECK(isc_app_onrun(mctx, task, console, NULL));

	(void)isc_app_run();

	if (tsigkey)
		dns_tsigkey_detach(&tsigkey);

	dns_requestmgr_shutdown(requestmgr);
	dns_requestmgr_detach(&requestmgr);

	dns_dispatch_detach(&dispatchv4);
	dns_dispatchmgr_destroy(&dispatchmgr);

	isc_timermgr_destroy(&timermgr);

	isc_task_detach(&task);
	isc_taskmgr_destroy(&taskmgr);

	isc_socket_detach(&sock);
	isc_socketmgr_destroy(&socketmgr);

	isc_mem_stats(mctx, stdout);

	dns_view_detach(&view);

	dst_lib_destroy();
	isc_entropy_detach(&ectx);

	isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	isc_app_finish();

	return (0);
}
#else
int
main(int argc, char *argv[]) {
	UNUSED(argc);
	UNUSED(argv);
	fprintf(stderr, "R:GSSAPIONLY\n");
	return (0);
}
#endif
