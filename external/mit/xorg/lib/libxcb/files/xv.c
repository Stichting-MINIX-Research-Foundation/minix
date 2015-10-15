/*
 * This file generated automatically from xv.xml by c_client.py.
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
#include "xv.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xproto.h"
#include "shm.h"

xcb_extension_t xcb_xv_id = { "XVideo", 0 };

void
xcb_xv_port_next (xcb_xv_port_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_xv_port_t);
}

xcb_generic_iterator_t
xcb_xv_port_end (xcb_xv_port_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_xv_encoding_next (xcb_xv_encoding_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_xv_encoding_t);
}

xcb_generic_iterator_t
xcb_xv_encoding_end (xcb_xv_encoding_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_xv_rational_next (xcb_xv_rational_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_xv_rational_t);
}

xcb_generic_iterator_t
xcb_xv_rational_end (xcb_xv_rational_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_xv_format_next (xcb_xv_format_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_xv_format_t);
}

xcb_generic_iterator_t
xcb_xv_format_end (xcb_xv_format_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_xv_adaptor_info_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xv_adaptor_info_t *_aux = (xcb_xv_adaptor_info_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xv_adaptor_info_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* name */
    xcb_block_len += _aux->name_size * sizeof(char);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(char);
    xcb_align_to = 4;
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* formats */
    xcb_block_len += _aux->num_formats * sizeof(xcb_xv_format_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_xv_format_t);
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
xcb_xv_adaptor_info_name (const xcb_xv_adaptor_info_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_xv_adaptor_info_name_length (const xcb_xv_adaptor_info_t *R  /**< */)
{
    return R->name_size;
}

xcb_generic_iterator_t
xcb_xv_adaptor_info_name_end (const xcb_xv_adaptor_info_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->name_size);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xv_format_t *
xcb_xv_adaptor_info_formats (const xcb_xv_adaptor_info_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_xv_adaptor_info_name_end(R);
    return (xcb_xv_format_t *) ((char *) prev.data + ((-prev.index) & (4 - 1)) + 0);
}

int
xcb_xv_adaptor_info_formats_length (const xcb_xv_adaptor_info_t *R  /**< */)
{
    return R->num_formats;
}

xcb_xv_format_iterator_t
xcb_xv_adaptor_info_formats_iterator (const xcb_xv_adaptor_info_t *R  /**< */)
{
    xcb_xv_format_iterator_t i;
    xcb_generic_iterator_t prev = xcb_xv_adaptor_info_name_end(R);
    i.data = (xcb_xv_format_t *) ((char *) prev.data + ((-prev.index) & (4 - 1)));
    i.rem = R->num_formats;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_xv_adaptor_info_next (xcb_xv_adaptor_info_iterator_t *i  /**< */)
{
    xcb_xv_adaptor_info_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_xv_adaptor_info_t *)(((char *)R) + xcb_xv_adaptor_info_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_xv_adaptor_info_t *) child.data;
}

xcb_generic_iterator_t
xcb_xv_adaptor_info_end (xcb_xv_adaptor_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_xv_adaptor_info_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_xv_encoding_info_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xv_encoding_info_t *_aux = (xcb_xv_encoding_info_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xv_encoding_info_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* name */
    xcb_block_len += _aux->name_size * sizeof(char);
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
xcb_xv_encoding_info_name (const xcb_xv_encoding_info_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_xv_encoding_info_name_length (const xcb_xv_encoding_info_t *R  /**< */)
{
    return R->name_size;
}

xcb_generic_iterator_t
xcb_xv_encoding_info_name_end (const xcb_xv_encoding_info_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->name_size);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_xv_encoding_info_next (xcb_xv_encoding_info_iterator_t *i  /**< */)
{
    xcb_xv_encoding_info_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_xv_encoding_info_t *)(((char *)R) + xcb_xv_encoding_info_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_xv_encoding_info_t *) child.data;
}

xcb_generic_iterator_t
xcb_xv_encoding_info_end (xcb_xv_encoding_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_xv_encoding_info_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_xv_image_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xv_image_t *_aux = (xcb_xv_image_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xv_image_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* pitches */
    xcb_block_len += _aux->num_planes * sizeof(uint32_t);
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
    /* offsets */
    xcb_block_len += _aux->num_planes * sizeof(uint32_t);
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
    /* data */
    xcb_block_len += _aux->data_size * sizeof(uint8_t);
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

uint32_t *
xcb_xv_image_pitches (const xcb_xv_image_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_xv_image_pitches_length (const xcb_xv_image_t *R  /**< */)
{
    return R->num_planes;
}

xcb_generic_iterator_t
xcb_xv_image_pitches_end (const xcb_xv_image_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->num_planes);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

uint32_t *
xcb_xv_image_offsets (const xcb_xv_image_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_xv_image_pitches_end(R);
    return (uint32_t *) ((char *) prev.data + XCB_TYPE_PAD(uint32_t, prev.index) + 0);
}

int
xcb_xv_image_offsets_length (const xcb_xv_image_t *R  /**< */)
{
    return R->num_planes;
}

xcb_generic_iterator_t
xcb_xv_image_offsets_end (const xcb_xv_image_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_xv_image_pitches_end(R);
    i.data = ((uint32_t *) child.data) + (R->num_planes);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

uint8_t *
xcb_xv_image_data (const xcb_xv_image_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_xv_image_offsets_end(R);
    return (uint8_t *) ((char *) prev.data + XCB_TYPE_PAD(uint8_t, prev.index) + 0);
}

int
xcb_xv_image_data_length (const xcb_xv_image_t *R  /**< */)
{
    return R->data_size;
}

xcb_generic_iterator_t
xcb_xv_image_data_end (const xcb_xv_image_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_xv_image_offsets_end(R);
    i.data = ((uint8_t *) child.data) + (R->data_size);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_xv_image_next (xcb_xv_image_iterator_t *i  /**< */)
{
    xcb_xv_image_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_xv_image_t *)(((char *)R) + xcb_xv_image_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_xv_image_t *) child.data;
}

xcb_generic_iterator_t
xcb_xv_image_end (xcb_xv_image_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_xv_image_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_xv_attribute_info_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xv_attribute_info_t *_aux = (xcb_xv_attribute_info_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xv_attribute_info_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* name */
    xcb_block_len += _aux->size * sizeof(char);
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
xcb_xv_attribute_info_name (const xcb_xv_attribute_info_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_xv_attribute_info_name_length (const xcb_xv_attribute_info_t *R  /**< */)
{
    return R->size;
}

xcb_generic_iterator_t
xcb_xv_attribute_info_name_end (const xcb_xv_attribute_info_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->size);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_xv_attribute_info_next (xcb_xv_attribute_info_iterator_t *i  /**< */)
{
    xcb_xv_attribute_info_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_xv_attribute_info_t *)(((char *)R) + xcb_xv_attribute_info_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_xv_attribute_info_t *) child.data;
}

xcb_generic_iterator_t
xcb_xv_attribute_info_end (xcb_xv_attribute_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_xv_attribute_info_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

void
xcb_xv_image_format_info_next (xcb_xv_image_format_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_xv_image_format_info_t);
}

xcb_generic_iterator_t
xcb_xv_image_format_info_end (xcb_xv_image_format_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

xcb_xv_query_extension_cookie_t
xcb_xv_query_extension (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_QUERY_EXTENSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_query_extension_cookie_t xcb_ret;
    xcb_xv_query_extension_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_query_extension_cookie_t
xcb_xv_query_extension_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_QUERY_EXTENSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_query_extension_cookie_t xcb_ret;
    xcb_xv_query_extension_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_query_extension_reply_t *
xcb_xv_query_extension_reply (xcb_connection_t                 *c  /**< */,
                              xcb_xv_query_extension_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_xv_query_extension_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_xv_query_adaptors_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xv_query_adaptors_reply_t *_aux = (xcb_xv_query_adaptors_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_xv_query_adaptors_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* info */
    for(i=0; i<_aux->num_adaptors; i++) {
        xcb_tmp_len = xcb_xv_adaptor_info_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_xv_adaptor_info_t);
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

xcb_xv_query_adaptors_cookie_t
xcb_xv_query_adaptors (xcb_connection_t *c  /**< */,
                       xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_QUERY_ADAPTORS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_query_adaptors_cookie_t xcb_ret;
    xcb_xv_query_adaptors_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_query_adaptors_cookie_t
xcb_xv_query_adaptors_unchecked (xcb_connection_t *c  /**< */,
                                 xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_QUERY_ADAPTORS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_query_adaptors_cookie_t xcb_ret;
    xcb_xv_query_adaptors_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_xv_query_adaptors_info_length (const xcb_xv_query_adaptors_reply_t *R  /**< */)
{
    return R->num_adaptors;
}

xcb_xv_adaptor_info_iterator_t
xcb_xv_query_adaptors_info_iterator (const xcb_xv_query_adaptors_reply_t *R  /**< */)
{
    xcb_xv_adaptor_info_iterator_t i;
    i.data = (xcb_xv_adaptor_info_t *) (R + 1);
    i.rem = R->num_adaptors;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xv_query_adaptors_reply_t *
xcb_xv_query_adaptors_reply (xcb_connection_t                *c  /**< */,
                             xcb_xv_query_adaptors_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_xv_query_adaptors_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_xv_query_encodings_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xv_query_encodings_reply_t *_aux = (xcb_xv_query_encodings_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_xv_query_encodings_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* info */
    for(i=0; i<_aux->num_encodings; i++) {
        xcb_tmp_len = xcb_xv_encoding_info_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_xv_encoding_info_t);
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

xcb_xv_query_encodings_cookie_t
xcb_xv_query_encodings (xcb_connection_t *c  /**< */,
                        xcb_xv_port_t     port  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_QUERY_ENCODINGS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_query_encodings_cookie_t xcb_ret;
    xcb_xv_query_encodings_request_t xcb_out;

    xcb_out.port = port;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_query_encodings_cookie_t
xcb_xv_query_encodings_unchecked (xcb_connection_t *c  /**< */,
                                  xcb_xv_port_t     port  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_QUERY_ENCODINGS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_query_encodings_cookie_t xcb_ret;
    xcb_xv_query_encodings_request_t xcb_out;

    xcb_out.port = port;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_xv_query_encodings_info_length (const xcb_xv_query_encodings_reply_t *R  /**< */)
{
    return R->num_encodings;
}

xcb_xv_encoding_info_iterator_t
xcb_xv_query_encodings_info_iterator (const xcb_xv_query_encodings_reply_t *R  /**< */)
{
    xcb_xv_encoding_info_iterator_t i;
    i.data = (xcb_xv_encoding_info_t *) (R + 1);
    i.rem = R->num_encodings;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xv_query_encodings_reply_t *
xcb_xv_query_encodings_reply (xcb_connection_t                 *c  /**< */,
                              xcb_xv_query_encodings_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_xv_query_encodings_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_xv_grab_port_cookie_t
xcb_xv_grab_port (xcb_connection_t *c  /**< */,
                  xcb_xv_port_t     port  /**< */,
                  xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_GRAB_PORT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_grab_port_cookie_t xcb_ret;
    xcb_xv_grab_port_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.time = time;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_grab_port_cookie_t
xcb_xv_grab_port_unchecked (xcb_connection_t *c  /**< */,
                            xcb_xv_port_t     port  /**< */,
                            xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_GRAB_PORT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_grab_port_cookie_t xcb_ret;
    xcb_xv_grab_port_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.time = time;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_grab_port_reply_t *
xcb_xv_grab_port_reply (xcb_connection_t           *c  /**< */,
                        xcb_xv_grab_port_cookie_t   cookie  /**< */,
                        xcb_generic_error_t       **e  /**< */)
{
    return (xcb_xv_grab_port_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_xv_ungrab_port_checked (xcb_connection_t *c  /**< */,
                            xcb_xv_port_t     port  /**< */,
                            xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_UNGRAB_PORT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_ungrab_port_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.time = time;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_ungrab_port (xcb_connection_t *c  /**< */,
                    xcb_xv_port_t     port  /**< */,
                    xcb_timestamp_t   time  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_UNGRAB_PORT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_ungrab_port_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.time = time;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_put_video_checked (xcb_connection_t *c  /**< */,
                          xcb_xv_port_t     port  /**< */,
                          xcb_drawable_t    drawable  /**< */,
                          xcb_gcontext_t    gc  /**< */,
                          int16_t           vid_x  /**< */,
                          int16_t           vid_y  /**< */,
                          uint16_t          vid_w  /**< */,
                          uint16_t          vid_h  /**< */,
                          int16_t           drw_x  /**< */,
                          int16_t           drw_y  /**< */,
                          uint16_t          drw_w  /**< */,
                          uint16_t          drw_h  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_PUT_VIDEO,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_put_video_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.vid_x = vid_x;
    xcb_out.vid_y = vid_y;
    xcb_out.vid_w = vid_w;
    xcb_out.vid_h = vid_h;
    xcb_out.drw_x = drw_x;
    xcb_out.drw_y = drw_y;
    xcb_out.drw_w = drw_w;
    xcb_out.drw_h = drw_h;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_put_video (xcb_connection_t *c  /**< */,
                  xcb_xv_port_t     port  /**< */,
                  xcb_drawable_t    drawable  /**< */,
                  xcb_gcontext_t    gc  /**< */,
                  int16_t           vid_x  /**< */,
                  int16_t           vid_y  /**< */,
                  uint16_t          vid_w  /**< */,
                  uint16_t          vid_h  /**< */,
                  int16_t           drw_x  /**< */,
                  int16_t           drw_y  /**< */,
                  uint16_t          drw_w  /**< */,
                  uint16_t          drw_h  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_PUT_VIDEO,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_put_video_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.vid_x = vid_x;
    xcb_out.vid_y = vid_y;
    xcb_out.vid_w = vid_w;
    xcb_out.vid_h = vid_h;
    xcb_out.drw_x = drw_x;
    xcb_out.drw_y = drw_y;
    xcb_out.drw_w = drw_w;
    xcb_out.drw_h = drw_h;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_put_still_checked (xcb_connection_t *c  /**< */,
                          xcb_xv_port_t     port  /**< */,
                          xcb_drawable_t    drawable  /**< */,
                          xcb_gcontext_t    gc  /**< */,
                          int16_t           vid_x  /**< */,
                          int16_t           vid_y  /**< */,
                          uint16_t          vid_w  /**< */,
                          uint16_t          vid_h  /**< */,
                          int16_t           drw_x  /**< */,
                          int16_t           drw_y  /**< */,
                          uint16_t          drw_w  /**< */,
                          uint16_t          drw_h  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_PUT_STILL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_put_still_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.vid_x = vid_x;
    xcb_out.vid_y = vid_y;
    xcb_out.vid_w = vid_w;
    xcb_out.vid_h = vid_h;
    xcb_out.drw_x = drw_x;
    xcb_out.drw_y = drw_y;
    xcb_out.drw_w = drw_w;
    xcb_out.drw_h = drw_h;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_put_still (xcb_connection_t *c  /**< */,
                  xcb_xv_port_t     port  /**< */,
                  xcb_drawable_t    drawable  /**< */,
                  xcb_gcontext_t    gc  /**< */,
                  int16_t           vid_x  /**< */,
                  int16_t           vid_y  /**< */,
                  uint16_t          vid_w  /**< */,
                  uint16_t          vid_h  /**< */,
                  int16_t           drw_x  /**< */,
                  int16_t           drw_y  /**< */,
                  uint16_t          drw_w  /**< */,
                  uint16_t          drw_h  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_PUT_STILL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_put_still_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.vid_x = vid_x;
    xcb_out.vid_y = vid_y;
    xcb_out.vid_w = vid_w;
    xcb_out.vid_h = vid_h;
    xcb_out.drw_x = drw_x;
    xcb_out.drw_y = drw_y;
    xcb_out.drw_w = drw_w;
    xcb_out.drw_h = drw_h;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_get_video_checked (xcb_connection_t *c  /**< */,
                          xcb_xv_port_t     port  /**< */,
                          xcb_drawable_t    drawable  /**< */,
                          xcb_gcontext_t    gc  /**< */,
                          int16_t           vid_x  /**< */,
                          int16_t           vid_y  /**< */,
                          uint16_t          vid_w  /**< */,
                          uint16_t          vid_h  /**< */,
                          int16_t           drw_x  /**< */,
                          int16_t           drw_y  /**< */,
                          uint16_t          drw_w  /**< */,
                          uint16_t          drw_h  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_GET_VIDEO,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_get_video_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.vid_x = vid_x;
    xcb_out.vid_y = vid_y;
    xcb_out.vid_w = vid_w;
    xcb_out.vid_h = vid_h;
    xcb_out.drw_x = drw_x;
    xcb_out.drw_y = drw_y;
    xcb_out.drw_w = drw_w;
    xcb_out.drw_h = drw_h;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_get_video (xcb_connection_t *c  /**< */,
                  xcb_xv_port_t     port  /**< */,
                  xcb_drawable_t    drawable  /**< */,
                  xcb_gcontext_t    gc  /**< */,
                  int16_t           vid_x  /**< */,
                  int16_t           vid_y  /**< */,
                  uint16_t          vid_w  /**< */,
                  uint16_t          vid_h  /**< */,
                  int16_t           drw_x  /**< */,
                  int16_t           drw_y  /**< */,
                  uint16_t          drw_w  /**< */,
                  uint16_t          drw_h  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_GET_VIDEO,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_get_video_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.vid_x = vid_x;
    xcb_out.vid_y = vid_y;
    xcb_out.vid_w = vid_w;
    xcb_out.vid_h = vid_h;
    xcb_out.drw_x = drw_x;
    xcb_out.drw_y = drw_y;
    xcb_out.drw_w = drw_w;
    xcb_out.drw_h = drw_h;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_get_still_checked (xcb_connection_t *c  /**< */,
                          xcb_xv_port_t     port  /**< */,
                          xcb_drawable_t    drawable  /**< */,
                          xcb_gcontext_t    gc  /**< */,
                          int16_t           vid_x  /**< */,
                          int16_t           vid_y  /**< */,
                          uint16_t          vid_w  /**< */,
                          uint16_t          vid_h  /**< */,
                          int16_t           drw_x  /**< */,
                          int16_t           drw_y  /**< */,
                          uint16_t          drw_w  /**< */,
                          uint16_t          drw_h  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_GET_STILL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_get_still_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.vid_x = vid_x;
    xcb_out.vid_y = vid_y;
    xcb_out.vid_w = vid_w;
    xcb_out.vid_h = vid_h;
    xcb_out.drw_x = drw_x;
    xcb_out.drw_y = drw_y;
    xcb_out.drw_w = drw_w;
    xcb_out.drw_h = drw_h;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_get_still (xcb_connection_t *c  /**< */,
                  xcb_xv_port_t     port  /**< */,
                  xcb_drawable_t    drawable  /**< */,
                  xcb_gcontext_t    gc  /**< */,
                  int16_t           vid_x  /**< */,
                  int16_t           vid_y  /**< */,
                  uint16_t          vid_w  /**< */,
                  uint16_t          vid_h  /**< */,
                  int16_t           drw_x  /**< */,
                  int16_t           drw_y  /**< */,
                  uint16_t          drw_w  /**< */,
                  uint16_t          drw_h  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_GET_STILL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_get_still_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.vid_x = vid_x;
    xcb_out.vid_y = vid_y;
    xcb_out.vid_w = vid_w;
    xcb_out.vid_h = vid_h;
    xcb_out.drw_x = drw_x;
    xcb_out.drw_y = drw_y;
    xcb_out.drw_w = drw_w;
    xcb_out.drw_h = drw_h;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_stop_video_checked (xcb_connection_t *c  /**< */,
                           xcb_xv_port_t     port  /**< */,
                           xcb_drawable_t    drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_STOP_VIDEO,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_stop_video_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.drawable = drawable;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_stop_video (xcb_connection_t *c  /**< */,
                   xcb_xv_port_t     port  /**< */,
                   xcb_drawable_t    drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_STOP_VIDEO,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_stop_video_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.drawable = drawable;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_select_video_notify_checked (xcb_connection_t *c  /**< */,
                                    xcb_drawable_t    drawable  /**< */,
                                    uint8_t           onoff  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_SELECT_VIDEO_NOTIFY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_select_video_notify_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.onoff = onoff;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_select_video_notify (xcb_connection_t *c  /**< */,
                            xcb_drawable_t    drawable  /**< */,
                            uint8_t           onoff  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_SELECT_VIDEO_NOTIFY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_select_video_notify_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.onoff = onoff;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_select_port_notify_checked (xcb_connection_t *c  /**< */,
                                   xcb_xv_port_t     port  /**< */,
                                   uint8_t           onoff  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_SELECT_PORT_NOTIFY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_select_port_notify_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.onoff = onoff;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_select_port_notify (xcb_connection_t *c  /**< */,
                           xcb_xv_port_t     port  /**< */,
                           uint8_t           onoff  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_SELECT_PORT_NOTIFY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_select_port_notify_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.onoff = onoff;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_query_best_size_cookie_t
xcb_xv_query_best_size (xcb_connection_t *c  /**< */,
                        xcb_xv_port_t     port  /**< */,
                        uint16_t          vid_w  /**< */,
                        uint16_t          vid_h  /**< */,
                        uint16_t          drw_w  /**< */,
                        uint16_t          drw_h  /**< */,
                        uint8_t           motion  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_QUERY_BEST_SIZE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_query_best_size_cookie_t xcb_ret;
    xcb_xv_query_best_size_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.vid_w = vid_w;
    xcb_out.vid_h = vid_h;
    xcb_out.drw_w = drw_w;
    xcb_out.drw_h = drw_h;
    xcb_out.motion = motion;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_query_best_size_cookie_t
xcb_xv_query_best_size_unchecked (xcb_connection_t *c  /**< */,
                                  xcb_xv_port_t     port  /**< */,
                                  uint16_t          vid_w  /**< */,
                                  uint16_t          vid_h  /**< */,
                                  uint16_t          drw_w  /**< */,
                                  uint16_t          drw_h  /**< */,
                                  uint8_t           motion  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_QUERY_BEST_SIZE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_query_best_size_cookie_t xcb_ret;
    xcb_xv_query_best_size_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.vid_w = vid_w;
    xcb_out.vid_h = vid_h;
    xcb_out.drw_w = drw_w;
    xcb_out.drw_h = drw_h;
    xcb_out.motion = motion;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_query_best_size_reply_t *
xcb_xv_query_best_size_reply (xcb_connection_t                 *c  /**< */,
                              xcb_xv_query_best_size_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_xv_query_best_size_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_xv_set_port_attribute_checked (xcb_connection_t *c  /**< */,
                                   xcb_xv_port_t     port  /**< */,
                                   xcb_atom_t        attribute  /**< */,
                                   int32_t           value  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_SET_PORT_ATTRIBUTE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_set_port_attribute_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.attribute = attribute;
    xcb_out.value = value;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_set_port_attribute (xcb_connection_t *c  /**< */,
                           xcb_xv_port_t     port  /**< */,
                           xcb_atom_t        attribute  /**< */,
                           int32_t           value  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_SET_PORT_ATTRIBUTE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_set_port_attribute_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.attribute = attribute;
    xcb_out.value = value;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_get_port_attribute_cookie_t
xcb_xv_get_port_attribute (xcb_connection_t *c  /**< */,
                           xcb_xv_port_t     port  /**< */,
                           xcb_atom_t        attribute  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_GET_PORT_ATTRIBUTE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_get_port_attribute_cookie_t xcb_ret;
    xcb_xv_get_port_attribute_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.attribute = attribute;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_get_port_attribute_cookie_t
xcb_xv_get_port_attribute_unchecked (xcb_connection_t *c  /**< */,
                                     xcb_xv_port_t     port  /**< */,
                                     xcb_atom_t        attribute  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_GET_PORT_ATTRIBUTE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_get_port_attribute_cookie_t xcb_ret;
    xcb_xv_get_port_attribute_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.attribute = attribute;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_get_port_attribute_reply_t *
xcb_xv_get_port_attribute_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_xv_get_port_attribute_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */)
{
    return (xcb_xv_get_port_attribute_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_xv_query_port_attributes_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xv_query_port_attributes_reply_t *_aux = (xcb_xv_query_port_attributes_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_xv_query_port_attributes_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* attributes */
    for(i=0; i<_aux->num_attributes; i++) {
        xcb_tmp_len = xcb_xv_attribute_info_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_xv_attribute_info_t);
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

xcb_xv_query_port_attributes_cookie_t
xcb_xv_query_port_attributes (xcb_connection_t *c  /**< */,
                              xcb_xv_port_t     port  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_QUERY_PORT_ATTRIBUTES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_query_port_attributes_cookie_t xcb_ret;
    xcb_xv_query_port_attributes_request_t xcb_out;

    xcb_out.port = port;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_query_port_attributes_cookie_t
xcb_xv_query_port_attributes_unchecked (xcb_connection_t *c  /**< */,
                                        xcb_xv_port_t     port  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_QUERY_PORT_ATTRIBUTES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_query_port_attributes_cookie_t xcb_ret;
    xcb_xv_query_port_attributes_request_t xcb_out;

    xcb_out.port = port;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_xv_query_port_attributes_attributes_length (const xcb_xv_query_port_attributes_reply_t *R  /**< */)
{
    return R->num_attributes;
}

xcb_xv_attribute_info_iterator_t
xcb_xv_query_port_attributes_attributes_iterator (const xcb_xv_query_port_attributes_reply_t *R  /**< */)
{
    xcb_xv_attribute_info_iterator_t i;
    i.data = (xcb_xv_attribute_info_t *) (R + 1);
    i.rem = R->num_attributes;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xv_query_port_attributes_reply_t *
xcb_xv_query_port_attributes_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_xv_query_port_attributes_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */)
{
    return (xcb_xv_query_port_attributes_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_xv_list_image_formats_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xv_list_image_formats_reply_t *_aux = (xcb_xv_list_image_formats_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xv_list_image_formats_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* format */
    xcb_block_len += _aux->num_formats * sizeof(xcb_xv_image_format_info_t);
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

xcb_xv_list_image_formats_cookie_t
xcb_xv_list_image_formats (xcb_connection_t *c  /**< */,
                           xcb_xv_port_t     port  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_LIST_IMAGE_FORMATS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_list_image_formats_cookie_t xcb_ret;
    xcb_xv_list_image_formats_request_t xcb_out;

    xcb_out.port = port;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_list_image_formats_cookie_t
xcb_xv_list_image_formats_unchecked (xcb_connection_t *c  /**< */,
                                     xcb_xv_port_t     port  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_LIST_IMAGE_FORMATS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_list_image_formats_cookie_t xcb_ret;
    xcb_xv_list_image_formats_request_t xcb_out;

    xcb_out.port = port;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_image_format_info_t *
xcb_xv_list_image_formats_format (const xcb_xv_list_image_formats_reply_t *R  /**< */)
{
    return (xcb_xv_image_format_info_t *) (R + 1);
}

int
xcb_xv_list_image_formats_format_length (const xcb_xv_list_image_formats_reply_t *R  /**< */)
{
    return R->num_formats;
}

xcb_xv_image_format_info_iterator_t
xcb_xv_list_image_formats_format_iterator (const xcb_xv_list_image_formats_reply_t *R  /**< */)
{
    xcb_xv_image_format_info_iterator_t i;
    i.data = (xcb_xv_image_format_info_t *) (R + 1);
    i.rem = R->num_formats;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xv_list_image_formats_reply_t *
xcb_xv_list_image_formats_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_xv_list_image_formats_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */)
{
    return (xcb_xv_list_image_formats_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_xv_query_image_attributes_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_xv_query_image_attributes_reply_t *_aux = (xcb_xv_query_image_attributes_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xv_query_image_attributes_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* pitches */
    xcb_block_len += _aux->num_planes * sizeof(uint32_t);
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
    /* offsets */
    xcb_block_len += _aux->num_planes * sizeof(uint32_t);
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

xcb_xv_query_image_attributes_cookie_t
xcb_xv_query_image_attributes (xcb_connection_t *c  /**< */,
                               xcb_xv_port_t     port  /**< */,
                               uint32_t          id  /**< */,
                               uint16_t          width  /**< */,
                               uint16_t          height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_QUERY_IMAGE_ATTRIBUTES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_query_image_attributes_cookie_t xcb_ret;
    xcb_xv_query_image_attributes_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.id = id;
    xcb_out.width = width;
    xcb_out.height = height;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xv_query_image_attributes_cookie_t
xcb_xv_query_image_attributes_unchecked (xcb_connection_t *c  /**< */,
                                         xcb_xv_port_t     port  /**< */,
                                         uint32_t          id  /**< */,
                                         uint16_t          width  /**< */,
                                         uint16_t          height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_QUERY_IMAGE_ATTRIBUTES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xv_query_image_attributes_cookie_t xcb_ret;
    xcb_xv_query_image_attributes_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.id = id;
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
xcb_xv_query_image_attributes_pitches (const xcb_xv_query_image_attributes_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_xv_query_image_attributes_pitches_length (const xcb_xv_query_image_attributes_reply_t *R  /**< */)
{
    return R->num_planes;
}

xcb_generic_iterator_t
xcb_xv_query_image_attributes_pitches_end (const xcb_xv_query_image_attributes_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->num_planes);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

uint32_t *
xcb_xv_query_image_attributes_offsets (const xcb_xv_query_image_attributes_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_xv_query_image_attributes_pitches_end(R);
    return (uint32_t *) ((char *) prev.data + XCB_TYPE_PAD(uint32_t, prev.index) + 0);
}

int
xcb_xv_query_image_attributes_offsets_length (const xcb_xv_query_image_attributes_reply_t *R  /**< */)
{
    return R->num_planes;
}

xcb_generic_iterator_t
xcb_xv_query_image_attributes_offsets_end (const xcb_xv_query_image_attributes_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_xv_query_image_attributes_pitches_end(R);
    i.data = ((uint32_t *) child.data) + (R->num_planes);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_xv_query_image_attributes_reply_t *
xcb_xv_query_image_attributes_reply (xcb_connection_t                        *c  /**< */,
                                     xcb_xv_query_image_attributes_cookie_t   cookie  /**< */,
                                     xcb_generic_error_t                    **e  /**< */)
{
    return (xcb_xv_query_image_attributes_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_xv_put_image_sizeof (const void  *_buffer  /**< */,
                         uint32_t     data_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_xv_put_image_request_t);
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
xcb_xv_put_image_checked (xcb_connection_t *c  /**< */,
                          xcb_xv_port_t     port  /**< */,
                          xcb_drawable_t    drawable  /**< */,
                          xcb_gcontext_t    gc  /**< */,
                          uint32_t          id  /**< */,
                          int16_t           src_x  /**< */,
                          int16_t           src_y  /**< */,
                          uint16_t          src_w  /**< */,
                          uint16_t          src_h  /**< */,
                          int16_t           drw_x  /**< */,
                          int16_t           drw_y  /**< */,
                          uint16_t          drw_w  /**< */,
                          uint16_t          drw_h  /**< */,
                          uint16_t          width  /**< */,
                          uint16_t          height  /**< */,
                          uint32_t          data_len  /**< */,
                          const uint8_t    *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_PUT_IMAGE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_put_image_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.id = id;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.src_w = src_w;
    xcb_out.src_h = src_h;
    xcb_out.drw_x = drw_x;
    xcb_out.drw_y = drw_y;
    xcb_out.drw_w = drw_w;
    xcb_out.drw_h = drw_h;
    xcb_out.width = width;
    xcb_out.height = height;

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
xcb_xv_put_image (xcb_connection_t *c  /**< */,
                  xcb_xv_port_t     port  /**< */,
                  xcb_drawable_t    drawable  /**< */,
                  xcb_gcontext_t    gc  /**< */,
                  uint32_t          id  /**< */,
                  int16_t           src_x  /**< */,
                  int16_t           src_y  /**< */,
                  uint16_t          src_w  /**< */,
                  uint16_t          src_h  /**< */,
                  int16_t           drw_x  /**< */,
                  int16_t           drw_y  /**< */,
                  uint16_t          drw_w  /**< */,
                  uint16_t          drw_h  /**< */,
                  uint16_t          width  /**< */,
                  uint16_t          height  /**< */,
                  uint32_t          data_len  /**< */,
                  const uint8_t    *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_PUT_IMAGE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_put_image_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.id = id;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.src_w = src_w;
    xcb_out.src_h = src_h;
    xcb_out.drw_x = drw_x;
    xcb_out.drw_y = drw_y;
    xcb_out.drw_w = drw_w;
    xcb_out.drw_h = drw_h;
    xcb_out.width = width;
    xcb_out.height = height;

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
xcb_xv_shm_put_image_checked (xcb_connection_t *c  /**< */,
                              xcb_xv_port_t     port  /**< */,
                              xcb_drawable_t    drawable  /**< */,
                              xcb_gcontext_t    gc  /**< */,
                              xcb_shm_seg_t     shmseg  /**< */,
                              uint32_t          id  /**< */,
                              uint32_t          offset  /**< */,
                              int16_t           src_x  /**< */,
                              int16_t           src_y  /**< */,
                              uint16_t          src_w  /**< */,
                              uint16_t          src_h  /**< */,
                              int16_t           drw_x  /**< */,
                              int16_t           drw_y  /**< */,
                              uint16_t          drw_w  /**< */,
                              uint16_t          drw_h  /**< */,
                              uint16_t          width  /**< */,
                              uint16_t          height  /**< */,
                              uint8_t           send_event  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_SHM_PUT_IMAGE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_shm_put_image_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.shmseg = shmseg;
    xcb_out.id = id;
    xcb_out.offset = offset;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.src_w = src_w;
    xcb_out.src_h = src_h;
    xcb_out.drw_x = drw_x;
    xcb_out.drw_y = drw_y;
    xcb_out.drw_w = drw_w;
    xcb_out.drw_h = drw_h;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.send_event = send_event;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_xv_shm_put_image (xcb_connection_t *c  /**< */,
                      xcb_xv_port_t     port  /**< */,
                      xcb_drawable_t    drawable  /**< */,
                      xcb_gcontext_t    gc  /**< */,
                      xcb_shm_seg_t     shmseg  /**< */,
                      uint32_t          id  /**< */,
                      uint32_t          offset  /**< */,
                      int16_t           src_x  /**< */,
                      int16_t           src_y  /**< */,
                      uint16_t          src_w  /**< */,
                      uint16_t          src_h  /**< */,
                      int16_t           drw_x  /**< */,
                      int16_t           drw_y  /**< */,
                      uint16_t          drw_w  /**< */,
                      uint16_t          drw_h  /**< */,
                      uint16_t          width  /**< */,
                      uint16_t          height  /**< */,
                      uint8_t           send_event  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xv_id,
        /* opcode */ XCB_XV_SHM_PUT_IMAGE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_xv_shm_put_image_request_t xcb_out;

    xcb_out.port = port;
    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.shmseg = shmseg;
    xcb_out.id = id;
    xcb_out.offset = offset;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.src_w = src_w;
    xcb_out.src_h = src_h;
    xcb_out.drw_x = drw_x;
    xcb_out.drw_y = drw_y;
    xcb_out.drw_w = drw_w;
    xcb_out.drw_h = drw_h;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.send_event = send_event;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

