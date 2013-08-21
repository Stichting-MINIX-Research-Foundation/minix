/*
 * Copyright (C) 2001-2004 Bart Massey and Jamey Sharp.
 * All Rights Reserved.
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

#ifndef __XCBEXT_H
#define __XCBEXT_H

#include "xcb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* xcb_ext.c */

struct xcb_extension_t {
    const char *name;
    int global_id;
};


/* xcb_out.c */

typedef struct {
    size_t count;
    xcb_extension_t *ext;
    uint8_t opcode;
    uint8_t isvoid;
} xcb_protocol_request_t;

enum xcb_send_request_flags_t {
    XCB_REQUEST_CHECKED = 1 << 0,
    XCB_REQUEST_RAW = 1 << 1,
    XCB_REQUEST_DISCARD_REPLY = 1 << 2
};

unsigned int xcb_send_request(xcb_connection_t *c, int flags, struct iovec *vector, const xcb_protocol_request_t *request);

/* xcb_take_socket allows external code to ask XCB for permission to
 * take over the write side of the socket and send raw data with
 * xcb_writev. xcb_take_socket provides the sequence number of the last
 * request XCB sent. The caller of xcb_take_socket must supply a
 * callback which XCB can call when it wants the write side of the
 * socket back to make a request. This callback synchronizes with the
 * external socket owner and flushes any output queues if appropriate.
 * If you are sending requests which won't cause a reply, please note the
 * comment for xcb_writev which explains some sequence number wrap issues.
 * */
int xcb_take_socket(xcb_connection_t *c, void (*return_socket)(void *closure), void *closure, int flags, uint64_t *sent);

/* You must own the write-side of the socket (you've called
 * xcb_take_socket, and haven't returned from return_socket yet) to call
 * xcb_writev. Also, the iovec must have at least 1 byte of data in it.
 * You have to make sure that xcb can detect sequence number wraps correctly.
 * This means that the first request you send after xcb_take_socket must cause a
 * reply (e.g. just insert a GetInputFocus request). After every (1 << 16) - 1
 * requests without a reply, you have to insert a request which will cause a
 * reply. You can again use GetInputFocus for this. You do not have to wait for
 * any of the GetInputFocus replies, but can instead handle them via
 * xcb_discard_reply(). */
int xcb_writev(xcb_connection_t *c, struct iovec *vector, int count, uint64_t requests);


/* xcb_in.c */

void *xcb_wait_for_reply(xcb_connection_t *c, unsigned int request, xcb_generic_error_t **e);
int xcb_poll_for_reply(xcb_connection_t *c, unsigned int request, void **reply, xcb_generic_error_t **error);


/* xcb_util.c */

int xcb_popcount(uint32_t mask);
int xcb_sumof(uint8_t *list, int len);

#ifdef __cplusplus
}
#endif

#endif
