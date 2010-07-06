#include "syslib.h"
#include <assert.h>
#include <unistd.h>
#include <minix/sysutil.h>

/* SEF Init callbacks. */
PRIVATE struct sef_cbs {
    sef_cb_init_t                       sef_cb_init_fresh;
    sef_cb_init_t                       sef_cb_init_lu;
    sef_cb_init_t                       sef_cb_init_restart;
} sef_cbs = {
    SEF_CB_INIT_FRESH_DEFAULT,
    SEF_CB_INIT_LU_DEFAULT,
    SEF_CB_INIT_RESTART_DEFAULT
};

/* SEF Init prototypes for sef_startup(). */
PUBLIC _PROTOTYPE( int do_sef_rs_init, (endpoint_t old_endpoint) );
PUBLIC _PROTOTYPE( int do_sef_init_request, (message *m_ptr) );

/* Debug. */
EXTERN _PROTOTYPE( char* sef_debug_header, (void) );

/* Information about SELF. */
EXTERN endpoint_t sef_self_endpoint;
EXTERN endpoint_t sef_self_priv_flags;

/*===========================================================================*
 *                              process_init             		     *
 *===========================================================================*/
PRIVATE int process_init(int type, sef_init_info_t *info)
{
/* Process initialization. */
  int r;

  /* Debug. */
#if SEF_INIT_DEBUG
  sef_init_debug_begin();
  sef_init_dprint("%s. Got a SEF Init request of type: %d. About to init.\n",
      sef_debug_header(), type);
  sef_init_debug_end();
#endif

  /* Let the callback code handle the specific initialization type. */
  switch(type) {
      case SEF_INIT_FRESH:
          r = sef_cbs.sef_cb_init_fresh(type, info);
      break;
      case SEF_INIT_LU:
          r = sef_cbs.sef_cb_init_lu(type, info);
      break;
      case SEF_INIT_RESTART:
          r = sef_cbs.sef_cb_init_restart(type, info);
      break;

      default:
          /* Not a valid SEF init type. */
          r = EINVAL;
      break;
  }

  return r;
}

/*===========================================================================*
 *                              do_sef_rs_init             		     *
 *===========================================================================*/
PUBLIC int do_sef_rs_init(endpoint_t old_endpoint)
{
/* Special SEF Init for RS. */
  int r;
  int type;
  sef_init_info_t info;

  /* Get init parameters from SEF. */
  type = SEF_INIT_FRESH;
  if(sef_self_priv_flags & LU_SYS_PROC) {
      type = SEF_INIT_LU;
  }
  else if(sef_self_priv_flags & RST_SYS_PROC) {
      type = SEF_INIT_RESTART;
  }
  info.rproctab_gid = -1;
  info.endpoint = sef_self_endpoint;
  info.old_endpoint = old_endpoint;

  /* Peform initialization. */
  r = process_init(type, &info);

  return r;
}

/*===========================================================================*
 *                            do_sef_init_request             		     *
 *===========================================================================*/
PUBLIC int do_sef_init_request(message *m_ptr)
{
/* Handle a SEF Init request. */
  int r;
  int type;
  sef_init_info_t info;

  /* Get init parameters from message. */
  type = m_ptr->RS_INIT_TYPE;
  info.rproctab_gid = m_ptr->RS_INIT_RPROCTAB_GID;
  info.endpoint = sef_self_endpoint;
  info.old_endpoint = m_ptr->RS_INIT_OLD_ENDPOINT;

  /* Peform initialization. */
  r = process_init(type, &info);

  /* Report back to RS. */
  m_ptr->RS_INIT_RESULT = r;
  r = sendrec(RS_PROC_NR, m_ptr);

  return r;
}

/*===========================================================================*
 *                         sef_setcb_init_fresh                              *
 *===========================================================================*/
PUBLIC void sef_setcb_init_fresh(sef_cb_init_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_init_fresh = cb;
}

/*===========================================================================*
 *                            sef_setcb_init_lu                              *
 *===========================================================================*/
PUBLIC void sef_setcb_init_lu(sef_cb_init_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_init_lu = cb;
}

/*===========================================================================*
 *                         sef_setcb_init_restart                            *
 *===========================================================================*/
PUBLIC void sef_setcb_init_restart(sef_cb_init_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_init_restart = cb;
}

/*===========================================================================*
 *      	              sef_cb_init_null                               *
 *===========================================================================*/
PUBLIC int sef_cb_init_null(int UNUSED(type),
   sef_init_info_t *UNUSED(info))
{
  return OK;
}

/*===========================================================================*
 *      	              sef_cb_init_fail                               *
 *===========================================================================*/
PUBLIC int sef_cb_init_fail(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
  return ENOSYS;
}

/*===========================================================================*
 *      	              sef_cb_init_crash                              *
 *===========================================================================*/
PUBLIC int sef_cb_init_crash(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
  panic("Simulating a crash at initialization time...");

  return OK;
}

