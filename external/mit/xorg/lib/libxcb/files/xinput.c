/*
 * This file generated automatically from xinput.xml by c_client.py.
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
#include "xinput.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xfixes.h"

xcb_extension_t xcb_input_id = { "XInputExtension", 0 };

void
xcb_input_event_class_next (xcb_input_event_class_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_event_class_t);
}

xcb_generic_iterator_t
xcb_input_event_class_end (xcb_input_event_class_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_key_code_next (xcb_input_key_code_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_key_code_t);
}

xcb_generic_iterator_t
xcb_input_key_code_end (xcb_input_key_code_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_device_id_next (xcb_input_device_id_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_id_t);
}

xcb_generic_iterator_t
xcb_input_device_id_end (xcb_input_device_id_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_fp1616_next (xcb_input_fp1616_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_fp1616_t);
}

xcb_generic_iterator_t
xcb_input_fp1616_end (xcb_input_fp1616_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_fp3232_next (xcb_input_fp3232_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_fp3232_t);
}

xcb_generic_iterator_t
xcb_input_fp3232_end (xcb_input_fp3232_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_get_extension_version_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_get_extension_version_request_t *_aux = (xcb_input_get_extension_version_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_get_extension_version_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
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

xcb_input_get_extension_version_cookie_t
xcb_input_get_extension_version (xcb_connection_t *c  /**< */,
                                 uint16_t          name_len  /**< */,
                                 const char       *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_EXTENSION_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_input_get_extension_version_cookie_t xcb_ret;
    xcb_input_get_extension_version_request_t xcb_out;

    xcb_out.name_len = name_len;
    memset(xcb_out.pad0, 0, 2);

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

xcb_input_get_extension_version_cookie_t
xcb_input_get_extension_version_unchecked (xcb_connection_t *c  /**< */,
                                           uint16_t          name_len  /**< */,
                                           const char       *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_EXTENSION_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_input_get_extension_version_cookie_t xcb_ret;
    xcb_input_get_extension_version_request_t xcb_out;

    xcb_out.name_len = name_len;
    memset(xcb_out.pad0, 0, 2);

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

xcb_input_get_extension_version_reply_t *
xcb_input_get_extension_version_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_input_get_extension_version_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */)
{
    return (xcb_input_get_extension_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

void
xcb_input_device_info_next (xcb_input_device_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_info_t);
}

xcb_generic_iterator_t
xcb_input_device_info_end (xcb_input_device_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_key_info_next (xcb_input_key_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_key_info_t);
}

xcb_generic_iterator_t
xcb_input_key_info_end (xcb_input_key_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_button_info_next (xcb_input_button_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_button_info_t);
}

xcb_generic_iterator_t
xcb_input_button_info_end (xcb_input_button_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_axis_info_next (xcb_input_axis_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_axis_info_t);
}

xcb_generic_iterator_t
xcb_input_axis_info_end (xcb_input_axis_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_valuator_info_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_valuator_info_t *_aux = (xcb_input_valuator_info_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_valuator_info_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* axes */
    xcb_block_len += _aux->axes_len * sizeof(xcb_input_axis_info_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_axis_info_t);
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

xcb_input_axis_info_t *
xcb_input_valuator_info_axes (const xcb_input_valuator_info_t *R  /**< */)
{
    return (xcb_input_axis_info_t *) (R + 1);
}

int
xcb_input_valuator_info_axes_length (const xcb_input_valuator_info_t *R  /**< */)
{
    return R->axes_len;
}

xcb_input_axis_info_iterator_t
xcb_input_valuator_info_axes_iterator (const xcb_input_valuator_info_t *R  /**< */)
{
    xcb_input_axis_info_iterator_t i;
    i.data = (xcb_input_axis_info_t *) (R + 1);
    i.rem = R->axes_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_valuator_info_next (xcb_input_valuator_info_iterator_t *i  /**< */)
{
    xcb_input_valuator_info_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_valuator_info_t *)(((char *)R) + xcb_input_valuator_info_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_valuator_info_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_valuator_info_end (xcb_input_valuator_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_valuator_info_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

void
xcb_input_input_info_next (xcb_input_input_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_input_info_t);
}

xcb_generic_iterator_t
xcb_input_input_info_end (xcb_input_input_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_device_name_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_device_name_t *_aux = (xcb_input_device_name_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_device_name_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* string */
    xcb_block_len += _aux->len * sizeof(char);
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
xcb_input_device_name_string (const xcb_input_device_name_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_input_device_name_string_length (const xcb_input_device_name_t *R  /**< */)
{
    return R->len;
}

xcb_generic_iterator_t
xcb_input_device_name_string_end (const xcb_input_device_name_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_device_name_next (xcb_input_device_name_iterator_t *i  /**< */)
{
    xcb_input_device_name_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_device_name_t *)(((char *)R) + xcb_input_device_name_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_device_name_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_device_name_end (xcb_input_device_name_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_device_name_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_input_list_input_devices_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_list_input_devices_reply_t *_aux = (xcb_input_list_input_devices_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_list_input_devices_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* devices */
    xcb_block_len += _aux->devices_len * sizeof(xcb_input_device_info_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_device_info_t);
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

xcb_input_list_input_devices_cookie_t
xcb_input_list_input_devices (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_LIST_INPUT_DEVICES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_list_input_devices_cookie_t xcb_ret;
    xcb_input_list_input_devices_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_list_input_devices_cookie_t
xcb_input_list_input_devices_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_LIST_INPUT_DEVICES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_list_input_devices_cookie_t xcb_ret;
    xcb_input_list_input_devices_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_device_info_t *
xcb_input_list_input_devices_devices (const xcb_input_list_input_devices_reply_t *R  /**< */)
{
    return (xcb_input_device_info_t *) (R + 1);
}

int
xcb_input_list_input_devices_devices_length (const xcb_input_list_input_devices_reply_t *R  /**< */)
{
    return R->devices_len;
}

xcb_input_device_info_iterator_t
xcb_input_list_input_devices_devices_iterator (const xcb_input_list_input_devices_reply_t *R  /**< */)
{
    xcb_input_device_info_iterator_t i;
    i.data = (xcb_input_device_info_t *) (R + 1);
    i.rem = R->devices_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_list_input_devices_reply_t *
xcb_input_list_input_devices_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_input_list_input_devices_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */)
{
    return (xcb_input_list_input_devices_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

void
xcb_input_input_class_info_next (xcb_input_input_class_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_input_class_info_t);
}

xcb_generic_iterator_t
xcb_input_input_class_info_end (xcb_input_input_class_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_open_device_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_open_device_reply_t *_aux = (xcb_input_open_device_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_open_device_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* class_info */
    xcb_block_len += _aux->num_classes * sizeof(xcb_input_input_class_info_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_input_class_info_t);
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

xcb_input_open_device_cookie_t
xcb_input_open_device (xcb_connection_t *c  /**< */,
                       uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_OPEN_DEVICE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_open_device_cookie_t xcb_ret;
    xcb_input_open_device_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_open_device_cookie_t
xcb_input_open_device_unchecked (xcb_connection_t *c  /**< */,
                                 uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_OPEN_DEVICE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_open_device_cookie_t xcb_ret;
    xcb_input_open_device_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_input_class_info_t *
xcb_input_open_device_class_info (const xcb_input_open_device_reply_t *R  /**< */)
{
    return (xcb_input_input_class_info_t *) (R + 1);
}

int
xcb_input_open_device_class_info_length (const xcb_input_open_device_reply_t *R  /**< */)
{
    return R->num_classes;
}

xcb_input_input_class_info_iterator_t
xcb_input_open_device_class_info_iterator (const xcb_input_open_device_reply_t *R  /**< */)
{
    xcb_input_input_class_info_iterator_t i;
    i.data = (xcb_input_input_class_info_t *) (R + 1);
    i.rem = R->num_classes;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_open_device_reply_t *
xcb_input_open_device_reply (xcb_connection_t                *c  /**< */,
                             xcb_input_open_device_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_input_open_device_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_input_close_device_checked (xcb_connection_t *c  /**< */,
                                uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CLOSE_DEVICE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_close_device_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_close_device (xcb_connection_t *c  /**< */,
                        uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CLOSE_DEVICE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_close_device_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_set_device_mode_cookie_t
xcb_input_set_device_mode (xcb_connection_t *c  /**< */,
                           uint8_t           device_id  /**< */,
                           uint8_t           mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_SET_DEVICE_MODE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_set_device_mode_cookie_t xcb_ret;
    xcb_input_set_device_mode_request_t xcb_out;

    xcb_out.device_id = device_id;
    xcb_out.mode = mode;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_set_device_mode_cookie_t
xcb_input_set_device_mode_unchecked (xcb_connection_t *c  /**< */,
                                     uint8_t           device_id  /**< */,
                                     uint8_t           mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_SET_DEVICE_MODE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_set_device_mode_cookie_t xcb_ret;
    xcb_input_set_device_mode_request_t xcb_out;

    xcb_out.device_id = device_id;
    xcb_out.mode = mode;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_set_device_mode_reply_t *
xcb_input_set_device_mode_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_input_set_device_mode_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */)
{
    return (xcb_input_set_device_mode_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_select_extension_event_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_select_extension_event_request_t *_aux = (xcb_input_select_extension_event_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_select_extension_event_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* classes */
    xcb_block_len += _aux->num_classes * sizeof(xcb_input_event_class_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_event_class_t);
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
xcb_input_select_extension_event_checked (xcb_connection_t              *c  /**< */,
                                          xcb_window_t                   window  /**< */,
                                          uint16_t                       num_classes  /**< */,
                                          const xcb_input_event_class_t *classes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_SELECT_EXTENSION_EVENT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_select_extension_event_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.num_classes = num_classes;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_event_class_t classes */
    xcb_parts[4].iov_base = (char *) classes;
    xcb_parts[4].iov_len = num_classes * sizeof(xcb_input_event_class_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_select_extension_event (xcb_connection_t              *c  /**< */,
                                  xcb_window_t                   window  /**< */,
                                  uint16_t                       num_classes  /**< */,
                                  const xcb_input_event_class_t *classes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_SELECT_EXTENSION_EVENT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_select_extension_event_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.num_classes = num_classes;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_event_class_t classes */
    xcb_parts[4].iov_base = (char *) classes;
    xcb_parts[4].iov_len = num_classes * sizeof(xcb_input_event_class_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_input_get_selected_extension_events_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_get_selected_extension_events_reply_t *_aux = (xcb_input_get_selected_extension_events_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_get_selected_extension_events_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* this_classes */
    xcb_block_len += _aux->num_this_classes * sizeof(xcb_input_event_class_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_event_class_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* all_classes */
    xcb_block_len += _aux->num_all_classes * sizeof(xcb_input_event_class_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_event_class_t);
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

xcb_input_get_selected_extension_events_cookie_t
xcb_input_get_selected_extension_events (xcb_connection_t *c  /**< */,
                                         xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_SELECTED_EXTENSION_EVENTS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_selected_extension_events_cookie_t xcb_ret;
    xcb_input_get_selected_extension_events_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_get_selected_extension_events_cookie_t
xcb_input_get_selected_extension_events_unchecked (xcb_connection_t *c  /**< */,
                                                   xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_SELECTED_EXTENSION_EVENTS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_selected_extension_events_cookie_t xcb_ret;
    xcb_input_get_selected_extension_events_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_event_class_t *
xcb_input_get_selected_extension_events_this_classes (const xcb_input_get_selected_extension_events_reply_t *R  /**< */)
{
    return (xcb_input_event_class_t *) (R + 1);
}

int
xcb_input_get_selected_extension_events_this_classes_length (const xcb_input_get_selected_extension_events_reply_t *R  /**< */)
{
    return R->num_this_classes;
}

xcb_generic_iterator_t
xcb_input_get_selected_extension_events_this_classes_end (const xcb_input_get_selected_extension_events_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_input_event_class_t *) (R + 1)) + (R->num_this_classes);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_event_class_t *
xcb_input_get_selected_extension_events_all_classes (const xcb_input_get_selected_extension_events_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_input_get_selected_extension_events_this_classes_end(R);
    return (xcb_input_event_class_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_input_event_class_t, prev.index) + 0);
}

int
xcb_input_get_selected_extension_events_all_classes_length (const xcb_input_get_selected_extension_events_reply_t *R  /**< */)
{
    return R->num_all_classes;
}

xcb_generic_iterator_t
xcb_input_get_selected_extension_events_all_classes_end (const xcb_input_get_selected_extension_events_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_input_get_selected_extension_events_this_classes_end(R);
    i.data = ((xcb_input_event_class_t *) child.data) + (R->num_all_classes);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_get_selected_extension_events_reply_t *
xcb_input_get_selected_extension_events_reply (xcb_connection_t                                  *c  /**< */,
                                               xcb_input_get_selected_extension_events_cookie_t   cookie  /**< */,
                                               xcb_generic_error_t                              **e  /**< */)
{
    return (xcb_input_get_selected_extension_events_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_change_device_dont_propagate_list_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_change_device_dont_propagate_list_request_t *_aux = (xcb_input_change_device_dont_propagate_list_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_change_device_dont_propagate_list_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* classes */
    xcb_block_len += _aux->num_classes * sizeof(xcb_input_event_class_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_event_class_t);
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
xcb_input_change_device_dont_propagate_list_checked (xcb_connection_t              *c  /**< */,
                                                     xcb_window_t                   window  /**< */,
                                                     uint16_t                       num_classes  /**< */,
                                                     uint8_t                        mode  /**< */,
                                                     const xcb_input_event_class_t *classes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_DEVICE_DONT_PROPAGATE_LIST,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_change_device_dont_propagate_list_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.num_classes = num_classes;
    xcb_out.mode = mode;
    xcb_out.pad0 = 0;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_event_class_t classes */
    xcb_parts[4].iov_base = (char *) classes;
    xcb_parts[4].iov_len = num_classes * sizeof(xcb_input_event_class_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_change_device_dont_propagate_list (xcb_connection_t              *c  /**< */,
                                             xcb_window_t                   window  /**< */,
                                             uint16_t                       num_classes  /**< */,
                                             uint8_t                        mode  /**< */,
                                             const xcb_input_event_class_t *classes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_DEVICE_DONT_PROPAGATE_LIST,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_change_device_dont_propagate_list_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.num_classes = num_classes;
    xcb_out.mode = mode;
    xcb_out.pad0 = 0;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_event_class_t classes */
    xcb_parts[4].iov_base = (char *) classes;
    xcb_parts[4].iov_len = num_classes * sizeof(xcb_input_event_class_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_input_get_device_dont_propagate_list_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_get_device_dont_propagate_list_reply_t *_aux = (xcb_input_get_device_dont_propagate_list_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_get_device_dont_propagate_list_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* classes */
    xcb_block_len += _aux->num_classes * sizeof(xcb_input_event_class_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_event_class_t);
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

xcb_input_get_device_dont_propagate_list_cookie_t
xcb_input_get_device_dont_propagate_list (xcb_connection_t *c  /**< */,
                                          xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_DONT_PROPAGATE_LIST,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_dont_propagate_list_cookie_t xcb_ret;
    xcb_input_get_device_dont_propagate_list_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_get_device_dont_propagate_list_cookie_t
xcb_input_get_device_dont_propagate_list_unchecked (xcb_connection_t *c  /**< */,
                                                    xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_DONT_PROPAGATE_LIST,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_dont_propagate_list_cookie_t xcb_ret;
    xcb_input_get_device_dont_propagate_list_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_event_class_t *
xcb_input_get_device_dont_propagate_list_classes (const xcb_input_get_device_dont_propagate_list_reply_t *R  /**< */)
{
    return (xcb_input_event_class_t *) (R + 1);
}

int
xcb_input_get_device_dont_propagate_list_classes_length (const xcb_input_get_device_dont_propagate_list_reply_t *R  /**< */)
{
    return R->num_classes;
}

xcb_generic_iterator_t
xcb_input_get_device_dont_propagate_list_classes_end (const xcb_input_get_device_dont_propagate_list_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_input_event_class_t *) (R + 1)) + (R->num_classes);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_get_device_dont_propagate_list_reply_t *
xcb_input_get_device_dont_propagate_list_reply (xcb_connection_t                                   *c  /**< */,
                                                xcb_input_get_device_dont_propagate_list_cookie_t   cookie  /**< */,
                                                xcb_generic_error_t                               **e  /**< */)
{
    return (xcb_input_get_device_dont_propagate_list_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

void
xcb_input_device_time_coord_next (xcb_input_device_time_coord_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_time_coord_t);
}

xcb_generic_iterator_t
xcb_input_device_time_coord_end (xcb_input_device_time_coord_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

xcb_input_get_device_motion_events_cookie_t
xcb_input_get_device_motion_events (xcb_connection_t *c  /**< */,
                                    xcb_timestamp_t   start  /**< */,
                                    xcb_timestamp_t   stop  /**< */,
                                    uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_MOTION_EVENTS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_motion_events_cookie_t xcb_ret;
    xcb_input_get_device_motion_events_request_t xcb_out;

    xcb_out.start = start;
    xcb_out.stop = stop;
    xcb_out.device_id = device_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_get_device_motion_events_cookie_t
xcb_input_get_device_motion_events_unchecked (xcb_connection_t *c  /**< */,
                                              xcb_timestamp_t   start  /**< */,
                                              xcb_timestamp_t   stop  /**< */,
                                              uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_MOTION_EVENTS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_motion_events_cookie_t xcb_ret;
    xcb_input_get_device_motion_events_request_t xcb_out;

    xcb_out.start = start;
    xcb_out.stop = stop;
    xcb_out.device_id = device_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_get_device_motion_events_reply_t *
xcb_input_get_device_motion_events_reply (xcb_connection_t                             *c  /**< */,
                                          xcb_input_get_device_motion_events_cookie_t   cookie  /**< */,
                                          xcb_generic_error_t                         **e  /**< */)
{
    return (xcb_input_get_device_motion_events_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_input_change_keyboard_device_cookie_t
xcb_input_change_keyboard_device (xcb_connection_t *c  /**< */,
                                  uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_KEYBOARD_DEVICE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_change_keyboard_device_cookie_t xcb_ret;
    xcb_input_change_keyboard_device_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_change_keyboard_device_cookie_t
xcb_input_change_keyboard_device_unchecked (xcb_connection_t *c  /**< */,
                                            uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_KEYBOARD_DEVICE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_change_keyboard_device_cookie_t xcb_ret;
    xcb_input_change_keyboard_device_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_change_keyboard_device_reply_t *
xcb_input_change_keyboard_device_reply (xcb_connection_t                           *c  /**< */,
                                        xcb_input_change_keyboard_device_cookie_t   cookie  /**< */,
                                        xcb_generic_error_t                       **e  /**< */)
{
    return (xcb_input_change_keyboard_device_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_input_change_pointer_device_cookie_t
xcb_input_change_pointer_device (xcb_connection_t *c  /**< */,
                                 uint8_t           x_axis  /**< */,
                                 uint8_t           y_axis  /**< */,
                                 uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_POINTER_DEVICE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_change_pointer_device_cookie_t xcb_ret;
    xcb_input_change_pointer_device_request_t xcb_out;

    xcb_out.x_axis = x_axis;
    xcb_out.y_axis = y_axis;
    xcb_out.device_id = device_id;
    xcb_out.pad0 = 0;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_change_pointer_device_cookie_t
xcb_input_change_pointer_device_unchecked (xcb_connection_t *c  /**< */,
                                           uint8_t           x_axis  /**< */,
                                           uint8_t           y_axis  /**< */,
                                           uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_POINTER_DEVICE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_change_pointer_device_cookie_t xcb_ret;
    xcb_input_change_pointer_device_request_t xcb_out;

    xcb_out.x_axis = x_axis;
    xcb_out.y_axis = y_axis;
    xcb_out.device_id = device_id;
    xcb_out.pad0 = 0;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_change_pointer_device_reply_t *
xcb_input_change_pointer_device_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_input_change_pointer_device_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */)
{
    return (xcb_input_change_pointer_device_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_grab_device_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_grab_device_request_t *_aux = (xcb_input_grab_device_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_grab_device_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* classes */
    xcb_block_len += _aux->num_classes * sizeof(xcb_input_event_class_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_event_class_t);
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

xcb_input_grab_device_cookie_t
xcb_input_grab_device (xcb_connection_t              *c  /**< */,
                       xcb_window_t                   grab_window  /**< */,
                       xcb_timestamp_t                time  /**< */,
                       uint16_t                       num_classes  /**< */,
                       uint8_t                        this_device_mode  /**< */,
                       uint8_t                        other_device_mode  /**< */,
                       uint8_t                        owner_events  /**< */,
                       uint8_t                        device_id  /**< */,
                       const xcb_input_event_class_t *classes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GRAB_DEVICE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_input_grab_device_cookie_t xcb_ret;
    xcb_input_grab_device_request_t xcb_out;

    xcb_out.grab_window = grab_window;
    xcb_out.time = time;
    xcb_out.num_classes = num_classes;
    xcb_out.this_device_mode = this_device_mode;
    xcb_out.other_device_mode = other_device_mode;
    xcb_out.owner_events = owner_events;
    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_event_class_t classes */
    xcb_parts[4].iov_base = (char *) classes;
    xcb_parts[4].iov_len = num_classes * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_grab_device_cookie_t
xcb_input_grab_device_unchecked (xcb_connection_t              *c  /**< */,
                                 xcb_window_t                   grab_window  /**< */,
                                 xcb_timestamp_t                time  /**< */,
                                 uint16_t                       num_classes  /**< */,
                                 uint8_t                        this_device_mode  /**< */,
                                 uint8_t                        other_device_mode  /**< */,
                                 uint8_t                        owner_events  /**< */,
                                 uint8_t                        device_id  /**< */,
                                 const xcb_input_event_class_t *classes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GRAB_DEVICE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_input_grab_device_cookie_t xcb_ret;
    xcb_input_grab_device_request_t xcb_out;

    xcb_out.grab_window = grab_window;
    xcb_out.time = time;
    xcb_out.num_classes = num_classes;
    xcb_out.this_device_mode = this_device_mode;
    xcb_out.other_device_mode = other_device_mode;
    xcb_out.owner_events = owner_events;
    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_event_class_t classes */
    xcb_parts[4].iov_base = (char *) classes;
    xcb_parts[4].iov_len = num_classes * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_grab_device_reply_t *
xcb_input_grab_device_reply (xcb_connection_t                *c  /**< */,
                             xcb_input_grab_device_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_input_grab_device_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_input_ungrab_device_checked (xcb_connection_t *c  /**< */,
                                 xcb_timestamp_t   time  /**< */,
                                 uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_UNGRAB_DEVICE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_ungrab_device_request_t xcb_out;

    xcb_out.time = time;
    xcb_out.device_id = device_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_ungrab_device (xcb_connection_t *c  /**< */,
                         xcb_timestamp_t   time  /**< */,
                         uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_UNGRAB_DEVICE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_ungrab_device_request_t xcb_out;

    xcb_out.time = time;
    xcb_out.device_id = device_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_input_grab_device_key_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_grab_device_key_request_t *_aux = (xcb_input_grab_device_key_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_grab_device_key_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* classes */
    xcb_block_len += _aux->num_classes * sizeof(xcb_input_event_class_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_event_class_t);
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
xcb_input_grab_device_key_checked (xcb_connection_t              *c  /**< */,
                                   xcb_window_t                   grab_window  /**< */,
                                   uint16_t                       num_classes  /**< */,
                                   uint16_t                       modifiers  /**< */,
                                   uint8_t                        modifier_device  /**< */,
                                   uint8_t                        grabbed_device  /**< */,
                                   uint8_t                        key  /**< */,
                                   uint8_t                        this_device_mode  /**< */,
                                   uint8_t                        other_device_mode  /**< */,
                                   uint8_t                        owner_events  /**< */,
                                   const xcb_input_event_class_t *classes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GRAB_DEVICE_KEY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_grab_device_key_request_t xcb_out;

    xcb_out.grab_window = grab_window;
    xcb_out.num_classes = num_classes;
    xcb_out.modifiers = modifiers;
    xcb_out.modifier_device = modifier_device;
    xcb_out.grabbed_device = grabbed_device;
    xcb_out.key = key;
    xcb_out.this_device_mode = this_device_mode;
    xcb_out.other_device_mode = other_device_mode;
    xcb_out.owner_events = owner_events;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_event_class_t classes */
    xcb_parts[4].iov_base = (char *) classes;
    xcb_parts[4].iov_len = num_classes * sizeof(xcb_input_event_class_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_grab_device_key (xcb_connection_t              *c  /**< */,
                           xcb_window_t                   grab_window  /**< */,
                           uint16_t                       num_classes  /**< */,
                           uint16_t                       modifiers  /**< */,
                           uint8_t                        modifier_device  /**< */,
                           uint8_t                        grabbed_device  /**< */,
                           uint8_t                        key  /**< */,
                           uint8_t                        this_device_mode  /**< */,
                           uint8_t                        other_device_mode  /**< */,
                           uint8_t                        owner_events  /**< */,
                           const xcb_input_event_class_t *classes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GRAB_DEVICE_KEY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_grab_device_key_request_t xcb_out;

    xcb_out.grab_window = grab_window;
    xcb_out.num_classes = num_classes;
    xcb_out.modifiers = modifiers;
    xcb_out.modifier_device = modifier_device;
    xcb_out.grabbed_device = grabbed_device;
    xcb_out.key = key;
    xcb_out.this_device_mode = this_device_mode;
    xcb_out.other_device_mode = other_device_mode;
    xcb_out.owner_events = owner_events;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_event_class_t classes */
    xcb_parts[4].iov_base = (char *) classes;
    xcb_parts[4].iov_len = num_classes * sizeof(xcb_input_event_class_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_ungrab_device_key_checked (xcb_connection_t *c  /**< */,
                                     xcb_window_t      grabWindow  /**< */,
                                     uint16_t          modifiers  /**< */,
                                     uint8_t           modifier_device  /**< */,
                                     uint8_t           key  /**< */,
                                     uint8_t           grabbed_device  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_UNGRAB_DEVICE_KEY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_ungrab_device_key_request_t xcb_out;

    xcb_out.grabWindow = grabWindow;
    xcb_out.modifiers = modifiers;
    xcb_out.modifier_device = modifier_device;
    xcb_out.key = key;
    xcb_out.grabbed_device = grabbed_device;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_ungrab_device_key (xcb_connection_t *c  /**< */,
                             xcb_window_t      grabWindow  /**< */,
                             uint16_t          modifiers  /**< */,
                             uint8_t           modifier_device  /**< */,
                             uint8_t           key  /**< */,
                             uint8_t           grabbed_device  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_UNGRAB_DEVICE_KEY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_ungrab_device_key_request_t xcb_out;

    xcb_out.grabWindow = grabWindow;
    xcb_out.modifiers = modifiers;
    xcb_out.modifier_device = modifier_device;
    xcb_out.key = key;
    xcb_out.grabbed_device = grabbed_device;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_input_grab_device_button_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_grab_device_button_request_t *_aux = (xcb_input_grab_device_button_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_grab_device_button_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* classes */
    xcb_block_len += _aux->num_classes * sizeof(xcb_input_event_class_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_event_class_t);
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
xcb_input_grab_device_button_checked (xcb_connection_t              *c  /**< */,
                                      xcb_window_t                   grab_window  /**< */,
                                      uint8_t                        grabbed_device  /**< */,
                                      uint8_t                        modifier_device  /**< */,
                                      uint16_t                       num_classes  /**< */,
                                      uint16_t                       modifiers  /**< */,
                                      uint8_t                        this_device_mode  /**< */,
                                      uint8_t                        other_device_mode  /**< */,
                                      uint8_t                        button  /**< */,
                                      uint8_t                        owner_events  /**< */,
                                      const xcb_input_event_class_t *classes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GRAB_DEVICE_BUTTON,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_grab_device_button_request_t xcb_out;

    xcb_out.grab_window = grab_window;
    xcb_out.grabbed_device = grabbed_device;
    xcb_out.modifier_device = modifier_device;
    xcb_out.num_classes = num_classes;
    xcb_out.modifiers = modifiers;
    xcb_out.this_device_mode = this_device_mode;
    xcb_out.other_device_mode = other_device_mode;
    xcb_out.button = button;
    xcb_out.owner_events = owner_events;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_event_class_t classes */
    xcb_parts[4].iov_base = (char *) classes;
    xcb_parts[4].iov_len = num_classes * sizeof(xcb_input_event_class_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_grab_device_button (xcb_connection_t              *c  /**< */,
                              xcb_window_t                   grab_window  /**< */,
                              uint8_t                        grabbed_device  /**< */,
                              uint8_t                        modifier_device  /**< */,
                              uint16_t                       num_classes  /**< */,
                              uint16_t                       modifiers  /**< */,
                              uint8_t                        this_device_mode  /**< */,
                              uint8_t                        other_device_mode  /**< */,
                              uint8_t                        button  /**< */,
                              uint8_t                        owner_events  /**< */,
                              const xcb_input_event_class_t *classes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GRAB_DEVICE_BUTTON,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_grab_device_button_request_t xcb_out;

    xcb_out.grab_window = grab_window;
    xcb_out.grabbed_device = grabbed_device;
    xcb_out.modifier_device = modifier_device;
    xcb_out.num_classes = num_classes;
    xcb_out.modifiers = modifiers;
    xcb_out.this_device_mode = this_device_mode;
    xcb_out.other_device_mode = other_device_mode;
    xcb_out.button = button;
    xcb_out.owner_events = owner_events;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_event_class_t classes */
    xcb_parts[4].iov_base = (char *) classes;
    xcb_parts[4].iov_len = num_classes * sizeof(xcb_input_event_class_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_ungrab_device_button_checked (xcb_connection_t *c  /**< */,
                                        xcb_window_t      grab_window  /**< */,
                                        uint16_t          modifiers  /**< */,
                                        uint8_t           modifier_device  /**< */,
                                        uint8_t           button  /**< */,
                                        uint8_t           grabbed_device  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_UNGRAB_DEVICE_BUTTON,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_ungrab_device_button_request_t xcb_out;

    xcb_out.grab_window = grab_window;
    xcb_out.modifiers = modifiers;
    xcb_out.modifier_device = modifier_device;
    xcb_out.button = button;
    xcb_out.grabbed_device = grabbed_device;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_ungrab_device_button (xcb_connection_t *c  /**< */,
                                xcb_window_t      grab_window  /**< */,
                                uint16_t          modifiers  /**< */,
                                uint8_t           modifier_device  /**< */,
                                uint8_t           button  /**< */,
                                uint8_t           grabbed_device  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_UNGRAB_DEVICE_BUTTON,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_ungrab_device_button_request_t xcb_out;

    xcb_out.grab_window = grab_window;
    xcb_out.modifiers = modifiers;
    xcb_out.modifier_device = modifier_device;
    xcb_out.button = button;
    xcb_out.grabbed_device = grabbed_device;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_allow_device_events_checked (xcb_connection_t *c  /**< */,
                                       xcb_timestamp_t   time  /**< */,
                                       uint8_t           mode  /**< */,
                                       uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_ALLOW_DEVICE_EVENTS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_allow_device_events_request_t xcb_out;

    xcb_out.time = time;
    xcb_out.mode = mode;
    xcb_out.device_id = device_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_allow_device_events (xcb_connection_t *c  /**< */,
                               xcb_timestamp_t   time  /**< */,
                               uint8_t           mode  /**< */,
                               uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_ALLOW_DEVICE_EVENTS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_allow_device_events_request_t xcb_out;

    xcb_out.time = time;
    xcb_out.mode = mode;
    xcb_out.device_id = device_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_get_device_focus_cookie_t
xcb_input_get_device_focus (xcb_connection_t *c  /**< */,
                            uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_FOCUS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_focus_cookie_t xcb_ret;
    xcb_input_get_device_focus_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_get_device_focus_cookie_t
xcb_input_get_device_focus_unchecked (xcb_connection_t *c  /**< */,
                                      uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_FOCUS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_focus_cookie_t xcb_ret;
    xcb_input_get_device_focus_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_get_device_focus_reply_t *
xcb_input_get_device_focus_reply (xcb_connection_t                     *c  /**< */,
                                  xcb_input_get_device_focus_cookie_t   cookie  /**< */,
                                  xcb_generic_error_t                 **e  /**< */)
{
    return (xcb_input_get_device_focus_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_input_set_device_focus_checked (xcb_connection_t *c  /**< */,
                                    xcb_window_t      focus  /**< */,
                                    xcb_timestamp_t   time  /**< */,
                                    uint8_t           revert_to  /**< */,
                                    uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_SET_DEVICE_FOCUS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_set_device_focus_request_t xcb_out;

    xcb_out.focus = focus;
    xcb_out.time = time;
    xcb_out.revert_to = revert_to;
    xcb_out.device_id = device_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_set_device_focus (xcb_connection_t *c  /**< */,
                            xcb_window_t      focus  /**< */,
                            xcb_timestamp_t   time  /**< */,
                            uint8_t           revert_to  /**< */,
                            uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_SET_DEVICE_FOCUS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_set_device_focus_request_t xcb_out;

    xcb_out.focus = focus;
    xcb_out.time = time;
    xcb_out.revert_to = revert_to;
    xcb_out.device_id = device_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

void
xcb_input_kbd_feedback_state_next (xcb_input_kbd_feedback_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_kbd_feedback_state_t);
}

xcb_generic_iterator_t
xcb_input_kbd_feedback_state_end (xcb_input_kbd_feedback_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_ptr_feedback_state_next (xcb_input_ptr_feedback_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_ptr_feedback_state_t);
}

xcb_generic_iterator_t
xcb_input_ptr_feedback_state_end (xcb_input_ptr_feedback_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_integer_feedback_state_next (xcb_input_integer_feedback_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_integer_feedback_state_t);
}

xcb_generic_iterator_t
xcb_input_integer_feedback_state_end (xcb_input_integer_feedback_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_string_feedback_state_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_string_feedback_state_t *_aux = (xcb_input_string_feedback_state_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_string_feedback_state_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* keysyms */
    xcb_block_len += _aux->num_keysyms * sizeof(xcb_keysym_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_keysym_t);
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

xcb_keysym_t *
xcb_input_string_feedback_state_keysyms (const xcb_input_string_feedback_state_t *R  /**< */)
{
    return (xcb_keysym_t *) (R + 1);
}

int
xcb_input_string_feedback_state_keysyms_length (const xcb_input_string_feedback_state_t *R  /**< */)
{
    return R->num_keysyms;
}

xcb_generic_iterator_t
xcb_input_string_feedback_state_keysyms_end (const xcb_input_string_feedback_state_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_keysym_t *) (R + 1)) + (R->num_keysyms);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_string_feedback_state_next (xcb_input_string_feedback_state_iterator_t *i  /**< */)
{
    xcb_input_string_feedback_state_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_string_feedback_state_t *)(((char *)R) + xcb_input_string_feedback_state_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_string_feedback_state_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_string_feedback_state_end (xcb_input_string_feedback_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_string_feedback_state_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

void
xcb_input_bell_feedback_state_next (xcb_input_bell_feedback_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_bell_feedback_state_t);
}

xcb_generic_iterator_t
xcb_input_bell_feedback_state_end (xcb_input_bell_feedback_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_led_feedback_state_next (xcb_input_led_feedback_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_led_feedback_state_t);
}

xcb_generic_iterator_t
xcb_input_led_feedback_state_end (xcb_input_led_feedback_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_feedback_state_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_feedback_state_t *_aux = (xcb_input_feedback_state_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_feedback_state_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* uninterpreted_data */
    xcb_block_len += (_aux->len - 4) * sizeof(uint8_t);
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

uint8_t *
xcb_input_feedback_state_uninterpreted_data (const xcb_input_feedback_state_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_input_feedback_state_uninterpreted_data_length (const xcb_input_feedback_state_t *R  /**< */)
{
    return (R->len - 4);
}

xcb_generic_iterator_t
xcb_input_feedback_state_uninterpreted_data_end (const xcb_input_feedback_state_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->len - 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_feedback_state_next (xcb_input_feedback_state_iterator_t *i  /**< */)
{
    xcb_input_feedback_state_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_feedback_state_t *)(((char *)R) + xcb_input_feedback_state_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_feedback_state_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_feedback_state_end (xcb_input_feedback_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_feedback_state_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_input_get_feedback_control_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_get_feedback_control_reply_t *_aux = (xcb_input_get_feedback_control_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_input_get_feedback_control_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* feedbacks */
    for(i=0; i<_aux->num_feedbacks; i++) {
        xcb_tmp_len = xcb_input_feedback_state_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_input_feedback_state_t);
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

xcb_input_get_feedback_control_cookie_t
xcb_input_get_feedback_control (xcb_connection_t *c  /**< */,
                                uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_FEEDBACK_CONTROL,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_feedback_control_cookie_t xcb_ret;
    xcb_input_get_feedback_control_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_get_feedback_control_cookie_t
xcb_input_get_feedback_control_unchecked (xcb_connection_t *c  /**< */,
                                          uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_FEEDBACK_CONTROL,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_feedback_control_cookie_t xcb_ret;
    xcb_input_get_feedback_control_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_input_get_feedback_control_feedbacks_length (const xcb_input_get_feedback_control_reply_t *R  /**< */)
{
    return R->num_feedbacks;
}

xcb_input_feedback_state_iterator_t
xcb_input_get_feedback_control_feedbacks_iterator (const xcb_input_get_feedback_control_reply_t *R  /**< */)
{
    xcb_input_feedback_state_iterator_t i;
    i.data = (xcb_input_feedback_state_t *) (R + 1);
    i.rem = R->num_feedbacks;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_get_feedback_control_reply_t *
xcb_input_get_feedback_control_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_input_get_feedback_control_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */)
{
    return (xcb_input_get_feedback_control_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

void
xcb_input_kbd_feedback_ctl_next (xcb_input_kbd_feedback_ctl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_kbd_feedback_ctl_t);
}

xcb_generic_iterator_t
xcb_input_kbd_feedback_ctl_end (xcb_input_kbd_feedback_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_ptr_feedback_ctl_next (xcb_input_ptr_feedback_ctl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_ptr_feedback_ctl_t);
}

xcb_generic_iterator_t
xcb_input_ptr_feedback_ctl_end (xcb_input_ptr_feedback_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_integer_feedback_ctl_next (xcb_input_integer_feedback_ctl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_integer_feedback_ctl_t);
}

xcb_generic_iterator_t
xcb_input_integer_feedback_ctl_end (xcb_input_integer_feedback_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_string_feedback_ctl_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_string_feedback_ctl_t *_aux = (xcb_input_string_feedback_ctl_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_string_feedback_ctl_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* keysyms */
    xcb_block_len += _aux->num_keysyms * sizeof(xcb_keysym_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_keysym_t);
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

xcb_keysym_t *
xcb_input_string_feedback_ctl_keysyms (const xcb_input_string_feedback_ctl_t *R  /**< */)
{
    return (xcb_keysym_t *) (R + 1);
}

int
xcb_input_string_feedback_ctl_keysyms_length (const xcb_input_string_feedback_ctl_t *R  /**< */)
{
    return R->num_keysyms;
}

xcb_generic_iterator_t
xcb_input_string_feedback_ctl_keysyms_end (const xcb_input_string_feedback_ctl_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_keysym_t *) (R + 1)) + (R->num_keysyms);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_string_feedback_ctl_next (xcb_input_string_feedback_ctl_iterator_t *i  /**< */)
{
    xcb_input_string_feedback_ctl_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_string_feedback_ctl_t *)(((char *)R) + xcb_input_string_feedback_ctl_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_string_feedback_ctl_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_string_feedback_ctl_end (xcb_input_string_feedback_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_string_feedback_ctl_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

void
xcb_input_bell_feedback_ctl_next (xcb_input_bell_feedback_ctl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_bell_feedback_ctl_t);
}

xcb_generic_iterator_t
xcb_input_bell_feedback_ctl_end (xcb_input_bell_feedback_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_led_feedback_ctl_next (xcb_input_led_feedback_ctl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_led_feedback_ctl_t);
}

xcb_generic_iterator_t
xcb_input_led_feedback_ctl_end (xcb_input_led_feedback_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_feedback_ctl_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_feedback_ctl_t *_aux = (xcb_input_feedback_ctl_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_feedback_ctl_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* uninterpreted_data */
    xcb_block_len += (_aux->len - 4) * sizeof(uint8_t);
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

uint8_t *
xcb_input_feedback_ctl_uninterpreted_data (const xcb_input_feedback_ctl_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_input_feedback_ctl_uninterpreted_data_length (const xcb_input_feedback_ctl_t *R  /**< */)
{
    return (R->len - 4);
}

xcb_generic_iterator_t
xcb_input_feedback_ctl_uninterpreted_data_end (const xcb_input_feedback_ctl_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->len - 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_feedback_ctl_next (xcb_input_feedback_ctl_iterator_t *i  /**< */)
{
    xcb_input_feedback_ctl_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_feedback_ctl_t *)(((char *)R) + xcb_input_feedback_ctl_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_feedback_ctl_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_feedback_ctl_end (xcb_input_feedback_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_feedback_ctl_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_input_change_feedback_control_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_change_feedback_control_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* feedback */
    xcb_block_len += xcb_input_feedback_ctl_sizeof(xcb_tmp);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_feedback_ctl_t);
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
xcb_input_change_feedback_control_checked (xcb_connection_t         *c  /**< */,
                                           uint32_t                  mask  /**< */,
                                           uint8_t                   device_id  /**< */,
                                           uint8_t                   feedback_id  /**< */,
                                           xcb_input_feedback_ctl_t *feedback  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_FEEDBACK_CONTROL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_change_feedback_control_request_t xcb_out;

    xcb_out.mask = mask;
    xcb_out.device_id = device_id;
    xcb_out.feedback_id = feedback_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_feedback_ctl_t feedback */
    xcb_parts[4].iov_base = (char *) feedback;
    xcb_parts[4].iov_len =
      xcb_input_feedback_ctl_sizeof (feedback);

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_change_feedback_control (xcb_connection_t         *c  /**< */,
                                   uint32_t                  mask  /**< */,
                                   uint8_t                   device_id  /**< */,
                                   uint8_t                   feedback_id  /**< */,
                                   xcb_input_feedback_ctl_t *feedback  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_FEEDBACK_CONTROL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_change_feedback_control_request_t xcb_out;

    xcb_out.mask = mask;
    xcb_out.device_id = device_id;
    xcb_out.feedback_id = feedback_id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_feedback_ctl_t feedback */
    xcb_parts[4].iov_base = (char *) feedback;
    xcb_parts[4].iov_len =
      xcb_input_feedback_ctl_sizeof (feedback);

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_input_get_device_key_mapping_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_get_device_key_mapping_reply_t *_aux = (xcb_input_get_device_key_mapping_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_get_device_key_mapping_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* keysyms */
    xcb_block_len += _aux->length * sizeof(xcb_keysym_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_keysym_t);
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

xcb_input_get_device_key_mapping_cookie_t
xcb_input_get_device_key_mapping (xcb_connection_t     *c  /**< */,
                                  uint8_t               device_id  /**< */,
                                  xcb_input_key_code_t  first_keycode  /**< */,
                                  uint8_t               count  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_KEY_MAPPING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_key_mapping_cookie_t xcb_ret;
    xcb_input_get_device_key_mapping_request_t xcb_out;

    xcb_out.device_id = device_id;
    xcb_out.first_keycode = first_keycode;
    xcb_out.count = count;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_get_device_key_mapping_cookie_t
xcb_input_get_device_key_mapping_unchecked (xcb_connection_t     *c  /**< */,
                                            uint8_t               device_id  /**< */,
                                            xcb_input_key_code_t  first_keycode  /**< */,
                                            uint8_t               count  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_KEY_MAPPING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_key_mapping_cookie_t xcb_ret;
    xcb_input_get_device_key_mapping_request_t xcb_out;

    xcb_out.device_id = device_id;
    xcb_out.first_keycode = first_keycode;
    xcb_out.count = count;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_keysym_t *
xcb_input_get_device_key_mapping_keysyms (const xcb_input_get_device_key_mapping_reply_t *R  /**< */)
{
    return (xcb_keysym_t *) (R + 1);
}

int
xcb_input_get_device_key_mapping_keysyms_length (const xcb_input_get_device_key_mapping_reply_t *R  /**< */)
{
    return R->length;
}

xcb_generic_iterator_t
xcb_input_get_device_key_mapping_keysyms_end (const xcb_input_get_device_key_mapping_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_keysym_t *) (R + 1)) + (R->length);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_get_device_key_mapping_reply_t *
xcb_input_get_device_key_mapping_reply (xcb_connection_t                           *c  /**< */,
                                        xcb_input_get_device_key_mapping_cookie_t   cookie  /**< */,
                                        xcb_generic_error_t                       **e  /**< */)
{
    return (xcb_input_get_device_key_mapping_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_change_device_key_mapping_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_change_device_key_mapping_request_t *_aux = (xcb_input_change_device_key_mapping_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_change_device_key_mapping_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* keysyms */
    xcb_block_len += (_aux->keycode_count * _aux->keysyms_per_keycode) * sizeof(xcb_keysym_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_keysym_t);
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
xcb_input_change_device_key_mapping_checked (xcb_connection_t     *c  /**< */,
                                             uint8_t               device_id  /**< */,
                                             xcb_input_key_code_t  first_keycode  /**< */,
                                             uint8_t               keysyms_per_keycode  /**< */,
                                             uint8_t               keycode_count  /**< */,
                                             const xcb_keysym_t   *keysyms  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_DEVICE_KEY_MAPPING,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_change_device_key_mapping_request_t xcb_out;

    xcb_out.device_id = device_id;
    xcb_out.first_keycode = first_keycode;
    xcb_out.keysyms_per_keycode = keysyms_per_keycode;
    xcb_out.keycode_count = keycode_count;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_keysym_t keysyms */
    xcb_parts[4].iov_base = (char *) keysyms;
    xcb_parts[4].iov_len = (keycode_count * keysyms_per_keycode) * sizeof(xcb_keysym_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_change_device_key_mapping (xcb_connection_t     *c  /**< */,
                                     uint8_t               device_id  /**< */,
                                     xcb_input_key_code_t  first_keycode  /**< */,
                                     uint8_t               keysyms_per_keycode  /**< */,
                                     uint8_t               keycode_count  /**< */,
                                     const xcb_keysym_t   *keysyms  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_DEVICE_KEY_MAPPING,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_change_device_key_mapping_request_t xcb_out;

    xcb_out.device_id = device_id;
    xcb_out.first_keycode = first_keycode;
    xcb_out.keysyms_per_keycode = keysyms_per_keycode;
    xcb_out.keycode_count = keycode_count;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_keysym_t keysyms */
    xcb_parts[4].iov_base = (char *) keysyms;
    xcb_parts[4].iov_len = (keycode_count * keysyms_per_keycode) * sizeof(xcb_keysym_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_input_get_device_modifier_mapping_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_get_device_modifier_mapping_reply_t *_aux = (xcb_input_get_device_modifier_mapping_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_get_device_modifier_mapping_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* keymaps */
    xcb_block_len += (_aux->keycodes_per_modifier * 8) * sizeof(uint8_t);
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

xcb_input_get_device_modifier_mapping_cookie_t
xcb_input_get_device_modifier_mapping (xcb_connection_t *c  /**< */,
                                       uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_MODIFIER_MAPPING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_modifier_mapping_cookie_t xcb_ret;
    xcb_input_get_device_modifier_mapping_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_get_device_modifier_mapping_cookie_t
xcb_input_get_device_modifier_mapping_unchecked (xcb_connection_t *c  /**< */,
                                                 uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_MODIFIER_MAPPING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_modifier_mapping_cookie_t xcb_ret;
    xcb_input_get_device_modifier_mapping_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_input_get_device_modifier_mapping_keymaps (const xcb_input_get_device_modifier_mapping_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_input_get_device_modifier_mapping_keymaps_length (const xcb_input_get_device_modifier_mapping_reply_t *R  /**< */)
{
    return (R->keycodes_per_modifier * 8);
}

xcb_generic_iterator_t
xcb_input_get_device_modifier_mapping_keymaps_end (const xcb_input_get_device_modifier_mapping_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->keycodes_per_modifier * 8));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_get_device_modifier_mapping_reply_t *
xcb_input_get_device_modifier_mapping_reply (xcb_connection_t                                *c  /**< */,
                                             xcb_input_get_device_modifier_mapping_cookie_t   cookie  /**< */,
                                             xcb_generic_error_t                            **e  /**< */)
{
    return (xcb_input_get_device_modifier_mapping_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_set_device_modifier_mapping_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_set_device_modifier_mapping_request_t *_aux = (xcb_input_set_device_modifier_mapping_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_set_device_modifier_mapping_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* keymaps */
    xcb_block_len += (_aux->keycodes_per_modifier * 8) * sizeof(uint8_t);
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

xcb_input_set_device_modifier_mapping_cookie_t
xcb_input_set_device_modifier_mapping (xcb_connection_t *c  /**< */,
                                       uint8_t           device_id  /**< */,
                                       uint8_t           keycodes_per_modifier  /**< */,
                                       const uint8_t    *keymaps  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_SET_DEVICE_MODIFIER_MAPPING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_input_set_device_modifier_mapping_cookie_t xcb_ret;
    xcb_input_set_device_modifier_mapping_request_t xcb_out;

    xcb_out.device_id = device_id;
    xcb_out.keycodes_per_modifier = keycodes_per_modifier;
    xcb_out.pad0 = 0;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t keymaps */
    xcb_parts[4].iov_base = (char *) keymaps;
    xcb_parts[4].iov_len = (keycodes_per_modifier * 8) * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_set_device_modifier_mapping_cookie_t
xcb_input_set_device_modifier_mapping_unchecked (xcb_connection_t *c  /**< */,
                                                 uint8_t           device_id  /**< */,
                                                 uint8_t           keycodes_per_modifier  /**< */,
                                                 const uint8_t    *keymaps  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_SET_DEVICE_MODIFIER_MAPPING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_input_set_device_modifier_mapping_cookie_t xcb_ret;
    xcb_input_set_device_modifier_mapping_request_t xcb_out;

    xcb_out.device_id = device_id;
    xcb_out.keycodes_per_modifier = keycodes_per_modifier;
    xcb_out.pad0 = 0;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t keymaps */
    xcb_parts[4].iov_base = (char *) keymaps;
    xcb_parts[4].iov_len = (keycodes_per_modifier * 8) * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_set_device_modifier_mapping_reply_t *
xcb_input_set_device_modifier_mapping_reply (xcb_connection_t                                *c  /**< */,
                                             xcb_input_set_device_modifier_mapping_cookie_t   cookie  /**< */,
                                             xcb_generic_error_t                            **e  /**< */)
{
    return (xcb_input_set_device_modifier_mapping_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_get_device_button_mapping_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_get_device_button_mapping_reply_t *_aux = (xcb_input_get_device_button_mapping_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_get_device_button_mapping_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* map */
    xcb_block_len += _aux->map_size * sizeof(uint8_t);
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

xcb_input_get_device_button_mapping_cookie_t
xcb_input_get_device_button_mapping (xcb_connection_t *c  /**< */,
                                     uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_BUTTON_MAPPING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_button_mapping_cookie_t xcb_ret;
    xcb_input_get_device_button_mapping_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_get_device_button_mapping_cookie_t
xcb_input_get_device_button_mapping_unchecked (xcb_connection_t *c  /**< */,
                                               uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_BUTTON_MAPPING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_button_mapping_cookie_t xcb_ret;
    xcb_input_get_device_button_mapping_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_input_get_device_button_mapping_map (const xcb_input_get_device_button_mapping_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_input_get_device_button_mapping_map_length (const xcb_input_get_device_button_mapping_reply_t *R  /**< */)
{
    return R->map_size;
}

xcb_generic_iterator_t
xcb_input_get_device_button_mapping_map_end (const xcb_input_get_device_button_mapping_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + (R->map_size);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_get_device_button_mapping_reply_t *
xcb_input_get_device_button_mapping_reply (xcb_connection_t                              *c  /**< */,
                                           xcb_input_get_device_button_mapping_cookie_t   cookie  /**< */,
                                           xcb_generic_error_t                          **e  /**< */)
{
    return (xcb_input_get_device_button_mapping_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_set_device_button_mapping_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_set_device_button_mapping_request_t *_aux = (xcb_input_set_device_button_mapping_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_set_device_button_mapping_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* map */
    xcb_block_len += _aux->map_size * sizeof(uint8_t);
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

xcb_input_set_device_button_mapping_cookie_t
xcb_input_set_device_button_mapping (xcb_connection_t *c  /**< */,
                                     uint8_t           device_id  /**< */,
                                     uint8_t           map_size  /**< */,
                                     const uint8_t    *map  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_SET_DEVICE_BUTTON_MAPPING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_input_set_device_button_mapping_cookie_t xcb_ret;
    xcb_input_set_device_button_mapping_request_t xcb_out;

    xcb_out.device_id = device_id;
    xcb_out.map_size = map_size;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t map */
    xcb_parts[4].iov_base = (char *) map;
    xcb_parts[4].iov_len = map_size * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_set_device_button_mapping_cookie_t
xcb_input_set_device_button_mapping_unchecked (xcb_connection_t *c  /**< */,
                                               uint8_t           device_id  /**< */,
                                               uint8_t           map_size  /**< */,
                                               const uint8_t    *map  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_SET_DEVICE_BUTTON_MAPPING,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_input_set_device_button_mapping_cookie_t xcb_ret;
    xcb_input_set_device_button_mapping_request_t xcb_out;

    xcb_out.device_id = device_id;
    xcb_out.map_size = map_size;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t map */
    xcb_parts[4].iov_base = (char *) map;
    xcb_parts[4].iov_len = map_size * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_set_device_button_mapping_reply_t *
xcb_input_set_device_button_mapping_reply (xcb_connection_t                              *c  /**< */,
                                           xcb_input_set_device_button_mapping_cookie_t   cookie  /**< */,
                                           xcb_generic_error_t                          **e  /**< */)
{
    return (xcb_input_set_device_button_mapping_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

void
xcb_input_key_state_next (xcb_input_key_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_key_state_t);
}

xcb_generic_iterator_t
xcb_input_key_state_end (xcb_input_key_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_button_state_next (xcb_input_button_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_button_state_t);
}

xcb_generic_iterator_t
xcb_input_button_state_end (xcb_input_button_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_valuator_state_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_valuator_state_t *_aux = (xcb_input_valuator_state_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_valuator_state_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* valuators */
    xcb_block_len += _aux->num_valuators * sizeof(uint32_t);
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

uint32_t *
xcb_input_valuator_state_valuators (const xcb_input_valuator_state_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_input_valuator_state_valuators_length (const xcb_input_valuator_state_t *R  /**< */)
{
    return R->num_valuators;
}

xcb_generic_iterator_t
xcb_input_valuator_state_valuators_end (const xcb_input_valuator_state_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->num_valuators);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_valuator_state_next (xcb_input_valuator_state_iterator_t *i  /**< */)
{
    xcb_input_valuator_state_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_valuator_state_t *)(((char *)R) + xcb_input_valuator_state_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_valuator_state_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_valuator_state_end (xcb_input_valuator_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_valuator_state_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_input_input_state_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_input_state_t *_aux = (xcb_input_input_state_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_input_state_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* uninterpreted_data */
    xcb_block_len += (_aux->len - 4) * sizeof(uint8_t);
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

uint8_t *
xcb_input_input_state_uninterpreted_data (const xcb_input_input_state_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_input_input_state_uninterpreted_data_length (const xcb_input_input_state_t *R  /**< */)
{
    return (R->len - 4);
}

xcb_generic_iterator_t
xcb_input_input_state_uninterpreted_data_end (const xcb_input_input_state_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->len - 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_input_state_next (xcb_input_input_state_iterator_t *i  /**< */)
{
    xcb_input_input_state_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_input_state_t *)(((char *)R) + xcb_input_input_state_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_input_state_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_input_state_end (xcb_input_input_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_input_state_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_input_query_device_state_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_query_device_state_reply_t *_aux = (xcb_input_query_device_state_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_input_query_device_state_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* classes */
    for(i=0; i<_aux->num_classes; i++) {
        xcb_tmp_len = xcb_input_input_state_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_input_input_state_t);
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

xcb_input_query_device_state_cookie_t
xcb_input_query_device_state (xcb_connection_t *c  /**< */,
                              uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_QUERY_DEVICE_STATE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_query_device_state_cookie_t xcb_ret;
    xcb_input_query_device_state_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_query_device_state_cookie_t
xcb_input_query_device_state_unchecked (xcb_connection_t *c  /**< */,
                                        uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_QUERY_DEVICE_STATE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_query_device_state_cookie_t xcb_ret;
    xcb_input_query_device_state_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_input_query_device_state_classes_length (const xcb_input_query_device_state_reply_t *R  /**< */)
{
    return R->num_classes;
}

xcb_input_input_state_iterator_t
xcb_input_query_device_state_classes_iterator (const xcb_input_query_device_state_reply_t *R  /**< */)
{
    xcb_input_input_state_iterator_t i;
    i.data = (xcb_input_input_state_t *) (R + 1);
    i.rem = R->num_classes;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_query_device_state_reply_t *
xcb_input_query_device_state_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_input_query_device_state_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */)
{
    return (xcb_input_query_device_state_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_send_extension_event_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_send_extension_event_request_t *_aux = (xcb_input_send_extension_event_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_send_extension_event_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* events */
    xcb_block_len += (_aux->num_events * 32) * sizeof(uint8_t);
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
    /* classes */
    xcb_block_len += _aux->num_classes * sizeof(xcb_input_event_class_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_event_class_t);
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
xcb_input_send_extension_event_checked (xcb_connection_t              *c  /**< */,
                                        xcb_window_t                   destination  /**< */,
                                        uint8_t                        device_id  /**< */,
                                        uint8_t                        propagate  /**< */,
                                        uint16_t                       num_classes  /**< */,
                                        uint8_t                        num_events  /**< */,
                                        const uint8_t                 *events  /**< */,
                                        const xcb_input_event_class_t *classes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_SEND_EXTENSION_EVENT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[8];
    xcb_void_cookie_t xcb_ret;
    xcb_input_send_extension_event_request_t xcb_out;

    xcb_out.destination = destination;
    xcb_out.device_id = device_id;
    xcb_out.propagate = propagate;
    xcb_out.num_classes = num_classes;
    xcb_out.num_events = num_events;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t events */
    xcb_parts[4].iov_base = (char *) events;
    xcb_parts[4].iov_len = (num_events * 32) * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_input_event_class_t classes */
    xcb_parts[6].iov_base = (char *) classes;
    xcb_parts[6].iov_len = num_classes * sizeof(xcb_input_event_class_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_send_extension_event (xcb_connection_t              *c  /**< */,
                                xcb_window_t                   destination  /**< */,
                                uint8_t                        device_id  /**< */,
                                uint8_t                        propagate  /**< */,
                                uint16_t                       num_classes  /**< */,
                                uint8_t                        num_events  /**< */,
                                const uint8_t                 *events  /**< */,
                                const xcb_input_event_class_t *classes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_SEND_EXTENSION_EVENT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[8];
    xcb_void_cookie_t xcb_ret;
    xcb_input_send_extension_event_request_t xcb_out;

    xcb_out.destination = destination;
    xcb_out.device_id = device_id;
    xcb_out.propagate = propagate;
    xcb_out.num_classes = num_classes;
    xcb_out.num_events = num_events;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t events */
    xcb_parts[4].iov_base = (char *) events;
    xcb_parts[4].iov_len = (num_events * 32) * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_input_event_class_t classes */
    xcb_parts[6].iov_base = (char *) classes;
    xcb_parts[6].iov_len = num_classes * sizeof(xcb_input_event_class_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_device_bell_checked (xcb_connection_t *c  /**< */,
                               uint8_t           device_id  /**< */,
                               uint8_t           feedback_id  /**< */,
                               uint8_t           feedback_class  /**< */,
                               int8_t            percent  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_DEVICE_BELL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_device_bell_request_t xcb_out;

    xcb_out.device_id = device_id;
    xcb_out.feedback_id = feedback_id;
    xcb_out.feedback_class = feedback_class;
    xcb_out.percent = percent;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_device_bell (xcb_connection_t *c  /**< */,
                       uint8_t           device_id  /**< */,
                       uint8_t           feedback_id  /**< */,
                       uint8_t           feedback_class  /**< */,
                       int8_t            percent  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_DEVICE_BELL,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_device_bell_request_t xcb_out;

    xcb_out.device_id = device_id;
    xcb_out.feedback_id = feedback_id;
    xcb_out.feedback_class = feedback_class;
    xcb_out.percent = percent;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_input_set_device_valuators_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_set_device_valuators_request_t *_aux = (xcb_input_set_device_valuators_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_set_device_valuators_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* valuators */
    xcb_block_len += _aux->num_valuators * sizeof(int32_t);
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

xcb_input_set_device_valuators_cookie_t
xcb_input_set_device_valuators (xcb_connection_t *c  /**< */,
                                uint8_t           device_id  /**< */,
                                uint8_t           first_valuator  /**< */,
                                uint8_t           num_valuators  /**< */,
                                const int32_t    *valuators  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_SET_DEVICE_VALUATORS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_input_set_device_valuators_cookie_t xcb_ret;
    xcb_input_set_device_valuators_request_t xcb_out;

    xcb_out.device_id = device_id;
    xcb_out.first_valuator = first_valuator;
    xcb_out.num_valuators = num_valuators;
    xcb_out.pad0 = 0;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* int32_t valuators */
    xcb_parts[4].iov_base = (char *) valuators;
    xcb_parts[4].iov_len = num_valuators * sizeof(int32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_set_device_valuators_cookie_t
xcb_input_set_device_valuators_unchecked (xcb_connection_t *c  /**< */,
                                          uint8_t           device_id  /**< */,
                                          uint8_t           first_valuator  /**< */,
                                          uint8_t           num_valuators  /**< */,
                                          const int32_t    *valuators  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_SET_DEVICE_VALUATORS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_input_set_device_valuators_cookie_t xcb_ret;
    xcb_input_set_device_valuators_request_t xcb_out;

    xcb_out.device_id = device_id;
    xcb_out.first_valuator = first_valuator;
    xcb_out.num_valuators = num_valuators;
    xcb_out.pad0 = 0;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* int32_t valuators */
    xcb_parts[4].iov_base = (char *) valuators;
    xcb_parts[4].iov_len = num_valuators * sizeof(int32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_set_device_valuators_reply_t *
xcb_input_set_device_valuators_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_input_set_device_valuators_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */)
{
    return (xcb_input_set_device_valuators_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_device_resolution_state_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_device_resolution_state_t *_aux = (xcb_input_device_resolution_state_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_device_resolution_state_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* resolution_values */
    xcb_block_len += _aux->num_valuators * sizeof(uint32_t);
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
    /* resolution_min */
    xcb_block_len += _aux->num_valuators * sizeof(uint32_t);
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
    /* resolution_max */
    xcb_block_len += _aux->num_valuators * sizeof(uint32_t);
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

uint32_t *
xcb_input_device_resolution_state_resolution_values (const xcb_input_device_resolution_state_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_input_device_resolution_state_resolution_values_length (const xcb_input_device_resolution_state_t *R  /**< */)
{
    return R->num_valuators;
}

xcb_generic_iterator_t
xcb_input_device_resolution_state_resolution_values_end (const xcb_input_device_resolution_state_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->num_valuators);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

uint32_t *
xcb_input_device_resolution_state_resolution_min (const xcb_input_device_resolution_state_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_input_device_resolution_state_resolution_values_end(R);
    return (uint32_t *) ((char *) prev.data + XCB_TYPE_PAD(uint32_t, prev.index) + 0);
}

int
xcb_input_device_resolution_state_resolution_min_length (const xcb_input_device_resolution_state_t *R  /**< */)
{
    return R->num_valuators;
}

xcb_generic_iterator_t
xcb_input_device_resolution_state_resolution_min_end (const xcb_input_device_resolution_state_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_input_device_resolution_state_resolution_values_end(R);
    i.data = ((uint32_t *) child.data) + (R->num_valuators);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

uint32_t *
xcb_input_device_resolution_state_resolution_max (const xcb_input_device_resolution_state_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_input_device_resolution_state_resolution_min_end(R);
    return (uint32_t *) ((char *) prev.data + XCB_TYPE_PAD(uint32_t, prev.index) + 0);
}

int
xcb_input_device_resolution_state_resolution_max_length (const xcb_input_device_resolution_state_t *R  /**< */)
{
    return R->num_valuators;
}

xcb_generic_iterator_t
xcb_input_device_resolution_state_resolution_max_end (const xcb_input_device_resolution_state_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_input_device_resolution_state_resolution_min_end(R);
    i.data = ((uint32_t *) child.data) + (R->num_valuators);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_device_resolution_state_next (xcb_input_device_resolution_state_iterator_t *i  /**< */)
{
    xcb_input_device_resolution_state_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_device_resolution_state_t *)(((char *)R) + xcb_input_device_resolution_state_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_device_resolution_state_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_device_resolution_state_end (xcb_input_device_resolution_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_device_resolution_state_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

void
xcb_input_device_abs_calib_state_next (xcb_input_device_abs_calib_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_abs_calib_state_t);
}

xcb_generic_iterator_t
xcb_input_device_abs_calib_state_end (xcb_input_device_abs_calib_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_device_abs_area_state_next (xcb_input_device_abs_area_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_abs_area_state_t);
}

xcb_generic_iterator_t
xcb_input_device_abs_area_state_end (xcb_input_device_abs_area_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_device_core_state_next (xcb_input_device_core_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_core_state_t);
}

xcb_generic_iterator_t
xcb_input_device_core_state_end (xcb_input_device_core_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_device_enable_state_next (xcb_input_device_enable_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_enable_state_t);
}

xcb_generic_iterator_t
xcb_input_device_enable_state_end (xcb_input_device_enable_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_device_state_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_device_state_t *_aux = (xcb_input_device_state_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_device_state_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* uninterpreted_data */
    xcb_block_len += (_aux->len - 4) * sizeof(uint8_t);
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

uint8_t *
xcb_input_device_state_uninterpreted_data (const xcb_input_device_state_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_input_device_state_uninterpreted_data_length (const xcb_input_device_state_t *R  /**< */)
{
    return (R->len - 4);
}

xcb_generic_iterator_t
xcb_input_device_state_uninterpreted_data_end (const xcb_input_device_state_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->len - 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_device_state_next (xcb_input_device_state_iterator_t *i  /**< */)
{
    xcb_input_device_state_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_device_state_t *)(((char *)R) + xcb_input_device_state_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_device_state_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_device_state_end (xcb_input_device_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_device_state_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_input_get_device_control_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_get_device_control_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* control */
    xcb_block_len += xcb_input_device_state_sizeof(xcb_tmp);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_device_state_t);
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

xcb_input_get_device_control_cookie_t
xcb_input_get_device_control (xcb_connection_t *c  /**< */,
                              uint16_t          control_id  /**< */,
                              uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_CONTROL,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_control_cookie_t xcb_ret;
    xcb_input_get_device_control_request_t xcb_out;

    xcb_out.control_id = control_id;
    xcb_out.device_id = device_id;
    xcb_out.pad0 = 0;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_get_device_control_cookie_t
xcb_input_get_device_control_unchecked (xcb_connection_t *c  /**< */,
                                        uint16_t          control_id  /**< */,
                                        uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_CONTROL,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_control_cookie_t xcb_ret;
    xcb_input_get_device_control_request_t xcb_out;

    xcb_out.control_id = control_id;
    xcb_out.device_id = device_id;
    xcb_out.pad0 = 0;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_device_state_t *
xcb_input_get_device_control_control (const xcb_input_get_device_control_reply_t *R  /**< */)
{
    return (xcb_input_device_state_t *) (R + 1);
}

xcb_input_get_device_control_reply_t *
xcb_input_get_device_control_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_input_get_device_control_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */)
{
    return (xcb_input_get_device_control_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_device_resolution_ctl_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_device_resolution_ctl_t *_aux = (xcb_input_device_resolution_ctl_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_device_resolution_ctl_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* resolution_values */
    xcb_block_len += _aux->num_valuators * sizeof(uint32_t);
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

uint32_t *
xcb_input_device_resolution_ctl_resolution_values (const xcb_input_device_resolution_ctl_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_input_device_resolution_ctl_resolution_values_length (const xcb_input_device_resolution_ctl_t *R  /**< */)
{
    return R->num_valuators;
}

xcb_generic_iterator_t
xcb_input_device_resolution_ctl_resolution_values_end (const xcb_input_device_resolution_ctl_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->num_valuators);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_device_resolution_ctl_next (xcb_input_device_resolution_ctl_iterator_t *i  /**< */)
{
    xcb_input_device_resolution_ctl_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_device_resolution_ctl_t *)(((char *)R) + xcb_input_device_resolution_ctl_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_device_resolution_ctl_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_device_resolution_ctl_end (xcb_input_device_resolution_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_device_resolution_ctl_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

void
xcb_input_device_abs_calib_ctl_next (xcb_input_device_abs_calib_ctl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_abs_calib_ctl_t);
}

xcb_generic_iterator_t
xcb_input_device_abs_calib_ctl_end (xcb_input_device_abs_calib_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_device_abs_area_ctrl_next (xcb_input_device_abs_area_ctrl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_abs_area_ctrl_t);
}

xcb_generic_iterator_t
xcb_input_device_abs_area_ctrl_end (xcb_input_device_abs_area_ctrl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_device_core_ctrl_next (xcb_input_device_core_ctrl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_core_ctrl_t);
}

xcb_generic_iterator_t
xcb_input_device_core_ctrl_end (xcb_input_device_core_ctrl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_device_enable_ctrl_next (xcb_input_device_enable_ctrl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_enable_ctrl_t);
}

xcb_generic_iterator_t
xcb_input_device_enable_ctrl_end (xcb_input_device_enable_ctrl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_device_ctl_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_device_ctl_t *_aux = (xcb_input_device_ctl_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_device_ctl_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* uninterpreted_data */
    xcb_block_len += (_aux->len - 4) * sizeof(uint8_t);
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

uint8_t *
xcb_input_device_ctl_uninterpreted_data (const xcb_input_device_ctl_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_input_device_ctl_uninterpreted_data_length (const xcb_input_device_ctl_t *R  /**< */)
{
    return (R->len - 4);
}

xcb_generic_iterator_t
xcb_input_device_ctl_uninterpreted_data_end (const xcb_input_device_ctl_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->len - 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_device_ctl_next (xcb_input_device_ctl_iterator_t *i  /**< */)
{
    xcb_input_device_ctl_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_device_ctl_t *)(((char *)R) + xcb_input_device_ctl_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_device_ctl_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_device_ctl_end (xcb_input_device_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_device_ctl_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_input_change_device_control_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_change_device_control_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* control */
    xcb_block_len += xcb_input_device_ctl_sizeof(xcb_tmp);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_device_ctl_t);
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

xcb_input_change_device_control_cookie_t
xcb_input_change_device_control (xcb_connection_t       *c  /**< */,
                                 uint16_t                control_id  /**< */,
                                 uint8_t                 device_id  /**< */,
                                 xcb_input_device_ctl_t *control  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_DEVICE_CONTROL,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_input_change_device_control_cookie_t xcb_ret;
    xcb_input_change_device_control_request_t xcb_out;

    xcb_out.control_id = control_id;
    xcb_out.device_id = device_id;
    xcb_out.pad0 = 0;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_device_ctl_t control */
    xcb_parts[4].iov_base = (char *) control;
    xcb_parts[4].iov_len =
      xcb_input_device_ctl_sizeof (control);

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_change_device_control_cookie_t
xcb_input_change_device_control_unchecked (xcb_connection_t       *c  /**< */,
                                           uint16_t                control_id  /**< */,
                                           uint8_t                 device_id  /**< */,
                                           xcb_input_device_ctl_t *control  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_DEVICE_CONTROL,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_input_change_device_control_cookie_t xcb_ret;
    xcb_input_change_device_control_request_t xcb_out;

    xcb_out.control_id = control_id;
    xcb_out.device_id = device_id;
    xcb_out.pad0 = 0;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_device_ctl_t control */
    xcb_parts[4].iov_base = (char *) control;
    xcb_parts[4].iov_len =
      xcb_input_device_ctl_sizeof (control);

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_change_device_control_reply_t *
xcb_input_change_device_control_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_input_change_device_control_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */)
{
    return (xcb_input_change_device_control_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_list_device_properties_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_list_device_properties_reply_t *_aux = (xcb_input_list_device_properties_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_list_device_properties_reply_t);
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

xcb_input_list_device_properties_cookie_t
xcb_input_list_device_properties (xcb_connection_t *c  /**< */,
                                  uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_LIST_DEVICE_PROPERTIES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_list_device_properties_cookie_t xcb_ret;
    xcb_input_list_device_properties_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_list_device_properties_cookie_t
xcb_input_list_device_properties_unchecked (xcb_connection_t *c  /**< */,
                                            uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_LIST_DEVICE_PROPERTIES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_list_device_properties_cookie_t xcb_ret;
    xcb_input_list_device_properties_request_t xcb_out;

    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_atom_t *
xcb_input_list_device_properties_atoms (const xcb_input_list_device_properties_reply_t *R  /**< */)
{
    return (xcb_atom_t *) (R + 1);
}

int
xcb_input_list_device_properties_atoms_length (const xcb_input_list_device_properties_reply_t *R  /**< */)
{
    return R->num_atoms;
}

xcb_generic_iterator_t
xcb_input_list_device_properties_atoms_end (const xcb_input_list_device_properties_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_atom_t *) (R + 1)) + (R->num_atoms);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_list_device_properties_reply_t *
xcb_input_list_device_properties_reply (xcb_connection_t                           *c  /**< */,
                                        xcb_input_list_device_properties_cookie_t   cookie  /**< */,
                                        xcb_generic_error_t                       **e  /**< */)
{
    return (xcb_input_list_device_properties_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

uint8_t *
xcb_input_change_device_property_items_data_8 (const xcb_input_change_device_property_items_t *S  /**< */)
{
    return /* items */ S->data8;
}

int
xcb_input_change_device_property_items_data_8_length (const xcb_input_change_device_property_request_t *R  /**< */,
                                                      const xcb_input_change_device_property_items_t *S  /**< */)
{
    return R->num_items;
}

xcb_generic_iterator_t
xcb_input_change_device_property_items_data_8_end (const xcb_input_change_device_property_request_t *R  /**< */,
                                                   const xcb_input_change_device_property_items_t *S  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = /* items */ S->data8 + R->num_items;
    i.rem = 0;
    i.index = (char *) i.data - (char *) S;
    return i;
}

uint16_t *
xcb_input_change_device_property_items_data_16 (const xcb_input_change_device_property_items_t *S  /**< */)
{
    return /* items */ S->data16;
}

int
xcb_input_change_device_property_items_data_16_length (const xcb_input_change_device_property_request_t *R  /**< */,
                                                       const xcb_input_change_device_property_items_t *S  /**< */)
{
    return R->num_items;
}

xcb_generic_iterator_t
xcb_input_change_device_property_items_data_16_end (const xcb_input_change_device_property_request_t *R  /**< */,
                                                    const xcb_input_change_device_property_items_t *S  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = /* items */ S->data16 + R->num_items;
    i.rem = 0;
    i.index = (char *) i.data - (char *) S;
    return i;
}

uint32_t *
xcb_input_change_device_property_items_data_32 (const xcb_input_change_device_property_items_t *S  /**< */)
{
    return /* items */ S->data32;
}

int
xcb_input_change_device_property_items_data_32_length (const xcb_input_change_device_property_request_t *R  /**< */,
                                                       const xcb_input_change_device_property_items_t *S  /**< */)
{
    return R->num_items;
}

xcb_generic_iterator_t
xcb_input_change_device_property_items_data_32_end (const xcb_input_change_device_property_request_t *R  /**< */,
                                                    const xcb_input_change_device_property_items_t *S  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = /* items */ S->data32 + R->num_items;
    i.rem = 0;
    i.index = (char *) i.data - (char *) S;
    return i;
}

int
xcb_input_change_device_property_items_serialize (void                                           **_buffer  /**< */,
                                                  uint32_t                                         num_items  /**< */,
                                                  uint8_t                                          format  /**< */,
                                                  const xcb_input_change_device_property_items_t  *_aux  /**< */)
{
    char *xcb_out = *_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_align_to = 0;

    unsigned int xcb_pad = 0;
    char xcb_pad0[3] = {0, 0, 0};
    struct iovec xcb_parts[7];
    unsigned int xcb_parts_idx = 0;
    unsigned int xcb_block_len = 0;
    unsigned int i;
    char *xcb_tmp;

    if(format & XCB_INPUT_PROPERTY_FORMAT_8_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
            xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
            xcb_parts_idx++;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data8 */
        xcb_parts[xcb_parts_idx].iov_base = (char *) _aux->data8;
        xcb_block_len += num_items * sizeof(uint8_t);
        xcb_parts[xcb_parts_idx].iov_len = num_items * sizeof(uint8_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint8_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_16_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
            xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
            xcb_parts_idx++;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data16 */
        xcb_parts[xcb_parts_idx].iov_base = (char *) _aux->data16;
        xcb_block_len += num_items * sizeof(uint16_t);
        xcb_parts[xcb_parts_idx].iov_len = num_items * sizeof(uint16_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint16_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_32_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
            xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
            xcb_parts_idx++;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data32 */
        xcb_parts[xcb_parts_idx].iov_base = (char *) _aux->data32;
        xcb_block_len += num_items * sizeof(uint32_t);
        xcb_parts[xcb_parts_idx].iov_len = num_items * sizeof(uint32_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint32_t);
    }
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
        xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
        xcb_parts_idx++;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    if (NULL == xcb_out) {
        /* allocate memory */
        xcb_out = malloc(xcb_buffer_len);
        *_buffer = xcb_out;
    }

    xcb_tmp = xcb_out;
    for(i=0; i<xcb_parts_idx; i++) {
        if (0 != xcb_parts[i].iov_base && 0 != xcb_parts[i].iov_len)
            memcpy(xcb_tmp, xcb_parts[i].iov_base, xcb_parts[i].iov_len);
        if (0 != xcb_parts[i].iov_len)
            xcb_tmp += xcb_parts[i].iov_len;
    }

    return xcb_buffer_len;
}

int
xcb_input_change_device_property_items_unpack (const void                                *_buffer  /**< */,
                                               uint32_t                                   num_items  /**< */,
                                               uint8_t                                    format  /**< */,
                                               xcb_input_change_device_property_items_t  *_aux  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    if(format & XCB_INPUT_PROPERTY_FORMAT_8_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_tmp += xcb_pad;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data8 */
        _aux->data8 = (uint8_t *)xcb_tmp;
        xcb_block_len += num_items * sizeof(uint8_t);
        xcb_tmp += xcb_block_len;
        xcb_align_to = ALIGNOF(uint8_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_16_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_tmp += xcb_pad;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data16 */
        _aux->data16 = (uint16_t *)xcb_tmp;
        xcb_block_len += num_items * sizeof(uint16_t);
        xcb_tmp += xcb_block_len;
        xcb_align_to = ALIGNOF(uint16_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_32_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_tmp += xcb_pad;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data32 */
        _aux->data32 = (uint32_t *)xcb_tmp;
        xcb_block_len += num_items * sizeof(uint32_t);
        xcb_tmp += xcb_block_len;
        xcb_align_to = ALIGNOF(uint32_t);
    }
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
xcb_input_change_device_property_items_sizeof (const void  *_buffer  /**< */,
                                               uint32_t     num_items  /**< */,
                                               uint8_t      format  /**< */)
{
    xcb_input_change_device_property_items_t _aux;
    return xcb_input_change_device_property_items_unpack(_buffer, num_items, format, &_aux);
}

xcb_void_cookie_t
xcb_input_change_device_property_checked (xcb_connection_t *c  /**< */,
                                          xcb_atom_t        property  /**< */,
                                          xcb_atom_t        type  /**< */,
                                          uint8_t           device_id  /**< */,
                                          uint8_t           format  /**< */,
                                          uint8_t           mode  /**< */,
                                          uint32_t          num_items  /**< */,
                                          const void       *items  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_DEVICE_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_input_change_device_property_request_t xcb_out;

    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.device_id = device_id;
    xcb_out.format = format;
    xcb_out.mode = mode;
    xcb_out.pad0 = 0;
    xcb_out.num_items = num_items;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_change_device_property_items_t items */
    xcb_parts[4].iov_base = (char *) items;
    xcb_parts[4].iov_len =
      xcb_input_change_device_property_items_sizeof (items, num_items, format);

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_change_device_property (xcb_connection_t *c  /**< */,
                                  xcb_atom_t        property  /**< */,
                                  xcb_atom_t        type  /**< */,
                                  uint8_t           device_id  /**< */,
                                  uint8_t           format  /**< */,
                                  uint8_t           mode  /**< */,
                                  uint32_t          num_items  /**< */,
                                  const void       *items  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_DEVICE_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_input_change_device_property_request_t xcb_out;

    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.device_id = device_id;
    xcb_out.format = format;
    xcb_out.mode = mode;
    xcb_out.pad0 = 0;
    xcb_out.num_items = num_items;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_change_device_property_items_t items */
    xcb_parts[4].iov_base = (char *) items;
    xcb_parts[4].iov_len =
      xcb_input_change_device_property_items_sizeof (items, num_items, format);

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_change_device_property_aux_checked (xcb_connection_t                               *c  /**< */,
                                              xcb_atom_t                                      property  /**< */,
                                              xcb_atom_t                                      type  /**< */,
                                              uint8_t                                         device_id  /**< */,
                                              uint8_t                                         format  /**< */,
                                              uint8_t                                         mode  /**< */,
                                              uint32_t                                        num_items  /**< */,
                                              const xcb_input_change_device_property_items_t *items  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_DEVICE_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_input_change_device_property_request_t xcb_out;
    void *xcb_aux0 = 0;

    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.device_id = device_id;
    xcb_out.format = format;
    xcb_out.mode = mode;
    xcb_out.pad0 = 0;
    xcb_out.num_items = num_items;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_change_device_property_items_t items */
    xcb_parts[4].iov_len =
      xcb_input_change_device_property_items_serialize (&xcb_aux0, num_items, format, items);
    xcb_parts[4].iov_base = xcb_aux0;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    free(xcb_aux0);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_change_device_property_aux (xcb_connection_t                               *c  /**< */,
                                      xcb_atom_t                                      property  /**< */,
                                      xcb_atom_t                                      type  /**< */,
                                      uint8_t                                         device_id  /**< */,
                                      uint8_t                                         format  /**< */,
                                      uint8_t                                         mode  /**< */,
                                      uint32_t                                        num_items  /**< */,
                                      const xcb_input_change_device_property_items_t *items  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_CHANGE_DEVICE_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_input_change_device_property_request_t xcb_out;
    void *xcb_aux0 = 0;

    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.device_id = device_id;
    xcb_out.format = format;
    xcb_out.mode = mode;
    xcb_out.pad0 = 0;
    xcb_out.num_items = num_items;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_change_device_property_items_t items */
    xcb_parts[4].iov_len =
      xcb_input_change_device_property_items_serialize (&xcb_aux0, num_items, format, items);
    xcb_parts[4].iov_base = xcb_aux0;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    free(xcb_aux0);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_delete_device_property_checked (xcb_connection_t *c  /**< */,
                                          xcb_atom_t        property  /**< */,
                                          uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_DELETE_DEVICE_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_delete_device_property_request_t xcb_out;

    xcb_out.property = property;
    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_delete_device_property (xcb_connection_t *c  /**< */,
                                  xcb_atom_t        property  /**< */,
                                  uint8_t           device_id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_DELETE_DEVICE_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_delete_device_property_request_t xcb_out;

    xcb_out.property = property;
    xcb_out.device_id = device_id;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_input_get_device_property_items_data_8 (const xcb_input_get_device_property_items_t *S  /**< */)
{
    return /* items */ S->data8;
}

int
xcb_input_get_device_property_items_data_8_length (const xcb_input_get_device_property_reply_t *R  /**< */,
                                                   const xcb_input_get_device_property_items_t *S  /**< */)
{
    return R->num_items;
}

xcb_generic_iterator_t
xcb_input_get_device_property_items_data_8_end (const xcb_input_get_device_property_reply_t *R  /**< */,
                                                const xcb_input_get_device_property_items_t *S  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = /* items */ S->data8 + R->num_items;
    i.rem = 0;
    i.index = (char *) i.data - (char *) S;
    return i;
}

uint16_t *
xcb_input_get_device_property_items_data_16 (const xcb_input_get_device_property_items_t *S  /**< */)
{
    return /* items */ S->data16;
}

int
xcb_input_get_device_property_items_data_16_length (const xcb_input_get_device_property_reply_t *R  /**< */,
                                                    const xcb_input_get_device_property_items_t *S  /**< */)
{
    return R->num_items;
}

xcb_generic_iterator_t
xcb_input_get_device_property_items_data_16_end (const xcb_input_get_device_property_reply_t *R  /**< */,
                                                 const xcb_input_get_device_property_items_t *S  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = /* items */ S->data16 + R->num_items;
    i.rem = 0;
    i.index = (char *) i.data - (char *) S;
    return i;
}

uint32_t *
xcb_input_get_device_property_items_data_32 (const xcb_input_get_device_property_items_t *S  /**< */)
{
    return /* items */ S->data32;
}

int
xcb_input_get_device_property_items_data_32_length (const xcb_input_get_device_property_reply_t *R  /**< */,
                                                    const xcb_input_get_device_property_items_t *S  /**< */)
{
    return R->num_items;
}

xcb_generic_iterator_t
xcb_input_get_device_property_items_data_32_end (const xcb_input_get_device_property_reply_t *R  /**< */,
                                                 const xcb_input_get_device_property_items_t *S  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = /* items */ S->data32 + R->num_items;
    i.rem = 0;
    i.index = (char *) i.data - (char *) S;
    return i;
}

int
xcb_input_get_device_property_items_serialize (void                                        **_buffer  /**< */,
                                               uint32_t                                      num_items  /**< */,
                                               uint8_t                                       format  /**< */,
                                               const xcb_input_get_device_property_items_t  *_aux  /**< */)
{
    char *xcb_out = *_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_align_to = 0;

    unsigned int xcb_pad = 0;
    char xcb_pad0[3] = {0, 0, 0};
    struct iovec xcb_parts[7];
    unsigned int xcb_parts_idx = 0;
    unsigned int xcb_block_len = 0;
    unsigned int i;
    char *xcb_tmp;

    if(format & XCB_INPUT_PROPERTY_FORMAT_8_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
            xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
            xcb_parts_idx++;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data8 */
        xcb_parts[xcb_parts_idx].iov_base = (char *) _aux->data8;
        xcb_block_len += num_items * sizeof(uint8_t);
        xcb_parts[xcb_parts_idx].iov_len = num_items * sizeof(uint8_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint8_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_16_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
            xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
            xcb_parts_idx++;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data16 */
        xcb_parts[xcb_parts_idx].iov_base = (char *) _aux->data16;
        xcb_block_len += num_items * sizeof(uint16_t);
        xcb_parts[xcb_parts_idx].iov_len = num_items * sizeof(uint16_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint16_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_32_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
            xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
            xcb_parts_idx++;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data32 */
        xcb_parts[xcb_parts_idx].iov_base = (char *) _aux->data32;
        xcb_block_len += num_items * sizeof(uint32_t);
        xcb_parts[xcb_parts_idx].iov_len = num_items * sizeof(uint32_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint32_t);
    }
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
        xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
        xcb_parts_idx++;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    if (NULL == xcb_out) {
        /* allocate memory */
        xcb_out = malloc(xcb_buffer_len);
        *_buffer = xcb_out;
    }

    xcb_tmp = xcb_out;
    for(i=0; i<xcb_parts_idx; i++) {
        if (0 != xcb_parts[i].iov_base && 0 != xcb_parts[i].iov_len)
            memcpy(xcb_tmp, xcb_parts[i].iov_base, xcb_parts[i].iov_len);
        if (0 != xcb_parts[i].iov_len)
            xcb_tmp += xcb_parts[i].iov_len;
    }

    return xcb_buffer_len;
}

int
xcb_input_get_device_property_items_unpack (const void                             *_buffer  /**< */,
                                            uint32_t                                num_items  /**< */,
                                            uint8_t                                 format  /**< */,
                                            xcb_input_get_device_property_items_t  *_aux  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    if(format & XCB_INPUT_PROPERTY_FORMAT_8_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_tmp += xcb_pad;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data8 */
        _aux->data8 = (uint8_t *)xcb_tmp;
        xcb_block_len += num_items * sizeof(uint8_t);
        xcb_tmp += xcb_block_len;
        xcb_align_to = ALIGNOF(uint8_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_16_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_tmp += xcb_pad;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data16 */
        _aux->data16 = (uint16_t *)xcb_tmp;
        xcb_block_len += num_items * sizeof(uint16_t);
        xcb_tmp += xcb_block_len;
        xcb_align_to = ALIGNOF(uint16_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_32_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_tmp += xcb_pad;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data32 */
        _aux->data32 = (uint32_t *)xcb_tmp;
        xcb_block_len += num_items * sizeof(uint32_t);
        xcb_tmp += xcb_block_len;
        xcb_align_to = ALIGNOF(uint32_t);
    }
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
xcb_input_get_device_property_items_sizeof (const void  *_buffer  /**< */,
                                            uint32_t     num_items  /**< */,
                                            uint8_t      format  /**< */)
{
    xcb_input_get_device_property_items_t _aux;
    return xcb_input_get_device_property_items_unpack(_buffer, num_items, format, &_aux);
}

xcb_input_get_device_property_cookie_t
xcb_input_get_device_property (xcb_connection_t *c  /**< */,
                               xcb_atom_t        property  /**< */,
                               xcb_atom_t        type  /**< */,
                               uint32_t          offset  /**< */,
                               uint32_t          len  /**< */,
                               uint8_t           device_id  /**< */,
                               uint8_t           _delete  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_PROPERTY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_property_cookie_t xcb_ret;
    xcb_input_get_device_property_request_t xcb_out;

    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.offset = offset;
    xcb_out.len = len;
    xcb_out.device_id = device_id;
    xcb_out._delete = _delete;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_get_device_property_cookie_t
xcb_input_get_device_property_unchecked (xcb_connection_t *c  /**< */,
                                         xcb_atom_t        property  /**< */,
                                         xcb_atom_t        type  /**< */,
                                         uint32_t          offset  /**< */,
                                         uint32_t          len  /**< */,
                                         uint8_t           device_id  /**< */,
                                         uint8_t           _delete  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_GET_DEVICE_PROPERTY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_get_device_property_cookie_t xcb_ret;
    xcb_input_get_device_property_request_t xcb_out;

    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.offset = offset;
    xcb_out.len = len;
    xcb_out.device_id = device_id;
    xcb_out._delete = _delete;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

void *
xcb_input_get_device_property_items (const xcb_input_get_device_property_reply_t *R  /**< */)
{
    return (void *) (R + 1);
}

xcb_input_get_device_property_reply_t *
xcb_input_get_device_property_reply (xcb_connection_t                        *c  /**< */,
                                     xcb_input_get_device_property_cookie_t   cookie  /**< */,
                                     xcb_generic_error_t                    **e  /**< */)
{
    return (xcb_input_get_device_property_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

void
xcb_input_group_info_next (xcb_input_group_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_group_info_t);
}

xcb_generic_iterator_t
xcb_input_group_info_end (xcb_input_group_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_modifier_info_next (xcb_input_modifier_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_modifier_info_t);
}

xcb_generic_iterator_t
xcb_input_modifier_info_end (xcb_input_modifier_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_xi_query_pointer_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_xi_query_pointer_reply_t *_aux = (xcb_input_xi_query_pointer_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_xi_query_pointer_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* buttons */
    xcb_block_len += _aux->buttons_len * sizeof(uint32_t);
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

xcb_input_xi_query_pointer_cookie_t
xcb_input_xi_query_pointer (xcb_connection_t      *c  /**< */,
                            xcb_window_t           window  /**< */,
                            xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_QUERY_POINTER,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_query_pointer_cookie_t xcb_ret;
    xcb_input_xi_query_pointer_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_query_pointer_cookie_t
xcb_input_xi_query_pointer_unchecked (xcb_connection_t      *c  /**< */,
                                      xcb_window_t           window  /**< */,
                                      xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_QUERY_POINTER,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_query_pointer_cookie_t xcb_ret;
    xcb_input_xi_query_pointer_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_input_xi_query_pointer_buttons (const xcb_input_xi_query_pointer_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_input_xi_query_pointer_buttons_length (const xcb_input_xi_query_pointer_reply_t *R  /**< */)
{
    return R->buttons_len;
}

xcb_generic_iterator_t
xcb_input_xi_query_pointer_buttons_end (const xcb_input_xi_query_pointer_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->buttons_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_xi_query_pointer_reply_t *
xcb_input_xi_query_pointer_reply (xcb_connection_t                     *c  /**< */,
                                  xcb_input_xi_query_pointer_cookie_t   cookie  /**< */,
                                  xcb_generic_error_t                 **e  /**< */)
{
    return (xcb_input_xi_query_pointer_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_input_xi_warp_pointer_checked (xcb_connection_t      *c  /**< */,
                                   xcb_window_t           src_win  /**< */,
                                   xcb_window_t           dst_win  /**< */,
                                   xcb_input_fp1616_t     src_x  /**< */,
                                   xcb_input_fp1616_t     src_y  /**< */,
                                   uint16_t               src_width  /**< */,
                                   uint16_t               src_height  /**< */,
                                   xcb_input_fp1616_t     dst_x  /**< */,
                                   xcb_input_fp1616_t     dst_y  /**< */,
                                   xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_WARP_POINTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_warp_pointer_request_t xcb_out;

    xcb_out.src_win = src_win;
    xcb_out.dst_win = dst_win;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.src_width = src_width;
    xcb_out.src_height = src_height;
    xcb_out.dst_x = dst_x;
    xcb_out.dst_y = dst_y;
    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_warp_pointer (xcb_connection_t      *c  /**< */,
                           xcb_window_t           src_win  /**< */,
                           xcb_window_t           dst_win  /**< */,
                           xcb_input_fp1616_t     src_x  /**< */,
                           xcb_input_fp1616_t     src_y  /**< */,
                           uint16_t               src_width  /**< */,
                           uint16_t               src_height  /**< */,
                           xcb_input_fp1616_t     dst_x  /**< */,
                           xcb_input_fp1616_t     dst_y  /**< */,
                           xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_WARP_POINTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_warp_pointer_request_t xcb_out;

    xcb_out.src_win = src_win;
    xcb_out.dst_win = dst_win;
    xcb_out.src_x = src_x;
    xcb_out.src_y = src_y;
    xcb_out.src_width = src_width;
    xcb_out.src_height = src_height;
    xcb_out.dst_x = dst_x;
    xcb_out.dst_y = dst_y;
    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_change_cursor_checked (xcb_connection_t      *c  /**< */,
                                    xcb_window_t           window  /**< */,
                                    xcb_cursor_t           cursor  /**< */,
                                    xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_CHANGE_CURSOR,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_change_cursor_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.cursor = cursor;
    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_change_cursor (xcb_connection_t      *c  /**< */,
                            xcb_window_t           window  /**< */,
                            xcb_cursor_t           cursor  /**< */,
                            xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_CHANGE_CURSOR,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_change_cursor_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.cursor = cursor;
    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_input_add_master_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_add_master_t *_aux = (xcb_input_add_master_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_add_master_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
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

char *
xcb_input_add_master_name (const xcb_input_add_master_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_input_add_master_name_length (const xcb_input_add_master_t *R  /**< */)
{
    return R->name_len;
}

xcb_generic_iterator_t
xcb_input_add_master_name_end (const xcb_input_add_master_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->name_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_add_master_next (xcb_input_add_master_iterator_t *i  /**< */)
{
    xcb_input_add_master_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_add_master_t *)(((char *)R) + xcb_input_add_master_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_add_master_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_add_master_end (xcb_input_add_master_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_add_master_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

void
xcb_input_remove_master_next (xcb_input_remove_master_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_remove_master_t);
}

xcb_generic_iterator_t
xcb_input_remove_master_end (xcb_input_remove_master_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_attach_slave_next (xcb_input_attach_slave_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_attach_slave_t);
}

xcb_generic_iterator_t
xcb_input_attach_slave_end (xcb_input_attach_slave_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_detach_slave_next (xcb_input_detach_slave_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_detach_slave_t);
}

xcb_generic_iterator_t
xcb_input_detach_slave_end (xcb_input_detach_slave_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_hierarchy_change_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_hierarchy_change_t *_aux = (xcb_input_hierarchy_change_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_hierarchy_change_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* uninterpreted_data */
    xcb_block_len += ((_aux->len * 4) - 4) * sizeof(uint8_t);
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

uint8_t *
xcb_input_hierarchy_change_uninterpreted_data (const xcb_input_hierarchy_change_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_input_hierarchy_change_uninterpreted_data_length (const xcb_input_hierarchy_change_t *R  /**< */)
{
    return ((R->len * 4) - 4);
}

xcb_generic_iterator_t
xcb_input_hierarchy_change_uninterpreted_data_end (const xcb_input_hierarchy_change_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + (((R->len * 4) - 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_hierarchy_change_next (xcb_input_hierarchy_change_iterator_t *i  /**< */)
{
    xcb_input_hierarchy_change_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_hierarchy_change_t *)(((char *)R) + xcb_input_hierarchy_change_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_hierarchy_change_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_hierarchy_change_end (xcb_input_hierarchy_change_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_hierarchy_change_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_input_xi_change_hierarchy_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_xi_change_hierarchy_request_t *_aux = (xcb_input_xi_change_hierarchy_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_input_xi_change_hierarchy_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* changes */
    for(i=0; i<_aux->num_changes; i++) {
        xcb_tmp_len = xcb_input_hierarchy_change_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_input_hierarchy_change_t);
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
xcb_input_xi_change_hierarchy_checked (xcb_connection_t                   *c  /**< */,
                                       uint8_t                             num_changes  /**< */,
                                       const xcb_input_hierarchy_change_t *changes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_CHANGE_HIERARCHY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_change_hierarchy_request_t xcb_out;
    unsigned int i;
    unsigned int xcb_tmp_len;
    char *xcb_tmp;

    xcb_out.num_changes = num_changes;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_hierarchy_change_t changes */
    xcb_parts[4].iov_base = (char *) changes;
    xcb_parts[4].iov_len = 0;
    xcb_tmp = (char *)changes;
    for(i=0; i<num_changes; i++) {
        xcb_tmp_len = xcb_input_hierarchy_change_sizeof(xcb_tmp);
        xcb_parts[4].iov_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_change_hierarchy (xcb_connection_t                   *c  /**< */,
                               uint8_t                             num_changes  /**< */,
                               const xcb_input_hierarchy_change_t *changes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_CHANGE_HIERARCHY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_change_hierarchy_request_t xcb_out;
    unsigned int i;
    unsigned int xcb_tmp_len;
    char *xcb_tmp;

    xcb_out.num_changes = num_changes;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_hierarchy_change_t changes */
    xcb_parts[4].iov_base = (char *) changes;
    xcb_parts[4].iov_len = 0;
    xcb_tmp = (char *)changes;
    for(i=0; i<num_changes; i++) {
        xcb_tmp_len = xcb_input_hierarchy_change_sizeof(xcb_tmp);
        xcb_parts[4].iov_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_set_client_pointer_checked (xcb_connection_t      *c  /**< */,
                                         xcb_window_t           window  /**< */,
                                         xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_SET_CLIENT_POINTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_set_client_pointer_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_set_client_pointer (xcb_connection_t      *c  /**< */,
                                 xcb_window_t           window  /**< */,
                                 xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_SET_CLIENT_POINTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_set_client_pointer_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_get_client_pointer_cookie_t
xcb_input_xi_get_client_pointer (xcb_connection_t *c  /**< */,
                                 xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_GET_CLIENT_POINTER,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_get_client_pointer_cookie_t xcb_ret;
    xcb_input_xi_get_client_pointer_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_get_client_pointer_cookie_t
xcb_input_xi_get_client_pointer_unchecked (xcb_connection_t *c  /**< */,
                                           xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_GET_CLIENT_POINTER,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_get_client_pointer_cookie_t xcb_ret;
    xcb_input_xi_get_client_pointer_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_get_client_pointer_reply_t *
xcb_input_xi_get_client_pointer_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_input_xi_get_client_pointer_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */)
{
    return (xcb_input_xi_get_client_pointer_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_event_mask_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_event_mask_t *_aux = (xcb_input_event_mask_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_event_mask_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* mask */
    xcb_block_len += _aux->mask_len * sizeof(uint32_t);
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

uint32_t *
xcb_input_event_mask_mask (const xcb_input_event_mask_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_input_event_mask_mask_length (const xcb_input_event_mask_t *R  /**< */)
{
    return R->mask_len;
}

xcb_generic_iterator_t
xcb_input_event_mask_mask_end (const xcb_input_event_mask_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->mask_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_event_mask_next (xcb_input_event_mask_iterator_t *i  /**< */)
{
    xcb_input_event_mask_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_event_mask_t *)(((char *)R) + xcb_input_event_mask_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_event_mask_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_event_mask_end (xcb_input_event_mask_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_event_mask_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_input_xi_select_events_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_xi_select_events_request_t *_aux = (xcb_input_xi_select_events_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_input_xi_select_events_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* masks */
    for(i=0; i<_aux->num_mask; i++) {
        xcb_tmp_len = xcb_input_event_mask_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_input_event_mask_t);
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
xcb_input_xi_select_events_checked (xcb_connection_t             *c  /**< */,
                                    xcb_window_t                  window  /**< */,
                                    uint16_t                      num_mask  /**< */,
                                    const xcb_input_event_mask_t *masks  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_SELECT_EVENTS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_select_events_request_t xcb_out;
    unsigned int i;
    unsigned int xcb_tmp_len;
    char *xcb_tmp;

    xcb_out.window = window;
    xcb_out.num_mask = num_mask;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_event_mask_t masks */
    xcb_parts[4].iov_base = (char *) masks;
    xcb_parts[4].iov_len = 0;
    xcb_tmp = (char *)masks;
    for(i=0; i<num_mask; i++) {
        xcb_tmp_len = xcb_input_event_mask_sizeof(xcb_tmp);
        xcb_parts[4].iov_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_select_events (xcb_connection_t             *c  /**< */,
                            xcb_window_t                  window  /**< */,
                            uint16_t                      num_mask  /**< */,
                            const xcb_input_event_mask_t *masks  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_SELECT_EVENTS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_select_events_request_t xcb_out;
    unsigned int i;
    unsigned int xcb_tmp_len;
    char *xcb_tmp;

    xcb_out.window = window;
    xcb_out.num_mask = num_mask;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_event_mask_t masks */
    xcb_parts[4].iov_base = (char *) masks;
    xcb_parts[4].iov_len = 0;
    xcb_tmp = (char *)masks;
    for(i=0; i<num_mask; i++) {
        xcb_tmp_len = xcb_input_event_mask_sizeof(xcb_tmp);
        xcb_parts[4].iov_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_query_version_cookie_t
xcb_input_xi_query_version (xcb_connection_t *c  /**< */,
                            uint16_t          major_version  /**< */,
                            uint16_t          minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_query_version_cookie_t xcb_ret;
    xcb_input_xi_query_version_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_query_version_cookie_t
xcb_input_xi_query_version_unchecked (xcb_connection_t *c  /**< */,
                                      uint16_t          major_version  /**< */,
                                      uint16_t          minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_query_version_cookie_t xcb_ret;
    xcb_input_xi_query_version_request_t xcb_out;

    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_query_version_reply_t *
xcb_input_xi_query_version_reply (xcb_connection_t                     *c  /**< */,
                                  xcb_input_xi_query_version_cookie_t   cookie  /**< */,
                                  xcb_generic_error_t                 **e  /**< */)
{
    return (xcb_input_xi_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_button_class_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_button_class_t *_aux = (xcb_input_button_class_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_button_class_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* state */
    xcb_block_len += ((_aux->num_buttons + 31) / 32) * sizeof(xcb_atom_t);
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
    /* labels */
    xcb_block_len += _aux->num_buttons * sizeof(xcb_atom_t);
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

uint32_t *
xcb_input_button_class_state (const xcb_input_button_class_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_input_button_class_state_length (const xcb_input_button_class_t *R  /**< */)
{
    return ((R->num_buttons + 31) / 32);
}

xcb_generic_iterator_t
xcb_input_button_class_state_end (const xcb_input_button_class_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (((R->num_buttons + 31) / 32));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_atom_t *
xcb_input_button_class_labels (const xcb_input_button_class_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_input_button_class_state_end(R);
    return (xcb_atom_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_atom_t, prev.index) + 0);
}

int
xcb_input_button_class_labels_length (const xcb_input_button_class_t *R  /**< */)
{
    return R->num_buttons;
}

xcb_generic_iterator_t
xcb_input_button_class_labels_end (const xcb_input_button_class_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_input_button_class_state_end(R);
    i.data = ((xcb_atom_t *) child.data) + (R->num_buttons);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_button_class_next (xcb_input_button_class_iterator_t *i  /**< */)
{
    xcb_input_button_class_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_button_class_t *)(((char *)R) + xcb_input_button_class_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_button_class_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_button_class_end (xcb_input_button_class_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_button_class_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_input_key_class_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_key_class_t *_aux = (xcb_input_key_class_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_key_class_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* keys */
    xcb_block_len += _aux->num_keys * sizeof(uint32_t);
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

uint32_t *
xcb_input_key_class_keys (const xcb_input_key_class_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_input_key_class_keys_length (const xcb_input_key_class_t *R  /**< */)
{
    return R->num_keys;
}

xcb_generic_iterator_t
xcb_input_key_class_keys_end (const xcb_input_key_class_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->num_keys);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_key_class_next (xcb_input_key_class_iterator_t *i  /**< */)
{
    xcb_input_key_class_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_key_class_t *)(((char *)R) + xcb_input_key_class_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_key_class_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_key_class_end (xcb_input_key_class_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_key_class_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

void
xcb_input_scroll_class_next (xcb_input_scroll_class_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_scroll_class_t);
}

xcb_generic_iterator_t
xcb_input_scroll_class_end (xcb_input_scroll_class_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_touch_class_next (xcb_input_touch_class_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_touch_class_t);
}

xcb_generic_iterator_t
xcb_input_touch_class_end (xcb_input_touch_class_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_input_valuator_class_next (xcb_input_valuator_class_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_valuator_class_t);
}

xcb_generic_iterator_t
xcb_input_valuator_class_end (xcb_input_valuator_class_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_device_class_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_device_class_t *_aux = (xcb_input_device_class_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_device_class_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* uninterpreted_data */
    xcb_block_len += ((_aux->len * 4) - 8) * sizeof(uint8_t);
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

uint8_t *
xcb_input_device_class_uninterpreted_data (const xcb_input_device_class_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_input_device_class_uninterpreted_data_length (const xcb_input_device_class_t *R  /**< */)
{
    return ((R->len * 4) - 8);
}

xcb_generic_iterator_t
xcb_input_device_class_uninterpreted_data_end (const xcb_input_device_class_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + (((R->len * 4) - 8));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_device_class_next (xcb_input_device_class_iterator_t *i  /**< */)
{
    xcb_input_device_class_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_device_class_t *)(((char *)R) + xcb_input_device_class_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_device_class_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_device_class_end (xcb_input_device_class_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_device_class_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_input_xi_device_info_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_xi_device_info_t *_aux = (xcb_input_xi_device_info_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_input_xi_device_info_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* name */
    xcb_block_len += (((_aux->name_len + 3) / 4) * 4) * sizeof(char);
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
    /* classes */
    for(i=0; i<_aux->num_classes; i++) {
        xcb_tmp_len = xcb_input_device_class_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_input_device_class_t);
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
xcb_input_xi_device_info_name (const xcb_input_xi_device_info_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_input_xi_device_info_name_length (const xcb_input_xi_device_info_t *R  /**< */)
{
    return (((R->name_len + 3) / 4) * 4);
}

xcb_generic_iterator_t
xcb_input_xi_device_info_name_end (const xcb_input_xi_device_info_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + ((((R->name_len + 3) / 4) * 4));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

int
xcb_input_xi_device_info_classes_length (const xcb_input_xi_device_info_t *R  /**< */)
{
    return R->num_classes;
}

xcb_input_device_class_iterator_t
xcb_input_xi_device_info_classes_iterator (const xcb_input_xi_device_info_t *R  /**< */)
{
    xcb_input_device_class_iterator_t i;
    xcb_generic_iterator_t prev = xcb_input_xi_device_info_name_end(R);
    i.data = (xcb_input_device_class_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_input_device_class_t, prev.index));
    i.rem = R->num_classes;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_input_xi_device_info_next (xcb_input_xi_device_info_iterator_t *i  /**< */)
{
    xcb_input_xi_device_info_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_input_xi_device_info_t *)(((char *)R) + xcb_input_xi_device_info_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_input_xi_device_info_t *) child.data;
}

xcb_generic_iterator_t
xcb_input_xi_device_info_end (xcb_input_xi_device_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_input_xi_device_info_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

int
xcb_input_xi_query_device_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_xi_query_device_reply_t *_aux = (xcb_input_xi_query_device_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_input_xi_query_device_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* infos */
    for(i=0; i<_aux->num_infos; i++) {
        xcb_tmp_len = xcb_input_xi_device_info_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_input_xi_device_info_t);
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

xcb_input_xi_query_device_cookie_t
xcb_input_xi_query_device (xcb_connection_t      *c  /**< */,
                           xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_QUERY_DEVICE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_query_device_cookie_t xcb_ret;
    xcb_input_xi_query_device_request_t xcb_out;

    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_query_device_cookie_t
xcb_input_xi_query_device_unchecked (xcb_connection_t      *c  /**< */,
                                     xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_QUERY_DEVICE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_query_device_cookie_t xcb_ret;
    xcb_input_xi_query_device_request_t xcb_out;

    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_input_xi_query_device_infos_length (const xcb_input_xi_query_device_reply_t *R  /**< */)
{
    return R->num_infos;
}

xcb_input_xi_device_info_iterator_t
xcb_input_xi_query_device_infos_iterator (const xcb_input_xi_query_device_reply_t *R  /**< */)
{
    xcb_input_xi_device_info_iterator_t i;
    i.data = (xcb_input_xi_device_info_t *) (R + 1);
    i.rem = R->num_infos;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_xi_query_device_reply_t *
xcb_input_xi_query_device_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_input_xi_query_device_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */)
{
    return (xcb_input_xi_query_device_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_input_xi_set_focus_checked (xcb_connection_t      *c  /**< */,
                                xcb_window_t           window  /**< */,
                                xcb_timestamp_t        time  /**< */,
                                xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_SET_FOCUS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_set_focus_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.time = time;
    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_set_focus (xcb_connection_t      *c  /**< */,
                        xcb_window_t           window  /**< */,
                        xcb_timestamp_t        time  /**< */,
                        xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_SET_FOCUS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_set_focus_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.time = time;
    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_get_focus_cookie_t
xcb_input_xi_get_focus (xcb_connection_t      *c  /**< */,
                        xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_GET_FOCUS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_get_focus_cookie_t xcb_ret;
    xcb_input_xi_get_focus_request_t xcb_out;

    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_get_focus_cookie_t
xcb_input_xi_get_focus_unchecked (xcb_connection_t      *c  /**< */,
                                  xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_GET_FOCUS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_get_focus_cookie_t xcb_ret;
    xcb_input_xi_get_focus_request_t xcb_out;

    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_get_focus_reply_t *
xcb_input_xi_get_focus_reply (xcb_connection_t                 *c  /**< */,
                              xcb_input_xi_get_focus_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_input_xi_get_focus_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_xi_grab_device_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_xi_grab_device_request_t *_aux = (xcb_input_xi_grab_device_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_xi_grab_device_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* mask */
    xcb_block_len += _aux->mask_len * sizeof(uint32_t);
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

xcb_input_xi_grab_device_cookie_t
xcb_input_xi_grab_device (xcb_connection_t      *c  /**< */,
                          xcb_window_t           window  /**< */,
                          xcb_timestamp_t        time  /**< */,
                          xcb_cursor_t           cursor  /**< */,
                          xcb_input_device_id_t  deviceid  /**< */,
                          uint8_t                mode  /**< */,
                          uint8_t                paired_device_mode  /**< */,
                          uint8_t                owner_events  /**< */,
                          uint16_t               mask_len  /**< */,
                          const uint32_t        *mask  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_GRAB_DEVICE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_input_xi_grab_device_cookie_t xcb_ret;
    xcb_input_xi_grab_device_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.time = time;
    xcb_out.cursor = cursor;
    xcb_out.deviceid = deviceid;
    xcb_out.mode = mode;
    xcb_out.paired_device_mode = paired_device_mode;
    xcb_out.owner_events = owner_events;
    xcb_out.pad0 = 0;
    xcb_out.mask_len = mask_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t mask */
    xcb_parts[4].iov_base = (char *) mask;
    xcb_parts[4].iov_len = mask_len * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_grab_device_cookie_t
xcb_input_xi_grab_device_unchecked (xcb_connection_t      *c  /**< */,
                                    xcb_window_t           window  /**< */,
                                    xcb_timestamp_t        time  /**< */,
                                    xcb_cursor_t           cursor  /**< */,
                                    xcb_input_device_id_t  deviceid  /**< */,
                                    uint8_t                mode  /**< */,
                                    uint8_t                paired_device_mode  /**< */,
                                    uint8_t                owner_events  /**< */,
                                    uint16_t               mask_len  /**< */,
                                    const uint32_t        *mask  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_GRAB_DEVICE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_input_xi_grab_device_cookie_t xcb_ret;
    xcb_input_xi_grab_device_request_t xcb_out;

    xcb_out.window = window;
    xcb_out.time = time;
    xcb_out.cursor = cursor;
    xcb_out.deviceid = deviceid;
    xcb_out.mode = mode;
    xcb_out.paired_device_mode = paired_device_mode;
    xcb_out.owner_events = owner_events;
    xcb_out.pad0 = 0;
    xcb_out.mask_len = mask_len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t mask */
    xcb_parts[4].iov_base = (char *) mask;
    xcb_parts[4].iov_len = mask_len * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_grab_device_reply_t *
xcb_input_xi_grab_device_reply (xcb_connection_t                   *c  /**< */,
                                xcb_input_xi_grab_device_cookie_t   cookie  /**< */,
                                xcb_generic_error_t               **e  /**< */)
{
    return (xcb_input_xi_grab_device_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_input_xi_ungrab_device_checked (xcb_connection_t      *c  /**< */,
                                    xcb_timestamp_t        time  /**< */,
                                    xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_UNGRAB_DEVICE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_ungrab_device_request_t xcb_out;

    xcb_out.time = time;
    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_ungrab_device (xcb_connection_t      *c  /**< */,
                            xcb_timestamp_t        time  /**< */,
                            xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_UNGRAB_DEVICE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_ungrab_device_request_t xcb_out;

    xcb_out.time = time;
    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_allow_events_checked (xcb_connection_t      *c  /**< */,
                                   xcb_timestamp_t        time  /**< */,
                                   xcb_input_device_id_t  deviceid  /**< */,
                                   uint8_t                event_mode  /**< */,
                                   uint32_t               touchid  /**< */,
                                   xcb_window_t           grab_window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_ALLOW_EVENTS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_allow_events_request_t xcb_out;

    xcb_out.time = time;
    xcb_out.deviceid = deviceid;
    xcb_out.event_mode = event_mode;
    xcb_out.pad0 = 0;
    xcb_out.touchid = touchid;
    xcb_out.grab_window = grab_window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_allow_events (xcb_connection_t      *c  /**< */,
                           xcb_timestamp_t        time  /**< */,
                           xcb_input_device_id_t  deviceid  /**< */,
                           uint8_t                event_mode  /**< */,
                           uint32_t               touchid  /**< */,
                           xcb_window_t           grab_window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_ALLOW_EVENTS,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_allow_events_request_t xcb_out;

    xcb_out.time = time;
    xcb_out.deviceid = deviceid;
    xcb_out.event_mode = event_mode;
    xcb_out.pad0 = 0;
    xcb_out.touchid = touchid;
    xcb_out.grab_window = grab_window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

void
xcb_input_grab_modifier_info_next (xcb_input_grab_modifier_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_grab_modifier_info_t);
}

xcb_generic_iterator_t
xcb_input_grab_modifier_info_end (xcb_input_grab_modifier_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_xi_passive_grab_device_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_xi_passive_grab_device_request_t *_aux = (xcb_input_xi_passive_grab_device_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_xi_passive_grab_device_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* mask */
    xcb_block_len += _aux->mask_len * sizeof(uint32_t);
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
    /* modifiers */
    xcb_block_len += _aux->num_modifiers * sizeof(uint32_t);
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

xcb_input_xi_passive_grab_device_cookie_t
xcb_input_xi_passive_grab_device (xcb_connection_t      *c  /**< */,
                                  xcb_timestamp_t        time  /**< */,
                                  xcb_window_t           grab_window  /**< */,
                                  xcb_cursor_t           cursor  /**< */,
                                  uint32_t               detail  /**< */,
                                  xcb_input_device_id_t  deviceid  /**< */,
                                  uint16_t               num_modifiers  /**< */,
                                  uint16_t               mask_len  /**< */,
                                  uint8_t                grab_type  /**< */,
                                  uint8_t                grab_mode  /**< */,
                                  uint8_t                paired_device_mode  /**< */,
                                  uint8_t                owner_events  /**< */,
                                  const uint32_t        *mask  /**< */,
                                  const uint32_t        *modifiers  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_PASSIVE_GRAB_DEVICE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[8];
    xcb_input_xi_passive_grab_device_cookie_t xcb_ret;
    xcb_input_xi_passive_grab_device_request_t xcb_out;

    xcb_out.time = time;
    xcb_out.grab_window = grab_window;
    xcb_out.cursor = cursor;
    xcb_out.detail = detail;
    xcb_out.deviceid = deviceid;
    xcb_out.num_modifiers = num_modifiers;
    xcb_out.mask_len = mask_len;
    xcb_out.grab_type = grab_type;
    xcb_out.grab_mode = grab_mode;
    xcb_out.paired_device_mode = paired_device_mode;
    xcb_out.owner_events = owner_events;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t mask */
    xcb_parts[4].iov_base = (char *) mask;
    xcb_parts[4].iov_len = mask_len * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* uint32_t modifiers */
    xcb_parts[6].iov_base = (char *) modifiers;
    xcb_parts[6].iov_len = num_modifiers * sizeof(uint32_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_passive_grab_device_cookie_t
xcb_input_xi_passive_grab_device_unchecked (xcb_connection_t      *c  /**< */,
                                            xcb_timestamp_t        time  /**< */,
                                            xcb_window_t           grab_window  /**< */,
                                            xcb_cursor_t           cursor  /**< */,
                                            uint32_t               detail  /**< */,
                                            xcb_input_device_id_t  deviceid  /**< */,
                                            uint16_t               num_modifiers  /**< */,
                                            uint16_t               mask_len  /**< */,
                                            uint8_t                grab_type  /**< */,
                                            uint8_t                grab_mode  /**< */,
                                            uint8_t                paired_device_mode  /**< */,
                                            uint8_t                owner_events  /**< */,
                                            const uint32_t        *mask  /**< */,
                                            const uint32_t        *modifiers  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_PASSIVE_GRAB_DEVICE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[8];
    xcb_input_xi_passive_grab_device_cookie_t xcb_ret;
    xcb_input_xi_passive_grab_device_request_t xcb_out;

    xcb_out.time = time;
    xcb_out.grab_window = grab_window;
    xcb_out.cursor = cursor;
    xcb_out.detail = detail;
    xcb_out.deviceid = deviceid;
    xcb_out.num_modifiers = num_modifiers;
    xcb_out.mask_len = mask_len;
    xcb_out.grab_type = grab_type;
    xcb_out.grab_mode = grab_mode;
    xcb_out.paired_device_mode = paired_device_mode;
    xcb_out.owner_events = owner_events;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t mask */
    xcb_parts[4].iov_base = (char *) mask;
    xcb_parts[4].iov_len = mask_len * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* uint32_t modifiers */
    xcb_parts[6].iov_base = (char *) modifiers;
    xcb_parts[6].iov_len = num_modifiers * sizeof(uint32_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_grab_modifier_info_t *
xcb_input_xi_passive_grab_device_modifiers (const xcb_input_xi_passive_grab_device_reply_t *R  /**< */)
{
    return (xcb_input_grab_modifier_info_t *) (R + 1);
}

int
xcb_input_xi_passive_grab_device_modifiers_length (const xcb_input_xi_passive_grab_device_reply_t *R  /**< */)
{
    return R->num_modifiers;
}

xcb_input_grab_modifier_info_iterator_t
xcb_input_xi_passive_grab_device_modifiers_iterator (const xcb_input_xi_passive_grab_device_reply_t *R  /**< */)
{
    xcb_input_grab_modifier_info_iterator_t i;
    i.data = (xcb_input_grab_modifier_info_t *) (R + 1);
    i.rem = R->num_modifiers;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_xi_passive_grab_device_reply_t *
xcb_input_xi_passive_grab_device_reply (xcb_connection_t                           *c  /**< */,
                                        xcb_input_xi_passive_grab_device_cookie_t   cookie  /**< */,
                                        xcb_generic_error_t                       **e  /**< */)
{
    return (xcb_input_xi_passive_grab_device_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_xi_passive_ungrab_device_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_xi_passive_ungrab_device_request_t *_aux = (xcb_input_xi_passive_ungrab_device_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_xi_passive_ungrab_device_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* modifiers */
    xcb_block_len += _aux->num_modifiers * sizeof(uint32_t);
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
xcb_input_xi_passive_ungrab_device_checked (xcb_connection_t      *c  /**< */,
                                            xcb_window_t           grab_window  /**< */,
                                            uint32_t               detail  /**< */,
                                            xcb_input_device_id_t  deviceid  /**< */,
                                            uint16_t               num_modifiers  /**< */,
                                            uint8_t                grab_type  /**< */,
                                            const uint32_t        *modifiers  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_PASSIVE_UNGRAB_DEVICE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_passive_ungrab_device_request_t xcb_out;

    xcb_out.grab_window = grab_window;
    xcb_out.detail = detail;
    xcb_out.deviceid = deviceid;
    xcb_out.num_modifiers = num_modifiers;
    xcb_out.grab_type = grab_type;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t modifiers */
    xcb_parts[4].iov_base = (char *) modifiers;
    xcb_parts[4].iov_len = num_modifiers * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_passive_ungrab_device (xcb_connection_t      *c  /**< */,
                                    xcb_window_t           grab_window  /**< */,
                                    uint32_t               detail  /**< */,
                                    xcb_input_device_id_t  deviceid  /**< */,
                                    uint16_t               num_modifiers  /**< */,
                                    uint8_t                grab_type  /**< */,
                                    const uint32_t        *modifiers  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_PASSIVE_UNGRAB_DEVICE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_passive_ungrab_device_request_t xcb_out;

    xcb_out.grab_window = grab_window;
    xcb_out.detail = detail;
    xcb_out.deviceid = deviceid;
    xcb_out.num_modifiers = num_modifiers;
    xcb_out.grab_type = grab_type;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t modifiers */
    xcb_parts[4].iov_base = (char *) modifiers;
    xcb_parts[4].iov_len = num_modifiers * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_input_xi_list_properties_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_xi_list_properties_reply_t *_aux = (xcb_input_xi_list_properties_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_xi_list_properties_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* properties */
    xcb_block_len += _aux->num_properties * sizeof(xcb_atom_t);
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

xcb_input_xi_list_properties_cookie_t
xcb_input_xi_list_properties (xcb_connection_t      *c  /**< */,
                              xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_LIST_PROPERTIES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_list_properties_cookie_t xcb_ret;
    xcb_input_xi_list_properties_request_t xcb_out;

    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_list_properties_cookie_t
xcb_input_xi_list_properties_unchecked (xcb_connection_t      *c  /**< */,
                                        xcb_input_device_id_t  deviceid  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_LIST_PROPERTIES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_list_properties_cookie_t xcb_ret;
    xcb_input_xi_list_properties_request_t xcb_out;

    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_atom_t *
xcb_input_xi_list_properties_properties (const xcb_input_xi_list_properties_reply_t *R  /**< */)
{
    return (xcb_atom_t *) (R + 1);
}

int
xcb_input_xi_list_properties_properties_length (const xcb_input_xi_list_properties_reply_t *R  /**< */)
{
    return R->num_properties;
}

xcb_generic_iterator_t
xcb_input_xi_list_properties_properties_end (const xcb_input_xi_list_properties_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_atom_t *) (R + 1)) + (R->num_properties);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_xi_list_properties_reply_t *
xcb_input_xi_list_properties_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_input_xi_list_properties_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */)
{
    return (xcb_input_xi_list_properties_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

uint8_t *
xcb_input_xi_change_property_items_data_8 (const xcb_input_xi_change_property_items_t *S  /**< */)
{
    return /* items */ S->data8;
}

int
xcb_input_xi_change_property_items_data_8_length (const xcb_input_xi_change_property_request_t *R  /**< */,
                                                  const xcb_input_xi_change_property_items_t *S  /**< */)
{
    return R->num_items;
}

xcb_generic_iterator_t
xcb_input_xi_change_property_items_data_8_end (const xcb_input_xi_change_property_request_t *R  /**< */,
                                               const xcb_input_xi_change_property_items_t *S  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = /* items */ S->data8 + R->num_items;
    i.rem = 0;
    i.index = (char *) i.data - (char *) S;
    return i;
}

uint16_t *
xcb_input_xi_change_property_items_data_16 (const xcb_input_xi_change_property_items_t *S  /**< */)
{
    return /* items */ S->data16;
}

int
xcb_input_xi_change_property_items_data_16_length (const xcb_input_xi_change_property_request_t *R  /**< */,
                                                   const xcb_input_xi_change_property_items_t *S  /**< */)
{
    return R->num_items;
}

xcb_generic_iterator_t
xcb_input_xi_change_property_items_data_16_end (const xcb_input_xi_change_property_request_t *R  /**< */,
                                                const xcb_input_xi_change_property_items_t *S  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = /* items */ S->data16 + R->num_items;
    i.rem = 0;
    i.index = (char *) i.data - (char *) S;
    return i;
}

uint32_t *
xcb_input_xi_change_property_items_data_32 (const xcb_input_xi_change_property_items_t *S  /**< */)
{
    return /* items */ S->data32;
}

int
xcb_input_xi_change_property_items_data_32_length (const xcb_input_xi_change_property_request_t *R  /**< */,
                                                   const xcb_input_xi_change_property_items_t *S  /**< */)
{
    return R->num_items;
}

xcb_generic_iterator_t
xcb_input_xi_change_property_items_data_32_end (const xcb_input_xi_change_property_request_t *R  /**< */,
                                                const xcb_input_xi_change_property_items_t *S  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = /* items */ S->data32 + R->num_items;
    i.rem = 0;
    i.index = (char *) i.data - (char *) S;
    return i;
}

int
xcb_input_xi_change_property_items_serialize (void                                       **_buffer  /**< */,
                                              uint32_t                                     num_items  /**< */,
                                              uint8_t                                      format  /**< */,
                                              const xcb_input_xi_change_property_items_t  *_aux  /**< */)
{
    char *xcb_out = *_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_align_to = 0;

    unsigned int xcb_pad = 0;
    char xcb_pad0[3] = {0, 0, 0};
    struct iovec xcb_parts[7];
    unsigned int xcb_parts_idx = 0;
    unsigned int xcb_block_len = 0;
    unsigned int i;
    char *xcb_tmp;

    if(format & XCB_INPUT_PROPERTY_FORMAT_8_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
            xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
            xcb_parts_idx++;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data8 */
        xcb_parts[xcb_parts_idx].iov_base = (char *) _aux->data8;
        xcb_block_len += num_items * sizeof(uint8_t);
        xcb_parts[xcb_parts_idx].iov_len = num_items * sizeof(uint8_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint8_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_16_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
            xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
            xcb_parts_idx++;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data16 */
        xcb_parts[xcb_parts_idx].iov_base = (char *) _aux->data16;
        xcb_block_len += num_items * sizeof(uint16_t);
        xcb_parts[xcb_parts_idx].iov_len = num_items * sizeof(uint16_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint16_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_32_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
            xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
            xcb_parts_idx++;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data32 */
        xcb_parts[xcb_parts_idx].iov_base = (char *) _aux->data32;
        xcb_block_len += num_items * sizeof(uint32_t);
        xcb_parts[xcb_parts_idx].iov_len = num_items * sizeof(uint32_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint32_t);
    }
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
        xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
        xcb_parts_idx++;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    if (NULL == xcb_out) {
        /* allocate memory */
        xcb_out = malloc(xcb_buffer_len);
        *_buffer = xcb_out;
    }

    xcb_tmp = xcb_out;
    for(i=0; i<xcb_parts_idx; i++) {
        if (0 != xcb_parts[i].iov_base && 0 != xcb_parts[i].iov_len)
            memcpy(xcb_tmp, xcb_parts[i].iov_base, xcb_parts[i].iov_len);
        if (0 != xcb_parts[i].iov_len)
            xcb_tmp += xcb_parts[i].iov_len;
    }

    return xcb_buffer_len;
}

int
xcb_input_xi_change_property_items_unpack (const void                            *_buffer  /**< */,
                                           uint32_t                               num_items  /**< */,
                                           uint8_t                                format  /**< */,
                                           xcb_input_xi_change_property_items_t  *_aux  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    if(format & XCB_INPUT_PROPERTY_FORMAT_8_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_tmp += xcb_pad;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data8 */
        _aux->data8 = (uint8_t *)xcb_tmp;
        xcb_block_len += num_items * sizeof(uint8_t);
        xcb_tmp += xcb_block_len;
        xcb_align_to = ALIGNOF(uint8_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_16_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_tmp += xcb_pad;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data16 */
        _aux->data16 = (uint16_t *)xcb_tmp;
        xcb_block_len += num_items * sizeof(uint16_t);
        xcb_tmp += xcb_block_len;
        xcb_align_to = ALIGNOF(uint16_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_32_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_tmp += xcb_pad;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data32 */
        _aux->data32 = (uint32_t *)xcb_tmp;
        xcb_block_len += num_items * sizeof(uint32_t);
        xcb_tmp += xcb_block_len;
        xcb_align_to = ALIGNOF(uint32_t);
    }
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
xcb_input_xi_change_property_items_sizeof (const void  *_buffer  /**< */,
                                           uint32_t     num_items  /**< */,
                                           uint8_t      format  /**< */)
{
    xcb_input_xi_change_property_items_t _aux;
    return xcb_input_xi_change_property_items_unpack(_buffer, num_items, format, &_aux);
}

xcb_void_cookie_t
xcb_input_xi_change_property_checked (xcb_connection_t      *c  /**< */,
                                      xcb_input_device_id_t  deviceid  /**< */,
                                      uint8_t                mode  /**< */,
                                      uint8_t                format  /**< */,
                                      xcb_atom_t             property  /**< */,
                                      xcb_atom_t             type  /**< */,
                                      uint32_t               num_items  /**< */,
                                      const void            *items  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_CHANGE_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_change_property_request_t xcb_out;

    xcb_out.deviceid = deviceid;
    xcb_out.mode = mode;
    xcb_out.format = format;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.num_items = num_items;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_xi_change_property_items_t items */
    xcb_parts[4].iov_base = (char *) items;
    xcb_parts[4].iov_len =
      xcb_input_xi_change_property_items_sizeof (items, num_items, format);

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_change_property (xcb_connection_t      *c  /**< */,
                              xcb_input_device_id_t  deviceid  /**< */,
                              uint8_t                mode  /**< */,
                              uint8_t                format  /**< */,
                              xcb_atom_t             property  /**< */,
                              xcb_atom_t             type  /**< */,
                              uint32_t               num_items  /**< */,
                              const void            *items  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_CHANGE_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_change_property_request_t xcb_out;

    xcb_out.deviceid = deviceid;
    xcb_out.mode = mode;
    xcb_out.format = format;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.num_items = num_items;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_xi_change_property_items_t items */
    xcb_parts[4].iov_base = (char *) items;
    xcb_parts[4].iov_len =
      xcb_input_xi_change_property_items_sizeof (items, num_items, format);

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_change_property_aux_checked (xcb_connection_t                           *c  /**< */,
                                          xcb_input_device_id_t                       deviceid  /**< */,
                                          uint8_t                                     mode  /**< */,
                                          uint8_t                                     format  /**< */,
                                          xcb_atom_t                                  property  /**< */,
                                          xcb_atom_t                                  type  /**< */,
                                          uint32_t                                    num_items  /**< */,
                                          const xcb_input_xi_change_property_items_t *items  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_CHANGE_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_change_property_request_t xcb_out;
    void *xcb_aux0 = 0;

    xcb_out.deviceid = deviceid;
    xcb_out.mode = mode;
    xcb_out.format = format;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.num_items = num_items;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_xi_change_property_items_t items */
    xcb_parts[4].iov_len =
      xcb_input_xi_change_property_items_serialize (&xcb_aux0, num_items, format, items);
    xcb_parts[4].iov_base = xcb_aux0;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    free(xcb_aux0);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_change_property_aux (xcb_connection_t                           *c  /**< */,
                                  xcb_input_device_id_t                       deviceid  /**< */,
                                  uint8_t                                     mode  /**< */,
                                  uint8_t                                     format  /**< */,
                                  xcb_atom_t                                  property  /**< */,
                                  xcb_atom_t                                  type  /**< */,
                                  uint32_t                                    num_items  /**< */,
                                  const xcb_input_xi_change_property_items_t *items  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_CHANGE_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_change_property_request_t xcb_out;
    void *xcb_aux0 = 0;

    xcb_out.deviceid = deviceid;
    xcb_out.mode = mode;
    xcb_out.format = format;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.num_items = num_items;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_xi_change_property_items_t items */
    xcb_parts[4].iov_len =
      xcb_input_xi_change_property_items_serialize (&xcb_aux0, num_items, format, items);
    xcb_parts[4].iov_base = xcb_aux0;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    free(xcb_aux0);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_delete_property_checked (xcb_connection_t      *c  /**< */,
                                      xcb_input_device_id_t  deviceid  /**< */,
                                      xcb_atom_t             property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_DELETE_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_delete_property_request_t xcb_out;

    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);
    xcb_out.property = property;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_delete_property (xcb_connection_t      *c  /**< */,
                              xcb_input_device_id_t  deviceid  /**< */,
                              xcb_atom_t             property  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_DELETE_PROPERTY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_delete_property_request_t xcb_out;

    xcb_out.deviceid = deviceid;
    memset(xcb_out.pad0, 0, 2);
    xcb_out.property = property;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_input_xi_get_property_items_data_8 (const xcb_input_xi_get_property_items_t *S  /**< */)
{
    return /* items */ S->data8;
}

int
xcb_input_xi_get_property_items_data_8_length (const xcb_input_xi_get_property_reply_t *R  /**< */,
                                               const xcb_input_xi_get_property_items_t *S  /**< */)
{
    return R->num_items;
}

xcb_generic_iterator_t
xcb_input_xi_get_property_items_data_8_end (const xcb_input_xi_get_property_reply_t *R  /**< */,
                                            const xcb_input_xi_get_property_items_t *S  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = /* items */ S->data8 + R->num_items;
    i.rem = 0;
    i.index = (char *) i.data - (char *) S;
    return i;
}

uint16_t *
xcb_input_xi_get_property_items_data_16 (const xcb_input_xi_get_property_items_t *S  /**< */)
{
    return /* items */ S->data16;
}

int
xcb_input_xi_get_property_items_data_16_length (const xcb_input_xi_get_property_reply_t *R  /**< */,
                                                const xcb_input_xi_get_property_items_t *S  /**< */)
{
    return R->num_items;
}

xcb_generic_iterator_t
xcb_input_xi_get_property_items_data_16_end (const xcb_input_xi_get_property_reply_t *R  /**< */,
                                             const xcb_input_xi_get_property_items_t *S  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = /* items */ S->data16 + R->num_items;
    i.rem = 0;
    i.index = (char *) i.data - (char *) S;
    return i;
}

uint32_t *
xcb_input_xi_get_property_items_data_32 (const xcb_input_xi_get_property_items_t *S  /**< */)
{
    return /* items */ S->data32;
}

int
xcb_input_xi_get_property_items_data_32_length (const xcb_input_xi_get_property_reply_t *R  /**< */,
                                                const xcb_input_xi_get_property_items_t *S  /**< */)
{
    return R->num_items;
}

xcb_generic_iterator_t
xcb_input_xi_get_property_items_data_32_end (const xcb_input_xi_get_property_reply_t *R  /**< */,
                                             const xcb_input_xi_get_property_items_t *S  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = /* items */ S->data32 + R->num_items;
    i.rem = 0;
    i.index = (char *) i.data - (char *) S;
    return i;
}

int
xcb_input_xi_get_property_items_serialize (void                                    **_buffer  /**< */,
                                           uint32_t                                  num_items  /**< */,
                                           uint8_t                                   format  /**< */,
                                           const xcb_input_xi_get_property_items_t  *_aux  /**< */)
{
    char *xcb_out = *_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_align_to = 0;

    unsigned int xcb_pad = 0;
    char xcb_pad0[3] = {0, 0, 0};
    struct iovec xcb_parts[7];
    unsigned int xcb_parts_idx = 0;
    unsigned int xcb_block_len = 0;
    unsigned int i;
    char *xcb_tmp;

    if(format & XCB_INPUT_PROPERTY_FORMAT_8_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
            xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
            xcb_parts_idx++;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data8 */
        xcb_parts[xcb_parts_idx].iov_base = (char *) _aux->data8;
        xcb_block_len += num_items * sizeof(uint8_t);
        xcb_parts[xcb_parts_idx].iov_len = num_items * sizeof(uint8_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint8_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_16_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
            xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
            xcb_parts_idx++;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data16 */
        xcb_parts[xcb_parts_idx].iov_base = (char *) _aux->data16;
        xcb_block_len += num_items * sizeof(uint16_t);
        xcb_parts[xcb_parts_idx].iov_len = num_items * sizeof(uint16_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint16_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_32_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
            xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
            xcb_parts_idx++;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data32 */
        xcb_parts[xcb_parts_idx].iov_base = (char *) _aux->data32;
        xcb_block_len += num_items * sizeof(uint32_t);
        xcb_parts[xcb_parts_idx].iov_len = num_items * sizeof(uint32_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint32_t);
    }
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_parts[xcb_parts_idx].iov_base = xcb_pad0;
        xcb_parts[xcb_parts_idx].iov_len = xcb_pad;
        xcb_parts_idx++;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    if (NULL == xcb_out) {
        /* allocate memory */
        xcb_out = malloc(xcb_buffer_len);
        *_buffer = xcb_out;
    }

    xcb_tmp = xcb_out;
    for(i=0; i<xcb_parts_idx; i++) {
        if (0 != xcb_parts[i].iov_base && 0 != xcb_parts[i].iov_len)
            memcpy(xcb_tmp, xcb_parts[i].iov_base, xcb_parts[i].iov_len);
        if (0 != xcb_parts[i].iov_len)
            xcb_tmp += xcb_parts[i].iov_len;
    }

    return xcb_buffer_len;
}

int
xcb_input_xi_get_property_items_unpack (const void                         *_buffer  /**< */,
                                        uint32_t                            num_items  /**< */,
                                        uint8_t                             format  /**< */,
                                        xcb_input_xi_get_property_items_t  *_aux  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    if(format & XCB_INPUT_PROPERTY_FORMAT_8_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_tmp += xcb_pad;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data8 */
        _aux->data8 = (uint8_t *)xcb_tmp;
        xcb_block_len += num_items * sizeof(uint8_t);
        xcb_tmp += xcb_block_len;
        xcb_align_to = ALIGNOF(uint8_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_16_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_tmp += xcb_pad;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data16 */
        _aux->data16 = (uint16_t *)xcb_tmp;
        xcb_block_len += num_items * sizeof(uint16_t);
        xcb_tmp += xcb_block_len;
        xcb_align_to = ALIGNOF(uint16_t);
    }
    if(format & XCB_INPUT_PROPERTY_FORMAT_32_BITS) {
        /* insert padding */
        xcb_pad = -xcb_block_len & (xcb_align_to - 1);
        xcb_buffer_len += xcb_block_len + xcb_pad;
        if (0 != xcb_pad) {
            xcb_tmp += xcb_pad;
            xcb_pad = 0;
        }
        xcb_block_len = 0;
        /* data32 */
        _aux->data32 = (uint32_t *)xcb_tmp;
        xcb_block_len += num_items * sizeof(uint32_t);
        xcb_tmp += xcb_block_len;
        xcb_align_to = ALIGNOF(uint32_t);
    }
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
xcb_input_xi_get_property_items_sizeof (const void  *_buffer  /**< */,
                                        uint32_t     num_items  /**< */,
                                        uint8_t      format  /**< */)
{
    xcb_input_xi_get_property_items_t _aux;
    return xcb_input_xi_get_property_items_unpack(_buffer, num_items, format, &_aux);
}

xcb_input_xi_get_property_cookie_t
xcb_input_xi_get_property (xcb_connection_t      *c  /**< */,
                           xcb_input_device_id_t  deviceid  /**< */,
                           uint8_t                _delete  /**< */,
                           xcb_atom_t             property  /**< */,
                           xcb_atom_t             type  /**< */,
                           uint32_t               offset  /**< */,
                           uint32_t               len  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_GET_PROPERTY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_get_property_cookie_t xcb_ret;
    xcb_input_xi_get_property_request_t xcb_out;

    xcb_out.deviceid = deviceid;
    xcb_out._delete = _delete;
    xcb_out.pad0 = 0;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.offset = offset;
    xcb_out.len = len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_get_property_cookie_t
xcb_input_xi_get_property_unchecked (xcb_connection_t      *c  /**< */,
                                     xcb_input_device_id_t  deviceid  /**< */,
                                     uint8_t                _delete  /**< */,
                                     xcb_atom_t             property  /**< */,
                                     xcb_atom_t             type  /**< */,
                                     uint32_t               offset  /**< */,
                                     uint32_t               len  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_GET_PROPERTY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_get_property_cookie_t xcb_ret;
    xcb_input_xi_get_property_request_t xcb_out;

    xcb_out.deviceid = deviceid;
    xcb_out._delete = _delete;
    xcb_out.pad0 = 0;
    xcb_out.property = property;
    xcb_out.type = type;
    xcb_out.offset = offset;
    xcb_out.len = len;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

void *
xcb_input_xi_get_property_items (const xcb_input_xi_get_property_reply_t *R  /**< */)
{
    return (void *) (R + 1);
}

xcb_input_xi_get_property_reply_t *
xcb_input_xi_get_property_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_input_xi_get_property_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */)
{
    return (xcb_input_xi_get_property_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_input_xi_get_selected_events_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_xi_get_selected_events_reply_t *_aux = (xcb_input_xi_get_selected_events_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_input_xi_get_selected_events_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* masks */
    for(i=0; i<_aux->num_masks; i++) {
        xcb_tmp_len = xcb_input_event_mask_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_input_event_mask_t);
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

xcb_input_xi_get_selected_events_cookie_t
xcb_input_xi_get_selected_events (xcb_connection_t *c  /**< */,
                                  xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_GET_SELECTED_EVENTS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_get_selected_events_cookie_t xcb_ret;
    xcb_input_xi_get_selected_events_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_input_xi_get_selected_events_cookie_t
xcb_input_xi_get_selected_events_unchecked (xcb_connection_t *c  /**< */,
                                            xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_GET_SELECTED_EVENTS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_input_xi_get_selected_events_cookie_t xcb_ret;
    xcb_input_xi_get_selected_events_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_input_xi_get_selected_events_masks_length (const xcb_input_xi_get_selected_events_reply_t *R  /**< */)
{
    return R->num_masks;
}

xcb_input_event_mask_iterator_t
xcb_input_xi_get_selected_events_masks_iterator (const xcb_input_xi_get_selected_events_reply_t *R  /**< */)
{
    xcb_input_event_mask_iterator_t i;
    i.data = (xcb_input_event_mask_t *) (R + 1);
    i.rem = R->num_masks;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_input_xi_get_selected_events_reply_t *
xcb_input_xi_get_selected_events_reply (xcb_connection_t                           *c  /**< */,
                                        xcb_input_xi_get_selected_events_cookie_t   cookie  /**< */,
                                        xcb_generic_error_t                       **e  /**< */)
{
    return (xcb_input_xi_get_selected_events_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

void
xcb_input_barrier_release_pointer_info_next (xcb_input_barrier_release_pointer_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_barrier_release_pointer_info_t);
}

xcb_generic_iterator_t
xcb_input_barrier_release_pointer_info_end (xcb_input_barrier_release_pointer_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_xi_barrier_release_pointer_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_xi_barrier_release_pointer_request_t *_aux = (xcb_input_xi_barrier_release_pointer_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_xi_barrier_release_pointer_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* barriers */
    xcb_block_len += _aux->num_barriers * sizeof(xcb_input_barrier_release_pointer_info_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_barrier_release_pointer_info_t);
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
xcb_input_xi_barrier_release_pointer_checked (xcb_connection_t                               *c  /**< */,
                                              uint32_t                                        num_barriers  /**< */,
                                              const xcb_input_barrier_release_pointer_info_t *barriers  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_BARRIER_RELEASE_POINTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_barrier_release_pointer_request_t xcb_out;

    xcb_out.num_barriers = num_barriers;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_barrier_release_pointer_info_t barriers */
    xcb_parts[4].iov_base = (char *) barriers;
    xcb_parts[4].iov_len = num_barriers * sizeof(xcb_input_barrier_release_pointer_info_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_input_xi_barrier_release_pointer (xcb_connection_t                               *c  /**< */,
                                      uint32_t                                        num_barriers  /**< */,
                                      const xcb_input_barrier_release_pointer_info_t *barriers  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_input_id,
        /* opcode */ XCB_INPUT_XI_BARRIER_RELEASE_POINTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_input_xi_barrier_release_pointer_request_t xcb_out;

    xcb_out.num_barriers = num_barriers;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_input_barrier_release_pointer_info_t barriers */
    xcb_parts[4].iov_base = (char *) barriers;
    xcb_parts[4].iov_len = num_barriers * sizeof(xcb_input_barrier_release_pointer_info_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_input_device_changed_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_device_changed_event_t *_aux = (xcb_input_device_changed_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_input_device_changed_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* classes */
    for(i=0; i<_aux->num_classes; i++) {
        xcb_tmp_len = xcb_input_device_class_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_input_device_class_t);
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
xcb_input_key_press_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_key_press_event_t *_aux = (xcb_input_key_press_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_key_press_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* button_mask */
    xcb_block_len += _aux->buttons_len * sizeof(uint32_t);
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
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_key_release_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_key_release_event_t *_aux = (xcb_input_key_release_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_key_release_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* button_mask */
    xcb_block_len += _aux->buttons_len * sizeof(uint32_t);
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
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_button_press_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_button_press_event_t *_aux = (xcb_input_button_press_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_button_press_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* button_mask */
    xcb_block_len += _aux->buttons_len * sizeof(uint32_t);
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
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_button_release_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_button_release_event_t *_aux = (xcb_input_button_release_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_button_release_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* button_mask */
    xcb_block_len += _aux->buttons_len * sizeof(uint32_t);
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
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_motion_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_motion_event_t *_aux = (xcb_input_motion_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_motion_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* button_mask */
    xcb_block_len += _aux->buttons_len * sizeof(uint32_t);
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
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_enter_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_enter_event_t *_aux = (xcb_input_enter_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_enter_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* buttons */
    xcb_block_len += _aux->buttons_len * sizeof(uint32_t);
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

int
xcb_input_leave_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_leave_event_t *_aux = (xcb_input_leave_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_leave_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* buttons */
    xcb_block_len += _aux->buttons_len * sizeof(uint32_t);
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

int
xcb_input_focus_in_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_focus_in_event_t *_aux = (xcb_input_focus_in_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_focus_in_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* buttons */
    xcb_block_len += _aux->buttons_len * sizeof(uint32_t);
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

int
xcb_input_focus_out_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_focus_out_event_t *_aux = (xcb_input_focus_out_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_focus_out_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* buttons */
    xcb_block_len += _aux->buttons_len * sizeof(uint32_t);
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

void
xcb_input_hierarchy_info_next (xcb_input_hierarchy_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_hierarchy_info_t);
}

xcb_generic_iterator_t
xcb_input_hierarchy_info_end (xcb_input_hierarchy_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_hierarchy_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_hierarchy_event_t *_aux = (xcb_input_hierarchy_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_hierarchy_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* infos */
    xcb_block_len += _aux->num_infos * sizeof(xcb_input_hierarchy_info_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_input_hierarchy_info_t);
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
xcb_input_raw_key_press_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_raw_key_press_event_t *_aux = (xcb_input_raw_key_press_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_raw_key_press_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_raw_key_release_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_raw_key_release_event_t *_aux = (xcb_input_raw_key_release_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_raw_key_release_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_raw_button_press_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_raw_button_press_event_t *_aux = (xcb_input_raw_button_press_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_raw_button_press_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_raw_button_release_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_raw_button_release_event_t *_aux = (xcb_input_raw_button_release_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_raw_button_release_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_raw_motion_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_raw_motion_event_t *_aux = (xcb_input_raw_motion_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_raw_motion_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_touch_begin_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_touch_begin_event_t *_aux = (xcb_input_touch_begin_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_touch_begin_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* button_mask */
    xcb_block_len += _aux->buttons_len * sizeof(uint32_t);
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
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_touch_update_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_touch_update_event_t *_aux = (xcb_input_touch_update_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_touch_update_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* button_mask */
    xcb_block_len += _aux->buttons_len * sizeof(uint32_t);
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
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_touch_end_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_touch_end_event_t *_aux = (xcb_input_touch_end_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_touch_end_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* button_mask */
    xcb_block_len += _aux->buttons_len * sizeof(uint32_t);
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
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_raw_touch_begin_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_raw_touch_begin_event_t *_aux = (xcb_input_raw_touch_begin_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_raw_touch_begin_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_raw_touch_update_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_raw_touch_update_event_t *_aux = (xcb_input_raw_touch_update_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_raw_touch_update_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

int
xcb_input_raw_touch_end_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_raw_touch_end_event_t *_aux = (xcb_input_raw_touch_end_event_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_input_raw_touch_end_event_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* valuator_mask */
    xcb_block_len += _aux->valuators_len * sizeof(uint32_t);
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

