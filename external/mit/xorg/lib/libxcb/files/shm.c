/*
 * This file generated automatically from shm.xml by c_client.py.
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
#include "shm.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xproto.h"

xcb_extension_t xcb_shm_id = { "MIT-SHM", 0 };

void
xcb_shm_seg_next (xcb_shm_seg_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_shm_seg_t);
}

xcb_generic_iterator_t
xcb_shm_seg_end (xcb_shm_seg_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

xcb_shm_query_version_cookie_t
xcb_shm_query_version (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_shm_query_version_cookie_t xcb_ret;
    xcb_shm_query_version_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shm_query_version_cookie_t
xcb_shm_query_version_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_shm_query_version_cookie_t xcb_ret;
    xcb_shm_query_version_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shm_query_version_reply_t *
xcb_shm_query_version_reply (xcb_connection_t                *c  /**< */,
                             xcb_shm_query_version_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_shm_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_shm_attach_checked (xcb_connection_t *c  /**< */,
                        xcb_shm_seg_t     shmseg  /**< */,
                        uint32_t          shmid  /**< */,
                        uint8_t           read_only  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_ATTACH,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shm_attach_request_t xcb_out;

    xcb_out.shmseg = shmseg;
    xcb_out.shmid = shmid;
    xcb_out.read_only = read_only;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shm_attach (xcb_connection_t *c  /**< */,
                xcb_shm_seg_t     shmseg  /**< */,
                uint32_t          shmid  /**< */,
                uint8_t           read_only  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_ATTACH,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shm_attach_request_t xcb_out;

    xcb_out.shmseg = shmseg;
    xcb_out.shmid = shmid;
    xcb_out.read_only = read_only;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shm_detach_checked (xcb_connection_t *c  /**< */,
                        xcb_shm_seg_t     shmseg  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_DETACH,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shm_detach_request_t xcb_out;

    xcb_out.shmseg = shmseg;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shm_detach (xcb_connection_t *c  /**< */,
                xcb_shm_seg_t     shmseg  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_DETACH,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shm_detach_request_t xcb_out;

    xcb_out.shmseg = shmseg;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shm_put_image_checked (xcb_connection_t *c  /**< */,
                           xcb_drawable_t    drawable  /**< */,
                           xcb_gcontext_t    gc  /**< */,
                           uint16_t          total_width  /**< */,
                           uint16_t          total_height  /**< */,
                           uint16_t          src_x  /**< */,
                           uint16_t          src_y  /**< */,
                           uint16_t          src_width  /**< */,
                           uint16_t          src_height  /**< */,
                           int16_t           dst_x  /**< */,
                           int16_t           dst_y  /**< */,
                           uint8_t           depth  /**< */,
                           uint8_t           format  /**< */,
                           uint8_t           send_event  /**< */,
                           xcb_shm_seg_t     shmseg  /**< */,
                           uint32_t          offset  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_PUT_IMAGE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shm_put_image_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.total_width = total_width;
    xcb_out.total_height = total_height;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.src_width = src_width;
    xcb_out.src_height = src_height;
    xcb_out.dst_x = dst_x;
    xcb_out.dst_y = dst_y;
    xcb_out.depth = depth;
    xcb_out.format = format;
    xcb_out.send_event = send_event;
    xcb_out.pad0 = 0;
    xcb_out.shmseg = shmseg;
    xcb_out.offset = offset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shm_put_image (xcb_connection_t *c  /**< */,
                   xcb_drawable_t    drawable  /**< */,
                   xcb_gcontext_t    gc  /**< */,
                   uint16_t          total_width  /**< */,
                   uint16_t          total_height  /**< */,
                   uint16_t          src_x  /**< */,
                   uint16_t          src_y  /**< */,
                   uint16_t          src_width  /**< */,
                   uint16_t          src_height  /**< */,
                   int16_t           dst_x  /**< */,
                   int16_t           dst_y  /**< */,
                   uint8_t           depth  /**< */,
                   uint8_t           format  /**< */,
                   uint8_t           send_event  /**< */,
                   xcb_shm_seg_t     shmseg  /**< */,
                   uint32_t          offset  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_PUT_IMAGE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shm_put_image_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.gc = gc;
    xcb_out.total_width = total_width;
    xcb_out.total_height = total_height;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.src_width = src_width;
    xcb_out.src_height = src_height;
    xcb_out.dst_x = dst_x;
    xcb_out.dst_y = dst_y;
    xcb_out.depth = depth;
    xcb_out.format = format;
    xcb_out.send_event = send_event;
    xcb_out.pad0 = 0;
    xcb_out.shmseg = shmseg;
    xcb_out.offset = offset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shm_get_image_cookie_t
xcb_shm_get_image (xcb_connection_t *c  /**< */,
                   xcb_drawable_t    drawable  /**< */,
                   int16_t           x  /**< */,
                   int16_t           y  /**< */,
                   uint16_t          width  /**< */,
                   uint16_t          height  /**< */,
                   uint32_t          plane_mask  /**< */,
                   uint8_t           format  /**< */,
                   xcb_shm_seg_t     shmseg  /**< */,
                   uint32_t          offset  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_GET_IMAGE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_shm_get_image_cookie_t xcb_ret;
    xcb_shm_get_image_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.x = x;
    xcb_out.y = y;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.plane_mask = plane_mask;
    xcb_out.format = format;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.shmseg = shmseg;
    xcb_out.offset = offset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shm_get_image_cookie_t
xcb_shm_get_image_unchecked (xcb_connection_t *c  /**< */,
                             xcb_drawable_t    drawable  /**< */,
                             int16_t           x  /**< */,
                             int16_t           y  /**< */,
                             uint16_t          width  /**< */,
                             uint16_t          height  /**< */,
                             uint32_t          plane_mask  /**< */,
                             uint8_t           format  /**< */,
                             xcb_shm_seg_t     shmseg  /**< */,
                             uint32_t          offset  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_GET_IMAGE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_shm_get_image_cookie_t xcb_ret;
    xcb_shm_get_image_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.x = x;
    xcb_out.y = y;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.plane_mask = plane_mask;
    xcb_out.format = format;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.shmseg = shmseg;
    xcb_out.offset = offset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shm_get_image_reply_t *
xcb_shm_get_image_reply (xcb_connection_t            *c  /**< */,
                         xcb_shm_get_image_cookie_t   cookie  /**< */,
                         xcb_generic_error_t        **e  /**< */)
{
    return (xcb_shm_get_image_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_shm_create_pixmap_checked (xcb_connection_t *c  /**< */,
                               xcb_pixmap_t      pid  /**< */,
                               xcb_drawable_t    drawable  /**< */,
                               uint16_t          width  /**< */,
                               uint16_t          height  /**< */,
                               uint8_t           depth  /**< */,
                               xcb_shm_seg_t     shmseg  /**< */,
                               uint32_t          offset  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_CREATE_PIXMAP,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shm_create_pixmap_request_t xcb_out;

    xcb_out.pid = pid;
    xcb_out.drawable = drawable;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.depth = depth;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.shmseg = shmseg;
    xcb_out.offset = offset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shm_create_pixmap (xcb_connection_t *c  /**< */,
                       xcb_pixmap_t      pid  /**< */,
                       xcb_drawable_t    drawable  /**< */,
                       uint16_t          width  /**< */,
                       uint16_t          height  /**< */,
                       uint8_t           depth  /**< */,
                       xcb_shm_seg_t     shmseg  /**< */,
                       uint32_t          offset  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_CREATE_PIXMAP,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shm_create_pixmap_request_t xcb_out;

    xcb_out.pid = pid;
    xcb_out.drawable = drawable;
    xcb_out.width = width;
    xcb_out.height = height;
    xcb_out.depth = depth;
    memset(xcb_out.pad0, 0, 3);
    xcb_out.shmseg = shmseg;
    xcb_out.offset = offset;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shm_attach_fd_checked (xcb_connection_t *c  /**< */,
                           xcb_shm_seg_t     shmseg  /**< */,
                           int32_t           shm_fd  /**< */,
                           uint8_t           read_only  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_ATTACH_FD,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shm_attach_fd_request_t xcb_out;

    xcb_out.shmseg = shmseg;
    xcb_out.read_only = read_only;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_send_fd(c, shm_fd);
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_shm_attach_fd (xcb_connection_t *c  /**< */,
                   xcb_shm_seg_t     shmseg  /**< */,
                   int32_t           shm_fd  /**< */,
                   uint8_t           read_only  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_ATTACH_FD,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_shm_attach_fd_request_t xcb_out;

    xcb_out.shmseg = shmseg;
    xcb_out.read_only = read_only;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_send_fd(c, shm_fd);
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shm_create_segment_cookie_t
xcb_shm_create_segment (xcb_connection_t *c  /**< */,
                        xcb_shm_seg_t     shmseg  /**< */,
                        uint32_t          size  /**< */,
                        uint8_t           read_only  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_CREATE_SEGMENT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_shm_create_segment_cookie_t xcb_ret;
    xcb_shm_create_segment_request_t xcb_out;

    xcb_out.shmseg = shmseg;
    xcb_out.size = size;
    xcb_out.read_only = read_only;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED|XCB_REQUEST_REPLY_FDS, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shm_create_segment_cookie_t
xcb_shm_create_segment_unchecked (xcb_connection_t *c  /**< */,
                                  xcb_shm_seg_t     shmseg  /**< */,
                                  uint32_t          size  /**< */,
                                  uint8_t           read_only  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_shm_id,
        /* opcode */ XCB_SHM_CREATE_SEGMENT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_shm_create_segment_cookie_t xcb_ret;
    xcb_shm_create_segment_request_t xcb_out;

    xcb_out.shmseg = shmseg;
    xcb_out.size = size;
    xcb_out.read_only = read_only;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_REPLY_FDS, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_shm_create_segment_reply_t *
xcb_shm_create_segment_reply (xcb_connection_t                 *c  /**< */,
                              xcb_shm_create_segment_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_shm_create_segment_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int *
xcb_shm_create_segment_reply_fds (xcb_connection_t                *c  /**< */,
                                  xcb_shm_create_segment_reply_t  *reply  /**< */)
{
    return xcb_get_reply_fds(c, reply, sizeof(xcb_shm_create_segment_reply_t) + 4 * reply->length);
}

