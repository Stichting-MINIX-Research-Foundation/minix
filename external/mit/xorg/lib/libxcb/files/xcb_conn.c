/* Copyright (C) 2001-2004 Bart Massey and Jamey Sharp.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Except as contained in this notice, the names of the authors or their
 * institutions shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization from the authors.
 */

/* Connection management: the core of XCB. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>

#include "xcb.h"
#include "xcbint.h"
#if USE_POLL
#include <poll.h>
#elif !defined _WIN32
#include <sys/select.h>
#endif

#ifdef _WIN32
#include "xcb_windefs.h"
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif /* _WIN32 */

/* SHUT_RDWR is fairly recent and is not available on all platforms */
#if !defined(SHUT_RDWR)
#define SHUT_RDWR 2
#endif

typedef struct {
    uint8_t  status;
    uint8_t  pad0[5];
    uint16_t length;
} xcb_setup_generic_t;

static const int xcb_con_error = XCB_CONN_ERROR;
static const int xcb_con_closed_mem_er = XCB_CONN_CLOSED_MEM_INSUFFICIENT;
static const int xcb_con_closed_parse_er = XCB_CONN_CLOSED_PARSE_ERR;
static const int xcb_con_closed_screen_er = XCB_CONN_CLOSED_INVALID_SCREEN;

static int set_fd_flags(const int fd)
{
/* Win32 doesn't have file descriptors and the fcntl function. This block sets the socket in non-blocking mode */

#ifdef _WIN32
   u_long iMode = 1; /* non-zero puts it in non-blocking mode, 0 in blocking mode */   
   int ret = 0;

   ret = ioctlsocket(fd, FIONBIO, &iMode);
   if(ret != 0) 
       return 0;
   return 1;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1)
        return 0;
    flags |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, flags) == -1)
        return 0;
    if(fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
        return 0;
    return 1;
#endif /* _WIN32 */
}

static int write_setup(xcb_connection_t *c, xcb_auth_info_t *auth_info)
{
    static const char pad[3];
    xcb_setup_request_t out;
    struct iovec parts[6];
    int count = 0;
    static const uint32_t endian = 0x01020304;
    int ret;

    memset(&out, 0, sizeof(out));

    /* B = 0x42 = MSB first, l = 0x6c = LSB first */
    if(htonl(endian) == endian)
        out.byte_order = 0x42;
    else
        out.byte_order = 0x6c;
    out.protocol_major_version = X_PROTOCOL;
    out.protocol_minor_version = X_PROTOCOL_REVISION;
    out.authorization_protocol_name_len = 0;
    out.authorization_protocol_data_len = 0;
    parts[count].iov_len = sizeof(xcb_setup_request_t);
    parts[count++].iov_base = &out;
    parts[count].iov_len = XCB_PAD(sizeof(xcb_setup_request_t));
    parts[count++].iov_base = (char *) pad;

    if(auth_info)
    {
        parts[count].iov_len = out.authorization_protocol_name_len = auth_info->namelen;
        parts[count++].iov_base = auth_info->name;
        parts[count].iov_len = XCB_PAD(out.authorization_protocol_name_len);
        parts[count++].iov_base = (char *) pad;
        parts[count].iov_len = out.authorization_protocol_data_len = auth_info->datalen;
        parts[count++].iov_base = auth_info->data;
        parts[count].iov_len = XCB_PAD(out.authorization_protocol_data_len);
        parts[count++].iov_base = (char *) pad;
    }
    assert(count <= (int) (sizeof(parts) / sizeof(*parts)));

    pthread_mutex_lock(&c->iolock);
    ret = _xcb_out_send(c, parts, count);
    pthread_mutex_unlock(&c->iolock);
    return ret;
}

static int read_setup(xcb_connection_t *c)
{
    /* Read the server response */
    c->setup = malloc(sizeof(xcb_setup_generic_t));
    if(!c->setup)
        return 0;

    if(_xcb_in_read_block(c, c->setup, sizeof(xcb_setup_generic_t)) != sizeof(xcb_setup_generic_t))
        return 0;

    {
        void *tmp = realloc(c->setup, c->setup->length * 4 + sizeof(xcb_setup_generic_t));
        if(!tmp)
            return 0;
        c->setup = tmp;
    }

    if(_xcb_in_read_block(c, (char *) c->setup + sizeof(xcb_setup_generic_t), c->setup->length * 4) <= 0)
        return 0;

    /* 0 = failed, 2 = authenticate, 1 = success */
    switch(c->setup->status)
    {
    case 0: /* failed */
        {
            xcb_setup_failed_t *setup = (xcb_setup_failed_t *) c->setup;
            write(STDERR_FILENO, xcb_setup_failed_reason(setup), xcb_setup_failed_reason_length(setup));
            return 0;
        }

    case 2: /* authenticate */
        {
            xcb_setup_authenticate_t *setup = (xcb_setup_authenticate_t *) c->setup;
            write(STDERR_FILENO, xcb_setup_authenticate_reason(setup), xcb_setup_authenticate_reason_length(setup));
            return 0;
        }
    }

    return 1;
}

/* precondition: there must be something for us to write. */
static int write_vec(xcb_connection_t *c, struct iovec **vector, int *count)
{
    int n;
    assert(!c->out.queue_len);

#ifdef _WIN32
    int i = 0;
    int ret = 0,err = 0;
    struct iovec *vec;
    n = 0;

    /* Could use the WSASend win32 function for scatter/gather i/o but setting up the WSABUF struct from
       an iovec would require more work and I'm not sure of the benefit....works for now */
    vec = *vector;
    while(i < *count)
    {         	 
         ret = send(c->fd,vec->iov_base,vec->iov_len,0);	 
         if(ret == SOCKET_ERROR)
         {
             err  = WSAGetLastError();
             if(err == WSAEWOULDBLOCK)
             {
                 return 1;
             }
         }
         n += ret;
         *vec++;
         i++;
    }
#else
    n = *count;
    if (n > IOV_MAX)
	n = IOV_MAX;

    n = writev(c->fd, *vector, n);
    if(n < 0 && errno == EAGAIN)
        return 1;
#endif /* _WIN32 */    

    if(n <= 0)
    {
        _xcb_conn_shutdown(c, XCB_CONN_ERROR);
        return 0;
    }

    for(; *count; --*count, ++*vector)
    {
        int cur = (*vector)->iov_len;
        if(cur > n)
            cur = n;
        (*vector)->iov_len -= cur;
        (*vector)->iov_base = (char *) (*vector)->iov_base + cur;
        n -= cur;
        if((*vector)->iov_len)
            break;
    }
    if(!*count)
        *vector = 0;
    assert(n == 0);
    return 1;
}

/* Public interface */

const xcb_setup_t *xcb_get_setup(xcb_connection_t *c)
{
    if(c->has_error)
        return 0;
    /* doesn't need locking because it's never written to. */
    return c->setup;
}

int xcb_get_file_descriptor(xcb_connection_t *c)
{
    if(c->has_error)
        return -1;
    /* doesn't need locking because it's never written to. */
    return c->fd;
}

int xcb_connection_has_error(xcb_connection_t *c)
{
    /* doesn't need locking because it's read and written atomically. */
    return c->has_error;
}

xcb_connection_t *xcb_connect_to_fd(int fd, xcb_auth_info_t *auth_info)
{
    xcb_connection_t* c;

#ifndef _WIN32
#ifndef USE_POLL
    if(fd >= FD_SETSIZE) /* would overflow in FD_SET */
    {
        close(fd);
        return _xcb_conn_ret_error(XCB_CONN_ERROR);
    }
#endif
#endif /* !_WIN32*/

    c = calloc(1, sizeof(xcb_connection_t));
    if(!c) {
        close(fd);
        return _xcb_conn_ret_error(XCB_CONN_CLOSED_MEM_INSUFFICIENT) ;
    }

    c->fd = fd;

    if(!(
        set_fd_flags(fd) &&
        pthread_mutex_init(&c->iolock, 0) == 0 &&
        _xcb_in_init(&c->in) &&
        _xcb_out_init(&c->out) &&
        write_setup(c, auth_info) &&
        read_setup(c) &&
        _xcb_ext_init(c) &&
        _xcb_xid_init(c)
        ))
    {
        xcb_disconnect(c);
        return _xcb_conn_ret_error(XCB_CONN_ERROR);
    }

    return c;
}

void xcb_disconnect(xcb_connection_t *c)
{
    if(c->has_error)
        return;

    free(c->setup);

    /* disallow further sends and receives */
    shutdown(c->fd, SHUT_RDWR);
    close(c->fd);

    pthread_mutex_destroy(&c->iolock);
    _xcb_in_destroy(&c->in);
    _xcb_out_destroy(&c->out);

    _xcb_ext_destroy(c);
    _xcb_xid_destroy(c);

    free(c);

#ifdef _WIN32
    WSACleanup();
#endif
}

/* Private interface */

void _xcb_conn_shutdown(xcb_connection_t *c, int err)
{
    c->has_error = err;
}

/* Return connection error state.
 * To make thread-safe, I need a seperate static
 * variable for every possible error.
 */
xcb_connection_t *_xcb_conn_ret_error(int err)
{

    switch(err)
    {
        case XCB_CONN_CLOSED_MEM_INSUFFICIENT:
        {
            return (xcb_connection_t *) &xcb_con_closed_mem_er;
        }
        case XCB_CONN_CLOSED_PARSE_ERR:
        {
            return (xcb_connection_t *) &xcb_con_closed_parse_er;
        }
        case XCB_CONN_CLOSED_INVALID_SCREEN:
        {
            return (xcb_connection_t *) &xcb_con_closed_screen_er;
        }
        case XCB_CONN_ERROR:
        default:
        {
            return (xcb_connection_t *) &xcb_con_error;
        }
    }
}

int _xcb_conn_wait(xcb_connection_t *c, pthread_cond_t *cond, struct iovec **vector, int *count)
{
    int ret;
#if USE_POLL
    struct pollfd fd;
#else
    fd_set rfds, wfds;
#endif

    /* If the thing I should be doing is already being done, wait for it. */
    if(count ? c->out.writing : c->in.reading)
    {
        pthread_cond_wait(cond, &c->iolock);
        return 1;
    }

#if USE_POLL
    memset(&fd, 0, sizeof(fd));
    fd.fd = c->fd;
    fd.events = POLLIN;
#else
    FD_ZERO(&rfds);
    FD_SET(c->fd, &rfds);
#endif
    ++c->in.reading;

#if USE_POLL
    if(count)
    {
        fd.events |= POLLOUT;
        ++c->out.writing;
    }
#else
    FD_ZERO(&wfds);
    if(count)
    {
        FD_SET(c->fd, &wfds);
        ++c->out.writing;
    }
#endif

    pthread_mutex_unlock(&c->iolock);
    do {
#if USE_POLL
        ret = poll(&fd, 1, -1);
        /* If poll() returns an event we didn't expect, such as POLLNVAL, treat
         * it as if it failed. */
        if(ret >= 0 && (fd.revents & ~fd.events))
        {
            ret = -1;
            break;
        }
#else
        ret = select(c->fd + 1, &rfds, &wfds, 0, 0);
#endif
    } while (ret == -1 && errno == EINTR);
    if(ret < 0)
    {
        _xcb_conn_shutdown(c, XCB_CONN_ERROR);
        ret = 0;
    }
    pthread_mutex_lock(&c->iolock);

    if(ret)
    {
        /* The code allows two threads to call select()/poll() at the same time.
         * First thread just wants to read, a second thread wants to write, too.
         * We have to make sure that we don't steal the reading thread's reply
         * and let it get stuck in select()/poll().
         * So a thread may read if either:
         * - There is no other thread that wants to read (the above situation
         *   did not occur).
         * - It is the reading thread (above situation occurred).
         */
        int may_read = c->in.reading == 1 || !count;
#if USE_POLL
        if(may_read && (fd.revents & POLLIN) == POLLIN)
#else
        if(may_read && FD_ISSET(c->fd, &rfds))
#endif
            ret = ret && _xcb_in_read(c);

#if USE_POLL
        if((fd.revents & POLLOUT) == POLLOUT)
#else
        if(FD_ISSET(c->fd, &wfds))
#endif
            ret = ret && write_vec(c, vector, count);
    }

    if(count)
        --c->out.writing;
    --c->in.reading;

    return ret;
}
