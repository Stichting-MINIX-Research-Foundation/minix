/*
 * This file generated automatically from dri3.xml by c_client.py.
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
#include "dri3.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xproto.h"

xcb_extension_t xcb_dri3_id = { "DRI3", 0 };

xcb_dri3_query_version_cookie_t
xcb_dri3_query_version (xcb_connection_t *c  /**< */,
                        uint32_t          major_version  /**< */,
                        uint32_t          minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dri3_id,
        /* opcode */ XCB_DRI3_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dri3_query_version_cookie_t xcb_ret;
    xcb_dri3_query_version_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dri3_query_version_cookie_t
xcb_dri3_query_version_unchecked (xcb_connection_t *c  /**< */,
                                  uint32_t          major_version  /**< */,
                                  uint32_t          minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dri3_id,
        /* opcode */ XCB_DRI3_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dri3_query_version_cookie_t xcb_ret;
    xcb_dri3_query_version_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dri3_query_version_reply_t *
xcb_dri3_query_version_reply (xcb_connection_t                 *c  /**< */,
                              xcb_dri3_query_version_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_dri3_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_dri3_open_cookie_t
xcb_dri3_open (xcb_connection_t *c  /**< */,
               xcb_drawable_t    drawable  /**< */,
               uint32_t          provider  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dri3_id,
        /* opcode */ XCB_DRI3_OPEN,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dri3_open_cookie_t xcb_ret;
    xcb_dri3_open_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.provider = provider;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED|XCB_REQUEST_REPLY_FDS, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dri3_open_cookie_t
xcb_dri3_open_unchecked (xcb_connection_t *c  /**< */,
                         xcb_drawable_t    drawable  /**< */,
                         uint32_t          provider  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dri3_id,
        /* opcode */ XCB_DRI3_OPEN,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dri3_open_cookie_t xcb_ret;
    xcb_dri3_open_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.provider = provider;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_REPLY_FDS, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dri3_open_reply_t *
xcb_dri3_open_reply (xcb_connection_t        *c  /**< */,
                     xcb_dri3_open_cookie_t   cookie  /**< */,
                     xcb_generic_error_t    **e  /**< */)
{
    return (xcb_dri3_open_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int *
xcb_dri3_open_reply_fds (xcb_connection_t       *c  /**< */,
                         xcb_dri3_open_reply_t  *reply  /**< */)
{
    return xcb_get_reply_fds(c, reply, sizeof(xcb_dri3_open_reply_t) + 4 * reply->length);
}

xcb_void_cookie_t
xcb_dri3_pixmap_from_buffer_checked (xcb_connection_t *c  /**< */,
                                     xcb_pixmap_t      pixmap  /**< */,
                                     xcb_drawable_t    drawable  /**< */,
                                     uint32_t          size  /**< */,
                                     uint16_t          width  /**< */,
                                     uint16_t          height  /**< */,
                                     uint16_t          stride  /**< */,
                                     uint8_t           depth  /**< */,
                                     uint8_t           bpp  /**< */,
                                     int32_t           pixmap_fd  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dri3_id,
        /* opcode */ XCB_DRI3_PIXMAP_FROM_BUFFER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_dri3_pixmap_from_buffer_request_t xcb_out;

    xcb_out.pixmap = pixmap;
    xcb_out.drawable = drawable;
    xcb_out.size = size;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.stride = stride;
    xcb_out.depth = depth;
    xcb_out.bpp = bpp;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_send_fd(c, pixmap_fd);
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_dri3_pixmap_from_buffer (xcb_connection_t *c  /**< */,
                             xcb_pixmap_t      pixmap  /**< */,
                             xcb_drawable_t    drawable  /**< */,
                             uint32_t          size  /**< */,
                             uint16_t          width  /**< */,
                             uint16_t          height  /**< */,
                             uint16_t          stride  /**< */,
                             uint8_t           depth  /**< */,
                             uint8_t           bpp  /**< */,
                             int32_t           pixmap_fd  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dri3_id,
        /* opcode */ XCB_DRI3_PIXMAP_FROM_BUFFER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_dri3_pixmap_from_buffer_request_t xcb_out;

    xcb_out.pixmap = pixmap;
    xcb_out.drawable = drawable;
    xcb_out.size = size;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.stride = stride;
    xcb_out.depth = depth;
    xcb_out.bpp = bpp;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_send_fd(c, pixmap_fd);
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dri3_buffer_from_pixmap_cookie_t
xcb_dri3_buffer_from_pixmap (xcb_connection_t *c  /**< */,
                             xcb_pixmap_t      pixmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dri3_id,
        /* opcode */ XCB_DRI3_BUFFER_FROM_PIXMAP,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dri3_buffer_from_pixmap_cookie_t xcb_ret;
    xcb_dri3_buffer_from_pixmap_request_t xcb_out;

    xcb_out.pixmap = pixmap;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED|XCB_REQUEST_REPLY_FDS, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dri3_buffer_from_pixmap_cookie_t
xcb_dri3_buffer_from_pixmap_unchecked (xcb_connection_t *c  /**< */,
                                       xcb_pixmap_t      pixmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dri3_id,
        /* opcode */ XCB_DRI3_BUFFER_FROM_PIXMAP,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dri3_buffer_from_pixmap_cookie_t xcb_ret;
    xcb_dri3_buffer_from_pixmap_request_t xcb_out;

    xcb_out.pixmap = pixmap;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_REPLY_FDS, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dri3_buffer_from_pixmap_reply_t *
xcb_dri3_buffer_from_pixmap_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_dri3_buffer_from_pixmap_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_dri3_buffer_from_pixmap_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int *
xcb_dri3_buffer_from_pixmap_reply_fds (xcb_connection_t                     *c  /**< */,
                                       xcb_dri3_buffer_from_pixmap_reply_t  *reply  /**< */)
{
    return xcb_get_reply_fds(c, reply, sizeof(xcb_dri3_buffer_from_pixmap_reply_t) + 4 * reply->length);
}

xcb_void_cookie_t
xcb_dri3_fence_from_fd_checked (xcb_connection_t *c  /**< */,
                                xcb_drawable_t    drawable  /**< */,
                                uint32_t          fence  /**< */,
                                uint8_t           initially_triggered  /**< */,
                                int32_t           fence_fd  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dri3_id,
        /* opcode */ XCB_DRI3_FENCE_FROM_FD,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_dri3_fence_from_fd_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.fence = fence;
    xcb_out.initially_triggered = initially_triggered;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_send_fd(c, fence_fd);
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_dri3_fence_from_fd (xcb_connection_t *c  /**< */,
                        xcb_drawable_t    drawable  /**< */,
                        uint32_t          fence  /**< */,
                        uint8_t           initially_triggered  /**< */,
                        int32_t           fence_fd  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dri3_id,
        /* opcode */ XCB_DRI3_FENCE_FROM_FD,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_dri3_fence_from_fd_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.fence = fence;
    xcb_out.initially_triggered = initially_triggered;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_send_fd(c, fence_fd);
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dri3_fd_from_fence_cookie_t
xcb_dri3_fd_from_fence (xcb_connection_t *c  /**< */,
                        xcb_drawable_t    drawable  /**< */,
                        uint32_t          fence  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dri3_id,
        /* opcode */ XCB_DRI3_FD_FROM_FENCE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dri3_fd_from_fence_cookie_t xcb_ret;
    xcb_dri3_fd_from_fence_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.fence = fence;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED|XCB_REQUEST_REPLY_FDS, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dri3_fd_from_fence_cookie_t
xcb_dri3_fd_from_fence_unchecked (xcb_connection_t *c  /**< */,
                                  xcb_drawable_t    drawable  /**< */,
                                  uint32_t          fence  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dri3_id,
        /* opcode */ XCB_DRI3_FD_FROM_FENCE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dri3_fd_from_fence_cookie_t xcb_ret;
    xcb_dri3_fd_from_fence_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.fence = fence;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_REPLY_FDS, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dri3_fd_from_fence_reply_t *
xcb_dri3_fd_from_fence_reply (xcb_connection_t                 *c  /**< */,
                              xcb_dri3_fd_from_fence_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_dri3_fd_from_fence_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int *
xcb_dri3_fd_from_fence_reply_fds (xcb_connection_t                *c  /**< */,
                                  xcb_dri3_fd_from_fence_reply_t  *reply  /**< */)
{
    return xcb_get_reply_fds(c, reply, sizeof(xcb_dri3_fd_from_fence_reply_t) + 4 * reply->length);
}

