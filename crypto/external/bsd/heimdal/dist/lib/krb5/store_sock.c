/*	$NetBSD: store_sock.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

/*
 * Copyright (c) 1997 - 2004 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"
#include "store-int.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

typedef struct socket_storage {
    krb5_socket_t sock;
} socket_storage;

#define SOCK(S) (((socket_storage*)(S)->data)->sock)

static ssize_t
socket_fetch(krb5_storage * sp, void *data, size_t size)
{
    return net_read(SOCK(sp), data, size);
}

static ssize_t
socket_store(krb5_storage * sp, const void *data, size_t size)
{
    return net_write(SOCK(sp), data, size);
}

static off_t
socket_seek(krb5_storage * sp, off_t offset, int whence)
{
    return lseek(SOCK(sp), offset, whence);
}

static int
socket_trunc(krb5_storage * sp, off_t offset)
{
    if (ftruncate(SOCK(sp), offset) == -1)
	return errno;
    return 0;
}

static int
socket_sync(krb5_storage * sp)
{
    if (fsync(SOCK(sp)) == -1)
	return errno;
    return 0;
}

static void
socket_free(krb5_storage * sp)
{
    int save_errno = errno;
    if (rk_IS_SOCKET_ERROR(rk_closesocket(SOCK(sp))))
        errno = rk_SOCK_ERRNO;
    else
        errno = save_errno;
}

/**
 *
 *
 * @return A krb5_storage on success, or NULL on out of memory error.
 *
 * @ingroup krb5_storage
 *
 * @sa krb5_storage_emem()
 * @sa krb5_storage_from_mem()
 * @sa krb5_storage_from_readonly_mem()
 * @sa krb5_storage_from_data()
 * @sa krb5_storage_from_fd()
 */

KRB5_LIB_FUNCTION krb5_storage * KRB5_LIB_CALL
krb5_storage_from_socket(krb5_socket_t sock_in)
{
    krb5_storage *sp;
    int saved_errno;
    krb5_socket_t sock;

#ifdef _WIN32
    WSAPROTOCOL_INFO info;

    if (WSADuplicateSocket(sock_in, GetCurrentProcessId(), &info) == 0)
    {

	sock = WSASocket( FROM_PROTOCOL_INFO,
			  FROM_PROTOCOL_INFO,
			  FROM_PROTOCOL_INFO,
			  &info, 0, 0);
    }
#else
    sock = dup(sock_in);
#endif

    if (sock == rk_INVALID_SOCKET)
	return NULL;

    errno = ENOMEM;
    sp = malloc(sizeof(krb5_storage));
    if (sp == NULL) {
	saved_errno = errno;
	rk_closesocket(sock);
	errno = saved_errno;
	return NULL;
    }

    errno = ENOMEM;
    sp->data = malloc(sizeof(socket_storage));
    if (sp->data == NULL) {
	saved_errno = errno;
	rk_closesocket(sock);
	free(sp);
	errno = saved_errno;
	return NULL;
    }
    sp->flags = 0;
    sp->eof_code = HEIM_ERR_EOF;
    SOCK(sp) = sock;
    sp->fetch = socket_fetch;
    sp->store = socket_store;
    sp->seek = socket_seek;
    sp->trunc = socket_trunc;
    sp->fsync = socket_sync;
    sp->free = socket_free;
    sp->max_alloc = UINT_MAX/8;
    return sp;
}
