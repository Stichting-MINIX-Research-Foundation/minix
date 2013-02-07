/* omapip_p.h

   Private master include file for the OMAPI library. */

/*
 * Copyright (c) 2009-2010 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004,2007 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   https://www.isc.org/
 *
 * This software has been written for Internet Systems Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about Internet Systems Consortium, see
 * ``https://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

#ifndef __OMAPIP_OMAPIP_P_H__
#define __OMAPIP_OMAPIP_P_H__

#ifndef __CYGWIN32__
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include <netdb.h>
#else
#define fd_set cygwin_fd_set
#include <sys/types.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>

/*
 * XXX: I'm not sure why these were here.
#include "cdefs.h"
#include "osdep.h"
 */

#include <dst/dst.h>
#include "result.h"

#include <omapip/convert.h>
#include <omapip/hash.h>
#include <omapip/omapip.h>
#include <omapip/trace.h>

/* DST_API control flags */
/* These are used in functions dst_sign_data and dst_verify_data */
#define SIG_MODE_INIT		1  /* initalize digest */
#define SIG_MODE_UPDATE		2  /* add data to digest */
#define SIG_MODE_FINAL		4  /* generate/verify signature */
#define SIG_MODE_ALL		(SIG_MODE_INIT|SIG_MODE_UPDATE|SIG_MODE_FINAL)

/* OMAPI protocol header, version 1.00 */
typedef struct {
	u_int32_t authlen;	/* Length of authenticator. */
	u_int32_t authid;	/* Authenticator object ID. */
	u_int32_t op;		/* Opcode. */
	omapi_handle_t handle;	/* Handle of object being operated on,
                                   or zero. */
	u_int32_t id;		/* Transaction ID. */
	u_int32_t rid;	/* ID of transaction to which this is a response. */
} omapi_protocol_header_t;

#define OMAPI_PROTOCOL_VERSION	100

#define OMAPI_OP_OPEN		1
#define OMAPI_OP_REFRESH	2
#define	OMAPI_OP_UPDATE		3
#define OMAPI_OP_NOTIFY		4
#define OMAPI_OP_STATUS		5
#define OMAPI_OP_DELETE		6

typedef enum {
	omapi_connection_unconnected,
	omapi_connection_connecting,
	omapi_connection_connected,
	omapi_connection_disconnecting,
	omapi_connection_closed
} omapi_connection_state_t;

typedef enum {
	omapi_protocol_intro_wait,
	omapi_protocol_header_wait,
	omapi_protocol_signature_wait,
	omapi_protocol_name_wait,
	omapi_protocol_name_length_wait,
	omapi_protocol_value_wait,
	omapi_protocol_value_length_wait
} omapi_protocol_state_t;

typedef struct __omapi_message_object {
	OMAPI_OBJECT_PREAMBLE;
	struct __omapi_message_object *next, *prev;
	omapi_object_t *object;
	omapi_object_t *notify_object;
	struct __omapi_protocol_object *protocol_object;
	u_int32_t authlen;
	omapi_typed_data_t *authenticator;
	u_int32_t authid;
	omapi_object_t *id_object;
	u_int32_t op;
	u_int32_t h;
	u_int32_t id;
	u_int32_t rid;
} omapi_message_object_t;

typedef struct __omapi_remote_auth {
	struct __omapi_remote_auth *next;
	omapi_handle_t remote_handle;
	omapi_object_t *a;
} omapi_remote_auth_t;

typedef struct __omapi_protocol_object {
	OMAPI_OBJECT_PREAMBLE;
	u_int32_t header_size;		
	u_int32_t protocol_version;
	u_int32_t next_xid;

	omapi_protocol_state_t state;	/* Input state. */
	int reading_message_values;	/* True if reading message-specific
					   values. */
	omapi_message_object_t *message;	/* Incoming message. */
	omapi_data_string_t *name;	/* Incoming name. */
	omapi_typed_data_t *value;	/* Incoming value. */
	isc_result_t verify_result;
	omapi_remote_auth_t *default_auth; /* Default authinfo to use. */
	omapi_remote_auth_t *remote_auth_list;	/* Authenticators active on
						   this connection. */

	isc_boolean_t insecure;		/* Set to allow unauthenticated
					   messages. */

	isc_result_t (*verify_auth) (omapi_object_t *, omapi_auth_key_t *);
} omapi_protocol_object_t;

typedef struct {
	OMAPI_OBJECT_PREAMBLE;

	isc_boolean_t insecure;		/* Set to allow unauthenticated
					   messages. */

	isc_result_t (*verify_auth) (omapi_object_t *, omapi_auth_key_t *);
} omapi_protocol_listener_object_t;

#include <omapip/buffer.h>

typedef struct __omapi_listener_object {
	OMAPI_OBJECT_PREAMBLE;
	int socket;		/* Connection socket. */
	int index;
	struct sockaddr_in address;
	isc_result_t (*verify_addr) (omapi_object_t *, omapi_addr_t *);
} omapi_listener_object_t;

typedef struct __omapi_connection_object {
	OMAPI_OBJECT_PREAMBLE;
	int socket;		/* Connection socket. */
	int32_t index;
	omapi_connection_state_t state;
	struct sockaddr_in remote_addr;
	struct sockaddr_in local_addr;
	omapi_addr_list_t *connect_list;	/* List of addresses to which
						   to connect. */
	int cptr;		/* Current element we are connecting to. */
	u_int32_t bytes_needed;	/* Bytes of input needed before wakeup. */
	u_int32_t in_bytes;	/* Bytes of input already buffered. */
	omapi_buffer_t *inbufs;
	u_int32_t out_bytes;	/* Bytes of output in buffers. */
	omapi_buffer_t *outbufs;
	omapi_listener_object_t *listener;	/* Listener that accepted this
						   connection, if any. */
	dst_key_t *in_key;	/* Authenticator signing incoming
				   data. */
	void *in_context;	/* Input hash context. */
	dst_key_t *out_key;	/* Authenticator signing outgoing
				   data. */
	void *out_context;	/* Output hash context. */
} omapi_connection_object_t;

typedef struct __omapi_io_object {
	OMAPI_OBJECT_PREAMBLE;
	struct __omapi_io_object *next;
	int (*readfd) (omapi_object_t *);
	int (*writefd) (omapi_object_t *);
	isc_result_t (*reader) (omapi_object_t *);
	isc_result_t (*writer) (omapi_object_t *);
	isc_result_t (*reaper) (omapi_object_t *);
	isc_socket_t *fd;
	isc_boolean_t closed; /* ISC_TRUE = closed, do not use */
} omapi_io_object_t;

typedef struct __omapi_generic_object {
	OMAPI_OBJECT_PREAMBLE;
	omapi_value_t **values;
	u_int8_t *changed;
	int nvalues, va_max;
} omapi_generic_object_t;

typedef struct __omapi_waiter_object {
	OMAPI_OBJECT_PREAMBLE;
	int ready;
	isc_result_t waitstatus;
	struct __omapi_waiter_object *next;
} omapi_waiter_object_t;

#define OMAPI_HANDLE_TABLE_SIZE 120

typedef struct __omapi_handle_table {
	omapi_handle_t first, limit;
	omapi_handle_t next;
	int leafp;
	union {
		omapi_object_t *object;
		struct __omapi_handle_table *table;
	} children [OMAPI_HANDLE_TABLE_SIZE];
} omapi_handle_table_t;

#include <omapip/alloc.h>

OMAPI_OBJECT_ALLOC_DECL (omapi_protocol, omapi_protocol_object_t,
			 omapi_type_protocol)
OMAPI_OBJECT_ALLOC_DECL (omapi_protocol_listener,
			 omapi_protocol_listener_object_t,
			 omapi_type_protocol_listener)
OMAPI_OBJECT_ALLOC_DECL (omapi_connection,
			 omapi_connection_object_t, omapi_type_connection)
OMAPI_OBJECT_ALLOC_DECL (omapi_listener,
			 omapi_listener_object_t, omapi_type_listener)
OMAPI_OBJECT_ALLOC_DECL (omapi_io,
			 omapi_io_object_t, omapi_type_io_object)
OMAPI_OBJECT_ALLOC_DECL (omapi_waiter,
			 omapi_waiter_object_t, omapi_type_waiter)
OMAPI_OBJECT_ALLOC_DECL (omapi_generic,
			 omapi_generic_object_t, omapi_type_generic)
OMAPI_OBJECT_ALLOC_DECL (omapi_message,
			 omapi_message_object_t, omapi_type_message)

isc_result_t omapi_connection_sign_data (int mode,
					 dst_key_t *key,
					 void **context,
					 const unsigned char *data,
					 const unsigned len,
					 omapi_typed_data_t **result);
isc_result_t omapi_listener_connect (omapi_connection_object_t **obj,
				     omapi_listener_object_t *listener,
				     int socket,
				     struct sockaddr_in *remote_addr);
void omapi_listener_trace_setup (void);
void omapi_connection_trace_setup (void);
void omapi_buffer_trace_setup (void);
void omapi_connection_register (omapi_connection_object_t *,
				const char *, int);
OMAPI_ARRAY_TYPE_DECL(omapi_listener, omapi_listener_object_t);
OMAPI_ARRAY_TYPE_DECL(omapi_connection, omapi_connection_object_t);

isc_result_t omapi_handle_clear(omapi_handle_t);

extern int log_priority;
extern int log_perror;
extern void (*log_cleanup) (void);

void log_fatal (const char *, ...)
	__attribute__((__format__(__printf__,1,2)));
int log_error (const char *, ...)
	__attribute__((__format__(__printf__,1,2)));
int log_info (const char *, ...)
	__attribute__((__format__(__printf__,1,2)));
int log_debug (const char *, ...)
	__attribute__((__format__(__printf__,1,2)));
void do_percentm (char *obuf, const char *ibuf);

isc_result_t uerr2isc (int);
isc_result_t ns_rcode_to_isc (int);

extern omapi_message_object_t *omapi_registered_messages;

#endif /* __OMAPIP_OMAPIP_P_H__ */
