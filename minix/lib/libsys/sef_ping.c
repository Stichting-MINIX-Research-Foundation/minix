#include "syslib.h"
#include <assert.h>
#include <minix/sysutil.h>

/* SEF Ping callbacks. */
static struct sef_cbs {
    sef_cb_ping_reply_t                 sef_cb_ping_reply;
} sef_cbs = {
    SEF_CB_PING_REPLY_DEFAULT
};

/* SEF Ping prototypes for sef_receive(). */
int do_sef_ping_request(message *m_ptr);

/* Debug. */
EXTERN char* sef_debug_header(void);

/*===========================================================================*
 *                            do_sef_ping_request             		     *
 *===========================================================================*/
int do_sef_ping_request(message *m_ptr)
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
  sef_cbs.sef_cb_ping_reply(m_ptr->m_source);

  /* Return OK not to let anybody else intercept the request. */
  return(OK);
}

/*===========================================================================*
 *                          sef_setcb_ping_reply                             *
 *===========================================================================*/
void sef_setcb_ping_reply(sef_cb_ping_reply_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_ping_reply = cb;
}

/*===========================================================================*
 *      	           sef_cb_ping_reply_null                            *
 *===========================================================================*/
void sef_cb_ping_reply_null(endpoint_t UNUSED(source))
{
}

/*===========================================================================*
 *      	           sef_cb_ping_reply_pong                            *
 *===========================================================================*/
void sef_cb_ping_reply_pong(endpoint_t source)
{
  ipc_notify(source);
}

