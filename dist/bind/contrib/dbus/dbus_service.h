/*  D-BUS Service Utilities
 *  
 *  Provides utilities for construction of D-BUS "Services"  
 *
 *  Copyright(C) Jason Vas Dias, Red Hat Inc., 2005
 *  Modified by Adam Tkac, Red Hat Inc., 2007
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation at 
 *           http://www.fsf.org/licensing/licenses/gpl.txt
 *  and included in this software distribution as the "LICENSE" file.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 */

#ifndef D_BUS_SERVER_UTILITIES_H
#define D_BUS_SERVER_UTILITIES_H

#include <stdint.h>
#include <stdarg.h>
#include <isc/types.h>

typedef struct dbcs_s* DBUS_SVC;

typedef enum 
{ HANDLED, NOT_HANDLED, HANDLED_NOW 
} dbus_svc_HandlerResult;

typedef enum 
{  INVALID, CALL, RETURN, ERROR, SIGNAL 
} dbus_svc_MessageType;

typedef enum 
{
    DBUS_SESSION,
    DBUS_SYSTEM, 
    DBUS_STARTER,
    DBUS_PRIVATE_SYSTEM,
    DBUS_PRIVATE_SESSION
} dbus_svc_DBUS_TYPE;

typedef enum /* D-BUS Protocol Type Codes / Signature Chars */
{
   TYPE_INVALID  =   (int)'\0',
   TYPE_BYTE     =   (int)'y',
   TYPE_BOOLEAN  =   (int)'b',
   TYPE_INT16    =   (int)'n',
   TYPE_UINT16   =   (int)'q',
   TYPE_INT32    =   (int)'i',
   TYPE_UINT32   =   (int)'u',
   TYPE_INT64    =   (int)'x',
   TYPE_UINT64   =   (int)'t',
   TYPE_DOUBLE   =   (int)'d',
   TYPE_STRING   =   (int)'s',
   TYPE_OBJECT_PATH =(int)'o',
   TYPE_SIGNATURE=   (int)'g',
   TYPE_ARRAY    =   (int)'a',
   TYPE_VARIANT  =   (int)'v',
   TYPE_STRUCT   =   (int)'r',
   TYPE_DICT_ENTRY = (int)'e',
   STRUCT_BEGIN  =   (int)'(',
   STRUCT_END    =   (int)')',
   DICT_ENTRY_BEGIN =(int)'{',
   DICT_ENTRY_END   =(int)'}'
} dbus_svc_DataType;

typedef struct DBusMessage* dbus_svc_MessageHandle;

typedef int 
(*dbus_svc_ErrorHandler)
( const char *errorFmt, ...
); /* Error Handler function prototype - handle FATAL errors from D-BUS calls */

typedef enum
{
    WATCH_ENABLE = 8,
    WATCH_ERROR  = 4,
    WATCH_WRITE  = 2,
    WATCH_READ   = 1
} dbus_svc_WatchFlags;

typedef void (*dbus_svc_WatchHandler)( int, dbus_svc_WatchFlags, void *arg );

typedef dbus_svc_HandlerResult 
(*dbus_svc_MessageHandler)
( DBUS_SVC dbus,
  dbus_svc_MessageType type,
  uint8_t  reply_expected,   /* 1 / 0 */
  uint32_t serial,           /* serial number of message; needed to reply */
  const char *destination,         /* D-BUS connection name / destination */
  const char *path,                /* D-BUS Object Path */  
  const char *member,              /* D-BUS Object Member */
  const char *interface,           /* D-BUS Object interface */
  const char *if_suffix,           /* remainder of interface prefixed by ifPrefix */
  const char *sender,              /* Senders' connection destination */
  const char *signature,           /* Signature String composed of Type Codes      */
  dbus_svc_MessageHandle msg,/* Message pointer: call dbus_svc_get_args(msg,...) to get data */
  const char *prefix,              /* If non-null, this is the root prefix for this sub-path message */ 
  const char *suffix,              /* If non-null, this is the suffix of this sub-path message */
  void *prefixObject,        /* If non-null, this is the object that was registered for the prefix */
  void *object               /* If non-null, this is the object that was registered for the complete path */
); /* Message Handler function prototype */

#define DBusMsgHandlerArgs \
  DBUS_SVC dbus, \
  dbus_svc_MessageType type, \
  uint8_t  reply_expected, \
  uint32_t serial,         \
  const char *destination,       \
  const char *path,              \
  const char *member,            \
  const char *interface,         \
  const char *if_suffix,         \
  const char *sender,            \
  const char *signature,         \
  dbus_svc_MessageHandle msg, \
  const char *prefix,            \
  const char *suffix,            \
  void *prefixObject,      \
  void *object             

#define SHUTDOWN 255

extern isc_result_t dbus_svc_init
( dbus_svc_DBUS_TYPE bus, 
  char *name,                         /* name to register with D-BUS */
  DBUS_SVC *dbus,                     /* dbus handle */
  dbus_svc_WatchHandler wh,           /* optional handler for watch events */
  dbus_svc_ErrorHandler eh,           /* optional error log message handler */
  dbus_svc_ErrorHandler dh,           /* optional debug / info log message handler */
  void *wh_arg                        /* optional watch handler arg */
); 
/*
 * Obtains connection to DBUS_BUS_STARTER and registers "name".
 * "eh" will be called for all errors from this server session.
 */

/* EITHER :
 *        pass a NULL WatchHandler to dbus_svc_init and use dbus_svc_main_loop
 * OR:
 *        supply a valid WatchHandler, and call dbus_svc_handle_watch when 
 *        select() returns the watch fd as ready for the watch action, and
 *        call dbus_svc_dispatch when all watches have been handled.
 */        


uint8_t
dbus_svc_add_filter
(  DBUS_SVC, dbus_svc_MessageHandler mh, void *obj, int n_matches, ... );
/*
 * Registers SINGLE message handler to handle ALL messages, adding match rules
 */

void  dbus_svc_main_loop( DBUS_SVC, void (*idle_handler)(DBUS_SVC) ); 
 
void  dbus_svc_handle_watch( DBUS_SVC, int watch_fd, dbus_svc_WatchFlags action);

void  dbus_svc_dispatch( DBUS_SVC );

/*
 * Enter message processing loop.
 * If "idle_handler" is non-null, it will be called once per iteration of loop.
 */

const char *dbus_svc_unique_name( DBUS_SVC );
/*
 * Returns connection "unique" (socket) name
 */

void  dbus_svc_quit( DBUS_SVC );
/*
 * Exit message processing loop
 */

void  dbus_svc_shutdown( DBUS_SVC );
/*
 * Close connections and clean up. 
 * DBUS_SVC pointer is invalid after this.
 */

uint8_t
dbus_svc_get_args( DBUS_SVC, dbus_svc_MessageHandle, dbus_svc_DataType, ... );
/* get arguments from message  */

uint8_t
dbus_svc_get_args_va( DBUS_SVC, dbus_svc_MessageHandle, dbus_svc_DataType, va_list );
/* get arguments from message  */


typedef void (*dbus_svc_ShutdownHandler) ( DBUS_SVC, void * );
uint8_t
dbus_svc_add_shutdown_filter
(
    DBUS_SVC, dbus_svc_ShutdownHandler sh, void *obj 
);
/* Registers a filter for D-BUS shutdown event.
 * Cannot be used in conjunction with dbus_svc_add_message_filter.
 */

uint8_t
dbus_svc_remove_message_filter
( DBUS_SVC, dbus_svc_MessageHandler mh);
/* Unregisters the message filter */

uint8_t 
dbus_svc_send
( DBUS_SVC,
  dbus_svc_MessageType type,
  int32_t reply_serial,
  uint32_t *new_serial,
  const char *destination,
  const char *path,
  const char *member,
  const char *interface,
  dbus_svc_DataType firstType,
  ... /* pointer, { (dbus_svc_DataType, pointer )...} */ 
); /* sends messages / replies to "destination" */

uint8_t 
dbus_svc_send_va
( DBUS_SVC cs,
  dbus_svc_MessageType type,
  int32_t reply_serial,
  uint32_t *new_serial,
  const char *destination,
  const char *path,
  const char *member,
  const char *interface,
  dbus_svc_DataType firstType,
  va_list va
); /* sends messages / replies to "destination" */

dbus_svc_MessageHandle
dbus_svc_call
( DBUS_SVC cs,
  const char *destination,
  const char *path,
  const char *member,
  const char *interface,
  dbus_svc_DataType firstType,
  ...
); /* constructs message, sends it, returns reply */

dbus_svc_MessageHandle
dbus_svc_new_message
( DBUS_SVC cs,
  dbus_svc_MessageType type,
  int32_t reply_serial,
  const char *destination,
  const char *path,
  const char *interface,
  const char *member
);

uint8_t
dbus_svc_send_message(DBUS_SVC , dbus_svc_MessageHandle , uint32_t *  );

uint8_t
dbus_svc_message_append_args( DBUS_SVC , dbus_svc_MessageHandle, dbus_svc_DataType, ...);

typedef struct DBusMessageIter *dbus_svc_MessageIterator;

dbus_svc_MessageIterator
dbus_svc_message_iterator_new( DBUS_SVC, dbus_svc_MessageHandle );

uint32_t
dbus_svc_message_next_arg_type( DBUS_SVC, dbus_svc_MessageIterator );

void
dbus_svc_message_next_arg( DBUS_SVC, dbus_svc_MessageIterator,  void * );

uint32_t
dbus_svc_message_element_type( DBUS_SVC, dbus_svc_MessageIterator );

void
dbus_svc_message_get_elements( DBUS_SVC, dbus_svc_MessageIterator, uint32_t *n, void *array );

uint8_t dbus_svc_message_type( dbus_svc_MessageHandle );

void dbus_svc_message_iterator_free(  DBUS_SVC, dbus_svc_MessageIterator  );

#endif
