/*
 * This file generated automatically from xevie.xml by c_client.py.
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
#include "xevie.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)

xcb_extension_t xcb_xevie_id = { "XEVIE", 0 };

xcb_xevie_query_version_cookie_t
xcb_xevie_query_version (xcb_connection_t *c  /**< */,
                         uint16_t          client_major_version  /**< */,
                         uint16_t          client_minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xevie_id,
        /* opcode */ XCB_XEVIE_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xevie_query_version_cookie_t xcb_ret;
    xcb_xevie_query_version_request_t xcb_out;

    xcb_out.client_major_version = client_major_version;
    xcb_out.client_minor_version = client_minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xevie_query_version_cookie_t
xcb_xevie_query_version_unchecked (xcb_connection_t *c  /**< */,
                                   uint16_t          client_major_version  /**< */,
                                   uint16_t          client_minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xevie_id,
        /* opcode */ XCB_XEVIE_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xevie_query_version_cookie_t xcb_ret;
    xcb_xevie_query_version_request_t xcb_out;

    xcb_out.client_major_version = client_major_version;
    xcb_out.client_minor_version = client_minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xevie_query_version_reply_t *
xcb_xevie_query_version_reply (xcb_connection_t                  *c  /**< */,
                               xcb_xevie_query_version_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_xevie_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_xevie_start_cookie_t
xcb_xevie_start (xcb_connection_t *c  /**< */,
                 uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xevie_id,
        /* opcode */ XCB_XEVIE_START,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xevie_start_cookie_t xcb_ret;
    xcb_xevie_start_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xevie_start_cookie_t
xcb_xevie_start_unchecked (xcb_connection_t *c  /**< */,
                           uint32_t          screen  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xevie_id,
        /* opcode */ XCB_XEVIE_START,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xevie_start_cookie_t xcb_ret;
    xcb_xevie_start_request_t xcb_out;

    xcb_out.screen = screen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xevie_start_reply_t *
xcb_xevie_start_reply (xcb_connection_t          *c  /**< */,
                       xcb_xevie_start_cookie_t   cookie  /**< */,
                       xcb_generic_error_t      **e  /**< */)
{
    return (xcb_xevie_start_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_xevie_end_cookie_t
xcb_xevie_end (xcb_connection_t *c  /**< */,
               uint32_t          cmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xevie_id,
        /* opcode */ XCB_XEVIE_END,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xevie_end_cookie_t xcb_ret;
    xcb_xevie_end_request_t xcb_out;

    xcb_out.cmap = cmap;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xevie_end_cookie_t
xcb_xevie_end_unchecked (xcb_connection_t *c  /**< */,
                         uint32_t          cmap  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xevie_id,
        /* opcode */ XCB_XEVIE_END,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xevie_end_cookie_t xcb_ret;
    xcb_xevie_end_request_t xcb_out;

    xcb_out.cmap = cmap;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xevie_end_reply_t *
xcb_xevie_end_reply (xcb_connection_t        *c  /**< */,
                     xcb_xevie_end_cookie_t   cookie  /**< */,
                     xcb_generic_error_t    **e  /**< */)
{
    return (xcb_xevie_end_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

void
xcb_xevie_event_next (xcb_xevie_event_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_xevie_event_t);
}

xcb_generic_iterator_t
xcb_xevie_event_end (xcb_xevie_event_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

xcb_xevie_send_cookie_t
xcb_xevie_send (xcb_connection_t  *c  /**< */,
                xcb_xevie_event_t  event  /**< */,
                uint32_t           data_type  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xevie_id,
        /* opcode */ XCB_XEVIE_SEND,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xevie_send_cookie_t xcb_ret;
    xcb_xevie_send_request_t xcb_out;

    xcb_out.event = event;
    xcb_out.data_type = data_type;
    memset(xcb_out.pad0, 0, 64);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xevie_send_cookie_t
xcb_xevie_send_unchecked (xcb_connection_t  *c  /**< */,
                          xcb_xevie_event_t  event  /**< */,
                          uint32_t           data_type  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xevie_id,
        /* opcode */ XCB_XEVIE_SEND,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xevie_send_cookie_t xcb_ret;
    xcb_xevie_send_request_t xcb_out;

    xcb_out.event = event;
    xcb_out.data_type = data_type;
    memset(xcb_out.pad0, 0, 64);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xevie_send_reply_t *
xcb_xevie_send_reply (xcb_connection_t         *c  /**< */,
                      xcb_xevie_send_cookie_t   cookie  /**< */,
                      xcb_generic_error_t     **e  /**< */)
{
    return (xcb_xevie_send_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_xevie_select_input_cookie_t
xcb_xevie_select_input (xcb_connection_t *c  /**< */,
                        uint32_t          event_mask  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xevie_id,
        /* opcode */ XCB_XEVIE_SELECT_INPUT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xevie_select_input_cookie_t xcb_ret;
    xcb_xevie_select_input_request_t xcb_out;

    xcb_out.event_mask = event_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xevie_select_input_cookie_t
xcb_xevie_select_input_unchecked (xcb_connection_t *c  /**< */,
                                  uint32_t          event_mask  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_xevie_id,
        /* opcode */ XCB_XEVIE_SELECT_INPUT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_xevie_select_input_cookie_t xcb_ret;
    xcb_xevie_select_input_request_t xcb_out;

    xcb_out.event_mask = event_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_xevie_select_input_reply_t *
xcb_xevie_select_input_reply (xcb_connection_t                 *c  /**< */,
                              xcb_xevie_select_input_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_xevie_select_input_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

