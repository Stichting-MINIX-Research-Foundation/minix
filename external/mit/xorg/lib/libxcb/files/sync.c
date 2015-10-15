/*
 * This file generated automatically from sync.xml by c_client.py.
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
#include "sync.h"

#define ALIGNOF(type) offsetof(struct { char dummy; type member; }, member)
#include "xproto.h"

xcb_extension_t xcb_sync_id = { "SYNC", 0 };

void
xcb_sync_alarm_next (xcb_sync_alarm_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_sync_alarm_t);
}

xcb_generic_iterator_t
xcb_sync_alarm_end (xcb_sync_alarm_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_sync_counter_next (xcb_sync_counter_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_sync_counter_t);
}

xcb_generic_iterator_t
xcb_sync_counter_end (xcb_sync_counter_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_sync_fence_next (xcb_sync_fence_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_sync_fence_t);
}

xcb_generic_iterator_t
xcb_sync_fence_end (xcb_sync_fence_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_sync_int64_next (xcb_sync_int64_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_sync_int64_t);
}

xcb_generic_iterator_t
xcb_sync_int64_end (xcb_sync_int64_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

int
xcb_sync_systemcounter_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_sync_systemcounter_t *_aux = (xcb_sync_systemcounter_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_sync_systemcounter_t);
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
xcb_sync_systemcounter_name (const xcb_sync_systemcounter_t *R  /**< */)
{
    return (char *) (R + 1);
}

int
xcb_sync_systemcounter_name_length (const xcb_sync_systemcounter_t *R  /**< */)
{
    return R->name_len;
}

xcb_generic_iterator_t
xcb_sync_systemcounter_name_end (const xcb_sync_systemcounter_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((char *) (R + 1)) + (R->name_len);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}

void
xcb_sync_systemcounter_next (xcb_sync_systemcounter_iterator_t *i  /**< */)
{
    xcb_sync_systemcounter_t *R = i->data;
    xcb_generic_iterator_t child;
    child.data = (xcb_sync_systemcounter_t *)(((char *)R) + xcb_sync_systemcounter_sizeof(R));
    i->index = (char *) child.data - (char *) i->data;
    --i->rem;
    i->data = (xcb_sync_systemcounter_t *) child.data;
}

xcb_generic_iterator_t
xcb_sync_systemcounter_end (xcb_sync_systemcounter_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_sync_systemcounter_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}

void
xcb_sync_trigger_next (xcb_sync_trigger_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_sync_trigger_t);
}

xcb_generic_iterator_t
xcb_sync_trigger_end (xcb_sync_trigger_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

void
xcb_sync_waitcondition_next (xcb_sync_waitcondition_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_sync_waitcondition_t);
}

xcb_generic_iterator_t
xcb_sync_waitcondition_end (xcb_sync_waitcondition_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}

xcb_sync_initialize_cookie_t
xcb_sync_initialize (xcb_connection_t *c  /**< */,
                     uint8_t           desired_major_version  /**< */,
                     uint8_t           desired_minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_INITIALIZE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_sync_initialize_cookie_t xcb_ret;
    xcb_sync_initialize_request_t xcb_out;

    xcb_out.desired_major_version = desired_major_version;
    xcb_out.desired_minor_version = desired_minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_initialize_cookie_t
xcb_sync_initialize_unchecked (xcb_connection_t *c  /**< */,
                               uint8_t           desired_major_version  /**< */,
                               uint8_t           desired_minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_INITIALIZE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_sync_initialize_cookie_t xcb_ret;
    xcb_sync_initialize_request_t xcb_out;

    xcb_out.desired_major_version = desired_major_version;
    xcb_out.desired_minor_version = desired_minor_version;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_initialize_reply_t *
xcb_sync_initialize_reply (xcb_connection_t              *c  /**< */,
                           xcb_sync_initialize_cookie_t   cookie  /**< */,
                           xcb_generic_error_t          **e  /**< */)
{
    return (xcb_sync_initialize_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_sync_list_system_counters_sizeof (const void  *_buffer  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    const xcb_sync_list_system_counters_reply_t *_aux = (xcb_sync_list_system_counters_reply_t *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;

    unsigned int i;
    unsigned int xcb_tmp_len;

    xcb_block_len += sizeof(xcb_sync_list_system_counters_reply_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* counters */
    for(i=0; i<_aux->counters_len; i++) {
        xcb_tmp_len = xcb_sync_systemcounter_sizeof(xcb_tmp);
        xcb_block_len += xcb_tmp_len;
        xcb_tmp += xcb_tmp_len;
    }
    xcb_align_to = ALIGNOF(xcb_sync_systemcounter_t);
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

xcb_sync_list_system_counters_cookie_t
xcb_sync_list_system_counters (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_LIST_SYSTEM_COUNTERS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_sync_list_system_counters_cookie_t xcb_ret;
    xcb_sync_list_system_counters_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_list_system_counters_cookie_t
xcb_sync_list_system_counters_unchecked (xcb_connection_t *c  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_LIST_SYSTEM_COUNTERS,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_sync_list_system_counters_cookie_t xcb_ret;
    xcb_sync_list_system_counters_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_sync_list_system_counters_counters_length (const xcb_sync_list_system_counters_reply_t *R  /**< */)
{
    return R->counters_len;
}

xcb_sync_systemcounter_iterator_t
xcb_sync_list_system_counters_counters_iterator (const xcb_sync_list_system_counters_reply_t *R  /**< */)
{
    xcb_sync_systemcounter_iterator_t i;
    i.data = (xcb_sync_systemcounter_t *) (R + 1);
    i.rem = R->counters_len;
    i.index = (char *) i.data - (char *) R;
    return i;
}

xcb_sync_list_system_counters_reply_t *
xcb_sync_list_system_counters_reply (xcb_connection_t                        *c  /**< */,
                                     xcb_sync_list_system_counters_cookie_t   cookie  /**< */,
                                     xcb_generic_error_t                    **e  /**< */)
{
    return (xcb_sync_list_system_counters_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_sync_create_counter_checked (xcb_connection_t   *c  /**< */,
                                 xcb_sync_counter_t  id  /**< */,
                                 xcb_sync_int64_t    initial_value  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_CREATE_COUNTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_create_counter_request_t xcb_out;

    xcb_out.id = id;
    xcb_out.initial_value = initial_value;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_create_counter (xcb_connection_t   *c  /**< */,
                         xcb_sync_counter_t  id  /**< */,
                         xcb_sync_int64_t    initial_value  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_CREATE_COUNTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_create_counter_request_t xcb_out;

    xcb_out.id = id;
    xcb_out.initial_value = initial_value;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_destroy_counter_checked (xcb_connection_t   *c  /**< */,
                                  xcb_sync_counter_t  counter  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_DESTROY_COUNTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_destroy_counter_request_t xcb_out;

    xcb_out.counter = counter;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_destroy_counter (xcb_connection_t   *c  /**< */,
                          xcb_sync_counter_t  counter  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_DESTROY_COUNTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_destroy_counter_request_t xcb_out;

    xcb_out.counter = counter;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_query_counter_cookie_t
xcb_sync_query_counter (xcb_connection_t   *c  /**< */,
                        xcb_sync_counter_t  counter  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_QUERY_COUNTER,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_sync_query_counter_cookie_t xcb_ret;
    xcb_sync_query_counter_request_t xcb_out;

    xcb_out.counter = counter;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_query_counter_cookie_t
xcb_sync_query_counter_unchecked (xcb_connection_t   *c  /**< */,
                                  xcb_sync_counter_t  counter  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_QUERY_COUNTER,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_sync_query_counter_cookie_t xcb_ret;
    xcb_sync_query_counter_request_t xcb_out;

    xcb_out.counter = counter;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_query_counter_reply_t *
xcb_sync_query_counter_reply (xcb_connection_t                 *c  /**< */,
                              xcb_sync_query_counter_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */)
{
    return (xcb_sync_query_counter_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_sync_await_sizeof (const void  *_buffer  /**< */,
                       uint32_t     wait_list_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_sync_await_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* wait_list */
    xcb_block_len += wait_list_len * sizeof(xcb_sync_waitcondition_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_sync_waitcondition_t);
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
xcb_sync_await_checked (xcb_connection_t               *c  /**< */,
                        uint32_t                        wait_list_len  /**< */,
                        const xcb_sync_waitcondition_t *wait_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_AWAIT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_await_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_sync_waitcondition_t wait_list */
    xcb_parts[4].iov_base = (char *) wait_list;
    xcb_parts[4].iov_len = wait_list_len * sizeof(xcb_sync_waitcondition_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_await (xcb_connection_t               *c  /**< */,
                uint32_t                        wait_list_len  /**< */,
                const xcb_sync_waitcondition_t *wait_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_AWAIT,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_await_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_sync_waitcondition_t wait_list */
    xcb_parts[4].iov_base = (char *) wait_list;
    xcb_parts[4].iov_len = wait_list_len * sizeof(xcb_sync_waitcondition_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_change_counter_checked (xcb_connection_t   *c  /**< */,
                                 xcb_sync_counter_t  counter  /**< */,
                                 xcb_sync_int64_t    amount  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_CHANGE_COUNTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_change_counter_request_t xcb_out;

    xcb_out.counter = counter;
    xcb_out.amount = amount;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_change_counter (xcb_connection_t   *c  /**< */,
                         xcb_sync_counter_t  counter  /**< */,
                         xcb_sync_int64_t    amount  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_CHANGE_COUNTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_change_counter_request_t xcb_out;

    xcb_out.counter = counter;
    xcb_out.amount = amount;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_set_counter_checked (xcb_connection_t   *c  /**< */,
                              xcb_sync_counter_t  counter  /**< */,
                              xcb_sync_int64_t    value  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_SET_COUNTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_set_counter_request_t xcb_out;

    xcb_out.counter = counter;
    xcb_out.value = value;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_set_counter (xcb_connection_t   *c  /**< */,
                      xcb_sync_counter_t  counter  /**< */,
                      xcb_sync_int64_t    value  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_SET_COUNTER,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_set_counter_request_t xcb_out;

    xcb_out.counter = counter;
    xcb_out.value = value;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

int
xcb_sync_create_alarm_value_list_serialize (void                                     **_buffer  /**< */,
                                            uint32_t                                   value_mask  /**< */,
                                            const xcb_sync_create_alarm_value_list_t  *_aux  /**< */)
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

    if(value_mask & XCB_SYNC_CA_COUNTER) {
        /* xcb_sync_create_alarm_value_list_t.counter */
        xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->counter;
        xcb_block_len += sizeof(xcb_sync_counter_t);
        xcb_parts[xcb_parts_idx].iov_len = sizeof(xcb_sync_counter_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(xcb_sync_counter_t);
    }
    if(value_mask & XCB_SYNC_CA_VALUE_TYPE) {
        /* xcb_sync_create_alarm_value_list_t.valueType */
        xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->valueType;
        xcb_block_len += sizeof(uint32_t);
        xcb_parts[xcb_parts_idx].iov_len = sizeof(uint32_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint32_t);
    }
    if(value_mask & XCB_SYNC_CA_VALUE) {
        /* xcb_sync_create_alarm_value_list_t.value */
        xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->value;
        xcb_block_len += sizeof(xcb_sync_int64_t);
        xcb_parts[xcb_parts_idx].iov_len = sizeof(xcb_sync_int64_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(xcb_sync_int64_t);
    }
    if(value_mask & XCB_SYNC_CA_TEST_TYPE) {
        /* xcb_sync_create_alarm_value_list_t.testType */
        xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->testType;
        xcb_block_len += sizeof(uint32_t);
        xcb_parts[xcb_parts_idx].iov_len = sizeof(uint32_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint32_t);
    }
    if(value_mask & XCB_SYNC_CA_DELTA) {
        /* xcb_sync_create_alarm_value_list_t.delta */
        xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->delta;
        xcb_block_len += sizeof(xcb_sync_int64_t);
        xcb_parts[xcb_parts_idx].iov_len = sizeof(xcb_sync_int64_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(xcb_sync_int64_t);
    }
    if(value_mask & XCB_SYNC_CA_EVENTS) {
        /* xcb_sync_create_alarm_value_list_t.events */
        xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->events;
        xcb_block_len += sizeof(uint32_t);
        xcb_parts[xcb_parts_idx].iov_len = sizeof(uint32_t);
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
xcb_sync_create_alarm_value_list_unpack (const void                          *_buffer  /**< */,
                                         uint32_t                             value_mask  /**< */,
                                         xcb_sync_create_alarm_value_list_t  *_aux  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    if(value_mask & XCB_SYNC_CA_COUNTER) {
        /* xcb_sync_create_alarm_value_list_t.counter */
        _aux->counter = *(xcb_sync_counter_t *)xcb_tmp;
        xcb_block_len += sizeof(xcb_sync_counter_t);
        xcb_tmp += sizeof(xcb_sync_counter_t);
        xcb_align_to = ALIGNOF(xcb_sync_counter_t);
    }
    if(value_mask & XCB_SYNC_CA_VALUE_TYPE) {
        /* xcb_sync_create_alarm_value_list_t.valueType */
        _aux->valueType = *(uint32_t *)xcb_tmp;
        xcb_block_len += sizeof(uint32_t);
        xcb_tmp += sizeof(uint32_t);
        xcb_align_to = ALIGNOF(uint32_t);
    }
    if(value_mask & XCB_SYNC_CA_VALUE) {
        /* xcb_sync_create_alarm_value_list_t.value */
        _aux->value = *(xcb_sync_int64_t *)xcb_tmp;
        xcb_block_len += sizeof(xcb_sync_int64_t);
        xcb_tmp += sizeof(xcb_sync_int64_t);
        xcb_align_to = ALIGNOF(xcb_sync_int64_t);
    }
    if(value_mask & XCB_SYNC_CA_TEST_TYPE) {
        /* xcb_sync_create_alarm_value_list_t.testType */
        _aux->testType = *(uint32_t *)xcb_tmp;
        xcb_block_len += sizeof(uint32_t);
        xcb_tmp += sizeof(uint32_t);
        xcb_align_to = ALIGNOF(uint32_t);
    }
    if(value_mask & XCB_SYNC_CA_DELTA) {
        /* xcb_sync_create_alarm_value_list_t.delta */
        _aux->delta = *(xcb_sync_int64_t *)xcb_tmp;
        xcb_block_len += sizeof(xcb_sync_int64_t);
        xcb_tmp += sizeof(xcb_sync_int64_t);
        xcb_align_to = ALIGNOF(xcb_sync_int64_t);
    }
    if(value_mask & XCB_SYNC_CA_EVENTS) {
        /* xcb_sync_create_alarm_value_list_t.events */
        _aux->events = *(uint32_t *)xcb_tmp;
        xcb_block_len += sizeof(uint32_t);
        xcb_tmp += sizeof(uint32_t);
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
xcb_sync_create_alarm_value_list_sizeof (const void  *_buffer  /**< */,
                                         uint32_t     value_mask  /**< */)
{
    xcb_sync_create_alarm_value_list_t _aux;
    return xcb_sync_create_alarm_value_list_unpack(_buffer, value_mask, &_aux);
}

xcb_void_cookie_t
xcb_sync_create_alarm_checked (xcb_connection_t *c  /**< */,
                               xcb_sync_alarm_t  id  /**< */,
                               uint32_t          value_mask  /**< */,
                               const void       *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_CREATE_ALARM,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_create_alarm_request_t xcb_out;

    xcb_out.id = id;
    xcb_out.value_mask = value_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_sync_create_alarm_value_list_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len =
      xcb_sync_create_alarm_value_list_sizeof (value_list, value_mask);

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_create_alarm (xcb_connection_t *c  /**< */,
                       xcb_sync_alarm_t  id  /**< */,
                       uint32_t          value_mask  /**< */,
                       const void       *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_CREATE_ALARM,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_create_alarm_request_t xcb_out;

    xcb_out.id = id;
    xcb_out.value_mask = value_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_sync_create_alarm_value_list_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len =
      xcb_sync_create_alarm_value_list_sizeof (value_list, value_mask);

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_create_alarm_aux_checked (xcb_connection_t                         *c  /**< */,
                                   xcb_sync_alarm_t                          id  /**< */,
                                   uint32_t                                  value_mask  /**< */,
                                   const xcb_sync_create_alarm_value_list_t *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_CREATE_ALARM,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_create_alarm_request_t xcb_out;
    void *xcb_aux0 = 0;

    xcb_out.id = id;
    xcb_out.value_mask = value_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_sync_create_alarm_value_list_t value_list */
    xcb_parts[4].iov_len =
      xcb_sync_create_alarm_value_list_serialize (&xcb_aux0, value_mask, value_list);
    xcb_parts[4].iov_base = xcb_aux0;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    free(xcb_aux0);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_create_alarm_aux (xcb_connection_t                         *c  /**< */,
                           xcb_sync_alarm_t                          id  /**< */,
                           uint32_t                                  value_mask  /**< */,
                           const xcb_sync_create_alarm_value_list_t *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_CREATE_ALARM,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_create_alarm_request_t xcb_out;
    void *xcb_aux0 = 0;

    xcb_out.id = id;
    xcb_out.value_mask = value_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_sync_create_alarm_value_list_t value_list */
    xcb_parts[4].iov_len =
      xcb_sync_create_alarm_value_list_serialize (&xcb_aux0, value_mask, value_list);
    xcb_parts[4].iov_base = xcb_aux0;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    free(xcb_aux0);
    return xcb_ret;
}

int
xcb_sync_change_alarm_value_list_serialize (void                                     **_buffer  /**< */,
                                            uint32_t                                   value_mask  /**< */,
                                            const xcb_sync_change_alarm_value_list_t  *_aux  /**< */)
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

    if(value_mask & XCB_SYNC_CA_COUNTER) {
        /* xcb_sync_change_alarm_value_list_t.counter */
        xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->counter;
        xcb_block_len += sizeof(xcb_sync_counter_t);
        xcb_parts[xcb_parts_idx].iov_len = sizeof(xcb_sync_counter_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(xcb_sync_counter_t);
    }
    if(value_mask & XCB_SYNC_CA_VALUE_TYPE) {
        /* xcb_sync_change_alarm_value_list_t.valueType */
        xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->valueType;
        xcb_block_len += sizeof(uint32_t);
        xcb_parts[xcb_parts_idx].iov_len = sizeof(uint32_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint32_t);
    }
    if(value_mask & XCB_SYNC_CA_VALUE) {
        /* xcb_sync_change_alarm_value_list_t.value */
        xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->value;
        xcb_block_len += sizeof(xcb_sync_int64_t);
        xcb_parts[xcb_parts_idx].iov_len = sizeof(xcb_sync_int64_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(xcb_sync_int64_t);
    }
    if(value_mask & XCB_SYNC_CA_TEST_TYPE) {
        /* xcb_sync_change_alarm_value_list_t.testType */
        xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->testType;
        xcb_block_len += sizeof(uint32_t);
        xcb_parts[xcb_parts_idx].iov_len = sizeof(uint32_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(uint32_t);
    }
    if(value_mask & XCB_SYNC_CA_DELTA) {
        /* xcb_sync_change_alarm_value_list_t.delta */
        xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->delta;
        xcb_block_len += sizeof(xcb_sync_int64_t);
        xcb_parts[xcb_parts_idx].iov_len = sizeof(xcb_sync_int64_t);
        xcb_parts_idx++;
        xcb_align_to = ALIGNOF(xcb_sync_int64_t);
    }
    if(value_mask & XCB_SYNC_CA_EVENTS) {
        /* xcb_sync_change_alarm_value_list_t.events */
        xcb_parts[xcb_parts_idx].iov_base = (char *) &_aux->events;
        xcb_block_len += sizeof(uint32_t);
        xcb_parts[xcb_parts_idx].iov_len = sizeof(uint32_t);
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
xcb_sync_change_alarm_value_list_unpack (const void                          *_buffer  /**< */,
                                         uint32_t                             value_mask  /**< */,
                                         xcb_sync_change_alarm_value_list_t  *_aux  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    if(value_mask & XCB_SYNC_CA_COUNTER) {
        /* xcb_sync_change_alarm_value_list_t.counter */
        _aux->counter = *(xcb_sync_counter_t *)xcb_tmp;
        xcb_block_len += sizeof(xcb_sync_counter_t);
        xcb_tmp += sizeof(xcb_sync_counter_t);
        xcb_align_to = ALIGNOF(xcb_sync_counter_t);
    }
    if(value_mask & XCB_SYNC_CA_VALUE_TYPE) {
        /* xcb_sync_change_alarm_value_list_t.valueType */
        _aux->valueType = *(uint32_t *)xcb_tmp;
        xcb_block_len += sizeof(uint32_t);
        xcb_tmp += sizeof(uint32_t);
        xcb_align_to = ALIGNOF(uint32_t);
    }
    if(value_mask & XCB_SYNC_CA_VALUE) {
        /* xcb_sync_change_alarm_value_list_t.value */
        _aux->value = *(xcb_sync_int64_t *)xcb_tmp;
        xcb_block_len += sizeof(xcb_sync_int64_t);
        xcb_tmp += sizeof(xcb_sync_int64_t);
        xcb_align_to = ALIGNOF(xcb_sync_int64_t);
    }
    if(value_mask & XCB_SYNC_CA_TEST_TYPE) {
        /* xcb_sync_change_alarm_value_list_t.testType */
        _aux->testType = *(uint32_t *)xcb_tmp;
        xcb_block_len += sizeof(uint32_t);
        xcb_tmp += sizeof(uint32_t);
        xcb_align_to = ALIGNOF(uint32_t);
    }
    if(value_mask & XCB_SYNC_CA_DELTA) {
        /* xcb_sync_change_alarm_value_list_t.delta */
        _aux->delta = *(xcb_sync_int64_t *)xcb_tmp;
        xcb_block_len += sizeof(xcb_sync_int64_t);
        xcb_tmp += sizeof(xcb_sync_int64_t);
        xcb_align_to = ALIGNOF(xcb_sync_int64_t);
    }
    if(value_mask & XCB_SYNC_CA_EVENTS) {
        /* xcb_sync_change_alarm_value_list_t.events */
        _aux->events = *(uint32_t *)xcb_tmp;
        xcb_block_len += sizeof(uint32_t);
        xcb_tmp += sizeof(uint32_t);
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
xcb_sync_change_alarm_value_list_sizeof (const void  *_buffer  /**< */,
                                         uint32_t     value_mask  /**< */)
{
    xcb_sync_change_alarm_value_list_t _aux;
    return xcb_sync_change_alarm_value_list_unpack(_buffer, value_mask, &_aux);
}

xcb_void_cookie_t
xcb_sync_change_alarm_checked (xcb_connection_t *c  /**< */,
                               xcb_sync_alarm_t  id  /**< */,
                               uint32_t          value_mask  /**< */,
                               const void       *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_CHANGE_ALARM,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_change_alarm_request_t xcb_out;

    xcb_out.id = id;
    xcb_out.value_mask = value_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_sync_change_alarm_value_list_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len =
      xcb_sync_change_alarm_value_list_sizeof (value_list, value_mask);

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_change_alarm (xcb_connection_t *c  /**< */,
                       xcb_sync_alarm_t  id  /**< */,
                       uint32_t          value_mask  /**< */,
                       const void       *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_CHANGE_ALARM,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_change_alarm_request_t xcb_out;

    xcb_out.id = id;
    xcb_out.value_mask = value_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_sync_change_alarm_value_list_t value_list */
    xcb_parts[4].iov_base = (char *) value_list;
    xcb_parts[4].iov_len =
      xcb_sync_change_alarm_value_list_sizeof (value_list, value_mask);

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_change_alarm_aux_checked (xcb_connection_t                         *c  /**< */,
                                   xcb_sync_alarm_t                          id  /**< */,
                                   uint32_t                                  value_mask  /**< */,
                                   const xcb_sync_change_alarm_value_list_t *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_CHANGE_ALARM,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_change_alarm_request_t xcb_out;
    void *xcb_aux0 = 0;

    xcb_out.id = id;
    xcb_out.value_mask = value_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_sync_change_alarm_value_list_t value_list */
    xcb_parts[4].iov_len =
      xcb_sync_change_alarm_value_list_serialize (&xcb_aux0, value_mask, value_list);
    xcb_parts[4].iov_base = xcb_aux0;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    free(xcb_aux0);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_change_alarm_aux (xcb_connection_t                         *c  /**< */,
                           xcb_sync_alarm_t                          id  /**< */,
                           uint32_t                                  value_mask  /**< */,
                           const xcb_sync_change_alarm_value_list_t *value_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 3,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_CHANGE_ALARM,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[5];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_change_alarm_request_t xcb_out;
    void *xcb_aux0 = 0;

    xcb_out.id = id;
    xcb_out.value_mask = value_mask;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_sync_change_alarm_value_list_t value_list */
    xcb_parts[4].iov_len =
      xcb_sync_change_alarm_value_list_serialize (&xcb_aux0, value_mask, value_list);
    xcb_parts[4].iov_base = xcb_aux0;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    free(xcb_aux0);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_destroy_alarm_checked (xcb_connection_t *c  /**< */,
                                xcb_sync_alarm_t  alarm  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_DESTROY_ALARM,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_destroy_alarm_request_t xcb_out;

    xcb_out.alarm = alarm;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_destroy_alarm (xcb_connection_t *c  /**< */,
                        xcb_sync_alarm_t  alarm  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_DESTROY_ALARM,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_destroy_alarm_request_t xcb_out;

    xcb_out.alarm = alarm;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_query_alarm_cookie_t
xcb_sync_query_alarm (xcb_connection_t *c  /**< */,
                      xcb_sync_alarm_t  alarm  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_QUERY_ALARM,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_sync_query_alarm_cookie_t xcb_ret;
    xcb_sync_query_alarm_request_t xcb_out;

    xcb_out.alarm = alarm;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_query_alarm_cookie_t
xcb_sync_query_alarm_unchecked (xcb_connection_t *c  /**< */,
                                xcb_sync_alarm_t  alarm  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_QUERY_ALARM,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_sync_query_alarm_cookie_t xcb_ret;
    xcb_sync_query_alarm_request_t xcb_out;

    xcb_out.alarm = alarm;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_query_alarm_reply_t *
xcb_sync_query_alarm_reply (xcb_connection_t               *c  /**< */,
                            xcb_sync_query_alarm_cookie_t   cookie  /**< */,
                            xcb_generic_error_t           **e  /**< */)
{
    return (xcb_sync_query_alarm_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_sync_set_priority_checked (xcb_connection_t *c  /**< */,
                               uint32_t          id  /**< */,
                               int32_t           priority  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_SET_PRIORITY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_set_priority_request_t xcb_out;

    xcb_out.id = id;
    xcb_out.priority = priority;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_set_priority (xcb_connection_t *c  /**< */,
                       uint32_t          id  /**< */,
                       int32_t           priority  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_SET_PRIORITY,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_set_priority_request_t xcb_out;

    xcb_out.id = id;
    xcb_out.priority = priority;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_get_priority_cookie_t
xcb_sync_get_priority (xcb_connection_t *c  /**< */,
                       uint32_t          id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_GET_PRIORITY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_sync_get_priority_cookie_t xcb_ret;
    xcb_sync_get_priority_request_t xcb_out;

    xcb_out.id = id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_get_priority_cookie_t
xcb_sync_get_priority_unchecked (xcb_connection_t *c  /**< */,
                                 uint32_t          id  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_GET_PRIORITY,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_sync_get_priority_cookie_t xcb_ret;
    xcb_sync_get_priority_request_t xcb_out;

    xcb_out.id = id;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_get_priority_reply_t *
xcb_sync_get_priority_reply (xcb_connection_t                *c  /**< */,
                             xcb_sync_get_priority_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */)
{
    return (xcb_sync_get_priority_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

xcb_void_cookie_t
xcb_sync_create_fence_checked (xcb_connection_t *c  /**< */,
                               xcb_drawable_t    drawable  /**< */,
                               xcb_sync_fence_t  fence  /**< */,
                               uint8_t           initially_triggered  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_CREATE_FENCE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_create_fence_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.fence = fence;
    xcb_out.initially_triggered = initially_triggered;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_create_fence (xcb_connection_t *c  /**< */,
                       xcb_drawable_t    drawable  /**< */,
                       xcb_sync_fence_t  fence  /**< */,
                       uint8_t           initially_triggered  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_CREATE_FENCE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_create_fence_request_t xcb_out;

    xcb_out.drawable = drawable;
    xcb_out.fence = fence;
    xcb_out.initially_triggered = initially_triggered;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_trigger_fence_checked (xcb_connection_t *c  /**< */,
                                xcb_sync_fence_t  fence  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_TRIGGER_FENCE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_trigger_fence_request_t xcb_out;

    xcb_out.fence = fence;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_trigger_fence (xcb_connection_t *c  /**< */,
                        xcb_sync_fence_t  fence  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_TRIGGER_FENCE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_trigger_fence_request_t xcb_out;

    xcb_out.fence = fence;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_reset_fence_checked (xcb_connection_t *c  /**< */,
                              xcb_sync_fence_t  fence  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_RESET_FENCE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_reset_fence_request_t xcb_out;

    xcb_out.fence = fence;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_reset_fence (xcb_connection_t *c  /**< */,
                      xcb_sync_fence_t  fence  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_RESET_FENCE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_reset_fence_request_t xcb_out;

    xcb_out.fence = fence;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_destroy_fence_checked (xcb_connection_t *c  /**< */,
                                xcb_sync_fence_t  fence  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_DESTROY_FENCE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_destroy_fence_request_t xcb_out;

    xcb_out.fence = fence;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_destroy_fence (xcb_connection_t *c  /**< */,
                        xcb_sync_fence_t  fence  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_DESTROY_FENCE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_destroy_fence_request_t xcb_out;

    xcb_out.fence = fence;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_query_fence_cookie_t
xcb_sync_query_fence (xcb_connection_t *c  /**< */,
                      xcb_sync_fence_t  fence  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_QUERY_FENCE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_sync_query_fence_cookie_t xcb_ret;
    xcb_sync_query_fence_request_t xcb_out;

    xcb_out.fence = fence;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_query_fence_cookie_t
xcb_sync_query_fence_unchecked (xcb_connection_t *c  /**< */,
                                xcb_sync_fence_t  fence  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_QUERY_FENCE,
        /* isvoid */ 0
    };

    struct iovec xcb_parts[4];
    xcb_sync_query_fence_cookie_t xcb_ret;
    xcb_sync_query_fence_request_t xcb_out;

    xcb_out.fence = fence;

    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_sync_query_fence_reply_t *
xcb_sync_query_fence_reply (xcb_connection_t               *c  /**< */,
                            xcb_sync_query_fence_cookie_t   cookie  /**< */,
                            xcb_generic_error_t           **e  /**< */)
{
    return (xcb_sync_query_fence_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

int
xcb_sync_await_fence_sizeof (const void  *_buffer  /**< */,
                             uint32_t     fence_list_len  /**< */)
{
    char *xcb_tmp = (char *)_buffer;
    unsigned int xcb_buffer_len = 0;
    unsigned int xcb_block_len = 0;
    unsigned int xcb_pad = 0;
    unsigned int xcb_align_to = 0;


    xcb_block_len += sizeof(xcb_sync_await_fence_request_t);
    xcb_tmp += xcb_block_len;
    xcb_buffer_len += xcb_block_len;
    xcb_block_len = 0;
    /* fence_list */
    xcb_block_len += fence_list_len * sizeof(xcb_sync_fence_t);
    xcb_tmp += xcb_block_len;
    xcb_align_to = ALIGNOF(xcb_sync_fence_t);
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
xcb_sync_await_fence_checked (xcb_connection_t       *c  /**< */,
                              uint32_t                fence_list_len  /**< */,
                              const xcb_sync_fence_t *fence_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_AWAIT_FENCE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_await_fence_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_sync_fence_t fence_list */
    xcb_parts[4].iov_base = (char *) fence_list;
    xcb_parts[4].iov_len = fence_list_len * sizeof(xcb_sync_fence_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

xcb_void_cookie_t
xcb_sync_await_fence (xcb_connection_t       *c  /**< */,
                      uint32_t                fence_list_len  /**< */,
                      const xcb_sync_fence_t *fence_list  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 4,
        /* ext */ &xcb_sync_id,
        /* opcode */ XCB_SYNC_AWAIT_FENCE,
        /* isvoid */ 1
    };

    struct iovec xcb_parts[6];
    xcb_void_cookie_t xcb_ret;
    xcb_sync_await_fence_request_t xcb_out;


    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    /* xcb_sync_fence_t fence_list */
    xcb_parts[4].iov_base = (char *) fence_list;
    xcb_parts[4].iov_len = fence_list_len * sizeof(xcb_sync_fence_t);
    xcb_parts[5].iov_base = 0;
    xcb_parts[5].iov_len = -xcb_parts[4].iov_len & 3;

    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}

