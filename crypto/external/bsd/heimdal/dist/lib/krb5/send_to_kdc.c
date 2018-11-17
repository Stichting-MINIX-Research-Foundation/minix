/*	$NetBSD: send_to_kdc.c,v 1.7 2017/01/30 00:25:15 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 - 2013 Apple Inc. All rights reserved.
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

#include "krb5_locl.h"
#include "send_to_kdc_plugin.h"

/**
 * @section send_to_kdc Locating and sending packets to the KDC
 *
 * The send to kdc code is responsible to request the list of KDC from
 * the locate-kdc subsystem and then send requests to each of them.
 *
 * - Each second a new hostname is tried.
 * - If the hostname have several addresses, the first will be tried
 *   directly then in turn the other will be tried every 3 seconds
 *   (host_timeout).
 * - UDP requests are tried 3 times, and it tried with a individual timeout of kdc_timeout / 3.
 * - TCP and HTTP requests are tried 1 time.
 *
 *  Total wait time shorter then (number of addresses * 3) + kdc_timeout seconds.
 *
 */

static int
init_port(const char *s, int fallback)
{
    int tmp;

    if (s && sscanf(s, "%d", &tmp) == 1)
        return htons(tmp);
    return fallback;
}

struct send_via_plugin_s {
    krb5_const_realm realm;
    krb5_krbhst_info *hi;
    time_t timeout;
    const krb5_data *send_data;
    krb5_data *receive;
};
    

static krb5_error_code KRB5_LIB_CALL
kdccallback(krb5_context context, const void *plug, void *plugctx, void *userctx)
{
    const krb5plugin_send_to_kdc_ftable *service = (const krb5plugin_send_to_kdc_ftable *)plug;
    struct send_via_plugin_s *ctx = userctx;

    if (service->send_to_kdc == NULL)
	return KRB5_PLUGIN_NO_HANDLE;
    return service->send_to_kdc(context, plugctx, ctx->hi, ctx->timeout,
				ctx->send_data, ctx->receive);
}

static krb5_error_code KRB5_LIB_CALL
realmcallback(krb5_context context, const void *plug, void *plugctx, void *userctx)
{
    const krb5plugin_send_to_kdc_ftable *service = (const krb5plugin_send_to_kdc_ftable *)plug;
    struct send_via_plugin_s *ctx = userctx;

    if (service->send_to_realm == NULL)
	return KRB5_PLUGIN_NO_HANDLE;
    return service->send_to_realm(context, plugctx, ctx->realm, ctx->timeout,
				  ctx->send_data, ctx->receive);
}

static krb5_error_code
kdc_via_plugin(krb5_context context,
	       krb5_krbhst_info *hi,
	       time_t timeout,
	       const krb5_data *send_data,
	       krb5_data *receive)
{
    struct send_via_plugin_s userctx;

    userctx.realm = NULL;
    userctx.hi = hi;
    userctx.timeout = timeout;
    userctx.send_data = send_data;
    userctx.receive = receive;

    return _krb5_plugin_run_f(context, "krb5", KRB5_PLUGIN_SEND_TO_KDC,
			      KRB5_PLUGIN_SEND_TO_KDC_VERSION_0, 0,
			      &userctx, kdccallback);
}

static krb5_error_code
realm_via_plugin(krb5_context context,
		 krb5_const_realm realm,
		 time_t timeout,
		 const krb5_data *send_data,
		 krb5_data *receive)
{
    struct send_via_plugin_s userctx;

    userctx.realm = realm;
    userctx.hi = NULL;
    userctx.timeout = timeout;
    userctx.send_data = send_data;
    userctx.receive = receive;

    return _krb5_plugin_run_f(context, "krb5", KRB5_PLUGIN_SEND_TO_KDC,
			      KRB5_PLUGIN_SEND_TO_KDC_VERSION_2, 0,
			      &userctx, realmcallback);
}

struct krb5_sendto_ctx_data {
    int flags;
    int type;
    krb5_sendto_ctx_func func;
    void *data;
    char *hostname;
    krb5_krbhst_handle krbhst;

    /* context2 */
    const krb5_data *send_data;
    krb5_data response;
    heim_array_t hosts;
    int stateflags;
#define KRBHST_COMPLETED	1

    /* prexmit */
    krb5_sendto_prexmit prexmit_func;
    void *prexmit_ctx;

    /* stats */
    struct {
	struct timeval start_time;
	struct timeval name_resolution;
	struct timeval krbhst;
	unsigned long sent_packets;
	unsigned long num_hosts;
    } stats;
    unsigned int stid;
};

static void
dealloc_sendto_ctx(void *ptr)
{
    krb5_sendto_ctx ctx = (krb5_sendto_ctx)ptr;
    if (ctx->hostname)
	free(ctx->hostname);
    heim_release(ctx->hosts);
    heim_release(ctx->krbhst);
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_sendto_ctx_alloc(krb5_context context, krb5_sendto_ctx *ctx)
{
    *ctx = heim_alloc(sizeof(**ctx), "sendto-context", dealloc_sendto_ctx);
    if (*ctx == NULL)
	return krb5_enomem(context);
    (*ctx)->hosts = heim_array_create();

    return 0;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_sendto_ctx_add_flags(krb5_sendto_ctx ctx, int flags)
{
    ctx->flags |= flags;
}

KRB5_LIB_FUNCTION int KRB5_LIB_CALL
krb5_sendto_ctx_get_flags(krb5_sendto_ctx ctx)
{
    return ctx->flags;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_sendto_ctx_set_type(krb5_sendto_ctx ctx, int type)
{
    ctx->type = type;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_sendto_ctx_set_func(krb5_sendto_ctx ctx,
			 krb5_sendto_ctx_func func,
			 void *data)
{
    ctx->func = func;
    ctx->data = data;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_sendto_ctx_set_prexmit(krb5_sendto_ctx ctx,
			     krb5_sendto_prexmit prexmit,
			     void *data)
{
    ctx->prexmit_func = prexmit;
    ctx->prexmit_ctx = data;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_sendto_set_hostname(krb5_context context,
			 krb5_sendto_ctx ctx,
			 const char *hostname)
{
    if (ctx->hostname == NULL)
	free(ctx->hostname);
    ctx->hostname = strdup(hostname);
    if (ctx->hostname == NULL) {
	krb5_set_error_message(context, ENOMEM, N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    return 0;
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
_krb5_sendto_ctx_set_krb5hst(krb5_context context,
			     krb5_sendto_ctx ctx,
			     krb5_krbhst_handle handle)
{
    heim_release(ctx->krbhst);
    ctx->krbhst = heim_retain(handle);
}

KRB5_LIB_FUNCTION void KRB5_LIB_CALL
krb5_sendto_ctx_free(krb5_context context, krb5_sendto_ctx ctx)
{
    heim_release(ctx);
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
_krb5_kdc_retry(krb5_context context, krb5_sendto_ctx ctx, void *data,
		const krb5_data *reply, int *action)
{
    krb5_error_code ret;
    KRB_ERROR error;

    if(krb5_rd_error(context, reply, &error))
	return 0;

    ret = krb5_error_from_rd_error(context, &error, NULL);
    krb5_free_error_contents(context, &error);

    switch(ret) {
    case KRB5KRB_ERR_RESPONSE_TOO_BIG: {
	if (krb5_sendto_ctx_get_flags(ctx) & KRB5_KRBHST_FLAGS_LARGE_MSG)
	    break;
	krb5_sendto_ctx_add_flags(ctx, KRB5_KRBHST_FLAGS_LARGE_MSG);
	*action = KRB5_SENDTO_RESET;
	break;
    }
    case KRB5KDC_ERR_SVC_UNAVAILABLE:
	*action = KRB5_SENDTO_CONTINUE;
	break;
    }
    return 0;
}

/*
 *
 */

struct host;

struct host_fun {
    krb5_error_code (*prepare)(krb5_context, struct host *, const krb5_data *);
    krb5_error_code (*send_fn)(krb5_context, struct host *);
    krb5_error_code (*recv_fn)(krb5_context, struct host *, krb5_data *);
    int ntries;
};

struct host {
    enum host_state { CONNECT, CONNECTING, CONNECTED, WAITING_REPLY, DEAD } state;
    krb5_krbhst_info *hi;
    struct addrinfo *ai;
    rk_socket_t fd;
    struct host_fun *fun;
    unsigned int tries;
    time_t timeout;
    krb5_data data;
    unsigned int tid;
};

static void
debug_host(krb5_context context, int level, struct host *host, const char *fmt, ...)
	__attribute__ ((__format__ (__printf__, 4, 5)));

static void
debug_host(krb5_context context, int level, struct host *host, const char *fmt, ...)
{
    const char *proto = "unknown";
    char name[NI_MAXHOST], port[NI_MAXSERV];
    char *text = NULL;
    va_list ap;
    int ret;

    if (!_krb5_have_debug(context, 5))
	return;

    va_start(ap, fmt);
    ret = vasprintf(&text, fmt, ap);
    va_end(ap);
    if (ret == -1 || text == NULL)
	return;

    if (host->hi->proto == KRB5_KRBHST_HTTP)
	proto = "http";
    else if (host->hi->proto == KRB5_KRBHST_TCP)
	proto = "tcp";
    else if (host->hi->proto == KRB5_KRBHST_UDP)
	proto = "udp";

    if (getnameinfo(host->ai->ai_addr, host->ai->ai_addrlen,
		    name, sizeof(name), port, sizeof(port), NI_NUMERICHOST) != 0)
	name[0] = '\0';

    _krb5_debug(context, level, "%s: %s %s:%s (%s) tid: %08x", text,
		proto, name, port, host->hi->hostname, host->tid);
    free(text);
}


static void
deallocate_host(void *ptr)
{
    struct host *host = ptr;
    if (!rk_IS_BAD_SOCKET(host->fd))
	rk_closesocket(host->fd);
    krb5_data_free(&host->data);
    host->ai = NULL;
}

static void
host_dead(krb5_context context, struct host *host, const char *msg)
{
    debug_host(context, 5, host, "%s", msg);
    rk_closesocket(host->fd);
    host->fd = rk_INVALID_SOCKET;
    host->state = DEAD;
}

static krb5_error_code
send_stream(krb5_context context, struct host *host)
{
    ssize_t len;

    len = krb5_net_write(context, &host->fd, host->data.data, host->data.length);

    if (len < 0)
	return errno;
    else if (len < host->data.length) {
	host->data.length -= len;
	memmove(host->data.data, ((uint8_t *)host->data.data) + len, host->data.length - len);
	return -1;
    } else {
	krb5_data_free(&host->data);
	return 0;
    }
}

static krb5_error_code
recv_stream(krb5_context context, struct host *host)
{
    krb5_error_code ret;
    size_t oldlen;
    ssize_t sret;
    int nbytes;

    if (rk_SOCK_IOCTL(host->fd, FIONREAD, &nbytes) != 0 || nbytes <= 0)
	return HEIM_NET_CONN_REFUSED;

    if (context->max_msg_size - host->data.length < nbytes) {
	krb5_set_error_message(context, KRB5KRB_ERR_FIELD_TOOLONG,
			       N_("TCP message from KDC too large %d", ""),
			       (int)(host->data.length + nbytes));
	return KRB5KRB_ERR_FIELD_TOOLONG;
    }

    oldlen = host->data.length;

    ret = krb5_data_realloc(&host->data, oldlen + nbytes + 1 /* NUL */);
    if (ret)
	return ret;

    sret = krb5_net_read(context, &host->fd, ((uint8_t *)host->data.data) + oldlen, nbytes);
    if (sret <= 0) {
	ret = errno;
	return ret;
    }
    host->data.length = oldlen + sret;
    /* zero terminate for http transport */
    ((uint8_t *)host->data.data)[host->data.length] = '\0';

    return 0;
}

/*
 *
 */

static void
host_next_timeout(krb5_context context, struct host *host)
{
    host->timeout = context->kdc_timeout / host->fun->ntries;
    if (host->timeout == 0)
	host->timeout = 1;

    host->timeout += time(NULL);
}

/*
 * connected host
 */

static void
host_connected(krb5_context context, krb5_sendto_ctx ctx, struct host *host)
{
    krb5_error_code ret;

    host->state = CONNECTED; 
    /*
     * Now prepare data to send to host
     */
    if (ctx->prexmit_func) {
	krb5_data data;
	    
	krb5_data_zero(&data);

	ret = ctx->prexmit_func(context, host->hi->proto,
				ctx->prexmit_ctx, host->fd, &data);
	if (ret == 0) {
	    if (data.length == 0) {
		host_dead(context, host, "prexmit function didn't send data");
		return;
	    }
	    ret = host->fun->prepare(context, host, &data);
	    krb5_data_free(&data);
	}
	
    } else {
	ret = host->fun->prepare(context, host, ctx->send_data);
    }
    if (ret)
	debug_host(context, 5, host, "failed to prexmit/prepare");
}

/*
 * connect host
 */

static void
host_connect(krb5_context context, krb5_sendto_ctx ctx, struct host *host)
{
    krb5_krbhst_info *hi = host->hi;
    struct addrinfo *ai = host->ai;

    debug_host(context, 5, host, "connecting to host");

    if (connect(host->fd, ai->ai_addr, ai->ai_addrlen) < 0) {
#ifdef HAVE_WINSOCK
	if (WSAGetLastError() == WSAEWOULDBLOCK)
	    errno = EINPROGRESS;
#endif /* HAVE_WINSOCK */
	if (errno == EINPROGRESS && (hi->proto == KRB5_KRBHST_HTTP || hi->proto == KRB5_KRBHST_TCP)) {
	    debug_host(context, 5, host, "connecting to %d", host->fd);
	    host->state = CONNECTING;
	} else {
	    host_dead(context, host, "failed to connect");
	}
    } else {
	host_connected(context, ctx, host);
    }

    host_next_timeout(context, host);
}

/*
 * HTTP transport
 */

static krb5_error_code
prepare_http(krb5_context context, struct host *host, const krb5_data *data)
{
    char *str = NULL, *request = NULL;
    krb5_error_code ret;
    int len;

    heim_assert(host->data.length == 0, "prepare_http called twice");

    len = rk_base64_encode(data->data, data->length, &str);
    if(len < 0)
	return ENOMEM;

    if (context->http_proxy)
	ret = asprintf(&request, "GET http://%s/%s HTTP/1.0\r\n\r\n", host->hi->hostname, str);
    else
	ret = asprintf(&request, "GET /%s HTTP/1.0\r\n\r\n", str);
    free(str);
    if(ret < 0 || request == NULL)
	return ENOMEM;
    
    host->data.data = request;
    host->data.length = strlen(request);

    return 0;
}

static krb5_error_code
recv_http(krb5_context context, struct host *host, krb5_data *data)
{
    krb5_error_code ret;
    unsigned long rep_len;
    size_t len;
    char *p;

    /*
     * recv_stream returns a NUL terminated stream
     */

    ret = recv_stream(context, host);
    if (ret)
	return ret;

    p = strstr(host->data.data, "\r\n\r\n");
    if (p == NULL)
	return -1;
    p += 4;

    len = host->data.length - (p - (char *)host->data.data);
    if (len < 4)
	return -1;

    _krb5_get_int(p, &rep_len, 4);
    if (len < rep_len)
	return -1;

    p += 4;

    memmove(host->data.data, p, rep_len);
    host->data.length = rep_len;

    *data = host->data;
    krb5_data_zero(&host->data);

    return 0;
}

/*
 * TCP transport
 */

static krb5_error_code
prepare_tcp(krb5_context context, struct host *host, const krb5_data *data)
{
    krb5_error_code ret;
    krb5_storage *sp;

    heim_assert(host->data.length == 0, "prepare_tcp called twice");

    sp = krb5_storage_emem();
    if (sp == NULL)
	return ENOMEM;
    
    ret = krb5_store_data(sp, *data);
    if (ret) {
	krb5_storage_free(sp);
	return ret;
    }
    ret = krb5_storage_to_data(sp, &host->data);
    krb5_storage_free(sp);

    return ret;
}

static krb5_error_code
recv_tcp(krb5_context context, struct host *host, krb5_data *data)
{
    krb5_error_code ret;
    unsigned long pktlen;

    ret = recv_stream(context, host);
    if (ret)
	return ret;

    if (host->data.length < 4)
	return -1;

    _krb5_get_int(host->data.data, &pktlen, 4);
    
    if (pktlen > host->data.length - 4)
	return -1;

    memmove(host->data.data, ((uint8_t *)host->data.data) + 4, host->data.length - 4);
    host->data.length -= 4;

    *data = host->data;
    krb5_data_zero(&host->data);
    
    return 0;
}

/*
 * UDP transport
 */

static krb5_error_code
prepare_udp(krb5_context context, struct host *host, const krb5_data *data)
{
    return krb5_data_copy(&host->data, data->data, data->length);
}

static krb5_error_code
send_udp(krb5_context context, struct host *host)
{
    if (send(host->fd, host->data.data, host->data.length, 0) < 0)
	return errno;
    return 0;
}

static krb5_error_code
recv_udp(krb5_context context, struct host *host, krb5_data *data)
{
    krb5_error_code ret;
    int nbytes;


    if (rk_SOCK_IOCTL(host->fd, FIONREAD, &nbytes) != 0 || nbytes <= 0)
	return HEIM_NET_CONN_REFUSED;

    if (context->max_msg_size < nbytes) {
	krb5_set_error_message(context, KRB5KRB_ERR_FIELD_TOOLONG,
			       N_("UDP message from KDC too large %d", ""),
			       (int)nbytes);
	return KRB5KRB_ERR_FIELD_TOOLONG;
    }

    ret = krb5_data_alloc(data, nbytes);
    if (ret)
	return ret;

    ret = recv(host->fd, data->data, data->length, 0);
    if (ret < 0) {
	ret = errno;
	krb5_data_free(data);
	return ret;
    }
    data->length = ret;

    return 0;
}

static struct host_fun http_fun = {
    prepare_http,
    send_stream,
    recv_http,
    1
};
static struct host_fun tcp_fun = {
    prepare_tcp,
    send_stream,
    recv_tcp,
    1
};
static struct host_fun udp_fun = {
    prepare_udp,
    send_udp,
    recv_udp,
    3
};


/*
 * Host state machine
 */

static int
eval_host_state(krb5_context context,
		krb5_sendto_ctx ctx,
		struct host *host,
		int readable, int writeable)
{
    krb5_error_code ret;

    if (host->state == CONNECT) {
	/* check if its this host time to connect */
	if (host->timeout < time(NULL))
	    host_connect(context, ctx, host);
	return 0;
    }

    if (host->state == CONNECTING && writeable)
	host_connected(context, ctx, host);

    if (readable) {

	debug_host(context, 5, host, "reading packet");

	ret = host->fun->recv_fn(context, host, &ctx->response);
	if (ret == -1) {
	    /* not done yet */
	} else if (ret == 0) {
	    /* if recv_foo function returns 0, we have a complete reply */
	    debug_host(context, 5, host, "host completed");
	    return 1;
	} else {
	    host_dead(context, host, "host disconnected");
	}
    }

    /* check if there is anything to send, state might DEAD after read */
    if (writeable && host->state == CONNECTED) {

	ctx->stats.sent_packets++;

	debug_host(context, 5, host, "writing packet");

	ret = host->fun->send_fn(context, host);
	if (ret == -1) {
	    /* not done yet */
	} else if (ret) {
	    host_dead(context, host, "host dead, write failed");
	} else
	    host->state = WAITING_REPLY;
    }

    return 0;
}

/*
 *
 */

static krb5_error_code
submit_request(krb5_context context, krb5_sendto_ctx ctx, krb5_krbhst_info *hi)
{
    unsigned long submitted_host = 0;
    krb5_boolean freeai = FALSE;
    struct timeval nrstart, nrstop;
    krb5_error_code ret;
    struct addrinfo *ai = NULL, *a;
    struct host *host;

    ret = kdc_via_plugin(context, hi, context->kdc_timeout,
			 ctx->send_data, &ctx->response);
    if (ret == 0) {
	return 0;
    } else if (ret != KRB5_PLUGIN_NO_HANDLE) {
	_krb5_debug(context, 5, "send via plugin failed %s: %d",
		    hi->hostname, ret);
	return ret;
    }

    /*
     * If we have a proxy, let use the address of the proxy instead of
     * the KDC and let the proxy deal with the resolving of the KDC.
     */

    gettimeofday(&nrstart, NULL);

    if (hi->proto == KRB5_KRBHST_HTTP && context->http_proxy) {
	char *proxy2 = strdup(context->http_proxy);
	char *el, *proxy  = proxy2;
	struct addrinfo hints;
	char portstr[NI_MAXSERV];
	unsigned short nport;
	
	if (proxy == NULL)
	    return ENOMEM;
	if (strncmp(proxy, "http://", 7) == 0)
	    proxy += 7;
	
	/* check for url terminating slash */
	el = strchr(proxy, '/');
	if (el != NULL)
	    *el = '\0';

	/* check for port in hostname, used below as port */
	el = strchr(proxy, ':');
	if(el != NULL)
	    *el++ = '\0';

	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* On some systems ntohs(foo(..., htons(...))) causes shadowing */
	nport = init_port(el, htons(80));
	snprintf(portstr, sizeof(portstr), "%d", ntohs(nport));

	ret = getaddrinfo(proxy, portstr, &hints, &ai);
	free(proxy2);
	if (ret)
	    return krb5_eai_to_heim_errno(ret, errno);
	
	freeai = TRUE;

    } else {
	ret = krb5_krbhst_get_addrinfo(context, hi, &ai);
	if (ret)
	    return ret;
    }

    /* add up times */
    gettimeofday(&nrstop, NULL);
    timevalsub(&nrstop, &nrstart);
    timevaladd(&ctx->stats.name_resolution, &nrstop);

    ctx->stats.num_hosts++;

    for (a = ai; a != NULL; a = a->ai_next) {
	rk_socket_t fd;

	fd = socket(a->ai_family, a->ai_socktype | SOCK_CLOEXEC, a->ai_protocol);
	if (rk_IS_BAD_SOCKET(fd))
	    continue;
	rk_cloexec(fd);

#ifndef NO_LIMIT_FD_SETSIZE
	if (fd >= FD_SETSIZE) {
	    _krb5_debug(context, 0, "fd too large for select");
	    rk_closesocket(fd);
	    continue;
	}
#endif
	socket_set_nonblocking(fd, 1);

	host = heim_alloc(sizeof(*host), "sendto-host", deallocate_host);
	if (host == NULL) {
            if (freeai)
                freeaddrinfo(ai);
	    rk_closesocket(fd);
	    return ENOMEM;
	}
	host->hi = hi;
	host->fd = fd;
	host->ai = a;
	/* next version of stid */
	host->tid = ctx->stid = (ctx->stid & 0xffff0000) | ((ctx->stid & 0xffff) + 1);

	host->state = CONNECT;

	switch (host->hi->proto) {
	case KRB5_KRBHST_HTTP :
	    host->fun = &http_fun;
	    break;
	case KRB5_KRBHST_TCP :
	    host->fun = &tcp_fun;
	    break;
	case KRB5_KRBHST_UDP :
	    host->fun = &udp_fun;
	    break;
	default:
	    heim_abort("undefined http transport protocol: %d", (int)host->hi->proto);
	}

	host->tries = host->fun->ntries;

	/*
	 * Connect directly next host, wait a host_timeout for each next address
	 */
	if (submitted_host == 0)
	    host_connect(context, ctx, host);
	else {
	    debug_host(context, 5, host,
		       "Queuing host in future (in %ds), its the %lu address on the same name",
		       (int)(context->host_timeout * submitted_host), submitted_host + 1);
	    host->timeout = time(NULL) + (submitted_host * context->host_timeout);
	}

	heim_array_append_value(ctx->hosts, host);

	heim_release(host);

	submitted_host++;
    }

    if (freeai)
	freeaddrinfo(ai);

    if (!submitted_host)
	return KRB5_KDC_UNREACH;

    return 0;
}

struct wait_ctx {
    krb5_context context;
    krb5_sendto_ctx ctx;
    fd_set rfds;
    fd_set wfds;
    unsigned max_fd;
    int got_reply;
    time_t timenow;
};

static void
wait_setup(heim_object_t obj, void *iter_ctx, int *stop)
{
    struct wait_ctx *wait_ctx = iter_ctx;
    struct host *h = (struct host *)obj;

    /* skip dead hosts */
    if (h->state == DEAD)
	return;

    if (h->state == CONNECT) {
	if (h->timeout < wait_ctx->timenow)
	    host_connect(wait_ctx->context, wait_ctx->ctx, h);
	return;
    }

    /* if host timed out, dec tries and (retry or kill host) */
    if (h->timeout < wait_ctx->timenow) {
	heim_assert(h->tries != 0, "tries should not reach 0");
	h->tries--;
	if (h->tries == 0) {
	    host_dead(wait_ctx->context, h, "host timed out");
	    return;
	} else {
	    debug_host(wait_ctx->context, 5, h, "retrying sending to");
	    host_next_timeout(wait_ctx->context, h);
	    host_connected(wait_ctx->context, wait_ctx->ctx, h);
	}
    }
    
#ifndef NO_LIMIT_FD_SETSIZE
    heim_assert(h->fd < FD_SETSIZE, "fd too large");
#endif
    switch (h->state) {
    case WAITING_REPLY:
	FD_SET(h->fd, &wait_ctx->rfds);
	break;
    case CONNECTING:
    case CONNECTED:
	FD_SET(h->fd, &wait_ctx->rfds);
	FD_SET(h->fd, &wait_ctx->wfds);
	break;
    default:
	heim_abort("invalid sendto host state");
    }
    if (h->fd > wait_ctx->max_fd)
	wait_ctx->max_fd = h->fd;
}

static int
wait_filter_dead(heim_object_t obj, void *ctx)
{
    struct host *h = (struct host *)obj;
    return (int)((h->state == DEAD) ? true : false);
}

static void
wait_process(heim_object_t obj, void *ctx, int *stop)
{
    struct wait_ctx *wait_ctx = ctx;
    struct host *h = (struct host *)obj;
    int readable, writeable;
    heim_assert(h->state != DEAD, "dead host resurected");

#ifndef NO_LIMIT_FD_SETSIZE
    heim_assert(h->fd < FD_SETSIZE, "fd too large");
#endif
    readable = FD_ISSET(h->fd, &wait_ctx->rfds);
    writeable = FD_ISSET(h->fd, &wait_ctx->wfds);

    if (readable || writeable || h->state == CONNECT)
	wait_ctx->got_reply |= eval_host_state(wait_ctx->context, wait_ctx->ctx, h, readable, writeable);

    /* if there is already a reply, just fall though the array */
    if (wait_ctx->got_reply)
	*stop = 1;
}

static krb5_error_code
wait_response(krb5_context context, int *action, krb5_sendto_ctx ctx)
{
    struct wait_ctx wait_ctx;
    struct timeval tv;
    int ret;

    wait_ctx.context = context;
    wait_ctx.ctx = ctx;
    FD_ZERO(&wait_ctx.rfds);
    FD_ZERO(&wait_ctx.wfds);
    wait_ctx.max_fd = 0;

    /* oh, we have a reply, it must be a plugin that got it for us */
    if (ctx->response.length) {
	*action = KRB5_SENDTO_FILTER;
	return 0;
    }

    wait_ctx.timenow = time(NULL);

    heim_array_iterate_f(ctx->hosts, &wait_ctx, wait_setup);
    heim_array_filter_f(ctx->hosts, &wait_ctx, wait_filter_dead);

    if (heim_array_get_length(ctx->hosts) == 0) {
	if (ctx->stateflags & KRBHST_COMPLETED) {
	    _krb5_debug(context, 5, "no more hosts to send/recv packets to/from "
			 "trying to pulling more hosts");
	    *action = KRB5_SENDTO_FAILED;
	} else {
	    _krb5_debug(context, 5, "no more hosts to send/recv packets to/from "
			 "and no more hosts -> failure");
	    *action = KRB5_SENDTO_TIMEOUT;
	}
	return 0;
    }

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    ret = select(wait_ctx.max_fd + 1, &wait_ctx.rfds, &wait_ctx.wfds, NULL, &tv);
    if (ret < 0)
	return errno;
    if (ret == 0) {
	*action = KRB5_SENDTO_TIMEOUT;
	return 0;
    }

    wait_ctx.got_reply = 0;
    heim_array_iterate_f(ctx->hosts, &wait_ctx, wait_process);
    if (wait_ctx.got_reply)
	*action = KRB5_SENDTO_FILTER;
    else
	*action = KRB5_SENDTO_CONTINUE;

    return 0;
}

static void
reset_context(krb5_context context, krb5_sendto_ctx ctx)
{
    krb5_data_free(&ctx->response);
    heim_release(ctx->hosts);
    ctx->hosts = heim_array_create();
    ctx->stateflags = 0;
}


/*
 *
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_sendto_context(krb5_context context,
		    krb5_sendto_ctx ctx,
		    const krb5_data *send_data,
		    krb5_const_realm realm,
		    krb5_data *receive)
{
    krb5_error_code ret = 0;
    krb5_krbhst_handle handle = NULL;
    struct timeval nrstart, nrstop, stop_time;
    int type, freectx = 0;
    int action;
    int numreset = 0;

    krb5_data_zero(receive);
    
    if (ctx == NULL) {
	ret = krb5_sendto_ctx_alloc(context, &ctx);
	if (ret)
	    goto out;
	freectx = 1;
    }

    ctx->stid = (context->num_kdc_requests++) << 16;

    memset(&ctx->stats, 0, sizeof(ctx->stats));
    gettimeofday(&ctx->stats.start_time, NULL);

    type = ctx->type;
    if (type == 0) {
	if ((ctx->flags & KRB5_KRBHST_FLAGS_MASTER) || context->use_admin_kdc)
	    type = KRB5_KRBHST_ADMIN;
	else
	    type = KRB5_KRBHST_KDC;
    }

    ctx->send_data = send_data;

    if ((int)send_data->length > context->large_msg_size)
	ctx->flags |= KRB5_KRBHST_FLAGS_LARGE_MSG;

    /* loop until we get back a appropriate response */

    action = KRB5_SENDTO_INITIAL;

    while (action != KRB5_SENDTO_DONE && action != KRB5_SENDTO_FAILED) {
	krb5_krbhst_info *hi;

	switch (action) {
	case KRB5_SENDTO_INITIAL:
	    ret = realm_via_plugin(context, realm, context->kdc_timeout,
				   send_data, &ctx->response);
	    if (ret == 0 || ret != KRB5_PLUGIN_NO_HANDLE) {
		action = KRB5_SENDTO_DONE;
		break;
	    }
	    action = KRB5_SENDTO_KRBHST;
	    /* FALLTHOUGH */
	case KRB5_SENDTO_KRBHST:
	    if (ctx->krbhst == NULL) {
		ret = krb5_krbhst_init_flags(context, realm, type,
					     ctx->flags, &handle);
		if (ret)
		    goto out;

		if (ctx->hostname) {
		    ret = krb5_krbhst_set_hostname(context, handle, ctx->hostname);
		    if (ret)
			goto out;
		}

	    } else {
		handle = heim_retain(ctx->krbhst);
	    }
	    action = KRB5_SENDTO_TIMEOUT;
	    /* FALLTHOUGH */
	case KRB5_SENDTO_TIMEOUT:

	    /*
	     * If we completed, just got to next step
	     */

	    if (ctx->stateflags & KRBHST_COMPLETED) {
		action = KRB5_SENDTO_CONTINUE;
		break;
	    }

	    /*
	     * Pull out next host, if there is no more, close the
	     * handle and mark as completed.
	     *
	     * Collect time spent in krbhst (dns, plugin, etc)
	     */


	    gettimeofday(&nrstart, NULL);

	    ret = krb5_krbhst_next(context, handle, &hi);

	    gettimeofday(&nrstop, NULL);
	    timevalsub(&nrstop, &nrstart);
	    timevaladd(&ctx->stats.krbhst, &nrstop);

	    action = KRB5_SENDTO_CONTINUE;
	    if (ret == 0) {
		_krb5_debug(context, 5, "submissing new requests to new host");
		if (submit_request(context, ctx, hi) != 0)
		    action = KRB5_SENDTO_TIMEOUT;
	    } else {
		_krb5_debug(context, 5, "out of hosts, waiting for replies");
		ctx->stateflags |= KRBHST_COMPLETED;
	    }

	    break;
	case KRB5_SENDTO_CONTINUE:

	    ret = wait_response(context, &action, ctx);
	    if (ret)
		goto out;

	    break;
	case KRB5_SENDTO_RESET:
	    /* start over */
	    _krb5_debug(context, 5,
			"krb5_sendto trying over again (reset): %d",
			numreset);
	    reset_context(context, ctx);
	    if (handle) {
		krb5_krbhst_free(context, handle);
		handle = NULL;
	    }
	    numreset++;
	    if (numreset >= 3)
		action = KRB5_SENDTO_FAILED;
	    else
		action = KRB5_SENDTO_KRBHST;

	    break;
	case KRB5_SENDTO_FILTER:
	    /* default to next state, the filter function might modify this */
	    action = KRB5_SENDTO_DONE;

	    if (ctx->func) {
		ret = (*ctx->func)(context, ctx, ctx->data,
				   &ctx->response, &action);
		if (ret)
		    goto out;
	    }
	    break;
	case KRB5_SENDTO_FAILED:
	    ret = KRB5_KDC_UNREACH;
	    break;
	case KRB5_SENDTO_DONE:
	    ret = 0;
	    break;
	default:
	    heim_abort("invalid krb5_sendto_context state");
	}
    }

out:
    gettimeofday(&stop_time, NULL);
    timevalsub(&stop_time, &ctx->stats.start_time);
    if (ret == 0 && ctx->response.length) {
	*receive = ctx->response;
	krb5_data_zero(&ctx->response);
    } else {
	krb5_data_free(&ctx->response);
	krb5_clear_error_message (context);
	ret = KRB5_KDC_UNREACH;
	krb5_set_error_message(context, ret,
			       N_("unable to reach any KDC in realm %s", ""),
			       realm);
    }

    _krb5_debug(context, 1,
		"%s %s done: %d hosts %lu packets %lu:"
		" wc: %jd.%06ld nr: %jd.%06ld kh: %jd.%06ld tid: %08x",
		__func__, realm, ret,
		ctx->stats.num_hosts, ctx->stats.sent_packets,
		(intmax_t)stop_time.tv_sec,
		(long)stop_time.tv_usec,
		(intmax_t)ctx->stats.name_resolution.tv_sec,
		(long)ctx->stats.name_resolution.tv_usec,
		(intmax_t)ctx->stats.krbhst.tv_sec,
		(long)ctx->stats.krbhst.tv_usec, ctx->stid);


    if (freectx)
	krb5_sendto_ctx_free(context, ctx);
    else
	reset_context(context, ctx);

    if (handle)
	krb5_krbhst_free(context, handle);

    return ret;
}
