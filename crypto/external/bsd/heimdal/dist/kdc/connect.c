/*	$NetBSD: connect.c,v 1.2.4.1 2018/05/06 10:29:30 martin Exp $	*/

/*
 * Copyright (c) 1997-2005 Kungliga Tekniska Högskolan
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

#include "kdc_locl.h"

/*
 * a tuple describing on what to listen
 */

struct port_desc{
    int family;
    int type;
    int port;
};

/* the current ones */

static struct port_desc *ports;
static size_t num_ports;
static pid_t bonjour_pid = -1;

/*
 * add `family, port, protocol' to the list with duplicate suppresion.
 */

static void
add_port(krb5_context context,
	 int family, int port, const char *protocol)
{
    int type;
    size_t i;

    if(strcmp(protocol, "udp") == 0)
	type = SOCK_DGRAM;
    else if(strcmp(protocol, "tcp") == 0)
	type = SOCK_STREAM;
    else
	return;
    for(i = 0; i < num_ports; i++){
	if(ports[i].type == type
	   && ports[i].port == port
	   && ports[i].family == family)
	    return;
    }
    ports = realloc(ports, (num_ports + 1) * sizeof(*ports));
    if (ports == NULL)
	krb5_err (context, 1, errno, "realloc");
    ports[num_ports].family = family;
    ports[num_ports].type   = type;
    ports[num_ports].port   = port;
    num_ports++;
}

/*
 * add a triple but with service -> port lookup
 * (this prints warnings for stuff that does not exist)
 */

static void
add_port_service(krb5_context context,
		 int family, const char *service, int port,
		 const char *protocol)
{
    port = krb5_getportbyname (context, service, protocol, port);
    add_port (context, family, port, protocol);
}

/*
 * add the port with service -> port lookup or string -> number
 * (no warning is printed)
 */

static void
add_port_string (krb5_context context,
		 int family, const char *str, const char *protocol)
{
    struct servent *sp;
    int port;

    sp = roken_getservbyname (str, protocol);
    if (sp != NULL) {
	port = sp->s_port;
    } else {
	char *end;

	port = htons(strtol(str, &end, 0));
	if (end == str)
	    return;
    }
    add_port (context, family, port, protocol);
}

/*
 * add the standard collection of ports for `family'
 */

static void
add_standard_ports (krb5_context context,
		    krb5_kdc_configuration *config,
		    int family)
{
    add_port_service(context, family, "kerberos", 88, "udp");
    add_port_service(context, family, "kerberos", 88, "tcp");
    add_port_service(context, family, "kerberos-sec", 88, "udp");
    add_port_service(context, family, "kerberos-sec", 88, "tcp");
    if(enable_http)
	add_port_service(context, family, "http", 80, "tcp");
    if(config->enable_kx509) {
	add_port_service(context, family, "kca_service", 9878, "udp");
	add_port_service(context, family, "kca_service", 9878, "tcp");
    }

}

/*
 * parse the set of space-delimited ports in `str' and add them.
 * "+" => all the standard ones
 * otherwise it's port|service[/protocol]
 */

static void
parse_ports(krb5_context context,
	    krb5_kdc_configuration *config,
	    const char *str)
{
    char *pos = NULL;
    char *p;
    char *str_copy = strdup (str);

    p = strtok_r(str_copy, " \t", &pos);
    while(p != NULL) {
	if(strcmp(p, "+") == 0) {
#ifdef HAVE_IPV6
	    add_standard_ports(context, config, AF_INET6);
#endif
	    add_standard_ports(context, config, AF_INET);
	} else {
	    char *q = strchr(p, '/');
	    if(q){
		*q++ = 0;
#ifdef HAVE_IPV6
		add_port_string(context, AF_INET6, p, q);
#endif
		add_port_string(context, AF_INET, p, q);
	    }else {
#ifdef HAVE_IPV6
		add_port_string(context, AF_INET6, p, "udp");
		add_port_string(context, AF_INET6, p, "tcp");
#endif
		add_port_string(context, AF_INET, p, "udp");
		add_port_string(context, AF_INET, p, "tcp");
	    }
	}

	p = strtok_r(NULL, " \t", &pos);
    }
    free (str_copy);
}

/*
 * every socket we listen on
 */

struct descr {
    krb5_socket_t s;
    int type;
    int port;
    unsigned char *buf;
    size_t size;
    size_t len;
    time_t timeout;
    struct sockaddr_storage __ss;
    struct sockaddr *sa;
    socklen_t sock_len;
    char addr_string[128];
};

static void
init_descr(struct descr *d)
{
    memset(d, 0, sizeof(*d));
    d->sa = (struct sockaddr *)&d->__ss;
    d->s = rk_INVALID_SOCKET;
}

/*
 * re-initialize all `n' ->sa in `d'.
 */

static void
reinit_descrs (struct descr *d, int n)
{
    int i;

    for (i = 0; i < n; ++i)
	d[i].sa = (struct sockaddr *)&d[i].__ss;
}

/*
 * Create the socket (family, type, port) in `d'
 */

static void
init_socket(krb5_context context,
	    krb5_kdc_configuration *config,
	    struct descr *d, krb5_address *a, int family, int type, int port)
{
    krb5_error_code ret;
    struct sockaddr_storage __ss;
    struct sockaddr *sa = (struct sockaddr *)&__ss;
    krb5_socklen_t sa_size = sizeof(__ss);

    init_descr (d);

    ret = krb5_addr2sockaddr (context, a, sa, &sa_size, port);
    if (ret) {
	krb5_warn(context, ret, "krb5_addr2sockaddr");
	rk_closesocket(d->s);
	d->s = rk_INVALID_SOCKET;
	return;
    }

    if (sa->sa_family != family)
	return;

    d->s = socket(family, type, 0);
    if(rk_IS_BAD_SOCKET(d->s)){
	krb5_warn(context, errno, "socket(%d, %d, 0)", family, type);
	d->s = rk_INVALID_SOCKET;
	return;
    }
    rk_cloexec(d->s);
#if defined(HAVE_SETSOCKOPT) && defined(SOL_SOCKET) && defined(SO_REUSEADDR)
    {
	int one = 1;
	setsockopt(d->s, SOL_SOCKET, SO_REUSEADDR, (void *)&one, sizeof(one));
    }
#endif
    d->type = type;
    d->port = port;

    socket_set_nonblocking(d->s, 1);

    if(rk_IS_SOCKET_ERROR(bind(d->s, sa, sa_size))){
	char a_str[256];
	size_t len;

	krb5_print_address (a, a_str, sizeof(a_str), &len);
	krb5_warn(context, errno, "bind %s/%d", a_str, ntohs(port));
	rk_closesocket(d->s);
	d->s = rk_INVALID_SOCKET;
	return;
    }
    if(type == SOCK_STREAM && rk_IS_SOCKET_ERROR(listen(d->s, SOMAXCONN))){
	char a_str[256];
	size_t len;

	krb5_print_address (a, a_str, sizeof(a_str), &len);
	krb5_warn(context, errno, "listen %s/%d", a_str, ntohs(port));
	rk_closesocket(d->s);
	d->s = rk_INVALID_SOCKET;
	return;
    }
}

/*
 * Allocate descriptors for all the sockets that we should listen on
 * and return the number of them.
 */

static int
init_sockets(krb5_context context,
	     krb5_kdc_configuration *config,
	     struct descr **desc)
{
    krb5_error_code ret;
    size_t i, j;
    struct descr *d;
    int num = 0;
    krb5_addresses addresses;

    if (explicit_addresses.len) {
	addresses = explicit_addresses;
    } else {
	ret = krb5_get_all_server_addrs (context, &addresses);
	if (ret)
	    krb5_err (context, 1, ret, "krb5_get_all_server_addrs");
    }
    parse_ports(context, config, port_str);
    d = malloc(addresses.len * num_ports * sizeof(*d));
    if (d == NULL)
	krb5_errx(context, 1, "malloc(%lu) failed",
		  (unsigned long)num_ports * sizeof(*d));

    for (i = 0; i < num_ports; i++){
	for (j = 0; j < addresses.len; ++j) {
	    init_socket(context, config, &d[num], &addresses.val[j],
			ports[i].family, ports[i].type, ports[i].port);
	    if(d[num].s != rk_INVALID_SOCKET){
		char a_str[80];
		size_t len;

		krb5_print_address (&addresses.val[j], a_str,
				    sizeof(a_str), &len);

		kdc_log(context, config, 5, "listening on %s port %u/%s",
			a_str,
			ntohs(ports[i].port),
			(ports[i].type == SOCK_STREAM) ? "tcp" : "udp");
		/* XXX */
		num++;
	    }
	}
    }
    krb5_free_addresses (context, &addresses);
    d = realloc(d, num * sizeof(*d));
    if (d == NULL && num != 0)
	krb5_errx(context, 1, "realloc(%lu) failed",
		  (unsigned long)num * sizeof(*d));
    reinit_descrs (d, num);
    *desc = d;
    return num;
}

/*
 *
 */

static const char *
descr_type(struct descr *d)
{
    if (d->type == SOCK_DGRAM)
	return "udp";
    else if (d->type == SOCK_STREAM)
	return "tcp";
    return "unknown";
}

static void
addr_to_string(krb5_context context,
	       struct sockaddr *addr, size_t addr_len, char *str, size_t len)
{
    krb5_address a;
    if(krb5_sockaddr2address(context, addr, &a) == 0) {
	if(krb5_print_address(&a, str, len, &len) == 0) {
	    krb5_free_address(context, &a);
	    return;
	}
	krb5_free_address(context, &a);
    }
    snprintf(str, len, "<family=%d>", addr->sa_family);
}

/*
 *
 */

static void
send_reply(krb5_context context,
	   krb5_kdc_configuration *config,
	   krb5_boolean prependlength,
	   struct descr *d,
	   krb5_data *reply)
{
    kdc_log(context, config, 5,
	    "sending %lu bytes to %s", (unsigned long)reply->length,
	    d->addr_string);
    if(prependlength){
	unsigned char l[4];
	l[0] = (reply->length >> 24) & 0xff;
	l[1] = (reply->length >> 16) & 0xff;
	l[2] = (reply->length >> 8) & 0xff;
	l[3] = reply->length & 0xff;
	if(rk_IS_SOCKET_ERROR(sendto(d->s, l, sizeof(l), 0, d->sa, d->sock_len))) {
	    kdc_log (context, config,
		     0, "sendto(%s): %s", d->addr_string,
		     strerror(rk_SOCK_ERRNO));
	    return;
	}
    }
    if(rk_IS_SOCKET_ERROR(sendto(d->s, reply->data, reply->length, 0, d->sa, d->sock_len))) {
	kdc_log (context, config, 0, "sendto(%s): %s", d->addr_string,
		 strerror(rk_SOCK_ERRNO));
	return;
    }
}

/*
 * Handle the request in `buf, len' to socket `d'
 */

static void
do_request(krb5_context context,
	   krb5_kdc_configuration *config,
	   void *buf, size_t len, krb5_boolean prependlength,
	   struct descr *d)
{
    krb5_error_code ret;
    krb5_data reply;
    int datagram_reply = (d->type == SOCK_DGRAM);

    krb5_kdc_update_time(NULL);

    krb5_data_zero(&reply);
    ret = krb5_kdc_process_request(context, config,
				   buf, len, &reply, &prependlength,
				   d->addr_string, d->sa,
				   datagram_reply);
    if(request_log)
	krb5_kdc_save_request(context, request_log, buf, len, &reply, d->sa);
    if(reply.length){
	send_reply(context, config, prependlength, d, &reply);
	krb5_data_free(&reply);
    }
    if(ret)
	kdc_log(context, config, 0,
		"Failed processing %lu byte request from %s",
		(unsigned long)len, d->addr_string);
}

/*
 * Handle incoming data to the UDP socket in `d'
 */

static void
handle_udp(krb5_context context,
	   krb5_kdc_configuration *config,
	   struct descr *d)
{
    unsigned char *buf;
    ssize_t n;

    buf = malloc(max_request_udp);
    if (buf == NULL){
	kdc_log(context, config, 0, "Failed to allocate %lu bytes",
	        (unsigned long)max_request_udp);
	return;
    }

    d->sock_len = sizeof(d->__ss);
    n = recvfrom(d->s, buf, max_request_udp, 0, d->sa, &d->sock_len);
    if (rk_IS_SOCKET_ERROR(n)) {
	if (rk_SOCK_ERRNO != EAGAIN && rk_SOCK_ERRNO != EINTR)
	    krb5_warn(context, rk_SOCK_ERRNO, "recvfrom");
    } else {
	addr_to_string (context, d->sa, d->sock_len,
			d->addr_string, sizeof(d->addr_string));
	if ((size_t)n == max_request_udp) {
	    krb5_data data;
	    krb5_warn(context, errno,
		      "recvfrom: truncated packet from %s, asking for TCP",
		      d->addr_string);
	    krb5_mk_error(context,
			  KRB5KRB_ERR_RESPONSE_TOO_BIG,
			  NULL,
			  NULL,
			  NULL,
			  NULL,
			  NULL,
			  NULL,
			  &data);
	    send_reply(context, config, FALSE, d, &data);
	    krb5_data_free(&data);
	} else {
	    do_request(context, config, buf, n, FALSE, d);
	}
    }
    free (buf);
}

static void
clear_descr(struct descr *d)
{
    if(d->buf)
	memset(d->buf, 0, d->size);
    d->len = 0;
    if(d->s != rk_INVALID_SOCKET)
	rk_closesocket(d->s);
    d->s = rk_INVALID_SOCKET;
}


/* remove HTTP %-quoting from buf */
static int
de_http(char *buf)
{
    unsigned char *p, *q;
    for(p = q = (unsigned char *)buf; *p; p++, q++) {
	if(*p == '%' && isxdigit(p[1]) && isxdigit(p[2])) {
	    unsigned int x;
	    if(sscanf((char *)p + 1, "%2x", &x) != 1)
		return -1;
	    *q = x;
	    p += 2;
	} else
	    *q = *p;
    }
    *q = '\0';
    return 0;
}

#define TCP_TIMEOUT 4

/*
 * accept a new TCP connection on `d[parent]' and store it in `d[child]'
 */

static void
add_new_tcp (krb5_context context,
	     krb5_kdc_configuration *config,
	     struct descr *d, int parent, int child)
{
    krb5_socket_t s;

    if (child == -1)
	return;

    d[child].sock_len = sizeof(d[child].__ss);
    s = accept(d[parent].s, d[child].sa, &d[child].sock_len);
    if(rk_IS_BAD_SOCKET(s)) {
	if (rk_SOCK_ERRNO != EAGAIN && rk_SOCK_ERRNO != EINTR)
	    krb5_warn(context, rk_SOCK_ERRNO, "accept");
	return;
    }

#ifdef FD_SETSIZE
    if (s >= FD_SETSIZE) {
	krb5_warnx(context, "socket FD too large");
	rk_closesocket (s);
	return;
    }
#endif

    d[child].s = s;
    d[child].timeout = time(NULL) + TCP_TIMEOUT;
    d[child].type = SOCK_STREAM;
    addr_to_string (context,
		    d[child].sa, d[child].sock_len,
		    d[child].addr_string, sizeof(d[child].addr_string));
}

/*
 * Grow `d' to handle at least `n'.
 * Return != 0 if fails
 */

static int
grow_descr (krb5_context context,
	    krb5_kdc_configuration *config,
	    struct descr *d, size_t n)
{
    if (d->size - d->len < n) {
	unsigned char *tmp;
	size_t grow;

	grow = max(1024, d->len + n);
	if (d->size + grow > max_request_tcp) {
	    kdc_log(context, config, 0, "Request exceeds max request size (%lu bytes).",
		    (unsigned long)d->size + grow);
	    clear_descr(d);
	    return -1;
	}
	tmp = realloc (d->buf, d->size + grow);
	if (tmp == NULL) {
	    kdc_log(context, config, 0, "Failed to re-allocate %lu bytes.",
		    (unsigned long)d->size + grow);
	    clear_descr(d);
	    return -1;
	}
	d->size += grow;
	d->buf = tmp;
    }
    return 0;
}

/*
 * Try to handle the TCP data at `d->buf, d->len'.
 * Return -1 if failed, 0 if succesful, and 1 if data is complete.
 */

static int
handle_vanilla_tcp (krb5_context context,
		    krb5_kdc_configuration *config,
		    struct descr *d)
{
    krb5_storage *sp;
    uint32_t len;

    sp = krb5_storage_from_mem(d->buf, d->len);
    if (sp == NULL) {
	kdc_log (context, config, 0, "krb5_storage_from_mem failed");
	return -1;
    }
    krb5_ret_uint32(sp, &len);
    krb5_storage_free(sp);
    if(d->len - 4 >= len) {
	memmove(d->buf, d->buf + 4, d->len - 4);
	d->len -= 4;
	return 1;
    }
    return 0;
}

/*
 * Try to handle the TCP/HTTP data at `d->buf, d->len'.
 * Return -1 if failed, 0 if succesful, and 1 if data is complete.
 */

static int
handle_http_tcp (krb5_context context,
		 krb5_kdc_configuration *config,
		 struct descr *d)
{
    char *s, *p, *t;
    void *data;
    char *proto;
    int len;

    s = (char *)d->buf;

    /* If its a multi line query, truncate off the first line */
    p = strstr(s, "\r\n");
    if (p)
	*p = 0;

    p = NULL;
    t = strtok_r(s, " \t", &p);
    if (t == NULL) {
	kdc_log(context, config, 0,
		"Missing HTTP operand (GET) request from %s", d->addr_string);
	return -1;
    }

    t = strtok_r(NULL, " \t", &p);
    if(t == NULL) {
	kdc_log(context, config, 0,
		"Missing HTTP GET data in request from %s", d->addr_string);
	return -1;
    }

    data = malloc(strlen(t));
    if (data == NULL) {
	kdc_log(context, config, 0, "Failed to allocate %lu bytes",
		(unsigned long)strlen(t));
	return -1;
    }
    if(*t == '/')
	t++;
    if(de_http(t) != 0) {
	kdc_log(context, config, 0, "Malformed HTTP request from %s", d->addr_string);
	kdc_log(context, config, 5, "HTTP request: %s", t);
	free(data);
	return -1;
    }
    proto = strtok_r(NULL, " \t", &p);
    if (proto == NULL) {
	kdc_log(context, config, 0, "Malformed HTTP request from %s", d->addr_string);
	free(data);
	return -1;
    }
    len = rk_base64_decode(t, data);
    if(len <= 0){
	const char *msg =
	    " 404 Not found\r\n"
	    "Server: Heimdal/" VERSION "\r\n"
	    "Cache-Control: no-cache\r\n"
	    "Pragma: no-cache\r\n"
	    "Content-type: text/html\r\n"
	    "Content-transfer-encoding: 8bit\r\n\r\n"
	    "<TITLE>404 Not found</TITLE>\r\n"
	    "<H1>404 Not found</H1>\r\n"
	    "That page doesn't exist, maybe you are looking for "
	    "<A HREF=\"http://www.h5l.org/\">Heimdal</A>?\r\n";
	kdc_log(context, config, 0, "HTTP request from %s is non KDC request", d->addr_string);
	kdc_log(context, config, 5, "HTTP request: %s", t);
	free(data);
	if (rk_IS_SOCKET_ERROR(send(d->s, proto, strlen(proto), 0))) {
	    kdc_log(context, config, 0, "HTTP write failed: %s: %s",
		    d->addr_string, strerror(rk_SOCK_ERRNO));
	    return -1;
	}
	if (rk_IS_SOCKET_ERROR(send(d->s, msg, strlen(msg), 0))) {
	    kdc_log(context, config, 0, "HTTP write failed: %s: %s",
		    d->addr_string, strerror(rk_SOCK_ERRNO));
	    return -1;
	}
	return -1;
    }
    {
	const char *msg =
	    " 200 OK\r\n"
	    "Server: Heimdal/" VERSION "\r\n"
	    "Cache-Control: no-cache\r\n"
	    "Pragma: no-cache\r\n"
	    "Content-type: application/octet-stream\r\n"
	    "Content-transfer-encoding: binary\r\n\r\n";
	if (rk_IS_SOCKET_ERROR(send(d->s, proto, strlen(proto), 0))) {
	    free(data);
	    kdc_log(context, config, 0, "HTTP write failed: %s: %s",
		    d->addr_string, strerror(rk_SOCK_ERRNO));
	    return -1;
	}
	if (rk_IS_SOCKET_ERROR(send(d->s, msg, strlen(msg), 0))) {
	    free(data);
	    kdc_log(context, config, 0, "HTTP write failed: %s: %s",
		    d->addr_string, strerror(rk_SOCK_ERRNO));
	    return -1;
	}
    }
    if ((size_t)len > d->len)
        len = d->len;
    memcpy(d->buf, data, len);
    d->len = len;
    free(data);
    return 1;
}

/*
 * Handle incoming data to the TCP socket in `d[index]'
 */

static void
handle_tcp(krb5_context context,
	   krb5_kdc_configuration *config,
	   struct descr *d, int idx, int min_free)
{
    unsigned char buf[1024];
    int n;
    int ret = 0;

    if (d[idx].timeout == 0) {
	add_new_tcp (context, config, d, idx, min_free);
	return;
    }

    n = recvfrom(d[idx].s, buf, sizeof(buf), 0, NULL, NULL);
    if(rk_IS_SOCKET_ERROR(n)){
	krb5_warn(context, rk_SOCK_ERRNO, "recvfrom failed from %s to %s/%d",
		  d[idx].addr_string, descr_type(d + idx),
		  ntohs(d[idx].port));
	return;
    } else if (n == 0) {
	krb5_warnx(context, "connection closed before end of data after %lu "
		   "bytes from %s to %s/%d", (unsigned long)d[idx].len,
		   d[idx].addr_string, descr_type(d + idx),
		   ntohs(d[idx].port));
	clear_descr (d + idx);
	return;
    }
    if (grow_descr (context, config, &d[idx], n))
	return;
    memcpy(d[idx].buf + d[idx].len, buf, n);
    d[idx].len += n;
    if(d[idx].len > 4 && d[idx].buf[0] == 0) {
	ret = handle_vanilla_tcp (context, config, &d[idx]);
    } else if(enable_http &&
	      d[idx].len >= 4 &&
	      strncmp((char *)d[idx].buf, "GET ", 4) == 0 &&
	      strncmp((char *)d[idx].buf + d[idx].len - 4,
		      "\r\n\r\n", 4) == 0) {

        /* remove the trailing \r\n\r\n so the string is NUL terminated */
        d[idx].buf[d[idx].len - 4] = '\0';

	ret = handle_http_tcp (context, config, &d[idx]);
	if (ret < 0)
	    clear_descr (d + idx);
    } else if (d[idx].len > 4) {
	kdc_log (context, config,
		 0, "TCP data of strange type from %s to %s/%d",
		 d[idx].addr_string, descr_type(d + idx),
		 ntohs(d[idx].port));
	if (d[idx].buf[0] & 0x80) {
	    krb5_data reply;

	    kdc_log (context, config, 0, "TCP extension not supported");

	    ret = krb5_mk_error(context,
				KRB5KRB_ERR_FIELD_TOOLONG,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL,
				NULL,
				&reply);
	    if (ret == 0) {
		send_reply(context, config, TRUE, d + idx, &reply);
		krb5_data_free(&reply);
	    }
	}
	clear_descr(d + idx);
	return;
    }
    if (ret < 0)
	return;
    else if (ret == 1) {
	do_request(context, config,
		   d[idx].buf, d[idx].len, TRUE, &d[idx]);
	clear_descr(d + idx);
    }
}

#ifdef HAVE_FORK
static void
handle_islive(int fd)
{
    char buf;
    int ret;

    ret = read(fd, &buf, 1);
    if (ret != 1)
	exit_flag = -1;
}
#endif

krb5_boolean
realloc_descrs(struct descr **d, unsigned int *ndescr)
{
    struct descr *tmp;
    size_t i;

    tmp = realloc(*d, (*ndescr + 4) * sizeof(**d));
    if(tmp == NULL)
        return FALSE;

    *d = tmp;
    reinit_descrs (*d, *ndescr);
    memset(*d + *ndescr, 0, 4 * sizeof(**d));
    for(i = *ndescr; i < *ndescr + 4; i++)
        init_descr (*d + i);

    *ndescr += 4;

    return TRUE;
}

int
next_min_free(krb5_context context, struct descr **d, unsigned int *ndescr)
{
    size_t i;
    int min_free;

    for(i = 0; i < *ndescr; i++) {
        int s = (*d + i)->s;
        if(rk_IS_BAD_SOCKET(s))
            return i;
    }

    min_free = *ndescr;
    if(!realloc_descrs(d, ndescr)) {
        min_free = -1;
        krb5_warnx(context, "No memory");
    }

    return min_free;
}

static void
loop(krb5_context context, krb5_kdc_configuration *config,
     struct descr *d, unsigned int ndescr, int islive)
{

    while (exit_flag == 0) {
	struct timeval tmout;
	fd_set fds;
	int min_free = -1;
	int max_fd = 0;
	size_t i;

	FD_ZERO(&fds);
        if (islive > -1) {
            FD_SET(islive, &fds);
            max_fd = islive;
        }
	for (i = 0; i < ndescr; i++) {
	    if (!rk_IS_BAD_SOCKET(d[i].s)) {
		if (d[i].type == SOCK_STREAM &&
		   d[i].timeout && d[i].timeout < time(NULL)) {
		    kdc_log(context, config, 1,
			    "TCP-connection from %s expired after %lu bytes",
			    d[i].addr_string, (unsigned long)d[i].len);
		    clear_descr(&d[i]);
		    continue;
		}
#ifndef NO_LIMIT_FD_SETSIZE
		if (max_fd < d[i].s)
		    max_fd = d[i].s;
#ifdef FD_SETSIZE
		if (max_fd >= FD_SETSIZE)
		    krb5_errx(context, 1, "fd too large");
#endif
#endif
		FD_SET(d[i].s, &fds);
	    }
	}

	tmout.tv_sec = TCP_TIMEOUT;
	tmout.tv_usec = 0;
	switch(select(max_fd + 1, &fds, 0, 0, &tmout)){
	case 0:
	    break;
	case -1:
	    if (errno != EINTR)
		krb5_warn(context, rk_SOCK_ERRNO, "select");
	    break;
	default:
#ifdef HAVE_FORK
	    if (islive > -1 && FD_ISSET(islive, &fds))
		handle_islive(islive);
#endif
	    for (i = 0; i < ndescr; i++)
		if (!rk_IS_BAD_SOCKET(d[i].s) && FD_ISSET(d[i].s, &fds)) {
		    min_free = next_min_free(context, &d, &ndescr);

		    if (d[i].type == SOCK_DGRAM)
			handle_udp(context, config, &d[i]);
		    else if (d[i].type == SOCK_STREAM)
			handle_tcp(context, config, d, i, min_free);
		}
	}
    }

    switch (exit_flag) {
    case -1:
	kdc_log(context, config, 0,
                "KDC worker process exiting because KDC master exited.");
	break;
#ifdef SIGXCPU
    case SIGXCPU:
	kdc_log(context, config, 0, "CPU time limit exceeded");
	break;
#endif
    case SIGINT:
    case SIGTERM:
	kdc_log(context, config, 0, "Terminated");
	break;
    default:
	kdc_log(context, config, 0, "Unexpected exit reason: %d", exit_flag);
	break;
    }
}

#ifdef __APPLE__
static void
bonjour_kid(krb5_context context, krb5_kdc_configuration *config, const char *argv0, int *islive)
{
    char buf;

    if (do_bonjour > 0) {
	bonjour_announce(context, config);

	while (read(0, &buf, 1) == 1)
	    continue;
	_exit(0);
    }

    if ((bonjour_pid = fork()) != 0)
	return;

    close(islive[0]);
    if (dup2(islive[1], 0) == -1)
	err(1, "failed to announce with bonjour (dup)");
    if (islive[1] != 0)
        close(islive[1]);
    execlp(argv0, "kdc", "--bonjour", NULL);
    err(1, "failed to announce with bonjour (exec)");
}
#endif

#ifdef HAVE_FORK
static void
kill_kids(pid_t *pids, int max_kids, int sig)
{
    int i;

    for (i=0; i < max_kids; i++)
	if (pids[i] > 0)
	    kill(sig, pids[i]);
    if (bonjour_pid > 0)
        kill(sig, bonjour_pid);
}

static int
reap_kid(krb5_context context, krb5_kdc_configuration *config,
	 pid_t *pids, int max_kids, int options)
{
    pid_t pid;
    char *what;
    int status;
    int i = 0; /* quiet warnings */

    pid = waitpid(-1, &status, options);
    if (pid < 1)
	return 0;

    if (pid != bonjour_pid) {
        for (i=0; i < max_kids; i++) {
            if (pids[i] == pid)
                break;
        }

        if (i == max_kids) {
            /* XXXrcd: this should not happen, have to do something, though */
            return 0;
        }
    }

    if (pid == bonjour_pid)
        what = "bonjour";
    else
        what = "worker";
    if (WIFEXITED(status))
        kdc_log(context, config, 0, "KDC reaped %s process: %d, exit status: %d",
                what, (int)pid, WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        kdc_log(context, config, 0, "KDC reaped %s process: %d, term signal %d%s",
                what, (int)pid, WTERMSIG(status),
                WCOREDUMP(status) ? " (core dumped)" : "");
    else
        kdc_log(context, config, 0, "KDC reaped %s process: %d",
                what, (int)pid);
    if (pid == bonjour_pid) {
        bonjour_pid = (pid_t)-1;
        return 0;
    } else {
        pids[i] = (pid_t)-1;
        return 1;
    }
}

static int
reap_kids(krb5_context context, krb5_kdc_configuration *config,
	  pid_t *pids, int max_kids)
{
    int reaped = 0;

    for (;;) {
	if (reap_kid(context, config, pids, max_kids, WNOHANG) == 0)
	    break;
	reaped++;
    }

    return reaped;
}

static void
select_sleep(int microseconds)
{
    struct timeval tv;

    tv.tv_sec = microseconds / 1000000;
    tv.tv_usec = microseconds % 1000000;
    select(0, NULL, NULL, NULL, &tv);
}
#endif

void
start_kdc(krb5_context context,
	  krb5_kdc_configuration *config, const char *argv0)
{
    struct timeval tv1;
    struct timeval tv2;
    struct descr *d;
    unsigned int ndescr;
    pid_t pid = -1;
#ifdef HAVE_FORK
    pid_t *pids;
    int max_kdcs = config->num_kdc_processes;
    int num_kdcs = 0;
    int i;
    int islive[2];
#endif

#ifdef __APPLE__
    if (do_bonjour > 0)
        bonjour_kid(context, config, argv0, NULL);
#endif

#ifdef HAVE_FORK
#ifdef _SC_NPROCESSORS_ONLN
    if (max_kdcs < 1)
	max_kdcs = sysconf(_SC_NPROCESSORS_ONLN);
#endif

    if (max_kdcs < 1)
	max_kdcs = 1;

    pids = calloc(max_kdcs, sizeof(*pids));
    if (!pids)
	krb5_err(context, 1, errno, "malloc");

    /*
     * We open a socketpair of which we hand one end to each of our kids.
     * When we exit, for whatever reason, the children will notice an EOF
     * on their end and be able to cleanly exit.
     */

    if (socketpair(PF_LOCAL, SOCK_STREAM, 0, islive) == -1)
	krb5_errx(context, 1, "socketpair");
    socket_set_nonblocking(islive[1], 1);
#endif

    ndescr = init_sockets(context, config, &d);
    if(ndescr <= 0)
	krb5_errx(context, 1, "No sockets!");

#ifdef HAVE_FORK

# ifdef __APPLE__
    if (do_bonjour < 0)
        bonjour_kid(context, config, argv0, islive);
# endif

    kdc_log(context, config, 0, "KDC started master process pid=%d", getpid());
#else
    kdc_log(context, config, 0, "KDC started pid=%d", getpid());
#endif

    roken_detach_finish(NULL, daemon_child);

    tv1.tv_sec  = 0;
    tv1.tv_usec = 0;

#ifdef HAVE_FORK
    if (!testing_flag) {
        /* Note that we might never execute the body of this loop */
        while (exit_flag == 0) {

            /* Slow down the creation of KDCs... */

            gettimeofday(&tv2, NULL);
            if (tv1.tv_sec == tv2.tv_sec && tv2.tv_usec - tv1.tv_usec < 25000) {
#if 0	/* XXXrcd: should print a message... */
                kdc_log(context, config, 0, "Spawning KDCs too quickly, "
                    "pausing for 50ms");
#endif
                select_sleep(12500);
                continue;
            }

            if (num_kdcs >= max_kdcs) {
                num_kdcs -= reap_kid(context, config, pids, max_kdcs, 0);
                continue;
            }

            if (num_kdcs > 0)
                num_kdcs -= reap_kids(context, config, pids, max_kdcs);

            pid = fork();
            switch (pid) {
            case 0:
                close(islive[0]);
                loop(context, config, d, ndescr, islive[1]);
                exit(0);
            case -1:
                /* XXXrcd: hmmm, do something useful?? */
                kdc_log(context, config, 0,
                        "KDC master process could not fork worker process");
                sleep(10);
                break;
            default:
                for (i=0; i < max_kdcs; i++) {
                    if (pids[i] < 1) {
                        pids[i] = pid;
                        break;
                    }
                }
                kdc_log(context, config, 0, "KDC worker process started: %d",
                        pid);
                num_kdcs++;
                gettimeofday(&tv1, NULL);
                break;
            }
        }

        /* Closing these sockets should cause the kids to die... */

        close(islive[0]);
        close(islive[1]);

        /* Close our listener sockets before terminating workers */
        for (i = 0; i < ndescr; ++i)
            clear_descr(&d[i]);

        gettimeofday(&tv1, NULL);
        tv2 = tv1;

        /* Reap every 10ms, terminate stragglers once a second, give up after 10 */
        for (;;) {
            struct timeval tv3;
            num_kdcs -= reap_kids(context, config, pids, max_kdcs);
            if (num_kdcs == 0 && bonjour_pid <= 0)
                goto end;
            /*
             * Using select to sleep will fail with EINTR if we receive a
             * SIGCHLD.  This is desirable.
             */
            select_sleep(10000);
            gettimeofday(&tv3, NULL);
            if (tv3.tv_sec - tv1.tv_sec > 10 ||
                (tv3.tv_sec - tv1.tv_sec == 10 && tv3.tv_usec >= tv1.tv_usec))
                break;
            if (tv3.tv_sec - tv2.tv_sec > 1 ||
                (tv3.tv_sec - tv2.tv_sec == 1 && tv3.tv_usec >= tv2.tv_usec)) {
                kill_kids(pids, max_kdcs, SIGTERM);
                tv2 = tv3;
            }
        }

        /* Kill stragglers and reap every 200ms, give up after 15s */
        for (;;) {
            kill_kids(pids, max_kdcs, SIGKILL);
            num_kdcs -= reap_kids(context, config, pids, max_kdcs);
            if (num_kdcs == 0 && bonjour_pid <= 0)
                break;
            select_sleep(200000);
            gettimeofday(&tv2, NULL);
            if (tv2.tv_sec - tv1.tv_sec > 15 ||
                (tv2.tv_sec - tv1.tv_sec == 15 && tv2.tv_usec >= tv1.tv_usec))
                break;
        }

     end:
        kdc_log(context, config, 0, "KDC master process exiting", pid);
        free(pids);
    } else {
        loop(context, config, d, ndescr, -1);
        kdc_log(context, config, 0, "KDC exiting", pid);
    }
#else
    loop(context, config, d, ndescr, -1);
    kdc_log(context, config, 0, "KDC exiting", pid);
#endif

    free(d);
}
