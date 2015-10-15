/*	$NetBSD: test-mini_inetd.c,v 1.1.1.2 2014/04/24 12:45:52 pettai Exp $	*/

/***********************************************************************
 * Copyright (c) 2009, Secure Endpoints Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **********************************************************************/

#include <config.h>
#include <krb5/roken.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define PORT 8013
#define PORT_S "8013"

char * prog = "Master";
int is_client = 0;

static int
get_address(int flags, struct addrinfo ** ret)
{
    struct addrinfo ai;
    int rv;

    memset(&ai, 0, sizeof(ai));

    ai.ai_flags = flags | AI_NUMERICHOST;
    ai.ai_family = AF_INET;
    ai.ai_socktype = SOCK_STREAM;
    ai.ai_protocol = PF_UNSPEC;

    rv = getaddrinfo("127.0.0.1", PORT_S, &ai, ret);
    if (rv)
	warnx("getaddrinfo: %s", gai_strerror(rv));
    return rv;
}

static int
get_connected_socket(rk_socket_t * s_ret)
{
    struct addrinfo * ai = NULL;
    int rv = 0;
    rk_socket_t s = rk_INVALID_SOCKET;

    rv = get_address(0, &ai);
    if (rv)
	return rv;

    s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (rk_IS_BAD_SOCKET(s)) {
	rv = 1;
	goto done;
    }

    rv = connect(s, ai->ai_addr, ai->ai_addrlen);
    if (rk_IS_SOCKET_ERROR(rv))
	goto done;

    *s_ret = s;
    s = rk_INVALID_SOCKET;
    rv = 0;

 done:
    if (!rk_IS_BAD_SOCKET(s))
	rk_closesocket(s);

    if (ai)
	freeaddrinfo(ai);

    return (rv) ? rk_SOCK_ERRNO : 0;
}

const char * test_strings[] = {
    "Hello",
    "01234566789012345689012345678901234567890123456789",
    "Another test",
    "exit"
};

static int
test_simple_echo_client(void)
{
    rk_socket_t s = rk_INVALID_SOCKET;
    int rv;
    char buf[81];
    int i;

    fprintf(stderr, "[%s] Getting connected socket...", getprogname());
    rv = get_connected_socket(&s);
    if (rv) {
	fprintf(stderr, "\n[%s] get_connected_socket() failed (%s)\n",
		getprogname(), strerror(rk_SOCK_ERRNO));
	return 1;
    }

    fprintf(stderr, "[%s] done\n", getprogname());

    for (i=0; i < sizeof(test_strings)/sizeof(test_strings[0]); i++) {
	rv = send(s, test_strings[i], strlen(test_strings[i]), 0);
	if (rk_IS_SOCKET_ERROR(rv)) {
	    fprintf(stderr, "[%s] send() failure (%s)\n",
		    getprogname(), strerror(rk_SOCK_ERRNO));
	    rk_closesocket(s);
	    return 1;
	}

	rv = recv(s, buf, sizeof(buf), 0);
	if (rk_IS_SOCKET_ERROR(rv)) {
	    fprintf (stderr, "[%s] recv() failure (%s)\n",
		     getprogname(), strerror(rk_SOCK_ERRNO));
	    rk_closesocket(s);
	    return 1;
	}

	if (rv == 0) {
	    fprintf (stderr, "[%s] No data received\n", prog);
	    rk_closesocket(s);
	    return 1;
	}

	if (rv != strlen(test_strings[i])) {
	    fprintf (stderr, "[%s] Data length mismatch %d != %d\n", prog, rv, strlen(test_strings[i]));
	    rk_closesocket(s);
	    return 1;
	}
    }

    fprintf (stderr, "[%s] Done\n", prog);
    rk_closesocket(s);
    return 0;
}

static int
test_simple_echo_socket(void)
{
    fprintf (stderr, "[%s] Process ID %d\n", prog, GetCurrentProcessId());
    fprintf (stderr, "[%s] Starting echo test with sockets\n", prog);

    if (is_client) {
	return test_simple_echo_client();
    } else {

	rk_socket_t s = rk_INVALID_SOCKET;

	fprintf (stderr, "[%s] Listening for connections...\n", prog);
	mini_inetd(htons(PORT), &s);
	if (rk_IS_BAD_SOCKET(s)) {
	    fprintf (stderr, "[%s] Connect failed (%s)\n",
		     getprogname(), strerror(rk_SOCK_ERRNO));
	} else {
	    fprintf (stderr, "[%s] Connected\n", prog);
	}

	{
	    char buf[81];
	    int rv, srv;

	    while ((rv = recv(s, buf, sizeof(buf), 0)) != 0 && !rk_IS_SOCKET_ERROR(rv)) {
		buf[rv] = 0;
		fprintf(stderr, "[%s] Received [%s]\n", prog, buf);

		/* simple echo */
		srv = send(s, buf, rv, 0);
		if (srv != rv) {
		    if (rk_IS_SOCKET_ERROR(srv))
			fprintf(stderr, "[%s] send() error [%s]\n",
				getprogname(), strerror(rk_SOCK_ERRNO));
		    else
			fprintf(stderr, "[%s] send() size mismatch %d != %d",
				getprogname(), srv, rv);
		}

		if (!strcmp(buf, "exit")) {
		    fprintf(stderr, "[%s] Exiting...\n", prog);
		    shutdown(s, SD_SEND);
		    rk_closesocket(s);
		    return 0;
		}
	    }

	    fprintf(stderr, "[%s] recv() failed (%s)\n",
		    getprogname(),
		    strerror(rk_SOCK_ERRNO));
	}

	rk_closesocket(s);
    }

    return 1;
}

static int
test_simple_echo(void)
{
    fprintf (stderr, "[%s] Starting echo test\n", prog);

    if (is_client) {

	return test_simple_echo_client();

    } else {

	fprintf (stderr, "[%s] Listening for connections...\n", prog);
	mini_inetd(htons(PORT), NULL);
	fprintf (stderr, "[%s] Connected\n", prog);

	{
	    char buf[81];
	    while (gets(buf)) {
		fprintf(stderr, "[%s] Received [%s]\n", prog, buf);

		if (!strcmp(buf, "exit"))
		    return 0;

		/* simple echo */
		puts(buf);
	    }

	    fprintf(stderr, "[%s] gets() failed (%s)\n", prog, _strerror("gets"));
	}
    }

    return 1;
}

static int
do_client(void)
{
    int rv = 0;

    rk_SOCK_INIT();

    prog = "Client";
    is_client = 1;

    fprintf(stderr, "Starting client...\n");

    rv = test_simple_echo_socket();

    rk_SOCK_EXIT();

    return rv;
}

static int
do_server(void)
{
    int rv = 0;

    rk_SOCK_INIT();

    prog = "Server";

    fprintf(stderr, "Starting server...\n");

    rv = test_simple_echo_socket();

    rk_SOCK_EXIT();

    return rv;
}

static time_t
wait_callback(void *p)
{
    return (time_t)-1;
}

static int
do_test(char * path)
{
    intptr_t p_server;
    intptr_t p_client;
    int client_rv;
    int server_rv;

    p_server = _spawnl(_P_NOWAIT, path, path, "--server", NULL);
    if (p_server <= 0) {
	fprintf(stderr, "%s: %s", path, _strerror("Can't start server process"));
	return 1;
    }
#ifdef _WIN32
    /* On Windows, the _spawn*() functions return a process handle on
       success.  We need a process ID for use with
       wait_for_process_timed(). */

    p_server = GetProcessId((HANDLE) p_server);
#endif
    fprintf(stderr, "Created server process ID %d\n", p_server);

    p_client = _spawnl(_P_NOWAIT, path, path, "--client", NULL);
    if (p_client <= 0) {
	fprintf(stderr, "%s: %s", path, _strerror("Can't start client process"));
	fprintf(stderr, "Waiting for server process to terminate ...");
	wait_for_process_timed(p_server, wait_callback, NULL, 5);
	fprintf(stderr, "DONE\n");
	return 1;
    }
#ifdef _WIN32
    p_client = GetProcessId((HANDLE) p_client);
#endif
    fprintf(stderr, "Created client process ID %d\n", p_client);

    fprintf(stderr, "Waiting for client process to terminate ...");
    client_rv = wait_for_process_timed(p_client, wait_callback, NULL, 5);
    if (SE_IS_ERROR(client_rv)) {
	fprintf(stderr, "\nwait_for_process_timed() failed for client. rv=%d\n", client_rv);
    } else {
	fprintf(stderr, "DONE\n");
    }

    fprintf(stderr, "Waiting for server process to terminate ...");
    server_rv = wait_for_process_timed(p_server, wait_callback, NULL, 5);
    if (SE_IS_ERROR(server_rv)) {
	fprintf(stderr, "\nwait_for_process_timed() failed for server. rv=%d\n", server_rv);
    } else {
	fprintf(stderr, "DONE\n");
    }

    if (client_rv == 0 && server_rv == 0) {
	fprintf(stderr, "PASS\n");
	return 0;
    } else {
	fprintf(stderr, "FAIL: Client rv=%d, Server rv=%d\n", client_rv, server_rv);
	return 1;
    }
}

int main(int argc, char ** argv)
{
    setprogname(argv[0]);

    if (argc == 2 && strcmp(argv[1], "--client") == 0)
	return do_client();
    else if (argc == 2 && strcmp(argv[1], "--server") == 0)
	return do_server();
    else if (argc == 1)
	return do_test(argv[0]);
    else {
	printf ("%s: Test mini_inetd() function.  Run with no arguments to start test\n",
		argv[0]);
	return 1;
    }
}
