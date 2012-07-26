/* dbus_mgr.c
 *
 *  named module to provide dynamic forwarding zones in 
 *  response to D-BUS dhcp events or commands.
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
 */
#include <config.h>
#include <isc/types.h>
#include <isc/net.h>
#include <isc/mem.h>
#include <isc/magic.h>
#include <isc/list.h>
#include <isc/task.h>
#include <isc/event.h>
#include <isc/socket.h>
#include <isc/timer.h>
#include <isc/netaddr.h>
#include <isc/sockaddr.h>
#include <isc/buffer.h>
#include <isc/log.h>

#include <dns/name.h>
#include <dns/acl.h>
#include <dns/fixedname.h>
#include <dns/view.h>
#include <dns/forward.h>

#include <named/types.h>
#include <named/config.h>
#include <named/server.h>
#include <named/globals.h>
#include <named/log.h>

#include <named/dbus_service.h>
#include <named/dbus_mgr.h>

#include <string.h>
#include <search.h>

typedef void (*__free_fn_t) (void *__nodep);
extern void tdestroy (void *__root, __free_fn_t __freefct);
extern void free(void*);

#ifdef ISC_USE_INTERNAL_MALLOC
#  if  ISC_USE_INTERNAL_MALLOC
#      error dbus_mgr cannot be used if ISC_USE_INTERNAL_MALLOC==1
#  endif
#endif

#define DBUSMGR_DESTINATION  "com.redhat.named"
#define DBUSMGR_OBJECT_PATH "/com/redhat/named"
#define DBUSMGR_INTERFACE    "com.redhat.named"

#define DBUSMGR_MAGIC	ISC_MAGIC('D', 'B', 'U', 'S')

struct ns_dbus_mgr
{
    unsigned int        magic;
    isc_mem_t         *	mctx;		/* Memory context. */
    isc_taskmgr_t     * taskmgr;	/* Task manager.   */
    isc_socketmgr_t   *	socketmgr;	/* Socket manager. */   
    isc_timermgr_t    *	timermgr;	/* Timer manager.  */   
    isc_task_t        * task;           /* task            */
    isc_timer_t       * timer;          /* dbus_init retry */
    void              * sockets;        /* dbus fd tree    */ 
    void              * dhc_if;         /* dhcp interface tree */
    void              * ifwdt;          /* initial forwarder tree */
    char              * dhcdbd_name;    /* dhcdbd destination  */
    DBUS_SVC            dbus;	        /* dbus handle */
};

typedef
struct dbus_mgr_sock_s
{
    int fd;
    struct ns_dbus_mgr *mgr;
    isc_socket_t *sock;
    isc_socketevent_t *ser;
    isc_socketevent_t *sew;
    isc_socketevent_t *sel;
} DBusMgrSocket;

typedef
enum dhc_state_e
{
   DHC_NBI,         	/* no broadcast interfaces found */
   DHC_PREINIT, 	/* configuration started   */
   DHC_BOUND, 		/* lease obtained          */
   DHC_RENEW, 		/* lease renewed           */
   DHC_REBOOT,	        /* have valid lease, but now obtained a different one */
   DHC_REBIND, 		/* new, different lease */
   DHC_STOP,  		/* remove old lease */
   DHC_MEDIUM, 		/* media selection begun */
   DHC_TIMEOUT, 	/* timed out contacting DHCP server */
   DHC_FAIL, 		/* all attempts to contact server timed out, sleeping */
   DHC_EXPIRE, 		/* lease has expired, renewing */
   DHC_RELEASE, 	/* releasing lease */
   DHC_START,           /* sent when dhclient started OK */
   DHC_ABEND,  		/* dhclient exited abnormally    */
   DHC_END, 		/* dhclient exited normally      */
   DHC_END_OPTIONS,     /* last option in subscription sent */
   DHC_INVALID=255      
} DHC_State;

typedef ISC_LIST(dns_name_t) DNSNameList;

typedef ISC_LIST(isc_sockaddr_t) SockAddrList;

typedef struct dbm_fwdr_s
{
    dns_fwdpolicy_t fwdpolicy;
    dns_name_t       dn;
    SockAddrList     sa;
    ISC_LINK( struct dbm_fwdr_s ) link;
} DBusMgrInitialFwdr;

typedef
struct dhc_if_s
{
    char           *if_name;
    DHC_State      dhc_state;
    DHC_State      previous_state;
    struct in_addr ip;
    struct in_addr subnet_mask;
    DNSNameList    dn;
    SockAddrList   dns;
} DHC_IF;

static void 
dbus_mgr_watch_handler( int fd, dbus_svc_WatchFlags flags, void *mgrp );

static
dbus_svc_HandlerResult 
dbus_mgr_message_handler
(  
    DBusMsgHandlerArgs
);

static
void dbus_mgr_close_socket( const void *p, const VISIT which, const int level);

static
void dbus_mgr_destroy_socket( void *p );

static
void dbus_mgr_free_dhc( void *p );

static void
dbus_mgr_watches_selected(isc_task_t *t, isc_event_t *ev);

static isc_result_t
dbus_mgr_init_dbus(ns_dbus_mgr_t *);

static isc_result_t
dbus_mgr_record_initial_fwdtable(ns_dbus_mgr_t *);

static
dns_fwdtable_t *dbus_mgr_get_fwdtable(void);

static void
dbus_mgr_free_initial_fwdtable(ns_dbus_mgr_t *);

static 
uint8_t dbus_mgr_subscribe_to_dhcdbd( ns_dbus_mgr_t * );

static 
void dbus_mgr_dbus_shutdown_handler ( ns_dbus_mgr_t * );

static
int dbus_mgr_log_err( const char *fmt, ...)
{
    va_list  va;
    va_start(va, fmt);
    isc_log_vwrite(ns_g_lctx,		  
		   NS_LOGCATEGORY_DBUS,
		   NS_LOGMODULE_DBUS,	
		   ISC_LOG_NOTICE,
		   fmt, va
	          );
    va_end(va);
    return 0;
}

static
int dbus_mgr_log_dbg( const char *fmt, ...)
{
    va_list  va;
    va_start(va, fmt);
    isc_log_vwrite(ns_g_lctx,		  
		   NS_LOGCATEGORY_DBUS,
		   NS_LOGMODULE_DBUS,	
		   ISC_LOG_DEBUG(80),
		   fmt, va
	          );
    va_end(va);
    return 0;
}

static
int dbus_mgr_log_info( const char *fmt, ...)
{
    va_list  va;
    va_start(va, fmt);
    isc_log_vwrite(ns_g_lctx,		  
		   NS_LOGCATEGORY_DBUS,
		   NS_LOGMODULE_DBUS,	
		   ISC_LOG_DEBUG(1),
		   fmt, va
	          );
    va_end(va);
    return 0;
}

isc_result_t 
dbus_mgr_create
(   isc_mem_t *mctx, 
    isc_taskmgr_t *taskmgr,
    isc_socketmgr_t *socketmgr,
    isc_timermgr_t *timermgr,
    ns_dbus_mgr_t **dbus_mgr
)
{
    isc_result_t result;
    ns_dbus_mgr_t *mgr;
    
    *dbus_mgr = 0L; 

    mgr = isc_mem_get(mctx, sizeof(*mgr));
    if (mgr == NULL)
	return (ISC_R_NOMEMORY);

    mgr->magic = DBUSMGR_MAGIC;
    mgr->mctx = mctx;
    mgr->taskmgr = taskmgr;
    mgr->socketmgr = socketmgr;
    mgr->timermgr = timermgr;
    mgr->task = 0L;
    mgr->sockets = 0L;
    mgr->timer = 0L;
    mgr->dhc_if = 0L;
    mgr->ifwdt = 0L;
    mgr->dhcdbd_name = 0L;

    if( (result = isc_task_create( taskmgr, 100, &(mgr->task)))
        != ISC_R_SUCCESS
      )	goto cleanup_mgr;
    
    isc_task_setname( mgr->task, "dbusmgr", mgr );

    mgr->dbus = 0L;

    if( (result = dbus_mgr_record_initial_fwdtable( mgr ))
	!= ISC_R_SUCCESS
      )	goto cleanup_mgr;

    if( (result = dbus_mgr_init_dbus( mgr ))
	!= ISC_R_SUCCESS 
      )	goto cleanup_mgr;     

    *dbus_mgr = mgr; 
   
    return ISC_R_SUCCESS;

 cleanup_mgr:
    if ( dbus_mgr_get_fwdtable() != NULL)
	dbus_mgr_free_initial_fwdtable (mgr);
    if( mgr->task != 0L )
	isc_task_detach(&(mgr->task));
    isc_mem_put(mctx, mgr, sizeof(*mgr));
    return (result);
}

static isc_result_t
dbus_mgr_init_dbus(ns_dbus_mgr_t * mgr)
{
    char destination[]=DBUSMGR_DESTINATION;
    isc_result_t result;

    if( mgr->sockets != 0L )
    {
	isc_task_purgerange(mgr->task, 0L, ISC_SOCKEVENT_READ_READY, ISC_SOCKEVENT_SELECTED, 0L);
	twalk(mgr->sockets, dbus_mgr_close_socket);
	tdestroy(mgr->sockets, dbus_mgr_destroy_socket);
	mgr->sockets = 0L;
    }

    if( mgr->dbus != 0L )
    {
	dbus_svc_shutdown(mgr->dbus);
	mgr->dbus = 0L;
    }

    result = dbus_svc_init(DBUS_PRIVATE_SYSTEM, destination, &mgr->dbus,
					dbus_mgr_watch_handler, 0L, 0L, mgr);

    if(result != ISC_R_SUCCESS)
	goto cleanup;

    if( mgr->dbus == 0L )
    {
	if( mgr->timer == 0L)
	{
	    isc_task_purgerange(mgr->task, 0L, ISC_SOCKEVENT_READ_READY, ISC_SOCKEVENT_SELECTED, 0L);
	    if( mgr->sockets != 0L )
	    {
		twalk(mgr->sockets, dbus_mgr_close_socket);
		tdestroy(mgr->sockets, dbus_mgr_destroy_socket);
		mgr->sockets = 0L;
	    }
	    dbus_mgr_dbus_shutdown_handler (  mgr );
	    return ISC_R_SUCCESS;
	}
	goto cleanup;
    }

    if( !dbus_svc_add_filter
	( mgr->dbus, dbus_mgr_message_handler, mgr, 4,
	  "type=signal,path=/org/freedesktop/DBus,member=NameOwnerChanged",
	  "type=signal,path=/org/freedesktop/DBus/Local,member=Disconnected",
	  "type=signal,interface=com.redhat.dhcp.subscribe.binary",
	  "type=method_call,destination=com.redhat.named,path=/com/redhat/named" 	 
	)
      )
    {
	dbus_mgr_log_err( "dbus_svc_add_filter failed" );
	goto cleanup;
    }
    
    if( mgr->timer != 0L )
    {
	isc_timer_reset(mgr->timer,
			isc_timertype_inactive,
			NULL, NULL, ISC_TRUE
	               );
    }

    if( !dbus_mgr_subscribe_to_dhcdbd( mgr ) )
	dbus_mgr_log_err("D-BUS dhcdbd subscription disabled.");

    dbus_mgr_log_err("D-BUS service enabled.");
    return ISC_R_SUCCESS;

 cleanup:
    isc_task_purgerange(mgr->task, 0L, ISC_SOCKEVENT_READ_READY, ISC_SOCKEVENT_SELECTED, 0L);
    twalk(mgr->sockets, dbus_mgr_close_socket);
    tdestroy(mgr->sockets, dbus_mgr_destroy_socket);
    mgr->sockets = 0L;
    if( mgr->dbus )
    {
	dbus_svc_shutdown(mgr->dbus);
	mgr->dbus = 0L;
    }
    return ISC_R_FAILURE;
}

static 
uint8_t dbus_mgr_subscribe_to_dhcdbd( ns_dbus_mgr_t *mgr )
{
    DBUS_SVC dbus = mgr->dbus;
    char subs[1024], path[1024], 
	dhcdbd_destination[]="com.redhat.dhcp", *ddp[1]={ &(dhcdbd_destination[0]) },
	*dhcdbd_name=0L;
    const char *options[] = { "reason", "ip-address", "subnet-mask", 
			      "domain-name", "domain-name-servers" 
                            };
    dbus_svc_MessageHandle msg;
    int i, n_opts = 5;

    if( mgr->dhcdbd_name == 0L )
    {
	msg = dbus_svc_call
	      ( dbus,
		"org.freedesktop.DBus",
		"/org/freedesktop/DBus",
		"GetNameOwner",		
		"org.freedesktop.DBus",
		TYPE_STRING, &ddp,
		TYPE_INVALID
	       );
	if( msg == 0L )	
	    return 0;    

	if( !dbus_svc_get_args(dbus, msg,
			       TYPE_STRING, &(dhcdbd_name),
			       TYPE_INVALID
		              ) 
	  )	return 0;

	mgr->dhcdbd_name = isc_mem_get(mgr->mctx, strlen(dhcdbd_name) + 1);
	if( mgr->dhcdbd_name == 0L )
	    return 0;

	strcpy(mgr->dhcdbd_name, dhcdbd_name);

    }

    sprintf(path,"/com/redhat/dhcp/subscribe");    
    sprintf(subs,"com.redhat.dhcp.binary");

    for(i = 0; i < n_opts; i++)
    {
	msg = dbus_svc_call
	      ( dbus,
		"com.redhat.dhcp",
		path,
		"binary",		
		subs,		
		TYPE_STRING, &(options[i]),
		TYPE_INVALID
	      );
	if(msg == 0L)
	    return 0;
	if ( dbus_svc_message_type( msg ) == ERROR )
	    return 0;
    }
    dbus_mgr_log_err("D-BUS dhcdbd subscription enabled.");
    return 1;
}

void 
dbus_mgr_shutdown
(   ns_dbus_mgr_t *mgr
)
{
    if( mgr->timer != 0L )
	isc_timer_detach(&(mgr->timer));
    if( mgr->dbus != 0L )
    {
	isc_task_purgerange(mgr->task, 0L, ISC_SOCKEVENT_READ_READY, ISC_SOCKEVENT_SELECTED, 0L);
	if( mgr->sockets != 0L )
	{
	    twalk(mgr->sockets, dbus_mgr_close_socket);
	    tdestroy(mgr->sockets, dbus_mgr_destroy_socket);
	    mgr->sockets = 0L;
	}
	dbus_svc_shutdown(mgr->dbus);
    }
    if( mgr->dhc_if != 0L )
	tdestroy(mgr->dhc_if, dbus_mgr_free_dhc);
    if( mgr->dhcdbd_name != 0L )
	isc_mem_put(mgr->mctx, mgr->dhcdbd_name, strlen(mgr->dhcdbd_name) + 1);
    isc_task_detach(&(mgr->task));
    dbus_mgr_free_initial_fwdtable(mgr);
    isc_mem_put(mgr->mctx, mgr, sizeof(ns_dbus_mgr_t));
}

static 
void dbus_mgr_restart_dbus(isc_task_t *t, isc_event_t *ev)
{   
    ns_dbus_mgr_t *mgr = (ns_dbus_mgr_t*)(ev->ev_arg) ;
    t=t;    
    isc_event_free(&ev);
    dbus_mgr_log_dbg("attempting to connect to D-BUS");
    dbus_mgr_init_dbus( mgr );
}

static
void dbus_mgr_handle_dbus_shutdown_event(isc_task_t *t, isc_event_t *ev)
{
    ns_dbus_mgr_t *mgr = ev->ev_arg;
    isc_time_t     tick={10,0};
    isc_interval_t tock={10,0};
    DBUS_SVC dbus = mgr->dbus;
    t = t;

    mgr->dbus = 0L;

    isc_event_free(&ev);

    if ( dbus != 0L )
    {
	isc_task_purgerange(mgr->task, 0L, ISC_SOCKEVENT_READ_READY, ISC_SOCKEVENT_SELECTED, 0L);
	if( mgr->sockets != 0L )
	{
	    twalk(mgr->sockets, dbus_mgr_close_socket);
	    tdestroy(mgr->sockets, dbus_mgr_destroy_socket);
	    mgr->sockets = 0L;
	}
	dbus_svc_shutdown(dbus);    
    }

    dbus_mgr_log_err( "D-BUS service disabled." );

    if( mgr->timer != 0L )
    {
	isc_timer_reset(mgr->timer,
			isc_timertype_ticker,
			&tick, &tock, ISC_TRUE
	               );
    }else
    if( isc_timer_create
	(   mgr->timermgr,
	    isc_timertype_ticker,
	    &tick, &tock,
	    mgr->task,
	    dbus_mgr_restart_dbus,
	    mgr,
	    &(mgr->timer)
	) != ISC_R_SUCCESS
      )
    {
	dbus_mgr_log_err( "D-BUS service cannot be restored." );
    }
}

static 
void dbus_mgr_dbus_shutdown_handler ( ns_dbus_mgr_t *mgr )
{ 
    isc_event_t *dbus_shutdown_event = 
	isc_event_allocate
	(   mgr->mctx, 
	    mgr->task, 
	    1,
	    dbus_mgr_handle_dbus_shutdown_event,
	    mgr,
	    sizeof(isc_event_t)
	 );
    if( dbus_shutdown_event != 0L )
    {
	isc_task_purgerange(mgr->task, 0L, ISC_SOCKEVENT_READ_READY, ISC_SOCKEVENT_SELECTED, 0L);
	isc_task_send( mgr->task, &dbus_shutdown_event );
    }else
	dbus_mgr_log_err("unable to allocate dbus shutdown event");
}

static
dns_view_t *dbus_mgr_get_localhost_view(void)
{
    dns_view_t       *view;
    isc_netaddr_t    localhost = { AF_INET, { { htonl( ( 127 << 24 ) | 1 ) } }, 0 };
    int              match;

    for (view = ISC_LIST_HEAD(ns_g_server->viewlist);
	 view != NULL;
	 view = ISC_LIST_NEXT(view, link)
        )
    {	
	/* return first view matching "localhost" source and dest */

	if(( (view->matchclients != 0L )   /* 0L: accept "any" */
	   &&(( dns_acl_match( &localhost, 
			     NULL, /* unsigned queries */
			     view->matchclients,
		  	     &(ns_g_server->aclenv),
			     &match, 
			     NULL  /* no match list */
		            ) != ISC_R_SUCCESS
	      ) || (match <= 0)
	     )
	    ) 
	 ||( (view->matchdestinations != 0L )   /* 0L: accept "any" */
	   &&(( dns_acl_match( &localhost, 
			     NULL, /* unsigned queries */
			     view->matchdestinations,
		  	     &(ns_g_server->aclenv),
			     &match, 
			     NULL  /* no match list */
		            ) != ISC_R_SUCCESS
	      ) || (match <= 0)
	     )
	    )
	  ) continue;	
	
	break;
    }
    return view;
}

static
dns_fwdtable_t *dbus_mgr_get_fwdtable(void)
{
    dns_view_t *view = dbus_mgr_get_localhost_view();
    if( view != 0L )
	return view->fwdtable;
    return 0L;
}

static
dns_fwdtable_t *dbus_mgr_get_view_and_fwdtable( dns_view_t **viewp )
{
    *viewp = dbus_mgr_get_localhost_view();
    if( *viewp != 0L )
	return (*viewp)->fwdtable;
    return 0L;
}

static int dbus_mgr_ifwdr_comparator( const void *p1, const void *p2 )
{
    char  n1buf[ DNS_NAME_FORMATSIZE ]="", *n1p=&(n1buf[0]),
	  n2buf[ DNS_NAME_FORMATSIZE ]="", *n2p=&(n2buf[0]);
    dns_name_t *dn1;
    dns_name_t *dn2;
    DE_CONST(&(((const DBusMgrInitialFwdr*)p1)->dn), dn1);
    DE_CONST(&(((const DBusMgrInitialFwdr*)p2)->dn), dn2);
    dns_name_format(dn1, n1p, DNS_NAME_FORMATSIZE );    
    dns_name_format(dn2, n2p, DNS_NAME_FORMATSIZE );    
    return strcmp(n1buf, n2buf);
}

static int dbus_mgr_dhc_if_comparator( const void *p1, const void *p2 );

static void dbus_mgr_record_initial_forwarder( dns_name_t *name, dns_forwarders_t *fwdr, void *mp )
{
    ns_dbus_mgr_t *mgr = mp;
    isc_sockaddr_t *sa, *nsa;
    DBusMgrInitialFwdr *ifwdr;
 
    if( ISC_LIST_HEAD(fwdr->addrs) == 0L) 
	return;

    if( (ifwdr = isc_mem_get(mgr->mctx, sizeof(DBusMgrInitialFwdr))) == 0L)
	return;

    ifwdr->fwdpolicy = fwdr->fwdpolicy;

    dns_name_init(&(ifwdr->dn), NULL);
    if( dns_name_dupwithoffsets(name, mgr->mctx, &(ifwdr->dn)) != ISC_R_SUCCESS )
	goto namedup_err;

    ISC_LIST_INIT(ifwdr->sa);
    
    for( sa = ISC_LIST_HEAD(fwdr->addrs);
	 sa != 0L;
	 sa = ISC_LIST_NEXT(sa,link)
       )
    {
	nsa = isc_mem_get(mgr->mctx, sizeof(isc_sockaddr_t));
	if( nsa == 0L ) 
	    goto nsa_err;
	*nsa = *sa;
	ISC_LINK_INIT(nsa, link);
	ISC_LIST_APPEND(ifwdr->sa, nsa, link);
    }
    ISC_LINK_INIT(ifwdr, link);
    tsearch( ifwdr, &(mgr->ifwdt), dbus_mgr_ifwdr_comparator);

    return;

nsa_err:
    while ( (sa = ISC_LIST_HEAD (ifwdr->sa)) != NULL) {
	ISC_LIST_UNLINK (ifwdr->sa, sa, link);
	isc_mem_put (mgr->mctx, sa, sizeof (*sa));
    }

namedup_err:
    isc_mem_put (mgr->mctx, ifwdr, sizeof (*ifwdr));

    return;
}

static isc_result_t
dbus_mgr_record_initial_fwdtable( ns_dbus_mgr_t *mgr )
{
    dns_fwdtable_t *fwdtable = dbus_mgr_get_fwdtable();
    
    if( fwdtable == 0L )
	return ISC_R_SUCCESS; /* no initial fwdtable */
    dns_fwdtable_foreach( fwdtable, dbus_mgr_record_initial_forwarder, mgr);
    return ISC_R_SUCCESS;
}

static void
dbus_mgr_free_initial_forwarder( void *p )
{
   DBusMgrInitialFwdr *ifwdr = p;
   isc_sockaddr_t *sa;

   dns_name_free(&(ifwdr->dn), ns_g_mctx);
   for( sa = ISC_LIST_HEAD( ifwdr->sa );
	sa != 0L;
	sa = ISC_LIST_HEAD( ifwdr->sa )
      )
   {
       if( ISC_LINK_LINKED(sa, link) )
	   ISC_LIST_UNLINK(ifwdr->sa, sa, link);
       isc_mem_put(ns_g_mctx, sa, sizeof(isc_sockaddr_t));
   }
   isc_mem_put(ns_g_mctx, ifwdr, sizeof(DBusMgrInitialFwdr));
}

static void
dbus_mgr_free_initial_fwdtable( ns_dbus_mgr_t *mgr )
{
    tdestroy(mgr->ifwdt, dbus_mgr_free_initial_forwarder);
    mgr->ifwdt = 0L;
}

static void
dbus_mgr_log_forwarders( const char *pfx, dns_name_t *name, SockAddrList *saList)
{
    isc_sockaddr_t   *sa;
    char nameP[DNS_NAME_FORMATSIZE], addrP[128];
    int s=0;
    dns_name_format(name, nameP, DNS_NAME_FORMATSIZE );    
    for( sa = ISC_LIST_HEAD(*saList);
	 sa != 0L;
	 sa = ISC_LIST_NEXT(sa,link)
	)
    {
	isc_sockaddr_format(sa, addrP, 128);
	dbus_mgr_log_info("%s zone %s server %d: %s", pfx, nameP, s++, addrP);
    }		
}

static
isc_result_t dbus_mgr_set_forwarders
(   
    ns_dbus_mgr_t *mgr,
    DNSNameList *nameList,
    SockAddrList *saList,
    dns_fwdpolicy_t fwdpolicy
)
{
    isc_result_t   result = ISC_R_SUCCESS;    
    dns_fwdtable_t *fwdtable;
    dns_view_t     *view=0L;
    dns_name_t     *dnsName;
    isc_sockaddr_t   *sa, *nsa;
    dns_forwarders_t *fwdr=0L;

    fwdtable = dbus_mgr_get_view_and_fwdtable(&view);

    if( fwdtable == 0L )
    {
	if( ISC_LIST_HEAD(*saList) == 0L )
	    return ISC_R_SUCCESS;/* deletion not required */

	view = dbus_mgr_get_localhost_view();
	if( view == 0L )
	    return ISC_R_NOPERM; /* if configuration does not allow localhost clients,
				  * then we really shouldn't be creating a forwarding table.
				  */
	result = isc_task_beginexclusive(mgr->task);

	if( result == ISC_R_SUCCESS )
	{
	    result = dns_fwdtable_create( mgr->mctx, &(view->fwdtable) );

	    isc_task_endexclusive(mgr->task);

	    if( result != ISC_R_SUCCESS )
		return result;

	    if( view->fwdtable == 0L )
		return ISC_R_NOMEMORY;

	    if( isc_log_getdebuglevel(ns_g_lctx) >= 1 )
		dbus_mgr_log_info("Created forwarder table.");
	}
    }
	
    for( dnsName = ISC_LIST_HEAD(*nameList);
	 dnsName != NULL;
	 dnsName = ISC_LIST_NEXT(dnsName,link)
	)
    {   	
	fwdr = 0L;
	if( ( dns_fwdtable_find_exact( fwdtable, dnsName, &fwdr ) != ISC_R_SUCCESS )
	  ||( fwdr == 0L )
	  )
	{ 
	    if( ISC_LIST_HEAD( *saList )  == 0L )
		continue;
	   /* no forwarders for name - add forwarders */

	    result = isc_task_beginexclusive(mgr->task);

	    if( result == ISC_R_SUCCESS )
	    {
		result = dns_fwdtable_add( fwdtable, dnsName, 
					   (isc_sockaddrlist_t*)saList, 
					   fwdpolicy
					 ) ;

		if( view != 0L )
		    dns_view_flushcache( view );

		isc_task_endexclusive(mgr->task);	

		if( result != ISC_R_SUCCESS )
		    return result;

		if( isc_log_getdebuglevel(ns_g_lctx) >= 1 )
		    dbus_mgr_log_forwarders("Created forwarder",dnsName, saList);
	    }
	    continue;
	}

	if( ISC_LIST_HEAD( *saList ) == 0L )
	{ /* empty forwarders list - delete forwarder entry */

	    if( isc_log_getdebuglevel(ns_g_lctx) >= 1 )
		dbus_mgr_log_forwarders("Deleting forwarder", dnsName, (SockAddrList*)&(fwdr->addrs));

	    result = isc_task_beginexclusive(mgr->task);
	    if( result == ISC_R_SUCCESS )
	    {
		result = dns_fwdtable_delete( fwdtable, dnsName );

		if( view != 0L )
		    dns_view_flushcache( view );

		isc_task_endexclusive(mgr->task);

		if( result != ISC_R_SUCCESS )
		    return result;	
	    }
	    continue;
	}	

	result = isc_task_beginexclusive(mgr->task);

	if( result == ISC_R_SUCCESS )
	{	 	   
	    fwdr->fwdpolicy = fwdpolicy;

	    if( isc_log_getdebuglevel(ns_g_lctx) >= 1 )
		dbus_mgr_log_forwarders("Removing forwarder", dnsName, (SockAddrList*)&(fwdr->addrs));
	    
	    for( sa = ISC_LIST_HEAD(fwdr->addrs);
		 sa != 0L ;
		 sa = ISC_LIST_HEAD(fwdr->addrs)
	       )
	    {
		if( ISC_LINK_LINKED(sa, link) )
		    ISC_LIST_UNLINK(fwdr->addrs, sa, link);
		isc_mem_put(mgr->mctx, sa, sizeof(isc_sockaddr_t));
	    }

	    ISC_LIST_INIT( fwdr->addrs );
	    
	    for( sa = ISC_LIST_HEAD(*saList);
		 sa != 0L;
		 sa = ISC_LIST_NEXT(sa,link)
	       )
	    {
		nsa = isc_mem_get(mgr->mctx, sizeof(isc_sockaddr_t));
		if( nsa == 0L )
		{
		    result = ISC_R_NOMEMORY;
		    break;
		}
		*nsa = *sa;
		ISC_LINK_INIT( nsa, link );
		ISC_LIST_APPEND( fwdr->addrs, nsa, link );
	    }	    

	    if( view != 0L )
		dns_view_flushcache( view );

	    isc_task_endexclusive(mgr->task);

	    if( isc_log_getdebuglevel(ns_g_lctx) >= 1 )
		dbus_mgr_log_forwarders("Added forwarder", dnsName, (SockAddrList*)&(fwdr->addrs));

	}else
	    return result;

    }
    return (result);
}

static void
dbus_mgr_get_name_list
( 
    ns_dbus_mgr_t *mgr,
    char *domains, 
    DNSNameList *nameList,
    char *error_name,
    char *error_message
)
{
    char *name, *endName, *endp;
    dns_fixedname_t *fixedname;
    dns_name_t      *dnsName;
    isc_buffer_t     buffer;
    isc_result_t     result;
    uint32_t total_length;
    
    total_length = strlen(domains);
    endp = domains + total_length;

    ISC_LIST_INIT( *nameList );

    for( name =   domains + strspn(domains," \t\n"),
	 endName = name + strcspn(name," \t\n");
	 (name < endp) && (endName <= endp); 
	 name =  endName + 1 + strspn(endName+1," \t\n"),
	 endName = name + strcspn(name," \t\n")
       )
    {  /* name loop */
	*endName = '\0';

	isc_buffer_init( &buffer, name, endName - name );
	isc_buffer_add(&buffer, endName - name);		
	
	fixedname = isc_mem_get( mgr->mctx, sizeof( dns_fixedname_t ));

	dns_fixedname_init(fixedname);

	dnsName = dns_fixedname_name(fixedname);

	result= dns_name_fromtext
	        (  dnsName, &buffer, ( *(endp-1) != '.') ? dns_rootname : NULL, 0, NULL
		); 

	if( result != ISC_R_SUCCESS )
	{
	    sprintf(error_name, "com.redhat.named.InvalidArgument");
	    sprintf(error_message,"Invalid DNS name initial argument: %s", name);
	    
	    isc_mem_put( mgr->mctx, fixedname, sizeof( dns_fixedname_t ) );

	    for( dnsName = ISC_LIST_HEAD( *nameList );
		 (dnsName != 0L);
		 dnsName = ISC_LIST_HEAD( *nameList )
	       )
	    {
		if( ISC_LINK_LINKED(dnsName,link) )
		    ISC_LIST_DEQUEUE( *nameList, dnsName, link );
		isc_mem_put( mgr->mctx, dnsName, sizeof( dns_fixedname_t ) );
	    }
	    ISC_LIST_INIT(*nameList);
	    return;
	}
	ISC_LINK_INIT(dnsName, link);
	ISC_LIST_ENQUEUE( *nameList, dnsName, link );
    }
}

static isc_result_t
dbus_mgr_get_sa_list
(
    ns_dbus_mgr_t *mgr,
    dbus_svc_MessageIterator iter,
    SockAddrList *saList ,
    uint8_t *fwdpolicy,
    char *error_name,
    char *error_message
)
{
    DBUS_SVC dbus = mgr->dbus;
    isc_sockaddr_t *nsSA=0L, *nsSA_Q=0L;
    uint32_t argType = dbus_svc_message_next_arg_type( dbus, iter ), 
	     length;    
    isc_result_t result;
    in_port_t port;
    char *ip;
    uint8_t *iparray=0L;

    ISC_LIST_INIT(*saList);

    if( argType == TYPE_INVALID ) 
	return ISC_R_SUCCESS; /* address list "removal" */
   
    do
    {
	switch( argType )
	{
	case TYPE_UINT32:

	    nsSA = isc_mem_get(mgr->mctx, sizeof(isc_sockaddr_t));
	    if( nsSA != 0L )
	    {
		memset(nsSA,'\0', sizeof(isc_sockaddr_t));
		nsSA_Q = nsSA;
		dbus_svc_message_next_arg(dbus, iter, &(nsSA->type.sin.sin_addr.s_addr));	
		nsSA->type.sa.sa_family = AF_INET;
		nsSA->length = sizeof( nsSA->type.sin );
	    }
	    break;

	case TYPE_ARRAY:

	    argType = dbus_svc_message_element_type( dbus, iter );
	    if( argType == TYPE_BYTE )
	    {
		iparray = 0L;
		length = 0;

		dbus_svc_message_get_elements(dbus, iter, &length, &iparray);
		
		if( iparray != 0L )
		{
		    if (length == sizeof( struct in_addr ))
		    {
			nsSA = isc_mem_get(mgr->mctx, sizeof(isc_sockaddr_t));
			if( nsSA != 0L )
			{
			    memset(nsSA,'\0', sizeof(isc_sockaddr_t));
			    nsSA_Q = nsSA;

			    memcpy(&(nsSA->type.sin.sin_addr), iparray, sizeof( struct in_addr ));
			    nsSA->type.sa.sa_family = AF_INET;
			    nsSA->length = sizeof( nsSA->type.sin );
			}
		    }else
		    if (length == sizeof( struct in6_addr ))
		    {
			nsSA = isc_mem_get(mgr->mctx, sizeof(isc_sockaddr_t));
			if( nsSA != 0L )
			{
			    memset(nsSA,'\0', sizeof(isc_sockaddr_t));
			    nsSA_Q = nsSA;
			
			    memcpy(&(nsSA->type.sin6.sin6_addr), iparray, sizeof( struct in6_addr ));
			    nsSA->type.sa.sa_family = AF_INET6;
			    nsSA->length = sizeof( nsSA->type.sin6 );
			}
		    }
		}
	    }
	    break;

	case TYPE_STRING:

	    ip = 0L;
	    dbus_svc_message_next_arg(dbus, iter, &(ip));	
	    if( ip != 0L )
	    {
		length = strlen(ip);
		if( strspn(ip, "0123456789.") == length )
		{
		    nsSA = isc_mem_get(mgr->mctx, sizeof(isc_sockaddr_t));
		    if( nsSA != 0L) 
		    {
			memset(nsSA,'\0', sizeof(isc_sockaddr_t));
			if( inet_pton( AF_INET, ip, &(nsSA->type.sin.sin_addr)) )
			{
			    nsSA->type.sa.sa_family = AF_INET;
			    nsSA->length = sizeof(nsSA->type.sin);
			    nsSA_Q = nsSA;
			}
		    }
		}else
		if( strspn(ip, "0123456789AaBbCcDdEeFf:.") == length)
		{
		    nsSA = isc_mem_get(mgr->mctx, sizeof(isc_sockaddr_t));
		    if( nsSA != 0L )
		    {
			memset(nsSA,'\0', sizeof(isc_sockaddr_t));
			if( inet_pton( AF_INET6, ip, &(nsSA->type.sin6.sin6_addr)) )
			{
			    nsSA->type.sa.sa_family = AF_INET6;
			    nsSA->length = sizeof(nsSA->type.sin6);
			    nsSA_Q = nsSA;
			}
		    }
		}
	    }
	    break;
	   
	case TYPE_UINT16:
	    
	    if( (nsSA == 0L) || (nsSA->type.sa.sa_family == AF_UNSPEC) )
		break;
	    else
	    if( nsSA->type.sa.sa_family == AF_INET )
		dbus_svc_message_next_arg(dbus, iter, &(nsSA->type.sin.sin_port));
	    else
            if( nsSA->type.sa.sa_family == AF_INET6 )
		dbus_svc_message_next_arg(dbus, iter, &(nsSA->type.sin6.sin6_port));
	    break;

	case TYPE_BYTE:

	    dbus_svc_message_next_arg(dbus, iter, fwdpolicy);
	    if(*fwdpolicy > dns_fwdpolicy_only)
		*fwdpolicy =  dns_fwdpolicy_only;
	    break;

	default:

	    if(nsSA != 0L) 
		nsSA->type.sa.sa_family = AF_UNSPEC;
	    sprintf(error_message,"Unhandled argument type: %c", argType);
	    break;
	}

	if( (nsSA != 0L) 
	  &&(nsSA->type.sa.sa_family == AF_UNSPEC)
	  )
	{
	    sprintf(error_name, "com.redhat.named.InvalidArgument");
	    if( error_message[0]=='\0')
	    {
		if( nsSA == 0L )
		    sprintf(error_message,"Missing IP Address Name Server argument");
		else
		    sprintf(error_message,"Bad IP Address Name Server argument");
	    }
       	    if( nsSA != 0L )
		isc_mem_put(mgr->mctx, nsSA, sizeof(isc_sockaddr_t));
	    nsSA = 0L;
	    for( nsSA = ISC_LIST_HEAD( *saList );
		 (nsSA != 0L);
		 nsSA = ISC_LIST_HEAD( *saList )
	       )
	    {
		if(ISC_LINK_LINKED(nsSA, link))
		    ISC_LIST_DEQUEUE( *saList, nsSA, link );
		isc_mem_put( mgr->mctx, nsSA, sizeof( isc_sockaddr_t ) );
	    }
	    ISC_LIST_INIT(*saList);
	    return ISC_R_FAILURE;
	}

	if( nsSA != 0L )
	{
	    if( nsSA->type.sin.sin_port == 0 )
	    {
		if( ns_g_port != 0L )
		    nsSA->type.sin.sin_port = htons(ns_g_port);
		else
		{
		    result = ns_config_getport(ns_g_config, &(port) );
		    if( result != ISC_R_SUCCESS )
			port = 53;
		    nsSA->type.sin.sin_port = htons( port );
		}
	    }	
	
	    if( nsSA_Q != 0L )
	    {
		ISC_LINK_INIT(nsSA,link);
		ISC_LIST_ENQUEUE(*saList, nsSA, link);
		nsSA_Q = 0L;
	    }
	}

	argType = dbus_svc_message_next_arg_type( dbus, iter );    

    } while ( argType != TYPE_INVALID );

    return ISC_R_SUCCESS;
}

static void
dbus_mgr_handle_set_forwarders
(
    ns_dbus_mgr_t *mgr,
    DBUS_SVC dbus, 
    uint8_t  reply_expected,
    uint32_t serial,        
    const char *path,         
    const char *member,         
    const char *interface,
    const char *sender,   
    dbus_svc_MessageHandle msg
)
{
    dbus_svc_MessageIterator iter;
    char error_name[1024]="", error_message[1024]="", *domains=0L;
    uint32_t       argType, new_serial;
    DNSNameList nameList; 
    dns_name_t     *dnsName;
    SockAddrList  saList;    
    isc_sockaddr_t *nsSA;
    isc_result_t   result;
    uint8_t fwdpolicy = dns_fwdpolicy_only;
    
    iter = dbus_svc_message_iterator_new( dbus, msg );

    if( iter == 0L )
    {
	if( reply_expected )
	{
	    sprintf(error_name, "com.redhat.named.InvalidArguments");
	    sprintf(error_message,"SetForwarders requires DNS name and nameservers arguments.");
	    dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
			   TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
		         );
	}
	return;
    }
    
    argType = dbus_svc_message_next_arg_type( dbus, iter );

    if( argType != TYPE_STRING )
    {
	if( reply_expected )
	{
	    sprintf(error_name, "com.redhat.named.InvalidArguments");
	    sprintf(error_message,"SetForwarders requires DNS name string initial argument.");
	    dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
			   TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
		         );
	}
	return;
    } 
    
    dbus_svc_message_next_arg( dbus, iter, &domains );

    if( ( domains == 0L ) || (*domains == '\0') )
    {
	if( reply_expected )
	{
	    sprintf(error_name, "com.redhat.named.InvalidArguments");
	    sprintf(error_message,"SetForwarders requires DNS name string initial argument.");
	    dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
			   TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
		         );
	}
	return;
    }
    
    dbus_mgr_get_name_list( mgr, domains, &nameList, error_name, error_message );
    
    if( error_name[0] != '\0' )
    {
	if( reply_expected )
	{
	    dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
			   TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
		         );
	}
	return;
    }

    if( ISC_LIST_HEAD( nameList ) == 0L )
	return;	

    result = dbus_mgr_get_sa_list( mgr, iter, &saList , &fwdpolicy, error_name, error_message );

    if( result == ISC_R_SUCCESS )
    {
	result = dbus_mgr_set_forwarders( mgr, &nameList, &saList, fwdpolicy );

	if( result != ISC_R_SUCCESS )
	{
	    if( reply_expected )
	    {
		sprintf(error_name, "com.redhat.named.Failure");
		sprintf(error_message, isc_result_totext(result));
		dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
			       TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
		             );
	    }
	}else	
	    if( reply_expected )
		dbus_svc_send( dbus, RETURN, serial, &new_serial, sender, path, interface, member,
		       TYPE_UINT32, &result, TYPE_INVALID
	             );
    }else
    {
	if( reply_expected )
	{
	    dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
			   TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
		         );
	}
    }

    for( dnsName = ISC_LIST_HEAD( nameList );
	 (dnsName != 0L) ;
	 dnsName = ISC_LIST_HEAD( nameList )
	)
    {
	if( ISC_LINK_LINKED(dnsName,link) )
	    ISC_LIST_DEQUEUE( nameList, dnsName, link );
	isc_mem_put( mgr->mctx, dnsName, sizeof( dns_fixedname_t ) );
    }

    for( nsSA = ISC_LIST_HEAD(saList);
	 (nsSA != 0L) ;
	 nsSA = ISC_LIST_HEAD(saList)
       )
    {
	if( ISC_LINK_LINKED(nsSA,link) )
	    ISC_LIST_DEQUEUE( saList, nsSA, link );
	isc_mem_put(mgr->mctx, nsSA, sizeof(isc_sockaddr_t));
    }
}

static 
int dbus_mgr_msg_append_dns_name
(   DBUS_SVC dbus, 
    dbus_svc_MessageHandle msg,
    dns_name_t *name
)
{
    char  nameBuf[ DNS_NAME_FORMATSIZE ]="", *nameP=&(nameBuf[0]);

    dns_name_format(name, nameP, DNS_NAME_FORMATSIZE );    

    if( *nameP == '\0' )
	return 0;

    return dbus_svc_message_append_args( dbus, msg, TYPE_STRING, &nameP, TYPE_INVALID ) > 0;
}

typedef enum dbmoi_e
{
    OUTPUT_BINARY,
    OUTPUT_TEXT    
}   DBusMgrOutputInterface;

static 
int dbus_mgr_msg_append_forwarders
(   DBUS_SVC               dbus, 
    dbus_svc_MessageHandle msg,
    dns_forwarders_t      *fwdr,
    DBusMgrOutputInterface outputType
)
{
    isc_sockaddr_t *sa;
    char policyBuf[16]="", *pbp[1]={&(policyBuf[0])}, addressBuf[64]="", *abp[1]={&(addressBuf[0])};
    uint8_t *byteArray[1];

    if( outputType == OUTPUT_BINARY )
    {
	if(!dbus_svc_message_append_args
	   (   dbus, msg, 
	       TYPE_BYTE, &(fwdr->fwdpolicy),
	       TYPE_INVALID
	   )
	  ) return 0;
    }else
    if( outputType == OUTPUT_TEXT )
    {
	sprintf(policyBuf,"%s",
		(fwdr->fwdpolicy == dns_fwdpolicy_none)
		? "none"
		:  (fwdr->fwdpolicy == dns_fwdpolicy_first)
		   ? "first"
		   : "only"
	       );
	if(!dbus_svc_message_append_args
	   (   dbus, msg, 
	       TYPE_STRING, pbp,
	       TYPE_INVALID
	   )
	  ) return 0;
    }else
	return 0;

    for( sa = ISC_LIST_HEAD(fwdr->addrs);
	 sa != 0L;
	 sa = ISC_LIST_NEXT(sa, link)
	)
    {
	if( outputType == OUTPUT_BINARY )
	{
	    if( sa->type.sa.sa_family == AF_INET )
	    {
		if(!dbus_svc_message_append_args
		   (   dbus, msg, 
		       TYPE_UINT32, &(sa->type.sin.sin_addr.s_addr),
		       TYPE_INVALID
		   )
		  ) return 0;
		
		if(!dbus_svc_message_append_args
		   (   dbus, msg, 
		       TYPE_UINT16, &(sa->type.sin.sin_port),
		       TYPE_INVALID
		   )
		  ) return 0;
	    }else
	    if( sa->type.sa.sa_family == AF_INET6 )
	    {
		byteArray[0] = (uint8_t*)&(sa->type.sin6.sin6_addr);
		if(!dbus_svc_message_append_args
		   (   dbus, msg, 
		       TYPE_ARRAY, TYPE_BYTE, &byteArray, sizeof(struct in6_addr),
		       TYPE_INVALID
		   )
		  ) return 0;

		if(!dbus_svc_message_append_args
		   (   dbus, msg, 
		       TYPE_UINT16, &(sa->type.sin6.sin6_port),
		       TYPE_INVALID
		   )
		  ) return 0;
	    }else
		continue;
	}else
	if( outputType == OUTPUT_TEXT )
	{
	    if( sa->type.sa.sa_family == AF_INET )
	    {
		if( inet_ntop( AF_INET, &(sa->type.sin.sin_addr), addressBuf, sizeof(addressBuf)) == 0L )
		    continue;
		if(!dbus_svc_message_append_args
		   (   dbus, msg, 
		       TYPE_STRING, abp,
		       TYPE_INVALID
		   )
		  ) return 0;
		sprintf(addressBuf, "%hu", ntohs( sa->type.sin.sin_port ));
		if(!dbus_svc_message_append_args
		   (   dbus, msg, 
		       TYPE_STRING, abp,
		       TYPE_INVALID
		   )
		  ) return 0;
	    }else
	    if( sa->type.sa.sa_family == AF_INET6 )
	    {
		if( inet_ntop( AF_INET6, &(sa->type.sin6.sin6_addr), addressBuf, sizeof(addressBuf)) == 0L )
		    continue;
		if(!dbus_svc_message_append_args
		   (   dbus, msg, 
		       TYPE_STRING, abp,
		       TYPE_INVALID
		   )
		  ) return 0;
		sprintf(addressBuf, "%hu", ntohs( sa->type.sin6.sin6_port ));
		if(!dbus_svc_message_append_args
		   (   dbus, msg, 
		       TYPE_STRING, abp,
		       TYPE_INVALID
		   )
		  ) return 0;		
	    }else
		continue;
	}else
	    return 0;
    }
    return 1;
}

typedef struct dbm_m_s
{
    DBUS_SVC dbus;
    dbus_svc_MessageHandle msg;
    DBusMgrOutputInterface outputType;
}   DBusMgrMsg;

static
void forwarders_to_msg( dns_name_t *name, dns_forwarders_t *fwdr, void *mp )
{
    DBusMgrMsg *m = mp;
    
    if( (fwdr == 0L) || (name == 0L) || (mp == 0L))
	return;
    dbus_mgr_msg_append_dns_name  ( m->dbus, m->msg, name );
    dbus_mgr_msg_append_forwarders( m->dbus, m->msg, fwdr, m->outputType );    
}

static void
dbus_mgr_handle_list_forwarders
(
    DBUS_SVC dbus, 
    uint8_t  reply_expected,
    uint32_t serial,        
    const char *path,         
    const char *member,         
    const char *interface,
    const char *sender,   
    dbus_svc_MessageHandle msg
)
{
    char error_name[1024], error_message[1024];
    DBusMgrMsg m;
    uint32_t new_serial;
    dns_fwdtable_t *fwdtable = dbus_mgr_get_fwdtable();
    DBusMgrOutputInterface outputType = OUTPUT_BINARY;
    uint32_t length = strlen(interface);
        
    if( !reply_expected )
	return;

    if( (length > 4) && (strcmp(interface + (length - 4), "text")==0))
	outputType = OUTPUT_TEXT;

    if( fwdtable == 0L )
    {	
	sprintf(error_name,"com.redhat.dbus.Failure");
	sprintf(error_message, "%s", isc_result_totext(ISC_R_NOPERM));
	dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
		       TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
	             );	    
	return;
    }

    msg = dbus_svc_new_message( dbus, RETURN, serial, sender, path, interface, member);

    m.dbus = dbus;
    m.msg = msg;
    m.outputType = outputType;

    if( msg == 0L )
    {
	sprintf(error_name,"com.redhat.dbus.OutOfMemory");
	sprintf(error_message,"out of memory");
	dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
		       TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
	             );	    
    }
    
    dns_fwdtable_foreach( fwdtable, forwarders_to_msg, &m );

    dbus_svc_send_message( dbus, msg, &new_serial );
}

static void
dbus_mgr_handle_get_forwarders
(
    DBUS_SVC dbus, 
    uint8_t  reply_expected,
    uint32_t serial,        
    const char *path,         
    const char *member,         
    const char *interface,
    const char *sender,   
    dbus_svc_MessageHandle msg
)
{
    char error_name[1024], error_message[1024], *domain=0L;
    isc_result_t    result;
    dns_fixedname_t  fixedname;
    dns_name_t       *dnsName;
    isc_buffer_t     buffer;
    uint32_t         length,  new_serial;
    dns_fwdtable_t   *fwdtable;
    dns_forwarders_t *fwdr=0L;
    dns_name_t       *foundname;
    dns_fixedname_t  fixedFoundName;
    DBusMgrOutputInterface outputType = OUTPUT_BINARY;

    if( !reply_expected )
	return;

    length = strlen(interface);

    if( (length > 4) && (strcmp(interface + (length - 4), "text")==0))
	outputType = OUTPUT_TEXT;

    if( (!dbus_svc_get_args( dbus, msg, TYPE_STRING, &domain, TYPE_INVALID))
      ||(domain == 0L)
      ||(*domain == '\0')
      )
    {

	sprintf(error_name,"com.redhat.dbus.InvalidArguments");
	sprintf(error_message,"domain name argument expected");
	dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
		       TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
	             );	    
	return;
    }
    
    length = strlen( domain );

    isc_buffer_init( &buffer, domain, length); 

    isc_buffer_add(&buffer, length);		
    
    dns_fixedname_init(&fixedname);

    dnsName = dns_fixedname_name(&fixedname);

    result = dns_name_fromtext
	     (  dnsName, &buffer, dns_rootname, 0, NULL
	     ); 

    if( result != ISC_R_SUCCESS )
    {	
	sprintf(error_name,"com.redhat.dbus.InvalidArguments");
	sprintf(error_message,"invalid domain name argument: %s", domain);
	dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
		       TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
	             );	    
	return;
    }

    msg = dbus_svc_new_message( dbus, RETURN, serial, sender, path, interface, member);
    
    if( msg == 0L )
    {
	sprintf(error_name,"com.redhat.dbus.OutOfMemory");
	sprintf(error_message,"out of memory");
	dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
		       TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
	             );	
	return;
    }	
    
    fwdtable = dbus_mgr_get_fwdtable();

    if( fwdtable == 0L )
    {
	sprintf(error_name,"com.redhat.dbus.Failure");
	sprintf(error_message, "%s", isc_result_totext(ISC_R_NOPERM));
	dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
		       TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
	             );	    
	return;
    }

    dns_fixedname_init(&fixedFoundName);
    foundname = dns_fixedname_name(&fixedFoundName);
	
    if( ( dns_fwdtable_find_closest( fwdtable, dnsName, foundname, &fwdr ) == ISC_R_SUCCESS )
      &&( fwdr != 0L )
      )
    {
	if( (!dbus_mgr_msg_append_dns_name( dbus, msg, foundname ))
	  ||(!dbus_mgr_msg_append_forwarders( dbus, msg, fwdr, outputType ))
	  )
	{
	    sprintf(error_name,"com.redhat.dbus.OutOfMemory");
	    sprintf(error_message,"out of memory");
	    dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
			   TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
		);	    
	    return;
	}	
	
    }else
    {
	result = ISC_R_NOTFOUND;
	if( outputType == OUTPUT_BINARY )
	{
	    dbus_svc_message_append_args( dbus, msg, 
					  TYPE_UINT32, &(result),
					  TYPE_INVALID
		                        ) ;
	}else
	{
	    sprintf(error_name,"com.redhat.dbus.NotFound");
	    sprintf(error_message,"Not Found");
	    dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
			   TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
		         );	   
	    return;
	}
    }
    dbus_svc_send_message( dbus, msg, &new_serial );
}

static void
dbus_mgr_check_dhcdbd_state( ns_dbus_mgr_t *mgr, dbus_svc_MessageHandle msg )
{
    DBUS_SVC dbus = mgr->dbus;
    char *name_owned = 0L,
	 *old_owner = 0L,
	 *new_owner = 0L;
    
    if( !dbus_svc_get_args( dbus, msg,
			    TYPE_STRING, &name_owned,
			    TYPE_STRING, &old_owner,
			    TYPE_STRING, &new_owner,
			    TYPE_INVALID
	                  )
      ) return;
	
    dbus_mgr_log_dbg("NameOwnerChanged: %s %s %s ( %s )", name_owned, old_owner, new_owner, mgr->dhcdbd_name);

    if( (name_owned == 0L) || (new_owner == 0L) || (old_owner == 0L) )
	return;

    if( strcmp( name_owned, "com.redhat.dhcp" ) == 0 )
    {
	if( *new_owner == '\0' )
	{
	    isc_mem_put(mgr->mctx, mgr->dhcdbd_name, strlen(mgr->dhcdbd_name) + 1);
	    mgr->dhcdbd_name = 0L;
	    dbus_mgr_log_err("D-BUS dhcdbd subscription disabled.");
	    return;
	}
	if( (mgr->dhcdbd_name == 0L) 
	  ||( strcmp( mgr->dhcdbd_name, new_owner) != 0 )
	  )
	{
	    if( mgr->dhcdbd_name != 0L )
	    {
		isc_mem_put(mgr->mctx, mgr->dhcdbd_name, strlen(mgr->dhcdbd_name)+1);
		mgr->dhcdbd_name = 0L;
	    }
	    mgr->dhcdbd_name = isc_mem_get(mgr->mctx, strlen(new_owner) + 1);
	    if( mgr->dhcdbd_name == 0L )
		return;
	    strcpy( mgr->dhcdbd_name, new_owner );
	    dbus_mgr_subscribe_to_dhcdbd( mgr );
	}
    }else
    if(  ( mgr->dhcdbd_name != 0L ) 
      && ( strcmp(mgr->dhcdbd_name, name_owned) == 0L )
      && ( *new_owner == '\0' )
      ) 
    {
	isc_mem_put(mgr->mctx, mgr->dhcdbd_name, strlen(mgr->dhcdbd_name));
	mgr->dhcdbd_name = 0L;
	dbus_mgr_log_err("D-BUS dhcdbd subscription disabled.");
    }
}

static int dbus_mgr_dhc_if_comparator( const void *p1, const void *p2 )
{
    return( strcmp( ((const DHC_IF*)p1)->if_name, ((const DHC_IF*)p2)->if_name) );
}

static 
dns_name_t *dbus_mgr_if_reverse_ip_name
(   ns_dbus_mgr_t *mgr,
    struct in_addr ip_address, 
    struct in_addr subnet_mask 
)
{
    dns_name_t *dns_name =0L;
    dns_fixedname_t *fixedname=0L;
    char name [ DNS_NAME_FORMATSIZE ], *p;
    uint32_t ip = (ntohl(ip_address.s_addr) & ntohl(subnet_mask.s_addr)), i;
    isc_buffer_t buffer;
    isc_result_t result;

    if( (ip == 0) || (ip == 0xffffffff) )
	return 0L;
    
    for(i = 8, p = name; (i < 32); i += 8)
	if( ip & ( 0xff << i ) )
	    p += sprintf(p, "%u.", (((ip & ( 0xff << i )) >> i ) & 0xff) );

    if( p > name )
    {
	p += sprintf(p, "in-addr.arpa");
	isc_buffer_init( &buffer, name, p - name );
	isc_buffer_add(&buffer, p - name);		
	
	fixedname = isc_mem_get( mgr->mctx, sizeof( dns_fixedname_t ));

	dns_fixedname_init(fixedname);

	dns_name = dns_fixedname_name(fixedname);

	result= dns_name_fromtext
	        (  dns_name, &buffer, dns_rootname, 0, NULL
		); 

	ISC_LINK_INIT(dns_name, link);
	if( result == ISC_R_SUCCESS )
	    return dns_name;
    }
    return 0L;
}

static void
dbus_mgr_free_dhc( void *p )
{
    DHC_IF *d_if = p;
    dns_name_t *dn;
    isc_sockaddr_t *sa;

    isc_mem_put( ns_g_mctx, d_if->if_name, strlen(d_if->if_name) + 1);
    for( sa = ISC_LIST_HEAD( d_if->dns );
	 sa != NULL;
	 sa = ISC_LIST_HEAD( d_if->dns )
       )
    {
	if( ISC_LINK_LINKED( sa, link ) )
	    ISC_LIST_UNLINK( d_if->dns, sa, link );
	isc_mem_put(ns_g_mctx, sa, sizeof(isc_sockaddr_t));
    }
    for( dn = ISC_LIST_HEAD( d_if->dn );
	 dn != NULL;
	 dn = ISC_LIST_HEAD( d_if->dn )
       )
    {
	if( ISC_LINK_LINKED( dn, link ) )
	    ISC_LIST_UNLINK( d_if->dn, dn, link );
	isc_mem_put( ns_g_mctx, dn, sizeof( dns_fixedname_t ) );
    }
    isc_mem_put( ns_g_mctx, d_if, sizeof(DHC_IF));    
}

static void
dbus_mgr_handle_dhcdbd_message
(
    ns_dbus_mgr_t *mgr,
    const char *path,
    const char *member,
    dbus_svc_MessageHandle msg
)
{
    DBUS_SVC dbus = mgr->dbus;
    DHC_IF *d_if, *const*d_ifpp, dif;
    DHC_State dhc_state;
    char *if_name, *opt_name, error_name[1024]="", error_message[1024]="";
    uint8_t *value=0L;
    uint32_t length;
    isc_result_t result;
    isc_sockaddr_t *sa = 0L;
    dns_name_t *dn = 0L;
    struct in_addr *ip;
    in_port_t port;
    char dnBuf[ DNS_NAME_FORMATSIZE ];
    isc_buffer_t buffer;
    DBusMgrInitialFwdr *ifwdr, *const*ifwdpp, ifwd;    
    ISC_LIST(DBusMgrInitialFwdr) ifwdrList;
    DNSNameList nameList;
    dbus_mgr_log_dbg("Got dhcdbd message: %s %s %p", path, member, msg );

    if( ( if_name = strrchr(path,'/') ) == 0L )
    {
	dbus_mgr_log_err("bad path in dhcdbd message:", path);
	return;
    }

    ++if_name;
    dif.if_name = if_name;

    if( ((d_ifpp=tfind( &dif, &(mgr->dhc_if), dbus_mgr_dhc_if_comparator)) == 0L)
      ||((d_if = *d_ifpp) == 0L)
      )
    {
	d_if = isc_mem_get( mgr->mctx, sizeof(DHC_IF));
	if( d_if == 0L )	  
	{
	    dbus_mgr_log_err("out of memory");
	    return;
	}
	memset(d_if, '\0', sizeof(DHC_IF));
	if((d_if->if_name =  isc_mem_get( mgr->mctx, strlen(if_name) + 1)) == 0L)
	{
	    dbus_mgr_log_err("out of memory");
	    return;
	}
	strcpy(d_if->if_name, if_name);
	d_if->dhc_state = DHC_INVALID;
	d_if->previous_state = DHC_INVALID;
	ISC_LIST_INIT( d_if->dn );
	ISC_LIST_INIT( d_if->dns );
	if( tsearch( d_if, &(mgr->dhc_if), dbus_mgr_dhc_if_comparator) == 0L )
	{
	    dbus_mgr_log_err("out of memory");
	    return;
	}
    }

    if( strcmp(member, "reason") == 0 )
    {
	if( (!dbus_svc_get_args( dbus, msg,
				 TYPE_STRING, &opt_name,
				 TYPE_ARRAY, TYPE_BYTE, &value, &length,
				 TYPE_INVALID
		               )
	    )
	  ||( value == 0L)
	  ||( length != sizeof(uint32_t))
	  ||( *((uint32_t*)value) > DHC_END_OPTIONS)
	  )
	{
	    dbus_mgr_log_err("Invalid DHC reason value received from dhcdbd");
	    return;
	}
	dhc_state = (DHC_State) *((uint32_t*)value);
	dbus_mgr_log_dbg("reason: %d %d %d", dhc_state, d_if->dhc_state, d_if->previous_state);
	switch( dhc_state )
	{

	case  DHC_END_OPTIONS:
	    switch( d_if->dhc_state )
	    {
	    case  DHC_END_OPTIONS:
		break;

	    case  DHC_RENEW: 
	    case  DHC_REBIND:		
		if( ( d_if->previous_state != DHC_INVALID ) 
		  &&( d_if->previous_state != DHC_RELEASE )
		  ) break; 
		    /* DHC_RENEW means the same lease parameters were obtained. 
		     * Only do configuration if we started up with existing dhclient
		     * which has now renewed - else we are already configured correctly.
		     */
		dbus_mgr_log_err("D-BUS: existing dhclient for interface %s RENEWed lease", if_name);

	    case  DHC_REBOOT:
	    case  DHC_BOUND:
		d_if->previous_state = d_if->dhc_state;
		d_if->dhc_state = DHC_BOUND;
		if( (dn = dbus_mgr_if_reverse_ip_name(mgr, d_if->ip, d_if->subnet_mask )) != 0L )
		{
		    ISC_LIST_APPEND(d_if->dn, dn, link );
		} 
		if( ( ISC_LIST_HEAD( d_if->dn ) != NULL )
		  &&( ISC_LIST_HEAD( d_if->dns ) != NULL )
		  )
		{
		    dbus_mgr_log_err("D-BUS: dhclient for interface %s acquired new lease - creating forwarders.", 
				     if_name
			            );
		    result = dbus_mgr_set_forwarders( mgr, &(d_if->dn), &(d_if->dns), dns_fwdpolicy_only );
		    if( result != ISC_R_SUCCESS )
		    {
			dbus_mgr_log_err("D-BUS: forwarder configuration failed: %s", isc_result_totext(result));
		    }
		}
		break;	  

	    case  DHC_STOP:
	    case  DHC_TIMEOUT:
	    case  DHC_FAIL:
	    case  DHC_EXPIRE:
	    case  DHC_RELEASE:
		d_if->previous_state = d_if->dhc_state;
		d_if->dhc_state = DHC_RELEASE;
		if( ISC_LIST_HEAD( d_if->dn ) != NULL )
		{
		    dbus_mgr_log_err("D-BUS: dhclient for interface %s released lease - removing forwarders.", 
				     if_name);
		    for( sa = ISC_LIST_HEAD( d_if->dns );
			 sa != 0L;
			 sa = ISC_LIST_HEAD( d_if->dns )
		       )
		    {
			if( ISC_LINK_LINKED( sa, link ) )
			    ISC_LIST_UNLINK( d_if->dns, sa, link );
			isc_mem_put( mgr->mctx, sa, sizeof(isc_sockaddr_t));
		    }
		    ISC_LIST_INIT( d_if->dns );
		    ISC_LIST_INIT( ifwdrList );

		    for( dn = ISC_LIST_HEAD( d_if->dn );
			 dn != 0L;
			 dn = ISC_LIST_NEXT( dn, link )
		       )
		    {
		        dns_name_init( &(ifwd.dn), NULL );
			isc_buffer_init( &buffer, dnBuf, DNS_NAME_FORMATSIZE);
			dns_name_setbuffer( &(ifwd.dn), &buffer);
			dns_name_copy(dn, &(ifwd.dn), NULL);
			if( ((ifwdpp = tfind(&ifwd, &(mgr->ifwdt), dbus_mgr_ifwdr_comparator)) != 0L )
			  &&((ifwdr = *ifwdpp) != 0L)
			  )
			{
			    ISC_LIST_APPEND( ifwdrList, ifwdr, link );
			}			
		    }
		    
		    result = dbus_mgr_set_forwarders( mgr, &(d_if->dn), &(d_if->dns), dns_fwdpolicy_none );
		    if( result != ISC_R_SUCCESS )
		    {
			dbus_mgr_log_err("D-BUS: removal of forwarders failed: %s", isc_result_totext(result));
		    }

		    for( dn = ISC_LIST_HEAD( d_if->dn );
			 dn != 0L;
			 dn = ISC_LIST_HEAD( d_if->dn )
		       )
		    {
			if( ISC_LINK_LINKED( dn, link ) )
			    ISC_LIST_UNLINK( d_if->dn, dn, link );			
			isc_mem_put( mgr->mctx, dn, sizeof( dns_fixedname_t ) );
		    }
		    ISC_LIST_INIT( d_if->dn );

		    for( ifwdr = ISC_LIST_HEAD( ifwdrList );
			 ifwdr != 0L;
			 ifwdr = ISC_LIST_HEAD( ifwdrList )
		       )
		    {
			if( ISC_LINK_LINKED( ifwdr, link ) )
			    ISC_LIST_UNLINK( ifwdrList, ifwdr, link );
			ISC_LINK_INIT(ifwdr, link);
			ISC_LIST_INIT(nameList);
			ISC_LINK_INIT(&(ifwdr->dn), link);
			ISC_LIST_APPEND( nameList, &(ifwdr->dn), link );
			result = dbus_mgr_set_forwarders( mgr, &nameList, 
							  &(ifwdr->sa), 
							  ifwdr->fwdpolicy 
			                                );
			if( result != ISC_R_SUCCESS )
			{
			    dbus_mgr_log_err("D-BUS: restore of forwarders failed: %s", isc_result_totext(result));
			}			
		    }
		}

	    case  DHC_ABEND:
	    case  DHC_END:
	    case  DHC_NBI:
	    case  DHC_PREINIT:
	    case  DHC_MEDIUM:
	    case  DHC_START:
	    case  DHC_INVALID:
	    default:
		break;
	    }
	    break;

	case  DHC_BOUND:
	case  DHC_REBOOT:
	case  DHC_REBIND:	
	case  DHC_RENEW:
        case  DHC_STOP:
        case  DHC_TIMEOUT:
	case  DHC_FAIL:
	case  DHC_EXPIRE:
	case  DHC_RELEASE:
		d_if->previous_state = d_if->dhc_state;
		d_if->dhc_state = dhc_state;

	case  DHC_ABEND:
	case  DHC_END:
	case  DHC_NBI:
	case  DHC_PREINIT:
	case  DHC_MEDIUM:
	case  DHC_START:
	case  DHC_INVALID:
	default:
	    break;
	}	
    }else
    if( strcmp( member, "domain_name" ) == 0 )
    {	
	if( (!dbus_svc_get_args( dbus, msg,
				 TYPE_STRING, &opt_name,
				 TYPE_ARRAY, TYPE_BYTE, &value, &length,
				 TYPE_INVALID
		               )
	    )
	  ||( value == 0L)
	  ||( length == 0)
	  )
	{
	    dbus_mgr_log_err("Invalid domain_name value received from dhcdbd");
	    return;	
	}
	dbus_mgr_log_dbg("domain-name %s", (char*)value);
	dbus_mgr_get_name_list( mgr, (char*)value, &(d_if->dn), error_name, error_message );
	if( ( error_message[0] != '\0' ) || (ISC_LIST_HEAD(d_if->dn) == 0L ))
	{
	    dbus_mgr_log_err("Bad domain_name value: %s", error_message );
	}
    }else
    if( strcmp( member, "domain_name_servers") == 0 )
    {
	if( (!dbus_svc_get_args( dbus, msg,
				 TYPE_STRING, &opt_name,
				 TYPE_ARRAY, TYPE_BYTE, &value, &length,
				 TYPE_INVALID
		               )
	    )
	  ||( value == 0L)
	  ||( length == 0)
	  )
	{
	    dbus_mgr_log_err("Invalid domain_name_servers value received from dhcdbd");
	    return;	
	}	
	for(ip = (struct in_addr*) value; ip < ((struct in_addr*)(value + length)); ip++)
	{
	    dbus_mgr_log_dbg("domain-name-servers: %s", inet_ntop(AF_INET, value, error_name, 16));
	    sa = isc_mem_get(mgr->mctx, sizeof(isc_sockaddr_t));
	    memset(sa, '\0', sizeof(isc_sockaddr_t));
	    sa->type.sin.sin_addr = *ip;
	    sa->type.sa.sa_family = AF_INET;
	    sa->length = sizeof(sa->type.sin);
	    result = ns_config_getport(ns_g_config, &(port) );
	    if( result != ISC_R_SUCCESS )
		port = 53;
	    sa->type.sin.sin_port = htons( port );
	    ISC_LIST_APPEND(d_if->dns, sa, link);
	}
    }else
    if( strcmp(member, "ip_address") == 0)
    {
	if( (!dbus_svc_get_args( dbus, msg,
				 TYPE_STRING, &opt_name,
				 TYPE_ARRAY, TYPE_BYTE, &value, &length,
				 TYPE_INVALID
		               )
	    )
	  ||( value == 0L)
	  ||( length != sizeof(struct in_addr) )
	  )
	{
	    dbus_mgr_log_err("Invalid ip_address value received from dhcdbd");
	    return;
	}
	dbus_mgr_log_dbg("ip-address: %s", inet_ntop(AF_INET, value, error_name, 16));
	d_if->ip = *((struct in_addr*)value);
    
    }else
    if( strcmp(member, "subnet_mask") == 0 )
    {
	if( (!dbus_svc_get_args( dbus, msg,
				 TYPE_STRING, &opt_name,
				 TYPE_ARRAY, TYPE_BYTE, &value, &length,
				 TYPE_INVALID
		               )
	    )
	  ||( value == 0L)
	  ||( length != sizeof(struct in_addr) )
	  )
	{
	    dbus_mgr_log_err("Invalid subnet_mask value received from dhcdbd");
	    return;
	}
	dbus_mgr_log_dbg("subnet-mask: %s", inet_ntop(AF_INET, value, error_name, 16));
	d_if->subnet_mask = *((struct in_addr*)value);
    }
}

static
dbus_svc_HandlerResult 
dbus_mgr_message_handler
(  
    DBusMsgHandlerArgs
)
{
    char error_name[1024], error_message[1024];
    ns_dbus_mgr_t *mgr = object;
    uint32_t new_serial;

    if_suffix = prefix = suffix = prefixObject = 0L;

    dbus_mgr_log_dbg("D-BUS message: %u %u %u %s %s %s %s %s %s",
		     type, reply_expected, serial, destination, path, member, interface, sender, signature
	            );

    if (  ( type == SIGNAL )
	&&( strcmp(path,"/org/freedesktop/DBus/Local") == 0 )
       )
    {
	if( strcmp(member,"Disconnected") == 0 )
	    dbus_mgr_dbus_shutdown_handler( mgr );
    }else
    if( ( type == SIGNAL )
      &&( strcmp(path,"/org/freedesktop/DBus") == 0 )
      &&(strcmp(member,"NameOwnerChanged") == 0)
      &&(strcmp(signature, "sss") == 0)
      )
    {
	dbus_mgr_check_dhcdbd_state( mgr, msg );
    }else
    if( ( type == SIGNAL ) 
      &&( (sender != 0L) && (mgr->dhcdbd_name != 0L) && (strcmp(sender,mgr->dhcdbd_name)   == 0)) 
      &&( strcmp(interface,"com.redhat.dhcp.subscribe.binary") == 0 )
      )
    {
	dbus_mgr_handle_dhcdbd_message( mgr, path, member, msg );
    }else
    if( (type == CALL) 
      &&( strcmp(destination, DBUSMGR_DESTINATION)==0)
      &&( strcmp(path, DBUSMGR_OBJECT_PATH)==0)
      )
    {
	if( strcmp(member, "SetForwarders") == 0 )
	    dbus_mgr_handle_set_forwarders
	    ( mgr, dbus, reply_expected, serial, path, member, interface, sender, msg );
	else
	if( strcmp(member, "GetForwarders") == 0 )
	{
	    if( *signature != '\0' )	   
		dbus_mgr_handle_get_forwarders
		( dbus, reply_expected, serial, path, member, interface, sender, msg );
	    else       
		dbus_mgr_handle_list_forwarders
		( dbus, reply_expected, serial, path, member, interface, sender, msg );
	}else
	if( reply_expected )
	{
	    sprintf(error_name, "InvalidOperation");
	    sprintf(error_message, "Unrecognized path / interface / member");
	    dbus_svc_send( dbus, ERROR, serial, &new_serial, sender, path, interface, member,
			   TYPE_STRING, error_name, TYPE_STRING, error_message, TYPE_INVALID 
		);	    
	}
    }
    return HANDLED;
}

static void
dbus_mgr_read_watch_activated(isc_task_t *t, isc_event_t *ev)
{
    DBusMgrSocket *sfd = (DBusMgrSocket*)(ev->ev_arg);
    t = t;
    isc_mem_put(sfd->mgr->mctx, ev, ev->ev_size);
    dbus_mgr_log_dbg("watch %d READ",sfd->fd);  
    isc_socket_fd_handle_reads( sfd->sock, sfd->ser );
    dbus_svc_handle_watch( sfd->mgr->dbus, sfd->fd, WATCH_ENABLE | WATCH_READ );
}

static void
dbus_mgr_write_watch_activated(isc_task_t *t, isc_event_t *ev)
{
    DBusMgrSocket *sfd = (DBusMgrSocket*)(ev->ev_arg);
    t = t;
    isc_mem_put(sfd->mgr->mctx, ev, ev->ev_size);
    dbus_mgr_log_dbg("watch %d WRITE",sfd->fd);
    isc_socket_fd_handle_writes( sfd->sock, sfd->ser );
    dbus_svc_handle_watch( sfd->mgr->dbus, sfd->fd, WATCH_ENABLE | WATCH_WRITE );    
}

static void
dbus_mgr_watches_selected(isc_task_t *t, isc_event_t *ev)
{
    ns_dbus_mgr_t *mgr = (ns_dbus_mgr_t*)(ev->ev_arg);
    t = t;
    isc_mem_put(mgr->mctx, ev, ev->ev_size);
    if( ( mgr->dbus == 0L ) || (mgr->sockets == 0L))
    {
	return;
    }
    dbus_mgr_log_dbg("watches selected");
    dbus_svc_dispatch( mgr->dbus );
    dbus_mgr_log_dbg("dispatch complete");
}

static int dbus_mgr_socket_comparator( const void *p1, const void *p2 )
{
    return( (   ((const DBusMgrSocket*)p1)->fd 
	     == ((const DBusMgrSocket*)p2)->fd
	    ) ? 0
	      : (   ((const DBusMgrSocket*)p1)->fd 
	          > ((const DBusMgrSocket*)p2)->fd
	        ) ? 1
	          : -1
	  );
}

static void 
dbus_mgr_watch_handler( int fd, dbus_svc_WatchFlags flags, void *mgrp )
{
    ns_dbus_mgr_t *mgr = mgrp;
    DBusMgrSocket sockFd, *sfd=0L, *const*spp=0L;
    isc_result_t  result=ISC_R_SUCCESS;
    isc_socketevent_t *sev;
    isc_event_t *pev[1];

    if(mgr == 0L)
	return;

    if( (flags & 7) == WATCH_ERROR )
	return;

    sockFd.fd = fd;

    dbus_mgr_log_dbg("watch handler: fd %d %d", fd, flags);

    if( ((spp = tfind( &sockFd, &(mgr->sockets), dbus_mgr_socket_comparator) ) == 0L )
      ||((sfd = *spp) == 0L )
      )
    {
	if( ( flags & WATCH_ENABLE ) == 0 )
	    return;

	sfd = isc_mem_get(mgr->mctx, sizeof(DBusMgrSocket));
	if( sfd == 0L )
	{
	    dbus_mgr_log_err("dbus_mgr: out of memory" );
	    return;
	}
	sfd->fd = fd;
	sfd->mgr = mgr;
	sfd->ser = sfd->sew = sfd->sel = 0L;

	if( tsearch(sfd, &(mgr->sockets), dbus_mgr_socket_comparator) == 0L )
	{
	    dbus_mgr_log_err("dbus_mgr: out of memory" );
	    isc_mem_put(mgr->mctx, sfd, sizeof(DBusMgrSocket));
	    return;
	}
	sfd->sock = 0L;
	result = isc_socket_create( mgr->socketmgr, fd, isc_sockettype_fd, &(sfd->sock) );
	if( result != ISC_R_SUCCESS )
	{
	    dbus_mgr_log_err("dbus_mgr: isc_socket_create failed: %s", 
			     isc_result_totext(result)
	                    );
	    tdelete(sfd,  &(mgr->sockets), dbus_mgr_socket_comparator);
	    isc_mem_put(mgr->mctx, sfd, sizeof(DBusMgrSocket));
	    return;
	}
    }
    
    if( (flags & WATCH_ENABLE) == WATCH_ENABLE )
    {
	if( (flags & WATCH_READ) == WATCH_READ )
	{
	    if( sfd->ser == 0L )
	    {
		sfd->ser = (isc_socketevent_t *)
		    isc_event_allocate
		    (
			mgr->mctx, mgr->task,
			ISC_SOCKEVENT_READ_READY,
			dbus_mgr_read_watch_activated,
			sfd,
			sizeof(isc_socketevent_t)
		    );

		if( sfd->ser == 0L )
		{
		    dbus_mgr_log_err("dbus_mgr: out of memory" );
		    tdelete(sfd,  &(mgr->sockets), dbus_mgr_socket_comparator);
		    isc_mem_put(mgr->mctx, sfd, sizeof(DBusMgrSocket));
		    return;		    
		}

		sev = isc_socket_fd_handle_reads(sfd->sock, sfd->ser );

	    }else
	    {
		sev = isc_socket_fd_handle_reads(sfd->sock, sfd->ser );
	    }
	}
	if( (flags & WATCH_WRITE) == WATCH_WRITE )
	{
	    if( sfd->sew == 0L )
	    {
		sfd->sew = (isc_socketevent_t *)
		    isc_event_allocate
		    (
			mgr->mctx, mgr->task,
			ISC_SOCKEVENT_WRITE_READY,
			dbus_mgr_write_watch_activated,
			sfd,
			sizeof(isc_socketevent_t)
		    );
		if( sfd->sew == 0L )
		{
		    dbus_mgr_log_err("dbus_mgr: out of memory" );
		    tdelete(sfd,  &(mgr->sockets), dbus_mgr_socket_comparator);
		    isc_mem_put(mgr->mctx, sfd, sizeof(DBusMgrSocket));
		    return;		    
		}
		
		sev = isc_socket_fd_handle_writes(sfd->sock, sfd->sew );
		
	    }else
	    {
		sev = isc_socket_fd_handle_writes(sfd->sock, sfd->sew );
	    }
	}
	if( (sfd->ser != 0L) || (sfd->sew != 0L) )
	{
	    if( sfd->sel == 0L )
	    {
		sfd->sel = (isc_socketevent_t *)
		    isc_event_allocate
		    (
			mgr->mctx, mgr->task,
			ISC_SOCKEVENT_SELECTED,
			dbus_mgr_watches_selected,
			mgr,
			sizeof(isc_socketevent_t)
		    );
		if( sfd->sel == 0L )
		{
		    dbus_mgr_log_err("dbus_mgr: out of memory" );
		    tdelete(sfd,  &(mgr->sockets), dbus_mgr_socket_comparator);
		    isc_mem_put(mgr->mctx, sfd, sizeof(DBusMgrSocket));
		    return;		    
		}
		
		sev = isc_socket_fd_handle_selected(sfd->sock, sfd->sel );
		
	    }else
	    {
		sev = isc_socket_fd_handle_selected(sfd->sock, sfd->sel);
	    }
	}
    }else
    {
	dbus_mgr_log_dbg("watch %d disabled",fd);
	if(flags & WATCH_READ)
	{
	    sev = isc_socket_fd_handle_reads( sfd->sock, 0L );
	    if( sev != 0L )
	    {
		pev[0]=(isc_event_t*)sev;
		isc_event_free(pev);
	    }
	    sfd->ser = 0L;
	}

	if( flags & WATCH_WRITE )
	{
	    sev = isc_socket_fd_handle_writes( sfd->sock, 0L );
	    if( sev != 0L )
	    {
		pev[0]=(isc_event_t*)sev;
		isc_event_free(pev);
	    }
	    sfd->sew = 0L;
	}

	if( (sfd->ser == 0L) && (sfd->sew == 0L) )
	{
	    sev = isc_socket_fd_handle_selected( sfd->sock, 0L );
	    if( sev != 0L )
	    {
		pev[0]=(isc_event_t*)sev;
		isc_event_free(pev);
	    }
	    sfd->sel = 0L;

	    tdelete(sfd, &(mgr->sockets), dbus_mgr_socket_comparator);

	    isc_mem_put(mgr->mctx, sfd,  sizeof(DBusMgrSocket));
	}
    }
}

static
void dbus_mgr_close_socket( const void *p, const VISIT which, const int level)
{
    DBusMgrSocket *const*spp=p, *sfd;
    isc_event_t *ev ;
    int i =  level ? 0 :1;
    i &= i;

    if( (spp==0L) || ((sfd = *spp)==0L) 
      ||((which != leaf) && (which != postorder))
      ) return;
   
    if( sfd->ser != 0L )
    {
	ev = (isc_event_t *)isc_socket_fd_handle_reads(sfd->sock, 0);
	if( ev != 0L )
	    isc_event_free((isc_event_t **)&ev);
	sfd->ser = 0L;
    }

    if( sfd->sew != 0L )
    {
	ev = (isc_event_t *)isc_socket_fd_handle_writes(sfd->sock, 0);
	if( ev != 0L )
	    isc_event_free((isc_event_t **)&ev);
	sfd->sew = 0L;
    }

    if( sfd->sel != 0L )
    {
	ev = (isc_event_t *)isc_socket_fd_handle_selected(sfd->sock, 0);
	if( ev != 0L )
	    isc_event_free((isc_event_t **)&ev);
	sfd->sel = 0L;
	dbus_mgr_log_dbg("CLOSED socket %d", sfd->fd);
    }
}

static
void dbus_mgr_destroy_socket( void *p )
{
    DBusMgrSocket *sfd = p;

    isc_mem_put( sfd->mgr->mctx, sfd, sizeof(DBusMgrSocket) );
}
