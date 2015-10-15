/*
 * This file generated automatically from randr.xml by c_client.py.
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
#include "randr.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xproto.h"
#include "render.h"

xcb_extension_t xcb_randr_id = { "RANDR", 0 };

void
xcb_randr_mode_next (xcb_randr_mode_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_randr_mode_t);
}

xcb_generic_iterator_t
xcb_randr_mode_end (xcb_randr_mode_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_randr_crtc_next (xcb_randr_crtc_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_randr_crtc_t);
}

xcb_generic_iterator_t
xcb_randr_crtc_end (xcb_randr_crtc_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_randr_output_next (xcb_randr_output_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_randr_output_t);
}

xcb_generic_iterator_t
xcb_randr_output_end (xcb_randr_output_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_randr_provider_next (xcb_randr_provider_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_randr_provider_t);
}

xcb_generic_iterator_t
xcb_randr_provider_end (xcb_randr_provider_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_randr_screen_size_next (xcb_randr_screen_size_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_randr_screen_size_t);
}

xcb_generic_iterator_t
xcb_randr_screen_size_end (xcb_randr_screen_size_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_randr_refresh_rates_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_refresh_rates_t *_aux = (xcb_randr_refresh_rates_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_refresh_rates_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* rates */
    xcb_block_len += _aux->nRates * sizeof(uint16_t);
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

uint16_t *
xcb_randr_refresh_rates_rates (const xcb_randr_refresh_rates_t *R  /**< */)
{
    return (uint16_t *) (R + 1);
}

int
xcb_randr_refresh_rates_rates_length (const xcb_randr_refresh_rates_t *R  /**< */)
{
    return R->nRates;
}

xcb_generic_iterator_t
xcb_randr_refresh_rates_rates_end (const xcb_randr_refresh_rates_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint16_t *) (R + 1)) + (R->nRates);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_randr_refresh_rates_next (xcb_randr_refresh_rates_iterator_t *i  /**< */)
{
    xcb_randr_refresh_rates_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_randr_refresh_rates_t *)(((char *)R) + xcb_randr_refresh_rates_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_randr_refresh_rates_t *) child.data;
}

xcb_generic_iterator_t
xcb_randr_refresh_rates_end (xcb_randr_refresh_rates_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_randr_refresh_rates_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

xcb_randr_query_version_cookie_t
xcb_randr_query_version (xcb_connection_t *c  /**< */,
                         uint32_t          major_version  /**< */,
                         uint32_t          minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_query_version_cookie_t xcb_ret;
    xcb_randr_query_version_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_query_version_cookie_t
xcb_randr_query_version_unchecked (xcb_connection_t *c  /**< */,
                                   uint32_t          major_version  /**< */,
                                   uint32_t          minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_query_version_cookie_t xcb_ret;
    xcb_randr_query_version_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_query_version_reply_t *
xcb_randr_query_version_reply (xcb_connection_t                  *c  /**< */,
                               xcb_randr_query_version_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_randr_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_randr_set_screen_config_cookie_t
xcb_randr_set_screen_config (xcb_connection_t *c  /**< */,
                             xcb_window_t      window  /**< */,
                             xcb_timestamp_t   timestamp  /**< */,
                             xcb_timestamp_t   config_timestamp  /**< */,
                             uint16_t          sizeID  /**< */,
                             uint16_t          rotation  /**< */,
                             uint16_t          rate  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_SCREEN_CONFIG,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_set_screen_config_cookie_t xcb_ret;
    xcb_randr_set_screen_config_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.timestamp = timestamp;
    xcb_out.config_timestamp = config_timestamp;
    xcb_out.sizeID = sizeID;
    xcb_out.rotation = rotation;
    xcb_out.rate = rate;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_set_screen_config_cookie_t
xcb_randr_set_screen_config_unchecked (xcb_connection_t *c  /**< */,
                                       xcb_window_t      window  /**< */,
                                       xcb_timestamp_t   timestamp  /**< */,
                                       xcb_timestamp_t   config_timestamp  /**< */,
                                       uint16_t          sizeID  /**< */,
                                       uint16_t          rotation  /**< */,
                                       uint16_t          rate  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_SCREEN_CONFIG,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_set_screen_config_cookie_t xcb_ret;
    xcb_randr_set_screen_config_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.timestamp = timestamp;
    xcb_out.config_timestamp = config_timestamp;
    xcb_out.sizeID = sizeID;
    xcb_out.rotation = rotation;
    xcb_out.rate = rate;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_set_screen_config_reply_t *
xcb_randr_set_screen_config_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_randr_set_screen_config_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_randr_set_screen_config_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_randr_select_input_checked (xcb_connection_t *c  /**< */,
                                xcb_window_t      window  /**< */,
                                uint16_t          enable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SELECT_INPUT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_select_input_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.enable = enable;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_select_input (xcb_connection_t *c  /**< */,
                        xcb_window_t      window  /**< */,
                        uint16_t          enable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SELECT_INPUT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_select_input_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.enable = enable;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_randr_get_screen_info_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_get_screen_info_reply_t *_aux = (xcb_randr_get_screen_info_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_randr_get_screen_info_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* sizes */
    xcb_block_len += _aux->nSizes * sizeof(xcb_randr_screen_size_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_screen_size_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* rates */
    for(i=0; i<(_aux->nInfo - _aux->nSizes); i++) {
        xcb_tmp_len = xcb_randr_refresh_rates_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_randr_refresh_rates_t);
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

xcb_randr_get_screen_info_cookie_t
xcb_randr_get_screen_info (xcb_connection_t *c  /**< */,
                           xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_SCREEN_INFO,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_screen_info_cookie_t xcb_ret;
    xcb_randr_get_screen_info_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_screen_info_cookie_t
xcb_randr_get_screen_info_unchecked (xcb_connection_t *c  /**< */,
                                     xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_SCREEN_INFO,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_screen_info_cookie_t xcb_ret;
    xcb_randr_get_screen_info_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_screen_size_t *
xcb_randr_get_screen_info_sizes (const xcb_randr_get_screen_info_reply_t *R  /**< */)
{
    return (xcb_randr_screen_size_t *) (R + 1);
}

int
xcb_randr_get_screen_info_sizes_length (const xcb_randr_get_screen_info_reply_t *R  /**< */)
{
    return R->nSizes;
}

xcb_randr_screen_size_iterator_t
xcb_randr_get_screen_info_sizes_iterator (const xcb_randr_get_screen_info_reply_t *R  /**< */)
{
    xcb_randr_screen_size_iterator_t i;
    i.data = (xcb_randr_screen_size_t *) (R + 1);
    i.rem = R->nSizes;
    i.index = (char *) i.data - (char *) R;
    return i;
}

int
xcb_randr_get_screen_info_rates_length (const xcb_randr_get_screen_info_reply_t *R  /**< */)
{
    return (R->nInfo - R->nSizes);
}

xcb_randr_refresh_rates_iterator_t
xcb_randr_get_screen_info_rates_iterator (const xcb_randr_get_screen_info_reply_t *R  /**< */)
{
    xcb_randr_refresh_rates_iterator_t i;
    xcb_generic_iterator_t prev = xcb_randr_screen_size_end(xcb_randr_get_screen_info_sizes_iterator(R));
    i.data = (xcb_randr_refresh_rates_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_randr_refresh_rates_t, prev.index));
    i.rem = (R->nInfo - R->nSizes);
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_get_screen_info_reply_t *
xcb_randr_get_screen_info_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_randr_get_screen_info_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */)
{
    return (xcb_randr_get_screen_info_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_randr_get_screen_size_range_cookie_t
xcb_randr_get_screen_size_range (xcb_connection_t *c  /**< */,
                                 xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_SCREEN_SIZE_RANGE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_screen_size_range_cookie_t xcb_ret;
    xcb_randr_get_screen_size_range_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_screen_size_range_cookie_t
xcb_randr_get_screen_size_range_unchecked (xcb_connection_t *c  /**< */,
                                           xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_SCREEN_SIZE_RANGE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_screen_size_range_cookie_t xcb_ret;
    xcb_randr_get_screen_size_range_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_screen_size_range_reply_t *
xcb_randr_get_screen_size_range_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_randr_get_screen_size_range_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */)
{
    return (xcb_randr_get_screen_size_range_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_randr_set_screen_size_checked (xcb_connection_t *c  /**< */,
                                   xcb_window_t      window  /**< */,
                                   uint16_t          width  /**< */,
                                   uint16_t          height  /**< */,
                                   uint32_t          mm_width  /**< */,
                                   uint32_t          mm_height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_SCREEN_SIZE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_set_screen_size_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.mm_width = mm_width;
    xcb_out.mm_height = mm_height;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_set_screen_size (xcb_connection_t *c  /**< */,
                           xcb_window_t      window  /**< */,
                           uint16_t          width  /**< */,
                           uint16_t          height  /**< */,
                           uint32_t          mm_width  /**< */,
                           uint32_t          mm_height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_SCREEN_SIZE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_set_screen_size_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.mm_width = mm_width;
    xcb_out.mm_height = mm_height;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

void
xcb_randr_mode_info_next (xcb_randr_mode_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_randr_mode_info_t);
}

xcb_generic_iterator_t
xcb_randr_mode_info_end (xcb_randr_mode_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_randr_get_screen_resources_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_get_screen_resources_reply_t *_aux = (xcb_randr_get_screen_resources_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_get_screen_resources_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* crtcs */
    xcb_block_len += _aux->num_crtcs * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_crtc_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* outputs */
    xcb_block_len += _aux->num_outputs * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_output_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* modes */
    xcb_block_len += _aux->num_modes * sizeof(xcb_randr_mode_info_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_mode_info_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* names */
    xcb_block_len += _aux->names_len * sizeof(uint8_t);
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

xcb_randr_get_screen_resources_cookie_t
xcb_randr_get_screen_resources (xcb_connection_t *c  /**< */,
                                xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_SCREEN_RESOURCES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_screen_resources_cookie_t xcb_ret;
    xcb_randr_get_screen_resources_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_screen_resources_cookie_t
xcb_randr_get_screen_resources_unchecked (xcb_connection_t *c  /**< */,
                                          xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_SCREEN_RESOURCES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_screen_resources_cookie_t xcb_ret;
    xcb_randr_get_screen_resources_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_crtc_t *
xcb_randr_get_screen_resources_crtcs (const xcb_randr_get_screen_resources_reply_t *R  /**< */)
{
    return (xcb_randr_crtc_t *) (R + 1);
}

int
xcb_randr_get_screen_resources_crtcs_length (const xcb_randr_get_screen_resources_reply_t *R  /**< */)
{
    return R->num_crtcs;
}

xcb_generic_iterator_t
xcb_randr_get_screen_resources_crtcs_end (const xcb_randr_get_screen_resources_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_randr_crtc_t *) (R + 1)) + (R->num_crtcs);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_output_t *
xcb_randr_get_screen_resources_outputs (const xcb_randr_get_screen_resources_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_screen_resources_crtcs_end(R);
    return (xcb_randr_output_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_randr_output_t, prev.index) + 0);
}

int
xcb_randr_get_screen_resources_outputs_length (const xcb_randr_get_screen_resources_reply_t *R  /**< */)
{
    return R->num_outputs;
}

xcb_generic_iterator_t
xcb_randr_get_screen_resources_outputs_end (const xcb_randr_get_screen_resources_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_screen_resources_crtcs_end(R);
    i.data = ((xcb_randr_output_t *) child.data) + (R->num_outputs);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_mode_info_t *
xcb_randr_get_screen_resources_modes (const xcb_randr_get_screen_resources_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_screen_resources_outputs_end(R);
    return (xcb_randr_mode_info_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_randr_mode_info_t, prev.index) + 0);
}

int
xcb_randr_get_screen_resources_modes_length (const xcb_randr_get_screen_resources_reply_t *R  /**< */)
{
    return R->num_modes;
}

xcb_randr_mode_info_iterator_t
xcb_randr_get_screen_resources_modes_iterator (const xcb_randr_get_screen_resources_reply_t *R  /**< */)
{
    xcb_randr_mode_info_iterator_t i;
    xcb_generic_iterator_t prev = xcb_randr_get_screen_resources_outputs_end(R);
    i.data = (xcb_randr_mode_info_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_randr_mode_info_t, prev.index));
    i.rem = R->num_modes;
    i.index = (char *) i.data - (char *) R;
    return i;
}

uint8_t *
xcb_randr_get_screen_resources_names (const xcb_randr_get_screen_resources_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_mode_info_end(xcb_randr_get_screen_resources_modes_iterator(R));
    return (uint8_t *) ((char *) prev.data + XCB_TYPE_PAD(uint8_t, prev.index) + 0);
}

int
xcb_randr_get_screen_resources_names_length (const xcb_randr_get_screen_resources_reply_t *R  /**< */)
{
    return R->names_len;
}

xcb_generic_iterator_t
xcb_randr_get_screen_resources_names_end (const xcb_randr_get_screen_resources_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_mode_info_end(xcb_randr_get_screen_resources_modes_iterator(R));
    i.data = ((uint8_t *) child.data) + (R->names_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_get_screen_resources_reply_t *
xcb_randr_get_screen_resources_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_randr_get_screen_resources_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */)
{
    return (xcb_randr_get_screen_resources_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_randr_get_output_info_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_get_output_info_reply_t *_aux = (xcb_randr_get_output_info_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_get_output_info_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* crtcs */
    xcb_block_len += _aux->num_crtcs * sizeof(xcb_randr_output_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_crtc_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* modes */
    xcb_block_len += _aux->num_modes * sizeof(xcb_randr_output_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_mode_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* clones */
    xcb_block_len += _aux->num_clones * sizeof(xcb_randr_output_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_output_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* name */
    xcb_block_len += _aux->name_len * sizeof(uint8_t);
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

xcb_randr_get_output_info_cookie_t
xcb_randr_get_output_info (xcb_connection_t   *c  /**< */,
                           xcb_randr_output_t  output  /**< */,
                           xcb_timestamp_t     config_timestamp  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_OUTPUT_INFO,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_output_info_cookie_t xcb_ret;
    xcb_randr_get_output_info_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.config_timestamp = config_timestamp;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_output_info_cookie_t
xcb_randr_get_output_info_unchecked (xcb_connection_t   *c  /**< */,
                                     xcb_randr_output_t  output  /**< */,
                                     xcb_timestamp_t     config_timestamp  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_OUTPUT_INFO,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_output_info_cookie_t xcb_ret;
    xcb_randr_get_output_info_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.config_timestamp = config_timestamp;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_crtc_t *
xcb_randr_get_output_info_crtcs (const xcb_randr_get_output_info_reply_t *R  /**< */)
{
    return (xcb_randr_crtc_t *) (R + 1);
}

int
xcb_randr_get_output_info_crtcs_length (const xcb_randr_get_output_info_reply_t *R  /**< */)
{
    return R->num_crtcs;
}

xcb_generic_iterator_t
xcb_randr_get_output_info_crtcs_end (const xcb_randr_get_output_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_randr_crtc_t *) (R + 1)) + (R->num_crtcs);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_mode_t *
xcb_randr_get_output_info_modes (const xcb_randr_get_output_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_output_info_crtcs_end(R);
    return (xcb_randr_mode_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_randr_mode_t, prev.index) + 0);
}

int
xcb_randr_get_output_info_modes_length (const xcb_randr_get_output_info_reply_t *R  /**< */)
{
    return R->num_modes;
}

xcb_generic_iterator_t
xcb_randr_get_output_info_modes_end (const xcb_randr_get_output_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_output_info_crtcs_end(R);
    i.data = ((xcb_randr_mode_t *) child.data) + (R->num_modes);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_output_t *
xcb_randr_get_output_info_clones (const xcb_randr_get_output_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_output_info_modes_end(R);
    return (xcb_randr_output_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_randr_output_t, prev.index) + 0);
}

int
xcb_randr_get_output_info_clones_length (const xcb_randr_get_output_info_reply_t *R  /**< */)
{
    return R->num_clones;
}

xcb_generic_iterator_t
xcb_randr_get_output_info_clones_end (const xcb_randr_get_output_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_output_info_modes_end(R);
    i.data = ((xcb_randr_output_t *) child.data) + (R->num_clones);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

uint8_t *
xcb_randr_get_output_info_name (const xcb_randr_get_output_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_output_info_clones_end(R);
    return (uint8_t *) ((char *) prev.data + XCB_TYPE_PAD(uint8_t, prev.index) + 0);
}

int
xcb_randr_get_output_info_name_length (const xcb_randr_get_output_info_reply_t *R  /**< */)
{
    return R->name_len;
}

xcb_generic_iterator_t
xcb_randr_get_output_info_name_end (const xcb_randr_get_output_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_output_info_clones_end(R);
    i.data = ((uint8_t *) child.data) + (R->name_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_get_output_info_reply_t *
xcb_randr_get_output_info_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_randr_get_output_info_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */)
{
    return (xcb_randr_get_output_info_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_randr_list_output_properties_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_list_output_properties_reply_t *_aux = (xcb_randr_list_output_properties_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_list_output_properties_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* atoms */
    xcb_block_len += _aux->num_atoms * sizeof(xcb_atom_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_atom_t);
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

xcb_randr_list_output_properties_cookie_t
xcb_randr_list_output_properties (xcb_connection_t   *c  /**< */,
                                  xcb_randr_output_t  output  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_LIST_OUTPUT_PROPERTIES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_list_output_properties_cookie_t xcb_ret;
    xcb_randr_list_output_properties_request_t xcb_out;

    xcb_out.output = output;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_list_output_properties_cookie_t
xcb_randr_list_output_properties_unchecked (xcb_connection_t   *c  /**< */,
                                            xcb_randr_output_t  output  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_LIST_OUTPUT_PROPERTIES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_list_output_properties_cookie_t xcb_ret;
    xcb_randr_list_output_properties_request_t xcb_out;

    xcb_out.output = output;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_atom_t *
xcb_randr_list_output_properties_atoms (const xcb_randr_list_output_properties_reply_t *R  /**< */)
{
    return (xcb_atom_t *) (R + 1);
}

int
xcb_randr_list_output_properties_atoms_length (const xcb_randr_list_output_properties_reply_t *R  /**< */)
{
    return R->num_atoms;
}

xcb_generic_iterator_t
xcb_randr_list_output_properties_atoms_end (const xcb_randr_list_output_properties_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_atom_t *) (R + 1)) + (R->num_atoms);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_list_output_properties_reply_t *
xcb_randr_list_output_properties_reply (xcb_connection_t                           *c  /**< */,
                                        xcb_randr_list_output_properties_cookie_t   cookie  /**< */,
                                        xcb_generic_error_t                       **e  /**< */)
{
    return (xcb_randr_list_output_properties_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_randr_query_output_property_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_query_output_property_reply_t *_aux = (xcb_randr_query_output_property_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_query_output_property_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* validValues */
    xcb_block_len += _aux->length * sizeof(int32_t);
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

xcb_randr_query_output_property_cookie_t
xcb_randr_query_output_property (xcb_connection_t   *c  /**< */,
                                 xcb_randr_output_t  output  /**< */,
                                 xcb_atom_t          property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_QUERY_OUTPUT_PROPERTY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_query_output_property_cookie_t xcb_ret;
    xcb_randr_query_output_property_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.property = property;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_query_output_property_cookie_t
xcb_randr_query_output_property_unchecked (xcb_connection_t   *c  /**< */,
                                           xcb_randr_output_t  output  /**< */,
                                           xcb_atom_t          property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_QUERY_OUTPUT_PROPERTY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_query_output_property_cookie_t xcb_ret;
    xcb_randr_query_output_property_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.property = property;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_randr_query_output_property_valid_values (const xcb_randr_query_output_property_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_randr_query_output_property_valid_values_length (const xcb_randr_query_output_property_reply_t *R  /**< */)
{
    return R->length;
}

xcb_generic_iterator_t
xcb_randr_query_output_property_valid_values_end (const xcb_randr_query_output_property_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->length);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_query_output_property_reply_t *
xcb_randr_query_output_property_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_randr_query_output_property_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */)
{
    return (xcb_randr_query_output_property_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_randr_configure_output_property_sizeof (const void  *_buffer  /**< */,
                                            uint32_t     values_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_configure_output_property_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* values */
    xcb_block_len += values_len * sizeof(int32_t);
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

xcb_void_cookie_t
xcb_randr_configure_output_property_checked (xcb_connection_t   *c  /**< */,
                                             xcb_randr_output_t  output  /**< */,
                                             xcb_atom_t          property  /**< */,
                                             uint8_t             pending  /**< */,
                                             uint8_t             range  /**< */,
                                             uint32_t            values_len  /**< */,
                                             const int32_t      *values  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_CONFIGURE_OUTPUT_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_configure_output_property_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.property = property;
    xcb_out.pending = pending;
    xcb_out.range = range;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* int32_t values */
    xcb_parts[4].iov_base = (char *) values;
    xcb_parts[4].iov_len = values_len * sizeof(int32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_configure_output_property (xcb_connection_t   *c  /**< */,
                                     xcb_randr_output_t  output  /**< */,
                                     xcb_atom_t          property  /**< */,
                                     uint8_t             pending  /**< */,
                                     uint8_t             range  /**< */,
                                     uint32_t            values_len  /**< */,
                                     const int32_t      *values  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_CONFIGURE_OUTPUT_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_configure_output_property_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.property = property;
    xcb_out.pending = pending;
    xcb_out.range = range;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* int32_t values */
    xcb_parts[4].iov_base = (char *) values;
    xcb_parts[4].iov_len = values_len * sizeof(int32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_randr_change_output_property_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_change_output_property_request_t *_aux = (xcb_randr_change_output_property_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_change_output_property_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += ((_aux->num_units * _aux->format) / 8) * sizeof(char);
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
xcb_randr_change_output_property_checked (xcb_connection_t   *c  /**< */,
                                          xcb_randr_output_t  output  /**< */,
                                          xcb_atom_t          property  /**< */,
                                          xcb_atom_t          type  /**< */,
                                          uint8_t             format  /**< */,
                                          uint8_t             mode  /**< */,
                                          uint32_t            num_units  /**< */,
                                          const void         *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_CHANGE_OUTPUT_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_change_output_property_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.format = format;
    xcb_out.mode = mode;
    memset(xcb_out.pad0, 0, 2);
    xcb_out.num_units = num_units;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* void data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = ((num_units * format) / 8) * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_change_output_property (xcb_connection_t   *c  /**< */,
                                  xcb_randr_output_t  output  /**< */,
                                  xcb_atom_t          property  /**< */,
                                  xcb_atom_t          type  /**< */,
                                  uint8_t             format  /**< */,
                                  uint8_t             mode  /**< */,
                                  uint32_t            num_units  /**< */,
                                  const void         *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_CHANGE_OUTPUT_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_change_output_property_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.format = format;
    xcb_out.mode = mode;
    memset(xcb_out.pad0, 0, 2);
    xcb_out.num_units = num_units;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* void data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = ((num_units * format) / 8) * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_delete_output_property_checked (xcb_connection_t   *c  /**< */,
                                          xcb_randr_output_t  output  /**< */,
                                          xcb_atom_t          property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_DELETE_OUTPUT_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_delete_output_property_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.property = property;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_delete_output_property (xcb_connection_t   *c  /**< */,
                                  xcb_randr_output_t  output  /**< */,
                                  xcb_atom_t          property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_DELETE_OUTPUT_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_delete_output_property_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.property = property;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_randr_get_output_property_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_get_output_property_reply_t *_aux = (xcb_randr_get_output_property_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_get_output_property_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += (_aux->num_items * (_aux->format / 8)) * sizeof(uint8_t);
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

xcb_randr_get_output_property_cookie_t
xcb_randr_get_output_property (xcb_connection_t   *c  /**< */,
                               xcb_randr_output_t  output  /**< */,
                               xcb_atom_t          property  /**< */,
                               xcb_atom_t          type  /**< */,
                               uint32_t            long_offset  /**< */,
                               uint32_t            long_length  /**< */,
                               uint8_t             _delete  /**< */,
                               uint8_t             pending  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_OUTPUT_PROPERTY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_output_property_cookie_t xcb_ret;
    xcb_randr_get_output_property_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.long_offset = long_offset;
    xcb_out.long_length = long_length;
    xcb_out._delete = _delete;
    xcb_out.pending = pending;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_output_property_cookie_t
xcb_randr_get_output_property_unchecked (xcb_connection_t   *c  /**< */,
                                         xcb_randr_output_t  output  /**< */,
                                         xcb_atom_t          property  /**< */,
                                         xcb_atom_t          type  /**< */,
                                         uint32_t            long_offset  /**< */,
                                         uint32_t            long_length  /**< */,
                                         uint8_t             _delete  /**< */,
                                         uint8_t             pending  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_OUTPUT_PROPERTY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_output_property_cookie_t xcb_ret;
    xcb_randr_get_output_property_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.long_offset = long_offset;
    xcb_out.long_length = long_length;
    xcb_out._delete = _delete;
    xcb_out.pending = pending;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_randr_get_output_property_data (const xcb_randr_get_output_property_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_randr_get_output_property_data_length (const xcb_randr_get_output_property_reply_t *R  /**< */)
{
    return (R->num_items * (R->format / 8));
}

xcb_generic_iterator_t
xcb_randr_get_output_property_data_end (const xcb_randr_get_output_property_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->num_items * (R->format / 8)));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_get_output_property_reply_t *
xcb_randr_get_output_property_reply (xcb_connection_t                        *c  /**< */,
                                     xcb_randr_get_output_property_cookie_t   cookie  /**< */,
                                     xcb_generic_error_t                    **e  /**< */)
{
    return (xcb_randr_get_output_property_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_randr_create_mode_sizeof (const void  *_buffer  /**< */,
                              uint32_t     name_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_create_mode_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* name */
    xcb_block_len += name_len * sizeof(char);
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

xcb_randr_create_mode_cookie_t
xcb_randr_create_mode (xcb_connection_t      *c  /**< */,
                       xcb_window_t           window  /**< */,
                       xcb_randr_mode_info_t  mode_info  /**< */,
                       uint32_t               name_len  /**< */,
                       const char            *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_CREATE_MODE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_randr_create_mode_cookie_t xcb_ret;
    xcb_randr_create_mode_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.mode_info = mode_info;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = name_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_create_mode_cookie_t
xcb_randr_create_mode_unchecked (xcb_connection_t      *c  /**< */,
                                 xcb_window_t           window  /**< */,
                                 xcb_randr_mode_info_t  mode_info  /**< */,
                                 uint32_t               name_len  /**< */,
                                 const char            *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_CREATE_MODE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_randr_create_mode_cookie_t xcb_ret;
    xcb_randr_create_mode_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.mode_info = mode_info;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = name_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_create_mode_reply_t *
xcb_randr_create_mode_reply (xcb_connection_t                *c  /**< */,
                             xcb_randr_create_mode_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_randr_create_mode_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_randr_destroy_mode_checked (xcb_connection_t *c  /**< */,
                                xcb_randr_mode_t  mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_DESTROY_MODE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_destroy_mode_request_t xcb_out;

    xcb_out.mode = mode;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_destroy_mode (xcb_connection_t *c  /**< */,
                        xcb_randr_mode_t  mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_DESTROY_MODE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_destroy_mode_request_t xcb_out;

    xcb_out.mode = mode;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_add_output_mode_checked (xcb_connection_t   *c  /**< */,
                                   xcb_randr_output_t  output  /**< */,
                                   xcb_randr_mode_t    mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_ADD_OUTPUT_MODE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_add_output_mode_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.mode = mode;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_add_output_mode (xcb_connection_t   *c  /**< */,
                           xcb_randr_output_t  output  /**< */,
                           xcb_randr_mode_t    mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_ADD_OUTPUT_MODE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_add_output_mode_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.mode = mode;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_delete_output_mode_checked (xcb_connection_t   *c  /**< */,
                                      xcb_randr_output_t  output  /**< */,
                                      xcb_randr_mode_t    mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_DELETE_OUTPUT_MODE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_delete_output_mode_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.mode = mode;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_delete_output_mode (xcb_connection_t   *c  /**< */,
                              xcb_randr_output_t  output  /**< */,
                              xcb_randr_mode_t    mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_DELETE_OUTPUT_MODE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_delete_output_mode_request_t xcb_out;

    xcb_out.output = output;
    xcb_out.mode = mode;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_randr_get_crtc_info_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_get_crtc_info_reply_t *_aux = (xcb_randr_get_crtc_info_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_get_crtc_info_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* outputs */
    xcb_block_len += _aux->num_outputs * sizeof(xcb_randr_output_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_output_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* possible */
    xcb_block_len += _aux->num_possible_outputs * sizeof(xcb_randr_output_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_output_t);
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

xcb_randr_get_crtc_info_cookie_t
xcb_randr_get_crtc_info (xcb_connection_t *c  /**< */,
                         xcb_randr_crtc_t  crtc  /**< */,
                         xcb_timestamp_t   config_timestamp  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_CRTC_INFO,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_crtc_info_cookie_t xcb_ret;
    xcb_randr_get_crtc_info_request_t xcb_out;

    xcb_out.crtc = crtc;
    xcb_out.config_timestamp = config_timestamp;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_crtc_info_cookie_t
xcb_randr_get_crtc_info_unchecked (xcb_connection_t *c  /**< */,
                                   xcb_randr_crtc_t  crtc  /**< */,
                                   xcb_timestamp_t   config_timestamp  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_CRTC_INFO,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_crtc_info_cookie_t xcb_ret;
    xcb_randr_get_crtc_info_request_t xcb_out;

    xcb_out.crtc = crtc;
    xcb_out.config_timestamp = config_timestamp;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_output_t *
xcb_randr_get_crtc_info_outputs (const xcb_randr_get_crtc_info_reply_t *R  /**< */)
{
    return (xcb_randr_output_t *) (R + 1);
}

int
xcb_randr_get_crtc_info_outputs_length (const xcb_randr_get_crtc_info_reply_t *R  /**< */)
{
    return R->num_outputs;
}

xcb_generic_iterator_t
xcb_randr_get_crtc_info_outputs_end (const xcb_randr_get_crtc_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_randr_output_t *) (R + 1)) + (R->num_outputs);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_output_t *
xcb_randr_get_crtc_info_possible (const xcb_randr_get_crtc_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_crtc_info_outputs_end(R);
    return (xcb_randr_output_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_randr_output_t, prev.index) + 0);
}

int
xcb_randr_get_crtc_info_possible_length (const xcb_randr_get_crtc_info_reply_t *R  /**< */)
{
    return R->num_possible_outputs;
}

xcb_generic_iterator_t
xcb_randr_get_crtc_info_possible_end (const xcb_randr_get_crtc_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_crtc_info_outputs_end(R);
    i.data = ((xcb_randr_output_t *) child.data) + (R->num_possible_outputs);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_get_crtc_info_reply_t *
xcb_randr_get_crtc_info_reply (xcb_connection_t                  *c  /**< */,
                               xcb_randr_get_crtc_info_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_randr_get_crtc_info_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_randr_set_crtc_config_sizeof (const void  *_buffer  /**< */,
                                  uint32_t     outputs_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_set_crtc_config_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* outputs */
    xcb_block_len += outputs_len * sizeof(xcb_randr_output_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_output_t);
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

xcb_randr_set_crtc_config_cookie_t
xcb_randr_set_crtc_config (xcb_connection_t         *c  /**< */,
                           xcb_randr_crtc_t          crtc  /**< */,
                           xcb_timestamp_t           timestamp  /**< */,
                           xcb_timestamp_t           config_timestamp  /**< */,
                           int16_t                   x  /**< */,
                           int16_t                   y  /**< */,
                           xcb_randr_mode_t          mode  /**< */,
                           uint16_t                  rotation  /**< */,
                           uint32_t                  outputs_len  /**< */,
                           const xcb_randr_output_t *outputs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_CRTC_CONFIG,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_randr_set_crtc_config_cookie_t xcb_ret;
    xcb_randr_set_crtc_config_request_t xcb_out;

    xcb_out.crtc = crtc;
    xcb_out.timestamp = timestamp;
    xcb_out.config_timestamp = config_timestamp;
    xcb_out.x = x;
    xcb_out.y = y;
    xcb_out.mode = mode;
    xcb_out.rotation = rotation;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_randr_output_t outputs */
    xcb_parts[4].iov_base = (char *) outputs;
    xcb_parts[4].iov_len = outputs_len * sizeof(xcb_timestamp_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_set_crtc_config_cookie_t
xcb_randr_set_crtc_config_unchecked (xcb_connection_t         *c  /**< */,
                                     xcb_randr_crtc_t          crtc  /**< */,
                                     xcb_timestamp_t           timestamp  /**< */,
                                     xcb_timestamp_t           config_timestamp  /**< */,
                                     int16_t                   x  /**< */,
                                     int16_t                   y  /**< */,
                                     xcb_randr_mode_t          mode  /**< */,
                                     uint16_t                  rotation  /**< */,
                                     uint32_t                  outputs_len  /**< */,
                                     const xcb_randr_output_t *outputs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_CRTC_CONFIG,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_randr_set_crtc_config_cookie_t xcb_ret;
    xcb_randr_set_crtc_config_request_t xcb_out;

    xcb_out.crtc = crtc;
    xcb_out.timestamp = timestamp;
    xcb_out.config_timestamp = config_timestamp;
    xcb_out.x = x;
    xcb_out.y = y;
    xcb_out.mode = mode;
    xcb_out.rotation = rotation;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_randr_output_t outputs */
    xcb_parts[4].iov_base = (char *) outputs;
    xcb_parts[4].iov_len = outputs_len * sizeof(xcb_timestamp_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_set_crtc_config_reply_t *
xcb_randr_set_crtc_config_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_randr_set_crtc_config_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */)
{
    return (xcb_randr_set_crtc_config_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_randr_get_crtc_gamma_size_cookie_t
xcb_randr_get_crtc_gamma_size (xcb_connection_t *c  /**< */,
                               xcb_randr_crtc_t  crtc  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_CRTC_GAMMA_SIZE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_crtc_gamma_size_cookie_t xcb_ret;
    xcb_randr_get_crtc_gamma_size_request_t xcb_out;

    xcb_out.crtc = crtc;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_crtc_gamma_size_cookie_t
xcb_randr_get_crtc_gamma_size_unchecked (xcb_connection_t *c  /**< */,
                                         xcb_randr_crtc_t  crtc  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_CRTC_GAMMA_SIZE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_crtc_gamma_size_cookie_t xcb_ret;
    xcb_randr_get_crtc_gamma_size_request_t xcb_out;

    xcb_out.crtc = crtc;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_crtc_gamma_size_reply_t *
xcb_randr_get_crtc_gamma_size_reply (xcb_connection_t                        *c  /**< */,
                                     xcb_randr_get_crtc_gamma_size_cookie_t   cookie  /**< */,
                                     xcb_generic_error_t                    **e  /**< */)
{
    return (xcb_randr_get_crtc_gamma_size_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_randr_get_crtc_gamma_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_get_crtc_gamma_reply_t *_aux = (xcb_randr_get_crtc_gamma_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_get_crtc_gamma_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* red */
    xcb_block_len += _aux->size * sizeof(uint16_t);
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
    /* green */
    xcb_block_len += _aux->size * sizeof(uint16_t);
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
    /* blue */
    xcb_block_len += _aux->size * sizeof(uint16_t);
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

xcb_randr_get_crtc_gamma_cookie_t
xcb_randr_get_crtc_gamma (xcb_connection_t *c  /**< */,
                          xcb_randr_crtc_t  crtc  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_CRTC_GAMMA,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_crtc_gamma_cookie_t xcb_ret;
    xcb_randr_get_crtc_gamma_request_t xcb_out;

    xcb_out.crtc = crtc;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_crtc_gamma_cookie_t
xcb_randr_get_crtc_gamma_unchecked (xcb_connection_t *c  /**< */,
                                    xcb_randr_crtc_t  crtc  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_CRTC_GAMMA,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_crtc_gamma_cookie_t xcb_ret;
    xcb_randr_get_crtc_gamma_request_t xcb_out;

    xcb_out.crtc = crtc;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint16_t *
xcb_randr_get_crtc_gamma_red (const xcb_randr_get_crtc_gamma_reply_t *R  /**< */)
{
    return (uint16_t *) (R + 1);
}

int
xcb_randr_get_crtc_gamma_red_length (const xcb_randr_get_crtc_gamma_reply_t *R  /**< */)
{
    return R->size;
}

xcb_generic_iterator_t
xcb_randr_get_crtc_gamma_red_end (const xcb_randr_get_crtc_gamma_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint16_t *) (R + 1)) + (R->size);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

uint16_t *
xcb_randr_get_crtc_gamma_green (const xcb_randr_get_crtc_gamma_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_crtc_gamma_red_end(R);
    return (uint16_t *) ((char *) prev.data + XCB_TYPE_PAD(uint16_t, prev.index) + 0);
}

int
xcb_randr_get_crtc_gamma_green_length (const xcb_randr_get_crtc_gamma_reply_t *R  /**< */)
{
    return R->size;
}

xcb_generic_iterator_t
xcb_randr_get_crtc_gamma_green_end (const xcb_randr_get_crtc_gamma_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_crtc_gamma_red_end(R);
    i.data = ((uint16_t *) child.data) + (R->size);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

uint16_t *
xcb_randr_get_crtc_gamma_blue (const xcb_randr_get_crtc_gamma_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_crtc_gamma_green_end(R);
    return (uint16_t *) ((char *) prev.data + XCB_TYPE_PAD(uint16_t, prev.index) + 0);
}

int
xcb_randr_get_crtc_gamma_blue_length (const xcb_randr_get_crtc_gamma_reply_t *R  /**< */)
{
    return R->size;
}

xcb_generic_iterator_t
xcb_randr_get_crtc_gamma_blue_end (const xcb_randr_get_crtc_gamma_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_crtc_gamma_green_end(R);
    i.data = ((uint16_t *) child.data) + (R->size);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_get_crtc_gamma_reply_t *
xcb_randr_get_crtc_gamma_reply (xcb_connection_t                   *c  /**< */,
                                xcb_randr_get_crtc_gamma_cookie_t   cookie  /**< */,
                                xcb_generic_error_t               **e  /**< */)
{
    return (xcb_randr_get_crtc_gamma_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_randr_set_crtc_gamma_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_set_crtc_gamma_request_t *_aux = (xcb_randr_set_crtc_gamma_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_set_crtc_gamma_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* red */
    xcb_block_len += _aux->size * sizeof(uint16_t);
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
    /* green */
    xcb_block_len += _aux->size * sizeof(uint16_t);
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
    /* blue */
    xcb_block_len += _aux->size * sizeof(uint16_t);
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

xcb_void_cookie_t
xcb_randr_set_crtc_gamma_checked (xcb_connection_t *c  /**< */,
                                  xcb_randr_crtc_t  crtc  /**< */,
                                  uint16_t          size  /**< */,
                                  const uint16_t   *red  /**< */,
                                  const uint16_t   *green  /**< */,
                                  const uint16_t   *blue  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 8,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_CRTC_GAMMA,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[10];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_set_crtc_gamma_request_t xcb_out;

    xcb_out.crtc = crtc;
    xcb_out.size = size;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint16_t red */
    xcb_parts[4].iov_base = (char *) red;
    xcb_parts[4].iov_len = size * sizeof(uint16_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* uint16_t green */
    xcb_parts[6].iov_base = (char *) green;
    xcb_parts[6].iov_len = size * sizeof(uint16_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;
    /* uint16_t blue */
    xcb_parts[8].iov_base = (char *) blue;
    xcb_parts[8].iov_len = size * sizeof(uint16_t);
    xcb_parts[9].iov_base = 0;
    xcb_parts[9].iov_len = -xcb_parts[8].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_set_crtc_gamma (xcb_connection_t *c  /**< */,
                          xcb_randr_crtc_t  crtc  /**< */,
                          uint16_t          size  /**< */,
                          const uint16_t   *red  /**< */,
                          const uint16_t   *green  /**< */,
                          const uint16_t   *blue  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 8,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_CRTC_GAMMA,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[10];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_set_crtc_gamma_request_t xcb_out;

    xcb_out.crtc = crtc;
    xcb_out.size = size;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint16_t red */
    xcb_parts[4].iov_base = (char *) red;
    xcb_parts[4].iov_len = size * sizeof(uint16_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* uint16_t green */
    xcb_parts[6].iov_base = (char *) green;
    xcb_parts[6].iov_len = size * sizeof(uint16_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;
    /* uint16_t blue */
    xcb_parts[8].iov_base = (char *) blue;
    xcb_parts[8].iov_len = size * sizeof(uint16_t);
    xcb_parts[9].iov_base = 0;
    xcb_parts[9].iov_len = -xcb_parts[8].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_randr_get_screen_resources_current_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_get_screen_resources_current_reply_t *_aux = (xcb_randr_get_screen_resources_current_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_get_screen_resources_current_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* crtcs */
    xcb_block_len += _aux->num_crtcs * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_crtc_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* outputs */
    xcb_block_len += _aux->num_outputs * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_output_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* modes */
    xcb_block_len += _aux->num_modes * sizeof(xcb_randr_mode_info_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_mode_info_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* names */
    xcb_block_len += _aux->names_len * sizeof(uint8_t);
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

xcb_randr_get_screen_resources_current_cookie_t
xcb_randr_get_screen_resources_current (xcb_connection_t *c  /**< */,
                                        xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_SCREEN_RESOURCES_CURRENT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_screen_resources_current_cookie_t xcb_ret;
    xcb_randr_get_screen_resources_current_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_screen_resources_current_cookie_t
xcb_randr_get_screen_resources_current_unchecked (xcb_connection_t *c  /**< */,
                                                  xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_SCREEN_RESOURCES_CURRENT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_screen_resources_current_cookie_t xcb_ret;
    xcb_randr_get_screen_resources_current_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_crtc_t *
xcb_randr_get_screen_resources_current_crtcs (const xcb_randr_get_screen_resources_current_reply_t *R  /**< */)
{
    return (xcb_randr_crtc_t *) (R + 1);
}

int
xcb_randr_get_screen_resources_current_crtcs_length (const xcb_randr_get_screen_resources_current_reply_t *R  /**< */)
{
    return R->num_crtcs;
}

xcb_generic_iterator_t
xcb_randr_get_screen_resources_current_crtcs_end (const xcb_randr_get_screen_resources_current_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_randr_crtc_t *) (R + 1)) + (R->num_crtcs);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_output_t *
xcb_randr_get_screen_resources_current_outputs (const xcb_randr_get_screen_resources_current_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_screen_resources_current_crtcs_end(R);
    return (xcb_randr_output_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_randr_output_t, prev.index) + 0);
}

int
xcb_randr_get_screen_resources_current_outputs_length (const xcb_randr_get_screen_resources_current_reply_t *R  /**< */)
{
    return R->num_outputs;
}

xcb_generic_iterator_t
xcb_randr_get_screen_resources_current_outputs_end (const xcb_randr_get_screen_resources_current_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_screen_resources_current_crtcs_end(R);
    i.data = ((xcb_randr_output_t *) child.data) + (R->num_outputs);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_mode_info_t *
xcb_randr_get_screen_resources_current_modes (const xcb_randr_get_screen_resources_current_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_screen_resources_current_outputs_end(R);
    return (xcb_randr_mode_info_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_randr_mode_info_t, prev.index) + 0);
}

int
xcb_randr_get_screen_resources_current_modes_length (const xcb_randr_get_screen_resources_current_reply_t *R  /**< */)
{
    return R->num_modes;
}

xcb_randr_mode_info_iterator_t
xcb_randr_get_screen_resources_current_modes_iterator (const xcb_randr_get_screen_resources_current_reply_t *R  /**< */)
{
    xcb_randr_mode_info_iterator_t i;
    xcb_generic_iterator_t prev = xcb_randr_get_screen_resources_current_outputs_end(R);
    i.data = (xcb_randr_mode_info_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_randr_mode_info_t, prev.index));
    i.rem = R->num_modes;
    i.index = (char *) i.data - (char *) R;
    return i;
}

uint8_t *
xcb_randr_get_screen_resources_current_names (const xcb_randr_get_screen_resources_current_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_mode_info_end(xcb_randr_get_screen_resources_current_modes_iterator(R));
    return (uint8_t *) ((char *) prev.data + XCB_TYPE_PAD(uint8_t, prev.index) + 0);
}

int
xcb_randr_get_screen_resources_current_names_length (const xcb_randr_get_screen_resources_current_reply_t *R  /**< */)
{
    return R->names_len;
}

xcb_generic_iterator_t
xcb_randr_get_screen_resources_current_names_end (const xcb_randr_get_screen_resources_current_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_mode_info_end(xcb_randr_get_screen_resources_current_modes_iterator(R));
    i.data = ((uint8_t *) child.data) + (R->names_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_get_screen_resources_current_reply_t *
xcb_randr_get_screen_resources_current_reply (xcb_connection_t                                 *c  /**< */,
                                              xcb_randr_get_screen_resources_current_cookie_t   cookie  /**< */,
                                              xcb_generic_error_t                             **e  /**< */)
{
    return (xcb_randr_get_screen_resources_current_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_randr_set_crtc_transform_sizeof (const void  *_buffer  /**< */,
                                     uint32_t     filter_params_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_set_crtc_transform_request_t *_aux = (xcb_randr_set_crtc_transform_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_set_crtc_transform_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* filter_name */
    xcb_block_len += _aux->filter_len * sizeof(char);
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
    /* filter_params */
    xcb_block_len += filter_params_len * sizeof(xcb_render_fixed_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_fixed_t);
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
xcb_randr_set_crtc_transform_checked (xcb_connection_t         *c  /**< */,
                                      xcb_randr_crtc_t          crtc  /**< */,
                                      xcb_render_transform_t    transform  /**< */,
                                      uint16_t                  filter_len  /**< */,
                                      const char               *filter_name  /**< */,
                                      uint32_t                  filter_params_len  /**< */,
                                      const xcb_render_fixed_t *filter_params  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_CRTC_TRANSFORM,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[8];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_set_crtc_transform_request_t xcb_out;

    xcb_out.crtc = crtc;
    xcb_out.transform = transform;
    xcb_out.filter_len = filter_len;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char filter_name */
    xcb_parts[4].iov_base = (char *) filter_name;
    xcb_parts[4].iov_len = filter_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_render_fixed_t filter_params */
    xcb_parts[6].iov_base = (char *) filter_params;
    xcb_parts[6].iov_len = filter_params_len * sizeof(xcb_render_fixed_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_set_crtc_transform (xcb_connection_t         *c  /**< */,
                              xcb_randr_crtc_t          crtc  /**< */,
                              xcb_render_transform_t    transform  /**< */,
                              uint16_t                  filter_len  /**< */,
                              const char               *filter_name  /**< */,
                              uint32_t                  filter_params_len  /**< */,
                              const xcb_render_fixed_t *filter_params  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_CRTC_TRANSFORM,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[8];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_set_crtc_transform_request_t xcb_out;

    xcb_out.crtc = crtc;
    xcb_out.transform = transform;
    xcb_out.filter_len = filter_len;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char filter_name */
    xcb_parts[4].iov_base = (char *) filter_name;
    xcb_parts[4].iov_len = filter_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_render_fixed_t filter_params */
    xcb_parts[6].iov_base = (char *) filter_params;
    xcb_parts[6].iov_len = filter_params_len * sizeof(xcb_render_fixed_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_randr_get_crtc_transform_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_get_crtc_transform_reply_t *_aux = (xcb_randr_get_crtc_transform_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_get_crtc_transform_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* pending_filter_name */
    xcb_block_len += _aux->pending_len * sizeof(char);
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
    /* pending_params */
    xcb_block_len += _aux->pending_nparams * sizeof(xcb_render_fixed_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_fixed_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* current_filter_name */
    xcb_block_len += _aux->current_len * sizeof(char);
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
    /* current_params */
    xcb_block_len += _aux->current_nparams * sizeof(xcb_render_fixed_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_fixed_t);
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

xcb_randr_get_crtc_transform_cookie_t
xcb_randr_get_crtc_transform (xcb_connection_t *c  /**< */,
                              xcb_randr_crtc_t  crtc  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_CRTC_TRANSFORM,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_crtc_transform_cookie_t xcb_ret;
    xcb_randr_get_crtc_transform_request_t xcb_out;

    xcb_out.crtc = crtc;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_crtc_transform_cookie_t
xcb_randr_get_crtc_transform_unchecked (xcb_connection_t *c  /**< */,
                                        xcb_randr_crtc_t  crtc  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_CRTC_TRANSFORM,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_crtc_transform_cookie_t xcb_ret;
    xcb_randr_get_crtc_transform_request_t xcb_out;

    xcb_out.crtc = crtc;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

char *
xcb_randr_get_crtc_transform_pending_filter_name (const xcb_randr_get_crtc_transform_reply_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_randr_get_crtc_transform_pending_filter_name_length (const xcb_randr_get_crtc_transform_reply_t *R  /**< */)
{
    return R->pending_len;
}

xcb_generic_iterator_t
xcb_randr_get_crtc_transform_pending_filter_name_end (const xcb_randr_get_crtc_transform_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->pending_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_render_fixed_t *
xcb_randr_get_crtc_transform_pending_params (const xcb_randr_get_crtc_transform_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_crtc_transform_pending_filter_name_end(R);
    return (xcb_render_fixed_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_render_fixed_t, prev.index) + 0);
}

int
xcb_randr_get_crtc_transform_pending_params_length (const xcb_randr_get_crtc_transform_reply_t *R  /**< */)
{
    return R->pending_nparams;
}

xcb_generic_iterator_t
xcb_randr_get_crtc_transform_pending_params_end (const xcb_randr_get_crtc_transform_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_crtc_transform_pending_filter_name_end(R);
    i.data = ((xcb_render_fixed_t *) child.data) + (R->pending_nparams);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

char *
xcb_randr_get_crtc_transform_current_filter_name (const xcb_randr_get_crtc_transform_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_crtc_transform_pending_params_end(R);
    return (char *) ((char *) prev.data + XCB_TYPE_PAD(char, prev.index) + 0);
}

int
xcb_randr_get_crtc_transform_current_filter_name_length (const xcb_randr_get_crtc_transform_reply_t *R  /**< */)
{
    return R->current_len;
}

xcb_generic_iterator_t
xcb_randr_get_crtc_transform_current_filter_name_end (const xcb_randr_get_crtc_transform_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_crtc_transform_pending_params_end(R);
    i.data = ((char *) child.data) + (R->current_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_render_fixed_t *
xcb_randr_get_crtc_transform_current_params (const xcb_randr_get_crtc_transform_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_crtc_transform_current_filter_name_end(R);
    return (xcb_render_fixed_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_render_fixed_t, prev.index) + 0);
}

int
xcb_randr_get_crtc_transform_current_params_length (const xcb_randr_get_crtc_transform_reply_t *R  /**< */)
{
    return R->current_nparams;
}

xcb_generic_iterator_t
xcb_randr_get_crtc_transform_current_params_end (const xcb_randr_get_crtc_transform_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_crtc_transform_current_filter_name_end(R);
    i.data = ((xcb_render_fixed_t *) child.data) + (R->current_nparams);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_get_crtc_transform_reply_t *
xcb_randr_get_crtc_transform_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_randr_get_crtc_transform_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */)
{
    return (xcb_randr_get_crtc_transform_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_randr_get_panning_cookie_t
xcb_randr_get_panning (xcb_connection_t *c  /**< */,
                       xcb_randr_crtc_t  crtc  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_PANNING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_panning_cookie_t xcb_ret;
    xcb_randr_get_panning_request_t xcb_out;

    xcb_out.crtc = crtc;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_panning_cookie_t
xcb_randr_get_panning_unchecked (xcb_connection_t *c  /**< */,
                                 xcb_randr_crtc_t  crtc  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_PANNING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_panning_cookie_t xcb_ret;
    xcb_randr_get_panning_request_t xcb_out;

    xcb_out.crtc = crtc;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_panning_reply_t *
xcb_randr_get_panning_reply (xcb_connection_t                *c  /**< */,
                             xcb_randr_get_panning_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_randr_get_panning_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_randr_set_panning_cookie_t
xcb_randr_set_panning (xcb_connection_t *c  /**< */,
                       xcb_randr_crtc_t  crtc  /**< */,
                       xcb_timestamp_t   timestamp  /**< */,
                       uint16_t          left  /**< */,
                       uint16_t          top  /**< */,
                       uint16_t          width  /**< */,
                       uint16_t          height  /**< */,
                       uint16_t          track_left  /**< */,
                       uint16_t          track_top  /**< */,
                       uint16_t          track_width  /**< */,
                       uint16_t          track_height  /**< */,
                       int16_t           border_left  /**< */,
                       int16_t           border_top  /**< */,
                       int16_t           border_right  /**< */,
                       int16_t           border_bottom  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_PANNING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_set_panning_cookie_t xcb_ret;
    xcb_randr_set_panning_request_t xcb_out;

    xcb_out.crtc = crtc;
    xcb_out.timestamp = timestamp;
    xcb_out.left = left;
    xcb_out.top = top;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.track_left = track_left;
    xcb_out.track_top = track_top;
    xcb_out.track_width = track_width;
    xcb_out.track_height = track_height;
    xcb_out.border_left = border_left;
    xcb_out.border_top = border_top;
    xcb_out.border_right = border_right;
    xcb_out.border_bottom = border_bottom;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_set_panning_cookie_t
xcb_randr_set_panning_unchecked (xcb_connection_t *c  /**< */,
                                 xcb_randr_crtc_t  crtc  /**< */,
                                 xcb_timestamp_t   timestamp  /**< */,
                                 uint16_t          left  /**< */,
                                 uint16_t          top  /**< */,
                                 uint16_t          width  /**< */,
                                 uint16_t          height  /**< */,
                                 uint16_t          track_left  /**< */,
                                 uint16_t          track_top  /**< */,
                                 uint16_t          track_width  /**< */,
                                 uint16_t          track_height  /**< */,
                                 int16_t           border_left  /**< */,
                                 int16_t           border_top  /**< */,
                                 int16_t           border_right  /**< */,
                                 int16_t           border_bottom  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_PANNING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_set_panning_cookie_t xcb_ret;
    xcb_randr_set_panning_request_t xcb_out;

    xcb_out.crtc = crtc;
    xcb_out.timestamp = timestamp;
    xcb_out.left = left;
    xcb_out.top = top;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.track_left = track_left;
    xcb_out.track_top = track_top;
    xcb_out.track_width = track_width;
    xcb_out.track_height = track_height;
    xcb_out.border_left = border_left;
    xcb_out.border_top = border_top;
    xcb_out.border_right = border_right;
    xcb_out.border_bottom = border_bottom;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_set_panning_reply_t *
xcb_randr_set_panning_reply (xcb_connection_t                *c  /**< */,
                             xcb_randr_set_panning_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_randr_set_panning_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_randr_set_output_primary_checked (xcb_connection_t   *c  /**< */,
                                      xcb_window_t        window  /**< */,
                                      xcb_randr_output_t  output  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_OUTPUT_PRIMARY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_set_output_primary_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.output = output;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_set_output_primary (xcb_connection_t   *c  /**< */,
                              xcb_window_t        window  /**< */,
                              xcb_randr_output_t  output  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_OUTPUT_PRIMARY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_set_output_primary_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.output = output;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_output_primary_cookie_t
xcb_randr_get_output_primary (xcb_connection_t *c  /**< */,
                              xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_OUTPUT_PRIMARY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_output_primary_cookie_t xcb_ret;
    xcb_randr_get_output_primary_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_output_primary_cookie_t
xcb_randr_get_output_primary_unchecked (xcb_connection_t *c  /**< */,
                                        xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_OUTPUT_PRIMARY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_output_primary_cookie_t xcb_ret;
    xcb_randr_get_output_primary_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_output_primary_reply_t *
xcb_randr_get_output_primary_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_randr_get_output_primary_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */)
{
    return (xcb_randr_get_output_primary_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_randr_get_providers_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_get_providers_reply_t *_aux = (xcb_randr_get_providers_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_get_providers_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* providers */
    xcb_block_len += _aux->num_providers * sizeof(xcb_randr_provider_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_provider_t);
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

xcb_randr_get_providers_cookie_t
xcb_randr_get_providers (xcb_connection_t *c  /**< */,
                         xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_PROVIDERS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_providers_cookie_t xcb_ret;
    xcb_randr_get_providers_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_providers_cookie_t
xcb_randr_get_providers_unchecked (xcb_connection_t *c  /**< */,
                                   xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_PROVIDERS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_providers_cookie_t xcb_ret;
    xcb_randr_get_providers_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_provider_t *
xcb_randr_get_providers_providers (const xcb_randr_get_providers_reply_t *R  /**< */)
{
    return (xcb_randr_provider_t *) (R + 1);
}

int
xcb_randr_get_providers_providers_length (const xcb_randr_get_providers_reply_t *R  /**< */)
{
    return R->num_providers;
}

xcb_generic_iterator_t
xcb_randr_get_providers_providers_end (const xcb_randr_get_providers_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_randr_provider_t *) (R + 1)) + (R->num_providers);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_get_providers_reply_t *
xcb_randr_get_providers_reply (xcb_connection_t                  *c  /**< */,
                               xcb_randr_get_providers_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_randr_get_providers_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_randr_get_provider_info_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_get_provider_info_reply_t *_aux = (xcb_randr_get_provider_info_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_get_provider_info_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* crtcs */
    xcb_block_len += _aux->num_crtcs * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_crtc_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* outputs */
    xcb_block_len += _aux->num_outputs * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_output_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* associated_providers */
    xcb_block_len += _aux->num_associated_providers * sizeof(uint32_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_randr_provider_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* associated_capability */
    xcb_block_len += _aux->num_associated_providers * sizeof(uint32_t);
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
    /* name */
    xcb_block_len += _aux->name_len * sizeof(char);
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

xcb_randr_get_provider_info_cookie_t
xcb_randr_get_provider_info (xcb_connection_t     *c  /**< */,
                             xcb_randr_provider_t  provider  /**< */,
                             xcb_timestamp_t       config_timestamp  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_PROVIDER_INFO,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_provider_info_cookie_t xcb_ret;
    xcb_randr_get_provider_info_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.config_timestamp = config_timestamp;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_provider_info_cookie_t
xcb_randr_get_provider_info_unchecked (xcb_connection_t     *c  /**< */,
                                       xcb_randr_provider_t  provider  /**< */,
                                       xcb_timestamp_t       config_timestamp  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_PROVIDER_INFO,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_provider_info_cookie_t xcb_ret;
    xcb_randr_get_provider_info_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.config_timestamp = config_timestamp;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_crtc_t *
xcb_randr_get_provider_info_crtcs (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    return (xcb_randr_crtc_t *) (R + 1);
}

int
xcb_randr_get_provider_info_crtcs_length (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    return R->num_crtcs;
}

xcb_generic_iterator_t
xcb_randr_get_provider_info_crtcs_end (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_randr_crtc_t *) (R + 1)) + (R->num_crtcs);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_output_t *
xcb_randr_get_provider_info_outputs (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_provider_info_crtcs_end(R);
    return (xcb_randr_output_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_randr_output_t, prev.index) + 0);
}

int
xcb_randr_get_provider_info_outputs_length (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    return R->num_outputs;
}

xcb_generic_iterator_t
xcb_randr_get_provider_info_outputs_end (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_provider_info_crtcs_end(R);
    i.data = ((xcb_randr_output_t *) child.data) + (R->num_outputs);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_provider_t *
xcb_randr_get_provider_info_associated_providers (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_provider_info_outputs_end(R);
    return (xcb_randr_provider_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_randr_provider_t, prev.index) + 0);
}

int
xcb_randr_get_provider_info_associated_providers_length (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    return R->num_associated_providers;
}

xcb_generic_iterator_t
xcb_randr_get_provider_info_associated_providers_end (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_provider_info_outputs_end(R);
    i.data = ((xcb_randr_provider_t *) child.data) + (R->num_associated_providers);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

uint32_t *
xcb_randr_get_provider_info_associated_capability (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_provider_info_associated_providers_end(R);
    return (uint32_t *) ((char *) prev.data + XCB_TYPE_PAD(uint32_t, prev.index) + 0);
}

int
xcb_randr_get_provider_info_associated_capability_length (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    return R->num_associated_providers;
}

xcb_generic_iterator_t
xcb_randr_get_provider_info_associated_capability_end (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_provider_info_associated_providers_end(R);
    i.data = ((uint32_t *) child.data) + (R->num_associated_providers);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

char *
xcb_randr_get_provider_info_name (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_randr_get_provider_info_associated_capability_end(R);
    return (char *) ((char *) prev.data + XCB_TYPE_PAD(char, prev.index) + 0);
}

int
xcb_randr_get_provider_info_name_length (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    return R->name_len;
}

xcb_generic_iterator_t
xcb_randr_get_provider_info_name_end (const xcb_randr_get_provider_info_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_randr_get_provider_info_associated_capability_end(R);
    i.data = ((char *) child.data) + (R->name_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_get_provider_info_reply_t *
xcb_randr_get_provider_info_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_randr_get_provider_info_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_randr_get_provider_info_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_randr_set_provider_offload_sink_checked (xcb_connection_t     *c  /**< */,
                                             xcb_randr_provider_t  provider  /**< */,
                                             xcb_randr_provider_t  sink_provider  /**< */,
                                             xcb_timestamp_t       config_timestamp  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_PROVIDER_OFFLOAD_SINK,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_set_provider_offload_sink_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.sink_provider = sink_provider;
    xcb_out.config_timestamp = config_timestamp;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_set_provider_offload_sink (xcb_connection_t     *c  /**< */,
                                     xcb_randr_provider_t  provider  /**< */,
                                     xcb_randr_provider_t  sink_provider  /**< */,
                                     xcb_timestamp_t       config_timestamp  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_PROVIDER_OFFLOAD_SINK,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_set_provider_offload_sink_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.sink_provider = sink_provider;
    xcb_out.config_timestamp = config_timestamp;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_set_provider_output_source_checked (xcb_connection_t     *c  /**< */,
                                              xcb_randr_provider_t  provider  /**< */,
                                              xcb_randr_provider_t  source_provider  /**< */,
                                              xcb_timestamp_t       config_timestamp  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_PROVIDER_OUTPUT_SOURCE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_set_provider_output_source_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.source_provider = source_provider;
    xcb_out.config_timestamp = config_timestamp;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_set_provider_output_source (xcb_connection_t     *c  /**< */,
                                      xcb_randr_provider_t  provider  /**< */,
                                      xcb_randr_provider_t  source_provider  /**< */,
                                      xcb_timestamp_t       config_timestamp  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_PROVIDER_OUTPUT_SOURCE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_set_provider_output_source_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.source_provider = source_provider;
    xcb_out.config_timestamp = config_timestamp;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_randr_list_provider_properties_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_list_provider_properties_reply_t *_aux = (xcb_randr_list_provider_properties_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_list_provider_properties_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* atoms */
    xcb_block_len += _aux->num_atoms * sizeof(xcb_atom_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_atom_t);
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

xcb_randr_list_provider_properties_cookie_t
xcb_randr_list_provider_properties (xcb_connection_t     *c  /**< */,
                                    xcb_randr_provider_t  provider  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_LIST_PROVIDER_PROPERTIES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_list_provider_properties_cookie_t xcb_ret;
    xcb_randr_list_provider_properties_request_t xcb_out;

    xcb_out.provider = provider;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_list_provider_properties_cookie_t
xcb_randr_list_provider_properties_unchecked (xcb_connection_t     *c  /**< */,
                                              xcb_randr_provider_t  provider  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_LIST_PROVIDER_PROPERTIES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_list_provider_properties_cookie_t xcb_ret;
    xcb_randr_list_provider_properties_request_t xcb_out;

    xcb_out.provider = provider;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_atom_t *
xcb_randr_list_provider_properties_atoms (const xcb_randr_list_provider_properties_reply_t *R  /**< */)
{
    return (xcb_atom_t *) (R + 1);
}

int
xcb_randr_list_provider_properties_atoms_length (const xcb_randr_list_provider_properties_reply_t *R  /**< */)
{
    return R->num_atoms;
}

xcb_generic_iterator_t
xcb_randr_list_provider_properties_atoms_end (const xcb_randr_list_provider_properties_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_atom_t *) (R + 1)) + (R->num_atoms);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_list_provider_properties_reply_t *
xcb_randr_list_provider_properties_reply (xcb_connection_t                             *c  /**< */,
                                          xcb_randr_list_provider_properties_cookie_t   cookie  /**< */,
                                          xcb_generic_error_t                         **e  /**< */)
{
    return (xcb_randr_list_provider_properties_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_randr_query_provider_property_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_query_provider_property_reply_t *_aux = (xcb_randr_query_provider_property_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_query_provider_property_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* valid_values */
    xcb_block_len += _aux->length * sizeof(int32_t);
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

xcb_randr_query_provider_property_cookie_t
xcb_randr_query_provider_property (xcb_connection_t     *c  /**< */,
                                   xcb_randr_provider_t  provider  /**< */,
                                   xcb_atom_t            property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_QUERY_PROVIDER_PROPERTY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_query_provider_property_cookie_t xcb_ret;
    xcb_randr_query_provider_property_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.property = property;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_query_provider_property_cookie_t
xcb_randr_query_provider_property_unchecked (xcb_connection_t     *c  /**< */,
                                             xcb_randr_provider_t  provider  /**< */,
                                             xcb_atom_t            property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_QUERY_PROVIDER_PROPERTY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_query_provider_property_cookie_t xcb_ret;
    xcb_randr_query_provider_property_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.property = property;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int32_t *
xcb_randr_query_provider_property_valid_values (const xcb_randr_query_provider_property_reply_t *R  /**< */)
{
    return (int32_t *) (R + 1);
}

int
xcb_randr_query_provider_property_valid_values_length (const xcb_randr_query_provider_property_reply_t *R  /**< */)
{
    return R->length;
}

xcb_generic_iterator_t
xcb_randr_query_provider_property_valid_values_end (const xcb_randr_query_provider_property_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((int32_t *) (R + 1)) + (R->length);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_query_provider_property_reply_t *
xcb_randr_query_provider_property_reply (xcb_connection_t                            *c  /**< */,
                                         xcb_randr_query_provider_property_cookie_t   cookie  /**< */,
                                         xcb_generic_error_t                        **e  /**< */)
{
    return (xcb_randr_query_provider_property_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_randr_configure_provider_property_sizeof (const void  *_buffer  /**< */,
                                              uint32_t     values_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_configure_provider_property_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* values */
    xcb_block_len += values_len * sizeof(int32_t);
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

xcb_void_cookie_t
xcb_randr_configure_provider_property_checked (xcb_connection_t     *c  /**< */,
                                               xcb_randr_provider_t  provider  /**< */,
                                               xcb_atom_t            property  /**< */,
                                               uint8_t               pending  /**< */,
                                               uint8_t               range  /**< */,
                                               uint32_t              values_len  /**< */,
                                               const int32_t        *values  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_CONFIGURE_PROVIDER_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_configure_provider_property_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.property = property;
    xcb_out.pending = pending;
    xcb_out.range = range;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* int32_t values */
    xcb_parts[4].iov_base = (char *) values;
    xcb_parts[4].iov_len = values_len * sizeof(int32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_configure_provider_property (xcb_connection_t     *c  /**< */,
                                       xcb_randr_provider_t  provider  /**< */,
                                       xcb_atom_t            property  /**< */,
                                       uint8_t               pending  /**< */,
                                       uint8_t               range  /**< */,
                                       uint32_t              values_len  /**< */,
                                       const int32_t        *values  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_CONFIGURE_PROVIDER_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_configure_provider_property_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.property = property;
    xcb_out.pending = pending;
    xcb_out.range = range;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* int32_t values */
    xcb_parts[4].iov_base = (char *) values;
    xcb_parts[4].iov_len = values_len * sizeof(int32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_randr_change_provider_property_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_change_provider_property_request_t *_aux = (xcb_randr_change_provider_property_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_change_provider_property_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += (_aux->num_items * (_aux->format / 8)) * sizeof(char);
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
xcb_randr_change_provider_property_checked (xcb_connection_t     *c  /**< */,
                                            xcb_randr_provider_t  provider  /**< */,
                                            xcb_atom_t            property  /**< */,
                                            xcb_atom_t            type  /**< */,
                                            uint8_t               format  /**< */,
                                            uint8_t               mode  /**< */,
                                            uint32_t              num_items  /**< */,
                                            const void           *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_CHANGE_PROVIDER_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_change_provider_property_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.format = format;
    xcb_out.mode = mode;
    memset(xcb_out.pad0, 0, 2);
    xcb_out.num_items = num_items;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* void data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = (num_items * (format / 8)) * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_change_provider_property (xcb_connection_t     *c  /**< */,
                                    xcb_randr_provider_t  provider  /**< */,
                                    xcb_atom_t            property  /**< */,
                                    xcb_atom_t            type  /**< */,
                                    uint8_t               format  /**< */,
                                    uint8_t               mode  /**< */,
                                    uint32_t              num_items  /**< */,
                                    const void           *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_CHANGE_PROVIDER_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_change_provider_property_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.format = format;
    xcb_out.mode = mode;
    memset(xcb_out.pad0, 0, 2);
    xcb_out.num_items = num_items;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* void data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = (num_items * (format / 8)) * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_delete_provider_property_checked (xcb_connection_t     *c  /**< */,
                                            xcb_randr_provider_t  provider  /**< */,
                                            xcb_atom_t            property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_DELETE_PROVIDER_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_delete_provider_property_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.property = property;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_randr_delete_provider_property (xcb_connection_t     *c  /**< */,
                                    xcb_randr_provider_t  provider  /**< */,
                                    xcb_atom_t            property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_DELETE_PROVIDER_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_delete_provider_property_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.property = property;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_randr_get_provider_property_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_randr_get_provider_property_reply_t *_aux = (xcb_randr_get_provider_property_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_randr_get_provider_property_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += (_aux->num_items * (_aux->format / 8)) * sizeof(char);
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

xcb_randr_get_provider_property_cookie_t
xcb_randr_get_provider_property (xcb_connection_t     *c  /**< */,
                                 xcb_randr_provider_t  provider  /**< */,
                                 xcb_atom_t            property  /**< */,
                                 xcb_atom_t            type  /**< */,
                                 uint32_t              long_offset  /**< */,
                                 uint32_t              long_length  /**< */,
                                 uint8_t               _delete  /**< */,
                                 uint8_t               pending  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_PROVIDER_PROPERTY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_provider_property_cookie_t xcb_ret;
    xcb_randr_get_provider_property_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.long_offset = long_offset;
    xcb_out.long_length = long_length;
    xcb_out._delete = _delete;
    xcb_out.pending = pending;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_randr_get_provider_property_cookie_t
xcb_randr_get_provider_property_unchecked (xcb_connection_t     *c  /**< */,
                                           xcb_randr_provider_t  provider  /**< */,
                                           xcb_atom_t            property  /**< */,
                                           xcb_atom_t            type  /**< */,
                                           uint32_t              long_offset  /**< */,
                                           uint32_t              long_length  /**< */,
                                           uint8_t               _delete  /**< */,
                                           uint8_t               pending  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_PROVIDER_PROPERTY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_randr_get_provider_property_cookie_t xcb_ret;
    xcb_randr_get_provider_property_request_t xcb_out;

    xcb_out.provider = provider;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.long_offset = long_offset;
    xcb_out.long_length = long_length;
    xcb_out._delete = _delete;
    xcb_out.pending = pending;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

void *
xcb_randr_get_provider_property_data (const xcb_randr_get_provider_property_reply_t *R  /**< */)
{
    return (void *) (R + 1);
}

int
xcb_randr_get_provider_property_data_length (const xcb_randr_get_provider_property_reply_t *R  /**< */)
{
    return (R->num_items * (R->format / 8));
}

xcb_generic_iterator_t
xcb_randr_get_provider_property_data_end (const xcb_randr_get_provider_property_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + ((R->num_items * (R->format / 8)));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_randr_get_provider_property_reply_t *
xcb_randr_get_provider_property_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_randr_get_provider_property_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */)
{
    return (xcb_randr_get_provider_property_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

void
xcb_randr_crtc_change_next (xcb_randr_crtc_change_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_randr_crtc_change_t);
}

xcb_generic_iterator_t
xcb_randr_crtc_change_end (xcb_randr_crtc_change_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_randr_output_change_next (xcb_randr_output_change_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_randr_output_change_t);
}

xcb_generic_iterator_t
xcb_randr_output_change_end (xcb_randr_output_change_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_randr_output_property_next (xcb_randr_output_property_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_randr_output_property_t);
}

xcb_generic_iterator_t
xcb_randr_output_property_end (xcb_randr_output_property_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_randr_provider_change_next (xcb_randr_provider_change_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_randr_provider_change_t);
}

xcb_generic_iterator_t
xcb_randr_provider_change_end (xcb_randr_provider_change_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_randr_provider_property_next (xcb_randr_provider_property_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_randr_provider_property_t);
}

xcb_generic_iterator_t
xcb_randr_provider_property_end (xcb_randr_provider_property_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_randr_resource_change_next (xcb_randr_resource_change_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_randr_resource_change_t);
}

xcb_generic_iterator_t
xcb_randr_resource_change_end (xcb_randr_resource_change_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_randr_notify_data_next (xcb_randr_notify_data_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_randr_notify_data_t);
}

xcb_generic_iterator_t
xcb_randr_notify_data_end (xcb_randr_notify_data_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

