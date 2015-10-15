/*
 * This file generated automatically from glx.xml by c_client.py.
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
#include "glx.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xproto.h"

xcb_extension_t xcb_glx_id = { "GLX", 0 };

void
xcb_glx_pixmap_next (xcb_glx_pixmap_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_glx_pixmap_t);
}

xcb_generic_iterator_t
xcb_glx_pixmap_end (xcb_glx_pixmap_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_glx_context_next (xcb_glx_context_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_glx_context_t);
}

xcb_generic_iterator_t
xcb_glx_context_end (xcb_glx_context_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_glx_pbuffer_next (xcb_glx_pbuffer_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_glx_pbuffer_t);
}

xcb_generic_iterator_t
xcb_glx_pbuffer_end (xcb_glx_pbuffer_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_glx_window_next (xcb_glx_window_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_glx_window_t);
}

xcb_generic_iterator_t
xcb_glx_window_end (xcb_glx_window_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_glx_fbconfig_next (xcb_glx_fbconfig_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_glx_fbconfig_t);
}

xcb_generic_iterator_t
xcb_glx_fbconfig_end (xcb_glx_fbconfig_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_glx_drawable_next (xcb_glx_drawable_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_glx_drawable_t);
}

xcb_generic_iterator_t
xcb_glx_drawable_end (xcb_glx_drawable_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_glx_float32_next (xcb_glx_float32_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_glx_float32_t);
}

xcb_generic_iterator_t
xcb_glx_float32_end (xcb_glx_float32_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_glx_float64_next (xcb_glx_float64_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_glx_float64_t);
}

xcb_generic_iterator_t
xcb_glx_float64_end (xcb_glx_float64_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_glx_bool32_next (xcb_glx_bool32_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_glx_bool32_t);
}

xcb_generic_iterator_t
xcb_glx_bool32_end (xcb_glx_bool32_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_glx_context_tag_next (xcb_glx_context_tag_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_glx_context_tag_t);
}

xcb_generic_iterator_t
xcb_glx_context_tag_end (xcb_glx_context_tag_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_glx_render_sizeof (const void  *_buffer  /**< */,
                       uint32_t     data_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_render_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += data_len * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
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
xcb_glx_render_checked (xcb_connection_t      *c  /**< */,
                        xcb_glx_context_tag_t  context_tag  /**< */,
                        uint32_t               data_len  /**< */,
                        const uint8_t         *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_RENDER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_render_request_t xcb_out;

    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = data_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_render (xcb_connection_t      *c  /**< */,
                xcb_glx_context_tag_t  context_tag  /**< */,
                uint32_t               data_len  /**< */,
                const uint8_t         *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_RENDER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_render_request_t xcb_out;

    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = data_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_render_large_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_render_large_request_t *_aux = (xcb_glx_render_large_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_render_large_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->data_len * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
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
xcb_glx_render_large_checked (xcb_connection_t      *c  /**< */,
                              xcb_glx_context_tag_t  context_tag  /**< */,
                              uint16_t               request_num  /**< */,
                              uint16_t               request_total  /**< */,
                              uint32_t               data_len  /**< */,
                              const uint8_t         *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_RENDER_LARGE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_render_large_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.request_num = request_num;
    xcb_out.request_total = request_total;
    xcb_out.data_len = data_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = data_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_render_large (xcb_connection_t      *c  /**< */,
                      xcb_glx_context_tag_t  context_tag  /**< */,
                      uint16_t               request_num  /**< */,
                      uint16_t               request_total  /**< */,
                      uint32_t               data_len  /**< */,
                      const uint8_t         *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_RENDER_LARGE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_render_large_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.request_num = request_num;
    xcb_out.request_total = request_total;
    xcb_out.data_len = data_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = data_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_create_context_checked (xcb_connection_t  *c  /**< */,
                                xcb_glx_context_t  context  /**< */,
                                xcb_visualid_t     visual  /**< */,
                                uint32_t           screen  /**< */,
                                xcb_glx_context_t  share_list  /**< */,
                                uint8_t            is_direct  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CREATE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_create_context_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.visual = visual;
    xcb_out.screen = screen;
    xcb_out.share_list = share_list;
    xcb_out.is_direct = is_direct;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_create_context (xcb_connection_t  *c  /**< */,
                        xcb_glx_context_t  context  /**< */,
                        xcb_visualid_t     visual  /**< */,
                        uint32_t           screen  /**< */,
                        xcb_glx_context_t  share_list  /**< */,
                        uint8_t            is_direct  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CREATE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_create_context_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.visual = visual;
    xcb_out.screen = screen;
    xcb_out.share_list = share_list;
    xcb_out.is_direct = is_direct;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_destroy_context_checked (xcb_connection_t  *c  /**< */,
                                 xcb_glx_context_t  context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DESTROY_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_destroy_context_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_destroy_context (xcb_connection_t  *c  /**< */,
                         xcb_glx_context_t  context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DESTROY_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_destroy_context_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_make_current_cookie_t
xcb_glx_make_current (xcb_connection_t      *c  /**< */,
                      xcb_glx_drawable_t     drawable  /**< */,
                      xcb_glx_context_t      context  /**< */,
                      xcb_glx_context_tag_t  old_context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_MAKE_CURRENT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_make_current_cookie_t xcb_ret;
    xcb_glx_make_current_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.context = context;
    xcb_out.old_context_tag = old_context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_make_current_cookie_t
xcb_glx_make_current_unchecked (xcb_connection_t      *c  /**< */,
                                xcb_glx_drawable_t     drawable  /**< */,
                                xcb_glx_context_t      context  /**< */,
                                xcb_glx_context_tag_t  old_context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_MAKE_CURRENT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_make_current_cookie_t xcb_ret;
    xcb_glx_make_current_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.context = context;
    xcb_out.old_context_tag = old_context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_make_current_reply_t *
xcb_glx_make_current_reply (xcb_connection_t               *c  /**< */,
                            xcb_glx_make_current_cookie_t   cookie  /**< */,
                            xcb_generic_error_t           **e  /**< */)
{
    return (xcb_glx_make_current_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_glx_is_direct_cookie_t
xcb_glx_is_direct (xcb_connection_t  *c  /**< */,
                   xcb_glx_context_t  context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_IS_DIRECT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_is_direct_cookie_t xcb_ret;
    xcb_glx_is_direct_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_is_direct_cookie_t
xcb_glx_is_direct_unchecked (xcb_connection_t  *c  /**< */,
                             xcb_glx_context_t  context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_IS_DIRECT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_is_direct_cookie_t xcb_ret;
    xcb_glx_is_direct_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_is_direct_reply_t *
xcb_glx_is_direct_reply (xcb_connection_t            *c  /**< */,
                         xcb_glx_is_direct_cookie_t   cookie  /**< */,
                         xcb_generic_error_t        **e  /**< */)
{
    return (xcb_glx_is_direct_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_glx_query_version_cookie_t
xcb_glx_query_version (xcb_connection_t *c  /**< */,
                       uint32_t          major_version  /**< */,
                       uint32_t          minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_query_version_cookie_t xcb_ret;
    xcb_glx_query_version_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_query_version_cookie_t
xcb_glx_query_version_unchecked (xcb_connection_t *c  /**< */,
                                 uint32_t          major_version  /**< */,
                                 uint32_t          minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_query_version_cookie_t xcb_ret;
    xcb_glx_query_version_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_query_version_reply_t *
xcb_glx_query_version_reply (xcb_connection_t                *c  /**< */,
                             xcb_glx_query_version_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_glx_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_glx_wait_gl_checked (xcb_connection_t      *c  /**< */,
                         xcb_glx_context_tag_t  context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_WAIT_GL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_wait_gl_request_t xcb_out;

    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_wait_gl (xcb_connection_t      *c  /**< */,
                 xcb_glx_context_tag_t  context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_WAIT_GL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_wait_gl_request_t xcb_out;

    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_wait_x_checked (xcb_connection_t      *c  /**< */,
                        xcb_glx_context_tag_t  context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_WAIT_X,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_wait_x_request_t xcb_out;

    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_wait_x (xcb_connection_t      *c  /**< */,
                xcb_glx_context_tag_t  context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_WAIT_X,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_wait_x_request_t xcb_out;

    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_copy_context_checked (xcb_connection_t      *c  /**< */,
                              xcb_glx_context_t      src  /**< */,
                              xcb_glx_context_t      dest  /**< */,
                              uint32_t               mask  /**< */,
                              xcb_glx_context_tag_t  src_context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_COPY_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_copy_context_request_t xcb_out;

    xcb_out.src = src;
    xcb_out.dest = dest;
    xcb_out.mask = mask;
    xcb_out.src_context_tag = src_context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_copy_context (xcb_connection_t      *c  /**< */,
                      xcb_glx_context_t      src  /**< */,
                      xcb_glx_context_t      dest  /**< */,
                      uint32_t               mask  /**< */,
                      xcb_glx_context_tag_t  src_context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_COPY_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_copy_context_request_t xcb_out;

    xcb_out.src = src;
    xcb_out.dest = dest;
    xcb_out.mask = mask;
    xcb_out.src_context_tag = src_context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_swap_buffers_checked (xcb_connection_t      *c  /**< */,
                              xcb_glx_context_tag_t  context_tag  /**< */,
                              xcb_glx_drawable_t     drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_SWAP_BUFFERS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_swap_buffers_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.drawable = drawable;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_swap_buffers (xcb_connection_t      *c  /**< */,
                      xcb_glx_context_tag_t  context_tag  /**< */,
                      xcb_glx_drawable_t     drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_SWAP_BUFFERS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_swap_buffers_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.drawable = drawable;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_use_x_font_checked (xcb_connection_t      *c  /**< */,
                            xcb_glx_context_tag_t  context_tag  /**< */,
                            xcb_font_t             font  /**< */,
                            uint32_t               first  /**< */,
                            uint32_t               count  /**< */,
                            uint32_t               list_base  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_USE_X_FONT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_use_x_font_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.font = font;
    xcb_out.first = first;
    xcb_out.count = count;
    xcb_out.list_base = list_base;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_use_x_font (xcb_connection_t      *c  /**< */,
                    xcb_glx_context_tag_t  context_tag  /**< */,
                    xcb_font_t             font  /**< */,
                    uint32_t               first  /**< */,
                    uint32_t               count  /**< */,
                    uint32_t               list_base  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_USE_X_FONT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_use_x_font_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.font = font;
    xcb_out.first = first;
    xcb_out.count = count;
    xcb_out.list_base = list_base;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_create_glx_pixmap_checked (xcb_connection_t *c  /**< */,
                                   uint32_t          screen  /**< */,
                                   xcb_visualid_t    visual  /**< */,
                                   xcb_pixmap_t      pixmap  /**< */,
                                   xcb_glx_pixmap_t  glx_pixmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CREATE_GLX_PIXMAP,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_create_glx_pixmap_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.visual = visual;
    xcb_out.pixmap = pixmap;
    xcb_out.glx_pixmap = glx_pixmap;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_create_glx_pixmap (xcb_connection_t *c  /**< */,
                           uint32_t          screen  /**< */,
                           xcb_visualid_t    visual  /**< */,
                           xcb_pixmap_t      pixmap  /**< */,
                           xcb_glx_pixmap_t  glx_pixmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CREATE_GLX_PIXMAP,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_create_glx_pixmap_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.visual = visual;
    xcb_out.pixmap = pixmap;
    xcb_out.glx_pixmap = glx_pixmap;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_get_visual_configs_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_visual_configs_reply_t *_aux = (xcb_glx_get_visual_configs_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_visual_configs_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* property_list */
    xcb_block_len += _aux->length * sizeof(uint32_t);
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

xcb_glx_get_visual_configs_cookie_t
xcb_glx_get_visual_configs (xcb_connection_t *c  /**< */,
                            uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_VISUAL_CONFIGS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_visual_configs_cookie_t xcb_ret;
    xcb_glx_get_visual_configs_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_visual_configs_cookie_t
xcb_glx_get_visual_configs_unchecked (xcb_connection_t *c  /**< */,
                                      uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_VISUAL_CONFIGS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_visual_configs_cookie_t xcb_ret;
    xcb_glx_get_visual_configs_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_glx_get_visual_configs_property_list (const xcb_glx_get_visual_configs_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_glx_get_visual_configs_property_list_length (const xcb_glx_get_visual_configs_reply_t *R  /**< */)
{
    return R->length;
}

xcb_generic_iterator_t
xcb_glx_get_visual_configs_property_list_end (const xcb_glx_get_visual_configs_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->length);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_visual_configs_reply_t *
xcb_glx_get_visual_configs_reply (xcb_connection_t                     *c  /**< */,
                                  xcb_glx_get_visual_configs_cookie_t   cookie  /**< */,
                                  xcb_generic_error_t                 **e  /**< */)
{
    return (xcb_glx_get_visual_configs_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_glx_destroy_glx_pixmap_checked (xcb_connection_t *c  /**< */,
                                    xcb_glx_pixmap_t  glx_pixmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DESTROY_GLX_PIXMAP,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_destroy_glx_pixmap_request_t xcb_out;

    xcb_out.glx_pixmap = glx_pixmap;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_destroy_glx_pixmap (xcb_connection_t *c  /**< */,
                            xcb_glx_pixmap_t  glx_pixmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DESTROY_GLX_PIXMAP,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_destroy_glx_pixmap_request_t xcb_out;

    xcb_out.glx_pixmap = glx_pixmap;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_vendor_private_sizeof (const void  *_buffer  /**< */,
                               uint32_t     data_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_vendor_private_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += data_len * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
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
xcb_glx_vendor_private_checked (xcb_connection_t      *c  /**< */,
                                uint32_t               vendor_code  /**< */,
                                xcb_glx_context_tag_t  context_tag  /**< */,
                                uint32_t               data_len  /**< */,
                                const uint8_t         *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_VENDOR_PRIVATE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_vendor_private_request_t xcb_out;

    xcb_out.vendor_code = vendor_code;
    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = data_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_vendor_private (xcb_connection_t      *c  /**< */,
                        uint32_t               vendor_code  /**< */,
                        xcb_glx_context_tag_t  context_tag  /**< */,
                        uint32_t               data_len  /**< */,
                        const uint8_t         *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_VENDOR_PRIVATE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_vendor_private_request_t xcb_out;

    xcb_out.vendor_code = vendor_code;
    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = data_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_vendor_private_with_reply_sizeof (const void  *_buffer  /**< */,
                                          uint32_t     data_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_vendor_private_with_reply_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += data_len * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
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

xcb_glx_vendor_private_with_reply_cookie_t
xcb_glx_vendor_private_with_reply (xcb_connection_t      *c  /**< */,
                                   uint32_t               vendor_code  /**< */,
                                   xcb_glx_context_tag_t  context_tag  /**< */,
                                   uint32_t               data_len  /**< */,
                                   const uint8_t         *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_VENDOR_PRIVATE_WITH_REPLY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_glx_vendor_private_with_reply_cookie_t xcb_ret;
    xcb_glx_vendor_private_with_reply_request_t xcb_out;

    xcb_out.vendor_code = vendor_code;
    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = data_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_vendor_private_with_reply_cookie_t
xcb_glx_vendor_private_with_reply_unchecked (xcb_connection_t      *c  /**< */,
                                             uint32_t               vendor_code  /**< */,
                                             xcb_glx_context_tag_t  context_tag  /**< */,
                                             uint32_t               data_len  /**< */,
                                             const uint8_t         *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_VENDOR_PRIVATE_WITH_REPLY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_glx_vendor_private_with_reply_cookie_t xcb_ret;
    xcb_glx_vendor_private_with_reply_request_t xcb_out;

    xcb_out.vendor_code = vendor_code;
    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = data_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_glx_vendor_private_with_reply_data_2 (const xcb_glx_vendor_private_with_reply_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_glx_vendor_private_with_reply_data_2_length (const xcb_glx_vendor_private_with_reply_reply_t *R  /**< */)
{
    return (R->length * 4);
}

xcb_generic_iterator_t
xcb_glx_vendor_private_with_reply_data_2_end (const xcb_glx_vendor_private_with_reply_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->length * 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_vendor_private_with_reply_reply_t *
xcb_glx_vendor_private_with_reply_reply (xcb_connection_t                            *c  /**< */,
                                         xcb_glx_vendor_private_with_reply_cookie_t   cookie  /**< */,
                                         xcb_generic_error_t                        **e  /**< */)
{
    return (xcb_glx_vendor_private_with_reply_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_glx_query_extensions_string_cookie_t
xcb_glx_query_extensions_string (xcb_connection_t *c  /**< */,
                                 uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_QUERY_EXTENSIONS_STRING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_query_extensions_string_cookie_t xcb_ret;
    xcb_glx_query_extensions_string_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_query_extensions_string_cookie_t
xcb_glx_query_extensions_string_unchecked (xcb_connection_t *c  /**< */,
                                           uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_QUERY_EXTENSIONS_STRING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_query_extensions_string_cookie_t xcb_ret;
    xcb_glx_query_extensions_string_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_query_extensions_string_reply_t *
xcb_glx_query_extensions_string_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_glx_query_extensions_string_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */)
{
    return (xcb_glx_query_extensions_string_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_query_server_string_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_query_server_string_reply_t *_aux = (xcb_glx_query_server_string_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_query_server_string_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* string */
    xcb_block_len += _aux->str_len * sizeof(char);
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

xcb_glx_query_server_string_cookie_t
xcb_glx_query_server_string (xcb_connection_t *c  /**< */,
                             uint32_t          screen  /**< */,
                             uint32_t          name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_QUERY_SERVER_STRING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_query_server_string_cookie_t xcb_ret;
    xcb_glx_query_server_string_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.name = name;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_query_server_string_cookie_t
xcb_glx_query_server_string_unchecked (xcb_connection_t *c  /**< */,
                                       uint32_t          screen  /**< */,
                                       uint32_t          name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_QUERY_SERVER_STRING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_query_server_string_cookie_t xcb_ret;
    xcb_glx_query_server_string_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.name = name;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_glx_query_server_string_string (const xcb_glx_query_server_string_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_glx_query_server_string_string_length (const xcb_glx_query_server_string_reply_t *R  /**< */)
{
    return R->str_len;
}

xcb_generic_iterator_t
xcb_glx_query_server_string_string_end (const xcb_glx_query_server_string_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->str_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_query_server_string_reply_t *
xcb_glx_query_server_string_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_glx_query_server_string_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_glx_query_server_string_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_client_info_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_client_info_request_t *_aux = (xcb_glx_client_info_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_client_info_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* string */
    xcb_block_len += _aux->str_len * sizeof(char);
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
xcb_glx_client_info_checked (xcb_connection_t *c  /**< */,
                             uint32_t          major_version  /**< */,
                             uint32_t          minor_version  /**< */,
                             uint32_t          str_len  /**< */,
                             const char       *string  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CLIENT_INFO,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_client_info_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;
    xcb_out.str_len = str_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char string */
    xcb_parts[4].iov_base = (char *) string;
    xcb_parts[4].iov_len = str_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_client_info (xcb_connection_t *c  /**< */,
                     uint32_t          major_version  /**< */,
                     uint32_t          minor_version  /**< */,
                     uint32_t          str_len  /**< */,
                     const char       *string  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CLIENT_INFO,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_client_info_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;
    xcb_out.str_len = str_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char string */
    xcb_parts[4].iov_base = (char *) string;
    xcb_parts[4].iov_len = str_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_get_fb_configs_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_fb_configs_reply_t *_aux = (xcb_glx_get_fb_configs_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_fb_configs_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* property_list */
    xcb_block_len += _aux->length * sizeof(uint32_t);
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

xcb_glx_get_fb_configs_cookie_t
xcb_glx_get_fb_configs (xcb_connection_t *c  /**< */,
                        uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_FB_CONFIGS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_fb_configs_cookie_t xcb_ret;
    xcb_glx_get_fb_configs_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_fb_configs_cookie_t
xcb_glx_get_fb_configs_unchecked (xcb_connection_t *c  /**< */,
                                  uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_FB_CONFIGS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_fb_configs_cookie_t xcb_ret;
    xcb_glx_get_fb_configs_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_glx_get_fb_configs_property_list (const xcb_glx_get_fb_configs_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_glx_get_fb_configs_property_list_length (const xcb_glx_get_fb_configs_reply_t *R  /**< */)
{
    return R->length;
}

xcb_generic_iterator_t
xcb_glx_get_fb_configs_property_list_end (const xcb_glx_get_fb_configs_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->length);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_fb_configs_reply_t *
xcb_glx_get_fb_configs_reply (xcb_connection_t                 *c  /**< */,
                              xcb_glx_get_fb_configs_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_glx_get_fb_configs_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_create_pixmap_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_create_pixmap_request_t *_aux = (xcb_glx_create_pixmap_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_create_pixmap_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* attribs */
    xcb_block_len += (_aux->num_attribs * 2) * sizeof(uint32_t);
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

xcb_void_cookie_t
xcb_glx_create_pixmap_checked (xcb_connection_t   *c  /**< */,
                               uint32_t            screen  /**< */,
                               xcb_glx_fbconfig_t  fbconfig  /**< */,
                               xcb_pixmap_t        pixmap  /**< */,
                               xcb_glx_pixmap_t    glx_pixmap  /**< */,
                               uint32_t            num_attribs  /**< */,
                               const uint32_t     *attribs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CREATE_PIXMAP,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_create_pixmap_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.fbconfig = fbconfig;
    xcb_out.pixmap = pixmap;
    xcb_out.glx_pixmap = glx_pixmap;
    xcb_out.num_attribs = num_attribs;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t attribs */
    xcb_parts[4].iov_base = (char *) attribs;
    xcb_parts[4].iov_len = (num_attribs * 2) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_create_pixmap (xcb_connection_t   *c  /**< */,
                       uint32_t            screen  /**< */,
                       xcb_glx_fbconfig_t  fbconfig  /**< */,
                       xcb_pixmap_t        pixmap  /**< */,
                       xcb_glx_pixmap_t    glx_pixmap  /**< */,
                       uint32_t            num_attribs  /**< */,
                       const uint32_t     *attribs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CREATE_PIXMAP,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_create_pixmap_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.fbconfig = fbconfig;
    xcb_out.pixmap = pixmap;
    xcb_out.glx_pixmap = glx_pixmap;
    xcb_out.num_attribs = num_attribs;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t attribs */
    xcb_parts[4].iov_base = (char *) attribs;
    xcb_parts[4].iov_len = (num_attribs * 2) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_destroy_pixmap_checked (xcb_connection_t *c  /**< */,
                                xcb_glx_pixmap_t  glx_pixmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DESTROY_PIXMAP,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_destroy_pixmap_request_t xcb_out;

    xcb_out.glx_pixmap = glx_pixmap;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_destroy_pixmap (xcb_connection_t *c  /**< */,
                        xcb_glx_pixmap_t  glx_pixmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DESTROY_PIXMAP,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_destroy_pixmap_request_t xcb_out;

    xcb_out.glx_pixmap = glx_pixmap;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_create_new_context_checked (xcb_connection_t   *c  /**< */,
                                    xcb_glx_context_t   context  /**< */,
                                    xcb_glx_fbconfig_t  fbconfig  /**< */,
                                    uint32_t            screen  /**< */,
                                    uint32_t            render_type  /**< */,
                                    xcb_glx_context_t   share_list  /**< */,
                                    uint8_t             is_direct  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CREATE_NEW_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_create_new_context_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.fbconfig = fbconfig;
    xcb_out.screen = screen;
    xcb_out.render_type = render_type;
    xcb_out.share_list = share_list;
    xcb_out.is_direct = is_direct;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_create_new_context (xcb_connection_t   *c  /**< */,
                            xcb_glx_context_t   context  /**< */,
                            xcb_glx_fbconfig_t  fbconfig  /**< */,
                            uint32_t            screen  /**< */,
                            uint32_t            render_type  /**< */,
                            xcb_glx_context_t   share_list  /**< */,
                            uint8_t             is_direct  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CREATE_NEW_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_create_new_context_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.fbconfig = fbconfig;
    xcb_out.screen = screen;
    xcb_out.render_type = render_type;
    xcb_out.share_list = share_list;
    xcb_out.is_direct = is_direct;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_query_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_query_context_reply_t *_aux = (xcb_glx_query_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_query_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* attribs */
    xcb_block_len += (_aux->num_attribs * 2) * sizeof(uint32_t);
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

xcb_glx_query_context_cookie_t
xcb_glx_query_context (xcb_connection_t  *c  /**< */,
                       xcb_glx_context_t  context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_QUERY_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_query_context_cookie_t xcb_ret;
    xcb_glx_query_context_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_query_context_cookie_t
xcb_glx_query_context_unchecked (xcb_connection_t  *c  /**< */,
                                 xcb_glx_context_t  context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_QUERY_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_query_context_cookie_t xcb_ret;
    xcb_glx_query_context_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_glx_query_context_attribs (const xcb_glx_query_context_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_glx_query_context_attribs_length (const xcb_glx_query_context_reply_t *R  /**< */)
{
    return (R->num_attribs * 2);
}

xcb_generic_iterator_t
xcb_glx_query_context_attribs_end (const xcb_glx_query_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + ((R->num_attribs * 2));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_query_context_reply_t *
xcb_glx_query_context_reply (xcb_connection_t                *c  /**< */,
                             xcb_glx_query_context_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_glx_query_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_glx_make_context_current_cookie_t
xcb_glx_make_context_current (xcb_connection_t      *c  /**< */,
                              xcb_glx_context_tag_t  old_context_tag  /**< */,
                              xcb_glx_drawable_t     drawable  /**< */,
                              xcb_glx_drawable_t     read_drawable  /**< */,
                              xcb_glx_context_t      context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_MAKE_CONTEXT_CURRENT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_make_context_current_cookie_t xcb_ret;
    xcb_glx_make_context_current_request_t xcb_out;

    xcb_out.old_context_tag = old_context_tag;
    xcb_out.drawable = drawable;
    xcb_out.read_drawable = read_drawable;
    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_make_context_current_cookie_t
xcb_glx_make_context_current_unchecked (xcb_connection_t      *c  /**< */,
                                        xcb_glx_context_tag_t  old_context_tag  /**< */,
                                        xcb_glx_drawable_t     drawable  /**< */,
                                        xcb_glx_drawable_t     read_drawable  /**< */,
                                        xcb_glx_context_t      context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_MAKE_CONTEXT_CURRENT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_make_context_current_cookie_t xcb_ret;
    xcb_glx_make_context_current_request_t xcb_out;

    xcb_out.old_context_tag = old_context_tag;
    xcb_out.drawable = drawable;
    xcb_out.read_drawable = read_drawable;
    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_make_context_current_reply_t *
xcb_glx_make_context_current_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_glx_make_context_current_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */)
{
    return (xcb_glx_make_context_current_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_create_pbuffer_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_create_pbuffer_request_t *_aux = (xcb_glx_create_pbuffer_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_create_pbuffer_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* attribs */
    xcb_block_len += (_aux->num_attribs * 2) * sizeof(uint32_t);
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

xcb_void_cookie_t
xcb_glx_create_pbuffer_checked (xcb_connection_t   *c  /**< */,
                                uint32_t            screen  /**< */,
                                xcb_glx_fbconfig_t  fbconfig  /**< */,
                                xcb_glx_pbuffer_t   pbuffer  /**< */,
                                uint32_t            num_attribs  /**< */,
                                const uint32_t     *attribs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CREATE_PBUFFER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_create_pbuffer_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.fbconfig = fbconfig;
    xcb_out.pbuffer = pbuffer;
    xcb_out.num_attribs = num_attribs;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t attribs */
    xcb_parts[4].iov_base = (char *) attribs;
    xcb_parts[4].iov_len = (num_attribs * 2) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_create_pbuffer (xcb_connection_t   *c  /**< */,
                        uint32_t            screen  /**< */,
                        xcb_glx_fbconfig_t  fbconfig  /**< */,
                        xcb_glx_pbuffer_t   pbuffer  /**< */,
                        uint32_t            num_attribs  /**< */,
                        const uint32_t     *attribs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CREATE_PBUFFER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_create_pbuffer_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.fbconfig = fbconfig;
    xcb_out.pbuffer = pbuffer;
    xcb_out.num_attribs = num_attribs;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t attribs */
    xcb_parts[4].iov_base = (char *) attribs;
    xcb_parts[4].iov_len = (num_attribs * 2) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_destroy_pbuffer_checked (xcb_connection_t  *c  /**< */,
                                 xcb_glx_pbuffer_t  pbuffer  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DESTROY_PBUFFER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_destroy_pbuffer_request_t xcb_out;

    xcb_out.pbuffer = pbuffer;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_destroy_pbuffer (xcb_connection_t  *c  /**< */,
                         xcb_glx_pbuffer_t  pbuffer  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DESTROY_PBUFFER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_destroy_pbuffer_request_t xcb_out;

    xcb_out.pbuffer = pbuffer;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_get_drawable_attributes_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_drawable_attributes_reply_t *_aux = (xcb_glx_get_drawable_attributes_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_drawable_attributes_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* attribs */
    xcb_block_len += (_aux->num_attribs * 2) * sizeof(uint32_t);
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

xcb_glx_get_drawable_attributes_cookie_t
xcb_glx_get_drawable_attributes (xcb_connection_t   *c  /**< */,
                                 xcb_glx_drawable_t  drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_DRAWABLE_ATTRIBUTES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_drawable_attributes_cookie_t xcb_ret;
    xcb_glx_get_drawable_attributes_request_t xcb_out;

    xcb_out.drawable = drawable;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_drawable_attributes_cookie_t
xcb_glx_get_drawable_attributes_unchecked (xcb_connection_t   *c  /**< */,
                                           xcb_glx_drawable_t  drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_DRAWABLE_ATTRIBUTES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_drawable_attributes_cookie_t xcb_ret;
    xcb_glx_get_drawable_attributes_request_t xcb_out;

    xcb_out.drawable = drawable;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_glx_get_drawable_attributes_attribs (const xcb_glx_get_drawable_attributes_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_glx_get_drawable_attributes_attribs_length (const xcb_glx_get_drawable_attributes_reply_t *R  /**< */)
{
    return (R->num_attribs * 2);
}

xcb_generic_iterator_t
xcb_glx_get_drawable_attributes_attribs_end (const xcb_glx_get_drawable_attributes_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + ((R->num_attribs * 2));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_drawable_attributes_reply_t *
xcb_glx_get_drawable_attributes_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_glx_get_drawable_attributes_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */)
{
    return (xcb_glx_get_drawable_attributes_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_change_drawable_attributes_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_change_drawable_attributes_request_t *_aux = (xcb_glx_change_drawable_attributes_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_change_drawable_attributes_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* attribs */
    xcb_block_len += (_aux->num_attribs * 2) * sizeof(uint32_t);
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

xcb_void_cookie_t
xcb_glx_change_drawable_attributes_checked (xcb_connection_t   *c  /**< */,
                                            xcb_glx_drawable_t  drawable  /**< */,
                                            uint32_t            num_attribs  /**< */,
                                            const uint32_t     *attribs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CHANGE_DRAWABLE_ATTRIBUTES,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_change_drawable_attributes_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.num_attribs = num_attribs;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t attribs */
    xcb_parts[4].iov_base = (char *) attribs;
    xcb_parts[4].iov_len = (num_attribs * 2) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_change_drawable_attributes (xcb_connection_t   *c  /**< */,
                                    xcb_glx_drawable_t  drawable  /**< */,
                                    uint32_t            num_attribs  /**< */,
                                    const uint32_t     *attribs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CHANGE_DRAWABLE_ATTRIBUTES,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_change_drawable_attributes_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.num_attribs = num_attribs;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t attribs */
    xcb_parts[4].iov_base = (char *) attribs;
    xcb_parts[4].iov_len = (num_attribs * 2) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_create_window_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_create_window_request_t *_aux = (xcb_glx_create_window_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_create_window_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* attribs */
    xcb_block_len += (_aux->num_attribs * 2) * sizeof(uint32_t);
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

xcb_void_cookie_t
xcb_glx_create_window_checked (xcb_connection_t   *c  /**< */,
                               uint32_t            screen  /**< */,
                               xcb_glx_fbconfig_t  fbconfig  /**< */,
                               xcb_window_t        window  /**< */,
                               xcb_glx_window_t    glx_window  /**< */,
                               uint32_t            num_attribs  /**< */,
                               const uint32_t     *attribs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CREATE_WINDOW,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_create_window_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.fbconfig = fbconfig;
    xcb_out.window = window;
    xcb_out.glx_window = glx_window;
    xcb_out.num_attribs = num_attribs;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t attribs */
    xcb_parts[4].iov_base = (char *) attribs;
    xcb_parts[4].iov_len = (num_attribs * 2) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_create_window (xcb_connection_t   *c  /**< */,
                       uint32_t            screen  /**< */,
                       xcb_glx_fbconfig_t  fbconfig  /**< */,
                       xcb_window_t        window  /**< */,
                       xcb_glx_window_t    glx_window  /**< */,
                       uint32_t            num_attribs  /**< */,
                       const uint32_t     *attribs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CREATE_WINDOW,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_create_window_request_t xcb_out;

    xcb_out.screen = screen;
    xcb_out.fbconfig = fbconfig;
    xcb_out.window = window;
    xcb_out.glx_window = glx_window;
    xcb_out.num_attribs = num_attribs;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t attribs */
    xcb_parts[4].iov_base = (char *) attribs;
    xcb_parts[4].iov_len = (num_attribs * 2) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_delete_window_checked (xcb_connection_t *c  /**< */,
                               xcb_glx_window_t  glxwindow  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DELETE_WINDOW,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_delete_window_request_t xcb_out;

    xcb_out.glxwindow = glxwindow;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_delete_window (xcb_connection_t *c  /**< */,
                       xcb_glx_window_t  glxwindow  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DELETE_WINDOW,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_delete_window_request_t xcb_out;

    xcb_out.glxwindow = glxwindow;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_set_client_info_arb_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_set_client_info_arb_request_t *_aux = (xcb_glx_set_client_info_arb_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_set_client_info_arb_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* gl_versions */
    xcb_block_len += (_aux->num_versions * 2) * sizeof(uint32_t);
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
    /* gl_extension_string */
    xcb_block_len += _aux->gl_str_len * sizeof(char);
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
    /* glx_extension_string */
    xcb_block_len += _aux->glx_str_len * sizeof(char);
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
xcb_glx_set_client_info_arb_checked (xcb_connection_t *c  /**< */,
                                     uint32_t          major_version  /**< */,
                                     uint32_t          minor_version  /**< */,
                                     uint32_t          num_versions  /**< */,
                                     uint32_t          gl_str_len  /**< */,
                                     uint32_t          glx_str_len  /**< */,
                                     const uint32_t   *gl_versions  /**< */,
                                     const char       *gl_extension_string  /**< */,
                                     const char       *glx_extension_string  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 8,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_SET_CLIENT_INFO_ARB,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[10];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_set_client_info_arb_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;
    xcb_out.num_versions = num_versions;
    xcb_out.gl_str_len = gl_str_len;
    xcb_out.glx_str_len = glx_str_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t gl_versions */
    xcb_parts[4].iov_base = (char *) gl_versions;
    xcb_parts[4].iov_len = (num_versions * 2) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* char gl_extension_string */
    xcb_parts[6].iov_base = (char *) gl_extension_string;
    xcb_parts[6].iov_len = gl_str_len * sizeof(char);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;
    /* char glx_extension_string */
    xcb_parts[8].iov_base = (char *) glx_extension_string;
    xcb_parts[8].iov_len = glx_str_len * sizeof(char);
    xcb_parts[9].iov_base = 0;
    xcb_parts[9].iov_len = -xcb_parts[8].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_set_client_info_arb (xcb_connection_t *c  /**< */,
                             uint32_t          major_version  /**< */,
                             uint32_t          minor_version  /**< */,
                             uint32_t          num_versions  /**< */,
                             uint32_t          gl_str_len  /**< */,
                             uint32_t          glx_str_len  /**< */,
                             const uint32_t   *gl_versions  /**< */,
                             const char       *gl_extension_string  /**< */,
                             const char       *glx_extension_string  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 8,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_SET_CLIENT_INFO_ARB,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[10];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_set_client_info_arb_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;
    xcb_out.num_versions = num_versions;
    xcb_out.gl_str_len = gl_str_len;
    xcb_out.glx_str_len = glx_str_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t gl_versions */
    xcb_parts[4].iov_base = (char *) gl_versions;
    xcb_parts[4].iov_len = (num_versions * 2) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* char gl_extension_string */
    xcb_parts[6].iov_base = (char *) gl_extension_string;
    xcb_parts[6].iov_len = gl_str_len * sizeof(char);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;
    /* char glx_extension_string */
    xcb_parts[8].iov_base = (char *) glx_extension_string;
    xcb_parts[8].iov_len = glx_str_len * sizeof(char);
    xcb_parts[9].iov_base = 0;
    xcb_parts[9].iov_len = -xcb_parts[8].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_create_context_attribs_arb_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_create_context_attribs_arb_request_t *_aux = (xcb_glx_create_context_attribs_arb_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_create_context_attribs_arb_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* attribs */
    xcb_block_len += (_aux->num_attribs * 2) * sizeof(uint32_t);
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

xcb_void_cookie_t
xcb_glx_create_context_attribs_arb_checked (xcb_connection_t   *c  /**< */,
                                            xcb_glx_context_t   context  /**< */,
                                            xcb_glx_fbconfig_t  fbconfig  /**< */,
                                            uint32_t            screen  /**< */,
                                            xcb_glx_context_t   share_list  /**< */,
                                            uint8_t             is_direct  /**< */,
                                            uint32_t            num_attribs  /**< */,
                                            const uint32_t     *attribs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CREATE_CONTEXT_ATTRIBS_ARB,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_create_context_attribs_arb_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.fbconfig = fbconfig;
    xcb_out.screen = screen;
    xcb_out.share_list = share_list;
    xcb_out.is_direct = is_direct;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.num_attribs = num_attribs;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t attribs */
    xcb_parts[4].iov_base = (char *) attribs;
    xcb_parts[4].iov_len = (num_attribs * 2) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_create_context_attribs_arb (xcb_connection_t   *c  /**< */,
                                    xcb_glx_context_t   context  /**< */,
                                    xcb_glx_fbconfig_t  fbconfig  /**< */,
                                    uint32_t            screen  /**< */,
                                    xcb_glx_context_t   share_list  /**< */,
                                    uint8_t             is_direct  /**< */,
                                    uint32_t            num_attribs  /**< */,
                                    const uint32_t     *attribs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_CREATE_CONTEXT_ATTRIBS_ARB,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_create_context_attribs_arb_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.fbconfig = fbconfig;
    xcb_out.screen = screen;
    xcb_out.share_list = share_list;
    xcb_out.is_direct = is_direct;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.num_attribs = num_attribs;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t attribs */
    xcb_parts[4].iov_base = (char *) attribs;
    xcb_parts[4].iov_len = (num_attribs * 2) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_set_client_info_2arb_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_set_client_info_2arb_request_t *_aux = (xcb_glx_set_client_info_2arb_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_set_client_info_2arb_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* gl_versions */
    xcb_block_len += (_aux->num_versions * 3) * sizeof(uint32_t);
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
    /* gl_extension_string */
    xcb_block_len += _aux->gl_str_len * sizeof(char);
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
    /* glx_extension_string */
    xcb_block_len += _aux->glx_str_len * sizeof(char);
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
xcb_glx_set_client_info_2arb_checked (xcb_connection_t *c  /**< */,
                                      uint32_t          major_version  /**< */,
                                      uint32_t          minor_version  /**< */,
                                      uint32_t          num_versions  /**< */,
                                      uint32_t          gl_str_len  /**< */,
                                      uint32_t          glx_str_len  /**< */,
                                      const uint32_t   *gl_versions  /**< */,
                                      const char       *gl_extension_string  /**< */,
                                      const char       *glx_extension_string  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 8,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_SET_CLIENT_INFO_2ARB,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[10];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_set_client_info_2arb_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;
    xcb_out.num_versions = num_versions;
    xcb_out.gl_str_len = gl_str_len;
    xcb_out.glx_str_len = glx_str_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t gl_versions */
    xcb_parts[4].iov_base = (char *) gl_versions;
    xcb_parts[4].iov_len = (num_versions * 3) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* char gl_extension_string */
    xcb_parts[6].iov_base = (char *) gl_extension_string;
    xcb_parts[6].iov_len = gl_str_len * sizeof(char);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;
    /* char glx_extension_string */
    xcb_parts[8].iov_base = (char *) glx_extension_string;
    xcb_parts[8].iov_len = glx_str_len * sizeof(char);
    xcb_parts[9].iov_base = 0;
    xcb_parts[9].iov_len = -xcb_parts[8].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_set_client_info_2arb (xcb_connection_t *c  /**< */,
                              uint32_t          major_version  /**< */,
                              uint32_t          minor_version  /**< */,
                              uint32_t          num_versions  /**< */,
                              uint32_t          gl_str_len  /**< */,
                              uint32_t          glx_str_len  /**< */,
                              const uint32_t   *gl_versions  /**< */,
                              const char       *gl_extension_string  /**< */,
                              const char       *glx_extension_string  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 8,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_SET_CLIENT_INFO_2ARB,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[10];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_set_client_info_2arb_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;
    xcb_out.num_versions = num_versions;
    xcb_out.gl_str_len = gl_str_len;
    xcb_out.glx_str_len = glx_str_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t gl_versions */
    xcb_parts[4].iov_base = (char *) gl_versions;
    xcb_parts[4].iov_len = (num_versions * 3) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* char gl_extension_string */
    xcb_parts[6].iov_base = (char *) gl_extension_string;
    xcb_parts[6].iov_len = gl_str_len * sizeof(char);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;
    /* char glx_extension_string */
    xcb_parts[8].iov_base = (char *) glx_extension_string;
    xcb_parts[8].iov_len = glx_str_len * sizeof(char);
    xcb_parts[9].iov_base = 0;
    xcb_parts[9].iov_len = -xcb_parts[8].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_new_list_checked (xcb_connection_t      *c  /**< */,
                          xcb_glx_context_tag_t  context_tag  /**< */,
                          uint32_t               list  /**< */,
                          uint32_t               mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_NEW_LIST,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_new_list_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.list = list;
    xcb_out.mode = mode;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_new_list (xcb_connection_t      *c  /**< */,
                  xcb_glx_context_tag_t  context_tag  /**< */,
                  uint32_t               list  /**< */,
                  uint32_t               mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_NEW_LIST,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_new_list_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.list = list;
    xcb_out.mode = mode;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_end_list_checked (xcb_connection_t      *c  /**< */,
                          xcb_glx_context_tag_t  context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_END_LIST,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_end_list_request_t xcb_out;

    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_end_list (xcb_connection_t      *c  /**< */,
                  xcb_glx_context_tag_t  context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_END_LIST,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_end_list_request_t xcb_out;

    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_delete_lists_checked (xcb_connection_t      *c  /**< */,
                              xcb_glx_context_tag_t  context_tag  /**< */,
                              uint32_t               list  /**< */,
                              int32_t                range  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DELETE_LISTS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_delete_lists_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.list = list;
    xcb_out.range = range;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_delete_lists (xcb_connection_t      *c  /**< */,
                      xcb_glx_context_tag_t  context_tag  /**< */,
                      uint32_t               list  /**< */,
                      int32_t                range  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DELETE_LISTS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_delete_lists_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.list = list;
    xcb_out.range = range;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_gen_lists_cookie_t
xcb_glx_gen_lists (xcb_connection_t      *c  /**< */,
                   xcb_glx_context_tag_t  context_tag  /**< */,
                   int32_t                range  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GEN_LISTS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_gen_lists_cookie_t xcb_ret;
    xcb_glx_gen_lists_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.range = range;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_gen_lists_cookie_t
xcb_glx_gen_lists_unchecked (xcb_connection_t      *c  /**< */,
                             xcb_glx_context_tag_t  context_tag  /**< */,
                             int32_t                range  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GEN_LISTS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_gen_lists_cookie_t xcb_ret;
    xcb_glx_gen_lists_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.range = range;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_gen_lists_reply_t *
xcb_glx_gen_lists_reply (xcb_connection_t            *c  /**< */,
                         xcb_glx_gen_lists_cookie_t   cookie  /**< */,
                         xcb_generic_error_t        **e  /**< */)
{
    return (xcb_glx_gen_lists_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_glx_feedback_buffer_checked (xcb_connection_t      *c  /**< */,
                                 xcb_glx_context_tag_t  context_tag  /**< */,
                                 int32_t                size  /**< */,
                                 int32_t                type  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_FEEDBACK_BUFFER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_feedback_buffer_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.size = size;
    xcb_out.type = type;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_feedback_buffer (xcb_connection_t      *c  /**< */,
                         xcb_glx_context_tag_t  context_tag  /**< */,
                         int32_t                size  /**< */,
                         int32_t                type  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_FEEDBACK_BUFFER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_feedback_buffer_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.size = size;
    xcb_out.type = type;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_select_buffer_checked (xcb_connection_t      *c  /**< */,
                               xcb_glx_context_tag_t  context_tag  /**< */,
                               int32_t                size  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_SELECT_BUFFER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_select_buffer_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.size = size;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_select_buffer (xcb_connection_t      *c  /**< */,
                       xcb_glx_context_tag_t  context_tag  /**< */,
                       int32_t                size  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_SELECT_BUFFER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_select_buffer_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.size = size;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_render_mode_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_render_mode_reply_t *_aux = (xcb_glx_render_mode_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_render_mode_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(uint32_t);
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

xcb_glx_render_mode_cookie_t
xcb_glx_render_mode (xcb_connection_t      *c  /**< */,
                     xcb_glx_context_tag_t  context_tag  /**< */,
                     uint32_t               mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_RENDER_MODE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_render_mode_cookie_t xcb_ret;
    xcb_glx_render_mode_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.mode = mode;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_render_mode_cookie_t
xcb_glx_render_mode_unchecked (xcb_connection_t      *c  /**< */,
                               xcb_glx_context_tag_t  context_tag  /**< */,
                               uint32_t               mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_RENDER_MODE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_render_mode_cookie_t xcb_ret;
    xcb_glx_render_mode_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.mode = mode;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_glx_render_mode_data (const xcb_glx_render_mode_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_glx_render_mode_data_length (const xcb_glx_render_mode_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_render_mode_data_end (const xcb_glx_render_mode_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_render_mode_reply_t *
xcb_glx_render_mode_reply (xcb_connection_t              *c  /**< */,
                           xcb_glx_render_mode_cookie_t   cookie  /**< */,
                           xcb_generic_error_t          **e  /**< */)
{
    return (xcb_glx_render_mode_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_glx_finish_cookie_t
xcb_glx_finish (xcb_connection_t      *c  /**< */,
                xcb_glx_context_tag_t  context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_FINISH,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_finish_cookie_t xcb_ret;
    xcb_glx_finish_request_t xcb_out;

    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_finish_cookie_t
xcb_glx_finish_unchecked (xcb_connection_t      *c  /**< */,
                          xcb_glx_context_tag_t  context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_FINISH,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_finish_cookie_t xcb_ret;
    xcb_glx_finish_request_t xcb_out;

    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_finish_reply_t *
xcb_glx_finish_reply (xcb_connection_t         *c  /**< */,
                      xcb_glx_finish_cookie_t   cookie  /**< */,
                      xcb_generic_error_t     **e  /**< */)
{
    return (xcb_glx_finish_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_glx_pixel_storef_checked (xcb_connection_t      *c  /**< */,
                              xcb_glx_context_tag_t  context_tag  /**< */,
                              uint32_t               pname  /**< */,
                              xcb_glx_float32_t      datum  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_PIXEL_STOREF,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_pixel_storef_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.pname = pname;
    xcb_out.datum = datum;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_pixel_storef (xcb_connection_t      *c  /**< */,
                      xcb_glx_context_tag_t  context_tag  /**< */,
                      uint32_t               pname  /**< */,
                      xcb_glx_float32_t      datum  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_PIXEL_STOREF,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_pixel_storef_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.pname = pname;
    xcb_out.datum = datum;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_pixel_storei_checked (xcb_connection_t      *c  /**< */,
                              xcb_glx_context_tag_t  context_tag  /**< */,
                              uint32_t               pname  /**< */,
                              int32_t                datum  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_PIXEL_STOREI,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_pixel_storei_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.pname = pname;
    xcb_out.datum = datum;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_pixel_storei (xcb_connection_t      *c  /**< */,
                      xcb_glx_context_tag_t  context_tag  /**< */,
                      uint32_t               pname  /**< */,
                      int32_t                datum  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_PIXEL_STOREI,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_pixel_storei_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.pname = pname;
    xcb_out.datum = datum;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_read_pixels_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_read_pixels_reply_t *_aux = (xcb_glx_read_pixels_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_read_pixels_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += (_aux->length * 4) * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
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

xcb_glx_read_pixels_cookie_t
xcb_glx_read_pixels (xcb_connection_t      *c  /**< */,
                     xcb_glx_context_tag_t  context_tag  /**< */,
                     int32_t                x  /**< */,
                     int32_t                y  /**< */,
                     int32_t                width  /**< */,
                     int32_t                height  /**< */,
                     uint32_t               format  /**< */,
                     uint32_t               type  /**< */,
                     uint8_t                swap_bytes  /**< */,
                     uint8_t                lsb_first  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_READ_PIXELS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_read_pixels_cookie_t xcb_ret;
    xcb_glx_read_pixels_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.x = x;
    xcb_out.y = y;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.format = format;
    xcb_out.type = type;
    xcb_out.swap_bytes = swap_bytes;
    xcb_out.lsb_first = lsb_first;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_read_pixels_cookie_t
xcb_glx_read_pixels_unchecked (xcb_connection_t      *c  /**< */,
                               xcb_glx_context_tag_t  context_tag  /**< */,
                               int32_t                x  /**< */,
                               int32_t                y  /**< */,
                               int32_t                width  /**< */,
                               int32_t                height  /**< */,
                               uint32_t               format  /**< */,
                               uint32_t               type  /**< */,
                               uint8_t                swap_bytes  /**< */,
                               uint8_t                lsb_first  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_READ_PIXELS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_read_pixels_cookie_t xcb_ret;
    xcb_glx_read_pixels_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.x = x;
    xcb_out.y = y;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.format = format;
    xcb_out.type = type;
    xcb_out.swap_bytes = swap_bytes;
    xcb_out.lsb_first = lsb_first;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_glx_read_pixels_data (const xcb_glx_read_pixels_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_glx_read_pixels_data_length (const xcb_glx_read_pixels_reply_t *R  /**< */)
{
    return (R->length * 4);
}

xcb_generic_iterator_t
xcb_glx_read_pixels_data_end (const xcb_glx_read_pixels_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->length * 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_read_pixels_reply_t *
xcb_glx_read_pixels_reply (xcb_connection_t              *c  /**< */,
                           xcb_glx_read_pixels_cookie_t   cookie  /**< */,
                           xcb_generic_error_t          **e  /**< */)
{
    return (xcb_glx_read_pixels_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_booleanv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_booleanv_reply_t *_aux = (xcb_glx_get_booleanv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_booleanv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
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

xcb_glx_get_booleanv_cookie_t
xcb_glx_get_booleanv (xcb_connection_t      *c  /**< */,
                      xcb_glx_context_tag_t  context_tag  /**< */,
                      int32_t                pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_BOOLEANV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_booleanv_cookie_t xcb_ret;
    xcb_glx_get_booleanv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_booleanv_cookie_t
xcb_glx_get_booleanv_unchecked (xcb_connection_t      *c  /**< */,
                                xcb_glx_context_tag_t  context_tag  /**< */,
                                int32_t                pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_BOOLEANV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_booleanv_cookie_t xcb_ret;
    xcb_glx_get_booleanv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_glx_get_booleanv_data (const xcb_glx_get_booleanv_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_glx_get_booleanv_data_length (const xcb_glx_get_booleanv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_booleanv_data_end (const xcb_glx_get_booleanv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_booleanv_reply_t *
xcb_glx_get_booleanv_reply (xcb_connection_t               *c  /**< */,
                            xcb_glx_get_booleanv_cookie_t   cookie  /**< */,
                            xcb_generic_error_t           **e  /**< */)
{
    return (xcb_glx_get_booleanv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_clip_plane_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_clip_plane_reply_t *_aux = (xcb_glx_get_clip_plane_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_clip_plane_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += (_aux->length / 2) * sizeof(xcb_glx_float64_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float64_t);
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

xcb_glx_get_clip_plane_cookie_t
xcb_glx_get_clip_plane (xcb_connection_t      *c  /**< */,
                        xcb_glx_context_tag_t  context_tag  /**< */,
                        int32_t                plane  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_CLIP_PLANE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_clip_plane_cookie_t xcb_ret;
    xcb_glx_get_clip_plane_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.plane = plane;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_clip_plane_cookie_t
xcb_glx_get_clip_plane_unchecked (xcb_connection_t      *c  /**< */,
                                  xcb_glx_context_tag_t  context_tag  /**< */,
                                  int32_t                plane  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_CLIP_PLANE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_clip_plane_cookie_t xcb_ret;
    xcb_glx_get_clip_plane_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.plane = plane;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float64_t *
xcb_glx_get_clip_plane_data (const xcb_glx_get_clip_plane_reply_t *R  /**< */)
{
    return (xcb_glx_float64_t *) (R + 1);
}

int
xcb_glx_get_clip_plane_data_length (const xcb_glx_get_clip_plane_reply_t *R  /**< */)
{
    return (R->length / 2);
}

xcb_generic_iterator_t
xcb_glx_get_clip_plane_data_end (const xcb_glx_get_clip_plane_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float64_t *) (R + 1)) + ((R->length / 2));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_clip_plane_reply_t *
xcb_glx_get_clip_plane_reply (xcb_connection_t                 *c  /**< */,
                              xcb_glx_get_clip_plane_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_glx_get_clip_plane_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_doublev_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_doublev_reply_t *_aux = (xcb_glx_get_doublev_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_doublev_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float64_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float64_t);
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

xcb_glx_get_doublev_cookie_t
xcb_glx_get_doublev (xcb_connection_t      *c  /**< */,
                     xcb_glx_context_tag_t  context_tag  /**< */,
                     uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_DOUBLEV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_doublev_cookie_t xcb_ret;
    xcb_glx_get_doublev_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_doublev_cookie_t
xcb_glx_get_doublev_unchecked (xcb_connection_t      *c  /**< */,
                               xcb_glx_context_tag_t  context_tag  /**< */,
                               uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_DOUBLEV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_doublev_cookie_t xcb_ret;
    xcb_glx_get_doublev_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float64_t *
xcb_glx_get_doublev_data (const xcb_glx_get_doublev_reply_t *R  /**< */)
{
    return (xcb_glx_float64_t *) (R + 1);
}

int
xcb_glx_get_doublev_data_length (const xcb_glx_get_doublev_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_doublev_data_end (const xcb_glx_get_doublev_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float64_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_doublev_reply_t *
xcb_glx_get_doublev_reply (xcb_connection_t              *c  /**< */,
                           xcb_glx_get_doublev_cookie_t   cookie  /**< */,
                           xcb_generic_error_t          **e  /**< */)
{
    return (xcb_glx_get_doublev_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_glx_get_error_cookie_t
xcb_glx_get_error (xcb_connection_t      *c  /**< */,
                   xcb_glx_context_tag_t  context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_ERROR,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_error_cookie_t xcb_ret;
    xcb_glx_get_error_request_t xcb_out;

    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_error_cookie_t
xcb_glx_get_error_unchecked (xcb_connection_t      *c  /**< */,
                             xcb_glx_context_tag_t  context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_ERROR,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_error_cookie_t xcb_ret;
    xcb_glx_get_error_request_t xcb_out;

    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_error_reply_t *
xcb_glx_get_error_reply (xcb_connection_t            *c  /**< */,
                         xcb_glx_get_error_cookie_t   cookie  /**< */,
                         xcb_generic_error_t        **e  /**< */)
{
    return (xcb_glx_get_error_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_floatv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_floatv_reply_t *_aux = (xcb_glx_get_floatv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_floatv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float32_t);
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

xcb_glx_get_floatv_cookie_t
xcb_glx_get_floatv (xcb_connection_t      *c  /**< */,
                    xcb_glx_context_tag_t  context_tag  /**< */,
                    uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_FLOATV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_floatv_cookie_t xcb_ret;
    xcb_glx_get_floatv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_floatv_cookie_t
xcb_glx_get_floatv_unchecked (xcb_connection_t      *c  /**< */,
                              xcb_glx_context_tag_t  context_tag  /**< */,
                              uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_FLOATV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_floatv_cookie_t xcb_ret;
    xcb_glx_get_floatv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float32_t *
xcb_glx_get_floatv_data (const xcb_glx_get_floatv_reply_t *R  /**< */)
{
    return (xcb_glx_float32_t *) (R + 1);
}

int
xcb_glx_get_floatv_data_length (const xcb_glx_get_floatv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_floatv_data_end (const xcb_glx_get_floatv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_floatv_reply_t *
xcb_glx_get_floatv_reply (xcb_connection_t             *c  /**< */,
                          xcb_glx_get_floatv_cookie_t   cookie  /**< */,
                          xcb_generic_error_t         **e  /**< */)
{
    return (xcb_glx_get_floatv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_integerv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_integerv_reply_t *_aux = (xcb_glx_get_integerv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_integerv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(int32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(int32_t);
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

xcb_glx_get_integerv_cookie_t
xcb_glx_get_integerv (xcb_connection_t      *c  /**< */,
                      xcb_glx_context_tag_t  context_tag  /**< */,
                      uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_INTEGERV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_integerv_cookie_t xcb_ret;
    xcb_glx_get_integerv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_integerv_cookie_t
xcb_glx_get_integerv_unchecked (xcb_connection_t      *c  /**< */,
                                xcb_glx_context_tag_t  context_tag  /**< */,
                                uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_INTEGERV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_integerv_cookie_t xcb_ret;
    xcb_glx_get_integerv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_glx_get_integerv_data (const xcb_glx_get_integerv_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_glx_get_integerv_data_length (const xcb_glx_get_integerv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_integerv_data_end (const xcb_glx_get_integerv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_integerv_reply_t *
xcb_glx_get_integerv_reply (xcb_connection_t               *c  /**< */,
                            xcb_glx_get_integerv_cookie_t   cookie  /**< */,
                            xcb_generic_error_t           **e  /**< */)
{
    return (xcb_glx_get_integerv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_lightfv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_lightfv_reply_t *_aux = (xcb_glx_get_lightfv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_lightfv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float32_t);
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

xcb_glx_get_lightfv_cookie_t
xcb_glx_get_lightfv (xcb_connection_t      *c  /**< */,
                     xcb_glx_context_tag_t  context_tag  /**< */,
                     uint32_t               light  /**< */,
                     uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_LIGHTFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_lightfv_cookie_t xcb_ret;
    xcb_glx_get_lightfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.light = light;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_lightfv_cookie_t
xcb_glx_get_lightfv_unchecked (xcb_connection_t      *c  /**< */,
                               xcb_glx_context_tag_t  context_tag  /**< */,
                               uint32_t               light  /**< */,
                               uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_LIGHTFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_lightfv_cookie_t xcb_ret;
    xcb_glx_get_lightfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.light = light;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float32_t *
xcb_glx_get_lightfv_data (const xcb_glx_get_lightfv_reply_t *R  /**< */)
{
    return (xcb_glx_float32_t *) (R + 1);
}

int
xcb_glx_get_lightfv_data_length (const xcb_glx_get_lightfv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_lightfv_data_end (const xcb_glx_get_lightfv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_lightfv_reply_t *
xcb_glx_get_lightfv_reply (xcb_connection_t              *c  /**< */,
                           xcb_glx_get_lightfv_cookie_t   cookie  /**< */,
                           xcb_generic_error_t          **e  /**< */)
{
    return (xcb_glx_get_lightfv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_lightiv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_lightiv_reply_t *_aux = (xcb_glx_get_lightiv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_lightiv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(int32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(int32_t);
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

xcb_glx_get_lightiv_cookie_t
xcb_glx_get_lightiv (xcb_connection_t      *c  /**< */,
                     xcb_glx_context_tag_t  context_tag  /**< */,
                     uint32_t               light  /**< */,
                     uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_LIGHTIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_lightiv_cookie_t xcb_ret;
    xcb_glx_get_lightiv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.light = light;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_lightiv_cookie_t
xcb_glx_get_lightiv_unchecked (xcb_connection_t      *c  /**< */,
                               xcb_glx_context_tag_t  context_tag  /**< */,
                               uint32_t               light  /**< */,
                               uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_LIGHTIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_lightiv_cookie_t xcb_ret;
    xcb_glx_get_lightiv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.light = light;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_glx_get_lightiv_data (const xcb_glx_get_lightiv_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_glx_get_lightiv_data_length (const xcb_glx_get_lightiv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_lightiv_data_end (const xcb_glx_get_lightiv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_lightiv_reply_t *
xcb_glx_get_lightiv_reply (xcb_connection_t              *c  /**< */,
                           xcb_glx_get_lightiv_cookie_t   cookie  /**< */,
                           xcb_generic_error_t          **e  /**< */)
{
    return (xcb_glx_get_lightiv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_mapdv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_mapdv_reply_t *_aux = (xcb_glx_get_mapdv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_mapdv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float64_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float64_t);
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

xcb_glx_get_mapdv_cookie_t
xcb_glx_get_mapdv (xcb_connection_t      *c  /**< */,
                   xcb_glx_context_tag_t  context_tag  /**< */,
                   uint32_t               target  /**< */,
                   uint32_t               query  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MAPDV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_mapdv_cookie_t xcb_ret;
    xcb_glx_get_mapdv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.query = query;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_mapdv_cookie_t
xcb_glx_get_mapdv_unchecked (xcb_connection_t      *c  /**< */,
                             xcb_glx_context_tag_t  context_tag  /**< */,
                             uint32_t               target  /**< */,
                             uint32_t               query  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MAPDV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_mapdv_cookie_t xcb_ret;
    xcb_glx_get_mapdv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.query = query;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float64_t *
xcb_glx_get_mapdv_data (const xcb_glx_get_mapdv_reply_t *R  /**< */)
{
    return (xcb_glx_float64_t *) (R + 1);
}

int
xcb_glx_get_mapdv_data_length (const xcb_glx_get_mapdv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_mapdv_data_end (const xcb_glx_get_mapdv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float64_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_mapdv_reply_t *
xcb_glx_get_mapdv_reply (xcb_connection_t            *c  /**< */,
                         xcb_glx_get_mapdv_cookie_t   cookie  /**< */,
                         xcb_generic_error_t        **e  /**< */)
{
    return (xcb_glx_get_mapdv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_mapfv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_mapfv_reply_t *_aux = (xcb_glx_get_mapfv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_mapfv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float32_t);
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

xcb_glx_get_mapfv_cookie_t
xcb_glx_get_mapfv (xcb_connection_t      *c  /**< */,
                   xcb_glx_context_tag_t  context_tag  /**< */,
                   uint32_t               target  /**< */,
                   uint32_t               query  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MAPFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_mapfv_cookie_t xcb_ret;
    xcb_glx_get_mapfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.query = query;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_mapfv_cookie_t
xcb_glx_get_mapfv_unchecked (xcb_connection_t      *c  /**< */,
                             xcb_glx_context_tag_t  context_tag  /**< */,
                             uint32_t               target  /**< */,
                             uint32_t               query  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MAPFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_mapfv_cookie_t xcb_ret;
    xcb_glx_get_mapfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.query = query;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float32_t *
xcb_glx_get_mapfv_data (const xcb_glx_get_mapfv_reply_t *R  /**< */)
{
    return (xcb_glx_float32_t *) (R + 1);
}

int
xcb_glx_get_mapfv_data_length (const xcb_glx_get_mapfv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_mapfv_data_end (const xcb_glx_get_mapfv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_mapfv_reply_t *
xcb_glx_get_mapfv_reply (xcb_connection_t            *c  /**< */,
                         xcb_glx_get_mapfv_cookie_t   cookie  /**< */,
                         xcb_generic_error_t        **e  /**< */)
{
    return (xcb_glx_get_mapfv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_mapiv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_mapiv_reply_t *_aux = (xcb_glx_get_mapiv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_mapiv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(int32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(int32_t);
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

xcb_glx_get_mapiv_cookie_t
xcb_glx_get_mapiv (xcb_connection_t      *c  /**< */,
                   xcb_glx_context_tag_t  context_tag  /**< */,
                   uint32_t               target  /**< */,
                   uint32_t               query  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MAPIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_mapiv_cookie_t xcb_ret;
    xcb_glx_get_mapiv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.query = query;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_mapiv_cookie_t
xcb_glx_get_mapiv_unchecked (xcb_connection_t      *c  /**< */,
                             xcb_glx_context_tag_t  context_tag  /**< */,
                             uint32_t               target  /**< */,
                             uint32_t               query  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MAPIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_mapiv_cookie_t xcb_ret;
    xcb_glx_get_mapiv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.query = query;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_glx_get_mapiv_data (const xcb_glx_get_mapiv_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_glx_get_mapiv_data_length (const xcb_glx_get_mapiv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_mapiv_data_end (const xcb_glx_get_mapiv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_mapiv_reply_t *
xcb_glx_get_mapiv_reply (xcb_connection_t            *c  /**< */,
                         xcb_glx_get_mapiv_cookie_t   cookie  /**< */,
                         xcb_generic_error_t        **e  /**< */)
{
    return (xcb_glx_get_mapiv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_materialfv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_materialfv_reply_t *_aux = (xcb_glx_get_materialfv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_materialfv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float32_t);
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

xcb_glx_get_materialfv_cookie_t
xcb_glx_get_materialfv (xcb_connection_t      *c  /**< */,
                        xcb_glx_context_tag_t  context_tag  /**< */,
                        uint32_t               face  /**< */,
                        uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MATERIALFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_materialfv_cookie_t xcb_ret;
    xcb_glx_get_materialfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.face = face;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_materialfv_cookie_t
xcb_glx_get_materialfv_unchecked (xcb_connection_t      *c  /**< */,
                                  xcb_glx_context_tag_t  context_tag  /**< */,
                                  uint32_t               face  /**< */,
                                  uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MATERIALFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_materialfv_cookie_t xcb_ret;
    xcb_glx_get_materialfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.face = face;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float32_t *
xcb_glx_get_materialfv_data (const xcb_glx_get_materialfv_reply_t *R  /**< */)
{
    return (xcb_glx_float32_t *) (R + 1);
}

int
xcb_glx_get_materialfv_data_length (const xcb_glx_get_materialfv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_materialfv_data_end (const xcb_glx_get_materialfv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_materialfv_reply_t *
xcb_glx_get_materialfv_reply (xcb_connection_t                 *c  /**< */,
                              xcb_glx_get_materialfv_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_glx_get_materialfv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_materialiv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_materialiv_reply_t *_aux = (xcb_glx_get_materialiv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_materialiv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(int32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(int32_t);
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

xcb_glx_get_materialiv_cookie_t
xcb_glx_get_materialiv (xcb_connection_t      *c  /**< */,
                        xcb_glx_context_tag_t  context_tag  /**< */,
                        uint32_t               face  /**< */,
                        uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MATERIALIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_materialiv_cookie_t xcb_ret;
    xcb_glx_get_materialiv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.face = face;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_materialiv_cookie_t
xcb_glx_get_materialiv_unchecked (xcb_connection_t      *c  /**< */,
                                  xcb_glx_context_tag_t  context_tag  /**< */,
                                  uint32_t               face  /**< */,
                                  uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MATERIALIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_materialiv_cookie_t xcb_ret;
    xcb_glx_get_materialiv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.face = face;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_glx_get_materialiv_data (const xcb_glx_get_materialiv_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_glx_get_materialiv_data_length (const xcb_glx_get_materialiv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_materialiv_data_end (const xcb_glx_get_materialiv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_materialiv_reply_t *
xcb_glx_get_materialiv_reply (xcb_connection_t                 *c  /**< */,
                              xcb_glx_get_materialiv_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_glx_get_materialiv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_pixel_mapfv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_pixel_mapfv_reply_t *_aux = (xcb_glx_get_pixel_mapfv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_pixel_mapfv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float32_t);
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

xcb_glx_get_pixel_mapfv_cookie_t
xcb_glx_get_pixel_mapfv (xcb_connection_t      *c  /**< */,
                         xcb_glx_context_tag_t  context_tag  /**< */,
                         uint32_t               map  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_PIXEL_MAPFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_pixel_mapfv_cookie_t xcb_ret;
    xcb_glx_get_pixel_mapfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.map = map;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_pixel_mapfv_cookie_t
xcb_glx_get_pixel_mapfv_unchecked (xcb_connection_t      *c  /**< */,
                                   xcb_glx_context_tag_t  context_tag  /**< */,
                                   uint32_t               map  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_PIXEL_MAPFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_pixel_mapfv_cookie_t xcb_ret;
    xcb_glx_get_pixel_mapfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.map = map;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float32_t *
xcb_glx_get_pixel_mapfv_data (const xcb_glx_get_pixel_mapfv_reply_t *R  /**< */)
{
    return (xcb_glx_float32_t *) (R + 1);
}

int
xcb_glx_get_pixel_mapfv_data_length (const xcb_glx_get_pixel_mapfv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_pixel_mapfv_data_end (const xcb_glx_get_pixel_mapfv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_pixel_mapfv_reply_t *
xcb_glx_get_pixel_mapfv_reply (xcb_connection_t                  *c  /**< */,
                               xcb_glx_get_pixel_mapfv_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_glx_get_pixel_mapfv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_pixel_mapuiv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_pixel_mapuiv_reply_t *_aux = (xcb_glx_get_pixel_mapuiv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_pixel_mapuiv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(uint32_t);
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

xcb_glx_get_pixel_mapuiv_cookie_t
xcb_glx_get_pixel_mapuiv (xcb_connection_t      *c  /**< */,
                          xcb_glx_context_tag_t  context_tag  /**< */,
                          uint32_t               map  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_PIXEL_MAPUIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_pixel_mapuiv_cookie_t xcb_ret;
    xcb_glx_get_pixel_mapuiv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.map = map;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_pixel_mapuiv_cookie_t
xcb_glx_get_pixel_mapuiv_unchecked (xcb_connection_t      *c  /**< */,
                                    xcb_glx_context_tag_t  context_tag  /**< */,
                                    uint32_t               map  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_PIXEL_MAPUIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_pixel_mapuiv_cookie_t xcb_ret;
    xcb_glx_get_pixel_mapuiv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.map = map;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_glx_get_pixel_mapuiv_data (const xcb_glx_get_pixel_mapuiv_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_glx_get_pixel_mapuiv_data_length (const xcb_glx_get_pixel_mapuiv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_pixel_mapuiv_data_end (const xcb_glx_get_pixel_mapuiv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_pixel_mapuiv_reply_t *
xcb_glx_get_pixel_mapuiv_reply (xcb_connection_t                   *c  /**< */,
                                xcb_glx_get_pixel_mapuiv_cookie_t   cookie  /**< */,
                                xcb_generic_error_t               **e  /**< */)
{
    return (xcb_glx_get_pixel_mapuiv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_pixel_mapusv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_pixel_mapusv_reply_t *_aux = (xcb_glx_get_pixel_mapusv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_pixel_mapusv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(uint16_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint16_t);
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

xcb_glx_get_pixel_mapusv_cookie_t
xcb_glx_get_pixel_mapusv (xcb_connection_t      *c  /**< */,
                          xcb_glx_context_tag_t  context_tag  /**< */,
                          uint32_t               map  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_PIXEL_MAPUSV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_pixel_mapusv_cookie_t xcb_ret;
    xcb_glx_get_pixel_mapusv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.map = map;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_pixel_mapusv_cookie_t
xcb_glx_get_pixel_mapusv_unchecked (xcb_connection_t      *c  /**< */,
                                    xcb_glx_context_tag_t  context_tag  /**< */,
                                    uint32_t               map  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_PIXEL_MAPUSV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_pixel_mapusv_cookie_t xcb_ret;
    xcb_glx_get_pixel_mapusv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.map = map;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint16_t *
xcb_glx_get_pixel_mapusv_data (const xcb_glx_get_pixel_mapusv_reply_t *R  /**< */)
{
    return (uint16_t *) (R + 1);
}

int
xcb_glx_get_pixel_mapusv_data_length (const xcb_glx_get_pixel_mapusv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_pixel_mapusv_data_end (const xcb_glx_get_pixel_mapusv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint16_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_pixel_mapusv_reply_t *
xcb_glx_get_pixel_mapusv_reply (xcb_connection_t                   *c  /**< */,
                                xcb_glx_get_pixel_mapusv_cookie_t   cookie  /**< */,
                                xcb_generic_error_t               **e  /**< */)
{
    return (xcb_glx_get_pixel_mapusv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_polygon_stipple_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_polygon_stipple_reply_t *_aux = (xcb_glx_get_polygon_stipple_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_polygon_stipple_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += (_aux->length * 4) * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
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

xcb_glx_get_polygon_stipple_cookie_t
xcb_glx_get_polygon_stipple (xcb_connection_t      *c  /**< */,
                             xcb_glx_context_tag_t  context_tag  /**< */,
                             uint8_t                lsb_first  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_POLYGON_STIPPLE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_polygon_stipple_cookie_t xcb_ret;
    xcb_glx_get_polygon_stipple_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.lsb_first = lsb_first;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_polygon_stipple_cookie_t
xcb_glx_get_polygon_stipple_unchecked (xcb_connection_t      *c  /**< */,
                                       xcb_glx_context_tag_t  context_tag  /**< */,
                                       uint8_t                lsb_first  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_POLYGON_STIPPLE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_polygon_stipple_cookie_t xcb_ret;
    xcb_glx_get_polygon_stipple_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.lsb_first = lsb_first;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_glx_get_polygon_stipple_data (const xcb_glx_get_polygon_stipple_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_glx_get_polygon_stipple_data_length (const xcb_glx_get_polygon_stipple_reply_t *R  /**< */)
{
    return (R->length * 4);
}

xcb_generic_iterator_t
xcb_glx_get_polygon_stipple_data_end (const xcb_glx_get_polygon_stipple_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->length * 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_polygon_stipple_reply_t *
xcb_glx_get_polygon_stipple_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_glx_get_polygon_stipple_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_glx_get_polygon_stipple_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_string_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_string_reply_t *_aux = (xcb_glx_get_string_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_string_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* string */
    xcb_block_len += _aux->n * sizeof(char);
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

xcb_glx_get_string_cookie_t
xcb_glx_get_string (xcb_connection_t      *c  /**< */,
                    xcb_glx_context_tag_t  context_tag  /**< */,
                    uint32_t               name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_STRING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_string_cookie_t xcb_ret;
    xcb_glx_get_string_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.name = name;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_string_cookie_t
xcb_glx_get_string_unchecked (xcb_connection_t      *c  /**< */,
                              xcb_glx_context_tag_t  context_tag  /**< */,
                              uint32_t               name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_STRING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_string_cookie_t xcb_ret;
    xcb_glx_get_string_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.name = name;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_glx_get_string_string (const xcb_glx_get_string_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_glx_get_string_string_length (const xcb_glx_get_string_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_string_string_end (const xcb_glx_get_string_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_string_reply_t *
xcb_glx_get_string_reply (xcb_connection_t             *c  /**< */,
                          xcb_glx_get_string_cookie_t   cookie  /**< */,
                          xcb_generic_error_t         **e  /**< */)
{
    return (xcb_glx_get_string_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_tex_envfv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_tex_envfv_reply_t *_aux = (xcb_glx_get_tex_envfv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_tex_envfv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float32_t);
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

xcb_glx_get_tex_envfv_cookie_t
xcb_glx_get_tex_envfv (xcb_connection_t      *c  /**< */,
                       xcb_glx_context_tag_t  context_tag  /**< */,
                       uint32_t               target  /**< */,
                       uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_ENVFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_envfv_cookie_t xcb_ret;
    xcb_glx_get_tex_envfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_tex_envfv_cookie_t
xcb_glx_get_tex_envfv_unchecked (xcb_connection_t      *c  /**< */,
                                 xcb_glx_context_tag_t  context_tag  /**< */,
                                 uint32_t               target  /**< */,
                                 uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_ENVFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_envfv_cookie_t xcb_ret;
    xcb_glx_get_tex_envfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float32_t *
xcb_glx_get_tex_envfv_data (const xcb_glx_get_tex_envfv_reply_t *R  /**< */)
{
    return (xcb_glx_float32_t *) (R + 1);
}

int
xcb_glx_get_tex_envfv_data_length (const xcb_glx_get_tex_envfv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_tex_envfv_data_end (const xcb_glx_get_tex_envfv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_tex_envfv_reply_t *
xcb_glx_get_tex_envfv_reply (xcb_connection_t                *c  /**< */,
                             xcb_glx_get_tex_envfv_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_glx_get_tex_envfv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_tex_enviv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_tex_enviv_reply_t *_aux = (xcb_glx_get_tex_enviv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_tex_enviv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(int32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(int32_t);
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

xcb_glx_get_tex_enviv_cookie_t
xcb_glx_get_tex_enviv (xcb_connection_t      *c  /**< */,
                       xcb_glx_context_tag_t  context_tag  /**< */,
                       uint32_t               target  /**< */,
                       uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_ENVIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_enviv_cookie_t xcb_ret;
    xcb_glx_get_tex_enviv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_tex_enviv_cookie_t
xcb_glx_get_tex_enviv_unchecked (xcb_connection_t      *c  /**< */,
                                 xcb_glx_context_tag_t  context_tag  /**< */,
                                 uint32_t               target  /**< */,
                                 uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_ENVIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_enviv_cookie_t xcb_ret;
    xcb_glx_get_tex_enviv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_glx_get_tex_enviv_data (const xcb_glx_get_tex_enviv_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_glx_get_tex_enviv_data_length (const xcb_glx_get_tex_enviv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_tex_enviv_data_end (const xcb_glx_get_tex_enviv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_tex_enviv_reply_t *
xcb_glx_get_tex_enviv_reply (xcb_connection_t                *c  /**< */,
                             xcb_glx_get_tex_enviv_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_glx_get_tex_enviv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_tex_gendv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_tex_gendv_reply_t *_aux = (xcb_glx_get_tex_gendv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_tex_gendv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float64_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float64_t);
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

xcb_glx_get_tex_gendv_cookie_t
xcb_glx_get_tex_gendv (xcb_connection_t      *c  /**< */,
                       xcb_glx_context_tag_t  context_tag  /**< */,
                       uint32_t               coord  /**< */,
                       uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_GENDV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_gendv_cookie_t xcb_ret;
    xcb_glx_get_tex_gendv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.coord = coord;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_tex_gendv_cookie_t
xcb_glx_get_tex_gendv_unchecked (xcb_connection_t      *c  /**< */,
                                 xcb_glx_context_tag_t  context_tag  /**< */,
                                 uint32_t               coord  /**< */,
                                 uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_GENDV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_gendv_cookie_t xcb_ret;
    xcb_glx_get_tex_gendv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.coord = coord;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float64_t *
xcb_glx_get_tex_gendv_data (const xcb_glx_get_tex_gendv_reply_t *R  /**< */)
{
    return (xcb_glx_float64_t *) (R + 1);
}

int
xcb_glx_get_tex_gendv_data_length (const xcb_glx_get_tex_gendv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_tex_gendv_data_end (const xcb_glx_get_tex_gendv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float64_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_tex_gendv_reply_t *
xcb_glx_get_tex_gendv_reply (xcb_connection_t                *c  /**< */,
                             xcb_glx_get_tex_gendv_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_glx_get_tex_gendv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_tex_genfv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_tex_genfv_reply_t *_aux = (xcb_glx_get_tex_genfv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_tex_genfv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float32_t);
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

xcb_glx_get_tex_genfv_cookie_t
xcb_glx_get_tex_genfv (xcb_connection_t      *c  /**< */,
                       xcb_glx_context_tag_t  context_tag  /**< */,
                       uint32_t               coord  /**< */,
                       uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_GENFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_genfv_cookie_t xcb_ret;
    xcb_glx_get_tex_genfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.coord = coord;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_tex_genfv_cookie_t
xcb_glx_get_tex_genfv_unchecked (xcb_connection_t      *c  /**< */,
                                 xcb_glx_context_tag_t  context_tag  /**< */,
                                 uint32_t               coord  /**< */,
                                 uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_GENFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_genfv_cookie_t xcb_ret;
    xcb_glx_get_tex_genfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.coord = coord;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float32_t *
xcb_glx_get_tex_genfv_data (const xcb_glx_get_tex_genfv_reply_t *R  /**< */)
{
    return (xcb_glx_float32_t *) (R + 1);
}

int
xcb_glx_get_tex_genfv_data_length (const xcb_glx_get_tex_genfv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_tex_genfv_data_end (const xcb_glx_get_tex_genfv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_tex_genfv_reply_t *
xcb_glx_get_tex_genfv_reply (xcb_connection_t                *c  /**< */,
                             xcb_glx_get_tex_genfv_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_glx_get_tex_genfv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_tex_geniv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_tex_geniv_reply_t *_aux = (xcb_glx_get_tex_geniv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_tex_geniv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(int32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(int32_t);
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

xcb_glx_get_tex_geniv_cookie_t
xcb_glx_get_tex_geniv (xcb_connection_t      *c  /**< */,
                       xcb_glx_context_tag_t  context_tag  /**< */,
                       uint32_t               coord  /**< */,
                       uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_GENIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_geniv_cookie_t xcb_ret;
    xcb_glx_get_tex_geniv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.coord = coord;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_tex_geniv_cookie_t
xcb_glx_get_tex_geniv_unchecked (xcb_connection_t      *c  /**< */,
                                 xcb_glx_context_tag_t  context_tag  /**< */,
                                 uint32_t               coord  /**< */,
                                 uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_GENIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_geniv_cookie_t xcb_ret;
    xcb_glx_get_tex_geniv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.coord = coord;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_glx_get_tex_geniv_data (const xcb_glx_get_tex_geniv_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_glx_get_tex_geniv_data_length (const xcb_glx_get_tex_geniv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_tex_geniv_data_end (const xcb_glx_get_tex_geniv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_tex_geniv_reply_t *
xcb_glx_get_tex_geniv_reply (xcb_connection_t                *c  /**< */,
                             xcb_glx_get_tex_geniv_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_glx_get_tex_geniv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_tex_image_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_tex_image_reply_t *_aux = (xcb_glx_get_tex_image_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_tex_image_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += (_aux->length * 4) * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
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

xcb_glx_get_tex_image_cookie_t
xcb_glx_get_tex_image (xcb_connection_t      *c  /**< */,
                       xcb_glx_context_tag_t  context_tag  /**< */,
                       uint32_t               target  /**< */,
                       int32_t                level  /**< */,
                       uint32_t               format  /**< */,
                       uint32_t               type  /**< */,
                       uint8_t                swap_bytes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_IMAGE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_image_cookie_t xcb_ret;
    xcb_glx_get_tex_image_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.level = level;
    xcb_out.format = format;
    xcb_out.type = type;
    xcb_out.swap_bytes = swap_bytes;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_tex_image_cookie_t
xcb_glx_get_tex_image_unchecked (xcb_connection_t      *c  /**< */,
                                 xcb_glx_context_tag_t  context_tag  /**< */,
                                 uint32_t               target  /**< */,
                                 int32_t                level  /**< */,
                                 uint32_t               format  /**< */,
                                 uint32_t               type  /**< */,
                                 uint8_t                swap_bytes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_IMAGE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_image_cookie_t xcb_ret;
    xcb_glx_get_tex_image_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.level = level;
    xcb_out.format = format;
    xcb_out.type = type;
    xcb_out.swap_bytes = swap_bytes;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_glx_get_tex_image_data (const xcb_glx_get_tex_image_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_glx_get_tex_image_data_length (const xcb_glx_get_tex_image_reply_t *R  /**< */)
{
    return (R->length * 4);
}

xcb_generic_iterator_t
xcb_glx_get_tex_image_data_end (const xcb_glx_get_tex_image_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->length * 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_tex_image_reply_t *
xcb_glx_get_tex_image_reply (xcb_connection_t                *c  /**< */,
                             xcb_glx_get_tex_image_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_glx_get_tex_image_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_tex_parameterfv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_tex_parameterfv_reply_t *_aux = (xcb_glx_get_tex_parameterfv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_tex_parameterfv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float32_t);
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

xcb_glx_get_tex_parameterfv_cookie_t
xcb_glx_get_tex_parameterfv (xcb_connection_t      *c  /**< */,
                             xcb_glx_context_tag_t  context_tag  /**< */,
                             uint32_t               target  /**< */,
                             uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_PARAMETERFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_parameterfv_cookie_t xcb_ret;
    xcb_glx_get_tex_parameterfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_tex_parameterfv_cookie_t
xcb_glx_get_tex_parameterfv_unchecked (xcb_connection_t      *c  /**< */,
                                       xcb_glx_context_tag_t  context_tag  /**< */,
                                       uint32_t               target  /**< */,
                                       uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_PARAMETERFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_parameterfv_cookie_t xcb_ret;
    xcb_glx_get_tex_parameterfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float32_t *
xcb_glx_get_tex_parameterfv_data (const xcb_glx_get_tex_parameterfv_reply_t *R  /**< */)
{
    return (xcb_glx_float32_t *) (R + 1);
}

int
xcb_glx_get_tex_parameterfv_data_length (const xcb_glx_get_tex_parameterfv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_tex_parameterfv_data_end (const xcb_glx_get_tex_parameterfv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_tex_parameterfv_reply_t *
xcb_glx_get_tex_parameterfv_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_glx_get_tex_parameterfv_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_glx_get_tex_parameterfv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_tex_parameteriv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_tex_parameteriv_reply_t *_aux = (xcb_glx_get_tex_parameteriv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_tex_parameteriv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(int32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(int32_t);
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

xcb_glx_get_tex_parameteriv_cookie_t
xcb_glx_get_tex_parameteriv (xcb_connection_t      *c  /**< */,
                             xcb_glx_context_tag_t  context_tag  /**< */,
                             uint32_t               target  /**< */,
                             uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_PARAMETERIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_parameteriv_cookie_t xcb_ret;
    xcb_glx_get_tex_parameteriv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_tex_parameteriv_cookie_t
xcb_glx_get_tex_parameteriv_unchecked (xcb_connection_t      *c  /**< */,
                                       xcb_glx_context_tag_t  context_tag  /**< */,
                                       uint32_t               target  /**< */,
                                       uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_PARAMETERIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_parameteriv_cookie_t xcb_ret;
    xcb_glx_get_tex_parameteriv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_glx_get_tex_parameteriv_data (const xcb_glx_get_tex_parameteriv_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_glx_get_tex_parameteriv_data_length (const xcb_glx_get_tex_parameteriv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_tex_parameteriv_data_end (const xcb_glx_get_tex_parameteriv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_tex_parameteriv_reply_t *
xcb_glx_get_tex_parameteriv_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_glx_get_tex_parameteriv_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_glx_get_tex_parameteriv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_tex_level_parameterfv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_tex_level_parameterfv_reply_t *_aux = (xcb_glx_get_tex_level_parameterfv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_tex_level_parameterfv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float32_t);
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

xcb_glx_get_tex_level_parameterfv_cookie_t
xcb_glx_get_tex_level_parameterfv (xcb_connection_t      *c  /**< */,
                                   xcb_glx_context_tag_t  context_tag  /**< */,
                                   uint32_t               target  /**< */,
                                   int32_t                level  /**< */,
                                   uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_LEVEL_PARAMETERFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_level_parameterfv_cookie_t xcb_ret;
    xcb_glx_get_tex_level_parameterfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.level = level;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_tex_level_parameterfv_cookie_t
xcb_glx_get_tex_level_parameterfv_unchecked (xcb_connection_t      *c  /**< */,
                                             xcb_glx_context_tag_t  context_tag  /**< */,
                                             uint32_t               target  /**< */,
                                             int32_t                level  /**< */,
                                             uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_LEVEL_PARAMETERFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_level_parameterfv_cookie_t xcb_ret;
    xcb_glx_get_tex_level_parameterfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.level = level;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float32_t *
xcb_glx_get_tex_level_parameterfv_data (const xcb_glx_get_tex_level_parameterfv_reply_t *R  /**< */)
{
    return (xcb_glx_float32_t *) (R + 1);
}

int
xcb_glx_get_tex_level_parameterfv_data_length (const xcb_glx_get_tex_level_parameterfv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_tex_level_parameterfv_data_end (const xcb_glx_get_tex_level_parameterfv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_tex_level_parameterfv_reply_t *
xcb_glx_get_tex_level_parameterfv_reply (xcb_connection_t                            *c  /**< */,
                                         xcb_glx_get_tex_level_parameterfv_cookie_t   cookie  /**< */,
                                         xcb_generic_error_t                        **e  /**< */)
{
    return (xcb_glx_get_tex_level_parameterfv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_tex_level_parameteriv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_tex_level_parameteriv_reply_t *_aux = (xcb_glx_get_tex_level_parameteriv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_tex_level_parameteriv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(int32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(int32_t);
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

xcb_glx_get_tex_level_parameteriv_cookie_t
xcb_glx_get_tex_level_parameteriv (xcb_connection_t      *c  /**< */,
                                   xcb_glx_context_tag_t  context_tag  /**< */,
                                   uint32_t               target  /**< */,
                                   int32_t                level  /**< */,
                                   uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_LEVEL_PARAMETERIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_level_parameteriv_cookie_t xcb_ret;
    xcb_glx_get_tex_level_parameteriv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.level = level;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_tex_level_parameteriv_cookie_t
xcb_glx_get_tex_level_parameteriv_unchecked (xcb_connection_t      *c  /**< */,
                                             xcb_glx_context_tag_t  context_tag  /**< */,
                                             uint32_t               target  /**< */,
                                             int32_t                level  /**< */,
                                             uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_TEX_LEVEL_PARAMETERIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_tex_level_parameteriv_cookie_t xcb_ret;
    xcb_glx_get_tex_level_parameteriv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.level = level;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_glx_get_tex_level_parameteriv_data (const xcb_glx_get_tex_level_parameteriv_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_glx_get_tex_level_parameteriv_data_length (const xcb_glx_get_tex_level_parameteriv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_tex_level_parameteriv_data_end (const xcb_glx_get_tex_level_parameteriv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_tex_level_parameteriv_reply_t *
xcb_glx_get_tex_level_parameteriv_reply (xcb_connection_t                            *c  /**< */,
                                         xcb_glx_get_tex_level_parameteriv_cookie_t   cookie  /**< */,
                                         xcb_generic_error_t                        **e  /**< */)
{
    return (xcb_glx_get_tex_level_parameteriv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_glx_is_list_cookie_t
xcb_glx_is_list (xcb_connection_t      *c  /**< */,
                 xcb_glx_context_tag_t  context_tag  /**< */,
                 uint32_t               list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_IS_LIST,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_is_list_cookie_t xcb_ret;
    xcb_glx_is_list_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.list = list;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_is_list_cookie_t
xcb_glx_is_list_unchecked (xcb_connection_t      *c  /**< */,
                           xcb_glx_context_tag_t  context_tag  /**< */,
                           uint32_t               list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_IS_LIST,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_is_list_cookie_t xcb_ret;
    xcb_glx_is_list_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.list = list;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_is_list_reply_t *
xcb_glx_is_list_reply (xcb_connection_t          *c  /**< */,
                       xcb_glx_is_list_cookie_t   cookie  /**< */,
                       xcb_generic_error_t      **e  /**< */)
{
    return (xcb_glx_is_list_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_glx_flush_checked (xcb_connection_t      *c  /**< */,
                       xcb_glx_context_tag_t  context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_FLUSH,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_flush_request_t xcb_out;

    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_flush (xcb_connection_t      *c  /**< */,
               xcb_glx_context_tag_t  context_tag  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_FLUSH,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_flush_request_t xcb_out;

    xcb_out.context_tag = context_tag;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_are_textures_resident_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_are_textures_resident_request_t *_aux = (xcb_glx_are_textures_resident_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_are_textures_resident_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* textures */
    xcb_block_len += _aux->n * sizeof(uint32_t);
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

xcb_glx_are_textures_resident_cookie_t
xcb_glx_are_textures_resident (xcb_connection_t      *c  /**< */,
                               xcb_glx_context_tag_t  context_tag  /**< */,
                               int32_t                n  /**< */,
                               const uint32_t        *textures  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_ARE_TEXTURES_RESIDENT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_glx_are_textures_resident_cookie_t xcb_ret;
    xcb_glx_are_textures_resident_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.n = n;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t textures */
    xcb_parts[4].iov_base = (char *) textures;
    xcb_parts[4].iov_len = n * sizeof(xcb_glx_bool32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_are_textures_resident_cookie_t
xcb_glx_are_textures_resident_unchecked (xcb_connection_t      *c  /**< */,
                                         xcb_glx_context_tag_t  context_tag  /**< */,
                                         int32_t                n  /**< */,
                                         const uint32_t        *textures  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_ARE_TEXTURES_RESIDENT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_glx_are_textures_resident_cookie_t xcb_ret;
    xcb_glx_are_textures_resident_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.n = n;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t textures */
    xcb_parts[4].iov_base = (char *) textures;
    xcb_parts[4].iov_len = n * sizeof(xcb_glx_bool32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_glx_are_textures_resident_data (const xcb_glx_are_textures_resident_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_glx_are_textures_resident_data_length (const xcb_glx_are_textures_resident_reply_t *R  /**< */)
{
    return (R->length * 4);
}

xcb_generic_iterator_t
xcb_glx_are_textures_resident_data_end (const xcb_glx_are_textures_resident_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->length * 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_are_textures_resident_reply_t *
xcb_glx_are_textures_resident_reply (xcb_connection_t                        *c  /**< */,
                                     xcb_glx_are_textures_resident_cookie_t   cookie  /**< */,
                                     xcb_generic_error_t                    **e  /**< */)
{
    return (xcb_glx_are_textures_resident_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_delete_textures_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_delete_textures_request_t *_aux = (xcb_glx_delete_textures_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_delete_textures_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* textures */
    xcb_block_len += _aux->n * sizeof(uint32_t);
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

xcb_void_cookie_t
xcb_glx_delete_textures_checked (xcb_connection_t      *c  /**< */,
                                 xcb_glx_context_tag_t  context_tag  /**< */,
                                 int32_t                n  /**< */,
                                 const uint32_t        *textures  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DELETE_TEXTURES,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_delete_textures_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.n = n;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t textures */
    xcb_parts[4].iov_base = (char *) textures;
    xcb_parts[4].iov_len = n * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_delete_textures (xcb_connection_t      *c  /**< */,
                         xcb_glx_context_tag_t  context_tag  /**< */,
                         int32_t                n  /**< */,
                         const uint32_t        *textures  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DELETE_TEXTURES,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_delete_textures_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.n = n;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t textures */
    xcb_parts[4].iov_base = (char *) textures;
    xcb_parts[4].iov_len = n * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_gen_textures_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_gen_textures_reply_t *_aux = (xcb_glx_gen_textures_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_gen_textures_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->length * sizeof(uint32_t);
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

xcb_glx_gen_textures_cookie_t
xcb_glx_gen_textures (xcb_connection_t      *c  /**< */,
                      xcb_glx_context_tag_t  context_tag  /**< */,
                      int32_t                n  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GEN_TEXTURES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_gen_textures_cookie_t xcb_ret;
    xcb_glx_gen_textures_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.n = n;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_gen_textures_cookie_t
xcb_glx_gen_textures_unchecked (xcb_connection_t      *c  /**< */,
                                xcb_glx_context_tag_t  context_tag  /**< */,
                                int32_t                n  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GEN_TEXTURES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_gen_textures_cookie_t xcb_ret;
    xcb_glx_gen_textures_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.n = n;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_glx_gen_textures_data (const xcb_glx_gen_textures_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_glx_gen_textures_data_length (const xcb_glx_gen_textures_reply_t *R  /**< */)
{
    return R->length;
}

xcb_generic_iterator_t
xcb_glx_gen_textures_data_end (const xcb_glx_gen_textures_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->length);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_gen_textures_reply_t *
xcb_glx_gen_textures_reply (xcb_connection_t               *c  /**< */,
                            xcb_glx_gen_textures_cookie_t   cookie  /**< */,
                            xcb_generic_error_t           **e  /**< */)
{
    return (xcb_glx_gen_textures_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_glx_is_texture_cookie_t
xcb_glx_is_texture (xcb_connection_t      *c  /**< */,
                    xcb_glx_context_tag_t  context_tag  /**< */,
                    uint32_t               texture  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_IS_TEXTURE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_is_texture_cookie_t xcb_ret;
    xcb_glx_is_texture_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.texture = texture;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_is_texture_cookie_t
xcb_glx_is_texture_unchecked (xcb_connection_t      *c  /**< */,
                              xcb_glx_context_tag_t  context_tag  /**< */,
                              uint32_t               texture  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_IS_TEXTURE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_is_texture_cookie_t xcb_ret;
    xcb_glx_is_texture_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.texture = texture;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_is_texture_reply_t *
xcb_glx_is_texture_reply (xcb_connection_t             *c  /**< */,
                          xcb_glx_is_texture_cookie_t   cookie  /**< */,
                          xcb_generic_error_t         **e  /**< */)
{
    return (xcb_glx_is_texture_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_color_table_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_color_table_reply_t *_aux = (xcb_glx_get_color_table_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_color_table_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += (_aux->length * 4) * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
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

xcb_glx_get_color_table_cookie_t
xcb_glx_get_color_table (xcb_connection_t      *c  /**< */,
                         xcb_glx_context_tag_t  context_tag  /**< */,
                         uint32_t               target  /**< */,
                         uint32_t               format  /**< */,
                         uint32_t               type  /**< */,
                         uint8_t                swap_bytes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_COLOR_TABLE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_color_table_cookie_t xcb_ret;
    xcb_glx_get_color_table_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.format = format;
    xcb_out.type = type;
    xcb_out.swap_bytes = swap_bytes;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_color_table_cookie_t
xcb_glx_get_color_table_unchecked (xcb_connection_t      *c  /**< */,
                                   xcb_glx_context_tag_t  context_tag  /**< */,
                                   uint32_t               target  /**< */,
                                   uint32_t               format  /**< */,
                                   uint32_t               type  /**< */,
                                   uint8_t                swap_bytes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_COLOR_TABLE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_color_table_cookie_t xcb_ret;
    xcb_glx_get_color_table_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.format = format;
    xcb_out.type = type;
    xcb_out.swap_bytes = swap_bytes;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_glx_get_color_table_data (const xcb_glx_get_color_table_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_glx_get_color_table_data_length (const xcb_glx_get_color_table_reply_t *R  /**< */)
{
    return (R->length * 4);
}

xcb_generic_iterator_t
xcb_glx_get_color_table_data_end (const xcb_glx_get_color_table_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->length * 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_color_table_reply_t *
xcb_glx_get_color_table_reply (xcb_connection_t                  *c  /**< */,
                               xcb_glx_get_color_table_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_glx_get_color_table_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_color_table_parameterfv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_color_table_parameterfv_reply_t *_aux = (xcb_glx_get_color_table_parameterfv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_color_table_parameterfv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float32_t);
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

xcb_glx_get_color_table_parameterfv_cookie_t
xcb_glx_get_color_table_parameterfv (xcb_connection_t      *c  /**< */,
                                     xcb_glx_context_tag_t  context_tag  /**< */,
                                     uint32_t               target  /**< */,
                                     uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_COLOR_TABLE_PARAMETERFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_color_table_parameterfv_cookie_t xcb_ret;
    xcb_glx_get_color_table_parameterfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_color_table_parameterfv_cookie_t
xcb_glx_get_color_table_parameterfv_unchecked (xcb_connection_t      *c  /**< */,
                                               xcb_glx_context_tag_t  context_tag  /**< */,
                                               uint32_t               target  /**< */,
                                               uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_COLOR_TABLE_PARAMETERFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_color_table_parameterfv_cookie_t xcb_ret;
    xcb_glx_get_color_table_parameterfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float32_t *
xcb_glx_get_color_table_parameterfv_data (const xcb_glx_get_color_table_parameterfv_reply_t *R  /**< */)
{
    return (xcb_glx_float32_t *) (R + 1);
}

int
xcb_glx_get_color_table_parameterfv_data_length (const xcb_glx_get_color_table_parameterfv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_color_table_parameterfv_data_end (const xcb_glx_get_color_table_parameterfv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_color_table_parameterfv_reply_t *
xcb_glx_get_color_table_parameterfv_reply (xcb_connection_t                              *c  /**< */,
                                           xcb_glx_get_color_table_parameterfv_cookie_t   cookie  /**< */,
                                           xcb_generic_error_t                          **e  /**< */)
{
    return (xcb_glx_get_color_table_parameterfv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_color_table_parameteriv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_color_table_parameteriv_reply_t *_aux = (xcb_glx_get_color_table_parameteriv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_color_table_parameteriv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(int32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(int32_t);
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

xcb_glx_get_color_table_parameteriv_cookie_t
xcb_glx_get_color_table_parameteriv (xcb_connection_t      *c  /**< */,
                                     xcb_glx_context_tag_t  context_tag  /**< */,
                                     uint32_t               target  /**< */,
                                     uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_COLOR_TABLE_PARAMETERIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_color_table_parameteriv_cookie_t xcb_ret;
    xcb_glx_get_color_table_parameteriv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_color_table_parameteriv_cookie_t
xcb_glx_get_color_table_parameteriv_unchecked (xcb_connection_t      *c  /**< */,
                                               xcb_glx_context_tag_t  context_tag  /**< */,
                                               uint32_t               target  /**< */,
                                               uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_COLOR_TABLE_PARAMETERIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_color_table_parameteriv_cookie_t xcb_ret;
    xcb_glx_get_color_table_parameteriv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_glx_get_color_table_parameteriv_data (const xcb_glx_get_color_table_parameteriv_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_glx_get_color_table_parameteriv_data_length (const xcb_glx_get_color_table_parameteriv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_color_table_parameteriv_data_end (const xcb_glx_get_color_table_parameteriv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_color_table_parameteriv_reply_t *
xcb_glx_get_color_table_parameteriv_reply (xcb_connection_t                              *c  /**< */,
                                           xcb_glx_get_color_table_parameteriv_cookie_t   cookie  /**< */,
                                           xcb_generic_error_t                          **e  /**< */)
{
    return (xcb_glx_get_color_table_parameteriv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_convolution_filter_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_convolution_filter_reply_t *_aux = (xcb_glx_get_convolution_filter_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_convolution_filter_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += (_aux->length * 4) * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
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

xcb_glx_get_convolution_filter_cookie_t
xcb_glx_get_convolution_filter (xcb_connection_t      *c  /**< */,
                                xcb_glx_context_tag_t  context_tag  /**< */,
                                uint32_t               target  /**< */,
                                uint32_t               format  /**< */,
                                uint32_t               type  /**< */,
                                uint8_t                swap_bytes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_CONVOLUTION_FILTER,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_convolution_filter_cookie_t xcb_ret;
    xcb_glx_get_convolution_filter_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.format = format;
    xcb_out.type = type;
    xcb_out.swap_bytes = swap_bytes;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_convolution_filter_cookie_t
xcb_glx_get_convolution_filter_unchecked (xcb_connection_t      *c  /**< */,
                                          xcb_glx_context_tag_t  context_tag  /**< */,
                                          uint32_t               target  /**< */,
                                          uint32_t               format  /**< */,
                                          uint32_t               type  /**< */,
                                          uint8_t                swap_bytes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_CONVOLUTION_FILTER,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_convolution_filter_cookie_t xcb_ret;
    xcb_glx_get_convolution_filter_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.format = format;
    xcb_out.type = type;
    xcb_out.swap_bytes = swap_bytes;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_glx_get_convolution_filter_data (const xcb_glx_get_convolution_filter_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_glx_get_convolution_filter_data_length (const xcb_glx_get_convolution_filter_reply_t *R  /**< */)
{
    return (R->length * 4);
}

xcb_generic_iterator_t
xcb_glx_get_convolution_filter_data_end (const xcb_glx_get_convolution_filter_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->length * 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_convolution_filter_reply_t *
xcb_glx_get_convolution_filter_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_glx_get_convolution_filter_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */)
{
    return (xcb_glx_get_convolution_filter_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_convolution_parameterfv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_convolution_parameterfv_reply_t *_aux = (xcb_glx_get_convolution_parameterfv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_convolution_parameterfv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float32_t);
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

xcb_glx_get_convolution_parameterfv_cookie_t
xcb_glx_get_convolution_parameterfv (xcb_connection_t      *c  /**< */,
                                     xcb_glx_context_tag_t  context_tag  /**< */,
                                     uint32_t               target  /**< */,
                                     uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_CONVOLUTION_PARAMETERFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_convolution_parameterfv_cookie_t xcb_ret;
    xcb_glx_get_convolution_parameterfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_convolution_parameterfv_cookie_t
xcb_glx_get_convolution_parameterfv_unchecked (xcb_connection_t      *c  /**< */,
                                               xcb_glx_context_tag_t  context_tag  /**< */,
                                               uint32_t               target  /**< */,
                                               uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_CONVOLUTION_PARAMETERFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_convolution_parameterfv_cookie_t xcb_ret;
    xcb_glx_get_convolution_parameterfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float32_t *
xcb_glx_get_convolution_parameterfv_data (const xcb_glx_get_convolution_parameterfv_reply_t *R  /**< */)
{
    return (xcb_glx_float32_t *) (R + 1);
}

int
xcb_glx_get_convolution_parameterfv_data_length (const xcb_glx_get_convolution_parameterfv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_convolution_parameterfv_data_end (const xcb_glx_get_convolution_parameterfv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_convolution_parameterfv_reply_t *
xcb_glx_get_convolution_parameterfv_reply (xcb_connection_t                              *c  /**< */,
                                           xcb_glx_get_convolution_parameterfv_cookie_t   cookie  /**< */,
                                           xcb_generic_error_t                          **e  /**< */)
{
    return (xcb_glx_get_convolution_parameterfv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_convolution_parameteriv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_convolution_parameteriv_reply_t *_aux = (xcb_glx_get_convolution_parameteriv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_convolution_parameteriv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(int32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(int32_t);
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

xcb_glx_get_convolution_parameteriv_cookie_t
xcb_glx_get_convolution_parameteriv (xcb_connection_t      *c  /**< */,
                                     xcb_glx_context_tag_t  context_tag  /**< */,
                                     uint32_t               target  /**< */,
                                     uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_CONVOLUTION_PARAMETERIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_convolution_parameteriv_cookie_t xcb_ret;
    xcb_glx_get_convolution_parameteriv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_convolution_parameteriv_cookie_t
xcb_glx_get_convolution_parameteriv_unchecked (xcb_connection_t      *c  /**< */,
                                               xcb_glx_context_tag_t  context_tag  /**< */,
                                               uint32_t               target  /**< */,
                                               uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_CONVOLUTION_PARAMETERIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_convolution_parameteriv_cookie_t xcb_ret;
    xcb_glx_get_convolution_parameteriv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_glx_get_convolution_parameteriv_data (const xcb_glx_get_convolution_parameteriv_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_glx_get_convolution_parameteriv_data_length (const xcb_glx_get_convolution_parameteriv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_convolution_parameteriv_data_end (const xcb_glx_get_convolution_parameteriv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_convolution_parameteriv_reply_t *
xcb_glx_get_convolution_parameteriv_reply (xcb_connection_t                              *c  /**< */,
                                           xcb_glx_get_convolution_parameteriv_cookie_t   cookie  /**< */,
                                           xcb_generic_error_t                          **e  /**< */)
{
    return (xcb_glx_get_convolution_parameteriv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_separable_filter_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_separable_filter_reply_t *_aux = (xcb_glx_get_separable_filter_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_separable_filter_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* rows_and_cols */
    xcb_block_len += (_aux->length * 4) * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
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

xcb_glx_get_separable_filter_cookie_t
xcb_glx_get_separable_filter (xcb_connection_t      *c  /**< */,
                              xcb_glx_context_tag_t  context_tag  /**< */,
                              uint32_t               target  /**< */,
                              uint32_t               format  /**< */,
                              uint32_t               type  /**< */,
                              uint8_t                swap_bytes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_SEPARABLE_FILTER,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_separable_filter_cookie_t xcb_ret;
    xcb_glx_get_separable_filter_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.format = format;
    xcb_out.type = type;
    xcb_out.swap_bytes = swap_bytes;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_separable_filter_cookie_t
xcb_glx_get_separable_filter_unchecked (xcb_connection_t      *c  /**< */,
                                        xcb_glx_context_tag_t  context_tag  /**< */,
                                        uint32_t               target  /**< */,
                                        uint32_t               format  /**< */,
                                        uint32_t               type  /**< */,
                                        uint8_t                swap_bytes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_SEPARABLE_FILTER,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_separable_filter_cookie_t xcb_ret;
    xcb_glx_get_separable_filter_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.format = format;
    xcb_out.type = type;
    xcb_out.swap_bytes = swap_bytes;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_glx_get_separable_filter_rows_and_cols (const xcb_glx_get_separable_filter_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_glx_get_separable_filter_rows_and_cols_length (const xcb_glx_get_separable_filter_reply_t *R  /**< */)
{
    return (R->length * 4);
}

xcb_generic_iterator_t
xcb_glx_get_separable_filter_rows_and_cols_end (const xcb_glx_get_separable_filter_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->length * 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_separable_filter_reply_t *
xcb_glx_get_separable_filter_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_glx_get_separable_filter_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */)
{
    return (xcb_glx_get_separable_filter_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_histogram_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_histogram_reply_t *_aux = (xcb_glx_get_histogram_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_histogram_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += (_aux->length * 4) * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
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

xcb_glx_get_histogram_cookie_t
xcb_glx_get_histogram (xcb_connection_t      *c  /**< */,
                       xcb_glx_context_tag_t  context_tag  /**< */,
                       uint32_t               target  /**< */,
                       uint32_t               format  /**< */,
                       uint32_t               type  /**< */,
                       uint8_t                swap_bytes  /**< */,
                       uint8_t                reset  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_HISTOGRAM,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_histogram_cookie_t xcb_ret;
    xcb_glx_get_histogram_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.format = format;
    xcb_out.type = type;
    xcb_out.swap_bytes = swap_bytes;
    xcb_out.reset = reset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_histogram_cookie_t
xcb_glx_get_histogram_unchecked (xcb_connection_t      *c  /**< */,
                                 xcb_glx_context_tag_t  context_tag  /**< */,
                                 uint32_t               target  /**< */,
                                 uint32_t               format  /**< */,
                                 uint32_t               type  /**< */,
                                 uint8_t                swap_bytes  /**< */,
                                 uint8_t                reset  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_HISTOGRAM,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_histogram_cookie_t xcb_ret;
    xcb_glx_get_histogram_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.format = format;
    xcb_out.type = type;
    xcb_out.swap_bytes = swap_bytes;
    xcb_out.reset = reset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_glx_get_histogram_data (const xcb_glx_get_histogram_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_glx_get_histogram_data_length (const xcb_glx_get_histogram_reply_t *R  /**< */)
{
    return (R->length * 4);
}

xcb_generic_iterator_t
xcb_glx_get_histogram_data_end (const xcb_glx_get_histogram_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->length * 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_histogram_reply_t *
xcb_glx_get_histogram_reply (xcb_connection_t                *c  /**< */,
                             xcb_glx_get_histogram_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_glx_get_histogram_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_histogram_parameterfv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_histogram_parameterfv_reply_t *_aux = (xcb_glx_get_histogram_parameterfv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_histogram_parameterfv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float32_t);
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

xcb_glx_get_histogram_parameterfv_cookie_t
xcb_glx_get_histogram_parameterfv (xcb_connection_t      *c  /**< */,
                                   xcb_glx_context_tag_t  context_tag  /**< */,
                                   uint32_t               target  /**< */,
                                   uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_HISTOGRAM_PARAMETERFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_histogram_parameterfv_cookie_t xcb_ret;
    xcb_glx_get_histogram_parameterfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_histogram_parameterfv_cookie_t
xcb_glx_get_histogram_parameterfv_unchecked (xcb_connection_t      *c  /**< */,
                                             xcb_glx_context_tag_t  context_tag  /**< */,
                                             uint32_t               target  /**< */,
                                             uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_HISTOGRAM_PARAMETERFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_histogram_parameterfv_cookie_t xcb_ret;
    xcb_glx_get_histogram_parameterfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float32_t *
xcb_glx_get_histogram_parameterfv_data (const xcb_glx_get_histogram_parameterfv_reply_t *R  /**< */)
{
    return (xcb_glx_float32_t *) (R + 1);
}

int
xcb_glx_get_histogram_parameterfv_data_length (const xcb_glx_get_histogram_parameterfv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_histogram_parameterfv_data_end (const xcb_glx_get_histogram_parameterfv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_histogram_parameterfv_reply_t *
xcb_glx_get_histogram_parameterfv_reply (xcb_connection_t                            *c  /**< */,
                                         xcb_glx_get_histogram_parameterfv_cookie_t   cookie  /**< */,
                                         xcb_generic_error_t                        **e  /**< */)
{
    return (xcb_glx_get_histogram_parameterfv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_histogram_parameteriv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_histogram_parameteriv_reply_t *_aux = (xcb_glx_get_histogram_parameteriv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_histogram_parameteriv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(int32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(int32_t);
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

xcb_glx_get_histogram_parameteriv_cookie_t
xcb_glx_get_histogram_parameteriv (xcb_connection_t      *c  /**< */,
                                   xcb_glx_context_tag_t  context_tag  /**< */,
                                   uint32_t               target  /**< */,
                                   uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_HISTOGRAM_PARAMETERIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_histogram_parameteriv_cookie_t xcb_ret;
    xcb_glx_get_histogram_parameteriv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_histogram_parameteriv_cookie_t
xcb_glx_get_histogram_parameteriv_unchecked (xcb_connection_t      *c  /**< */,
                                             xcb_glx_context_tag_t  context_tag  /**< */,
                                             uint32_t               target  /**< */,
                                             uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_HISTOGRAM_PARAMETERIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_histogram_parameteriv_cookie_t xcb_ret;
    xcb_glx_get_histogram_parameteriv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_glx_get_histogram_parameteriv_data (const xcb_glx_get_histogram_parameteriv_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_glx_get_histogram_parameteriv_data_length (const xcb_glx_get_histogram_parameteriv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_histogram_parameteriv_data_end (const xcb_glx_get_histogram_parameteriv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_histogram_parameteriv_reply_t *
xcb_glx_get_histogram_parameteriv_reply (xcb_connection_t                            *c  /**< */,
                                         xcb_glx_get_histogram_parameteriv_cookie_t   cookie  /**< */,
                                         xcb_generic_error_t                        **e  /**< */)
{
    return (xcb_glx_get_histogram_parameteriv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_minmax_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_minmax_reply_t *_aux = (xcb_glx_get_minmax_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_minmax_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += (_aux->length * 4) * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
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

xcb_glx_get_minmax_cookie_t
xcb_glx_get_minmax (xcb_connection_t      *c  /**< */,
                    xcb_glx_context_tag_t  context_tag  /**< */,
                    uint32_t               target  /**< */,
                    uint32_t               format  /**< */,
                    uint32_t               type  /**< */,
                    uint8_t                swap_bytes  /**< */,
                    uint8_t                reset  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MINMAX,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_minmax_cookie_t xcb_ret;
    xcb_glx_get_minmax_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.format = format;
    xcb_out.type = type;
    xcb_out.swap_bytes = swap_bytes;
    xcb_out.reset = reset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_minmax_cookie_t
xcb_glx_get_minmax_unchecked (xcb_connection_t      *c  /**< */,
                              xcb_glx_context_tag_t  context_tag  /**< */,
                              uint32_t               target  /**< */,
                              uint32_t               format  /**< */,
                              uint32_t               type  /**< */,
                              uint8_t                swap_bytes  /**< */,
                              uint8_t                reset  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MINMAX,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_minmax_cookie_t xcb_ret;
    xcb_glx_get_minmax_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.format = format;
    xcb_out.type = type;
    xcb_out.swap_bytes = swap_bytes;
    xcb_out.reset = reset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_glx_get_minmax_data (const xcb_glx_get_minmax_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_glx_get_minmax_data_length (const xcb_glx_get_minmax_reply_t *R  /**< */)
{
    return (R->length * 4);
}

xcb_generic_iterator_t
xcb_glx_get_minmax_data_end (const xcb_glx_get_minmax_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->length * 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_minmax_reply_t *
xcb_glx_get_minmax_reply (xcb_connection_t             *c  /**< */,
                          xcb_glx_get_minmax_cookie_t   cookie  /**< */,
                          xcb_generic_error_t         **e  /**< */)
{
    return (xcb_glx_get_minmax_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_minmax_parameterfv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_minmax_parameterfv_reply_t *_aux = (xcb_glx_get_minmax_parameterfv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_minmax_parameterfv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(xcb_glx_float32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_glx_float32_t);
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

xcb_glx_get_minmax_parameterfv_cookie_t
xcb_glx_get_minmax_parameterfv (xcb_connection_t      *c  /**< */,
                                xcb_glx_context_tag_t  context_tag  /**< */,
                                uint32_t               target  /**< */,
                                uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MINMAX_PARAMETERFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_minmax_parameterfv_cookie_t xcb_ret;
    xcb_glx_get_minmax_parameterfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_minmax_parameterfv_cookie_t
xcb_glx_get_minmax_parameterfv_unchecked (xcb_connection_t      *c  /**< */,
                                          xcb_glx_context_tag_t  context_tag  /**< */,
                                          uint32_t               target  /**< */,
                                          uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MINMAX_PARAMETERFV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_minmax_parameterfv_cookie_t xcb_ret;
    xcb_glx_get_minmax_parameterfv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_float32_t *
xcb_glx_get_minmax_parameterfv_data (const xcb_glx_get_minmax_parameterfv_reply_t *R  /**< */)
{
    return (xcb_glx_float32_t *) (R + 1);
}

int
xcb_glx_get_minmax_parameterfv_data_length (const xcb_glx_get_minmax_parameterfv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_minmax_parameterfv_data_end (const xcb_glx_get_minmax_parameterfv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_glx_float32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_minmax_parameterfv_reply_t *
xcb_glx_get_minmax_parameterfv_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_glx_get_minmax_parameterfv_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */)
{
    return (xcb_glx_get_minmax_parameterfv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_minmax_parameteriv_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_minmax_parameteriv_reply_t *_aux = (xcb_glx_get_minmax_parameteriv_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_minmax_parameteriv_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(int32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(int32_t);
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

xcb_glx_get_minmax_parameteriv_cookie_t
xcb_glx_get_minmax_parameteriv (xcb_connection_t      *c  /**< */,
                                xcb_glx_context_tag_t  context_tag  /**< */,
                                uint32_t               target  /**< */,
                                uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MINMAX_PARAMETERIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_minmax_parameteriv_cookie_t xcb_ret;
    xcb_glx_get_minmax_parameteriv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_minmax_parameteriv_cookie_t
xcb_glx_get_minmax_parameteriv_unchecked (xcb_connection_t      *c  /**< */,
                                          xcb_glx_context_tag_t  context_tag  /**< */,
                                          uint32_t               target  /**< */,
                                          uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_MINMAX_PARAMETERIV,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_minmax_parameteriv_cookie_t xcb_ret;
    xcb_glx_get_minmax_parameteriv_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_glx_get_minmax_parameteriv_data (const xcb_glx_get_minmax_parameteriv_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_glx_get_minmax_parameteriv_data_length (const xcb_glx_get_minmax_parameteriv_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_minmax_parameteriv_data_end (const xcb_glx_get_minmax_parameteriv_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_minmax_parameteriv_reply_t *
xcb_glx_get_minmax_parameteriv_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_glx_get_minmax_parameteriv_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */)
{
    return (xcb_glx_get_minmax_parameteriv_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_compressed_tex_image_arb_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_compressed_tex_image_arb_reply_t *_aux = (xcb_glx_get_compressed_tex_image_arb_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_compressed_tex_image_arb_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += (_aux->length * 4) * sizeof(uint8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(uint8_t);
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

xcb_glx_get_compressed_tex_image_arb_cookie_t
xcb_glx_get_compressed_tex_image_arb (xcb_connection_t      *c  /**< */,
                                      xcb_glx_context_tag_t  context_tag  /**< */,
                                      uint32_t               target  /**< */,
                                      int32_t                level  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_COMPRESSED_TEX_IMAGE_ARB,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_compressed_tex_image_arb_cookie_t xcb_ret;
    xcb_glx_get_compressed_tex_image_arb_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.level = level;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_compressed_tex_image_arb_cookie_t
xcb_glx_get_compressed_tex_image_arb_unchecked (xcb_connection_t      *c  /**< */,
                                                xcb_glx_context_tag_t  context_tag  /**< */,
                                                uint32_t               target  /**< */,
                                                int32_t                level  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_COMPRESSED_TEX_IMAGE_ARB,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_compressed_tex_image_arb_cookie_t xcb_ret;
    xcb_glx_get_compressed_tex_image_arb_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.level = level;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_glx_get_compressed_tex_image_arb_data (const xcb_glx_get_compressed_tex_image_arb_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_glx_get_compressed_tex_image_arb_data_length (const xcb_glx_get_compressed_tex_image_arb_reply_t *R  /**< */)
{
    return (R->length * 4);
}

xcb_generic_iterator_t
xcb_glx_get_compressed_tex_image_arb_data_end (const xcb_glx_get_compressed_tex_image_arb_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->length * 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_compressed_tex_image_arb_reply_t *
xcb_glx_get_compressed_tex_image_arb_reply (xcb_connection_t                               *c  /**< */,
                                            xcb_glx_get_compressed_tex_image_arb_cookie_t   cookie  /**< */,
                                            xcb_generic_error_t                           **e  /**< */)
{
    return (xcb_glx_get_compressed_tex_image_arb_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_delete_queries_arb_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_delete_queries_arb_request_t *_aux = (xcb_glx_delete_queries_arb_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_delete_queries_arb_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* ids */
    xcb_block_len += _aux->n * sizeof(uint32_t);
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

xcb_void_cookie_t
xcb_glx_delete_queries_arb_checked (xcb_connection_t      *c  /**< */,
                                    xcb_glx_context_tag_t  context_tag  /**< */,
                                    int32_t                n  /**< */,
                                    const uint32_t        *ids  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DELETE_QUERIES_ARB,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_delete_queries_arb_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.n = n;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t ids */
    xcb_parts[4].iov_base = (char *) ids;
    xcb_parts[4].iov_len = n * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_glx_delete_queries_arb (xcb_connection_t      *c  /**< */,
                            xcb_glx_context_tag_t  context_tag  /**< */,
                            int32_t                n  /**< */,
                            const uint32_t        *ids  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_DELETE_QUERIES_ARB,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_glx_delete_queries_arb_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.n = n;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t ids */
    xcb_parts[4].iov_base = (char *) ids;
    xcb_parts[4].iov_len = n * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_glx_gen_queries_arb_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_gen_queries_arb_reply_t *_aux = (xcb_glx_gen_queries_arb_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_gen_queries_arb_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->length * sizeof(uint32_t);
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

xcb_glx_gen_queries_arb_cookie_t
xcb_glx_gen_queries_arb (xcb_connection_t      *c  /**< */,
                         xcb_glx_context_tag_t  context_tag  /**< */,
                         int32_t                n  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GEN_QUERIES_ARB,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_gen_queries_arb_cookie_t xcb_ret;
    xcb_glx_gen_queries_arb_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.n = n;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_gen_queries_arb_cookie_t
xcb_glx_gen_queries_arb_unchecked (xcb_connection_t      *c  /**< */,
                                   xcb_glx_context_tag_t  context_tag  /**< */,
                                   int32_t                n  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GEN_QUERIES_ARB,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_gen_queries_arb_cookie_t xcb_ret;
    xcb_glx_gen_queries_arb_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.n = n;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_glx_gen_queries_arb_data (const xcb_glx_gen_queries_arb_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_glx_gen_queries_arb_data_length (const xcb_glx_gen_queries_arb_reply_t *R  /**< */)
{
    return R->length;
}

xcb_generic_iterator_t
xcb_glx_gen_queries_arb_data_end (const xcb_glx_gen_queries_arb_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->length);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_gen_queries_arb_reply_t *
xcb_glx_gen_queries_arb_reply (xcb_connection_t                  *c  /**< */,
                               xcb_glx_gen_queries_arb_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_glx_gen_queries_arb_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_glx_is_query_arb_cookie_t
xcb_glx_is_query_arb (xcb_connection_t      *c  /**< */,
                      xcb_glx_context_tag_t  context_tag  /**< */,
                      uint32_t               id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_IS_QUERY_ARB,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_is_query_arb_cookie_t xcb_ret;
    xcb_glx_is_query_arb_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.id = id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_is_query_arb_cookie_t
xcb_glx_is_query_arb_unchecked (xcb_connection_t      *c  /**< */,
                                xcb_glx_context_tag_t  context_tag  /**< */,
                                uint32_t               id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_IS_QUERY_ARB,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_is_query_arb_cookie_t xcb_ret;
    xcb_glx_is_query_arb_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.id = id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_is_query_arb_reply_t *
xcb_glx_is_query_arb_reply (xcb_connection_t               *c  /**< */,
                            xcb_glx_is_query_arb_cookie_t   cookie  /**< */,
                            xcb_generic_error_t           **e  /**< */)
{
    return (xcb_glx_is_query_arb_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_queryiv_arb_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_queryiv_arb_reply_t *_aux = (xcb_glx_get_queryiv_arb_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_queryiv_arb_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(int32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(int32_t);
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

xcb_glx_get_queryiv_arb_cookie_t
xcb_glx_get_queryiv_arb (xcb_connection_t      *c  /**< */,
                         xcb_glx_context_tag_t  context_tag  /**< */,
                         uint32_t               target  /**< */,
                         uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_QUERYIV_ARB,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_queryiv_arb_cookie_t xcb_ret;
    xcb_glx_get_queryiv_arb_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_queryiv_arb_cookie_t
xcb_glx_get_queryiv_arb_unchecked (xcb_connection_t      *c  /**< */,
                                   xcb_glx_context_tag_t  context_tag  /**< */,
                                   uint32_t               target  /**< */,
                                   uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_QUERYIV_ARB,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_queryiv_arb_cookie_t xcb_ret;
    xcb_glx_get_queryiv_arb_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.target = target;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_glx_get_queryiv_arb_data (const xcb_glx_get_queryiv_arb_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_glx_get_queryiv_arb_data_length (const xcb_glx_get_queryiv_arb_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_queryiv_arb_data_end (const xcb_glx_get_queryiv_arb_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_queryiv_arb_reply_t *
xcb_glx_get_queryiv_arb_reply (xcb_connection_t                  *c  /**< */,
                               xcb_glx_get_queryiv_arb_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_glx_get_queryiv_arb_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_query_objectiv_arb_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_query_objectiv_arb_reply_t *_aux = (xcb_glx_get_query_objectiv_arb_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_query_objectiv_arb_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(int32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(int32_t);
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

xcb_glx_get_query_objectiv_arb_cookie_t
xcb_glx_get_query_objectiv_arb (xcb_connection_t      *c  /**< */,
                                xcb_glx_context_tag_t  context_tag  /**< */,
                                uint32_t               id  /**< */,
                                uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_QUERY_OBJECTIV_ARB,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_query_objectiv_arb_cookie_t xcb_ret;
    xcb_glx_get_query_objectiv_arb_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.id = id;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_query_objectiv_arb_cookie_t
xcb_glx_get_query_objectiv_arb_unchecked (xcb_connection_t      *c  /**< */,
                                          xcb_glx_context_tag_t  context_tag  /**< */,
                                          uint32_t               id  /**< */,
                                          uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_QUERY_OBJECTIV_ARB,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_query_objectiv_arb_cookie_t xcb_ret;
    xcb_glx_get_query_objectiv_arb_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.id = id;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_glx_get_query_objectiv_arb_data (const xcb_glx_get_query_objectiv_arb_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_glx_get_query_objectiv_arb_data_length (const xcb_glx_get_query_objectiv_arb_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_query_objectiv_arb_data_end (const xcb_glx_get_query_objectiv_arb_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_query_objectiv_arb_reply_t *
xcb_glx_get_query_objectiv_arb_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_glx_get_query_objectiv_arb_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */)
{
    return (xcb_glx_get_query_objectiv_arb_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_glx_get_query_objectuiv_arb_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_glx_get_query_objectuiv_arb_reply_t *_aux = (xcb_glx_get_query_objectuiv_arb_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_glx_get_query_objectuiv_arb_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->n * sizeof(uint32_t);
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

xcb_glx_get_query_objectuiv_arb_cookie_t
xcb_glx_get_query_objectuiv_arb (xcb_connection_t      *c  /**< */,
                                 xcb_glx_context_tag_t  context_tag  /**< */,
                                 uint32_t               id  /**< */,
                                 uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_QUERY_OBJECTUIV_ARB,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_query_objectuiv_arb_cookie_t xcb_ret;
    xcb_glx_get_query_objectuiv_arb_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.id = id;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_glx_get_query_objectuiv_arb_cookie_t
xcb_glx_get_query_objectuiv_arb_unchecked (xcb_connection_t      *c  /**< */,
                                           xcb_glx_context_tag_t  context_tag  /**< */,
                                           uint32_t               id  /**< */,
                                           uint32_t               pname  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_glx_id,
        /* opcode */ XCB_GLX_GET_QUERY_OBJECTUIV_ARB,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_glx_get_query_objectuiv_arb_cookie_t xcb_ret;
    xcb_glx_get_query_objectuiv_arb_request_t xcb_out;

    xcb_out.context_tag = context_tag;
    xcb_out.id = id;
    xcb_out.pname = pname;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_glx_get_query_objectuiv_arb_data (const xcb_glx_get_query_objectuiv_arb_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_glx_get_query_objectuiv_arb_data_length (const xcb_glx_get_query_objectuiv_arb_reply_t *R  /**< */)
{
    return R->n;
}

xcb_generic_iterator_t
xcb_glx_get_query_objectuiv_arb_data_end (const xcb_glx_get_query_objectuiv_arb_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->n);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_glx_get_query_objectuiv_arb_reply_t *
xcb_glx_get_query_objectuiv_arb_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_glx_get_query_objectuiv_arb_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */)
{
    return (xcb_glx_get_query_objectuiv_arb_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

