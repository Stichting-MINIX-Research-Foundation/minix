/*
 * This file generated automatically from shape.xml by c_client.py.
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
#include "shape.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xproto.h"

xcb_extension_t xcb_shape_id = { "SHAPE", 0 };

void
xcb_shape_op_next (xcb_shape_op_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_shape_op_t);
}

xcb_generic_iterator_t
xcb_shape_op_end (xcb_shape_op_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_shape_kind_next (xcb_shape_kind_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_shape_kind_t);
}

xcb_generic_iterator_t
xcb_shape_kind_end (xcb_shape_kind_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

xcb_shape_query_version_cookie_t
xcb_shape_query_version (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_shape_query_version_cookie_t xcb_ret;
    xcb_shape_query_version_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shape_query_version_cookie_t
xcb_shape_query_version_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_shape_query_version_cookie_t xcb_ret;
    xcb_shape_query_version_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shape_query_version_reply_t *
xcb_shape_query_version_reply (xcb_connection_t                  *c  /**< */,
                               xcb_shape_query_version_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_shape_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_shape_rectangles_sizeof (const void  *_buffer  /**< */,
                             uint32_t     rectangles_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_shape_rectangles_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* rectangles */
    xcb_block_len += rectangles_len * sizeof(xcb_rectangle_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_rectangle_t);
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
xcb_shape_rectangles_checked (xcb_connection_t      *c  /**< */,
                              xcb_shape_op_t         operation  /**< */,
                              xcb_shape_kind_t       destination_kind  /**< */,
                              uint8_t                ordering  /**< */,
                              xcb_window_t           destination_window  /**< */,
                              int16_t                x_offset  /**< */,
                              int16_t                y_offset  /**< */,
                              uint32_t               rectangles_len  /**< */,
                              const xcb_rectangle_t *rectangles  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_RECTANGLES,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_shape_rectangles_request_t xcb_out;

    xcb_out.operation = operation;
    xcb_out.destination_kind = destination_kind;
    xcb_out.ordering = ordering;
    xcb_out.pad0 = 0;
    xcb_out.destination_window = destination_window;
    xcb_out.x_offset = x_offset;
    xcb_out.y_offset = y_offset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_rectangle_t rectangles */
    xcb_parts[4].iov_base = (char *) rectangles;
    xcb_parts[4].iov_len = rectangles_len * sizeof(xcb_rectangle_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shape_rectangles (xcb_connection_t      *c  /**< */,
                      xcb_shape_op_t         operation  /**< */,
                      xcb_shape_kind_t       destination_kind  /**< */,
                      uint8_t                ordering  /**< */,
                      xcb_window_t           destination_window  /**< */,
                      int16_t                x_offset  /**< */,
                      int16_t                y_offset  /**< */,
                      uint32_t               rectangles_len  /**< */,
                      const xcb_rectangle_t *rectangles  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_RECTANGLES,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_shape_rectangles_request_t xcb_out;

    xcb_out.operation = operation;
    xcb_out.destination_kind = destination_kind;
    xcb_out.ordering = ordering;
    xcb_out.pad0 = 0;
    xcb_out.destination_window = destination_window;
    xcb_out.x_offset = x_offset;
    xcb_out.y_offset = y_offset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_rectangle_t rectangles */
    xcb_parts[4].iov_base = (char *) rectangles;
    xcb_parts[4].iov_len = rectangles_len * sizeof(xcb_rectangle_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shape_mask_checked (xcb_connection_t *c  /**< */,
                        xcb_shape_op_t    operation  /**< */,
                        xcb_shape_kind_t  destination_kind  /**< */,
                        xcb_window_t      destination_window  /**< */,
                        int16_t           x_offset  /**< */,
                        int16_t           y_offset  /**< */,
                        xcb_pixmap_t      source_bitmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_MASK,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shape_mask_request_t xcb_out;

    xcb_out.operation = operation;
    xcb_out.destination_kind = destination_kind;
    memset(xcb_out.pad0, 0, 2);
    xcb_out.destination_window = destination_window;
    xcb_out.x_offset = x_offset;
    xcb_out.y_offset = y_offset;
    xcb_out.source_bitmap = source_bitmap;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shape_mask (xcb_connection_t *c  /**< */,
                xcb_shape_op_t    operation  /**< */,
                xcb_shape_kind_t  destination_kind  /**< */,
                xcb_window_t      destination_window  /**< */,
                int16_t           x_offset  /**< */,
                int16_t           y_offset  /**< */,
                xcb_pixmap_t      source_bitmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_MASK,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shape_mask_request_t xcb_out;

    xcb_out.operation = operation;
    xcb_out.destination_kind = destination_kind;
    memset(xcb_out.pad0, 0, 2);
    xcb_out.destination_window = destination_window;
    xcb_out.x_offset = x_offset;
    xcb_out.y_offset = y_offset;
    xcb_out.source_bitmap = source_bitmap;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shape_combine_checked (xcb_connection_t *c  /**< */,
                           xcb_shape_op_t    operation  /**< */,
                           xcb_shape_kind_t  destination_kind  /**< */,
                           xcb_shape_kind_t  source_kind  /**< */,
                           xcb_window_t      destination_window  /**< */,
                           int16_t           x_offset  /**< */,
                           int16_t           y_offset  /**< */,
                           xcb_window_t      source_window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_COMBINE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shape_combine_request_t xcb_out;

    xcb_out.operation = operation;
    xcb_out.destination_kind = destination_kind;
    xcb_out.source_kind = source_kind;
    xcb_out.pad0 = 0;
    xcb_out.destination_window = destination_window;
    xcb_out.x_offset = x_offset;
    xcb_out.y_offset = y_offset;
    xcb_out.source_window = source_window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shape_combine (xcb_connection_t *c  /**< */,
                   xcb_shape_op_t    operation  /**< */,
                   xcb_shape_kind_t  destination_kind  /**< */,
                   xcb_shape_kind_t  source_kind  /**< */,
                   xcb_window_t      destination_window  /**< */,
                   int16_t           x_offset  /**< */,
                   int16_t           y_offset  /**< */,
                   xcb_window_t      source_window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_COMBINE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shape_combine_request_t xcb_out;

    xcb_out.operation = operation;
    xcb_out.destination_kind = destination_kind;
    xcb_out.source_kind = source_kind;
    xcb_out.pad0 = 0;
    xcb_out.destination_window = destination_window;
    xcb_out.x_offset = x_offset;
    xcb_out.y_offset = y_offset;
    xcb_out.source_window = source_window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shape_offset_checked (xcb_connection_t *c  /**< */,
                          xcb_shape_kind_t  destination_kind  /**< */,
                          xcb_window_t      destination_window  /**< */,
                          int16_t           x_offset  /**< */,
                          int16_t           y_offset  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_OFFSET,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shape_offset_request_t xcb_out;

    xcb_out.destination_kind = destination_kind;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.destination_window = destination_window;
    xcb_out.x_offset = x_offset;
    xcb_out.y_offset = y_offset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shape_offset (xcb_connection_t *c  /**< */,
                  xcb_shape_kind_t  destination_kind  /**< */,
                  xcb_window_t      destination_window  /**< */,
                  int16_t           x_offset  /**< */,
                  int16_t           y_offset  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_OFFSET,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shape_offset_request_t xcb_out;

    xcb_out.destination_kind = destination_kind;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.destination_window = destination_window;
    xcb_out.x_offset = x_offset;
    xcb_out.y_offset = y_offset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shape_query_extents_cookie_t
xcb_shape_query_extents (xcb_connection_t *c  /**< */,
                         xcb_window_t      destination_window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_QUERY_EXTENTS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_shape_query_extents_cookie_t xcb_ret;
    xcb_shape_query_extents_request_t xcb_out;

    xcb_out.destination_window = destination_window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shape_query_extents_cookie_t
xcb_shape_query_extents_unchecked (xcb_connection_t *c  /**< */,
                                   xcb_window_t      destination_window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_QUERY_EXTENTS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_shape_query_extents_cookie_t xcb_ret;
    xcb_shape_query_extents_request_t xcb_out;

    xcb_out.destination_window = destination_window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shape_query_extents_reply_t *
xcb_shape_query_extents_reply (xcb_connection_t                  *c  /**< */,
                               xcb_shape_query_extents_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_shape_query_extents_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_shape_select_input_checked (xcb_connection_t *c  /**< */,
                                xcb_window_t      destination_window  /**< */,
                                uint8_t           enable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_SELECT_INPUT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shape_select_input_request_t xcb_out;

    xcb_out.destination_window = destination_window;
    xcb_out.enable = enable;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shape_select_input (xcb_connection_t *c  /**< */,
                        xcb_window_t      destination_window  /**< */,
                        uint8_t           enable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_SELECT_INPUT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shape_select_input_request_t xcb_out;

    xcb_out.destination_window = destination_window;
    xcb_out.enable = enable;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shape_input_selected_cookie_t
xcb_shape_input_selected (xcb_connection_t *c  /**< */,
                          xcb_window_t      destination_window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_INPUT_SELECTED,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_shape_input_selected_cookie_t xcb_ret;
    xcb_shape_input_selected_request_t xcb_out;

    xcb_out.destination_window = destination_window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shape_input_selected_cookie_t
xcb_shape_input_selected_unchecked (xcb_connection_t *c  /**< */,
                                    xcb_window_t      destination_window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_INPUT_SELECTED,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_shape_input_selected_cookie_t xcb_ret;
    xcb_shape_input_selected_request_t xcb_out;

    xcb_out.destination_window = destination_window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shape_input_selected_reply_t *
xcb_shape_input_selected_reply (xcb_connection_t                   *c  /**< */,
                                xcb_shape_input_selected_cookie_t   cookie  /**< */,
                                xcb_generic_error_t               **e  /**< */)
{
    return (xcb_shape_input_selected_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_shape_get_rectangles_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_shape_get_rectangles_reply_t *_aux = (xcb_shape_get_rectangles_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_shape_get_rectangles_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* rectangles */
    xcb_block_len += _aux->rectangles_len * sizeof(xcb_rectangle_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_rectangle_t);
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

xcb_shape_get_rectangles_cookie_t
xcb_shape_get_rectangles (xcb_connection_t *c  /**< */,
                          xcb_window_t      window  /**< */,
                          xcb_shape_kind_t  source_kind  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_GET_RECTANGLES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_shape_get_rectangles_cookie_t xcb_ret;
    xcb_shape_get_rectangles_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.source_kind = source_kind;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shape_get_rectangles_cookie_t
xcb_shape_get_rectangles_unchecked (xcb_connection_t *c  /**< */,
                                    xcb_window_t      window  /**< */,
                                    xcb_shape_kind_t  source_kind  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shape_id,
        /* opcode */ XCB_SHAPE_GET_RECTANGLES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_shape_get_rectangles_cookie_t xcb_ret;
    xcb_shape_get_rectangles_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.source_kind = source_kind;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_rectangle_t *
xcb_shape_get_rectangles_rectangles (const xcb_shape_get_rectangles_reply_t *R  /**< */)
{
    return (xcb_rectangle_t *) (R + 1);
}

int
xcb_shape_get_rectangles_rectangles_length (const xcb_shape_get_rectangles_reply_t *R  /**< */)
{
    return R->rectangles_len;
}

xcb_rectangle_iterator_t
xcb_shape_get_rectangles_rectangles_iterator (const xcb_shape_get_rectangles_reply_t *R  /**< */)
{
    xcb_rectangle_iterator_t i;
    i.data = (xcb_rectangle_t *) (R + 1);
    i.rem = R->rectangles_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_shape_get_rectangles_reply_t *
xcb_shape_get_rectangles_reply (xcb_connection_t                   *c  /**< */,
                                xcb_shape_get_rectangles_cookie_t   cookie  /**< */,
                                xcb_generic_error_t               **e  /**< */)
{
    return (xcb_shape_get_rectangles_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

