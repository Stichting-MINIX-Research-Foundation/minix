#include "syslib.h"
#include <assert.h>
#include <unistd.h>
#include <minix/sysutil.h>
#include <string.h>

/* SEF Init callbacks. */
static struct sef_cbs {
    sef_cb_init_t                       sef_cb_init_fresh;
    sef_cb_init_t                       sef_cb_init_lu;
    sef_cb_init_t                       sef_cb_init_restart;
    sef_cb_init_response_t              sef_cb_init_response;
} sef_cbs = {
    SEF_CB_INIT_FRESH_DEFAULT,
    SEF_CB_INIT_LU_DEFAULT,
    SEF_CB_INIT_RESTART_DEFAULT,
    SEF_CB_INIT_RESPONSE_DEFAULT
};

/* SEF Init prototypes for sef_startup(). */
int do_sef_rs_init(endpoint_t old_endpoint);
int do_sef_init_request(message *m_ptr);

/* Debug. */
EXTERN char* sef_debug_header(void);

/* Information about SELF. */
EXTERN endpoint_t sef_self_endpoint;
EXTERN endpoint_t sef_self_priv_flags;

/*===========================================================================*
 *                              process_init             		     *
 *===========================================================================*/
static int process_init(int type, sef_init_info_t *info)
{
/* Process initialization. */
  int r, result;
  message m;

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
          result = sef_cbs.sef_cb_init_fresh(type, info);
      break;
      case SEF_INIT_LU:
          result = sef_cbs.sef_cb_init_lu(type, info);
      break;
      case SEF_INIT_RESTART:
          result = sef_cbs.sef_cb_init_restart(type, info);
      break;

      default:
          /* Not a valid SEF init type. */
          result = EINVAL;
      break;
  }

  memset(&m, 0, sizeof(m));
  m.m_source = sef_self_endpoint;
  m.m_type = RS_INIT;
  m.m_rs_init.result = result;
  r = sef_cbs.sef_cb_init_response(&m);

  return r;
}

/*===========================================================================*
 *                              do_sef_rs_init             		     *
 *===========================================================================*/
int do_sef_rs_init(endpoint_t old_endpoint)
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
int do_sef_init_request(message *m_ptr)
{
/* Handle a SEF Init request. */
  int r;
  int type;
  sef_init_info_t info;

  /* Get init parameters from message. */
  type = m_ptr->m_rs_init.type;
  info.rproctab_gid = m_ptr->m_rs_init.rproctab_gid;
  info.endpoint = sef_self_endpoint;
  info.old_endpoint = m_ptr->m_rs_init.old_endpoint;

  /* Peform initialization. */
  r = process_init(type, &info);

  return r;
}

/*===========================================================================*
 *                         sef_setcb_init_fresh                              *
 *===========================================================================*/
void sef_setcb_init_fresh(sef_cb_init_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_init_fresh = cb;
}

/*===========================================================================*
 *                            sef_setcb_init_lu                              *
 *===========================================================================*/
void sef_setcb_init_lu(sef_cb_init_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_init_lu = cb;
}

/*===========================================================================*
 *                         sef_setcb_init_restart                            *
 *===========================================================================*/
void sef_setcb_init_restart(sef_cb_init_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_init_restart = cb;
}

/*===========================================================================*
 *                         sef_setcb_init_response                           *
 *===========================================================================*/
void sef_setcb_init_response(sef_cb_init_response_t cb)
{
  assert(cb != NULL);
  sef_cbs.sef_cb_init_response = cb;
}

/*===========================================================================*
 *      	              sef_cb_init_null                               *
 *===========================================================================*/
int sef_cb_init_null(int UNUSED(type),
   sef_init_info_t *UNUSED(info))
{
  return OK;
}

/*===========================================================================*
 *                        sef_cb_init_response_null        		     *
 *===========================================================================*/
int sef_cb_init_response_null(message * UNUSED(m_ptr))
{
  return ENOSYS;
}

/*===========================================================================*
 *      	              sef_cb_init_fail                               *
 *===========================================================================*/
int sef_cb_init_fail(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
  return ENOSYS;
}

/*===========================================================================*
 *      	              sef_cb_init_reset                              *
 *===========================================================================*/
int sef_cb_init_reset(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
  /* Tell RS to reincarnate us, with no old resources, and a new endpoint. */
  return ERESTART;
}

/*===========================================================================*
 *      	              sef_cb_init_crash                              *
 *===========================================================================*/
int sef_cb_init_crash(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
  panic("Simulating a crash at initialization time...");

  return OK;
}

/*===========================================================================*
 *                       sef_cb_init_response_rs_reply        		     *
 *===========================================================================*/
int sef_cb_init_response_rs_reply(message *m_ptr)
{
  int r;

  /* Inform RS that we completed initialization with the given result. */
  r = ipc_sendrec(RS_PROC_NR, m_ptr);

  return r;
}

