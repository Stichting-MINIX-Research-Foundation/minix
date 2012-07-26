/*  dbus_service.c
 *
 *  D-BUS Service Utilities
 *  
 *  Provides MINIMAL utilities for construction of D-BUS "Services".
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

#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
extern size_t strnlen(const char *s, size_t maxlen);
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <search.h>
#include <getopt.h>
typedef void (*__free_fn_t) (void *__nodep);
extern void tdestroy (void *__root, __free_fn_t __freefct);
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#define  DBUS_API_SUBJECT_TO_CHANGE  "Very Annoying and Silly!"
#include <dbus/dbus.h>

#include <named/dbus_service.h>
#include <isc/result.h>

typedef struct dbcs_s
{
    DBusConnection *connection;
    DBusDispatchStatus dispatchStatus;
    uint32_t        status;
    dbus_svc_WatchHandler wh;
    void *          wh_arg;
    const char    * unique_name;
    dbus_svc_MessageHandler mh;
    void          * def_mh_obj;
    dbus_svc_MessageHandler mf;
    void          * def_mf_obj;
    dbus_svc_ShutdownHandler sh;
    void          * sh_obj;  
    dbus_svc_ErrorHandler eh;
    dbus_svc_ErrorHandler dh;
    /*{ glibc b-trees: */
    void *          roots;
    void *          timeouts;  
    void *          watches;
    void *          filters;
    /*}*/
    int             n; 
    fd_set          r_fds;
    fd_set          s_r_fds;
    fd_set          w_fds;
    fd_set          s_w_fds;
    fd_set          e_fds;  
    fd_set          s_e_fds;
    DBusMessage     *currentMessage;
    int             rejectMessage;
} DBusConnectionState;

typedef struct root_s
{
    char *path;    
    char *if_prefix;
    DBUS_SVC cs;
    dbus_svc_MessageHandler mh;        
    void *object;
    void *tree;
} Root;

typedef struct mhn_s
{
    char *path;    
    dbus_svc_MessageHandler mh;    
    void *object;
} MessageHandlerNode;

typedef struct mfn_s
{
    DBusConnectionState *cs;
    dbus_svc_MessageHandler mf;
    void *obj;
    int n_matches;
    char **matches;
} MessageFilterNode;

typedef struct dbto_s
{
    DBusTimeout   *to;
    DBusConnectionState *cs;
    struct timeval tv;
} DBusConnectionTimeout;

static void no_free( void *p){ p=0; }

static int ptr_key_comparator( const void *p1, const void *p2 )
{
    return
	(  (p1 == p2) 
	   ? 0
	   :( (p1 > p2)
	      ? 1
	      : -1
	    )
	);
}

static DBusHandlerResult
default_message_filter 
(   DBusConnection     *connection,
    DBusMessage        *message,
    void               *p
)
{
    DBusConnectionState *cs = p;
    uint32_t type  =dbus_message_get_type( message ),
	   serial  =dbus_message_get_serial( message );
    uint8_t  reply =dbus_message_get_no_reply( message )==0;
    const char 
	*path =    dbus_message_get_path( message ),
	*dest =    dbus_message_get_destination( message ),
	*member =  dbus_message_get_member( message ),
	*interface=dbus_message_get_interface( message ),
	*sender   =dbus_message_get_sender( message ),
	*signature=dbus_message_get_signature( message );
    connection = connection;
    if(cs->mf)
	return
	(*(cs->mf))( cs, type, reply, serial, dest, path, member, interface, 0L,
		    sender, signature, message, 0L, 0L, 0L, cs->def_mf_obj
	          ) ;
    return HANDLED;
}

uint8_t
dbus_svc_add_filter
(  DBusConnectionState *cs, dbus_svc_MessageHandler mh, void *obj, int n_matches, ... )
{
    DBusError error;
    va_list va;
    char *m;

    va_start(va, n_matches );
    
    cs->mf = mh;
    cs->def_mf_obj = obj;

    if ( ! dbus_connection_add_filter (cs->connection, default_message_filter, cs, NULL))
    {
	if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_add_filter: dbus_connection_add_filter failed");
	va_end(va);
	return( 0 );
    }

    if( n_matches )
    {
	memset(&error,'\0',sizeof(DBusError));
	dbus_error_init(&error);
	while( n_matches-- )
	{
	    m = va_arg(va, char* ) ;

	    dbus_bus_add_match(cs->connection, m, &error);

	    if( dbus_error_is_set(&error))
	    {
		if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_add_filter: dbus_bus_add_match failed for %s: %s %s",
					       m, error.name, error.message
		                              );
		va_end(va);
		return(0);
	    }
	}
    }
    va_end(va);
    return( 1 );
}


uint8_t
dbus_svc_get_args_va(DBusConnectionState *cs, DBusMessage* msg, dbus_svc_DataType firstType, va_list va)
{
    DBusError error;
    memset(&error,'\0',sizeof(DBusError));
    dbus_error_init(&error);
    if( (!dbus_message_get_args_valist(msg, &error, firstType, va)) || dbus_error_is_set(&error) )
    {
	if(  dbus_error_is_set(&error) )
	{
	    if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_get_args failed: %s %s",error.name, error.message);
	    dbus_error_free(&error);
	}else
	    if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_get_args failed: dbus_message_get_args_valist failed");
	return( 0 );
    }
    return( 1 );
}

uint8_t
dbus_svc_get_args(DBusConnectionState *cs, DBusMessage* msg, dbus_svc_DataType firstType, ...)
{
    va_list va;
    uint8_t r;
    va_start(va, firstType);
    r = dbus_svc_get_args_va( cs, msg, firstType, va);
    va_end(va);
    return r;
}

uint8_t 
dbus_svc_send_va
(  DBusConnectionState *cs,
   dbus_svc_MessageType type,   
   int32_t reply_serial,
   uint32_t *new_serial,
   const char *destination,
   const char *path,
   const char *interface,
   const char *member,
   dbus_svc_DataType firstType,
   va_list va
)
{
    DBusMessageIter iter;
    char *e;
    DBusMessage *msg = 
	dbus_svc_new_message
	(   cs,
	    type,
	    reply_serial,
	    destination,
	    path,
	    interface,
	    member
	);

    if(msg == 0L)
    {
	if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_send: dbus_svc_new_message failed");
	return 0;
    }

    if( type != DBUS_MESSAGE_TYPE_ERROR )
    {
	if( !dbus_message_append_args_valist( msg, firstType, va ) )
	{
	    if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_send: dbus_message_append_args_valist failed");
	    return 0;
	}
    }else
    {
	if( firstType == DBUS_TYPE_STRING )
	{
	    e = 0L;
	    e = va_arg( va, char* );	    
	    if( (e == 0L) ||  !dbus_message_set_error_name( msg, e ) )
	    {
		if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_send: dbus_message_set_error_name failed");
		return 0;
	    }
	    firstType = va_arg(va, int);
	    if( firstType == DBUS_TYPE_STRING )
	    {
		e = 0L;
		e =  va_arg( va, char* );
		if( e == 0L )
		{
		    if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_send: NULL error message");
		    return 0;		    		    
		}
		dbus_message_iter_init_append (msg, &iter);		
		if( !dbus_message_iter_append_basic 
		    (&iter, DBUS_TYPE_STRING, &e)
		  )
		{
		    if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_send: dbus_message_iter_append_basic failed");
		    return 0;		    		    
		}
	    }
	}else
	{
	    if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_send: unhandled type for error name: %c", firstType);
	    return 0;	    
	}
    }

    if( !dbus_connection_send(cs->connection, msg, new_serial) )
    {
	if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_send: dbus_message_send failed");
	return 0;
    }
    if( cs->dh != 0L ) (*(cs->dh))("Sending message");
    dbus_connection_flush(cs->connection);
    return 1;
}

uint8_t 
dbus_svc_send
(  DBusConnectionState *cs,
   dbus_svc_MessageType type,
   int32_t reply_serial,
   uint32_t *new_serial,
   const char *destination,
   const char *path,
   const char *interface,
   const char *member,
   dbus_svc_DataType firstType,
   ...
)
{
    uint8_t r;
    va_list va;
    va_start(va, firstType);
    r = dbus_svc_send_va(cs, type, reply_serial, new_serial, destination, path,interface,member,firstType,va);
    va_end(va);
    return ( r ) ;
}

dbus_svc_MessageHandle
dbus_svc_new_message
( DBusConnectionState* cs,
  dbus_svc_MessageType type,
  int32_t reply_serial,
  const char *destination,
  const char *path,
  const char *interface,
  const char *member
)
{
    DBusMessage *msg = dbus_message_new(type);
    
    if( msg == 0L)
    {
	if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_new_message: dbus_message_set_reply_serial failed");
	return 0;
    }

    if( reply_serial != -1 )
    {
	if( !dbus_message_set_reply_serial(msg,reply_serial) )
	{
	    if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_new_message: dbus_message_set_reply_serial failed");
	    return 0;
	}    
    }
	    
    if( (destination !=0L) && !dbus_message_set_destination(msg, destination) )
    {
	if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_new_message: dbus_message_set_destination failed");
	return 0;
    }

    if( !dbus_message_set_path(msg, path) )
    {
	if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_new_message: dbus_message_set_path failed");
	return 0;
    }

    if( ! dbus_message_set_interface(msg,interface) )
    {
	if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_new_message: dbus_message_set_interface failed - %s", interface);
	return 0;
    }

    if( !dbus_message_set_member(msg,member) )
    {
	if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_new_message: dbus_message_set_member failed");
	return 0;
    }

    return msg;    
}

extern uint8_t
dbus_svc_send_message
(
    DBusConnectionState *cs, 
    dbus_svc_MessageHandle msg, 
    uint32_t *new_serial 
)
{
    if( !dbus_connection_send(cs->connection, msg, new_serial) )
    {
	if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_send: dbus_message_send failed");
	return 0;
    }
    if( cs->dh != 0L ) (*(cs->dh))("Sending message");
    dbus_connection_flush(cs->connection);   
    return 1;
}

uint8_t
dbus_svc_message_append_args(DBusConnectionState *cs, dbus_svc_MessageHandle msg, dbus_svc_DataType firstType, ...)
{
    va_list va;
    va_start(va, firstType);
    if( !dbus_message_append_args_valist( msg, firstType, va ) )
    {
	if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_send: dbus_message_append_args failed");
	va_end(va);
	return 0;	
    }
    va_end(va);
    return ( 1 ) ;
}

dbus_svc_MessageHandle 
dbus_svc_call
( DBusConnectionState *cs,
  const char *destination,
  const char *path,
  const char *member,
  const char *interface,
  dbus_svc_DataType firstType,
  ... 
)
{
    DBusMessage *message=0L, *reply=0L;
    va_list va;
    DBusError error;
    int reply_timeout=20000;

    va_start(va, firstType);

    memset(&error,'\0',sizeof(DBusError));
    dbus_error_init(&error);

    if(( message = 
	 dbus_message_new_method_call 
	 (  destination,
	    path,
	    interface,
	    member
	 )
       ) == 0L
      )
    {
	if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_call: dbus_message_new_method_call failed");
	va_end(va);
	return(0L);
    };

    if( !dbus_message_append_args_valist( message, firstType, va ) )
    {
	if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_call: dbus_message_append_args_valist failed");
	va_end(va);
	return(0L);
    }

    if((reply = 
	dbus_connection_send_with_reply_and_block 
	(cs->connection,
	 message, reply_timeout,
	 &error
	)
       ) == 0L    
      )
    {
	if( cs->eh != 0L ) (*(cs->eh))("dbus_svc_call: dbus_message_send_with_reply_and_block failed: %s %s",
		    error.name, error.message
	           );
	va_end(va);
	return(0L);
    }
    va_end(va);
    return reply;
}

dbus_svc_MessageIterator
dbus_svc_message_iterator_new( DBusConnectionState *cs, DBusMessage *msg)
{
    DBusMessageIter *iter = malloc( sizeof(DBusMessageIter) );
    void *p =cs;
    p++;
    if( iter != 0L )
    {
	if( !dbus_message_iter_init( msg, iter ))
	{
	    free( iter ) ;
	    iter = 0L;
	}
    }
    return iter;
}

uint32_t
dbus_svc_message_next_arg_type( DBusConnectionState *cs, dbus_svc_MessageIterator iter )
{
    void *p =cs;
    p++;
    return dbus_message_iter_get_arg_type( iter );
}

void
dbus_svc_message_next_arg( DBusConnectionState *cs, dbus_svc_MessageIterator iter, void *value )
{
    void *p =cs;
    p++;
    dbus_message_iter_get_basic( iter, value );
    dbus_message_iter_next( iter );
}

uint32_t
dbus_svc_message_element_type(DBusConnectionState *cs , dbus_svc_MessageIterator  iter)
{
    void *p =cs;
    p++;
    return dbus_message_iter_get_element_type(iter);
}

void
dbus_svc_message_get_elements( DBusConnectionState *cs , dbus_svc_MessageIterator  iter, uint32_t *n, void *array )
{
    void *p =cs;
    p++;
    dbus_message_iter_get_fixed_array( iter, n, array);
}

void dbus_svc_message_iterator_free(  DBusConnectionState *cs, dbus_svc_MessageIterator iter )
{
    void *p =cs;
    p++;
    free( iter );
}

uint8_t dbus_svc_message_type(  DBusMessage *msg )
{
    return dbus_message_get_type( msg );
}

static DBusConnectionState *
dbcs_new( DBusConnection *connection )
{
    DBusConnectionState *dbcs = (DBusConnectionState *) malloc( sizeof(DBusConnectionState) );
    if ( dbcs )
    {
	memset( dbcs, '\0', sizeof( DBusConnectionState ));
	dbcs->connection = connection;
    }
    return(dbcs);
}

static DBusConnectionTimeout *
timeout_new( DBusTimeout *timeout )
{
    DBusConnectionTimeout *to = (DBusConnectionTimeout *) malloc ( sizeof(DBusConnectionTimeout) );
    if( to != 0L )
    {
	to->to = timeout;
	dbus_timeout_set_data(timeout, to, 0L);
	if( dbus_timeout_get_enabled(timeout) )
	    gettimeofday(&(to->tv),0L);
	else
	{
	    to->tv.tv_sec = 0 ;
	    to->tv.tv_usec = 0 ;
	}	    
    }
    return( to );
}

static dbus_bool_t
add_timeout( DBusTimeout *timeout, void *csp )
{
    DBusConnectionState   *cs = csp;
    DBusConnectionTimeout *to = timeout_new(timeout);
    if( cs->dh != 0L ) (*(cs->dh))("add_timeout: %d", dbus_timeout_get_interval(timeout));
    to->cs = cs;
    if ( to )
    {
	if( tsearch((void*)to, &(cs->timeouts), ptr_key_comparator) != 0L )
	    return TRUE;
    }
    if( cs->eh != 0L ) (*(cs->eh))("add_timeout: out of memory");
    return FALSE;
}

static void
remove_timeout( DBusTimeout *timeout, void *csp )
{
    DBusConnectionState   *cs = csp;
    DBusConnectionTimeout *to = dbus_timeout_get_data(timeout);
    if( (to != 0L) && (to->to == timeout) )
    {
	if( cs->dh != 0L ) (*(cs->dh))("remove_timeout: %d", dbus_timeout_get_interval(to->to));
	if( tdelete((const void*)to, &(cs->timeouts), ptr_key_comparator) != 0L )
	{
	    free(to);
	}else
	    if( cs->eh != 0L ) (*(cs->eh))("remove_timeout: can't happen?!?: timeout data %p not found", to);   	
    }else
	if( cs->eh != 0L ) (*(cs->eh))("remove_timeout: can't happen?!?: timeout %p did not record data %p %p", 
		    timeout, to, ((to != 0L) ? to->to : 0L)
	           );
}

static void
toggle_timeout( DBusTimeout *timeout, void *csp )
{
    DBusConnectionState   *cs = csp;
    DBusConnectionTimeout **top = tfind( (const void*) dbus_timeout_get_data(timeout), 
				         &(cs->timeouts),
				         ptr_key_comparator
	                               ),
	                   *to=0L;
    if( (top != 0L) && ((to=*top) != 0L) && (to->to == timeout) )
    {
	if( cs->dh != 0L ) (*(cs->dh))("toggle_timeout: %d", dbus_timeout_get_interval(to->to));
	if(  dbus_timeout_get_enabled(timeout) )
	    gettimeofday(&(to->tv),0L);
	else
	{
	    to->tv.tv_sec = 0 ;
	    to->tv.tv_usec = 0 ;
	}
    }else
	if( cs->eh != 0L ) (*(cs->eh))("toggle_timeout: can't happen?!?: timeout %p %s %p %p", timeout, 
		    ((to==0L) ? "did not record data" : "not found"),
		    to, ((to != 0L) ? to->to : 0L)
	           );	
}

static void
process_timeout( const void *p, const VISIT which, const int level)
{
    DBusConnectionState   *cs;
    void * const *tpp = p;
    DBusConnectionTimeout *to;
    struct timeval tv;
    float now, then, interval;
    int l = level ? 1 : 0;
    l=l;

    gettimeofday(&tv,0L);
    
    if( (tpp != 0L) && (*tpp != 0L) && ((which == postorder) || (which == leaf)) ) 
    {
	to = (DBusConnectionTimeout*)*tpp;
	cs = to->cs;
	if ( !dbus_timeout_get_enabled(to->to) )
	    return;
	cs = dbus_timeout_get_data(to->to);
	then = ((float)to->tv.tv_sec) + (((float)to->tv.tv_usec)  / 1000000.0);
	if( then != 0.0 )
	{
	    interval = ((float)dbus_timeout_get_interval(to->to)) / 1000.0;
	    now = ((float)tv.tv_sec) + (( (float)tv.tv_usec) /   1000000.0);
	    if( (now - then) >= interval )
	    {
		if( cs->dh != 0L ) (*(cs->dh))("handle_timeout: %d - %f %f %f",  dbus_timeout_get_interval(to->to), then, now, interval);
		dbus_timeout_handle( to->to );
		to->tv=tv;
	    }
	}else
	{
	    to->tv = tv;
	}
    }
}

static void
process_timeouts ( DBusConnectionState *cs )
{
    twalk( cs->timeouts, process_timeout );
}

static void 
set_watch_fds( DBusWatch *watch, DBusConnectionState *cs )
{
    uint8_t flags = dbus_watch_get_flags(watch);
    int fd = dbus_watch_get_fd(watch);

    if ( cs->n <= fd )
	cs->n = fd + 1;

    if ( dbus_watch_get_enabled(watch) )
    {
	if ( flags & DBUS_WATCH_READABLE )
	{
	    FD_SET(fd , &(cs->r_fds));
	    if( cs->wh != 0L )
		(*(cs->wh))( fd, WATCH_ENABLE | WATCH_READ, cs->wh_arg ); 
	}else	    
	{
	    FD_CLR(fd , &(cs->r_fds));
	    if( cs->wh != 0L )
		(*(cs->wh))( fd, WATCH_READ, cs->wh_arg ); 
	}

	if ( flags & DBUS_WATCH_WRITABLE )
	{
	    FD_SET(fd , &(cs->w_fds));
	    if( cs->wh != 0L )
		(*(cs->wh))( fd, WATCH_ENABLE | WATCH_WRITE, cs->wh_arg ); 	 
	}else	    
	{
	    FD_CLR(fd , &(cs->w_fds));
	    if( cs->wh != 0L )
		(*(cs->wh))( fd, WATCH_WRITE, cs->wh_arg ); 	 
	}
	if ( flags & DBUS_WATCH_ERROR )
	{
	    FD_SET(fd , &(cs->e_fds));
	    if( cs->wh != 0L )
		(*(cs->wh))( fd, WATCH_ENABLE | WATCH_ERROR, cs->wh_arg ); 	
	}else	    
	{
	    FD_CLR(fd , &(cs->e_fds));
	    if( cs->wh != 0L )
		(*(cs->wh))( fd, WATCH_ERROR, cs->wh_arg ); 	
	}	
    }else
    {
	if( FD_ISSET( fd, &(cs->r_fds)) )
	    if( cs->wh != 0L )
		(*(cs->wh))( fd, WATCH_READ, cs->wh_arg );
	FD_CLR(fd , &(cs->r_fds));

	if( FD_ISSET( fd, &(cs->w_fds)) )
	    if( cs->wh != 0L )
		(*(cs->wh))( fd, WATCH_WRITE, cs->wh_arg );	
	FD_CLR(fd , &(cs->w_fds));
	
	if( FD_ISSET( fd, &(cs->e_fds)) )
	    if( cs->wh != 0L )
		(*(cs->wh))( fd, WATCH_ERROR, cs->wh_arg );
	FD_CLR(fd , &(cs->e_fds));
    }	
}

static dbus_bool_t
add_watch ( DBusWatch *watch, void *csp )
{    
    DBusConnectionState *cs = csp;

    dbus_watch_set_data(watch, cs, no_free );
    if( cs->dh != 0L ) (*(cs->dh))("add_watch: %d", dbus_watch_get_fd(watch));
    if( tsearch((const void*)watch,&(cs->watches),ptr_key_comparator) == 0L )
    {
	if( cs->eh != 0L ) (*(cs->eh))("add_watch: out of memory");
	return FALSE;
    }    
    set_watch_fds(watch,cs);
    return TRUE;
}

static void
remove_watch ( DBusWatch *watch, void *csp )
{
    DBusConnectionState *cs = csp;
    int  fd = dbus_watch_get_fd(watch);
    if( tdelete((const void*)watch, &(cs->watches), ptr_key_comparator) == 0L )
	if( cs->eh != 0L ) (*(cs->eh))("remove_watch: can't happen?!?: watch not found");
    if( cs->dh != 0L ) (*(cs->dh))("remove_watch: %d", dbus_watch_get_fd(watch));
    FD_CLR(fd , &(cs->r_fds));
    FD_CLR(fd , &(cs->w_fds));
    FD_CLR(fd , &(cs->e_fds));
    if( cs->wh != 0L )
	(*(cs->wh))(dbus_watch_get_fd(watch), WATCH_READ | WATCH_WRITE | WATCH_ERROR, cs->wh_arg );
}

static void
toggle_watch ( DBusWatch *watch, void *csp )
{
    DBusConnectionState *cs = csp;    
    if( cs->dh != 0L ) (*(cs->dh))("toggle_watch: %d", dbus_watch_get_fd(watch));
    set_watch_fds(watch,cs);
}

static void
process_watch( const void *p, const VISIT which, const int level)
{
    void * const *wpp=p;
    DBusWatch *w;
    int fd;
    uint8_t flags;
    DBusConnectionState *cs;
    int l = level ? 1 : 0;
    l=l;

    if((wpp != 0L) && (*wpp != 0L) && ( (which == postorder) || (which == leaf) ) )
    {
	w = (DBusWatch*)*wpp;
	cs = dbus_watch_get_data( w );
	if( cs == 0 )
	    return;
	if( ! dbus_watch_get_enabled(w) )
	    return;
	fd = dbus_watch_get_fd( w );
	flags = dbus_watch_get_flags( w );
	if( cs->dh != 0L ) (*(cs->dh))("handle_watch: %d", dbus_watch_get_fd( w ));
	if ( (flags & DBUS_WATCH_READABLE) && (FD_ISSET(fd, &(cs->s_r_fds))) )
	    dbus_watch_handle(w, DBUS_WATCH_READABLE);
	if ( (flags & DBUS_WATCH_WRITABLE) && (FD_ISSET(fd, &(cs->s_w_fds))) )
	    dbus_watch_handle(w, DBUS_WATCH_READABLE);
	if ( (flags & DBUS_WATCH_ERROR) && (FD_ISSET(fd, &(cs->s_e_fds))) )
	    dbus_watch_handle(w, DBUS_WATCH_ERROR);
    }
} 

static void
process_watches ( DBusConnectionState *cs )
{
    twalk( cs->watches, process_watch );
}

void dbus_svc_handle_watch(  DBusConnectionState *cs, int fd, dbus_svc_WatchFlags action )
{
    switch( action & 7 )
    {
    case WATCH_READ:
	FD_SET(fd, &(cs->s_r_fds));
	break;

    case WATCH_WRITE:
	FD_SET(fd, &(cs->s_w_fds));
	break;
	
    case WATCH_ERROR:
	FD_SET(fd, &(cs->s_e_fds));
	break;
    }
}

static void
dispatch_status
(   DBusConnection *connection, 
    DBusDispatchStatus new_status,
    void *csp 
)
{
    connection=connection;
    DBusConnectionState *cs = csp;
    cs->dispatchStatus = new_status;
}

void
dbus_svc_main_loop( DBusConnectionState *cs, void (*idle_handler)(DBusConnectionState *) )
{
    struct timeval timeout={0,200000};
    int n_fds;

    while( cs->status != SHUTDOWN )
    {
	cs->s_r_fds = cs->r_fds;
	cs->s_w_fds = cs->w_fds;
	cs->s_e_fds = cs->e_fds;
	
	timeout.tv_sec = 0;
	timeout.tv_usec= 200000;

	if ( (n_fds = select(cs->n, &(cs->s_r_fds), &(cs->s_w_fds), &(cs->s_e_fds), &timeout)) < 0 )
	{	    
	    if (errno != EINTR) 
	    {
		if( cs->eh != 0L ) (*(cs->eh))( "select failed: %d : %s", errno, strerror(errno));
	        return;
	    }
	}

	if( n_fds > 0 )
	    process_watches(cs);

	process_timeouts(cs);

	if ( cs->dispatchStatus != DBUS_DISPATCH_COMPLETE )
	    dbus_connection_dispatch( cs->connection );

	if( idle_handler != 0L )
	    (*idle_handler)(cs);
    }
}

void dbus_svc_dispatch(DBusConnectionState *cs)
{
    process_watches(cs);
    
    FD_ZERO(&(cs->s_r_fds));
    FD_ZERO(&(cs->s_w_fds));
    FD_ZERO(&(cs->s_e_fds));

    process_timeouts(cs);

    while ( cs->dispatchStatus != DBUS_DISPATCH_COMPLETE )
	dbus_connection_dispatch( cs->connection );
}

void 
dbus_svc_quit( DBusConnectionState *cs )
{
    cs->status = SHUTDOWN;
}

static isc_result_t
connection_setup 
(   DBusConnection *connection,
    DBUS_SVC *dbus,
    dbus_svc_WatchHandler wh, 
    dbus_svc_ErrorHandler eh, 
    dbus_svc_ErrorHandler dh,
    void *wh_arg
)
{
    *dbus = dbcs_new( connection );
    
    if ( *dbus == 0L )
    {
	if(eh)(*(eh))("connection_setup: out of memory");
	goto fail;
    }
    (*dbus)->wh = wh;
    (*dbus)->wh_arg = wh_arg;
    (*dbus)->eh = eh;
    (*dbus)->dh = dh;

    if (!dbus_connection_set_watch_functions 
	 (    (*dbus)->connection,
	      add_watch,
	      remove_watch,
	      toggle_watch,
	      *dbus,
	      no_free
	  )
       )
    {
	if( (*dbus)->eh != 0L ) (*((*dbus)->eh))("connection_setup: dbus_connection_set_watch_functions failed");
	goto fail; 
    }
      
    if (!dbus_connection_set_timeout_functions 
	 (    connection,
	      add_timeout,
	      remove_timeout,
	      toggle_timeout,
	      *dbus, 
	      no_free
	 )
       )
    {
	if( (*dbus)->eh != 0L ) (*((*dbus)->eh))("connection_setup: dbus_connection_set_timeout_functions failed");
	goto fail;
    }

    dbus_connection_set_dispatch_status_function 
    (   connection, 
	dispatch_status, 
	*dbus, 
	no_free
    ); 

    if (dbus_connection_get_dispatch_status (connection) != DBUS_DISPATCH_COMPLETE)
	dbus_connection_ref(connection);    
    
    return ISC_R_SUCCESS;
  
 fail:
    if( *dbus != 0L )
	free(*dbus);
  
    dbus_connection_set_dispatch_status_function (connection, NULL, NULL, NULL);
    dbus_connection_set_watch_functions (connection, NULL, NULL, NULL, NULL, NULL);
    dbus_connection_set_timeout_functions (connection, NULL, NULL, NULL, NULL, NULL);
  
    return ISC_R_FAILURE;
}

isc_result_t
dbus_svc_init
(
    dbus_svc_DBUS_TYPE    bus,
    char                  *name, 
    DBUS_SVC		  *dbus,
    dbus_svc_WatchHandler wh ,
    dbus_svc_ErrorHandler eh ,
    dbus_svc_ErrorHandler dh ,
    void *wh_arg
)
{
    DBusConnection       *connection;
    DBusError            error;
    char *session_bus_address=0L;

    memset(&error,'\0',sizeof(DBusError));

    dbus_error_init(&error);

    switch( bus )
    {
	/* DBUS_PRIVATE_* bus types are the only type which allow reconnection if the dbus-daemon is restarted
         */
    case DBUS_PRIVATE_SYSTEM:
       
	if ( (connection = dbus_connection_open_private("unix:path=/var/run/dbus/system_bus_socket", &error)) == 0L )
	{
	    if(eh)(*eh)("dbus_svc_init failed: %s %s",error.name, error.message);
	    return ISC_R_FAILURE;
	}

	if ( ! dbus_bus_register(connection,&error) )
	{
	    if(eh)(*eh)("dbus_bus_register failed: %s %s", error.name, error.message);
	    dbus_connection_close(connection);
	    free(connection);
	    return ISC_R_FAILURE;
	}
	break;

    case DBUS_PRIVATE_SESSION:
	
	session_bus_address = getenv("DBUS_SESSION_BUS_ADDRESS");
	if ( session_bus_address == 0L )
	{
	    if(eh)(*eh)("dbus_svc_init failed: DBUS_SESSION_BUS_ADDRESS environment variable not set");
	    return ISC_R_FAILURE;
	}

	if ( (connection = dbus_connection_open_private(session_bus_address, &error)) == 0L )
	{
	    if(eh)(*eh)("dbus_svc_init failed: %s %s",error.name, error.message);
	    return ISC_R_FAILURE;
	}

	if ( ! dbus_bus_register(connection,&error) )
	{
	    if(eh)(*eh)("dbus_bus_register failed: %s %s", error.name, error.message);
	    dbus_connection_close(connection);
	    free(connection);
	    return ISC_R_FAILURE;
	}
	break;

    case DBUS_SYSTEM:
    case DBUS_SESSION:

	if ( (connection = dbus_bus_get (bus, &error)) == 0L )
	{
	    if(eh)(*eh)("dbus_svc_init failed: %s %s",error.name, error.message);
	    return ISC_R_FAILURE;
	}
	break;

    default:
	if(eh)(*eh)("dbus_svc_init failed: unknown bus type %d", bus);
	return ISC_R_FAILURE;
    }
    
    dbus_connection_set_exit_on_disconnect(connection, FALSE);

    if ( (connection_setup(connection, dbus, wh, eh, dh, wh_arg)) != ISC_R_SUCCESS)
    {
	if(eh)(*eh)("dbus_svc_init failed: connection_setup failed");
	return ISC_R_FAILURE;
    }

    if( name == 0L )
	return ISC_R_SUCCESS;
    
    (*dbus)->unique_name = dbus_bus_get_unique_name(connection);

    switch
	(   dbus_bus_request_name 
	    (   connection, name, 
#ifdef DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT
		DBUS_NAME_FLAG_PROHIBIT_REPLACEMENT ,
#else
		0 ,
#endif
		&error
	    ) 
	)
    {   
    case DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER:	
	break;
    case DBUS_REQUEST_NAME_REPLY_EXISTS:
    case DBUS_REQUEST_NAME_REPLY_IN_QUEUE:
    case DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER:
	if(eh)(*eh)("dbus_svc_init: dbus_bus_request_name failed:  Name already registered");
	goto give_up;
    default:
	if(eh)(*eh)("dbus_svc_init: dbus_bus_request_name failed: %s %s", error.name, error.message);
	goto give_up;
    }
    return ISC_R_SUCCESS;

 give_up:
    dbus_connection_close( connection );
    dbus_connection_unref( connection );
    if( *dbus )
    {
	dbus_connection_set_dispatch_status_function (connection, NULL, NULL, NULL);
	dbus_connection_set_watch_functions (connection, NULL, NULL, NULL, NULL, NULL);
	dbus_connection_set_timeout_functions (connection, NULL, NULL, NULL, NULL, NULL);
	free(*dbus);    
    }
    return ISC_R_FAILURE;
}

const char *dbus_svc_unique_name(DBusConnectionState *cs)
{
    return cs->unique_name;
}

void
dbus_svc_shutdown ( DBusConnectionState *cs )                          
{
    if (!dbus_connection_set_watch_functions 
	 (   cs->connection,
	     NULL, NULL, NULL, NULL, NULL
	 )
       ) if( cs->eh != 0L ) (*(cs->eh))("connection_shutdown: dbus_connection_set_watch_functions: No Memory."
                     "Setting watch functions to NULL failed."
	            );
  
    if (!dbus_connection_set_timeout_functions 
	 (   cs->connection,
	     NULL, NULL, NULL, NULL, NULL
	 )
       ) if( cs->eh != 0L ) (*(cs->eh))("connection_shutdown: dbus_connection_set_timeout_functions: No Memory."
		     "Setting timeout functions to NULL failed."
	            );

    dbus_connection_set_dispatch_status_function (cs->connection, NULL, NULL, NULL);
    
    tdestroy( cs->timeouts, free);
    cs->timeouts=0L;
    tdestroy( cs->watches, no_free);
    cs->watches=0L;
    
    dbus_connection_close( cs->connection );
    dbus_connection_unref( cs->connection );

    free( cs );
}
