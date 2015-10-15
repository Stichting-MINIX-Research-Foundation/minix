/*
 * This file generated automatically from damage.xml by c_client.py.
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
#include "damage.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xproto.h"
#include "xfixes.h"

xcb_extension_t xcb_damage_id = { "DAMAGE", 0 };

void
xcb_damage_damage_next (xcb_damage_damage_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_damage_damage_t);
}

xcb_generic_iterator_t
xcb_damage_damage_end (xcb_damage_damage_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

xcb_damage_query_version_cookie_t
xcb_damage_query_version (xcb_connection_t *c  /**< */,
                          uint32_t          client_major_version  /**< */,
                          uint32_t          client_minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_damage_id,
        /* opcode */ XCB_DAMAGE_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_damage_query_version_cookie_t xcb_ret;
    xcb_damage_query_version_request_t xcb_out;

    xcb_out.client_major_version = client_major_version;
    xcb_out.client_minor_version = client_minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_damage_query_version_cookie_t
xcb_damage_query_version_unchecked (xcb_connection_t *c  /**< */,
                                    uint32_t          client_major_version  /**< */,
                                    uint32_t          client_minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_damage_id,
        /* opcode */ XCB_DAMAGE_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_damage_query_version_cookie_t xcb_ret;
    xcb_damage_query_version_request_t xcb_out;

    xcb_out.client_major_version = client_major_version;
    xcb_out.client_minor_version = client_minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_damage_query_version_reply_t *
xcb_damage_query_version_reply (xcb_connection_t                   *c  /**< */,
                                xcb_damage_query_version_cookie_t   cookie  /**< */,
                                xcb_generic_error_t               **e  /**< */)
{
    return (xcb_damage_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_damage_create_checked (xcb_connection_t    *c  /**< */,
                           xcb_damage_damage_t  damage  /**< */,
                           xcb_drawable_t       drawable  /**< */,
                           uint8_t              level  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_damage_id,
        /* opcode */ XCB_DAMAGE_CREATE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_damage_create_request_t xcb_out;

    xcb_out.damage = damage;
    xcb_out.drawable = drawable;
    xcb_out.level = level;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_damage_create (xcb_connection_t    *c  /**< */,
                   xcb_damage_damage_t  damage  /**< */,
                   xcb_drawable_t       drawable  /**< */,
                   uint8_t              level  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_damage_id,
        /* opcode */ XCB_DAMAGE_CREATE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_damage_create_request_t xcb_out;

    xcb_out.damage = damage;
    xcb_out.drawable = drawable;
    xcb_out.level = level;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_damage_destroy_checked (xcb_connection_t    *c  /**< */,
                            xcb_damage_damage_t  damage  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_damage_id,
        /* opcode */ XCB_DAMAGE_DESTROY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_damage_destroy_request_t xcb_out;

    xcb_out.damage = damage;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_damage_destroy (xcb_connection_t    *c  /**< */,
                    xcb_damage_damage_t  damage  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_damage_id,
        /* opcode */ XCB_DAMAGE_DESTROY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_damage_destroy_request_t xcb_out;

    xcb_out.damage = damage;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_damage_subtract_checked (xcb_connection_t    *c  /**< */,
                             xcb_damage_damage_t  damage  /**< */,
                             xcb_xfixes_region_t  repair  /**< */,
                             xcb_xfixes_region_t  parts  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_damage_id,
        /* opcode */ XCB_DAMAGE_SUBTRACT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_damage_subtract_request_t xcb_out;

    xcb_out.damage = damage;
    xcb_out.repair = repair;
    xcb_out.parts = parts;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_damage_subtract (xcb_connection_t    *c  /**< */,
                     xcb_damage_damage_t  damage  /**< */,
                     xcb_xfixes_region_t  repair  /**< */,
                     xcb_xfixes_region_t  parts  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_damage_id,
        /* opcode */ XCB_DAMAGE_SUBTRACT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_damage_subtract_request_t xcb_out;

    xcb_out.damage = damage;
    xcb_out.repair = repair;
    xcb_out.parts = parts;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_damage_add_checked (xcb_connection_t    *c  /**< */,
                        xcb_drawable_t       drawable  /**< */,
                        xcb_xfixes_region_t  region  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_damage_id,
        /* opcode */ XCB_DAMAGE_ADD,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_damage_add_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.region = region;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_damage_add (xcb_connection_t    *c  /**< */,
                xcb_drawable_t       drawable  /**< */,
                xcb_xfixes_region_t  region  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_damage_id,
        /* opcode */ XCB_DAMAGE_ADD,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_damage_add_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.region = region;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

