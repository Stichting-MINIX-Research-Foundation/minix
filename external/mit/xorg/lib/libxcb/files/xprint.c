/*
 * This file generated automatically from xprint.xml by c_client.py.
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
#include "xprint.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xproto.h"

xcb_extension_t xcb_x_print_id = { "XpExtension", 0 };

void
xcb_x_print_string8_next (xcb_x_print_string8_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_x_print_string8_t);
}

xcb_generic_iterator_t
xcb_x_print_string8_end (xcb_x_print_string8_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_x_print_printer_serialize (void                        **_buffer  /**< */,
                               const xcb_x_print_printer_t  *_aux  /**< */,
                               const xcb_x_print_string8_t  *name  /**< */,
                               const xcb_x_print_string8_t  *description  /**< */)
{
    char *xcb_out = *_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_align_to = 0;

    unsigned int xcb_pad = 0;
    char xcb_pad0[3] = {0, 0, 0};
    struct iovec xcb_parts[5];
    unsigned int xcb_parts_idx = 0;
    unsigned int xcb_block_len = 0;
    unsigned int i;
    char *xcb_tmp;

    /* xcb_x_print_printer_t.nameLen */
    xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->nameLen;
    xcb_block_len += sizeof(uint32_t);
    xcb_parts[xcb_parts_idx].iov_len = sizeof(uint32_t);
    xcb_parts_idx++;
    xcb_align_to = ALIGNOF(uint32_t);
    /* name */
    xcb_parts[xcb_parts_idx].iov_base = (char *) name;
    xcb_block_len += _aux->nameLen * sizeof(xcb_x_print_string8_t);
    xcb_parts[xcb_parts_idx].iov_len = _aux->nameLen * sizeof(xcb_x_print_string8_t);
    xcb_parts_idx++;
    xcb_align_to = ALIGNOF(xcb_x_print_string8_t);
    /* xcb_x_print_printer_t.descLen */
    xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->descLen;
    xcb_block_len += sizeof(uint32_t);
    xcb_parts[xcb_parts_idx].iov_len = sizeof(uint32_t);
    xcb_parts_idx++;
    xcb_align_to = ALIGNOF(uint32_t);
    /* description */
    xcb_parts[xcb_parts_idx].iov_base = (char *) description;
    xcb_block_len += _aux->descLen * sizeof(xcb_x_print_string8_t);
    xcb_parts[xcb_parts_idx].iov_len = _aux->descLen * sizeof(xcb_x_print_string8_t);
    xcb_parts_idx++;
    xcb_align_to = ALIGNOF(xcb_x_print_string8_t);
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
xcb_x_print_printer_unserialize (const void              *_buffer  /**< */,
                                 xcb_x_print_printer_t  **_aux  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    xcb_x_print_printer_t xcb_out;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    xcb_x_print_string8_t *name;
    int name_len;
    xcb_x_print_string8_t *description;
    int description_len;

    /* xcb_x_print_printer_t.nameLen */
    xcb_out.nameLen = *(uint32_t *)xcb_tmp;
    xcb_block_len += sizeof(uint32_t);
    xcb_tmp += sizeof(uint32_t);
    xcb_align_to = ALIGNOF(uint32_t);
    /* name */
    name = (xcb_x_print_string8_t *)xcb_tmp;
    name_len = xcb_out.nameLen * sizeof(xcb_x_print_string8_t);
    xcb_block_len += name_len;
    xcb_tmp += name_len;
    xcb_align_to = ALIGNOF(xcb_x_print_string8_t);
    /* xcb_x_print_printer_t.descLen */
    xcb_out.descLen = *(uint32_t *)xcb_tmp;
    xcb_block_len += sizeof(uint32_t);
    xcb_tmp += sizeof(uint32_t);
    xcb_align_to = ALIGNOF(uint32_t);
    /* description */
    description = (xcb_x_print_string8_t *)xcb_tmp;
    description_len = xcb_out.descLen * sizeof(xcb_x_print_string8_t);
    xcb_block_len += description_len;
    xcb_tmp += description_len;
    xcb_align_to = ALIGNOF(xcb_x_print_string8_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    if (NULL == _aux)
        return xcb_buffer_len;

    if (NULL == *_aux) {
        /* allocate memory */
        *_aux = malloc(xcb_buffer_len);
    }

    xcb_tmp = ((char *)*_aux)+xcb_buffer_len;
    xcb_tmp -= description_len;
    memmove(xcb_tmp, description, description_len);
    xcb_tmp -= name_len;
    memmove(xcb_tmp, name, name_len);
    **_aux = xcb_out;

    return xcb_buffer_len;
}

int
xcb_x_print_printer_sizeof (const void  *_buffer  /**< */)
{
    return xcb_x_print_printer_unserialize(_buffer, NULL);
}

xcb_x_print_string8_t *
xcb_x_print_printer_name (const xcb_x_print_printer_t *R  /**< */)
{
    return (xcb_x_print_string8_t *) (R + 1);
}

int
xcb_x_print_printer_name_length (const xcb_x_print_printer_t *R  /**< */)
{
    return R->nameLen;
}

xcb_generic_iterator_t
xcb_x_print_printer_name_end (const xcb_x_print_printer_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_x_print_string8_t *) (R + 1)) + (R->nameLen);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_x_print_string8_t *
xcb_x_print_printer_description (const xcb_x_print_printer_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_x_print_printer_name_end(R);
    return (xcb_x_print_string8_t *) ((char *) prev.data + XCB_TYPE_PAD(uint32_t, prev.index) + 4);
}

int
xcb_x_print_printer_description_length (const xcb_x_print_printer_t *R  /**< */)
{
    return R->descLen;
}

xcb_generic_iterator_t
xcb_x_print_printer_description_end (const xcb_x_print_printer_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_x_print_printer_name_end(R);
    i.data = ((xcb_x_print_string8_t *) child.data) + (R->descLen);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_x_print_printer_next (xcb_x_print_printer_iterator_t *i  /**< */)
{
    xcb_x_print_printer_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_x_print_printer_t *)(((char *)R) + xcb_x_print_printer_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_x_print_printer_t *) child.data;
}

xcb_generic_iterator_t
xcb_x_print_printer_end (xcb_x_print_printer_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_x_print_printer_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

void
xcb_x_print_pcontext_next (xcb_x_print_pcontext_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_x_print_pcontext_t);
}

xcb_generic_iterator_t
xcb_x_print_pcontext_end (xcb_x_print_pcontext_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

xcb_x_print_print_query_version_cookie_t
xcb_x_print_print_query_version (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_query_version_cookie_t xcb_ret;
    xcb_x_print_print_query_version_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_query_version_cookie_t
xcb_x_print_print_query_version_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_QUERY_VERSION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_query_version_cookie_t xcb_ret;
    xcb_x_print_print_query_version_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_query_version_reply_t *
xcb_x_print_print_query_version_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_x_print_print_query_version_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */)
{
    return (xcb_x_print_print_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_x_print_print_get_printer_list_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_x_print_print_get_printer_list_request_t *_aux = (xcb_x_print_print_get_printer_list_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_x_print_print_get_printer_list_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* printer_name */
    xcb_block_len += _aux->printerNameLen * sizeof(xcb_x_print_string8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_x_print_string8_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* locale */
    xcb_block_len += _aux->localeLen * sizeof(xcb_x_print_string8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_x_print_string8_t);
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

xcb_x_print_print_get_printer_list_cookie_t
xcb_x_print_print_get_printer_list (xcb_connection_t            *c  /**< */,
                                    uint32_t                     printerNameLen  /**< */,
                                    uint32_t                     localeLen  /**< */,
                                    const xcb_x_print_string8_t *printer_name  /**< */,
                                    const xcb_x_print_string8_t *locale  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_PRINTER_LIST,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[8];
    xcb_x_print_print_get_printer_list_cookie_t xcb_ret;
    xcb_x_print_print_get_printer_list_request_t xcb_out;

    xcb_out.printerNameLen = printerNameLen;
    xcb_out.localeLen = localeLen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_x_print_string8_t printer_name */
    xcb_parts[4].iov_base = (char *) printer_name;
    xcb_parts[4].iov_len = printerNameLen * sizeof(xcb_x_print_string8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_x_print_string8_t locale */
    xcb_parts[6].iov_base = (char *) locale;
    xcb_parts[6].iov_len = localeLen * sizeof(xcb_x_print_string8_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_printer_list_cookie_t
xcb_x_print_print_get_printer_list_unchecked (xcb_connection_t            *c  /**< */,
                                              uint32_t                     printerNameLen  /**< */,
                                              uint32_t                     localeLen  /**< */,
                                              const xcb_x_print_string8_t *printer_name  /**< */,
                                              const xcb_x_print_string8_t *locale  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_PRINTER_LIST,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[8];
    xcb_x_print_print_get_printer_list_cookie_t xcb_ret;
    xcb_x_print_print_get_printer_list_request_t xcb_out;

    xcb_out.printerNameLen = printerNameLen;
    xcb_out.localeLen = localeLen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_x_print_string8_t printer_name */
    xcb_parts[4].iov_base = (char *) printer_name;
    xcb_parts[4].iov_len = printerNameLen * sizeof(xcb_x_print_string8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_x_print_string8_t locale */
    xcb_parts[6].iov_base = (char *) locale;
    xcb_parts[6].iov_len = localeLen * sizeof(xcb_x_print_string8_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_x_print_print_get_printer_list_printers_length (const xcb_x_print_print_get_printer_list_reply_t *R  /**< */)
{
    return R->listCount;
}

xcb_x_print_printer_iterator_t
xcb_x_print_print_get_printer_list_printers_iterator (const xcb_x_print_print_get_printer_list_reply_t *R  /**< */)
{
    xcb_x_print_printer_iterator_t i;
    i.data = (xcb_x_print_printer_t *) (R + 1);
    i.rem = R->listCount;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_x_print_print_get_printer_list_reply_t *
xcb_x_print_print_get_printer_list_reply (xcb_connection_t                             *c  /**< */,
                                          xcb_x_print_print_get_printer_list_cookie_t   cookie  /**< */,
                                          xcb_generic_error_t                         **e  /**< */)
{
    xcb_x_print_print_get_printer_list_reply_t *reply = (xcb_x_print_print_get_printer_list_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
    int i;
    xcb_x_print_printer_iterator_t printers_iter = xcb_x_print_print_get_printer_list_printers_iterator(reply);
    int printers_len = xcb_x_print_print_get_printer_list_printers_length(reply);
    xcb_x_print_printer_t *printers_data;
    /* special cases: transform parts of the reply to match XCB data structures */
    for(i=0; i<printers_len; i++) {
        printers_data = printers_iter.data;
        xcb_x_print_printer_unserialize((const void *)printers_data, &printers_data);
        xcb_x_print_printer_next(&printers_iter);
    }
    return reply;
}

xcb_void_cookie_t
xcb_x_print_print_rehash_printer_list_checked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_REHASH_PRINTER_LIST,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_rehash_printer_list_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_rehash_printer_list (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_REHASH_PRINTER_LIST,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_rehash_printer_list_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_x_print_create_context_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_x_print_create_context_request_t *_aux = (xcb_x_print_create_context_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_x_print_create_context_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* printerName */
    xcb_block_len += _aux->printerNameLen * sizeof(xcb_x_print_string8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_x_print_string8_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* locale */
    xcb_block_len += _aux->localeLen * sizeof(xcb_x_print_string8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_x_print_string8_t);
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
xcb_x_print_create_context_checked (xcb_connection_t            *c  /**< */,
                                    uint32_t                     context_id  /**< */,
                                    uint32_t                     printerNameLen  /**< */,
                                    uint32_t                     localeLen  /**< */,
                                    const xcb_x_print_string8_t *printerName  /**< */,
                                    const xcb_x_print_string8_t *locale  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_CREATE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[8];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_create_context_request_t xcb_out;

    xcb_out.context_id = context_id;
    xcb_out.printerNameLen = printerNameLen;
    xcb_out.localeLen = localeLen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_x_print_string8_t printerName */
    xcb_parts[4].iov_base = (char *) printerName;
    xcb_parts[4].iov_len = printerNameLen * sizeof(xcb_x_print_string8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_x_print_string8_t locale */
    xcb_parts[6].iov_base = (char *) locale;
    xcb_parts[6].iov_len = localeLen * sizeof(xcb_x_print_string8_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_create_context (xcb_connection_t            *c  /**< */,
                            uint32_t                     context_id  /**< */,
                            uint32_t                     printerNameLen  /**< */,
                            uint32_t                     localeLen  /**< */,
                            const xcb_x_print_string8_t *printerName  /**< */,
                            const xcb_x_print_string8_t *locale  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 6,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_CREATE_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[8];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_create_context_request_t xcb_out;

    xcb_out.context_id = context_id;
    xcb_out.printerNameLen = printerNameLen;
    xcb_out.localeLen = localeLen;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_x_print_string8_t printerName */
    xcb_parts[4].iov_base = (char *) printerName;
    xcb_parts[4].iov_len = printerNameLen * sizeof(xcb_x_print_string8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_x_print_string8_t locale */
    xcb_parts[6].iov_base = (char *) locale;
    xcb_parts[6].iov_len = localeLen * sizeof(xcb_x_print_string8_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_set_context_checked (xcb_connection_t *c  /**< */,
                                       uint32_t          context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_SET_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_set_context_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_set_context (xcb_connection_t *c  /**< */,
                               uint32_t          context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_SET_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_set_context_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_context_cookie_t
xcb_x_print_print_get_context (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_get_context_cookie_t xcb_ret;
    xcb_x_print_print_get_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_context_cookie_t
xcb_x_print_print_get_context_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_get_context_cookie_t xcb_ret;
    xcb_x_print_print_get_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_context_reply_t *
xcb_x_print_print_get_context_reply (xcb_connection_t                        *c  /**< */,
                                     xcb_x_print_print_get_context_cookie_t   cookie  /**< */,
                                     xcb_generic_error_t                    **e  /**< */)
{
    return (xcb_x_print_print_get_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_x_print_print_destroy_context_checked (xcb_connection_t *c  /**< */,
                                           uint32_t          context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_DESTROY_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_destroy_context_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_destroy_context (xcb_connection_t *c  /**< */,
                                   uint32_t          context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_DESTROY_CONTEXT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_destroy_context_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_screen_of_context_cookie_t
xcb_x_print_print_get_screen_of_context (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_SCREEN_OF_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_get_screen_of_context_cookie_t xcb_ret;
    xcb_x_print_print_get_screen_of_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_screen_of_context_cookie_t
xcb_x_print_print_get_screen_of_context_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_SCREEN_OF_CONTEXT,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_get_screen_of_context_cookie_t xcb_ret;
    xcb_x_print_print_get_screen_of_context_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_screen_of_context_reply_t *
xcb_x_print_print_get_screen_of_context_reply (xcb_connection_t                                  *c  /**< */,
                                               xcb_x_print_print_get_screen_of_context_cookie_t   cookie  /**< */,
                                               xcb_generic_error_t                              **e  /**< */)
{
    return (xcb_x_print_print_get_screen_of_context_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_x_print_print_start_job_checked (xcb_connection_t *c  /**< */,
                                     uint8_t           output_mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_START_JOB,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_start_job_request_t xcb_out;

    xcb_out.output_mode = output_mode;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_start_job (xcb_connection_t *c  /**< */,
                             uint8_t           output_mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_START_JOB,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_start_job_request_t xcb_out;

    xcb_out.output_mode = output_mode;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_end_job_checked (xcb_connection_t *c  /**< */,
                                   uint8_t           cancel  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_END_JOB,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_end_job_request_t xcb_out;

    xcb_out.cancel = cancel;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_end_job (xcb_connection_t *c  /**< */,
                           uint8_t           cancel  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_END_JOB,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_end_job_request_t xcb_out;

    xcb_out.cancel = cancel;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_start_doc_checked (xcb_connection_t *c  /**< */,
                                     uint8_t           driver_mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_START_DOC,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_start_doc_request_t xcb_out;

    xcb_out.driver_mode = driver_mode;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_start_doc (xcb_connection_t *c  /**< */,
                             uint8_t           driver_mode  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_START_DOC,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_start_doc_request_t xcb_out;

    xcb_out.driver_mode = driver_mode;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_end_doc_checked (xcb_connection_t *c  /**< */,
                                   uint8_t           cancel  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_END_DOC,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_end_doc_request_t xcb_out;

    xcb_out.cancel = cancel;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_end_doc (xcb_connection_t *c  /**< */,
                           uint8_t           cancel  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_END_DOC,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_end_doc_request_t xcb_out;

    xcb_out.cancel = cancel;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_x_print_print_put_document_data_sizeof (const void  *_buffer  /**< */,
                                            uint32_t     doc_format_len  /**< */,
                                            uint32_t     options_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_x_print_print_put_document_data_request_t *_aux = (xcb_x_print_print_put_document_data_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_x_print_print_put_document_data_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->len_data * sizeof(uint8_t);
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
    /* doc_format */
    xcb_block_len += doc_format_len * sizeof(xcb_x_print_string8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_x_print_string8_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;
    /* options */
    xcb_block_len += options_len * sizeof(xcb_x_print_string8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_x_print_string8_t);
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
xcb_x_print_print_put_document_data_checked (xcb_connection_t            *c  /**< */,
                                             xcb_drawable_t               drawable  /**< */,
                                             uint32_t                     len_data  /**< */,
                                             uint16_t                     len_fmt  /**< */,
                                             uint16_t                     len_options  /**< */,
                                             const uint8_t               *data  /**< */,
                                             uint32_t                     doc_format_len  /**< */,
                                             const xcb_x_print_string8_t *doc_format  /**< */,
                                             uint32_t                     options_len  /**< */,
                                             const xcb_x_print_string8_t *options  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 8,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_PUT_DOCUMENT_DATA,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[10];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_put_document_data_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.len_data = len_data;
    xcb_out.len_fmt = len_fmt;
    xcb_out.len_options = len_options;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = len_data * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_x_print_string8_t doc_format */
    xcb_parts[6].iov_base = (char *) doc_format;
    xcb_parts[6].iov_len = doc_format_len * sizeof(xcb_x_print_string8_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;
    /* xcb_x_print_string8_t options */
    xcb_parts[8].iov_base = (char *) options;
    xcb_parts[8].iov_len = options_len * sizeof(xcb_x_print_string8_t);
    xcb_parts[9].iov_base = 0;
    xcb_parts[9].iov_len = -xcb_parts[8].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_put_document_data (xcb_connection_t            *c  /**< */,
                                     xcb_drawable_t               drawable  /**< */,
                                     uint32_t                     len_data  /**< */,
                                     uint16_t                     len_fmt  /**< */,
                                     uint16_t                     len_options  /**< */,
                                     const uint8_t               *data  /**< */,
                                     uint32_t                     doc_format_len  /**< */,
                                     const xcb_x_print_string8_t *doc_format  /**< */,
                                     uint32_t                     options_len  /**< */,
                                     const xcb_x_print_string8_t *options  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 8,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_PUT_DOCUMENT_DATA,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[10];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_put_document_data_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.len_data = len_data;
    xcb_out.len_fmt = len_fmt;
    xcb_out.len_options = len_options;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint8_t data */
    xcb_parts[4].iov_base = (char *) data;
    xcb_parts[4].iov_len = len_data * sizeof(uint8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;
    /* xcb_x_print_string8_t doc_format */
    xcb_parts[6].iov_base = (char *) doc_format;
    xcb_parts[6].iov_len = doc_format_len * sizeof(xcb_x_print_string8_t);
    xcb_parts[7].iov_base = 0;
    xcb_parts[7].iov_len = -xcb_parts[6].iov_len & 3;
    /* xcb_x_print_string8_t options */
    xcb_parts[8].iov_base = (char *) options;
    xcb_parts[8].iov_len = options_len * sizeof(xcb_x_print_string8_t);
    xcb_parts[9].iov_base = 0;
    xcb_parts[9].iov_len = -xcb_parts[8].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_x_print_print_get_document_data_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_x_print_print_get_document_data_reply_t *_aux = (xcb_x_print_print_get_document_data_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_x_print_print_get_document_data_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* data */
    xcb_block_len += _aux->dataLen * sizeof(uint8_t);
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

xcb_x_print_print_get_document_data_cookie_t
xcb_x_print_print_get_document_data (xcb_connection_t       *c  /**< */,
                                     xcb_x_print_pcontext_t  context  /**< */,
                                     uint32_t                max_bytes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_DOCUMENT_DATA,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_get_document_data_cookie_t xcb_ret;
    xcb_x_print_print_get_document_data_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.max_bytes = max_bytes;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_document_data_cookie_t
xcb_x_print_print_get_document_data_unchecked (xcb_connection_t       *c  /**< */,
                                               xcb_x_print_pcontext_t  context  /**< */,
                                               uint32_t                max_bytes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_DOCUMENT_DATA,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_get_document_data_cookie_t xcb_ret;
    xcb_x_print_print_get_document_data_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.max_bytes = max_bytes;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint8_t *
xcb_x_print_print_get_document_data_data (const xcb_x_print_print_get_document_data_reply_t *R  /**< */)
{
    return (uint8_t *) (R + 1);
}

int
xcb_x_print_print_get_document_data_data_length (const xcb_x_print_print_get_document_data_reply_t *R  /**< */)
{
    return R->dataLen;
}

xcb_generic_iterator_t
xcb_x_print_print_get_document_data_data_end (const xcb_x_print_print_get_document_data_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint8_t *) (R + 1)) + (R->dataLen);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_x_print_print_get_document_data_reply_t *
xcb_x_print_print_get_document_data_reply (xcb_connection_t                              *c  /**< */,
                                           xcb_x_print_print_get_document_data_cookie_t   cookie  /**< */,
                                           xcb_generic_error_t                          **e  /**< */)
{
    return (xcb_x_print_print_get_document_data_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_x_print_print_start_page_checked (xcb_connection_t *c  /**< */,
                                      xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_START_PAGE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_start_page_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_start_page (xcb_connection_t *c  /**< */,
                              xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_START_PAGE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_start_page_request_t xcb_out;

    xcb_out.window = window;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_end_page_checked (xcb_connection_t *c  /**< */,
                                    uint8_t           cancel  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_END_PAGE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_end_page_request_t xcb_out;

    xcb_out.cancel = cancel;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_end_page (xcb_connection_t *c  /**< */,
                            uint8_t           cancel  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_END_PAGE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_end_page_request_t xcb_out;

    xcb_out.cancel = cancel;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_x_print_print_select_input_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_x_print_print_select_input_request_t *_aux = (xcb_x_print_print_select_input_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_x_print_print_select_input_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* event_list */
    xcb_block_len += xcb_popcount(_aux->event_mask) * sizeof(uint32_t);
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
xcb_x_print_print_select_input_checked (xcb_connection_t       *c  /**< */,
                                        xcb_x_print_pcontext_t  context  /**< */,
                                        uint32_t                event_mask  /**< */,
                                        const uint32_t         *event_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_SELECT_INPUT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_select_input_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.event_mask = event_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t event_list */
    xcb_parts[4].iov_base = (char *) event_list;
    xcb_parts[4].iov_len = xcb_popcount(event_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_select_input (xcb_connection_t       *c  /**< */,
                                xcb_x_print_pcontext_t  context  /**< */,
                                uint32_t                event_mask  /**< */,
                                const uint32_t         *event_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_SELECT_INPUT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_select_input_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.event_mask = event_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* uint32_t event_list */
    xcb_parts[4].iov_base = (char *) event_list;
    xcb_parts[4].iov_len = xcb_popcount(event_mask) * sizeof(uint32_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_x_print_print_input_selected_serialize (void                                           **_buffer  /**< */,
                                            const xcb_x_print_print_input_selected_reply_t  *_aux  /**< */,
                                            const uint32_t                                  *event_list  /**< */,
                                            const uint32_t                                  *all_events_list  /**< */)
{
    char *xcb_out = *_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_align_to = 0;

    unsigned int xcb_pad = 0;
    char xcb_pad0[3] = {0, 0, 0};
    struct iovec xcb_parts[6];
    unsigned int xcb_parts_idx = 0;
    unsigned int xcb_block_len = 0;
    unsigned int i;
    char *xcb_tmp;

    /* xcb_x_print_print_input_selected_reply_t.pad0 */
    xcb_parts[xcb_parts_idx].iov_base = (char *) &xcb_pad;
    xcb_block_len += sizeof(uint8_t);
    xcb_parts[xcb_parts_idx].iov_len = sizeof(uint8_t);
    xcb_parts_idx++;
    xcb_align_to = ALIGNOF(uint8_t);
    /* xcb_x_print_print_input_selected_reply_t.event_mask */
    xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->event_mask;
    xcb_block_len += sizeof(uint32_t);
    xcb_parts[xcb_parts_idx].iov_len = sizeof(uint32_t);
    xcb_parts_idx++;
    xcb_align_to = ALIGNOF(uint32_t);
    /* event_list */
    xcb_parts[xcb_parts_idx].iov_base = (char *) event_list;
    xcb_block_len += xcb_popcount(_aux->event_mask) * sizeof(uint32_t);
    xcb_parts[xcb_parts_idx].iov_len = xcb_popcount(_aux->event_mask) * sizeof(uint32_t);
    xcb_parts_idx++;
    xcb_align_to = ALIGNOF(uint32_t);
    /* xcb_x_print_print_input_selected_reply_t.all_events_mask */
    xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->all_events_mask;
    xcb_block_len += sizeof(uint32_t);
    xcb_parts[xcb_parts_idx].iov_len = sizeof(uint32_t);
    xcb_parts_idx++;
    xcb_align_to = ALIGNOF(uint32_t);
    /* all_events_list */
    xcb_parts[xcb_parts_idx].iov_base = (char *) all_events_list;
    xcb_block_len += xcb_popcount(_aux->all_events_mask) * sizeof(uint32_t);
    xcb_parts[xcb_parts_idx].iov_len = xcb_popcount(_aux->all_events_mask) * sizeof(uint32_t);
    xcb_parts_idx++;
    xcb_align_to = ALIGNOF(uint32_t);
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
xcb_x_print_print_input_selected_unserialize (const void                                 *_buffer  /**< */,
                                              xcb_x_print_print_input_selected_reply_t  **_aux  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    xcb_x_print_print_input_selected_reply_t xcb_out;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    uint32_t *event_list;
    int event_list_len;
    uint32_t *all_events_list;
    int all_events_list_len;

    /* xcb_x_print_print_input_selected_reply_t.response_type */
    xcb_out.response_type = *(uint8_t *)xcb_tmp;
    xcb_block_len += sizeof(uint8_t);
    xcb_tmp += sizeof(uint8_t);
    xcb_align_to = ALIGNOF(uint8_t);
    /* xcb_x_print_print_input_selected_reply_t.pad0 */
    xcb_out.pad0 = *(uint8_t *)xcb_tmp;
    xcb_block_len += sizeof(uint8_t);
    xcb_tmp += sizeof(uint8_t);
    xcb_align_to = ALIGNOF(uint8_t);
    /* xcb_x_print_print_input_selected_reply_t.sequence */
    xcb_out.sequence = *(uint16_t *)xcb_tmp;
    xcb_block_len += sizeof(uint16_t);
    xcb_tmp += sizeof(uint16_t);
    xcb_align_to = ALIGNOF(uint16_t);
    /* xcb_x_print_print_input_selected_reply_t.length */
    xcb_out.length = *(uint32_t *)xcb_tmp;
    xcb_block_len += sizeof(uint32_t);
    xcb_tmp += sizeof(uint32_t);
    xcb_align_to = ALIGNOF(uint32_t);
    /* xcb_x_print_print_input_selected_reply_t.event_mask */
    xcb_out.event_mask = *(uint32_t *)xcb_tmp;
    xcb_block_len += sizeof(uint32_t);
    xcb_tmp += sizeof(uint32_t);
    xcb_align_to = ALIGNOF(uint32_t);
    /* event_list */
    event_list = (uint32_t *)xcb_tmp;
    event_list_len = xcb_popcount(xcb_out.event_mask) * sizeof(uint32_t);
    xcb_block_len += event_list_len;
    xcb_tmp += event_list_len;
    xcb_align_to = ALIGNOF(uint32_t);
    /* xcb_x_print_print_input_selected_reply_t.all_events_mask */
    xcb_out.all_events_mask = *(uint32_t *)xcb_tmp;
    xcb_block_len += sizeof(uint32_t);
    xcb_tmp += sizeof(uint32_t);
    xcb_align_to = ALIGNOF(uint32_t);
    /* all_events_list */
    all_events_list = (uint32_t *)xcb_tmp;
    all_events_list_len = xcb_popcount(xcb_out.all_events_mask) * sizeof(uint32_t);
    xcb_block_len += all_events_list_len;
    xcb_tmp += all_events_list_len;
    xcb_align_to = ALIGNOF(uint32_t);
    /* insert padding */
    xcb_pad = -xcb_block_len & (xcb_align_to - 1);
    xcb_buffer_len += xcb_block_len + xcb_pad;
    if (0 != xcb_pad) {
        xcb_tmp += xcb_pad;
        xcb_pad = 0;
    }
    xcb_block_len = 0;

    if (NULL == _aux)
        return xcb_buffer_len;

    if (NULL == *_aux) {
        /* allocate memory */
        *_aux = malloc(xcb_buffer_len);
    }

    xcb_tmp = ((char *)*_aux)+xcb_buffer_len;
    xcb_tmp -= all_events_list_len;
    memmove(xcb_tmp, all_events_list, all_events_list_len);
    xcb_tmp -= event_list_len;
    memmove(xcb_tmp, event_list, event_list_len);
    **_aux = xcb_out;

    return xcb_buffer_len;
}

int
xcb_x_print_print_input_selected_sizeof (const void  *_buffer  /**< */)
{
    return xcb_x_print_print_input_selected_unserialize(_buffer, NULL);
}

xcb_x_print_print_input_selected_cookie_t
xcb_x_print_print_input_selected (xcb_connection_t       *c  /**< */,
                                  xcb_x_print_pcontext_t  context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_INPUT_SELECTED,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_input_selected_cookie_t xcb_ret;
    xcb_x_print_print_input_selected_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_input_selected_cookie_t
xcb_x_print_print_input_selected_unchecked (xcb_connection_t       *c  /**< */,
                                            xcb_x_print_pcontext_t  context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_INPUT_SELECTED,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_input_selected_cookie_t xcb_ret;
    xcb_x_print_print_input_selected_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

uint32_t *
xcb_x_print_print_input_selected_event_list (const xcb_x_print_print_input_selected_reply_t *R  /**< */)
{
    return (uint32_t *) (R + 1);
}

int
xcb_x_print_print_input_selected_event_list_length (const xcb_x_print_print_input_selected_reply_t *R  /**< */)
{
    return xcb_popcount(R->event_mask);
}

xcb_generic_iterator_t
xcb_x_print_print_input_selected_event_list_end (const xcb_x_print_print_input_selected_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint32_t *) (R + 1)) + (xcb_popcount(R->event_mask));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

uint32_t *
xcb_x_print_print_input_selected_all_events_list (const xcb_x_print_print_input_selected_reply_t *R  /**< */)
{
    xcb_generic_iterator_t prev = xcb_x_print_print_input_selected_event_list_end(R);
    return (uint32_t *) ((char *) prev.data + XCB_TYPE_PAD(uint32_t, prev.index) + 4);
}

int
xcb_x_print_print_input_selected_all_events_list_length (const xcb_x_print_print_input_selected_reply_t *R  /**< */)
{
    return xcb_popcount(R->all_events_mask);
}

xcb_generic_iterator_t
xcb_x_print_print_input_selected_all_events_list_end (const xcb_x_print_print_input_selected_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    xcb_generic_iterator_t child = xcb_x_print_print_input_selected_event_list_end(R);
    i.data = ((uint32_t *) child.data) + (xcb_popcount(R->all_events_mask));
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_x_print_print_input_selected_reply_t *
xcb_x_print_print_input_selected_reply (xcb_connection_t                           *c  /**< */,
                                        xcb_x_print_print_input_selected_cookie_t   cookie  /**< */,
                                        xcb_generic_error_t                       **e  /**< */)
{
    return (xcb_x_print_print_input_selected_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_x_print_print_get_attributes_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_x_print_print_get_attributes_reply_t *_aux = (xcb_x_print_print_get_attributes_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_x_print_print_get_attributes_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* attributes */
    xcb_block_len += _aux->stringLen * sizeof(xcb_x_print_string8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_x_print_string8_t);
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

xcb_x_print_print_get_attributes_cookie_t
xcb_x_print_print_get_attributes (xcb_connection_t       *c  /**< */,
                                  xcb_x_print_pcontext_t  context  /**< */,
                                  uint8_t                 pool  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_ATTRIBUTES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_get_attributes_cookie_t xcb_ret;
    xcb_x_print_print_get_attributes_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.pool = pool;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_attributes_cookie_t
xcb_x_print_print_get_attributes_unchecked (xcb_connection_t       *c  /**< */,
                                            xcb_x_print_pcontext_t  context  /**< */,
                                            uint8_t                 pool  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_ATTRIBUTES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_get_attributes_cookie_t xcb_ret;
    xcb_x_print_print_get_attributes_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.pool = pool;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_string8_t *
xcb_x_print_print_get_attributes_attributes (const xcb_x_print_print_get_attributes_reply_t *R  /**< */)
{
    return (xcb_x_print_string8_t *) (R + 1);
}

int
xcb_x_print_print_get_attributes_attributes_length (const xcb_x_print_print_get_attributes_reply_t *R  /**< */)
{
    return R->stringLen;
}

xcb_generic_iterator_t
xcb_x_print_print_get_attributes_attributes_end (const xcb_x_print_print_get_attributes_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_x_print_string8_t *) (R + 1)) + (R->stringLen);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_x_print_print_get_attributes_reply_t *
xcb_x_print_print_get_attributes_reply (xcb_connection_t                           *c  /**< */,
                                        xcb_x_print_print_get_attributes_cookie_t   cookie  /**< */,
                                        xcb_generic_error_t                       **e  /**< */)
{
    return (xcb_x_print_print_get_attributes_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_x_print_print_get_one_attributes_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_x_print_print_get_one_attributes_request_t *_aux = (xcb_x_print_print_get_one_attributes_request_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_x_print_print_get_one_attributes_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* name */
    xcb_block_len += _aux->nameLen * sizeof(xcb_x_print_string8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_x_print_string8_t);
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

xcb_x_print_print_get_one_attributes_cookie_t
xcb_x_print_print_get_one_attributes (xcb_connection_t            *c  /**< */,
                                      xcb_x_print_pcontext_t       context  /**< */,
                                      uint32_t                     nameLen  /**< */,
                                      uint8_t                      pool  /**< */,
                                      const xcb_x_print_string8_t *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_ONE_ATTRIBUTES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_x_print_print_get_one_attributes_cookie_t xcb_ret;
    xcb_x_print_print_get_one_attributes_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.nameLen = nameLen;
    xcb_out.pool = pool;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_x_print_string8_t name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = nameLen * sizeof(xcb_x_print_string8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_one_attributes_cookie_t
xcb_x_print_print_get_one_attributes_unchecked (xcb_connection_t            *c  /**< */,
                                                xcb_x_print_pcontext_t       context  /**< */,
                                                uint32_t                     nameLen  /**< */,
                                                uint8_t                      pool  /**< */,
                                                const xcb_x_print_string8_t *name  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_ONE_ATTRIBUTES,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[6];
    xcb_x_print_print_get_one_attributes_cookie_t xcb_ret;
    xcb_x_print_print_get_one_attributes_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.nameLen = nameLen;
    xcb_out.pool = pool;
    memset(xcb_out.pad0, 0, 3);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_x_print_string8_t name */
    xcb_parts[4].iov_base = (char *) name;
    xcb_parts[4].iov_len = nameLen * sizeof(xcb_x_print_string8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_string8_t *
xcb_x_print_print_get_one_attributes_value (const xcb_x_print_print_get_one_attributes_reply_t *R  /**< */)
{
    return (xcb_x_print_string8_t *) (R + 1);
}

int
xcb_x_print_print_get_one_attributes_value_length (const xcb_x_print_print_get_one_attributes_reply_t *R  /**< */)
{
    return R->valueLen;
}

xcb_generic_iterator_t
xcb_x_print_print_get_one_attributes_value_end (const xcb_x_print_print_get_one_attributes_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_x_print_string8_t *) (R + 1)) + (R->valueLen);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_x_print_print_get_one_attributes_reply_t *
xcb_x_print_print_get_one_attributes_reply (xcb_connection_t                               *c  /**< */,
                                            xcb_x_print_print_get_one_attributes_cookie_t   cookie  /**< */,
                                            xcb_generic_error_t                           **e  /**< */)
{
    return (xcb_x_print_print_get_one_attributes_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_x_print_print_set_attributes_sizeof (const void  *_buffer  /**< */,
                                         uint32_t     attributes_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_x_print_print_set_attributes_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* attributes */
    xcb_block_len += attributes_len * sizeof(xcb_x_print_string8_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_x_print_string8_t);
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
xcb_x_print_print_set_attributes_checked (xcb_connection_t            *c  /**< */,
                                          xcb_x_print_pcontext_t       context  /**< */,
                                          uint32_t                     stringLen  /**< */,
                                          uint8_t                      pool  /**< */,
                                          uint8_t                      rule  /**< */,
                                          uint32_t                     attributes_len  /**< */,
                                          const xcb_x_print_string8_t *attributes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_SET_ATTRIBUTES,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_set_attributes_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.stringLen = stringLen;
    xcb_out.pool = pool;
    xcb_out.rule = rule;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_x_print_string8_t attributes */
    xcb_parts[4].iov_base = (char *) attributes;
    xcb_parts[4].iov_len = attributes_len * sizeof(xcb_x_print_string8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_x_print_print_set_attributes (xcb_connection_t            *c  /**< */,
                                  xcb_x_print_pcontext_t       context  /**< */,
                                  uint32_t                     stringLen  /**< */,
                                  uint8_t                      pool  /**< */,
                                  uint8_t                      rule  /**< */,
                                  uint32_t                     attributes_len  /**< */,
                                  const xcb_x_print_string8_t *attributes  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_SET_ATTRIBUTES,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_x_print_print_set_attributes_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.stringLen = stringLen;
    xcb_out.pool = pool;
    xcb_out.rule = rule;
    memset(xcb_out.pad0, 0, 2);

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_x_print_string8_t attributes */
    xcb_parts[4].iov_base = (char *) attributes;
    xcb_parts[4].iov_len = attributes_len * sizeof(xcb_x_print_string8_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_page_dimensions_cookie_t
xcb_x_print_print_get_page_dimensions (xcb_connection_t       *c  /**< */,
                                       xcb_x_print_pcontext_t  context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_PAGE_DIMENSIONS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_get_page_dimensions_cookie_t xcb_ret;
    xcb_x_print_print_get_page_dimensions_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_page_dimensions_cookie_t
xcb_x_print_print_get_page_dimensions_unchecked (xcb_connection_t       *c  /**< */,
                                                 xcb_x_print_pcontext_t  context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_PAGE_DIMENSIONS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_get_page_dimensions_cookie_t xcb_ret;
    xcb_x_print_print_get_page_dimensions_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_page_dimensions_reply_t *
xcb_x_print_print_get_page_dimensions_reply (xcb_connection_t                                *c  /**< */,
                                             xcb_x_print_print_get_page_dimensions_cookie_t   cookie  /**< */,
                                             xcb_generic_error_t                            **e  /**< */)
{
    return (xcb_x_print_print_get_page_dimensions_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_x_print_print_query_screens_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_x_print_print_query_screens_reply_t *_aux = (xcb_x_print_print_query_screens_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_x_print_print_query_screens_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* roots */
    xcb_block_len += _aux->listCount * sizeof(xcb_window_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_window_t);
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

xcb_x_print_print_query_screens_cookie_t
xcb_x_print_print_query_screens (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_QUERY_SCREENS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_query_screens_cookie_t xcb_ret;
    xcb_x_print_print_query_screens_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_query_screens_cookie_t
xcb_x_print_print_query_screens_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_QUERY_SCREENS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_query_screens_cookie_t xcb_ret;
    xcb_x_print_print_query_screens_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_window_t *
xcb_x_print_print_query_screens_roots (const xcb_x_print_print_query_screens_reply_t *R  /**< */)
{
    return (xcb_window_t *) (R + 1);
}

int
xcb_x_print_print_query_screens_roots_length (const xcb_x_print_print_query_screens_reply_t *R  /**< */)
{
    return R->listCount;
}

xcb_generic_iterator_t
xcb_x_print_print_query_screens_roots_end (const xcb_x_print_print_query_screens_reply_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((xcb_window_t *) (R + 1)) + (R->listCount);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_x_print_print_query_screens_reply_t *
xcb_x_print_print_query_screens_reply (xcb_connection_t                          *c  /**< */,
                                       xcb_x_print_print_query_screens_cookie_t   cookie  /**< */,
                                       xcb_generic_error_t                      **e  /**< */)
{
    return (xcb_x_print_print_query_screens_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_x_print_print_set_image_resolution_cookie_t
xcb_x_print_print_set_image_resolution (xcb_connection_t       *c  /**< */,
                                        xcb_x_print_pcontext_t  context  /**< */,
                                        uint16_t                image_resolution  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_SET_IMAGE_RESOLUTION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_set_image_resolution_cookie_t xcb_ret;
    xcb_x_print_print_set_image_resolution_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.image_resolution = image_resolution;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_set_image_resolution_cookie_t
xcb_x_print_print_set_image_resolution_unchecked (xcb_connection_t       *c  /**< */,
                                                  xcb_x_print_pcontext_t  context  /**< */,
                                                  uint16_t                image_resolution  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_SET_IMAGE_RESOLUTION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_set_image_resolution_cookie_t xcb_ret;
    xcb_x_print_print_set_image_resolution_request_t xcb_out;

    xcb_out.context = context;
    xcb_out.image_resolution = image_resolution;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_set_image_resolution_reply_t *
xcb_x_print_print_set_image_resolution_reply (xcb_connection_t                                 *c  /**< */,
                                              xcb_x_print_print_set_image_resolution_cookie_t   cookie  /**< */,
                                              xcb_generic_error_t                             **e  /**< */)
{
    return (xcb_x_print_print_set_image_resolution_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_x_print_print_get_image_resolution_cookie_t
xcb_x_print_print_get_image_resolution (xcb_connection_t       *c  /**< */,
                                        xcb_x_print_pcontext_t  context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_IMAGE_RESOLUTION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_get_image_resolution_cookie_t xcb_ret;
    xcb_x_print_print_get_image_resolution_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_image_resolution_cookie_t
xcb_x_print_print_get_image_resolution_unchecked (xcb_connection_t       *c  /**< */,
                                                  xcb_x_print_pcontext_t  context  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_x_print_id,
        /* opcode */ XCB_X_PRINT_PRINT_GET_IMAGE_RESOLUTION,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_x_print_print_get_image_resolution_cookie_t xcb_ret;
    xcb_x_print_print_get_image_resolution_request_t xcb_out;

    xcb_out.context = context;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_x_print_print_get_image_resolution_reply_t *
xcb_x_print_print_get_image_resolution_reply (xcb_connection_t                                 *c  /**< */,
                                              xcb_x_print_print_get_image_resolution_cookie_t   cookie  /**< */,
                                              xcb_generic_error_t                             **e  /**< */)
{
    return (xcb_x_print_print_get_image_resolution_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

