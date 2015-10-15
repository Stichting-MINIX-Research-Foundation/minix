/*
 * This file generated automatically from xf86dri.xml by c_client.py.
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
#include "xf86dri.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)

xcb_extension_t xcb_xf86dri_id = { "XFree86-DRI", 0 };

void
xcb_xf86dri_drm_clip_rect_next (xcb_xf86dri_drm_clip_rect_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_xf86dri_drm_clip_rect_t);
}

xcb_generic_iterator_t
xcb_xf86dri_drm_clip_rect_end (xcb_xf86dri_drm_clip_rect_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

xcb_xf86dri_query_version_cookie_t
xcb_xf86dri_query_version (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_query_version_cookie_t xcb_ret;
    xcb_xf86dri_query_version_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_query_version_cookie_t
xcb_xf86dri_query_version_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_query_version_cookie_t xcb_ret;
    xcb_xf86dri_query_version_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_query_version_reply_t *
xcb_xf86dri_query_version_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_xf86dri_query_version_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */)
{
    return (xcb_xf86dri_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_xf86dri_query_direct_rendering_capable_cookie_t
xcb_xf86dri_query_direct_rendering_capable (xcb_connection_t *c  /**< */,
                                            uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_QUERY_DIRECT_RENDERING_CAPABLE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_query_direct_rendering_capable_cookie_t xcb_ret;
    xcb_xf86dri_query_direct_rendering_capable_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_query_direct_rendering_capable_cookie_t
xcb_xf86dri_query_direct_rendering_capable_unchecked (xcb_connection_t *c  /**< */,
                                                      uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_QUERY_DIRECT_RENDERING_CAPABLE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_query_direct_rendering_capable_cookie_t xcb_ret;
    xcb_xf86dri_query_direct_rendering_capable_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_query_direct_rendering_capable_reply_t *
xcb_xf86dri_query_direct_rendering_capable_reply (xcb_connection_t                                     *c  /**< */,
                                                  xcb_xf86dri_query_direct_rendering_capable_cookie_t   cookie  /**< */,
                                                  xcb_generic_error_t                                 **e  /**< */)
{
    return (xcb_xf86dri_query_direct_rendering_capable_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_xf86dri_open_connection_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xf86dri_open_connection_reply_t *_aux = (xcb_xf86dri_open_connection_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xf86dri_open_connection_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* bus_id */
    xcb_block_len += _aux->bus_id_len * sizeof(char);
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

xcb_xf86dri_open_connection_cookie_t
xcb_xf86dri_open_connection (xcb_connection_t *c  /**< */,
                             uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_OPEN_CONNECTION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_open_connection_cookie_t xcb_ret;
    xcb_xf86dri_open_connection_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_open_connection_cookie_t
xcb_xf86dri_open_connection_unchecked (xcb_connection_t *c  /**< */,
                                       uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_OPEN_CONNECTION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_open_connection_cookie_t xcb_ret;
    xcb_xf86dri_open_connection_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_xf86dri_open_connection_bus_id (const xcb_xf86dri_open_connection_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_xf86dri_open_connection_bus_id_length (const xcb_xf86dri_open_connection_reply_t *R  /**< */)
{
    return R->bus_id_len;
}

xcb_generic_iterator_t
xcb_xf86dri_open_connection_bus_id_end (const xcb_xf86dri_open_connection_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->bus_id_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xf86dri_open_connection_reply_t *
xcb_xf86dri_open_connection_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_xf86dri_open_connection_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_xf86dri_open_connection_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_xf86dri_close_connection_checked (xcb_connection_t *c  /**< */,
                                      uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_CLOSE_CONNECTION,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xf86dri_close_connection_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xf86dri_close_connection (xcb_connection_t *c  /**< */,
                              uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_CLOSE_CONNECTION,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xf86dri_close_connection_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_xf86dri_get_client_driver_name_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xf86dri_get_client_driver_name_reply_t *_aux = (xcb_xf86dri_get_client_driver_name_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xf86dri_get_client_driver_name_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* client_driver_name */
    xcb_block_len += _aux->client_driver_name_len * sizeof(char);
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

xcb_xf86dri_get_client_driver_name_cookie_t
xcb_xf86dri_get_client_driver_name (xcb_connection_t *c  /**< */,
                                    uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_GET_CLIENT_DRIVER_NAME,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_get_client_driver_name_cookie_t xcb_ret;
    xcb_xf86dri_get_client_driver_name_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_get_client_driver_name_cookie_t
xcb_xf86dri_get_client_driver_name_unchecked (xcb_connection_t *c  /**< */,
                                              uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_GET_CLIENT_DRIVER_NAME,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_get_client_driver_name_cookie_t xcb_ret;
    xcb_xf86dri_get_client_driver_name_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_xf86dri_get_client_driver_name_client_driver_name (const xcb_xf86dri_get_client_driver_name_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_xf86dri_get_client_driver_name_client_driver_name_length (const xcb_xf86dri_get_client_driver_name_reply_t *R  /**< */)
{
    return R->client_driver_name_len;
}

xcb_generic_iterator_t
xcb_xf86dri_get_client_driver_name_client_driver_name_end (const xcb_xf86dri_get_client_driver_name_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->client_driver_name_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xf86dri_get_client_driver_name_reply_t *
xcb_xf86dri_get_client_driver_name_reply (xcb_connection_t                             *c  /**< */,
                                          xcb_xf86dri_get_client_driver_name_cookie_t   cookie  /**< */,
                                          xcb_generic_error_t                         **e  /**< */)
{
    return (xcb_xf86dri_get_client_driver_name_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_xf86dri_create_context_cookie_t
xcb_xf86dri_create_context (xcb_connection_t *c  /**< */,
                            uint32_t          screen  /**< */,
                            uint32_t          visual  /**< */,
                            uint32_t          context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_CREATE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_create_context_cookie_t xcb_ret;
    xcb_xf86dri_create_context_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.visual = visual;
    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_create_context_cookie_t
xcb_xf86dri_create_context_unchecked (xcb_connection_t *c  /**< */,
                                      uint32_t          screen  /**< */,
                                      uint32_t          visual  /**< */,
                                      uint32_t          context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_CREATE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_create_context_cookie_t xcb_ret;
    xcb_xf86dri_create_context_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.visual = visual;
    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_create_context_reply_t *
xcb_xf86dri_create_context_reply (xcb_connection_t                     *c  /**< */,
                                  xcb_xf86dri_create_context_cookie_t   cookie  /**< */,
                                  xcb_generic_error_t                 **e  /**< */)
{
    return (xcb_xf86dri_create_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_xf86dri_destroy_context_checked (xcb_connection_t *c  /**< */,
                                     uint32_t          screen  /**< */,
                                     uint32_t          context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_DESTROY_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xf86dri_destroy_context_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xf86dri_destroy_context (xcb_connection_t *c  /**< */,
                             uint32_t          screen  /**< */,
                             uint32_t          context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_DESTROY_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xf86dri_destroy_context_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_create_drawable_cookie_t
xcb_xf86dri_create_drawable (xcb_connection_t *c  /**< */,
                             uint32_t          screen  /**< */,
                             uint32_t          drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_CREATE_DRAWABLE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_create_drawable_cookie_t xcb_ret;
    xcb_xf86dri_create_drawable_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.drawable = drawable;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_create_drawable_cookie_t
xcb_xf86dri_create_drawable_unchecked (xcb_connection_t *c  /**< */,
                                       uint32_t          screen  /**< */,
                                       uint32_t          drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_CREATE_DRAWABLE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_create_drawable_cookie_t xcb_ret;
    xcb_xf86dri_create_drawable_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.drawable = drawable;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_create_drawable_reply_t *
xcb_xf86dri_create_drawable_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_xf86dri_create_drawable_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_xf86dri_create_drawable_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_xf86dri_destroy_drawable_checked (xcb_connection_t *c  /**< */,
                                      uint32_t          screen  /**< */,
                                      uint32_t          drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_DESTROY_DRAWABLE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xf86dri_destroy_drawable_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.drawable = drawable;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xf86dri_destroy_drawable (xcb_connection_t *c  /**< */,
                              uint32_t          screen  /**< */,
                              uint32_t          drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_DESTROY_DRAWABLE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xf86dri_destroy_drawable_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.drawable = drawable;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_xf86dri_get_drawable_info_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xf86dri_get_drawable_info_reply_t *_aux = (xcb_xf86dri_get_drawable_info_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xf86dri_get_drawable_info_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* clip_rects */
    xcb_block_len += _aux->num_clip_rects * sizeof(xcb_xf86dri_drm_clip_rect_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_xf86dri_drm_clip_rect_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* back_clip_rects */
    xcb_block_len += _aux->num_back_clip_rects * sizeof(xcb_xf86dri_drm_clip_rect_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_xf86dri_drm_clip_rect_t);
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

xcb_xf86dri_get_drawable_info_cookie_t
xcb_xf86dri_get_drawable_info (xcb_connection_t *c  /**< */,
                               uint32_t          screen  /**< */,
                               uint32_t          drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_GET_DRAWABLE_INFO,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_get_drawable_info_cookie_t xcb_ret;
    xcb_xf86dri_get_drawable_info_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.drawable = drawable;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_get_drawable_info_cookie_t
xcb_xf86dri_get_drawable_info_unchecked (xcb_connection_t *c  /**< */,
                                         uint32_t          screen  /**< */,
                                         uint32_t          drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_GET_DRAWABLE_INFO,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_get_drawable_info_cookie_t xcb_ret;
    xcb_xf86dri_get_drawable_info_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.drawable = drawable;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_drm_clip_rect_t *
xcb_xf86dri_get_drawable_info_clip_rects (const xcb_xf86dri_get_drawable_info_reply_t *R  /**< */)
{
    return (xcb_xf86dri_drm_clip_rect_t *) (R + 1);
}

int
xcb_xf86dri_get_drawable_info_clip_rects_length (const xcb_xf86dri_get_drawable_info_reply_t *R  /**< */)
{
    return R->num_clip_rects;
}

xcb_xf86dri_drm_clip_rect_iterator_t
xcb_xf86dri_get_drawable_info_clip_rects_iterator (const xcb_xf86dri_get_drawable_info_reply_t *R  /**< */)
{
    xcb_xf86dri_drm_clip_rect_iterator_t i;
    i.data = (xcb_xf86dri_drm_clip_rect_t *) (R + 1);
    i.rem = R->num_clip_rects;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xf86dri_drm_clip_rect_t *
xcb_xf86dri_get_drawable_info_back_clip_rects (const xcb_xf86dri_get_drawable_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_xf86dri_drm_clip_rect_end(xcb_xf86dri_get_drawable_info_clip_rects_iterator(R));
    return (xcb_xf86dri_drm_clip_rect_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_xf86dri_drm_clip_rect_t, prev.index) + 0);
}

int
xcb_xf86dri_get_drawable_info_back_clip_rects_length (const xcb_xf86dri_get_drawable_info_reply_t *R  /**< */)
{
    return R->num_back_clip_rects;
}

xcb_xf86dri_drm_clip_rect_iterator_t
xcb_xf86dri_get_drawable_info_back_clip_rects_iterator (const xcb_xf86dri_get_drawable_info_reply_t *R  /**< */)
{
    xcb_xf86dri_drm_clip_rect_iterator_t i;
    xcb_generic_iterator_t prev = xcb_xf86dri_drm_clip_rect_end(xcb_xf86dri_get_drawable_info_clip_rects_iterator(R));
    i.data = (xcb_xf86dri_drm_clip_rect_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_xf86dri_drm_clip_rect_t, prev.index));
    i.rem = R->num_back_clip_rects;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xf86dri_get_drawable_info_reply_t *
xcb_xf86dri_get_drawable_info_reply (xcb_connection_t                        *c  /**< */,
                                     xcb_xf86dri_get_drawable_info_cookie_t   cookie  /**< */,
                                     xcb_generic_error_t                    **e  /**< */)
{
    return (xcb_xf86dri_get_drawable_info_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_xf86dri_get_device_info_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xf86dri_get_device_info_reply_t *_aux = (xcb_xf86dri_get_device_info_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xf86dri_get_device_info_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* device_private */
    xcb_block_len += _aux->device_private_size * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint32_t);
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

xcb_xf86dri_get_device_info_cookie_t
xcb_xf86dri_get_device_info (xcb_connection_t *c  /**< */,
                             uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_GET_DEVICE_INFO,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_get_device_info_cookie_t xcb_ret;
    xcb_xf86dri_get_device_info_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_get_device_info_cookie_t
xcb_xf86dri_get_device_info_unchecked (xcb_connection_t *c  /**< */,
                                       uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_GET_DEVICE_INFO,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_get_device_info_cookie_t xcb_ret;
    xcb_xf86dri_get_device_info_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_xf86dri_get_device_info_device_private (const xcb_xf86dri_get_device_info_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_xf86dri_get_device_info_device_private_length (const xcb_xf86dri_get_device_info_reply_t *R  /**< */)
{
    return R->device_private_size;
}

xcb_generic_iterator_t
xcb_xf86dri_get_device_info_device_private_end (const xcb_xf86dri_get_device_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->device_private_size);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xf86dri_get_device_info_reply_t *
xcb_xf86dri_get_device_info_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_xf86dri_get_device_info_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_xf86dri_get_device_info_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_xf86dri_auth_connection_cookie_t
xcb_xf86dri_auth_connection (xcb_connection_t *c  /**< */,
                             uint32_t          screen  /**< */,
                             uint32_t          magic  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_AUTH_CONNECTION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_auth_connection_cookie_t xcb_ret;
    xcb_xf86dri_auth_connection_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.magic = magic;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_auth_connection_cookie_t
xcb_xf86dri_auth_connection_unchecked (xcb_connection_t *c  /**< */,
                                       uint32_t          screen  /**< */,
                                       uint32_t          magic  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xf86dri_id,
        /* opcode */ XCB_XF86DRI_AUTH_CONNECTION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xf86dri_auth_connection_cookie_t xcb_ret;
    xcb_xf86dri_auth_connection_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.magic = magic;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xf86dri_auth_connection_reply_t *
xcb_xf86dri_auth_connection_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_xf86dri_auth_connection_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_xf86dri_auth_connection_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

