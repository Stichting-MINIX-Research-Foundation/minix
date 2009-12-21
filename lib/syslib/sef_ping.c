#include "syslib.h"
#include <assert.h>
#include <minix/sysutil.h>

/* SEF Ping callbacks. */
PRIVATE struct sef_cbs {
    sef_cb_ping_reply_t                 sef_cb_ping_reply;
} sef_cbs = {
    SEF_CB_PING_REPLY_DEFAULT
};

/* SEF Ping prototypes for sef_receive(). */
PUBLIC _PROTOTYPE( int do_sef_ping_request, (message *m_ptr) );

/* Debug. */
EXTERN _PROTOTYPE( char* sef_debug_header, (void) );

/*===========================================================================*
 *                            do_sef_ping_request             		     *
 *===========================================================================*/
PUBLIC int do_sef_ping_request(message *m_ptr)
{
/* Handle a SEF Ping request. */

  /* Debug. */
#if SEF_PING_DEBUG
  sef_ping_debug_begin();
  sef_ping_dprint("%s. Got a SEF Ping request! About to reply.\n", 
      sef_debug_header());
  sef_ping_debug_end();
#endif

  /* Let the callback code handle the request. */
  sef_cbs.sef_cb_ping_reply(m_ptr);

  /* Return OK not to let anybody else intercept the request. */
  return(OK);
}

/*===========================================================================*
 *                          sef_setcb_ping_reply                             *
 *===========================================================================*/
PUBLIC void sef_setcb_ping_reply(sef_cb_ping_reply_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_ping_reply = cb;
}

/*===========================================================================*
 *      	           sef_cb_ping_reply_null                            *
 *===========================================================================*/
PUBLIC void sef_cb_ping_reply_null(message *m_ptr)
{
}

/*===========================================================================*
 *      	           sef_cb_ping_reply_pong                            *
 *===========================================================================*/
PUBLIC void sef_cb_ping_reply_pong(message *m_ptr)
{
  notify(m_ptr->m_source);
}

