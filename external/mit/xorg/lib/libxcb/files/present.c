/*
 * This file generated automatically from present.xml by c_client.py.
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
#include "present.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xproto.h"
#include "randr.h"
#include "xfixes.h"
#include "sync.h"

xcb_extension_t xcb_present_id = { "Present", 0 };

void
xcb_present_notify_next (xcb_present_notify_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_present_notify_t);
}

xcb_generic_iterator_t
xcb_present_notify_end (xcb_present_notify_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

xcb_present_query_version_cookie_t
xcb_present_query_version (xcb_connection_t *c  /**< */,
                           uint32_t          major_version  /**< */,
                           uint32_t          minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_present_id,
        /* opcode */ XCB_PRESENT_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_present_query_version_cookie_t xcb_ret;
    xcb_present_query_version_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_present_query_version_cookie_t
xcb_present_query_version_unchecked (xcb_connection_t *c  /**< */,
                                     uint32_t          major_version  /**< */,
                                     uint32_t          minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_present_id,
        /* opcode */ XCB_PRESENT_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_present_query_version_cookie_t xcb_ret;
    xcb_present_query_version_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_present_query_version_reply_t *
xcb_present_query_version_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_present_query_version_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */)
{
    return (xcb_present_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_present_pixmap_sizeof (const void  *_buffer  /**< */,
                           uint32_t     notifies_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_present_pixmap_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* notifies */
    xcb_block_len += notifies_len * sizeof(xcb_present_notify_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_present_notify_t);
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
xcb_present_pixmap_checked (xcb_connection_t           *c  /**< */,
                            xcb_window_t                window  /**< */,
                            xcb_pixmap_t                pixmap  /**< */,
                            uint32_t                    serial  /**< */,
                            xcb_xfixes_region_t         valid  /**< */,
                            xcb_xfixes_region_t         update  /**< */,
                            int16_t                     x_off  /**< */,
                            int16_t                     y_off  /**< */,
                            xcb_randr_crtc_t            target_crtc  /**< */,
                            xcb_sync_fence_t            wait_fence  /**< */,
                            xcb_sync_fence_t            idle_fence  /**< */,
                            uint32_t                    options  /**< */,
                            uint64_t                    target_msc  /**< */,
                            uint64_t                    divisor  /**< */,
                            uint64_t                    remainder  /**< */,
                            uint32_t                    notifies_len  /**< */,
                            const xcb_present_notify_t *notifies  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_present_id,
        /* opcode */ XCB_PRESENT_PIXMAP,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_present_pixmap_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.pixmap = pixmap;
    xcb_out.serial = serial;
    xcb_out.valid = valid;
    xcb_out.update = update;
    xcb_out.x_off = x_off;
    xcb_out.y_off = y_off;
    xcb_out.target_crtc = target_crtc;
    xcb_out.wait_fence = wait_fence;
    xcb_out.idle_fence = idle_fence;
    xcb_out.options = options;
    memset(xcb_out.pad0, 0, 4);
    xcb_out.target_msc = target_msc;
    xcb_out.divisor = divisor;
    xcb_out.remainder = remainder;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_present_notify_t notifies */
    xcb_parts[4].iov_base = (char *) notifies;
    xcb_parts[4].iov_len = notifies_len * sizeof(xcb_present_notify_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_present_pixmap (xcb_connection_t           *c  /**< */,
                    xcb_window_t                window  /**< */,
                    xcb_pixmap_t                pixmap  /**< */,
                    uint32_t                    serial  /**< */,
                    xcb_xfixes_region_t         valid  /**< */,
                    xcb_xfixes_region_t         update  /**< */,
                    int16_t                     x_off  /**< */,
                    int16_t                     y_off  /**< */,
                    xcb_randr_crtc_t            target_crtc  /**< */,
                    xcb_sync_fence_t            wait_fence  /**< */,
                    xcb_sync_fence_t            idle_fence  /**< */,
                    uint32_t                    options  /**< */,
                    uint64_t                    target_msc  /**< */,
                    uint64_t                    divisor  /**< */,
                    uint64_t                    remainder  /**< */,
                    uint32_t                    notifies_len  /**< */,
                    const xcb_present_notify_t *notifies  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_present_id,
        /* opcode */ XCB_PRESENT_PIXMAP,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_present_pixmap_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.pixmap = pixmap;
    xcb_out.serial = serial;
    xcb_out.valid = valid;
    xcb_out.update = update;
    xcb_out.x_off = x_off;
    xcb_out.y_off = y_off;
    xcb_out.target_crtc = target_crtc;
    xcb_out.wait_fence = wait_fence;
    xcb_out.idle_fence = idle_fence;
    xcb_out.options = options;
    memset(xcb_out.pad0, 0, 4);
    xcb_out.target_msc = target_msc;
    xcb_out.divisor = divisor;
    xcb_out.remainder = remainder;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_present_notify_t notifies */
    xcb_parts[4].iov_base = (char *) notifies;
    xcb_parts[4].iov_len = notifies_len * sizeof(xcb_present_notify_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_present_notify_msc_checked (xcb_connection_t *c  /**< */,
                                xcb_window_t      window  /**< */,
                                uint32_t          serial  /**< */,
                                uint64_t          target_msc  /**< */,
                                uint64_t          divisor  /**< */,
                                uint64_t          remainder  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_present_id,
        /* opcode */ XCB_PRESENT_NOTIFY_MSC,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_present_notify_msc_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.serial = serial;
    memset(xcb_out.pad0, 0, 4);
    xcb_out.target_msc = target_msc;
    xcb_out.divisor = divisor;
    xcb_out.remainder = remainder;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_present_notify_msc (xcb_connection_t *c  /**< */,
                        xcb_window_t      window  /**< */,
                        uint32_t          serial  /**< */,
                        uint64_t          target_msc  /**< */,
                        uint64_t          divisor  /**< */,
                        uint64_t          remainder  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_present_id,
        /* opcode */ XCB_PRESENT_NOTIFY_MSC,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_present_notify_msc_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.serial = serial;
    memset(xcb_out.pad0, 0, 4);
    xcb_out.target_msc = target_msc;
    xcb_out.divisor = divisor;
    xcb_out.remainder = remainder;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

void
xcb_present_event_next (xcb_present_event_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_present_event_t);
}

xcb_generic_iterator_t
xcb_present_event_end (xcb_present_event_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

xcb_void_cookie_t
xcb_present_select_input_checked (xcb_connection_t    *c  /**< */,
                                  xcb_present_event_t  eid  /**< */,
                                  xcb_window_t         window  /**< */,
                                  uint32_t             event_mask  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_present_id,
        /* opcode */ XCB_PRESENT_SELECT_INPUT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_present_select_input_request_t xcb_out;

    xcb_out.eid = eid;
    xcb_out.window = window;
    xcb_out.event_mask = event_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_present_select_input (xcb_connection_t    *c  /**< */,
                          xcb_present_event_t  eid  /**< */,
                          xcb_window_t         window  /**< */,
                          uint32_t             event_mask  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_present_id,
        /* opcode */ XCB_PRESENT_SELECT_INPUT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_present_select_input_request_t xcb_out;

    xcb_out.eid = eid;
    xcb_out.window = window;
    xcb_out.event_mask = event_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_present_query_capabilities_cookie_t
xcb_present_query_capabilities (xcb_connection_t *c  /**< */,
                                uint32_t          target  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_present_id,
        /* opcode */ XCB_PRESENT_QUERY_CAPABILITIES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_present_query_capabilities_cookie_t xcb_ret;
    xcb_present_query_capabilities_request_t xcb_out;

    xcb_out.target = target;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_present_query_capabilities_cookie_t
xcb_present_query_capabilities_unchecked (xcb_connection_t *c  /**< */,
                                          uint32_t          target  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_present_id,
        /* opcode */ XCB_PRESENT_QUERY_CAPABILITIES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_present_query_capabilities_cookie_t xcb_ret;
    xcb_present_query_capabilities_request_t xcb_out;

    xcb_out.target = target;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_present_query_capabilities_reply_t *
xcb_present_query_capabilities_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_present_query_capabilities_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */)
{
    return (xcb_present_query_capabilities_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_present_redirect_notify_sizeof (const void  *_buffer  /**< */,
                                    uint32_t     notifies_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_present_redirect_notify_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* notifies */
    xcb_block_len += notifies_len * sizeof(xcb_present_notify_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_present_notify_t);
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

