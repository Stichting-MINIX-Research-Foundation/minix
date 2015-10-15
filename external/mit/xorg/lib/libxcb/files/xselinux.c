/*
 * This file generated automatically from xselinux.xml by c_client.py.
 * Edit at your peril.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>  /* for offsetof() */
#include "xcbext.h"
#include "xselinux.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xproto.h"

xcb_extension_t xcb_selinux_id = { "SELinux", 0 };

xcb_selinux_query_version_cookie_t
xcb_selinux_query_version (xcb_connection_t *c  /**< */,
                           uint8_t           client_major  /**< */,
                           uint8_t           client_minor  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_query_version_cookie_t xcb_ret;
    xcb_selinux_query_version_request_t xcb_out;

    xcb_out.client_major = client_major;
    xcb_out.client_minor = client_minor;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_query_version_cookie_t
xcb_selinux_query_version_unchecked (xcb_connection_t *c  /**< */,
                                     uint8_t           client_major  /**< */,
                                     uint8_t           client_minor  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_query_version_cookie_t xcb_ret;
    xcb_selinux_query_version_request_t xcb_out;

    xcb_out.client_major = client_major;
    xcb_out.client_minor = client_minor;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_query_version_reply_t *
xcb_selinux_query_version_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_selinux_query_version_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */)
{
    return (xcb_selinux_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_set_device_create_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_set_device_create_context_request_t *_aux = (xcb_selinux_set_device_create_context_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_set_device_create_context_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_void_cookie_t
xcb_selinux_set_device_create_context_checked (xcb_connection_t *c  /**< */,
                                               uint32_t          context_len  /**< */,
                                               const char       *context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_SET_DEVICE_CREATE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_selinux_set_device_create_context_request_t xcb_out;

    xcb_out.context_len = context_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char context */
    xcb_parts[4].iov_base = (char *) context;
    xcb_parts[4].iov_len = context_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_selinux_set_device_create_context (xcb_connection_t *c  /**< */,
                                       uint32_t          context_len  /**< */,
                                       const char       *context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_SET_DEVICE_CREATE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_selinux_set_device_create_context_request_t xcb_out;

    xcb_out.context_len = context_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char context */
    xcb_parts[4].iov_base = (char *) context;
    xcb_parts[4].iov_len = context_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_selinux_get_device_create_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_get_device_create_context_reply_t *_aux = (xcb_selinux_get_device_create_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_get_device_create_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_get_device_create_context_cookie_t
xcb_selinux_get_device_create_context (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_DEVICE_CREATE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_device_create_context_cookie_t xcb_ret;
    xcb_selinux_get_device_create_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_get_device_create_context_cookie_t
xcb_selinux_get_device_create_context_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_DEVICE_CREATE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_device_create_context_cookie_t xcb_ret;
    xcb_selinux_get_device_create_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_selinux_get_device_create_context_context (const xcb_selinux_get_device_create_context_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_selinux_get_device_create_context_context_length (const xcb_selinux_get_device_create_context_reply_t *R  /**< */)
{
    return R->context_len;
}

xcb_generic_iterator_t
xcb_selinux_get_device_create_context_context_end (const xcb_selinux_get_device_create_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_get_device_create_context_reply_t *
xcb_selinux_get_device_create_context_reply (xcb_connection_t                                *c  /**< */,
                                             xcb_selinux_get_device_create_context_cookie_t   cookie  /**< */,
                                             xcb_generic_error_t                            **e  /**< */)
{
    return (xcb_selinux_get_device_create_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_set_device_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_set_device_context_request_t *_aux = (xcb_selinux_set_device_context_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_set_device_context_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_void_cookie_t
xcb_selinux_set_device_context_checked (xcb_connection_t *c  /**< */,
                                        uint32_t          device  /**< */,
                                        uint32_t          context_len  /**< */,
                                        const char       *context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_SET_DEVICE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_selinux_set_device_context_request_t xcb_out;

    xcb_out.device = device;
    xcb_out.context_len = context_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char context */
    xcb_parts[4].iov_base = (char *) context;
    xcb_parts[4].iov_len = context_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_selinux_set_device_context (xcb_connection_t *c  /**< */,
                                uint32_t          device  /**< */,
                                uint32_t          context_len  /**< */,
                                const char       *context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_SET_DEVICE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_selinux_set_device_context_request_t xcb_out;

    xcb_out.device = device;
    xcb_out.context_len = context_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char context */
    xcb_parts[4].iov_base = (char *) context;
    xcb_parts[4].iov_len = context_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_selinux_get_device_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_get_device_context_reply_t *_aux = (xcb_selinux_get_device_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_get_device_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_get_device_context_cookie_t
xcb_selinux_get_device_context (xcb_connection_t *c  /**< */,
                                uint32_t          device  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_DEVICE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_device_context_cookie_t xcb_ret;
    xcb_selinux_get_device_context_request_t xcb_out;

    xcb_out.device = device;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_get_device_context_cookie_t
xcb_selinux_get_device_context_unchecked (xcb_connection_t *c  /**< */,
                                          uint32_t          device  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_DEVICE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_device_context_cookie_t xcb_ret;
    xcb_selinux_get_device_context_request_t xcb_out;

    xcb_out.device = device;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_selinux_get_device_context_context (const xcb_selinux_get_device_context_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_selinux_get_device_context_context_length (const xcb_selinux_get_device_context_reply_t *R  /**< */)
{
    return R->context_len;
}

xcb_generic_iterator_t
xcb_selinux_get_device_context_context_end (const xcb_selinux_get_device_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_get_device_context_reply_t *
xcb_selinux_get_device_context_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_selinux_get_device_context_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */)
{
    return (xcb_selinux_get_device_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_set_window_create_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_set_window_create_context_request_t *_aux = (xcb_selinux_set_window_create_context_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_set_window_create_context_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_void_cookie_t
xcb_selinux_set_window_create_context_checked (xcb_connection_t *c  /**< */,
                                               uint32_t          context_len  /**< */,
                                               const char       *context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_SET_WINDOW_CREATE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_selinux_set_window_create_context_request_t xcb_out;

    xcb_out.context_len = context_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char context */
    xcb_parts[4].iov_base = (char *) context;
    xcb_parts[4].iov_len = context_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_selinux_set_window_create_context (xcb_connection_t *c  /**< */,
                                       uint32_t          context_len  /**< */,
                                       const char       *context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_SET_WINDOW_CREATE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_selinux_set_window_create_context_request_t xcb_out;

    xcb_out.context_len = context_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char context */
    xcb_parts[4].iov_base = (char *) context;
    xcb_parts[4].iov_len = context_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_selinux_get_window_create_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_get_window_create_context_reply_t *_aux = (xcb_selinux_get_window_create_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_get_window_create_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_get_window_create_context_cookie_t
xcb_selinux_get_window_create_context (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_WINDOW_CREATE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_window_create_context_cookie_t xcb_ret;
    xcb_selinux_get_window_create_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_get_window_create_context_cookie_t
xcb_selinux_get_window_create_context_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_WINDOW_CREATE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_window_create_context_cookie_t xcb_ret;
    xcb_selinux_get_window_create_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_selinux_get_window_create_context_context (const xcb_selinux_get_window_create_context_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_selinux_get_window_create_context_context_length (const xcb_selinux_get_window_create_context_reply_t *R  /**< */)
{
    return R->context_len;
}

xcb_generic_iterator_t
xcb_selinux_get_window_create_context_context_end (const xcb_selinux_get_window_create_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_get_window_create_context_reply_t *
xcb_selinux_get_window_create_context_reply (xcb_connection_t                                *c  /**< */,
                                             xcb_selinux_get_window_create_context_cookie_t   cookie  /**< */,
                                             xcb_generic_error_t                            **e  /**< */)
{
    return (xcb_selinux_get_window_create_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_get_window_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_get_window_context_reply_t *_aux = (xcb_selinux_get_window_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_get_window_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_get_window_context_cookie_t
xcb_selinux_get_window_context (xcb_connection_t *c  /**< */,
                                xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_WINDOW_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_window_context_cookie_t xcb_ret;
    xcb_selinux_get_window_context_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_get_window_context_cookie_t
xcb_selinux_get_window_context_unchecked (xcb_connection_t *c  /**< */,
                                          xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_WINDOW_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_window_context_cookie_t xcb_ret;
    xcb_selinux_get_window_context_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_selinux_get_window_context_context (const xcb_selinux_get_window_context_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_selinux_get_window_context_context_length (const xcb_selinux_get_window_context_reply_t *R  /**< */)
{
    return R->context_len;
}

xcb_generic_iterator_t
xcb_selinux_get_window_context_context_end (const xcb_selinux_get_window_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_get_window_context_reply_t *
xcb_selinux_get_window_context_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_selinux_get_window_context_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */)
{
    return (xcb_selinux_get_window_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_list_item_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_list_item_t *_aux = (xcb_selinux_list_item_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_list_item_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* object_context */
    xcb_block_len += _aux->object_context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* data_context */
    xcb_block_len += _aux->data_context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

char *
xcb_selinux_list_item_object_context (const xcb_selinux_list_item_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_selinux_list_item_object_context_length (const xcb_selinux_list_item_t *R  /**< */)
{
    return R->object_context_len;
}

xcb_generic_iterator_t
xcb_selinux_list_item_object_context_end (const xcb_selinux_list_item_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->object_context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

char *
xcb_selinux_list_item_data_context (const xcb_selinux_list_item_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_selinux_list_item_object_context_end(R);
    return (char *) ((char *) prev.data + XCB_TYPE_PAD(char, prev.index) + 0);
}

int
xcb_selinux_list_item_data_context_length (const xcb_selinux_list_item_t *R  /**< */)
{
    return R->data_context_len;
}

xcb_generic_iterator_t
xcb_selinux_list_item_data_context_end (const xcb_selinux_list_item_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_selinux_list_item_object_context_end(R);
    i.data = ((char *) child.data) + (R->data_context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_selinux_list_item_next (xcb_selinux_list_item_iterator_t *i  /**< */)
{
    xcb_selinux_list_item_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_selinux_list_item_t *)(((char *)R) + xcb_selinux_list_item_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_selinux_list_item_t *) child.data;
}

xcb_generic_iterator_t
xcb_selinux_list_item_end (xcb_selinux_list_item_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_selinux_list_item_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_selinux_set_property_create_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_set_property_create_context_request_t *_aux = (xcb_selinux_set_property_create_context_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_set_property_create_context_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_void_cookie_t
xcb_selinux_set_property_create_context_checked (xcb_connection_t *c  /**< */,
                                                 uint32_t          context_len  /**< */,
                                                 const char       *context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_SET_PROPERTY_CREATE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_selinux_set_property_create_context_request_t xcb_out;

    xcb_out.context_len = context_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char context */
    xcb_parts[4].iov_base = (char *) context;
    xcb_parts[4].iov_len = context_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_selinux_set_property_create_context (xcb_connection_t *c  /**< */,
                                         uint32_t          context_len  /**< */,
                                         const char       *context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_SET_PROPERTY_CREATE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_selinux_set_property_create_context_request_t xcb_out;

    xcb_out.context_len = context_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char context */
    xcb_parts[4].iov_base = (char *) context;
    xcb_parts[4].iov_len = context_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_selinux_get_property_create_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_get_property_create_context_reply_t *_aux = (xcb_selinux_get_property_create_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_get_property_create_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_get_property_create_context_cookie_t
xcb_selinux_get_property_create_context (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_PROPERTY_CREATE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_property_create_context_cookie_t xcb_ret;
    xcb_selinux_get_property_create_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_get_property_create_context_cookie_t
xcb_selinux_get_property_create_context_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_PROPERTY_CREATE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_property_create_context_cookie_t xcb_ret;
    xcb_selinux_get_property_create_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_selinux_get_property_create_context_context (const xcb_selinux_get_property_create_context_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_selinux_get_property_create_context_context_length (const xcb_selinux_get_property_create_context_reply_t *R  /**< */)
{
    return R->context_len;
}

xcb_generic_iterator_t
xcb_selinux_get_property_create_context_context_end (const xcb_selinux_get_property_create_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_get_property_create_context_reply_t *
xcb_selinux_get_property_create_context_reply (xcb_connection_t                                  *c  /**< */,
                                               xcb_selinux_get_property_create_context_cookie_t   cookie  /**< */,
                                               xcb_generic_error_t                              **e  /**< */)
{
    return (xcb_selinux_get_property_create_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_set_property_use_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_set_property_use_context_request_t *_aux = (xcb_selinux_set_property_use_context_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_set_property_use_context_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_void_cookie_t
xcb_selinux_set_property_use_context_checked (xcb_connection_t *c  /**< */,
                                              uint32_t          context_len  /**< */,
                                              const char       *context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_SET_PROPERTY_USE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_selinux_set_property_use_context_request_t xcb_out;

    xcb_out.context_len = context_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char context */
    xcb_parts[4].iov_base = (char *) context;
    xcb_parts[4].iov_len = context_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_selinux_set_property_use_context (xcb_connection_t *c  /**< */,
                                      uint32_t          context_len  /**< */,
                                      const char       *context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_SET_PROPERTY_USE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_selinux_set_property_use_context_request_t xcb_out;

    xcb_out.context_len = context_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char context */
    xcb_parts[4].iov_base = (char *) context;
    xcb_parts[4].iov_len = context_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_selinux_get_property_use_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_get_property_use_context_reply_t *_aux = (xcb_selinux_get_property_use_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_get_property_use_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_get_property_use_context_cookie_t
xcb_selinux_get_property_use_context (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_PROPERTY_USE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_property_use_context_cookie_t xcb_ret;
    xcb_selinux_get_property_use_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_get_property_use_context_cookie_t
xcb_selinux_get_property_use_context_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_PROPERTY_USE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_property_use_context_cookie_t xcb_ret;
    xcb_selinux_get_property_use_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_selinux_get_property_use_context_context (const xcb_selinux_get_property_use_context_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_selinux_get_property_use_context_context_length (const xcb_selinux_get_property_use_context_reply_t *R  /**< */)
{
    return R->context_len;
}

xcb_generic_iterator_t
xcb_selinux_get_property_use_context_context_end (const xcb_selinux_get_property_use_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_get_property_use_context_reply_t *
xcb_selinux_get_property_use_context_reply (xcb_connection_t                               *c  /**< */,
                                            xcb_selinux_get_property_use_context_cookie_t   cookie  /**< */,
                                            xcb_generic_error_t                           **e  /**< */)
{
    return (xcb_selinux_get_property_use_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_get_property_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_get_property_context_reply_t *_aux = (xcb_selinux_get_property_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_get_property_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_get_property_context_cookie_t
xcb_selinux_get_property_context (xcb_connection_t *c  /**< */,
                                  xcb_window_t      window  /**< */,
                                  xcb_atom_t        property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_PROPERTY_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_property_context_cookie_t xcb_ret;
    xcb_selinux_get_property_context_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.property = property;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_get_property_context_cookie_t
xcb_selinux_get_property_context_unchecked (xcb_connection_t *c  /**< */,
                                            xcb_window_t      window  /**< */,
                                            xcb_atom_t        property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_PROPERTY_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_property_context_cookie_t xcb_ret;
    xcb_selinux_get_property_context_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.property = property;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_selinux_get_property_context_context (const xcb_selinux_get_property_context_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_selinux_get_property_context_context_length (const xcb_selinux_get_property_context_reply_t *R  /**< */)
{
    return R->context_len;
}

xcb_generic_iterator_t
xcb_selinux_get_property_context_context_end (const xcb_selinux_get_property_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_get_property_context_reply_t *
xcb_selinux_get_property_context_reply (xcb_connection_t                           *c  /**< */,
                                        xcb_selinux_get_property_context_cookie_t   cookie  /**< */,
                                        xcb_generic_error_t                       **e  /**< */)
{
    return (xcb_selinux_get_property_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_get_property_data_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_get_property_data_context_reply_t *_aux = (xcb_selinux_get_property_data_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_get_property_data_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_get_property_data_context_cookie_t
xcb_selinux_get_property_data_context (xcb_connection_t *c  /**< */,
                                       xcb_window_t      window  /**< */,
                                       xcb_atom_t        property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_PROPERTY_DATA_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_property_data_context_cookie_t xcb_ret;
    xcb_selinux_get_property_data_context_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.property = property;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_get_property_data_context_cookie_t
xcb_selinux_get_property_data_context_unchecked (xcb_connection_t *c  /**< */,
                                                 xcb_window_t      window  /**< */,
                                                 xcb_atom_t        property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_PROPERTY_DATA_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_property_data_context_cookie_t xcb_ret;
    xcb_selinux_get_property_data_context_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.property = property;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_selinux_get_property_data_context_context (const xcb_selinux_get_property_data_context_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_selinux_get_property_data_context_context_length (const xcb_selinux_get_property_data_context_reply_t *R  /**< */)
{
    return R->context_len;
}

xcb_generic_iterator_t
xcb_selinux_get_property_data_context_context_end (const xcb_selinux_get_property_data_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_get_property_data_context_reply_t *
xcb_selinux_get_property_data_context_reply (xcb_connection_t                                *c  /**< */,
                                             xcb_selinux_get_property_data_context_cookie_t   cookie  /**< */,
                                             xcb_generic_error_t                            **e  /**< */)
{
    return (xcb_selinux_get_property_data_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_list_properties_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_list_properties_reply_t *_aux = (xcb_selinux_list_properties_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_selinux_list_properties_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* properties */
    for(i=0; i<_aux->properties_len; i++) {
        xcb_tmp_len = xcb_selinux_list_item_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_selinux_list_item_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_list_properties_cookie_t
xcb_selinux_list_properties (xcb_connection_t *c  /**< */,
                             xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_LIST_PROPERTIES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_list_properties_cookie_t xcb_ret;
    xcb_selinux_list_properties_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_list_properties_cookie_t
xcb_selinux_list_properties_unchecked (xcb_connection_t *c  /**< */,
                                       xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_LIST_PROPERTIES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_list_properties_cookie_t xcb_ret;
    xcb_selinux_list_properties_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_selinux_list_properties_properties_length (const xcb_selinux_list_properties_reply_t *R  /**< */)
{
    return R->properties_len;
}

xcb_selinux_list_item_iterator_t
xcb_selinux_list_properties_properties_iterator (const xcb_selinux_list_properties_reply_t *R  /**< */)
{
    xcb_selinux_list_item_iterator_t i;
    i.data = (xcb_selinux_list_item_t *) (R + 1);
    i.rem = R->properties_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_list_properties_reply_t *
xcb_selinux_list_properties_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_selinux_list_properties_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_selinux_list_properties_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_set_selection_create_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_set_selection_create_context_request_t *_aux = (xcb_selinux_set_selection_create_context_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_set_selection_create_context_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_void_cookie_t
xcb_selinux_set_selection_create_context_checked (xcb_connection_t *c  /**< */,
                                                  uint32_t          context_len  /**< */,
                                                  const char       *context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_SET_SELECTION_CREATE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_selinux_set_selection_create_context_request_t xcb_out;

    xcb_out.context_len = context_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char context */
    xcb_parts[4].iov_base = (char *) context;
    xcb_parts[4].iov_len = context_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_selinux_set_selection_create_context (xcb_connection_t *c  /**< */,
                                          uint32_t          context_len  /**< */,
                                          const char       *context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_SET_SELECTION_CREATE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_selinux_set_selection_create_context_request_t xcb_out;

    xcb_out.context_len = context_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char context */
    xcb_parts[4].iov_base = (char *) context;
    xcb_parts[4].iov_len = context_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_selinux_get_selection_create_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_get_selection_create_context_reply_t *_aux = (xcb_selinux_get_selection_create_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_get_selection_create_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_get_selection_create_context_cookie_t
xcb_selinux_get_selection_create_context (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_SELECTION_CREATE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_selection_create_context_cookie_t xcb_ret;
    xcb_selinux_get_selection_create_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_get_selection_create_context_cookie_t
xcb_selinux_get_selection_create_context_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_SELECTION_CREATE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_selection_create_context_cookie_t xcb_ret;
    xcb_selinux_get_selection_create_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_selinux_get_selection_create_context_context (const xcb_selinux_get_selection_create_context_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_selinux_get_selection_create_context_context_length (const xcb_selinux_get_selection_create_context_reply_t *R  /**< */)
{
    return R->context_len;
}

xcb_generic_iterator_t
xcb_selinux_get_selection_create_context_context_end (const xcb_selinux_get_selection_create_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_get_selection_create_context_reply_t *
xcb_selinux_get_selection_create_context_reply (xcb_connection_t                                   *c  /**< */,
                                                xcb_selinux_get_selection_create_context_cookie_t   cookie  /**< */,
                                                xcb_generic_error_t                               **e  /**< */)
{
    return (xcb_selinux_get_selection_create_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_set_selection_use_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_set_selection_use_context_request_t *_aux = (xcb_selinux_set_selection_use_context_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_set_selection_use_context_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_void_cookie_t
xcb_selinux_set_selection_use_context_checked (xcb_connection_t *c  /**< */,
                                               uint32_t          context_len  /**< */,
                                               const char       *context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_SET_SELECTION_USE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_selinux_set_selection_use_context_request_t xcb_out;

    xcb_out.context_len = context_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char context */
    xcb_parts[4].iov_base = (char *) context;
    xcb_parts[4].iov_len = context_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_selinux_set_selection_use_context (xcb_connection_t *c  /**< */,
                                       uint32_t          context_len  /**< */,
                                       const char       *context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_SET_SELECTION_USE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_selinux_set_selection_use_context_request_t xcb_out;

    xcb_out.context_len = context_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char context */
    xcb_parts[4].iov_base = (char *) context;
    xcb_parts[4].iov_len = context_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_selinux_get_selection_use_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_get_selection_use_context_reply_t *_aux = (xcb_selinux_get_selection_use_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_get_selection_use_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_get_selection_use_context_cookie_t
xcb_selinux_get_selection_use_context (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_SELECTION_USE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_selection_use_context_cookie_t xcb_ret;
    xcb_selinux_get_selection_use_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_get_selection_use_context_cookie_t
xcb_selinux_get_selection_use_context_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_SELECTION_USE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_selection_use_context_cookie_t xcb_ret;
    xcb_selinux_get_selection_use_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_selinux_get_selection_use_context_context (const xcb_selinux_get_selection_use_context_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_selinux_get_selection_use_context_context_length (const xcb_selinux_get_selection_use_context_reply_t *R  /**< */)
{
    return R->context_len;
}

xcb_generic_iterator_t
xcb_selinux_get_selection_use_context_context_end (const xcb_selinux_get_selection_use_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_get_selection_use_context_reply_t *
xcb_selinux_get_selection_use_context_reply (xcb_connection_t                                *c  /**< */,
                                             xcb_selinux_get_selection_use_context_cookie_t   cookie  /**< */,
                                             xcb_generic_error_t                            **e  /**< */)
{
    return (xcb_selinux_get_selection_use_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_get_selection_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_get_selection_context_reply_t *_aux = (xcb_selinux_get_selection_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_get_selection_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_get_selection_context_cookie_t
xcb_selinux_get_selection_context (xcb_connection_t *c  /**< */,
                                   xcb_atom_t        selection  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_SELECTION_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_selection_context_cookie_t xcb_ret;
    xcb_selinux_get_selection_context_request_t xcb_out;

    xcb_out.selection = selection;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_get_selection_context_cookie_t
xcb_selinux_get_selection_context_unchecked (xcb_connection_t *c  /**< */,
                                             xcb_atom_t        selection  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_SELECTION_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_selection_context_cookie_t xcb_ret;
    xcb_selinux_get_selection_context_request_t xcb_out;

    xcb_out.selection = selection;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_selinux_get_selection_context_context (const xcb_selinux_get_selection_context_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_selinux_get_selection_context_context_length (const xcb_selinux_get_selection_context_reply_t *R  /**< */)
{
    return R->context_len;
}

xcb_generic_iterator_t
xcb_selinux_get_selection_context_context_end (const xcb_selinux_get_selection_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_get_selection_context_reply_t *
xcb_selinux_get_selection_context_reply (xcb_connection_t                            *c  /**< */,
                                         xcb_selinux_get_selection_context_cookie_t   cookie  /**< */,
                                         xcb_generic_error_t                        **e  /**< */)
{
    return (xcb_selinux_get_selection_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_get_selection_data_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_get_selection_data_context_reply_t *_aux = (xcb_selinux_get_selection_data_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_get_selection_data_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_get_selection_data_context_cookie_t
xcb_selinux_get_selection_data_context (xcb_connection_t *c  /**< */,
                                        xcb_atom_t        selection  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_SELECTION_DATA_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_selection_data_context_cookie_t xcb_ret;
    xcb_selinux_get_selection_data_context_request_t xcb_out;

    xcb_out.selection = selection;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_get_selection_data_context_cookie_t
xcb_selinux_get_selection_data_context_unchecked (xcb_connection_t *c  /**< */,
                                                  xcb_atom_t        selection  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_SELECTION_DATA_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_selection_data_context_cookie_t xcb_ret;
    xcb_selinux_get_selection_data_context_request_t xcb_out;

    xcb_out.selection = selection;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_selinux_get_selection_data_context_context (const xcb_selinux_get_selection_data_context_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_selinux_get_selection_data_context_context_length (const xcb_selinux_get_selection_data_context_reply_t *R  /**< */)
{
    return R->context_len;
}

xcb_generic_iterator_t
xcb_selinux_get_selection_data_context_context_end (const xcb_selinux_get_selection_data_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_get_selection_data_context_reply_t *
xcb_selinux_get_selection_data_context_reply (xcb_connection_t                                 *c  /**< */,
                                              xcb_selinux_get_selection_data_context_cookie_t   cookie  /**< */,
                                              xcb_generic_error_t                             **e  /**< */)
{
    return (xcb_selinux_get_selection_data_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_list_selections_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_list_selections_reply_t *_aux = (xcb_selinux_list_selections_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_selinux_list_selections_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* selections */
    for(i=0; i<_aux->selections_len; i++) {
        xcb_tmp_len = xcb_selinux_list_item_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_selinux_list_item_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_list_selections_cookie_t
xcb_selinux_list_selections (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_LIST_SELECTIONS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_list_selections_cookie_t xcb_ret;
    xcb_selinux_list_selections_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_list_selections_cookie_t
xcb_selinux_list_selections_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_LIST_SELECTIONS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_list_selections_cookie_t xcb_ret;
    xcb_selinux_list_selections_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_selinux_list_selections_selections_length (const xcb_selinux_list_selections_reply_t *R  /**< */)
{
    return R->selections_len;
}

xcb_selinux_list_item_iterator_t
xcb_selinux_list_selections_selections_iterator (const xcb_selinux_list_selections_reply_t *R  /**< */)
{
    xcb_selinux_list_item_iterator_t i;
    i.data = (xcb_selinux_list_item_t *) (R + 1);
    i.rem = R->selections_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_list_selections_reply_t *
xcb_selinux_list_selections_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_selinux_list_selections_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_selinux_list_selections_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_selinux_get_client_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_selinux_get_client_context_reply_t *_aux = (xcb_selinux_get_client_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_selinux_get_client_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* context */
    xcb_block_len += _aux->context_len * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    return xcb_buffer_len;
}

xcb_selinux_get_client_context_cookie_t
xcb_selinux_get_client_context (xcb_connection_t *c  /**< */,
                                uint32_t          resource  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_CLIENT_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_client_context_cookie_t xcb_ret;
    xcb_selinux_get_client_context_request_t xcb_out;

    xcb_out.resource = resource;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_selinux_get_client_context_cookie_t
xcb_selinux_get_client_context_unchecked (xcb_connection_t *c  /**< */,
                                          uint32_t          resource  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_selinux_id,
        /* opcode */ XCB_SELINUX_GET_CLIENT_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_selinux_get_client_context_cookie_t xcb_ret;
    xcb_selinux_get_client_context_request_t xcb_out;

    xcb_out.resource = resource;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_selinux_get_client_context_context (const xcb_selinux_get_client_context_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_selinux_get_client_context_context_length (const xcb_selinux_get_client_context_reply_t *R  /**< */)
{
    return R->context_len;
}

xcb_generic_iterator_t
xcb_selinux_get_client_context_context_end (const xcb_selinux_get_client_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->context_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_selinux_get_client_context_reply_t *
xcb_selinux_get_client_context_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_selinux_get_client_context_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */)
{
    return (xcb_selinux_get_client_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

