/*	$NetBSD: store_fd.c,v 1.2 2017/01/28 21:31:49 christos Exp $	*/

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

typedef struct fd_storage {
    int fd;
} fd_storage;

#define FD(S) (((fd_storage*)(S)->data)->fd)

static ssize_t
fd_fetch(krb5_storage * sp, void *data, size_t size)
{
    char *cbuf = (char *)data;
    ssize_t count;
    size_t rem = size;

    /* similar pattern to net_read() to support pipes */
    while (rem > 0) {
	count = read (FD(sp), cbuf, rem);
	if (count < 0) {
	    if (errno == EINTR)
		continue;
	    else
		return count;
	} else if (count == 0) {
	    return count;
	}
	cbuf += count;
	rem -= count;
    }
    return size;
}

static ssize_t
fd_store(krb5_storage * sp, const void *data, size_t size)
{
    const char *cbuf = (const char *)data;
    ssize_t count;
    size_t rem = size;

    /* similar pattern to net_write() to support pipes */
    while (rem > 0) {
	count = write(FD(sp), cbuf, rem);
	if (count < 0) {
	    if (errno == EINTR)
		continue;
	    else
		return count;
	}
	cbuf += count;
	rem -= count;
    }
    return size;
}

static off_t
fd_seek(krb5_storage * sp, off_t offset, int whence)
{
    return lseek(FD(sp), offset, whence);
}

static int
fd_trunc(krb5_storage * sp, off_t offset)
{
    if (ftruncate(FD(sp), offset) == -1)
	return errno;
    return 0;
}

static int
fd_sync(krb5_storage * sp)
{
    if (fsync(FD(sp)) == -1)
	return errno;
    return 0;
}

static void
fd_free(krb5_storage * sp)
{
    int save_errno = errno;
    if (close(FD(sp)) == 0)
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
 * @sa krb5_storage_from_socket()
 */

KRB5_LIB_FUNCTION krb5_storage * KRB5_LIB_CALL
krb5_storage_from_fd(int fd_in)
{
    krb5_storage *sp;
    int saved_errno;
    int fd;

#ifdef _MSC_VER
    /*
     * This function used to try to pass the input to
     * _get_osfhandle() to test if the value is a HANDLE
     * but this doesn't work because doing so throws an
     * exception that will result in Watson being triggered
     * to file a Windows Error Report.
     */
    fd = _dup(fd_in);
#else
    fd = dup(fd_in);
#endif

    if (fd < 0)
	return NULL;

    errno = ENOMEM;
    sp = malloc(sizeof(krb5_storage));
    if (sp == NULL) {
	saved_errno = errno;
	close(fd);
	errno = saved_errno;
	return NULL;
    }

    errno = ENOMEM;
    sp->data = malloc(sizeof(fd_storage));
    if (sp->data == NULL) {
	saved_errno = errno;
	close(fd);
	free(sp);
	errno = saved_errno;
	return NULL;
    }
    sp->flags = 0;
    sp->eof_code = HEIM_ERR_EOF;
    FD(sp) = fd;
    sp->fetch = fd_fetch;
    sp->store = fd_store;
    sp->seek = fd_seek;
    sp->trunc = fd_trunc;
    sp->fsync = fd_sync;
    sp->free = fd_free;
    sp->max_alloc = UINT_MAX/8;
    return sp;
}
