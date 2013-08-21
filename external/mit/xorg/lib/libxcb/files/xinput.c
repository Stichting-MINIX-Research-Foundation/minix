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
#include "xproto.h"

xcb_extension_t xcb_input_id = { "XInputExtension", 0 };


/*****************************************************************************
 **
 ** void xcb_input_key_code_next
 ** 
 ** @param xcb_input_key_code_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_key_code_next (xcb_input_key_code_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_key_code_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_key_code_end
 ** 
 ** @param xcb_input_key_code_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_key_code_end (xcb_input_key_code_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_event_class_next
 ** 
 ** @param xcb_input_event_class_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_event_class_next (xcb_input_event_class_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_event_class_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_event_class_end
 ** 
 ** @param xcb_input_event_class_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_event_class_end (xcb_input_event_class_iterator_t i  /**< */)
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_get_extension_version_request_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_input_get_extension_version_cookie_t xcb_input_get_extension_version
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_input_get_extension_version_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_extension_version_cookie_t xcb_input_get_extension_version_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          name_len
 ** @param const char       *name
 ** @returns xcb_input_get_extension_version_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_extension_version_reply_t * xcb_input_get_extension_version_reply
 ** 
 ** @param xcb_connection_t                          *c
 ** @param xcb_input_get_extension_version_cookie_t   cookie
 ** @param xcb_generic_error_t                      **e
 ** @returns xcb_input_get_extension_version_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_extension_version_reply_t *
xcb_input_get_extension_version_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_input_get_extension_version_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */)
{
    return (xcb_input_get_extension_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** void xcb_input_device_info_next
 ** 
 ** @param xcb_input_device_info_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_info_next (xcb_input_device_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_info_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_info_end
 ** 
 ** @param xcb_input_device_info_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_info_end (xcb_input_device_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_list_input_devices_reply_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_input_list_input_devices_cookie_t xcb_input_list_input_devices
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_input_list_input_devices_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_list_input_devices_cookie_t xcb_input_list_input_devices_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_input_list_input_devices_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_device_info_t * xcb_input_list_input_devices_devices
 ** 
 ** @param const xcb_input_list_input_devices_reply_t *R
 ** @returns xcb_input_device_info_t *
 **
 *****************************************************************************/
 
xcb_input_device_info_t *
xcb_input_list_input_devices_devices (const xcb_input_list_input_devices_reply_t *R  /**< */)
{
    return (xcb_input_device_info_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_input_list_input_devices_devices_length
 ** 
 ** @param const xcb_input_list_input_devices_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_list_input_devices_devices_length (const xcb_input_list_input_devices_reply_t *R  /**< */)
{
    return R->devices_len;
}


/*****************************************************************************
 **
 ** xcb_input_device_info_iterator_t xcb_input_list_input_devices_devices_iterator
 ** 
 ** @param const xcb_input_list_input_devices_reply_t *R
 ** @returns xcb_input_device_info_iterator_t
 **
 *****************************************************************************/
 
xcb_input_device_info_iterator_t
xcb_input_list_input_devices_devices_iterator (const xcb_input_list_input_devices_reply_t *R  /**< */)
{
    xcb_input_device_info_iterator_t i;
    i.data = (xcb_input_device_info_t *) (R + 1);
    i.rem = R->devices_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_input_list_input_devices_reply_t * xcb_input_list_input_devices_reply
 ** 
 ** @param xcb_connection_t                       *c
 ** @param xcb_input_list_input_devices_cookie_t   cookie
 ** @param xcb_generic_error_t                   **e
 ** @returns xcb_input_list_input_devices_reply_t *
 **
 *****************************************************************************/
 
xcb_input_list_input_devices_reply_t *
xcb_input_list_input_devices_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_input_list_input_devices_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */)
{
    return (xcb_input_list_input_devices_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** void xcb_input_input_info_next
 ** 
 ** @param xcb_input_input_info_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_input_info_next (xcb_input_input_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_input_info_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_input_info_end
 ** 
 ** @param xcb_input_input_info_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_input_info_end (xcb_input_input_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_key_info_next
 ** 
 ** @param xcb_input_key_info_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_key_info_next (xcb_input_key_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_key_info_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_key_info_end
 ** 
 ** @param xcb_input_key_info_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_key_info_end (xcb_input_key_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_button_info_next
 ** 
 ** @param xcb_input_button_info_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_button_info_next (xcb_input_button_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_button_info_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_button_info_end
 ** 
 ** @param xcb_input_button_info_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_button_info_end (xcb_input_button_info_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_axis_info_next
 ** 
 ** @param xcb_input_axis_info_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_axis_info_next (xcb_input_axis_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_axis_info_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_axis_info_end
 ** 
 ** @param xcb_input_axis_info_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_valuator_info_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_input_axis_info_t * xcb_input_valuator_info_axes
 ** 
 ** @param const xcb_input_valuator_info_t *R
 ** @returns xcb_input_axis_info_t *
 **
 *****************************************************************************/
 
xcb_input_axis_info_t *
xcb_input_valuator_info_axes (const xcb_input_valuator_info_t *R  /**< */)
{
    return (xcb_input_axis_info_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_input_valuator_info_axes_length
 ** 
 ** @param const xcb_input_valuator_info_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_valuator_info_axes_length (const xcb_input_valuator_info_t *R  /**< */)
{
    return R->axes_len;
}


/*****************************************************************************
 **
 ** xcb_input_axis_info_iterator_t xcb_input_valuator_info_axes_iterator
 ** 
 ** @param const xcb_input_valuator_info_t *R
 ** @returns xcb_input_axis_info_iterator_t
 **
 *****************************************************************************/
 
xcb_input_axis_info_iterator_t
xcb_input_valuator_info_axes_iterator (const xcb_input_valuator_info_t *R  /**< */)
{
    xcb_input_axis_info_iterator_t i;
    i.data = (xcb_input_axis_info_t *) (R + 1);
    i.rem = R->axes_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** void xcb_input_valuator_info_next
 ** 
 ** @param xcb_input_valuator_info_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_valuator_info_end
 ** 
 ** @param xcb_input_valuator_info_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** void xcb_input_input_class_info_next
 ** 
 ** @param xcb_input_input_class_info_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_input_class_info_next (xcb_input_input_class_info_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_input_class_info_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_input_class_info_end
 ** 
 ** @param xcb_input_input_class_info_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_open_device_reply_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_input_open_device_cookie_t xcb_input_open_device
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_open_device_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_open_device_cookie_t xcb_input_open_device_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_open_device_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_input_class_info_t * xcb_input_open_device_class_info
 ** 
 ** @param const xcb_input_open_device_reply_t *R
 ** @returns xcb_input_input_class_info_t *
 **
 *****************************************************************************/
 
xcb_input_input_class_info_t *
xcb_input_open_device_class_info (const xcb_input_open_device_reply_t *R  /**< */)
{
    return (xcb_input_input_class_info_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_input_open_device_class_info_length
 ** 
 ** @param const xcb_input_open_device_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_open_device_class_info_length (const xcb_input_open_device_reply_t *R  /**< */)
{
    return R->num_classes;
}


/*****************************************************************************
 **
 ** xcb_input_input_class_info_iterator_t xcb_input_open_device_class_info_iterator
 ** 
 ** @param const xcb_input_open_device_reply_t *R
 ** @returns xcb_input_input_class_info_iterator_t
 **
 *****************************************************************************/
 
xcb_input_input_class_info_iterator_t
xcb_input_open_device_class_info_iterator (const xcb_input_open_device_reply_t *R  /**< */)
{
    xcb_input_input_class_info_iterator_t i;
    i.data = (xcb_input_input_class_info_t *) (R + 1);
    i.rem = R->num_classes;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_input_open_device_reply_t * xcb_input_open_device_reply
 ** 
 ** @param xcb_connection_t                *c
 ** @param xcb_input_open_device_cookie_t   cookie
 ** @param xcb_generic_error_t            **e
 ** @returns xcb_input_open_device_reply_t *
 **
 *****************************************************************************/
 
xcb_input_open_device_reply_t *
xcb_input_open_device_reply (xcb_connection_t                *c  /**< */,
                             xcb_input_open_device_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_input_open_device_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_close_device_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_close_device
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_set_device_mode_cookie_t xcb_input_set_device_mode
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           mode
 ** @returns xcb_input_set_device_mode_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_set_device_mode_cookie_t xcb_input_set_device_mode_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           mode
 ** @returns xcb_input_set_device_mode_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_set_device_mode_reply_t * xcb_input_set_device_mode_reply
 ** 
 ** @param xcb_connection_t                    *c
 ** @param xcb_input_set_device_mode_cookie_t   cookie
 ** @param xcb_generic_error_t                **e
 ** @returns xcb_input_set_device_mode_reply_t *
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_select_extension_event_request_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_select_extension_event_checked
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   window
 ** @param uint16_t                       num_classes
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_select_extension_event
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   window
 ** @param uint16_t                       num_classes
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_get_selected_extension_events_reply_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_input_get_selected_extension_events_cookie_t xcb_input_get_selected_extension_events
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_input_get_selected_extension_events_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_selected_extension_events_cookie_t xcb_input_get_selected_extension_events_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_input_get_selected_extension_events_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_event_class_t * xcb_input_get_selected_extension_events_this_classes
 ** 
 ** @param const xcb_input_get_selected_extension_events_reply_t *R
 ** @returns xcb_input_event_class_t *
 **
 *****************************************************************************/
 
xcb_input_event_class_t *
xcb_input_get_selected_extension_events_this_classes (const xcb_input_get_selected_extension_events_reply_t *R  /**< */)
{
    return (xcb_input_event_class_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_input_get_selected_extension_events_this_classes_length
 ** 
 ** @param const xcb_input_get_selected_extension_events_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_get_selected_extension_events_this_classes_length (const xcb_input_get_selected_extension_events_reply_t *R  /**< */)
{
    return R->num_this_classes;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_get_selected_extension_events_this_classes_end
 ** 
 ** @param const xcb_input_get_selected_extension_events_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_get_selected_extension_events_this_classes_end (const xcb_input_get_selected_extension_events_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_input_event_class_t *) (R + 1)) + (R->num_this_classes);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_input_event_class_t * xcb_input_get_selected_extension_events_all_classes
 ** 
 ** @param const xcb_input_get_selected_extension_events_reply_t *R
 ** @returns xcb_input_event_class_t *
 **
 *****************************************************************************/
 
xcb_input_event_class_t *
xcb_input_get_selected_extension_events_all_classes (const xcb_input_get_selected_extension_events_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_input_get_selected_extension_events_this_classes_end(R);
    return (xcb_input_event_class_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_input_event_class_t, prev.index) + 0);
}


/*****************************************************************************
 **
 ** int xcb_input_get_selected_extension_events_all_classes_length
 ** 
 ** @param const xcb_input_get_selected_extension_events_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_get_selected_extension_events_all_classes_length (const xcb_input_get_selected_extension_events_reply_t *R  /**< */)
{
    return R->num_all_classes;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_get_selected_extension_events_all_classes_end
 ** 
 ** @param const xcb_input_get_selected_extension_events_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_selected_extension_events_reply_t * xcb_input_get_selected_extension_events_reply
 ** 
 ** @param xcb_connection_t                                  *c
 ** @param xcb_input_get_selected_extension_events_cookie_t   cookie
 ** @param xcb_generic_error_t                              **e
 ** @returns xcb_input_get_selected_extension_events_reply_t *
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_change_device_dont_propagate_list_request_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_change_device_dont_propagate_list_checked
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   window
 ** @param uint16_t                       num_classes
 ** @param uint8_t                        mode
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_change_device_dont_propagate_list
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   window
 ** @param uint16_t                       num_classes
 ** @param uint8_t                        mode
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_get_device_dont_propagate_list_reply_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_input_get_device_dont_propagate_list_cookie_t xcb_input_get_device_dont_propagate_list
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_input_get_device_dont_propagate_list_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_device_dont_propagate_list_cookie_t xcb_input_get_device_dont_propagate_list_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_input_get_device_dont_propagate_list_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_event_class_t * xcb_input_get_device_dont_propagate_list_classes
 ** 
 ** @param const xcb_input_get_device_dont_propagate_list_reply_t *R
 ** @returns xcb_input_event_class_t *
 **
 *****************************************************************************/
 
xcb_input_event_class_t *
xcb_input_get_device_dont_propagate_list_classes (const xcb_input_get_device_dont_propagate_list_reply_t *R  /**< */)
{
    return (xcb_input_event_class_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_input_get_device_dont_propagate_list_classes_length
 ** 
 ** @param const xcb_input_get_device_dont_propagate_list_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_get_device_dont_propagate_list_classes_length (const xcb_input_get_device_dont_propagate_list_reply_t *R  /**< */)
{
    return R->num_classes;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_get_device_dont_propagate_list_classes_end
 ** 
 ** @param const xcb_input_get_device_dont_propagate_list_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_get_device_dont_propagate_list_classes_end (const xcb_input_get_device_dont_propagate_list_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_input_event_class_t *) (R + 1)) + (R->num_classes);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_input_get_device_dont_propagate_list_reply_t * xcb_input_get_device_dont_propagate_list_reply
 ** 
 ** @param xcb_connection_t                                   *c
 ** @param xcb_input_get_device_dont_propagate_list_cookie_t   cookie
 ** @param xcb_generic_error_t                               **e
 ** @returns xcb_input_get_device_dont_propagate_list_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_device_dont_propagate_list_reply_t *
xcb_input_get_device_dont_propagate_list_reply (xcb_connection_t                                   *c  /**< */,
                                                xcb_input_get_device_dont_propagate_list_cookie_t   cookie  /**< */,
                                                xcb_generic_error_t                               **e  /**< */)
{
    return (xcb_input_get_device_dont_propagate_list_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_input_get_device_motion_events_cookie_t xcb_input_get_device_motion_events
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   start
 ** @param xcb_timestamp_t   stop
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_motion_events_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_device_motion_events_cookie_t xcb_input_get_device_motion_events_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   start
 ** @param xcb_timestamp_t   stop
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_motion_events_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_device_motion_events_reply_t * xcb_input_get_device_motion_events_reply
 ** 
 ** @param xcb_connection_t                             *c
 ** @param xcb_input_get_device_motion_events_cookie_t   cookie
 ** @param xcb_generic_error_t                         **e
 ** @returns xcb_input_get_device_motion_events_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_device_motion_events_reply_t *
xcb_input_get_device_motion_events_reply (xcb_connection_t                             *c  /**< */,
                                          xcb_input_get_device_motion_events_cookie_t   cookie  /**< */,
                                          xcb_generic_error_t                         **e  /**< */)
{
    return (xcb_input_get_device_motion_events_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** void xcb_input_device_time_coord_next
 ** 
 ** @param xcb_input_device_time_coord_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_time_coord_next (xcb_input_device_time_coord_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_time_coord_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_time_coord_end
 ** 
 ** @param xcb_input_device_time_coord_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_time_coord_end (xcb_input_device_time_coord_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** xcb_input_change_keyboard_device_cookie_t xcb_input_change_keyboard_device
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_change_keyboard_device_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_change_keyboard_device_cookie_t xcb_input_change_keyboard_device_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_change_keyboard_device_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_change_keyboard_device_reply_t * xcb_input_change_keyboard_device_reply
 ** 
 ** @param xcb_connection_t                           *c
 ** @param xcb_input_change_keyboard_device_cookie_t   cookie
 ** @param xcb_generic_error_t                       **e
 ** @returns xcb_input_change_keyboard_device_reply_t *
 **
 *****************************************************************************/
 
xcb_input_change_keyboard_device_reply_t *
xcb_input_change_keyboard_device_reply (xcb_connection_t                           *c  /**< */,
                                        xcb_input_change_keyboard_device_cookie_t   cookie  /**< */,
                                        xcb_generic_error_t                       **e  /**< */)
{
    return (xcb_input_change_keyboard_device_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_input_change_pointer_device_cookie_t xcb_input_change_pointer_device
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           x_axis
 ** @param uint8_t           y_axis
 ** @param uint8_t           device_id
 ** @returns xcb_input_change_pointer_device_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_change_pointer_device_cookie_t xcb_input_change_pointer_device_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           x_axis
 ** @param uint8_t           y_axis
 ** @param uint8_t           device_id
 ** @returns xcb_input_change_pointer_device_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_change_pointer_device_reply_t * xcb_input_change_pointer_device_reply
 ** 
 ** @param xcb_connection_t                          *c
 ** @param xcb_input_change_pointer_device_cookie_t   cookie
 ** @param xcb_generic_error_t                      **e
 ** @returns xcb_input_change_pointer_device_reply_t *
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_grab_device_request_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_input_grab_device_cookie_t xcb_input_grab_device
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   grab_window
 ** @param xcb_timestamp_t                time
 ** @param uint16_t                       num_classes
 ** @param uint8_t                        this_device_mode
 ** @param uint8_t                        other_device_mode
 ** @param uint8_t                        owner_events
 ** @param uint8_t                        device_id
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_input_grab_device_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_grab_device_cookie_t xcb_input_grab_device_unchecked
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   grab_window
 ** @param xcb_timestamp_t                time
 ** @param uint16_t                       num_classes
 ** @param uint8_t                        this_device_mode
 ** @param uint8_t                        other_device_mode
 ** @param uint8_t                        owner_events
 ** @param uint8_t                        device_id
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_input_grab_device_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_grab_device_reply_t * xcb_input_grab_device_reply
 ** 
 ** @param xcb_connection_t                *c
 ** @param xcb_input_grab_device_cookie_t   cookie
 ** @param xcb_generic_error_t            **e
 ** @returns xcb_input_grab_device_reply_t *
 **
 *****************************************************************************/
 
xcb_input_grab_device_reply_t *
xcb_input_grab_device_reply (xcb_connection_t                *c  /**< */,
                             xcb_input_grab_device_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_input_grab_device_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_ungrab_device_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   time
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_ungrab_device
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   time
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_grab_device_key_request_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_grab_device_key_checked
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   grab_window
 ** @param uint16_t                       num_classes
 ** @param uint16_t                       modifiers
 ** @param uint8_t                        modifier_device
 ** @param uint8_t                        grabbed_device
 ** @param uint8_t                        key
 ** @param uint8_t                        this_device_mode
 ** @param uint8_t                        other_device_mode
 ** @param uint8_t                        owner_events
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_grab_device_key
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   grab_window
 ** @param uint16_t                       num_classes
 ** @param uint16_t                       modifiers
 ** @param uint8_t                        modifier_device
 ** @param uint8_t                        grabbed_device
 ** @param uint8_t                        key
 ** @param uint8_t                        this_device_mode
 ** @param uint8_t                        other_device_mode
 ** @param uint8_t                        owner_events
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_ungrab_device_key_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      grabWindow
 ** @param uint16_t          modifiers
 ** @param uint8_t           modifier_device
 ** @param uint8_t           key
 ** @param uint8_t           grabbed_device
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_ungrab_device_key
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      grabWindow
 ** @param uint16_t          modifiers
 ** @param uint8_t           modifier_device
 ** @param uint8_t           key
 ** @param uint8_t           grabbed_device
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_grab_device_button_request_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_grab_device_button_checked
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   grab_window
 ** @param uint8_t                        grabbed_device
 ** @param uint8_t                        modifier_device
 ** @param uint16_t                       num_classes
 ** @param uint16_t                       modifiers
 ** @param uint8_t                        this_device_mode
 ** @param uint8_t                        other_device_mode
 ** @param uint8_t                        button
 ** @param uint8_t                        owner_events
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_grab_device_button
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   grab_window
 ** @param uint8_t                        grabbed_device
 ** @param uint8_t                        modifier_device
 ** @param uint16_t                       num_classes
 ** @param uint16_t                       modifiers
 ** @param uint8_t                        this_device_mode
 ** @param uint8_t                        other_device_mode
 ** @param uint8_t                        button
 ** @param uint8_t                        owner_events
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_ungrab_device_button_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      grab_window
 ** @param uint16_t          modifiers
 ** @param uint8_t           modifier_device
 ** @param uint8_t           button
 ** @param uint8_t           grabbed_device
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_ungrab_device_button
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      grab_window
 ** @param uint16_t          modifiers
 ** @param uint8_t           modifier_device
 ** @param uint8_t           button
 ** @param uint8_t           grabbed_device
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_allow_device_events_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   time
 ** @param uint8_t           mode
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_allow_device_events
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_timestamp_t   time
 ** @param uint8_t           mode
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_device_focus_cookie_t xcb_input_get_device_focus
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_focus_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_device_focus_cookie_t xcb_input_get_device_focus_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_focus_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_device_focus_reply_t * xcb_input_get_device_focus_reply
 ** 
 ** @param xcb_connection_t                     *c
 ** @param xcb_input_get_device_focus_cookie_t   cookie
 ** @param xcb_generic_error_t                 **e
 ** @returns xcb_input_get_device_focus_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_device_focus_reply_t *
xcb_input_get_device_focus_reply (xcb_connection_t                     *c  /**< */,
                                  xcb_input_get_device_focus_cookie_t   cookie  /**< */,
                                  xcb_generic_error_t                 **e  /**< */)
{
    return (xcb_input_get_device_focus_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_set_device_focus_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      focus
 ** @param xcb_timestamp_t   time
 ** @param uint8_t           revert_to
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_set_device_focus
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      focus
 ** @param xcb_timestamp_t   time
 ** @param uint8_t           revert_to
 ** @param uint8_t           device_id
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_feedback_control_cookie_t xcb_input_get_feedback_control
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_feedback_control_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_feedback_control_cookie_t xcb_input_get_feedback_control_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_feedback_control_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_feedback_control_reply_t * xcb_input_get_feedback_control_reply
 ** 
 ** @param xcb_connection_t                         *c
 ** @param xcb_input_get_feedback_control_cookie_t   cookie
 ** @param xcb_generic_error_t                     **e
 ** @returns xcb_input_get_feedback_control_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_feedback_control_reply_t *
xcb_input_get_feedback_control_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_input_get_feedback_control_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */)
{
    return (xcb_input_get_feedback_control_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** void xcb_input_feedback_state_next
 ** 
 ** @param xcb_input_feedback_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_feedback_state_next (xcb_input_feedback_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_feedback_state_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_feedback_state_end
 ** 
 ** @param xcb_input_feedback_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_feedback_state_end (xcb_input_feedback_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_kbd_feedback_state_next
 ** 
 ** @param xcb_input_kbd_feedback_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_kbd_feedback_state_next (xcb_input_kbd_feedback_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_kbd_feedback_state_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_kbd_feedback_state_end
 ** 
 ** @param xcb_input_kbd_feedback_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_kbd_feedback_state_end (xcb_input_kbd_feedback_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_ptr_feedback_state_next
 ** 
 ** @param xcb_input_ptr_feedback_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_ptr_feedback_state_next (xcb_input_ptr_feedback_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_ptr_feedback_state_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_ptr_feedback_state_end
 ** 
 ** @param xcb_input_ptr_feedback_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_ptr_feedback_state_end (xcb_input_ptr_feedback_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_integer_feedback_state_next
 ** 
 ** @param xcb_input_integer_feedback_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_integer_feedback_state_next (xcb_input_integer_feedback_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_integer_feedback_state_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_integer_feedback_state_end
 ** 
 ** @param xcb_input_integer_feedback_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_string_feedback_state_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_keysym_t * xcb_input_string_feedback_state_keysyms
 ** 
 ** @param const xcb_input_string_feedback_state_t *R
 ** @returns xcb_keysym_t *
 **
 *****************************************************************************/
 
xcb_keysym_t *
xcb_input_string_feedback_state_keysyms (const xcb_input_string_feedback_state_t *R  /**< */)
{
    return (xcb_keysym_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_input_string_feedback_state_keysyms_length
 ** 
 ** @param const xcb_input_string_feedback_state_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_string_feedback_state_keysyms_length (const xcb_input_string_feedback_state_t *R  /**< */)
{
    return R->num_keysyms;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_string_feedback_state_keysyms_end
 ** 
 ** @param const xcb_input_string_feedback_state_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_string_feedback_state_keysyms_end (const xcb_input_string_feedback_state_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_keysym_t *) (R + 1)) + (R->num_keysyms);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** void xcb_input_string_feedback_state_next
 ** 
 ** @param xcb_input_string_feedback_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_string_feedback_state_end
 ** 
 ** @param xcb_input_string_feedback_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** void xcb_input_bell_feedback_state_next
 ** 
 ** @param xcb_input_bell_feedback_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_bell_feedback_state_next (xcb_input_bell_feedback_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_bell_feedback_state_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_bell_feedback_state_end
 ** 
 ** @param xcb_input_bell_feedback_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_bell_feedback_state_end (xcb_input_bell_feedback_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_led_feedback_state_next
 ** 
 ** @param xcb_input_led_feedback_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_led_feedback_state_next (xcb_input_led_feedback_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_led_feedback_state_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_led_feedback_state_end
 ** 
 ** @param xcb_input_led_feedback_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_led_feedback_state_end (xcb_input_led_feedback_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_feedback_ctl_next
 ** 
 ** @param xcb_input_feedback_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_feedback_ctl_next (xcb_input_feedback_ctl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_feedback_ctl_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_feedback_ctl_end
 ** 
 ** @param xcb_input_feedback_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_feedback_ctl_end (xcb_input_feedback_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_kbd_feedback_ctl_next
 ** 
 ** @param xcb_input_kbd_feedback_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_kbd_feedback_ctl_next (xcb_input_kbd_feedback_ctl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_kbd_feedback_ctl_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_kbd_feedback_ctl_end
 ** 
 ** @param xcb_input_kbd_feedback_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_kbd_feedback_ctl_end (xcb_input_kbd_feedback_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_ptr_feedback_ctl_next
 ** 
 ** @param xcb_input_ptr_feedback_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_ptr_feedback_ctl_next (xcb_input_ptr_feedback_ctl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_ptr_feedback_ctl_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_ptr_feedback_ctl_end
 ** 
 ** @param xcb_input_ptr_feedback_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_ptr_feedback_ctl_end (xcb_input_ptr_feedback_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_integer_feedback_ctl_next
 ** 
 ** @param xcb_input_integer_feedback_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_integer_feedback_ctl_next (xcb_input_integer_feedback_ctl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_integer_feedback_ctl_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_integer_feedback_ctl_end
 ** 
 ** @param xcb_input_integer_feedback_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_string_feedback_ctl_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_keysym_t * xcb_input_string_feedback_ctl_keysyms
 ** 
 ** @param const xcb_input_string_feedback_ctl_t *R
 ** @returns xcb_keysym_t *
 **
 *****************************************************************************/
 
xcb_keysym_t *
xcb_input_string_feedback_ctl_keysyms (const xcb_input_string_feedback_ctl_t *R  /**< */)
{
    return (xcb_keysym_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_input_string_feedback_ctl_keysyms_length
 ** 
 ** @param const xcb_input_string_feedback_ctl_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_string_feedback_ctl_keysyms_length (const xcb_input_string_feedback_ctl_t *R  /**< */)
{
    return R->num_keysyms;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_string_feedback_ctl_keysyms_end
 ** 
 ** @param const xcb_input_string_feedback_ctl_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_string_feedback_ctl_keysyms_end (const xcb_input_string_feedback_ctl_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_keysym_t *) (R + 1)) + (R->num_keysyms);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** void xcb_input_string_feedback_ctl_next
 ** 
 ** @param xcb_input_string_feedback_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_string_feedback_ctl_end
 ** 
 ** @param xcb_input_string_feedback_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** void xcb_input_bell_feedback_ctl_next
 ** 
 ** @param xcb_input_bell_feedback_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_bell_feedback_ctl_next (xcb_input_bell_feedback_ctl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_bell_feedback_ctl_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_bell_feedback_ctl_end
 ** 
 ** @param xcb_input_bell_feedback_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_bell_feedback_ctl_end (xcb_input_bell_feedback_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_led_feedback_ctl_next
 ** 
 ** @param xcb_input_led_feedback_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_led_feedback_ctl_next (xcb_input_led_feedback_ctl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_led_feedback_ctl_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_led_feedback_ctl_end
 ** 
 ** @param xcb_input_led_feedback_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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
xcb_input_get_device_key_mapping_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_get_device_key_mapping_reply_t *_aux = (xcb_input_get_device_key_mapping_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_get_device_key_mapping_reply_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_input_get_device_key_mapping_cookie_t xcb_input_get_device_key_mapping
 ** 
 ** @param xcb_connection_t     *c
 ** @param uint8_t               device_id
 ** @param xcb_input_key_code_t  first_keycode
 ** @param uint8_t               count
 ** @returns xcb_input_get_device_key_mapping_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_device_key_mapping_cookie_t xcb_input_get_device_key_mapping_unchecked
 ** 
 ** @param xcb_connection_t     *c
 ** @param uint8_t               device_id
 ** @param xcb_input_key_code_t  first_keycode
 ** @param uint8_t               count
 ** @returns xcb_input_get_device_key_mapping_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_keysym_t * xcb_input_get_device_key_mapping_keysyms
 ** 
 ** @param const xcb_input_get_device_key_mapping_reply_t *R
 ** @returns xcb_keysym_t *
 **
 *****************************************************************************/
 
xcb_keysym_t *
xcb_input_get_device_key_mapping_keysyms (const xcb_input_get_device_key_mapping_reply_t *R  /**< */)
{
    return (xcb_keysym_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_input_get_device_key_mapping_keysyms_length
 ** 
 ** @param const xcb_input_get_device_key_mapping_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_get_device_key_mapping_keysyms_length (const xcb_input_get_device_key_mapping_reply_t *R  /**< */)
{
    return R->length;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_get_device_key_mapping_keysyms_end
 ** 
 ** @param const xcb_input_get_device_key_mapping_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_get_device_key_mapping_keysyms_end (const xcb_input_get_device_key_mapping_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_keysym_t *) (R + 1)) + (R->length);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_input_get_device_key_mapping_reply_t * xcb_input_get_device_key_mapping_reply
 ** 
 ** @param xcb_connection_t                           *c
 ** @param xcb_input_get_device_key_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t                       **e
 ** @returns xcb_input_get_device_key_mapping_reply_t *
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_change_device_key_mapping_request_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_change_device_key_mapping_checked
 ** 
 ** @param xcb_connection_t     *c
 ** @param uint8_t               device_id
 ** @param xcb_input_key_code_t  first_keycode
 ** @param uint8_t               keysyms_per_keycode
 ** @param uint8_t               keycode_count
 ** @param const xcb_keysym_t   *keysyms
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_change_device_key_mapping
 ** 
 ** @param xcb_connection_t     *c
 ** @param uint8_t               device_id
 ** @param xcb_input_key_code_t  first_keycode
 ** @param uint8_t               keysyms_per_keycode
 ** @param uint8_t               keycode_count
 ** @param const xcb_keysym_t   *keysyms
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_get_device_modifier_mapping_reply_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_input_get_device_modifier_mapping_cookie_t xcb_input_get_device_modifier_mapping
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_modifier_mapping_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_device_modifier_mapping_cookie_t xcb_input_get_device_modifier_mapping_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_modifier_mapping_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** uint8_t * xcb_input_get_device_modifier_mapping_keymaps
 ** 
 ** @param const xcb_input_get_device_modifier_mapping_reply_t *R
 ** @returns uint8_t *
 **
 *****************************************************************************/
 
uint8_t *
xcb_input_get_device_modifier_mapping_keymaps (const xcb_input_get_device_modifier_mapping_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_input_get_device_modifier_mapping_keymaps_length
 ** 
 ** @param const xcb_input_get_device_modifier_mapping_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_get_device_modifier_mapping_keymaps_length (const xcb_input_get_device_modifier_mapping_reply_t *R  /**< */)
{
    return (R->keycodes_per_modifier * 8);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_get_device_modifier_mapping_keymaps_end
 ** 
 ** @param const xcb_input_get_device_modifier_mapping_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_get_device_modifier_mapping_keymaps_end (const xcb_input_get_device_modifier_mapping_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + ((R->keycodes_per_modifier * 8));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_input_get_device_modifier_mapping_reply_t * xcb_input_get_device_modifier_mapping_reply
 ** 
 ** @param xcb_connection_t                                *c
 ** @param xcb_input_get_device_modifier_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t                            **e
 ** @returns xcb_input_get_device_modifier_mapping_reply_t *
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_set_device_modifier_mapping_request_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_input_set_device_modifier_mapping_cookie_t xcb_input_set_device_modifier_mapping
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           keycodes_per_modifier
 ** @param const uint8_t    *keymaps
 ** @returns xcb_input_set_device_modifier_mapping_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_set_device_modifier_mapping_cookie_t xcb_input_set_device_modifier_mapping_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           keycodes_per_modifier
 ** @param const uint8_t    *keymaps
 ** @returns xcb_input_set_device_modifier_mapping_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_set_device_modifier_mapping_reply_t * xcb_input_set_device_modifier_mapping_reply
 ** 
 ** @param xcb_connection_t                                *c
 ** @param xcb_input_set_device_modifier_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t                            **e
 ** @returns xcb_input_set_device_modifier_mapping_reply_t *
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_get_device_button_mapping_reply_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_input_get_device_button_mapping_cookie_t xcb_input_get_device_button_mapping
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_button_mapping_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_device_button_mapping_cookie_t xcb_input_get_device_button_mapping_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_button_mapping_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** uint8_t * xcb_input_get_device_button_mapping_map
 ** 
 ** @param const xcb_input_get_device_button_mapping_reply_t *R
 ** @returns uint8_t *
 **
 *****************************************************************************/
 
uint8_t *
xcb_input_get_device_button_mapping_map (const xcb_input_get_device_button_mapping_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_input_get_device_button_mapping_map_length
 ** 
 ** @param const xcb_input_get_device_button_mapping_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_get_device_button_mapping_map_length (const xcb_input_get_device_button_mapping_reply_t *R  /**< */)
{
    return R->map_size;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_get_device_button_mapping_map_end
 ** 
 ** @param const xcb_input_get_device_button_mapping_reply_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_get_device_button_mapping_map_end (const xcb_input_get_device_button_mapping_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + (R->map_size);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** xcb_input_get_device_button_mapping_reply_t * xcb_input_get_device_button_mapping_reply
 ** 
 ** @param xcb_connection_t                              *c
 ** @param xcb_input_get_device_button_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t                          **e
 ** @returns xcb_input_get_device_button_mapping_reply_t *
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_set_device_button_mapping_request_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_input_set_device_button_mapping_cookie_t xcb_input_set_device_button_mapping
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           map_size
 ** @param const uint8_t    *map
 ** @returns xcb_input_set_device_button_mapping_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_set_device_button_mapping_cookie_t xcb_input_set_device_button_mapping_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           map_size
 ** @param const uint8_t    *map
 ** @returns xcb_input_set_device_button_mapping_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_set_device_button_mapping_reply_t * xcb_input_set_device_button_mapping_reply
 ** 
 ** @param xcb_connection_t                              *c
 ** @param xcb_input_set_device_button_mapping_cookie_t   cookie
 ** @param xcb_generic_error_t                          **e
 ** @returns xcb_input_set_device_button_mapping_reply_t *
 **
 *****************************************************************************/
 
xcb_input_set_device_button_mapping_reply_t *
xcb_input_set_device_button_mapping_reply (xcb_connection_t                              *c  /**< */,
                                           xcb_input_set_device_button_mapping_cookie_t   cookie  /**< */,
                                           xcb_generic_error_t                          **e  /**< */)
{
    return (xcb_input_set_device_button_mapping_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_input_query_device_state_cookie_t xcb_input_query_device_state
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_query_device_state_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_query_device_state_cookie_t xcb_input_query_device_state_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @returns xcb_input_query_device_state_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_query_device_state_reply_t * xcb_input_query_device_state_reply
 ** 
 ** @param xcb_connection_t                       *c
 ** @param xcb_input_query_device_state_cookie_t   cookie
 ** @param xcb_generic_error_t                   **e
 ** @returns xcb_input_query_device_state_reply_t *
 **
 *****************************************************************************/
 
xcb_input_query_device_state_reply_t *
xcb_input_query_device_state_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_input_query_device_state_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */)
{
    return (xcb_input_query_device_state_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** void xcb_input_input_state_next
 ** 
 ** @param xcb_input_input_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_input_state_next (xcb_input_input_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_input_state_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_input_state_end
 ** 
 ** @param xcb_input_input_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_input_state_end (xcb_input_input_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_key_state_next
 ** 
 ** @param xcb_input_key_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_key_state_next (xcb_input_key_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_key_state_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_key_state_end
 ** 
 ** @param xcb_input_key_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_key_state_end (xcb_input_key_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_button_state_next
 ** 
 ** @param xcb_input_button_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_button_state_next (xcb_input_button_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_button_state_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_button_state_end
 ** 
 ** @param xcb_input_button_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_valuator_state_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** uint32_t * xcb_input_valuator_state_valuators
 ** 
 ** @param const xcb_input_valuator_state_t *R
 ** @returns uint32_t *
 **
 *****************************************************************************/
 
uint32_t *
xcb_input_valuator_state_valuators (const xcb_input_valuator_state_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_input_valuator_state_valuators_length
 ** 
 ** @param const xcb_input_valuator_state_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_valuator_state_valuators_length (const xcb_input_valuator_state_t *R  /**< */)
{
    return R->num_valuators;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_valuator_state_valuators_end
 ** 
 ** @param const xcb_input_valuator_state_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_valuator_state_valuators_end (const xcb_input_valuator_state_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->num_valuators);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** void xcb_input_valuator_state_next
 ** 
 ** @param xcb_input_valuator_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_valuator_state_end
 ** 
 ** @param xcb_input_valuator_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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
xcb_input_send_extension_event_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_send_extension_event_request_t *_aux = (xcb_input_send_extension_event_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_send_extension_event_request_t);
    xcb_tmp += xcb_block_len;
    /* events */
    xcb_block_len += (_aux->num_events * 32) * sizeof(char);
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_send_extension_event_checked
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   destination
 ** @param uint8_t                        device_id
 ** @param uint8_t                        propagate
 ** @param uint16_t                       num_classes
 ** @param uint8_t                        num_events
 ** @param const char                    *events
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_send_extension_event_checked (xcb_connection_t              *c  /**< */,
                                        xcb_window_t                   destination  /**< */,
                                        uint8_t                        device_id  /**< */,
                                        uint8_t                        propagate  /**< */,
                                        uint16_t                       num_classes  /**< */,
                                        uint8_t                        num_events  /**< */,
                                        const char                    *events  /**< */,
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
    /* char events */
    xcb_parts[4].iov_base = (char *) events;
    xcb_parts[4].iov_len = (num_events * 32) * sizeof(char);
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_send_extension_event
 ** 
 ** @param xcb_connection_t              *c
 ** @param xcb_window_t                   destination
 ** @param uint8_t                        device_id
 ** @param uint8_t                        propagate
 ** @param uint16_t                       num_classes
 ** @param uint8_t                        num_events
 ** @param const char                    *events
 ** @param const xcb_input_event_class_t *classes
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_input_send_extension_event (xcb_connection_t              *c  /**< */,
                                xcb_window_t                   destination  /**< */,
                                uint8_t                        device_id  /**< */,
                                uint8_t                        propagate  /**< */,
                                uint16_t                       num_classes  /**< */,
                                uint8_t                        num_events  /**< */,
                                const char                    *events  /**< */,
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
    /* char events */
    xcb_parts[4].iov_base = (char *) events;
    xcb_parts[4].iov_len = (num_events * 32) * sizeof(char);
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_device_bell_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           feedback_id
 ** @param uint8_t           feedback_class
 ** @param int8_t            percent
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_input_device_bell
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           feedback_id
 ** @param uint8_t           feedback_class
 ** @param int8_t            percent
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
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
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_set_device_valuators_request_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** xcb_input_set_device_valuators_cookie_t xcb_input_set_device_valuators
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           first_valuator
 ** @param uint8_t           num_valuators
 ** @param const int32_t    *valuators
 ** @returns xcb_input_set_device_valuators_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_set_device_valuators_cookie_t xcb_input_set_device_valuators_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           device_id
 ** @param uint8_t           first_valuator
 ** @param uint8_t           num_valuators
 ** @param const int32_t    *valuators
 ** @returns xcb_input_set_device_valuators_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_set_device_valuators_reply_t * xcb_input_set_device_valuators_reply
 ** 
 ** @param xcb_connection_t                         *c
 ** @param xcb_input_set_device_valuators_cookie_t   cookie
 ** @param xcb_generic_error_t                     **e
 ** @returns xcb_input_set_device_valuators_reply_t *
 **
 *****************************************************************************/
 
xcb_input_set_device_valuators_reply_t *
xcb_input_set_device_valuators_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_input_set_device_valuators_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */)
{
    return (xcb_input_set_device_valuators_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** xcb_input_get_device_control_cookie_t xcb_input_get_device_control
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          control_id
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_control_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_device_control_cookie_t xcb_input_get_device_control_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          control_id
 ** @param uint8_t           device_id
 ** @returns xcb_input_get_device_control_cookie_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_input_get_device_control_reply_t * xcb_input_get_device_control_reply
 ** 
 ** @param xcb_connection_t                       *c
 ** @param xcb_input_get_device_control_cookie_t   cookie
 ** @param xcb_generic_error_t                   **e
 ** @returns xcb_input_get_device_control_reply_t *
 **
 *****************************************************************************/
 
xcb_input_get_device_control_reply_t *
xcb_input_get_device_control_reply (xcb_connection_t                       *c  /**< */,
                                    xcb_input_get_device_control_cookie_t   cookie  /**< */,
                                    xcb_generic_error_t                   **e  /**< */)
{
    return (xcb_input_get_device_control_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


/*****************************************************************************
 **
 ** void xcb_input_device_state_next
 ** 
 ** @param xcb_input_device_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_state_next (xcb_input_device_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_state_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_state_end
 ** 
 ** @param xcb_input_device_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_state_end (xcb_input_device_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_device_resolution_state_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_device_resolution_state_t *_aux = (xcb_input_device_resolution_state_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_device_resolution_state_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** uint32_t * xcb_input_device_resolution_state_resolution_values
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns uint32_t *
 **
 *****************************************************************************/
 
uint32_t *
xcb_input_device_resolution_state_resolution_values (const xcb_input_device_resolution_state_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_input_device_resolution_state_resolution_values_length
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_device_resolution_state_resolution_values_length (const xcb_input_device_resolution_state_t *R  /**< */)
{
    return R->num_valuators;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_resolution_state_resolution_values_end
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_resolution_state_resolution_values_end (const xcb_input_device_resolution_state_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->num_valuators);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** uint32_t * xcb_input_device_resolution_state_resolution_min
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns uint32_t *
 **
 *****************************************************************************/
 
uint32_t *
xcb_input_device_resolution_state_resolution_min (const xcb_input_device_resolution_state_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_input_device_resolution_state_resolution_values_end(R);
    return (uint32_t *) ((char *) prev.data + XCB_TYPE_PAD(uint32_t, prev.index) + 0);
}


/*****************************************************************************
 **
 ** int xcb_input_device_resolution_state_resolution_min_length
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_device_resolution_state_resolution_min_length (const xcb_input_device_resolution_state_t *R  /**< */)
{
    return R->num_valuators;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_resolution_state_resolution_min_end
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** uint32_t * xcb_input_device_resolution_state_resolution_max
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns uint32_t *
 **
 *****************************************************************************/
 
uint32_t *
xcb_input_device_resolution_state_resolution_max (const xcb_input_device_resolution_state_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_input_device_resolution_state_resolution_min_end(R);
    return (uint32_t *) ((char *) prev.data + XCB_TYPE_PAD(uint32_t, prev.index) + 0);
}


/*****************************************************************************
 **
 ** int xcb_input_device_resolution_state_resolution_max_length
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_device_resolution_state_resolution_max_length (const xcb_input_device_resolution_state_t *R  /**< */)
{
    return R->num_valuators;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_resolution_state_resolution_max_end
 ** 
 ** @param const xcb_input_device_resolution_state_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** void xcb_input_device_resolution_state_next
 ** 
 ** @param xcb_input_device_resolution_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_resolution_state_end
 ** 
 ** @param xcb_input_device_resolution_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** void xcb_input_device_abs_calib_state_next
 ** 
 ** @param xcb_input_device_abs_calib_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_abs_calib_state_next (xcb_input_device_abs_calib_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_abs_calib_state_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_abs_calib_state_end
 ** 
 ** @param xcb_input_device_abs_calib_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_abs_calib_state_end (xcb_input_device_abs_calib_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_device_abs_area_state_next
 ** 
 ** @param xcb_input_device_abs_area_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_abs_area_state_next (xcb_input_device_abs_area_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_abs_area_state_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_abs_area_state_end
 ** 
 ** @param xcb_input_device_abs_area_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_abs_area_state_end (xcb_input_device_abs_area_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_device_core_state_next
 ** 
 ** @param xcb_input_device_core_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_core_state_next (xcb_input_device_core_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_core_state_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_core_state_end
 ** 
 ** @param xcb_input_device_core_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_core_state_end (xcb_input_device_core_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_device_enable_state_next
 ** 
 ** @param xcb_input_device_enable_state_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_enable_state_next (xcb_input_device_enable_state_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_enable_state_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_enable_state_end
 ** 
 ** @param xcb_input_device_enable_state_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_enable_state_end (xcb_input_device_enable_state_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_device_ctl_next
 ** 
 ** @param xcb_input_device_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_ctl_next (xcb_input_device_ctl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_ctl_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_ctl_end
 ** 
 ** @param xcb_input_device_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_ctl_end (xcb_input_device_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_input_device_resolution_ctl_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_input_device_resolution_ctl_t *_aux = (xcb_input_device_resolution_ctl_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to;


    xcb_block_len += sizeof(xcb_input_device_resolution_ctl_t);
    xcb_tmp += xcb_block_len;
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


/*****************************************************************************
 **
 ** uint32_t * xcb_input_device_resolution_ctl_resolution_values
 ** 
 ** @param const xcb_input_device_resolution_ctl_t *R
 ** @returns uint32_t *
 **
 *****************************************************************************/
 
uint32_t *
xcb_input_device_resolution_ctl_resolution_values (const xcb_input_device_resolution_ctl_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_input_device_resolution_ctl_resolution_values_length
 ** 
 ** @param const xcb_input_device_resolution_ctl_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_input_device_resolution_ctl_resolution_values_length (const xcb_input_device_resolution_ctl_t *R  /**< */)
{
    return R->num_valuators;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_resolution_ctl_resolution_values_end
 ** 
 ** @param const xcb_input_device_resolution_ctl_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_resolution_ctl_resolution_values_end (const xcb_input_device_resolution_ctl_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (R->num_valuators);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** void xcb_input_device_resolution_ctl_next
 ** 
 ** @param xcb_input_device_resolution_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_resolution_ctl_end
 ** 
 ** @param xcb_input_device_resolution_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
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


/*****************************************************************************
 **
 ** void xcb_input_device_abs_calib_ctl_next
 ** 
 ** @param xcb_input_device_abs_calib_ctl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_abs_calib_ctl_next (xcb_input_device_abs_calib_ctl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_abs_calib_ctl_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_abs_calib_ctl_end
 ** 
 ** @param xcb_input_device_abs_calib_ctl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_abs_calib_ctl_end (xcb_input_device_abs_calib_ctl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_device_abs_area_ctrl_next
 ** 
 ** @param xcb_input_device_abs_area_ctrl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_abs_area_ctrl_next (xcb_input_device_abs_area_ctrl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_abs_area_ctrl_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_abs_area_ctrl_end
 ** 
 ** @param xcb_input_device_abs_area_ctrl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_abs_area_ctrl_end (xcb_input_device_abs_area_ctrl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_device_core_ctrl_next
 ** 
 ** @param xcb_input_device_core_ctrl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_core_ctrl_next (xcb_input_device_core_ctrl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_core_ctrl_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_core_ctrl_end
 ** 
 ** @param xcb_input_device_core_ctrl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_core_ctrl_end (xcb_input_device_core_ctrl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** void xcb_input_device_enable_ctrl_next
 ** 
 ** @param xcb_input_device_enable_ctrl_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_input_device_enable_ctrl_next (xcb_input_device_enable_ctrl_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_input_device_enable_ctrl_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_input_device_enable_ctrl_end
 ** 
 ** @param xcb_input_device_enable_ctrl_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_input_device_enable_ctrl_end (xcb_input_device_enable_ctrl_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

