#include "syslib.h"
#include <assert.h>
#include <minix/sysutil.h>
#include <stdio.h>

/* Self variables. */
#define SEF_SELF_NAME_MAXLEN 20
PRIVATE char sef_self_name[SEF_SELF_NAME_MAXLEN];
PRIVATE endpoint_t sef_self_endpoint;

/* Debug. */
#define SEF_DEBUG_HEADER_MAXLEN 32
PRIVATE time_t sef_debug_boottime = 0;
PRIVATE u32_t sef_debug_system_hz = 0;
PRIVATE time_t sef_debug_time_sec = 0;
PRIVATE time_t sef_debug_time_us = 0;
PRIVATE char sef_debug_header_buff[SEF_DEBUG_HEADER_MAXLEN];
FORWARD _PROTOTYPE( void sef_debug_refresh_params, (void) );
PUBLIC _PROTOTYPE( char* sef_debug_header, (void) );

/* SEF Init prototypes. */
EXTERN _PROTOTYPE( int do_sef_rs_init, (void) );
EXTERN _PROTOTYPE( int do_sef_init_request, (message *m_ptr) );

/* SEF Live update prototypes. */
EXTERN _PROTOTYPE( void do_sef_lu_before_receive, (void) );
EXTERN _PROTOTYPE( int do_sef_lu_request, (message *m_ptr) );

/* SEF Ping prototypes. */
EXTERN _PROTOTYPE( int do_sef_ping_request, (message *m_ptr) );

/*===========================================================================*
 *				sef_startup				     *
 *===========================================================================*/
PUBLIC void sef_startup()
{
/* SEF startup interface for system processes. */
  int r;

  /* Get information about self. */
  r = sys_whoami(&sef_self_endpoint, sef_self_name, SEF_SELF_NAME_MAXLEN);
  if ( r != OK) {
      sef_self_endpoint = SELF;
      sprintf(sef_self_name, "%s", "Unknown");
  }

#if INTERCEPT_SEF_INIT_REQUESTS
  /* Intercept SEF Init requests. */
  if(sef_self_endpoint == RS_PROC_NR) {
      if((r = do_sef_rs_init()) != OK) {
          panic("SEF", "unable to complete init", r);
      }
  }
  else {
      message m;

      if((r = receive(RS_PROC_NR, &m)) != OK) {
          panic("SEF", "unable to receive from RS", r);
      }
      if(IS_SEF_INIT_REQUEST(&m)) {
          if((r = do_sef_init_request(&m)) != OK) {
              panic("SEF", "unable to process init request", r);
          }
      }
      else {
          panic("SEF", "unable to receive init request", NO_NUM);
      }
  }
#endif
}

/*===========================================================================*
 *				sef_receive				     *
 *===========================================================================*/
PUBLIC int sef_receive(endpoint_t src, message *m_ptr)
{
/* SEF receive() interface for system processes. */
  int r;

  while(TRUE) {

#if INTERCEPT_SEF_LU_REQUESTS
      /* Handle SEF Live update before receive events. */
      do_sef_lu_before_receive();
#endif

      /* Receive and return in case of error. */
      r = receive(src, m_ptr);
      if(r != OK) {
          return r;
      }

#if INTERCEPT_SEF_PING_REQUESTS
      /* Intercept SEF Ping requests. */
      if(IS_SEF_PING_REQUEST(m_ptr)) {
          if(do_sef_ping_request(m_ptr) == OK) {
              continue;
          }
      }
#endif

#if INTERCEPT_SEF_LU_REQUESTS
      /* Intercept SEF Live update requests. */
      if(IS_SEF_LU_REQUEST(m_ptr)) {
          if(do_sef_lu_request(m_ptr) == OK) {
              continue;
          }
      }
#endif

      /* If we get this far, this is not a valid SEF request, return and
       * let the caller deal with that.
       */
      break;
  }

  return r;
}

/*===========================================================================*
 *                         sef_debug_refresh_params              	     *
 *===========================================================================*/
PRIVATE void sef_debug_refresh_params(void)
{
/* Refresh SEF debug params. */
  clock_t uptime;
  endpoint_t endpoint;
  int r;

  /* Get boottime the first time. */
  if(!sef_debug_boottime) {
      r = sys_times(NONE, NULL, NULL, NULL, &sef_debug_boottime);
      if ( r != OK) {
          sef_debug_boottime = -1;
      }
  }

  /* Get system hz the first time. */
  if(!sef_debug_system_hz) {
      r = sys_getinfo(GET_HZ, &sef_debug_system_hz,
          sizeof(sef_debug_system_hz), 0, 0);
      if ( r != OK) {
          sef_debug_system_hz = -1;
      }
  }

  /* Get uptime. */
  uptime = -1;
  if(sef_debug_boottime!=-1 && sef_debug_system_hz!=-1) {
      r = sys_times(NONE, NULL, NULL, &uptime, NULL);
      if ( r != OK) {
            uptime = -1;
      }
  }

  /* Compute current time. */
  if(sef_debug_boottime==-1 || sef_debug_system_hz==-1 || uptime==-1) {
      sef_debug_time_sec = 0;
      sef_debug_time_us = 0;
  }
  else {
      sef_debug_time_sec = (time_t) (sef_debug_boottime
          + (uptime/sef_debug_system_hz));
      sef_debug_time_us = (uptime%sef_debug_system_hz)
          * 1000000/sef_debug_system_hz;
  }
}

/*===========================================================================*
 *                              sef_debug_header              		     *
 *===========================================================================*/
PUBLIC char* sef_debug_header(void)
{
/* Build and return a SEF debug header. */
  sef_debug_refresh_params();
  sprintf(sef_debug_header_buff, "%s: time = %ds %06dus", 
      sef_self_name, sef_debug_time_sec, sef_debug_time_us);

  return sef_debug_header_buff;
}

