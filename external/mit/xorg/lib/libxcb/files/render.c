/*
 * This file generated automatically from render.xml by c_client.py.
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
#include "render.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xproto.h"

xcb_extension_t xcb_render_id = { "RENDER", 0 };

void
xcb_render_glyph_next (xcb_render_glyph_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_glyph_t);
}

xcb_generic_iterator_t
xcb_render_glyph_end (xcb_render_glyph_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_render_glyphset_next (xcb_render_glyphset_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_glyphset_t);
}

xcb_generic_iterator_t
xcb_render_glyphset_end (xcb_render_glyphset_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_render_picture_next (xcb_render_picture_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_picture_t);
}

xcb_generic_iterator_t
xcb_render_picture_end (xcb_render_picture_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_render_pictformat_next (xcb_render_pictformat_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_pictformat_t);
}

xcb_generic_iterator_t
xcb_render_pictformat_end (xcb_render_pictformat_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_render_fixed_next (xcb_render_fixed_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_fixed_t);
}

xcb_generic_iterator_t
xcb_render_fixed_end (xcb_render_fixed_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_render_directformat_next (xcb_render_directformat_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_directformat_t);
}

xcb_generic_iterator_t
xcb_render_directformat_end (xcb_render_directformat_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_render_pictforminfo_next (xcb_render_pictforminfo_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_pictforminfo_t);
}

xcb_generic_iterator_t
xcb_render_pictforminfo_end (xcb_render_pictforminfo_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_render_pictvisual_next (xcb_render_pictvisual_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_pictvisual_t);
}

xcb_generic_iterator_t
xcb_render_pictvisual_end (xcb_render_pictvisual_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_render_pictdepth_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_render_pictdepth_t *_aux = (xcb_render_pictdepth_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_pictdepth_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* visuals */
    xcb_block_len += _aux->num_visuals * sizeof(xcb_render_pictvisual_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_pictvisual_t);
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

xcb_render_pictvisual_t *
xcb_render_pictdepth_visuals (const xcb_render_pictdepth_t *R  /**< */)
{
    return (xcb_render_pictvisual_t *) (R + 1);
}

int
xcb_render_pictdepth_visuals_length (const xcb_render_pictdepth_t *R  /**< */)
{
    return R->num_visuals;
}

xcb_render_pictvisual_iterator_t
xcb_render_pictdepth_visuals_iterator (const xcb_render_pictdepth_t *R  /**< */)
{
    xcb_render_pictvisual_iterator_t i;
    i.data = (xcb_render_pictvisual_t *) (R + 1);
    i.rem = R->num_visuals;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_render_pictdepth_next (xcb_render_pictdepth_iterator_t *i  /**< */)
{
    xcb_render_pictdepth_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_render_pictdepth_t *)(((char *)R) + xcb_render_pictdepth_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_render_pictdepth_t *) child.data;
}

xcb_generic_iterator_t
xcb_render_pictdepth_end (xcb_render_pictdepth_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_render_pictdepth_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_render_pictscreen_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_render_pictscreen_t *_aux = (xcb_render_pictscreen_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_render_pictscreen_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* depths */
    for(i=0; i<_aux->num_depths; i++) {
        xcb_tmp_len = xcb_render_pictdepth_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_render_pictdepth_t);
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

int
xcb_render_pictscreen_depths_length (const xcb_render_pictscreen_t *R  /**< */)
{
    return R->num_depths;
}

xcb_render_pictdepth_iterator_t
xcb_render_pictscreen_depths_iterator (const xcb_render_pictscreen_t *R  /**< */)
{
    xcb_render_pictdepth_iterator_t i;
    i.data = (xcb_render_pictdepth_t *) (R + 1);
    i.rem = R->num_depths;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_render_pictscreen_next (xcb_render_pictscreen_iterator_t *i  /**< */)
{
    xcb_render_pictscreen_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_render_pictscreen_t *)(((char *)R) + xcb_render_pictscreen_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_render_pictscreen_t *) child.data;
}

xcb_generic_iterator_t
xcb_render_pictscreen_end (xcb_render_pictscreen_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_render_pictscreen_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

void
xcb_render_indexvalue_next (xcb_render_indexvalue_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_indexvalue_t);
}

xcb_generic_iterator_t
xcb_render_indexvalue_end (xcb_render_indexvalue_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_render_color_next (xcb_render_color_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_color_t);
}

xcb_generic_iterator_t
xcb_render_color_end (xcb_render_color_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_render_pointfix_next (xcb_render_pointfix_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_pointfix_t);
}

xcb_generic_iterator_t
xcb_render_pointfix_end (xcb_render_pointfix_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_render_linefix_next (xcb_render_linefix_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_linefix_t);
}

xcb_generic_iterator_t
xcb_render_linefix_end (xcb_render_linefix_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_render_triangle_next (xcb_render_triangle_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_triangle_t);
}

xcb_generic_iterator_t
xcb_render_triangle_end (xcb_render_triangle_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_render_trapezoid_next (xcb_render_trapezoid_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_trapezoid_t);
}

xcb_generic_iterator_t
xcb_render_trapezoid_end (xcb_render_trapezoid_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_render_glyphinfo_next (xcb_render_glyphinfo_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_glyphinfo_t);
}

xcb_generic_iterator_t
xcb_render_glyphinfo_end (xcb_render_glyphinfo_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

xcb_render_query_version_cookie_t
xcb_render_query_version (xcb_connection_t *c  /**< */,
                          uint32_t          client_major_version  /**< */,
                          uint32_t          client_minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_render_query_version_cookie_t xcb_ret;
    xcb_render_query_version_request_t xcb_out;

    xcb_out.client_major_version = client_major_version;
    xcb_out.client_minor_version = client_minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_render_query_version_cookie_t
xcb_render_query_version_unchecked (xcb_connection_t *c  /**< */,
                                    uint32_t          client_major_version  /**< */,
                                    uint32_t          client_minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_render_query_version_cookie_t xcb_ret;
    xcb_render_query_version_request_t xcb_out;

    xcb_out.client_major_version = client_major_version;
    xcb_out.client_minor_version = client_minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_render_query_version_reply_t *
xcb_render_query_version_reply (xcb_connection_t                   *c  /**< */,
                                xcb_render_query_version_cookie_t   cookie  /**< */,
                                xcb_generic_error_t               **e  /**< */)
{
    return (xcb_render_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_render_query_pict_formats_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_render_query_pict_formats_reply_t *_aux = (xcb_render_query_pict_formats_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_render_query_pict_formats_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* formats */
    xcb_block_len += _aux->num_formats * sizeof(xcb_render_pictforminfo_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_pictforminfo_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* screens */
    for(i=0; i<_aux->num_screens; i++) {
        xcb_tmp_len = xcb_render_pictscreen_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_render_pictscreen_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* subpixels */
    xcb_block_len += _aux->num_subpixel * sizeof(uint32_t);
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

xcb_render_query_pict_formats_cookie_t
xcb_render_query_pict_formats (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_QUERY_PICT_FORMATS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_render_query_pict_formats_cookie_t xcb_ret;
    xcb_render_query_pict_formats_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_render_query_pict_formats_cookie_t
xcb_render_query_pict_formats_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_QUERY_PICT_FORMATS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_render_query_pict_formats_cookie_t xcb_ret;
    xcb_render_query_pict_formats_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_render_pictforminfo_t *
xcb_render_query_pict_formats_formats (const xcb_render_query_pict_formats_reply_t *R  /**< */)
{
    return (xcb_render_pictforminfo_t *) (R + 1);
}

int
xcb_render_query_pict_formats_formats_length (const xcb_render_query_pict_formats_reply_t *R  /**< */)
{
    return R->num_formats;
}

xcb_render_pictforminfo_iterator_t
xcb_render_query_pict_formats_formats_iterator (const xcb_render_query_pict_formats_reply_t *R  /**< */)
{
    xcb_render_pictforminfo_iterator_t i;
    i.data = (xcb_render_pictforminfo_t *) (R + 1);
    i.rem = R->num_formats;
    i.index = (char *) i.data - (char *) R;
    return i;
}

int
xcb_render_query_pict_formats_screens_length (const xcb_render_query_pict_formats_reply_t *R  /**< */)
{
    return R->num_screens;
}

xcb_render_pictscreen_iterator_t
xcb_render_query_pict_formats_screens_iterator (const xcb_render_query_pict_formats_reply_t *R  /**< */)
{
    xcb_render_pictscreen_iterator_t i;
    xcb_generic_iterator_t prev = xcb_render_pictforminfo_end(xcb_render_query_pict_formats_formats_iterator(R));
    i.data = (xcb_render_pictscreen_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_render_pictscreen_t, prev.index));
    i.rem = R->num_screens;
    i.index = (char *) i.data - (char *) R;
    return i;
}

uint32_t *
xcb_render_query_pict_formats_subpixels (const xcb_render_query_pict_formats_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_render_pictscreen_end(xcb_render_query_pict_formats_screens_iterator(R));
    return (uint32_t *) ((char *) prev.data + XCB_TYPE_PAD(uint32_t, prev.index) + 0);
}

int
xcb_render_query_pict_formats_subpixels_length (const xcb_render_query_pict_formats_reply_t *R  /**< */)
{
    return R->num_subpixel;
}

xcb_generic_iterator_t
xcb_render_query_pict_formats_subpixels_end (const xcb_render_query_pict_formats_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_render_pictscreen_end(xcb_render_query_pict_formats_screens_iterator(R));
    i.data = ((uint32_t *) child.data) + (R->num_subpixel);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_render_query_pict_formats_reply_t *
xcb_render_query_pict_formats_reply (xcb_connection_t                        *c  /**< */,
                                     xcb_render_query_pict_formats_cookie_t   cookie  /**< */,
                                     xcb_generic_error_t                    **e  /**< */)
{
    return (xcb_render_query_pict_formats_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_render_query_pict_index_values_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_render_query_pict_index_values_reply_t *_aux = (xcb_render_query_pict_index_values_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_query_pict_index_values_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* values */
    xcb_block_len += _aux->num_values * sizeof(xcb_render_indexvalue_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_indexvalue_t);
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

xcb_render_query_pict_index_values_cookie_t
xcb_render_query_pict_index_values (xcb_connection_t        *c  /**< */,
                                    xcb_render_pictformat_t  format  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_QUERY_PICT_INDEX_VALUES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_render_query_pict_index_values_cookie_t xcb_ret;
    xcb_render_query_pict_index_values_request_t xcb_out;

    xcb_out.format = format;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_render_query_pict_index_values_cookie_t
xcb_render_query_pict_index_values_unchecked (xcb_connection_t        *c  /**< */,
                                              xcb_render_pictformat_t  format  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_QUERY_PICT_INDEX_VALUES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_render_query_pict_index_values_cookie_t xcb_ret;
    xcb_render_query_pict_index_values_request_t xcb_out;

    xcb_out.format = format;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_render_indexvalue_t *
xcb_render_query_pict_index_values_values (const xcb_render_query_pict_index_values_reply_t *R  /**< */)
{
    return (xcb_render_indexvalue_t *) (R + 1);
}

int
xcb_render_query_pict_index_values_values_length (const xcb_render_query_pict_index_values_reply_t *R  /**< */)
{
    return R->num_values;
}

xcb_render_indexvalue_iterator_t
xcb_render_query_pict_index_values_values_iterator (const xcb_render_query_pict_index_values_reply_t *R  /**< */)
{
    xcb_render_indexvalue_iterator_t i;
    i.data = (xcb_render_indexvalue_t *) (R + 1);
    i.rem = R->num_values;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_render_query_pict_index_values_reply_t *
xcb_render_query_pict_index_values_reply (xcb_connection_t                             *c  /**< */,
                                          xcb_render_query_pict_index_values_cookie_t   cookie  /**< */,
                                          xcb_generic_error_t                         **e  /**< */)
{
    return (xcb_render_query_pict_index_values_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_render_create_picture_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_render_create_picture_request_t *_aux = (xcb_render_create_picture_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_create_picture_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* value_list */
    xcb_block_len += xcb_popcount(_aux->value_mask) * sizeof(uint32_t);
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
xcb_render_create_picture_checked (xcb_connection_t        *c  /**< */,
                                   xcb_render_picture_t     pid  /**< */,
                                   xcb_drawable_t           drawable  /**< */,
                                   xcb_render_pictformat_t  format  /**< */,
                                   uint32_t                 value_mask  /**< */,
                                   const uint32_t          *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_PICTURE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_picture_request_t xcb_out;

    xcb_out.pid = pid;
    xcb_out.drawable = drawable;
    xcb_out.format = format;
    xcb_out.value_mask = value_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_create_picture (xcb_connection_t        *c  /**< */,
                           xcb_render_picture_t     pid  /**< */,
                           xcb_drawable_t           drawable  /**< */,
                           xcb_render_pictformat_t  format  /**< */,
                           uint32_t                 value_mask  /**< */,
                           const uint32_t          *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_PICTURE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_picture_request_t xcb_out;

    xcb_out.pid = pid;
    xcb_out.drawable = drawable;
    xcb_out.format = format;
    xcb_out.value_mask = value_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_change_picture_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_render_change_picture_request_t *_aux = (xcb_render_change_picture_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_change_picture_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* value_list */
    xcb_block_len += xcb_popcount(_aux->value_mask) * sizeof(uint32_t);
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
xcb_render_change_picture_checked (xcb_connection_t     *c  /**< */,
                                   xcb_render_picture_t  picture  /**< */,
                                   uint32_t              value_mask  /**< */,
                                   const uint32_t       *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CHANGE_PICTURE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_change_picture_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.value_mask = value_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_change_picture (xcb_connection_t     *c  /**< */,
                           xcb_render_picture_t  picture  /**< */,
                           uint32_t              value_mask  /**< */,
                           const uint32_t       *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CHANGE_PICTURE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_change_picture_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.value_mask = value_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len = xcb_popcount(value_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_set_picture_clip_rectangles_sizeof (const void  *_buffer  /**< */,
                                               uint32_t     rectangles_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_set_picture_clip_rectangles_request_t);
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
xcb_render_set_picture_clip_rectangles_checked (xcb_connection_t      *c  /**< */,
                                                xcb_render_picture_t   picture  /**< */,
                                                int16_t                clip_x_origin  /**< */,
                                                int16_t                clip_y_origin  /**< */,
                                                uint32_t               rectangles_len  /**< */,
                                                const xcb_rectangle_t *rectangles  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_SET_PICTURE_CLIP_RECTANGLES,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_set_picture_clip_rectangles_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.clip_x_origin = clip_x_origin;
    xcb_out.clip_y_origin = clip_y_origin;

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
xcb_render_set_picture_clip_rectangles (xcb_connection_t      *c  /**< */,
                                        xcb_render_picture_t   picture  /**< */,
                                        int16_t                clip_x_origin  /**< */,
                                        int16_t                clip_y_origin  /**< */,
                                        uint32_t               rectangles_len  /**< */,
                                        const xcb_rectangle_t *rectangles  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_SET_PICTURE_CLIP_RECTANGLES,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_set_picture_clip_rectangles_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.clip_x_origin = clip_x_origin;
    xcb_out.clip_y_origin = clip_y_origin;

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
xcb_render_free_picture_checked (xcb_connection_t     *c  /**< */,
                                 xcb_render_picture_t  picture  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_FREE_PICTURE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_free_picture_request_t xcb_out;

    xcb_out.picture = picture;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_free_picture (xcb_connection_t     *c  /**< */,
                         xcb_render_picture_t  picture  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_FREE_PICTURE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_free_picture_request_t xcb_out;

    xcb_out.picture = picture;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_composite_checked (xcb_connection_t     *c  /**< */,
                              uint8_t               op  /**< */,
                              xcb_render_picture_t  src  /**< */,
                              xcb_render_picture_t  mask  /**< */,
                              xcb_render_picture_t  dst  /**< */,
                              int16_t               src_x  /**< */,
                              int16_t               src_y  /**< */,
                              int16_t               mask_x  /**< */,
                              int16_t               mask_y  /**< */,
                              int16_t               dst_x  /**< */,
                              int16_t               dst_y  /**< */,
                              uint16_t              width  /**< */,
                              uint16_t              height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_COMPOSITE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_composite_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.mask = mask;
    xcb_out.dst = dst;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.mask_x = mask_x;
    xcb_out.mask_y = mask_y;
    xcb_out.dst_x = dst_x;
    xcb_out.dst_y = dst_y;
    xcb_out.width = width;
    xcb_out.height = height;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_composite (xcb_connection_t     *c  /**< */,
                      uint8_t               op  /**< */,
                      xcb_render_picture_t  src  /**< */,
                      xcb_render_picture_t  mask  /**< */,
                      xcb_render_picture_t  dst  /**< */,
                      int16_t               src_x  /**< */,
                      int16_t               src_y  /**< */,
                      int16_t               mask_x  /**< */,
                      int16_t               mask_y  /**< */,
                      int16_t               dst_x  /**< */,
                      int16_t               dst_y  /**< */,
                      uint16_t              width  /**< */,
                      uint16_t              height  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_COMPOSITE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_composite_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.mask = mask;
    xcb_out.dst = dst;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.mask_x = mask_x;
    xcb_out.mask_y = mask_y;
    xcb_out.dst_x = dst_x;
    xcb_out.dst_y = dst_y;
    xcb_out.width = width;
    xcb_out.height = height;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_trapezoids_sizeof (const void  *_buffer  /**< */,
                              uint32_t     traps_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_trapezoids_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* traps */
    xcb_block_len += traps_len * sizeof(xcb_render_trapezoid_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_trapezoid_t);
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
xcb_render_trapezoids_checked (xcb_connection_t             *c  /**< */,
                               uint8_t                       op  /**< */,
                               xcb_render_picture_t          src  /**< */,
                               xcb_render_picture_t          dst  /**< */,
                               xcb_render_pictformat_t       mask_format  /**< */,
                               int16_t                       src_x  /**< */,
                               int16_t                       src_y  /**< */,
                               uint32_t                      traps_len  /**< */,
                               const xcb_render_trapezoid_t *traps  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_TRAPEZOIDS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_trapezoids_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.dst = dst;
    xcb_out.mask_format = mask_format;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_trapezoid_t traps */
    xcb_parts[4].iov_base = (char *) traps;
    xcb_parts[4].iov_len = traps_len * sizeof(xcb_render_trapezoid_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_trapezoids (xcb_connection_t             *c  /**< */,
                       uint8_t                       op  /**< */,
                       xcb_render_picture_t          src  /**< */,
                       xcb_render_picture_t          dst  /**< */,
                       xcb_render_pictformat_t       mask_format  /**< */,
                       int16_t                       src_x  /**< */,
                       int16_t                       src_y  /**< */,
                       uint32_t                      traps_len  /**< */,
                       const xcb_render_trapezoid_t *traps  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_TRAPEZOIDS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_trapezoids_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.dst = dst;
    xcb_out.mask_format = mask_format;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_trapezoid_t traps */
    xcb_parts[4].iov_base = (char *) traps;
    xcb_parts[4].iov_len = traps_len * sizeof(xcb_render_trapezoid_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_triangles_sizeof (const void  *_buffer  /**< */,
                             uint32_t     triangles_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_triangles_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* triangles */
    xcb_block_len += triangles_len * sizeof(xcb_render_triangle_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_triangle_t);
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
xcb_render_triangles_checked (xcb_connection_t            *c  /**< */,
                              uint8_t                      op  /**< */,
                              xcb_render_picture_t         src  /**< */,
                              xcb_render_picture_t         dst  /**< */,
                              xcb_render_pictformat_t      mask_format  /**< */,
                              int16_t                      src_x  /**< */,
                              int16_t                      src_y  /**< */,
                              uint32_t                     triangles_len  /**< */,
                              const xcb_render_triangle_t *triangles  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_TRIANGLES,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_triangles_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.dst = dst;
    xcb_out.mask_format = mask_format;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_triangle_t triangles */
    xcb_parts[4].iov_base = (char *) triangles;
    xcb_parts[4].iov_len = triangles_len * sizeof(xcb_render_triangle_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_triangles (xcb_connection_t            *c  /**< */,
                      uint8_t                      op  /**< */,
                      xcb_render_picture_t         src  /**< */,
                      xcb_render_picture_t         dst  /**< */,
                      xcb_render_pictformat_t      mask_format  /**< */,
                      int16_t                      src_x  /**< */,
                      int16_t                      src_y  /**< */,
                      uint32_t                     triangles_len  /**< */,
                      const xcb_render_triangle_t *triangles  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_TRIANGLES,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_triangles_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.dst = dst;
    xcb_out.mask_format = mask_format;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_triangle_t triangles */
    xcb_parts[4].iov_base = (char *) triangles;
    xcb_parts[4].iov_len = triangles_len * sizeof(xcb_render_triangle_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_tri_strip_sizeof (const void  *_buffer  /**< */,
                             uint32_t     points_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_tri_strip_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* points */
    xcb_block_len += points_len * sizeof(xcb_render_pointfix_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_pointfix_t);
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
xcb_render_tri_strip_checked (xcb_connection_t            *c  /**< */,
                              uint8_t                      op  /**< */,
                              xcb_render_picture_t         src  /**< */,
                              xcb_render_picture_t         dst  /**< */,
                              xcb_render_pictformat_t      mask_format  /**< */,
                              int16_t                      src_x  /**< */,
                              int16_t                      src_y  /**< */,
                              uint32_t                     points_len  /**< */,
                              const xcb_render_pointfix_t *points  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_TRI_STRIP,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_tri_strip_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.dst = dst;
    xcb_out.mask_format = mask_format;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_pointfix_t points */
    xcb_parts[4].iov_base = (char *) points;
    xcb_parts[4].iov_len = points_len * sizeof(xcb_render_pointfix_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_tri_strip (xcb_connection_t            *c  /**< */,
                      uint8_t                      op  /**< */,
                      xcb_render_picture_t         src  /**< */,
                      xcb_render_picture_t         dst  /**< */,
                      xcb_render_pictformat_t      mask_format  /**< */,
                      int16_t                      src_x  /**< */,
                      int16_t                      src_y  /**< */,
                      uint32_t                     points_len  /**< */,
                      const xcb_render_pointfix_t *points  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_TRI_STRIP,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_tri_strip_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.dst = dst;
    xcb_out.mask_format = mask_format;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_pointfix_t points */
    xcb_parts[4].iov_base = (char *) points;
    xcb_parts[4].iov_len = points_len * sizeof(xcb_render_pointfix_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_tri_fan_sizeof (const void  *_buffer  /**< */,
                           uint32_t     points_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_tri_fan_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* points */
    xcb_block_len += points_len * sizeof(xcb_render_pointfix_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_pointfix_t);
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
xcb_render_tri_fan_checked (xcb_connection_t            *c  /**< */,
                            uint8_t                      op  /**< */,
                            xcb_render_picture_t         src  /**< */,
                            xcb_render_picture_t         dst  /**< */,
                            xcb_render_pictformat_t      mask_format  /**< */,
                            int16_t                      src_x  /**< */,
                            int16_t                      src_y  /**< */,
                            uint32_t                     points_len  /**< */,
                            const xcb_render_pointfix_t *points  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_TRI_FAN,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_tri_fan_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.dst = dst;
    xcb_out.mask_format = mask_format;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_pointfix_t points */
    xcb_parts[4].iov_base = (char *) points;
    xcb_parts[4].iov_len = points_len * sizeof(xcb_render_pointfix_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_tri_fan (xcb_connection_t            *c  /**< */,
                    uint8_t                      op  /**< */,
                    xcb_render_picture_t         src  /**< */,
                    xcb_render_picture_t         dst  /**< */,
                    xcb_render_pictformat_t      mask_format  /**< */,
                    int16_t                      src_x  /**< */,
                    int16_t                      src_y  /**< */,
                    uint32_t                     points_len  /**< */,
                    const xcb_render_pointfix_t *points  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_TRI_FAN,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_tri_fan_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.dst = dst;
    xcb_out.mask_format = mask_format;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_pointfix_t points */
    xcb_parts[4].iov_base = (char *) points;
    xcb_parts[4].iov_len = points_len * sizeof(xcb_render_pointfix_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_create_glyph_set_checked (xcb_connection_t        *c  /**< */,
                                     xcb_render_glyphset_t    gsid  /**< */,
                                     xcb_render_pictformat_t  format  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_GLYPH_SET,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_glyph_set_request_t xcb_out;

    xcb_out.gsid = gsid;
    xcb_out.format = format;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_create_glyph_set (xcb_connection_t        *c  /**< */,
                             xcb_render_glyphset_t    gsid  /**< */,
                             xcb_render_pictformat_t  format  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_GLYPH_SET,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_glyph_set_request_t xcb_out;

    xcb_out.gsid = gsid;
    xcb_out.format = format;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_reference_glyph_set_checked (xcb_connection_t      *c  /**< */,
                                        xcb_render_glyphset_t  gsid  /**< */,
                                        xcb_render_glyphset_t  existing  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_REFERENCE_GLYPH_SET,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_reference_glyph_set_request_t xcb_out;

    xcb_out.gsid = gsid;
    xcb_out.existing = existing;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_reference_glyph_set (xcb_connection_t      *c  /**< */,
                                xcb_render_glyphset_t  gsid  /**< */,
                                xcb_render_glyphset_t  existing  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_REFERENCE_GLYPH_SET,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_reference_glyph_set_request_t xcb_out;

    xcb_out.gsid = gsid;
    xcb_out.existing = existing;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_free_glyph_set_checked (xcb_connection_t      *c  /**< */,
                                   xcb_render_glyphset_t  glyphset  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_FREE_GLYPH_SET,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_free_glyph_set_request_t xcb_out;

    xcb_out.glyphset = glyphset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_free_glyph_set (xcb_connection_t      *c  /**< */,
                           xcb_render_glyphset_t  glyphset  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_FREE_GLYPH_SET,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_free_glyph_set_request_t xcb_out;

    xcb_out.glyphset = glyphset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_add_glyphs_sizeof (const void  *_buffer  /**< */,
                              uint32_t     data_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_render_add_glyphs_request_t *_aux = (xcb_render_add_glyphs_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_add_glyphs_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* glyphids */
    xcb_block_len += _aux->glyphs_len * sizeof(uint32_t);
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
    /* glyphs */
    xcb_block_len += _aux->glyphs_len * sizeof(xcb_render_glyphinfo_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_glyphinfo_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
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
xcb_render_add_glyphs_checked (xcb_connection_t             *c  /**< */,
                               xcb_render_glyphset_t         glyphset  /**< */,
                               uint32_t                      glyphs_len  /**< */,
                               const uint32_t               *glyphids  /**< */,
                               const xcb_render_glyphinfo_t *glyphs  /**< */,
                               uint32_t                      data_len  /**< */,
                               const uint8_t                *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 8,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_ADD_GLYPHS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[10];
    xcb_void_cookie_t xcb_ret;
    xcb_render_add_glyphs_request_t xcb_out;

    xcb_out.glyphset = glyphset;
    xcb_out.glyphs_len = glyphs_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t glyphids */
    xcb_parts[4].iov_base = (char *) glyphids;
    xcb_parts[4].iov_len = glyphs_len * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_render_glyphinfo_t glyphs */
    xcb_parts[6].iov_base = (char *) glyphs;
    xcb_parts[6].iov_len = glyphs_len * sizeof(xcb_render_glyphinfo_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;
    /* uint8_t data */
    xcb_parts[8].iov_base = (char *) data;
    xcb_parts[8].iov_len = data_len * sizeof(uint8_t);
    xcb_parts[9].iov_base = 0;
    xcb_parts[9].iov_len = -xcb_parts[8].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_add_glyphs (xcb_connection_t             *c  /**< */,
                       xcb_render_glyphset_t         glyphset  /**< */,
                       uint32_t                      glyphs_len  /**< */,
                       const uint32_t               *glyphids  /**< */,
                       const xcb_render_glyphinfo_t *glyphs  /**< */,
                       uint32_t                      data_len  /**< */,
                       const uint8_t                *data  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 8,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_ADD_GLYPHS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[10];
    xcb_void_cookie_t xcb_ret;
    xcb_render_add_glyphs_request_t xcb_out;

    xcb_out.glyphset = glyphset;
    xcb_out.glyphs_len = glyphs_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t glyphids */
    xcb_parts[4].iov_base = (char *) glyphids;
    xcb_parts[4].iov_len = glyphs_len * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_render_glyphinfo_t glyphs */
    xcb_parts[6].iov_base = (char *) glyphs;
    xcb_parts[6].iov_len = glyphs_len * sizeof(xcb_render_glyphinfo_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;
    /* uint8_t data */
    xcb_parts[8].iov_base = (char *) data;
    xcb_parts[8].iov_len = data_len * sizeof(uint8_t);
    xcb_parts[9].iov_base = 0;
    xcb_parts[9].iov_len = -xcb_parts[8].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_free_glyphs_sizeof (const void  *_buffer  /**< */,
                               uint32_t     glyphs_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_free_glyphs_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* glyphs */
    xcb_block_len += glyphs_len * sizeof(xcb_render_glyph_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_glyph_t);
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
xcb_render_free_glyphs_checked (xcb_connection_t         *c  /**< */,
                                xcb_render_glyphset_t     glyphset  /**< */,
                                uint32_t                  glyphs_len  /**< */,
                                const xcb_render_glyph_t *glyphs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_FREE_GLYPHS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_free_glyphs_request_t xcb_out;

    xcb_out.glyphset = glyphset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_glyph_t glyphs */
    xcb_parts[4].iov_base = (char *) glyphs;
    xcb_parts[4].iov_len = glyphs_len * sizeof(xcb_render_glyph_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_free_glyphs (xcb_connection_t         *c  /**< */,
                        xcb_render_glyphset_t     glyphset  /**< */,
                        uint32_t                  glyphs_len  /**< */,
                        const xcb_render_glyph_t *glyphs  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_FREE_GLYPHS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_free_glyphs_request_t xcb_out;

    xcb_out.glyphset = glyphset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_glyph_t glyphs */
    xcb_parts[4].iov_base = (char *) glyphs;
    xcb_parts[4].iov_len = glyphs_len * sizeof(xcb_render_glyph_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_composite_glyphs_8_sizeof (const void  *_buffer  /**< */,
                                      uint32_t     glyphcmds_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_composite_glyphs_8_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* glyphcmds */
    xcb_block_len += glyphcmds_len * sizeof(uint8_t);
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
xcb_render_composite_glyphs_8_checked (xcb_connection_t        *c  /**< */,
                                       uint8_t                  op  /**< */,
                                       xcb_render_picture_t     src  /**< */,
                                       xcb_render_picture_t     dst  /**< */,
                                       xcb_render_pictformat_t  mask_format  /**< */,
                                       xcb_render_glyphset_t    glyphset  /**< */,
                                       int16_t                  src_x  /**< */,
                                       int16_t                  src_y  /**< */,
                                       uint32_t                 glyphcmds_len  /**< */,
                                       const uint8_t           *glyphcmds  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_COMPOSITE_GLYPHS_8,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_composite_glyphs_8_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.dst = dst;
    xcb_out.mask_format = mask_format;
    xcb_out.glyphset = glyphset;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t glyphcmds */
    xcb_parts[4].iov_base = (char *) glyphcmds;
    xcb_parts[4].iov_len = glyphcmds_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_composite_glyphs_8 (xcb_connection_t        *c  /**< */,
                               uint8_t                  op  /**< */,
                               xcb_render_picture_t     src  /**< */,
                               xcb_render_picture_t     dst  /**< */,
                               xcb_render_pictformat_t  mask_format  /**< */,
                               xcb_render_glyphset_t    glyphset  /**< */,
                               int16_t                  src_x  /**< */,
                               int16_t                  src_y  /**< */,
                               uint32_t                 glyphcmds_len  /**< */,
                               const uint8_t           *glyphcmds  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_COMPOSITE_GLYPHS_8,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_composite_glyphs_8_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.dst = dst;
    xcb_out.mask_format = mask_format;
    xcb_out.glyphset = glyphset;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t glyphcmds */
    xcb_parts[4].iov_base = (char *) glyphcmds;
    xcb_parts[4].iov_len = glyphcmds_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_composite_glyphs_16_sizeof (const void  *_buffer  /**< */,
                                       uint32_t     glyphcmds_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_composite_glyphs_16_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* glyphcmds */
    xcb_block_len += glyphcmds_len * sizeof(uint8_t);
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
xcb_render_composite_glyphs_16_checked (xcb_connection_t        *c  /**< */,
                                        uint8_t                  op  /**< */,
                                        xcb_render_picture_t     src  /**< */,
                                        xcb_render_picture_t     dst  /**< */,
                                        xcb_render_pictformat_t  mask_format  /**< */,
                                        xcb_render_glyphset_t    glyphset  /**< */,
                                        int16_t                  src_x  /**< */,
                                        int16_t                  src_y  /**< */,
                                        uint32_t                 glyphcmds_len  /**< */,
                                        const uint8_t           *glyphcmds  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_COMPOSITE_GLYPHS_16,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_composite_glyphs_16_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.dst = dst;
    xcb_out.mask_format = mask_format;
    xcb_out.glyphset = glyphset;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t glyphcmds */
    xcb_parts[4].iov_base = (char *) glyphcmds;
    xcb_parts[4].iov_len = glyphcmds_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_composite_glyphs_16 (xcb_connection_t        *c  /**< */,
                                uint8_t                  op  /**< */,
                                xcb_render_picture_t     src  /**< */,
                                xcb_render_picture_t     dst  /**< */,
                                xcb_render_pictformat_t  mask_format  /**< */,
                                xcb_render_glyphset_t    glyphset  /**< */,
                                int16_t                  src_x  /**< */,
                                int16_t                  src_y  /**< */,
                                uint32_t                 glyphcmds_len  /**< */,
                                const uint8_t           *glyphcmds  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_COMPOSITE_GLYPHS_16,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_composite_glyphs_16_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.dst = dst;
    xcb_out.mask_format = mask_format;
    xcb_out.glyphset = glyphset;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t glyphcmds */
    xcb_parts[4].iov_base = (char *) glyphcmds;
    xcb_parts[4].iov_len = glyphcmds_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_composite_glyphs_32_sizeof (const void  *_buffer  /**< */,
                                       uint32_t     glyphcmds_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_composite_glyphs_32_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* glyphcmds */
    xcb_block_len += glyphcmds_len * sizeof(uint8_t);
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
xcb_render_composite_glyphs_32_checked (xcb_connection_t        *c  /**< */,
                                        uint8_t                  op  /**< */,
                                        xcb_render_picture_t     src  /**< */,
                                        xcb_render_picture_t     dst  /**< */,
                                        xcb_render_pictformat_t  mask_format  /**< */,
                                        xcb_render_glyphset_t    glyphset  /**< */,
                                        int16_t                  src_x  /**< */,
                                        int16_t                  src_y  /**< */,
                                        uint32_t                 glyphcmds_len  /**< */,
                                        const uint8_t           *glyphcmds  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_COMPOSITE_GLYPHS_32,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_composite_glyphs_32_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.dst = dst;
    xcb_out.mask_format = mask_format;
    xcb_out.glyphset = glyphset;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t glyphcmds */
    xcb_parts[4].iov_base = (char *) glyphcmds;
    xcb_parts[4].iov_len = glyphcmds_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_composite_glyphs_32 (xcb_connection_t        *c  /**< */,
                                uint8_t                  op  /**< */,
                                xcb_render_picture_t     src  /**< */,
                                xcb_render_picture_t     dst  /**< */,
                                xcb_render_pictformat_t  mask_format  /**< */,
                                xcb_render_glyphset_t    glyphset  /**< */,
                                int16_t                  src_x  /**< */,
                                int16_t                  src_y  /**< */,
                                uint32_t                 glyphcmds_len  /**< */,
                                const uint8_t           *glyphcmds  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_COMPOSITE_GLYPHS_32,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_composite_glyphs_32_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.src = src;
    xcb_out.dst = dst;
    xcb_out.mask_format = mask_format;
    xcb_out.glyphset = glyphset;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t glyphcmds */
    xcb_parts[4].iov_base = (char *) glyphcmds;
    xcb_parts[4].iov_len = glyphcmds_len * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_fill_rectangles_sizeof (const void  *_buffer  /**< */,
                                   uint32_t     rects_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_fill_rectangles_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* rects */
    xcb_block_len += rects_len * sizeof(xcb_rectangle_t);
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
xcb_render_fill_rectangles_checked (xcb_connection_t      *c  /**< */,
                                    uint8_t                op  /**< */,
                                    xcb_render_picture_t   dst  /**< */,
                                    xcb_render_color_t     color  /**< */,
                                    uint32_t               rects_len  /**< */,
                                    const xcb_rectangle_t *rects  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_FILL_RECTANGLES,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_fill_rectangles_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.dst = dst;
    xcb_out.color = color;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_rectangle_t rects */
    xcb_parts[4].iov_base = (char *) rects;
    xcb_parts[4].iov_len = rects_len * sizeof(xcb_rectangle_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_fill_rectangles (xcb_connection_t      *c  /**< */,
                            uint8_t                op  /**< */,
                            xcb_render_picture_t   dst  /**< */,
                            xcb_render_color_t     color  /**< */,
                            uint32_t               rects_len  /**< */,
                            const xcb_rectangle_t *rects  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_FILL_RECTANGLES,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_fill_rectangles_request_t xcb_out;

    xcb_out.op = op;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.dst = dst;
    xcb_out.color = color;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_rectangle_t rects */
    xcb_parts[4].iov_base = (char *) rects;
    xcb_parts[4].iov_len = rects_len * sizeof(xcb_rectangle_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_create_cursor_checked (xcb_connection_t     *c  /**< */,
                                  xcb_cursor_t          cid  /**< */,
                                  xcb_render_picture_t  source  /**< */,
                                  uint16_t              x  /**< */,
                                  uint16_t              y  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_CURSOR,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_cursor_request_t xcb_out;

    xcb_out.cid = cid;
    xcb_out.source = source;
    xcb_out.x = x;
    xcb_out.y = y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_create_cursor (xcb_connection_t     *c  /**< */,
                          xcb_cursor_t          cid  /**< */,
                          xcb_render_picture_t  source  /**< */,
                          uint16_t              x  /**< */,
                          uint16_t              y  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_CURSOR,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_cursor_request_t xcb_out;

    xcb_out.cid = cid;
    xcb_out.source = source;
    xcb_out.x = x;
    xcb_out.y = y;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

void
xcb_render_transform_next (xcb_render_transform_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_transform_t);
}

xcb_generic_iterator_t
xcb_render_transform_end (xcb_render_transform_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

xcb_void_cookie_t
xcb_render_set_picture_transform_checked (xcb_connection_t       *c  /**< */,
                                          xcb_render_picture_t    picture  /**< */,
                                          xcb_render_transform_t  transform  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_SET_PICTURE_TRANSFORM,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_set_picture_transform_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.transform = transform;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_set_picture_transform (xcb_connection_t       *c  /**< */,
                                  xcb_render_picture_t    picture  /**< */,
                                  xcb_render_transform_t  transform  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_SET_PICTURE_TRANSFORM,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_set_picture_transform_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.transform = transform;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_query_filters_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_render_query_filters_reply_t *_aux = (xcb_render_query_filters_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_render_query_filters_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* aliases */
    xcb_block_len += _aux->num_aliases * sizeof(uint16_t);
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
    /* filters */
    for(i=0; i<_aux->num_filters; i++) {
        xcb_tmp_len = xcb_str_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_str_t);
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

xcb_render_query_filters_cookie_t
xcb_render_query_filters (xcb_connection_t *c  /**< */,
                          xcb_drawable_t    drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_QUERY_FILTERS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_render_query_filters_cookie_t xcb_ret;
    xcb_render_query_filters_request_t xcb_out;

    xcb_out.drawable = drawable;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_render_query_filters_cookie_t
xcb_render_query_filters_unchecked (xcb_connection_t *c  /**< */,
                                    xcb_drawable_t    drawable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_QUERY_FILTERS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_render_query_filters_cookie_t xcb_ret;
    xcb_render_query_filters_request_t xcb_out;

    xcb_out.drawable = drawable;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint16_t *
xcb_render_query_filters_aliases (const xcb_render_query_filters_reply_t *R  /**< */)
{
    return (uint16_t *) (R + 1);
}

int
xcb_render_query_filters_aliases_length (const xcb_render_query_filters_reply_t *R  /**< */)
{
    return R->num_aliases;
}

xcb_generic_iterator_t
xcb_render_query_filters_aliases_end (const xcb_render_query_filters_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint16_t *) (R + 1)) + (R->num_aliases);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

int
xcb_render_query_filters_filters_length (const xcb_render_query_filters_reply_t *R  /**< */)
{
    return R->num_filters;
}

xcb_str_iterator_t
xcb_render_query_filters_filters_iterator (const xcb_render_query_filters_reply_t *R  /**< */)
{
    xcb_str_iterator_t i;
    xcb_generic_iterator_t prev = xcb_render_query_filters_aliases_end(R);
    i.data = (xcb_str_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_str_t, prev.index));
    i.rem = R->num_filters;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_render_query_filters_reply_t *
xcb_render_query_filters_reply (xcb_connection_t                   *c  /**< */,
                                xcb_render_query_filters_cookie_t   cookie  /**< */,
                                xcb_generic_error_t               **e  /**< */)
{
    return (xcb_render_query_filters_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_render_set_picture_filter_sizeof (const void  *_buffer  /**< */,
                                      uint32_t     values_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_render_set_picture_filter_request_t *_aux = (xcb_render_set_picture_filter_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_set_picture_filter_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* filter */
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
    /* values */
    xcb_block_len += values_len * sizeof(xcb_render_fixed_t);
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
xcb_render_set_picture_filter_checked (xcb_connection_t         *c  /**< */,
                                       xcb_render_picture_t      picture  /**< */,
                                       uint16_t                  filter_len  /**< */,
                                       const char               *filter  /**< */,
                                       uint32_t                  values_len  /**< */,
                                       const xcb_render_fixed_t *values  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_SET_PICTURE_FILTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[8];
    xcb_void_cookie_t xcb_ret;
    xcb_render_set_picture_filter_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.filter_len = filter_len;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char filter */
    xcb_parts[4].iov_base = (char *) filter;
    xcb_parts[4].iov_len = filter_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_render_fixed_t values */
    xcb_parts[6].iov_base = (char *) values;
    xcb_parts[6].iov_len = values_len * sizeof(xcb_render_fixed_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_set_picture_filter (xcb_connection_t         *c  /**< */,
                               xcb_render_picture_t      picture  /**< */,
                               uint16_t                  filter_len  /**< */,
                               const char               *filter  /**< */,
                               uint32_t                  values_len  /**< */,
                               const xcb_render_fixed_t *values  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_SET_PICTURE_FILTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[8];
    xcb_void_cookie_t xcb_ret;
    xcb_render_set_picture_filter_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.filter_len = filter_len;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* char filter */
    xcb_parts[4].iov_base = (char *) filter;
    xcb_parts[4].iov_len = filter_len * sizeof(char);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_render_fixed_t values */
    xcb_parts[6].iov_base = (char *) values;
    xcb_parts[6].iov_len = values_len * sizeof(xcb_render_fixed_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

void
xcb_render_animcursorelt_next (xcb_render_animcursorelt_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_animcursorelt_t);
}

xcb_generic_iterator_t
xcb_render_animcursorelt_end (xcb_render_animcursorelt_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_render_create_anim_cursor_sizeof (const void  *_buffer  /**< */,
                                      uint32_t     cursors_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_create_anim_cursor_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* cursors */
    xcb_block_len += cursors_len * sizeof(xcb_render_animcursorelt_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_animcursorelt_t);
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
xcb_render_create_anim_cursor_checked (xcb_connection_t                 *c  /**< */,
                                       xcb_cursor_t                      cid  /**< */,
                                       uint32_t                          cursors_len  /**< */,
                                       const xcb_render_animcursorelt_t *cursors  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_ANIM_CURSOR,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_anim_cursor_request_t xcb_out;

    xcb_out.cid = cid;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_animcursorelt_t cursors */
    xcb_parts[4].iov_base = (char *) cursors;
    xcb_parts[4].iov_len = cursors_len * sizeof(xcb_render_animcursorelt_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_create_anim_cursor (xcb_connection_t                 *c  /**< */,
                               xcb_cursor_t                      cid  /**< */,
                               uint32_t                          cursors_len  /**< */,
                               const xcb_render_animcursorelt_t *cursors  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_ANIM_CURSOR,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_anim_cursor_request_t xcb_out;

    xcb_out.cid = cid;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_animcursorelt_t cursors */
    xcb_parts[4].iov_base = (char *) cursors;
    xcb_parts[4].iov_len = cursors_len * sizeof(xcb_render_animcursorelt_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

void
xcb_render_spanfix_next (xcb_render_spanfix_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_spanfix_t);
}

xcb_generic_iterator_t
xcb_render_spanfix_end (xcb_render_spanfix_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_render_trap_next (xcb_render_trap_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_render_trap_t);
}

xcb_generic_iterator_t
xcb_render_trap_end (xcb_render_trap_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_render_add_traps_sizeof (const void  *_buffer  /**< */,
                             uint32_t     traps_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_add_traps_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* traps */
    xcb_block_len += traps_len * sizeof(xcb_render_trap_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_trap_t);
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
xcb_render_add_traps_checked (xcb_connection_t        *c  /**< */,
                              xcb_render_picture_t     picture  /**< */,
                              int16_t                  x_off  /**< */,
                              int16_t                  y_off  /**< */,
                              uint32_t                 traps_len  /**< */,
                              const xcb_render_trap_t *traps  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_ADD_TRAPS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_add_traps_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.x_off = x_off;
    xcb_out.y_off = y_off;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_trap_t traps */
    xcb_parts[4].iov_base = (char *) traps;
    xcb_parts[4].iov_len = traps_len * sizeof(xcb_render_trap_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_add_traps (xcb_connection_t        *c  /**< */,
                      xcb_render_picture_t     picture  /**< */,
                      int16_t                  x_off  /**< */,
                      int16_t                  y_off  /**< */,
                      uint32_t                 traps_len  /**< */,
                      const xcb_render_trap_t *traps  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_ADD_TRAPS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_render_add_traps_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.x_off = x_off;
    xcb_out.y_off = y_off;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_trap_t traps */
    xcb_parts[4].iov_base = (char *) traps;
    xcb_parts[4].iov_len = traps_len * sizeof(xcb_render_trap_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_create_solid_fill_checked (xcb_connection_t     *c  /**< */,
                                      xcb_render_picture_t  picture  /**< */,
                                      xcb_render_color_t    color  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_SOLID_FILL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_solid_fill_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.color = color;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_create_solid_fill (xcb_connection_t     *c  /**< */,
                              xcb_render_picture_t  picture  /**< */,
                              xcb_render_color_t    color  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_SOLID_FILL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_solid_fill_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.color = color;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_create_linear_gradient_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_render_create_linear_gradient_request_t *_aux = (xcb_render_create_linear_gradient_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_create_linear_gradient_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* stops */
    xcb_block_len += _aux->num_stops * sizeof(xcb_render_fixed_t);
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
    /* colors */
    xcb_block_len += _aux->num_stops * sizeof(xcb_render_color_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_color_t);
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
xcb_render_create_linear_gradient_checked (xcb_connection_t         *c  /**< */,
                                           xcb_render_picture_t      picture  /**< */,
                                           xcb_render_pointfix_t     p1  /**< */,
                                           xcb_render_pointfix_t     p2  /**< */,
                                           uint32_t                  num_stops  /**< */,
                                           const xcb_render_fixed_t *stops  /**< */,
                                           const xcb_render_color_t *colors  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_LINEAR_GRADIENT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[8];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_linear_gradient_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.p1 = p1;
    xcb_out.p2 = p2;
    xcb_out.num_stops = num_stops;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_fixed_t stops */
    xcb_parts[4].iov_base = (char *) stops;
    xcb_parts[4].iov_len = num_stops * sizeof(xcb_render_fixed_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_render_color_t colors */
    xcb_parts[6].iov_base = (char *) colors;
    xcb_parts[6].iov_len = num_stops * sizeof(xcb_render_color_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_create_linear_gradient (xcb_connection_t         *c  /**< */,
                                   xcb_render_picture_t      picture  /**< */,
                                   xcb_render_pointfix_t     p1  /**< */,
                                   xcb_render_pointfix_t     p2  /**< */,
                                   uint32_t                  num_stops  /**< */,
                                   const xcb_render_fixed_t *stops  /**< */,
                                   const xcb_render_color_t *colors  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_LINEAR_GRADIENT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[8];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_linear_gradient_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.p1 = p1;
    xcb_out.p2 = p2;
    xcb_out.num_stops = num_stops;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_fixed_t stops */
    xcb_parts[4].iov_base = (char *) stops;
    xcb_parts[4].iov_len = num_stops * sizeof(xcb_render_fixed_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_render_color_t colors */
    xcb_parts[6].iov_base = (char *) colors;
    xcb_parts[6].iov_len = num_stops * sizeof(xcb_render_color_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_create_radial_gradient_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_render_create_radial_gradient_request_t *_aux = (xcb_render_create_radial_gradient_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_create_radial_gradient_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* stops */
    xcb_block_len += _aux->num_stops * sizeof(xcb_render_fixed_t);
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
    /* colors */
    xcb_block_len += _aux->num_stops * sizeof(xcb_render_color_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_color_t);
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
xcb_render_create_radial_gradient_checked (xcb_connection_t         *c  /**< */,
                                           xcb_render_picture_t      picture  /**< */,
                                           xcb_render_pointfix_t     inner  /**< */,
                                           xcb_render_pointfix_t     outer  /**< */,
                                           xcb_render_fixed_t        inner_radius  /**< */,
                                           xcb_render_fixed_t        outer_radius  /**< */,
                                           uint32_t                  num_stops  /**< */,
                                           const xcb_render_fixed_t *stops  /**< */,
                                           const xcb_render_color_t *colors  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_RADIAL_GRADIENT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[8];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_radial_gradient_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.inner = inner;
    xcb_out.outer = outer;
    xcb_out.inner_radius = inner_radius;
    xcb_out.outer_radius = outer_radius;
    xcb_out.num_stops = num_stops;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_fixed_t stops */
    xcb_parts[4].iov_base = (char *) stops;
    xcb_parts[4].iov_len = num_stops * sizeof(xcb_render_fixed_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_render_color_t colors */
    xcb_parts[6].iov_base = (char *) colors;
    xcb_parts[6].iov_len = num_stops * sizeof(xcb_render_color_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_create_radial_gradient (xcb_connection_t         *c  /**< */,
                                   xcb_render_picture_t      picture  /**< */,
                                   xcb_render_pointfix_t     inner  /**< */,
                                   xcb_render_pointfix_t     outer  /**< */,
                                   xcb_render_fixed_t        inner_radius  /**< */,
                                   xcb_render_fixed_t        outer_radius  /**< */,
                                   uint32_t                  num_stops  /**< */,
                                   const xcb_render_fixed_t *stops  /**< */,
                                   const xcb_render_color_t *colors  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_RADIAL_GRADIENT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[8];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_radial_gradient_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.inner = inner;
    xcb_out.outer = outer;
    xcb_out.inner_radius = inner_radius;
    xcb_out.outer_radius = outer_radius;
    xcb_out.num_stops = num_stops;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_fixed_t stops */
    xcb_parts[4].iov_base = (char *) stops;
    xcb_parts[4].iov_len = num_stops * sizeof(xcb_render_fixed_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_render_color_t colors */
    xcb_parts[6].iov_base = (char *) colors;
    xcb_parts[6].iov_len = num_stops * sizeof(xcb_render_color_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_render_create_conical_gradient_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_render_create_conical_gradient_request_t *_aux = (xcb_render_create_conical_gradient_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_render_create_conical_gradient_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* stops */
    xcb_block_len += _aux->num_stops * sizeof(xcb_render_fixed_t);
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
    /* colors */
    xcb_block_len += _aux->num_stops * sizeof(xcb_render_color_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_render_color_t);
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
xcb_render_create_conical_gradient_checked (xcb_connection_t         *c  /**< */,
                                            xcb_render_picture_t      picture  /**< */,
                                            xcb_render_pointfix_t     center  /**< */,
                                            xcb_render_fixed_t        angle  /**< */,
                                            uint32_t                  num_stops  /**< */,
                                            const xcb_render_fixed_t *stops  /**< */,
                                            const xcb_render_color_t *colors  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_CONICAL_GRADIENT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[8];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_conical_gradient_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.center = center;
    xcb_out.angle = angle;
    xcb_out.num_stops = num_stops;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_fixed_t stops */
    xcb_parts[4].iov_base = (char *) stops;
    xcb_parts[4].iov_len = num_stops * sizeof(xcb_render_fixed_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_render_color_t colors */
    xcb_parts[6].iov_base = (char *) colors;
    xcb_parts[6].iov_len = num_stops * sizeof(xcb_render_color_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_render_create_conical_gradient (xcb_connection_t         *c  /**< */,
                                    xcb_render_picture_t      picture  /**< */,
                                    xcb_render_pointfix_t     center  /**< */,
                                    xcb_render_fixed_t        angle  /**< */,
                                    uint32_t                  num_stops  /**< */,
                                    const xcb_render_fixed_t *stops  /**< */,
                                    const xcb_render_color_t *colors  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_render_id,
        /* opcode */ XCB_RENDER_CREATE_CONICAL_GRADIENT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[8];
    xcb_void_cookie_t xcb_ret;
    xcb_render_create_conical_gradient_request_t xcb_out;

    xcb_out.picture = picture;
    xcb_out.center = center;
    xcb_out.angle = angle;
    xcb_out.num_stops = num_stops;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_render_fixed_t stops */
    xcb_parts[4].iov_base = (char *) stops;
    xcb_parts[4].iov_len = num_stops * sizeof(xcb_render_fixed_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_render_color_t colors */
    xcb_parts[6].iov_base = (char *) colors;
    xcb_parts[6].iov_len = num_stops * sizeof(xcb_render_color_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

