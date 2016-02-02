/*
 * This file generated automatically from xtest.xml by c_client.py.
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
#include "xtest.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xproto.h"

xcb_extension_t xcb_test_id = { "XTEST", 0 };

xcb_test_get_version_cookie_t
xcb_test_get_version (xcb_connection_t *c  /**< */,
                      uint8_t           major_version  /**< */,
                      uint16_t          minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_test_id,
        /* opcode */ XCB_TEST_GET_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_test_get_version_cookie_t xcb_ret;
    xcb_test_get_version_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.pad0 = 0;
    xcb_out.minor_version = minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_test_get_version_cookie_t
xcb_test_get_version_unchecked (xcb_connection_t *c  /**< */,
                                uint8_t           major_version  /**< */,
                                uint16_t          minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_test_id,
        /* opcode */ XCB_TEST_GET_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_test_get_version_cookie_t xcb_ret;
    xcb_test_get_version_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.pad0 = 0;
    xcb_out.minor_version = minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_test_get_version_reply_t *
xcb_test_get_version_reply (xcb_connection_t               *c  /**< */,
                            xcb_test_get_version_cookie_t   cookie  /**< */,
                            xcb_generic_error_t           **e  /**< */)
{
    return (xcb_test_get_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_test_compare_cursor_cookie_t
xcb_test_compare_cursor (xcb_connection_t *c  /**< */,
                         xcb_window_t      window  /**< */,
                         xcb_cursor_t      cursor  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_test_id,
        /* opcode */ XCB_TEST_COMPARE_CURSOR,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_test_compare_cursor_cookie_t xcb_ret;
    xcb_test_compare_cursor_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.cursor = cursor;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_test_compare_cursor_cookie_t
xcb_test_compare_cursor_unchecked (xcb_connection_t *c  /**< */,
                                   xcb_window_t      window  /**< */,
                                   xcb_cursor_t      cursor  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_test_id,
        /* opcode */ XCB_TEST_COMPARE_CURSOR,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_test_compare_cursor_cookie_t xcb_ret;
    xcb_test_compare_cursor_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.cursor = cursor;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_test_compare_cursor_reply_t *
xcb_test_compare_cursor_reply (xcb_connection_t                  *c  /**< */,
                               xcb_test_compare_cursor_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_test_compare_cursor_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_test_fake_input_checked (xcb_connection_t *c  /**< */,
                             uint8_t           type  /**< */,
                             uint8_t           detail  /**< */,
                             uint32_t          time  /**< */,
                             xcb_window_t      root  /**< */,
                             int16_t           rootX  /**< */,
                             int16_t           rootY  /**< */,
                             uint8_t           deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_test_id,
        /* opcode */ XCB_TEST_FAKE_INPUT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_test_fake_input_request_t xcb_out;

    xcb_out.type = type;
    xcb_out.detail = detail;
    memset(xcb_out.pad0, 0, 2);
    xcb_out.time = time;
    xcb_out.root = root;
    memset(xcb_out.pad1, 0, 8);
    xcb_out.rootX = rootX;
    xcb_out.rootY = rootY;
    memset(xcb_out.pad2, 0, 7);
    xcb_out.deviceid = deviceid;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_test_fake_input (xcb_connection_t *c  /**< */,
                     uint8_t           type  /**< */,
                     uint8_t           detail  /**< */,
                     uint32_t          time  /**< */,
                     xcb_window_t      root  /**< */,
                     int16_t           rootX  /**< */,
                     int16_t           rootY  /**< */,
                     uint8_t           deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_test_id,
        /* opcode */ XCB_TEST_FAKE_INPUT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_test_fake_input_request_t xcb_out;

    xcb_out.type = type;
    xcb_out.detail = detail;
    memset(xcb_out.pad0, 0, 2);
    xcb_out.time = time;
    xcb_out.root = root;
    memset(xcb_out.pad1, 0, 8);
    xcb_out.rootX = rootX;
    xcb_out.rootY = rootY;
    memset(xcb_out.pad2, 0, 7);
    xcb_out.deviceid = deviceid;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_test_grab_control_checked (xcb_connection_t *c  /**< */,
                               uint8_t           impervious  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_test_id,
        /* opcode */ XCB_TEST_GRAB_CONTROL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_test_grab_control_request_t xcb_out;

    xcb_out.impervious = impervious;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_test_grab_control (xcb_connection_t *c  /**< */,
                       uint8_t           impervious  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_test_id,
        /* opcode */ XCB_TEST_GRAB_CONTROL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_test_grab_control_request_t xcb_out;

    xcb_out.impervious = impervious;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

