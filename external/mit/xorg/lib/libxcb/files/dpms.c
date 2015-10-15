/*
 * This file generated automatically from dpms.xml by c_client.py.
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
#include "dpms.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)

xcb_extension_t xcb_dpms_id = { "DPMS", 0 };

xcb_dpms_get_version_cookie_t
xcb_dpms_get_version (xcb_connection_t *c  /**< */,
                      uint16_t          client_major_version  /**< */,
                      uint16_t          client_minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_GET_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dpms_get_version_cookie_t xcb_ret;
    xcb_dpms_get_version_request_t xcb_out;

    xcb_out.client_major_version = client_major_version;
    xcb_out.client_minor_version = client_minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dpms_get_version_cookie_t
xcb_dpms_get_version_unchecked (xcb_connection_t *c  /**< */,
                                uint16_t          client_major_version  /**< */,
                                uint16_t          client_minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_GET_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dpms_get_version_cookie_t xcb_ret;
    xcb_dpms_get_version_request_t xcb_out;

    xcb_out.client_major_version = client_major_version;
    xcb_out.client_minor_version = client_minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dpms_get_version_reply_t *
xcb_dpms_get_version_reply (xcb_connection_t               *c  /**< */,
                            xcb_dpms_get_version_cookie_t   cookie  /**< */,
                            xcb_generic_error_t           **e  /**< */)
{
    return (xcb_dpms_get_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_dpms_capable_cookie_t
xcb_dpms_capable (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_CAPABLE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dpms_capable_cookie_t xcb_ret;
    xcb_dpms_capable_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dpms_capable_cookie_t
xcb_dpms_capable_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_CAPABLE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dpms_capable_cookie_t xcb_ret;
    xcb_dpms_capable_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dpms_capable_reply_t *
xcb_dpms_capable_reply (xcb_connection_t           *c  /**< */,
                        xcb_dpms_capable_cookie_t   cookie  /**< */,
                        xcb_generic_error_t       **e  /**< */)
{
    return (xcb_dpms_capable_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_dpms_get_timeouts_cookie_t
xcb_dpms_get_timeouts (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_GET_TIMEOUTS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dpms_get_timeouts_cookie_t xcb_ret;
    xcb_dpms_get_timeouts_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dpms_get_timeouts_cookie_t
xcb_dpms_get_timeouts_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_GET_TIMEOUTS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dpms_get_timeouts_cookie_t xcb_ret;
    xcb_dpms_get_timeouts_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dpms_get_timeouts_reply_t *
xcb_dpms_get_timeouts_reply (xcb_connection_t                *c  /**< */,
                             xcb_dpms_get_timeouts_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_dpms_get_timeouts_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_dpms_set_timeouts_checked (xcb_connection_t *c  /**< */,
                               uint16_t          standby_timeout  /**< */,
                               uint16_t          suspend_timeout  /**< */,
                               uint16_t          off_timeout  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_SET_TIMEOUTS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_dpms_set_timeouts_request_t xcb_out;

    xcb_out.standby_timeout = standby_timeout;
    xcb_out.suspend_timeout = suspend_timeout;
    xcb_out.off_timeout = off_timeout;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_dpms_set_timeouts (xcb_connection_t *c  /**< */,
                       uint16_t          standby_timeout  /**< */,
                       uint16_t          suspend_timeout  /**< */,
                       uint16_t          off_timeout  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_SET_TIMEOUTS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_dpms_set_timeouts_request_t xcb_out;

    xcb_out.standby_timeout = standby_timeout;
    xcb_out.suspend_timeout = suspend_timeout;
    xcb_out.off_timeout = off_timeout;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_dpms_enable_checked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_ENABLE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_dpms_enable_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_dpms_enable (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_ENABLE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_dpms_enable_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_dpms_disable_checked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_DISABLE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_dpms_disable_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_dpms_disable (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_DISABLE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_dpms_disable_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_dpms_force_level_checked (xcb_connection_t *c  /**< */,
                              uint16_t          power_level  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_FORCE_LEVEL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_dpms_force_level_request_t xcb_out;

    xcb_out.power_level = power_level;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_dpms_force_level (xcb_connection_t *c  /**< */,
                      uint16_t          power_level  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_FORCE_LEVEL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_dpms_force_level_request_t xcb_out;

    xcb_out.power_level = power_level;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dpms_info_cookie_t
xcb_dpms_info (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_INFO,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dpms_info_cookie_t xcb_ret;
    xcb_dpms_info_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dpms_info_cookie_t
xcb_dpms_info_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_dpms_id,
        /* opcode */ XCB_DPMS_INFO,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_dpms_info_cookie_t xcb_ret;
    xcb_dpms_info_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_dpms_info_reply_t *
xcb_dpms_info_reply (xcb_connection_t        *c  /**< */,
                     xcb_dpms_info_cookie_t   cookie  /**< */,
                     xcb_generic_error_t    **e  /**< */)
{
    return (xcb_dpms_info_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

