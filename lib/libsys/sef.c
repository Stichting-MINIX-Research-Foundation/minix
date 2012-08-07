#include "syslib.h"
#include <assert.h>
#include <minix/sysutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* Self variables. */
#define SEF_SELF_NAME_MAXLEN 20
char sef_self_name[SEF_SELF_NAME_MAXLEN];
endpoint_t sef_self_endpoint;
int sef_self_priv_flags;
int sef_self_first_receive_done;
int sef_self_receiving;

/* Debug. */
#if SEF_INIT_DEBUG || SEF_LU_DEBUG || SEF_PING_DEBUG || SEF_SIGNAL_DEBUG
#define SEF_DEBUG_HEADER_MAXLEN 32
static time_t sef_debug_boottime = 0;
static u32_t sef_debug_system_hz = 0;
static time_t sef_debug_time_sec = 0;
static time_t sef_debug_time_us = 0;
static char sef_debug_header_buff[SEF_DEBUG_HEADER_MAXLEN];
static void sef_debug_refresh_params(void);
char* sef_debug_header(void);
#endif

/* SEF Init prototypes. */
#ifdef USE_COVERAGE
EXTERN int do_sef_gcov_request(message *m_ptr);
#endif
EXTERN int do_sef_rs_init(endpoint_t old_endpoint);
EXTERN int do_sef_init_request(message *m_ptr);

/* SEF Ping prototypes. */
EXTERN int do_sef_ping_request(message *m_ptr);

/* SEF Live update prototypes. */
EXTERN void do_sef_lu_before_receive(void);
EXTERN int do_sef_lu_request(message *m_ptr);

/* SEF Signal prototypes. */
EXTERN int do_sef_signal_request(message *m_ptr);

/*===========================================================================*
 *				sef_startup				     *
 *===========================================================================*/
void sef_startup()
{
/* SEF startup interface for system services. */
  int r, status;
  endpoint_t old_endpoint;
  int priv_flags;

  /* Get information about self. */
  r = sys_whoami(&sef_self_endpoint, sef_self_name, SEF_SELF_NAME_MAXLEN,
      &priv_flags);
  if ( r != OK) {
      sef_self_endpoint = SELF;
      strlcpy(sef_self_name, "Unknown", sizeof(sef_self_name));
  }
  sef_self_priv_flags = priv_flags;
  old_endpoint = NONE;

#if USE_LIVEUPDATE
  /* RS may wake up with the wrong endpoint, perfom the update in that case. */
  if((sef_self_priv_flags & ROOT_SYS_PROC) && sef_self_endpoint != RS_PROC_NR) {
      r = vm_update(RS_PROC_NR, sef_self_endpoint);
      if(r != OK) {
          panic("unable to update RS from instance %d to %d",
              RS_PROC_NR, sef_self_endpoint);
      }
      old_endpoint = sef_self_endpoint;
      sef_self_endpoint = RS_PROC_NR;
  }
#endif /* USE_LIVEUPDATE */

#if INTERCEPT_SEF_INIT_REQUESTS
  /* Intercept SEF Init requests. */
  if(sef_self_priv_flags & ROOT_SYS_PROC) {
      /* RS initialization is special. */
      if((r = do_sef_rs_init(old_endpoint)) != OK) {
          panic("RS unable to complete init: %d", r);
      }
  }
  else if(sef_self_endpoint == VM_PROC_NR) {
  	/* VM handles initialization by RS later */
  } else {
      message m;

      /* Wait for an initialization message from RS. We need this to learn the
       * initialization type and parameters. When restarting after a crash, we
       * may get some spurious IPC messages from RS (e.g. update request) that
       * were originally meant to be delivered to the old instance. We discard
       * these messages and block till a proper initialization request arrives.
       */
      do {
          r = receive(RS_PROC_NR, &m, &status);
          if(r != OK) {
              panic("unable to receive from RS: %d", r);
          }
      } while(!IS_SEF_INIT_REQUEST(&m));

      /* Process initialization request for this system service. */
      if((r = do_sef_init_request(&m)) != OK) {
          panic("unable to process init request: %d", r);
      }
  }
#endif

  /* (Re)initialize SEF variables. */
  sef_self_first_receive_done = FALSE;
  sef_self_priv_flags = priv_flags;
}

/*===========================================================================*
 *				sef_receive_status			     *
 *===========================================================================*/
int sef_receive_status(endpoint_t src, message *m_ptr, int *status_ptr)
{
/* SEF receive() interface for system services. */
  int r, status;

  sef_self_receiving = TRUE;

  while(TRUE) {
      /* If the caller indicated that it no longer wants to receive a message,
       * return now.
       */
      if (!sef_self_receiving)
          return EINTR;

#if INTERCEPT_SEF_LU_REQUESTS
      /* Handle SEF Live update before receive events. */
      do_sef_lu_before_receive();
#endif

      /* Receive and return in case of error. */
      r = receive(src, m_ptr, &status);
      if(status_ptr) *status_ptr = status;
      if(!sef_self_first_receive_done) sef_self_first_receive_done = TRUE;
      if(r != OK) {
          return r;
      }

#if INTERCEPT_SEF_PING_REQUESTS
      /* Intercept SEF Ping requests. */
      if(IS_SEF_PING_REQUEST(m_ptr, status)) {
          if(do_sef_ping_request(m_ptr) == OK) {
              continue;
          }
      }
#endif

#if INTERCEPT_SEF_LU_REQUESTS
      /* Intercept SEF Live update requests. */
      if(IS_SEF_LU_REQUEST(m_ptr, status)) {
          if(do_sef_lu_request(m_ptr) == OK) {
              continue;
          }
      }
#endif

#if INTERCEPT_SEF_SIGNAL_REQUESTS
      /* Intercept SEF Signal requests. */
      if(IS_SEF_SIGNAL_REQUEST(m_ptr, status)) {
          if(do_sef_signal_request(m_ptr) == OK) {
              continue;
          }
      }
#endif

#ifdef USE_COVERAGE
      /* Intercept GCOV data requests (sent by VFS in vfs/gcov.c). */
      if(m_ptr->m_type == COMMON_REQ_GCOV_DATA &&
	 m_ptr->m_source == VFS_PROC_NR) {
          if(do_sef_gcov_request(m_ptr) == OK) {
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
 *				sef_cancel				     *
 *===========================================================================*/
void sef_cancel(void)
{
/* Cancel receiving a message. This function be called from a callback invoked
 * from within sef_receive_status(), which will then return an EINTR error
 * code. In particular, this function can be used to exit from the main receive
 * loop when a signal handler causes the process to want to shut down.
 */

  sef_self_receiving = FALSE;
}

/*===========================================================================*
 *      	                  sef_exit                                   *
 *===========================================================================*/
void sef_exit(int status)
{
/* System services use a special version of exit() that generates a
 * self-termination signal.
 */
  message m;

  /* Ask the kernel to exit. */
  sys_exit();

  /* If sys_exit() fails, this is not a system service. Exit through PM. */
  m.m1_i1 = status;
  _syscall(PM_PROC_NR, EXIT, &m);

  /* If everything else fails, hang. */
  printf("Warning: system service %d couldn't exit\n", sef_self_endpoint);
  for(;;) { }
}

/*===========================================================================*
 *      	                     _exit                                   *
 *===========================================================================*/
void _exit(int status)
{
/* Make exit() an alias for sef_exit() for system services. */
  sef_exit(status);
  panic("sef_exit failed");
}

/*===========================================================================*
 *      	                    __exit                                   *
 *===========================================================================*/
void __exit(int status)
{
/* Make exit() an alias for sef_exit() for system services. */
  sef_exit(status);
  panic("sef_exit failed");
}

#if SEF_INIT_DEBUG || SEF_LU_DEBUG || SEF_PING_DEBUG || SEF_SIGNAL_DEBUG
/*===========================================================================*
 *                         sef_debug_refresh_params              	     *
 *===========================================================================*/
static void sef_debug_refresh_params(void)
{
/* Refresh SEF debug params. */
  clock_t uptime;
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
char* sef_debug_header(void)
{
/* Build and return a SEF debug header. */
  sef_debug_refresh_params();
  snprintf(sef_debug_header_buff, sizeof(sef_debug_header_buff),
      "%s: time = %ds %06dus", sef_self_name, (int) sef_debug_time_sec,
      (int) sef_debug_time_us);

  return sef_debug_header_buff;
}
#endif /*SEF_INIT_DEBUG || SEF_LU_DEBUG || SEF_PING_DEBUG || SEF_SIGNAL_DEBUG*/

