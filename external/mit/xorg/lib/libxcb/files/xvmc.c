/*
 * This file generated automatically from xvmc.xml by c_client.py.
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
#include "xvmc.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xv.h"

xcb_extension_t xcb_xvmc_id = { "XVideo-MotionCompensation", 0 };

void
xcb_xvmc_context_next (xcb_xvmc_context_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_xvmc_context_t);
}

xcb_generic_iterator_t
xcb_xvmc_context_end (xcb_xvmc_context_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_xvmc_surface_next (xcb_xvmc_surface_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_xvmc_surface_t);
}

xcb_generic_iterator_t
xcb_xvmc_surface_end (xcb_xvmc_surface_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_xvmc_subpicture_next (xcb_xvmc_subpicture_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_xvmc_subpicture_t);
}

xcb_generic_iterator_t
xcb_xvmc_subpicture_end (xcb_xvmc_subpicture_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_xvmc_surface_info_next (xcb_xvmc_surface_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_xvmc_surface_info_t);
}

xcb_generic_iterator_t
xcb_xvmc_surface_info_end (xcb_xvmc_surface_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

xcb_xvmc_query_version_cookie_t
xcb_xvmc_query_version (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xvmc_query_version_cookie_t xcb_ret;
    xcb_xvmc_query_version_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xvmc_query_version_cookie_t
xcb_xvmc_query_version_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xvmc_query_version_cookie_t xcb_ret;
    xcb_xvmc_query_version_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xvmc_query_version_reply_t *
xcb_xvmc_query_version_reply (xcb_connection_t                 *c  /**< */,
                              xcb_xvmc_query_version_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_xvmc_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_xvmc_list_surface_types_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xvmc_list_surface_types_reply_t *_aux = (xcb_xvmc_list_surface_types_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xvmc_list_surface_types_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* surfaces */
    xcb_block_len += _aux->num * sizeof(xcb_xvmc_surface_info_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_xvmc_surface_info_t);
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

xcb_xvmc_list_surface_types_cookie_t
xcb_xvmc_list_surface_types (xcb_connection_t *c  /**< */,
                             xcb_xv_port_t     port_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_LIST_SURFACE_TYPES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xvmc_list_surface_types_cookie_t xcb_ret;
    xcb_xvmc_list_surface_types_request_t xcb_out;

    xcb_out.port_id = port_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xvmc_list_surface_types_cookie_t
xcb_xvmc_list_surface_types_unchecked (xcb_connection_t *c  /**< */,
                                       xcb_xv_port_t     port_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_LIST_SURFACE_TYPES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xvmc_list_surface_types_cookie_t xcb_ret;
    xcb_xvmc_list_surface_types_request_t xcb_out;

    xcb_out.port_id = port_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xvmc_surface_info_t *
xcb_xvmc_list_surface_types_surfaces (const xcb_xvmc_list_surface_types_reply_t *R  /**< */)
{
    return (xcb_xvmc_surface_info_t *) (R + 1);
}

int
xcb_xvmc_list_surface_types_surfaces_length (const xcb_xvmc_list_surface_types_reply_t *R  /**< */)
{
    return R->num;
}

xcb_xvmc_surface_info_iterator_t
xcb_xvmc_list_surface_types_surfaces_iterator (const xcb_xvmc_list_surface_types_reply_t *R  /**< */)
{
    xcb_xvmc_surface_info_iterator_t i;
    i.data = (xcb_xvmc_surface_info_t *) (R + 1);
    i.rem = R->num;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xvmc_list_surface_types_reply_t *
xcb_xvmc_list_surface_types_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_xvmc_list_surface_types_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_xvmc_list_surface_types_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_xvmc_create_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xvmc_create_context_reply_t *_aux = (xcb_xvmc_create_context_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xvmc_create_context_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* priv_data */
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

xcb_xvmc_create_context_cookie_t
xcb_xvmc_create_context (xcb_connection_t   *c  /**< */,
                         xcb_xvmc_context_t  context_id  /**< */,
                         xcb_xv_port_t       port_id  /**< */,
                         xcb_xvmc_surface_t  surface_id  /**< */,
                         uint16_t            width  /**< */,
                         uint16_t            height  /**< */,
                         uint32_t            flags  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_CREATE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xvmc_create_context_cookie_t xcb_ret;
    xcb_xvmc_create_context_request_t xcb_out;

    xcb_out.context_id = context_id;
    xcb_out.port_id = port_id;
    xcb_out.surface_id = surface_id;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.flags = flags;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xvmc_create_context_cookie_t
xcb_xvmc_create_context_unchecked (xcb_connection_t   *c  /**< */,
                                   xcb_xvmc_context_t  context_id  /**< */,
                                   xcb_xv_port_t       port_id  /**< */,
                                   xcb_xvmc_surface_t  surface_id  /**< */,
                                   uint16_t            width  /**< */,
                                   uint16_t            height  /**< */,
                                   uint32_t            flags  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_CREATE_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xvmc_create_context_cookie_t xcb_ret;
    xcb_xvmc_create_context_request_t xcb_out;

    xcb_out.context_id = context_id;
    xcb_out.port_id = port_id;
    xcb_out.surface_id = surface_id;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.flags = flags;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_xvmc_create_context_priv_data (const xcb_xvmc_create_context_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_xvmc_create_context_priv_data_length (const xcb_xvmc_create_context_reply_t *R  /**< */)
{
    return R->length;
}

xcb_generic_iterator_t
xcb_xvmc_create_context_priv_data_end (const xcb_xvmc_create_context_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->length);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xvmc_create_context_reply_t *
xcb_xvmc_create_context_reply (xcb_connection_t                  *c  /**< */,
                               xcb_xvmc_create_context_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_xvmc_create_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_xvmc_destroy_context_checked (xcb_connection_t   *c  /**< */,
                                  xcb_xvmc_context_t  context_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_DESTROY_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xvmc_destroy_context_request_t xcb_out;

    xcb_out.context_id = context_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xvmc_destroy_context (xcb_connection_t   *c  /**< */,
                          xcb_xvmc_context_t  context_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_DESTROY_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xvmc_destroy_context_request_t xcb_out;

    xcb_out.context_id = context_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_xvmc_create_surface_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xvmc_create_surface_reply_t *_aux = (xcb_xvmc_create_surface_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xvmc_create_surface_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* priv_data */
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

xcb_xvmc_create_surface_cookie_t
xcb_xvmc_create_surface (xcb_connection_t   *c  /**< */,
                         xcb_xvmc_surface_t  surface_id  /**< */,
                         xcb_xvmc_context_t  context_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_CREATE_SURFACE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xvmc_create_surface_cookie_t xcb_ret;
    xcb_xvmc_create_surface_request_t xcb_out;

    xcb_out.surface_id = surface_id;
    xcb_out.context_id = context_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xvmc_create_surface_cookie_t
xcb_xvmc_create_surface_unchecked (xcb_connection_t   *c  /**< */,
                                   xcb_xvmc_surface_t  surface_id  /**< */,
                                   xcb_xvmc_context_t  context_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_CREATE_SURFACE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xvmc_create_surface_cookie_t xcb_ret;
    xcb_xvmc_create_surface_request_t xcb_out;

    xcb_out.surface_id = surface_id;
    xcb_out.context_id = context_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_xvmc_create_surface_priv_data (const xcb_xvmc_create_surface_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_xvmc_create_surface_priv_data_length (const xcb_xvmc_create_surface_reply_t *R  /**< */)
{
    return R->length;
}

xcb_generic_iterator_t
xcb_xvmc_create_surface_priv_data_end (const xcb_xvmc_create_surface_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->length);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xvmc_create_surface_reply_t *
xcb_xvmc_create_surface_reply (xcb_connection_t                  *c  /**< */,
                               xcb_xvmc_create_surface_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_xvmc_create_surface_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_xvmc_destroy_surface_checked (xcb_connection_t   *c  /**< */,
                                  xcb_xvmc_surface_t  surface_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_DESTROY_SURFACE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xvmc_destroy_surface_request_t xcb_out;

    xcb_out.surface_id = surface_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xvmc_destroy_surface (xcb_connection_t   *c  /**< */,
                          xcb_xvmc_surface_t  surface_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_DESTROY_SURFACE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xvmc_destroy_surface_request_t xcb_out;

    xcb_out.surface_id = surface_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_xvmc_create_subpicture_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xvmc_create_subpicture_reply_t *_aux = (xcb_xvmc_create_subpicture_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xvmc_create_subpicture_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* priv_data */
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

xcb_xvmc_create_subpicture_cookie_t
xcb_xvmc_create_subpicture (xcb_connection_t      *c  /**< */,
                            xcb_xvmc_subpicture_t  subpicture_id  /**< */,
                            xcb_xvmc_context_t     context  /**< */,
                            uint32_t               xvimage_id  /**< */,
                            uint16_t               width  /**< */,
                            uint16_t               height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_CREATE_SUBPICTURE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xvmc_create_subpicture_cookie_t xcb_ret;
    xcb_xvmc_create_subpicture_request_t xcb_out;

    xcb_out.subpicture_id = subpicture_id;
    xcb_out.context = context;
    xcb_out.xvimage_id = xvimage_id;
    xcb_out.width = width;
    xcb_out.height = height;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xvmc_create_subpicture_cookie_t
xcb_xvmc_create_subpicture_unchecked (xcb_connection_t      *c  /**< */,
                                      xcb_xvmc_subpicture_t  subpicture_id  /**< */,
                                      xcb_xvmc_context_t     context  /**< */,
                                      uint32_t               xvimage_id  /**< */,
                                      uint16_t               width  /**< */,
                                      uint16_t               height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_CREATE_SUBPICTURE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xvmc_create_subpicture_cookie_t xcb_ret;
    xcb_xvmc_create_subpicture_request_t xcb_out;

    xcb_out.subpicture_id = subpicture_id;
    xcb_out.context = context;
    xcb_out.xvimage_id = xvimage_id;
    xcb_out.width = width;
    xcb_out.height = height;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_xvmc_create_subpicture_priv_data (const xcb_xvmc_create_subpicture_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_xvmc_create_subpicture_priv_data_length (const xcb_xvmc_create_subpicture_reply_t *R  /**< */)
{
    return R->length;
}

xcb_generic_iterator_t
xcb_xvmc_create_subpicture_priv_data_end (const xcb_xvmc_create_subpicture_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->length);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xvmc_create_subpicture_reply_t *
xcb_xvmc_create_subpicture_reply (xcb_connection_t                     *c  /**< */,
                                  xcb_xvmc_create_subpicture_cookie_t   cookie  /**< */,
                                  xcb_generic_error_t                 **e  /**< */)
{
    return (xcb_xvmc_create_subpicture_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_xvmc_destroy_subpicture_checked (xcb_connection_t      *c  /**< */,
                                     xcb_xvmc_subpicture_t  subpicture_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_DESTROY_SUBPICTURE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xvmc_destroy_subpicture_request_t xcb_out;

    xcb_out.subpicture_id = subpicture_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xvmc_destroy_subpicture (xcb_connection_t      *c  /**< */,
                             xcb_xvmc_subpicture_t  subpicture_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_DESTROY_SUBPICTURE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xvmc_destroy_subpicture_request_t xcb_out;

    xcb_out.subpicture_id = subpicture_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_xvmc_list_subpicture_types_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xvmc_list_subpicture_types_reply_t *_aux = (xcb_xvmc_list_subpicture_types_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xvmc_list_subpicture_types_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* types */
    xcb_block_len += _aux->num * sizeof(xcb_xv_image_format_info_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_xv_image_format_info_t);
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

xcb_xvmc_list_subpicture_types_cookie_t
xcb_xvmc_list_subpicture_types (xcb_connection_t   *c  /**< */,
                                xcb_xv_port_t       port_id  /**< */,
                                xcb_xvmc_surface_t  surface_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_LIST_SUBPICTURE_TYPES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xvmc_list_subpicture_types_cookie_t xcb_ret;
    xcb_xvmc_list_subpicture_types_request_t xcb_out;

    xcb_out.port_id = port_id;
    xcb_out.surface_id = surface_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xvmc_list_subpicture_types_cookie_t
xcb_xvmc_list_subpicture_types_unchecked (xcb_connection_t   *c  /**< */,
                                          xcb_xv_port_t       port_id  /**< */,
                                          xcb_xvmc_surface_t  surface_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xvmc_id,
        /* opcode */ XCB_XVMC_LIST_SUBPICTURE_TYPES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xvmc_list_subpicture_types_cookie_t xcb_ret;
    xcb_xvmc_list_subpicture_types_request_t xcb_out;

    xcb_out.port_id = port_id;
    xcb_out.surface_id = surface_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_image_format_info_t *
xcb_xvmc_list_subpicture_types_types (const xcb_xvmc_list_subpicture_types_reply_t *R  /**< */)
{
    return (xcb_xv_image_format_info_t *) (R + 1);
}

int
xcb_xvmc_list_subpicture_types_types_length (const xcb_xvmc_list_subpicture_types_reply_t *R  /**< */)
{
    return R->num;
}

xcb_xv_image_format_info_iterator_t
xcb_xvmc_list_subpicture_types_types_iterator (const xcb_xvmc_list_subpicture_types_reply_t *R  /**< */)
{
    xcb_xv_image_format_info_iterator_t i;
    i.data = (xcb_xv_image_format_info_t *) (R + 1);
    i.rem = R->num;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xvmc_list_subpicture_types_reply_t *
xcb_xvmc_list_subpicture_types_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_xvmc_list_subpicture_types_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */)
{
    return (xcb_xvmc_list_subpicture_types_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

