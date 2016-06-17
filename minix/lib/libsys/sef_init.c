#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <machine/vmparam.h>

#include <minix/sysutil.h>

#include "syslib.h"
/* SEF Init callbacks. */
static struct sef_init_cbs {
    sef_cb_init_t                       sef_cb_init_fresh;
    sef_cb_init_t                       sef_cb_init_lu;
    sef_cb_init_t                       sef_cb_init_restart;
    sef_cb_init_response_t              sef_cb_init_response;
} sef_init_cbs = {
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
EXTERN endpoint_t sef_self_init_flags;
EXTERN int sef_controlled_crash;

#ifndef ST_STACK_REFS_BUFF_SIZE
#define ST_STACK_REFS_BUFF_SIZE           1024
#endif

/*===========================================================================*
 *                              process_init             		     *
 *===========================================================================*/
static int process_init(int type, sef_init_info_t *info)
{
/* Process initialization. */
  int r, result, debug_result_found, is_def_cb;
  cp_grant_id_t gid;
  message m;

  /* Debug. */
#if SEF_INIT_DEBUG
  sef_init_debug_begin();
  sef_init_dprint("%s. Got a SEF Init request of type %d, flags 0x%08x, rproctab_gid %d, ep %d, old ep %d, restarts %d. About to init.\n",
      sef_debug_header(), type, info->flags, info->rproctab_gid, info->endpoint, info->old_endpoint, info->restarts);
  sef_init_debug_end();
#endif

  /* Clear any IPC filter. */
  r = sys_statectl(SYS_STATE_CLEAR_IPC_FILTERS, 0, 0);
  assert(r == OK);

  /* Create grant for state transfer. */
  gid = cpf_grant_direct(sef_self_endpoint, 0, ULONG_MAX, CPF_READ);
  if(!GRANT_VALID(gid)) {
      panic("unable to create grant for state transfer");
  }
  if(gid != SEF_STATE_TRANSFER_GID) {
      panic("bad state transfer gid");
  }

  /* If debug init flags are allowed, process them first. */
  debug_result_found = 0;
  if(SEF_INIT_ALLOW_DEBUG_INIT_FLAGS) {
      int flags = info->flags;
      if(flags & SEF_INIT_CRASH) {
          result = sef_cb_init_crash(type, info);
          debug_result_found = 1;
      }
      else if(flags & SEF_INIT_FAIL) {
          result = sef_cb_init_fail(type, info);
          debug_result_found = 1;
      }
      else if(flags & SEF_INIT_TIMEOUT) {
          result = sef_cb_init_timeout(type, info);
          debug_result_found = 1;
      }
  }

  if(!debug_result_found) {
      /* Let the callback code handle the specific initialization type. */
      is_def_cb = info->flags & SEF_INIT_DEFCB;
      switch(type) {
          case SEF_INIT_FRESH:
              result = is_def_cb ? SEF_CB_INIT_FRESH_DEFAULT(type, info)
                                 : sef_init_cbs.sef_cb_init_fresh(type, info);
          break;
          case SEF_INIT_LU:
              result = is_def_cb ? SEF_CB_INIT_LU_DEFAULT(type, info)
                                 : sef_init_cbs.sef_cb_init_lu(type, info);
          break;
          case SEF_INIT_RESTART:
              result = is_def_cb ? SEF_CB_INIT_RESTART_DEFAULT(type, info)
                                 : sef_init_cbs.sef_cb_init_restart(type, info);
          break;

          default:
              /* Not a valid SEF init type. */
              result = EINVAL;
          break;
      }
  }

  memset(&m, 0, sizeof(m));
  m.m_source = sef_self_endpoint;
  m.m_type = RS_INIT;
  m.m_rs_init.result = result;
  r = sef_init_cbs.sef_cb_init_response(&m);
  if (r != OK) {
      return r;
  }

  /* See if we need to unmap the initialization buffer. */
  if(info->init_buff_cleanup_start) {
      void *addrstart = info->init_buff_cleanup_start;
      size_t len = info->init_buff_len - (size_t)((char*)info->init_buff_cleanup_start - (char*)info->init_buff_start);
      r = sef_munmap(addrstart, len, VM_MUNMAP);
      if(r != OK) {
          printf("process_init: warning: munmap failed for init buffer\n");
      }
  }

  /* Tell the kernel about the grant table. */
  cpf_reload();

  /* Tell the kernel about the senda table. */
  r = senda_reload();
  if(r != OK) {
      printf("process_init: warning: senda_reload failed\n");
  }

  /* Tell the kernel about the state table. */
  sys_statectl(SYS_STATE_SET_STATE_TABLE, sef_llvm_state_table_addr(), 0);

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
  memset(&info, 0, sizeof(info));

  /* Get init parameters from SEF. */
  type = SEF_INIT_FRESH;
  if(sef_self_priv_flags & LU_SYS_PROC) {
      type = SEF_INIT_LU;
  }
  else if(sef_self_priv_flags & RST_SYS_PROC) {
      type = SEF_INIT_RESTART;
  }
  info.flags = sef_self_init_flags;
  info.rproctab_gid = GRANT_INVALID;
  info.endpoint = sef_self_endpoint;
  info.old_endpoint = old_endpoint;
  info.restarts = 0;

  /* Get init buffer details from VM. */
  info.init_buff_start = NULL;
  info.init_buff_len = 0;
  if(type != SEF_INIT_FRESH) {
      r = vm_memctl(RS_PROC_NR, VM_RS_MEM_GET_PREALLOC_MAP,
          &info.init_buff_start, &info.init_buff_len);
      if(r != OK) {
          printf("do_sef_rs_init: vm_memctl failed\n");
      }
  }
  info.init_buff_cleanup_start = info.init_buff_start;

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
  memset(&info, 0, sizeof(info));

  /* Get init parameters from message. */
  type = m_ptr->m_rs_init.type;
  info.flags = m_ptr->m_rs_init.flags;
  info.rproctab_gid = m_ptr->m_rs_init.rproctab_gid;
  info.endpoint = sef_self_endpoint;
  info.old_endpoint = m_ptr->m_rs_init.old_endpoint;
  info.restarts = m_ptr->m_rs_init.restarts;
  info.init_buff_start = (void*) m_ptr->m_rs_init.buff_addr;
  info.init_buff_cleanup_start = info.init_buff_start;
  info.init_buff_len = m_ptr->m_rs_init.buff_len;
  info.prepare_state = m_ptr->m_rs_init.prepare_state;

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
  sef_init_cbs.sef_cb_init_fresh = cb;
}

/*===========================================================================*
 *                            sef_setcb_init_lu                              *
 *===========================================================================*/
void sef_setcb_init_lu(sef_cb_init_t cb)
{
  assert(cb != NULL);
  sef_init_cbs.sef_cb_init_lu = cb;
}

/*===========================================================================*
 *                         sef_setcb_init_restart                            *
 *===========================================================================*/
void sef_setcb_init_restart(sef_cb_init_t cb)
{
  assert(cb != NULL);
  sef_init_cbs.sef_cb_init_restart = cb;
}

/*===========================================================================*
 *                         sef_setcb_init_response                           *
 *===========================================================================*/
void sef_setcb_init_response(sef_cb_init_response_t cb)
{
  assert(cb != NULL);
  sef_init_cbs.sef_cb_init_response = cb;
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
  panic("Simulating a crash at initialization time...\n");

  return OK;
}

/*===========================================================================*
 *      	             sef_cb_init_timeout                             *
 *===========================================================================*/
int sef_cb_init_timeout(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
  message m;
  int status;

  printf("Simulating a timeout at initialization time...\n");

  ipc_receive(IDLE, &m, &status);

  return EBADCALL;
}

/*===========================================================================*
 *      	           sef_cb_init_restart_generic                       *
 *===========================================================================*/
int sef_cb_init_restart_generic(int type, sef_init_info_t *info)
{
  /* Always resort to simple identity transfer for self updates. */
  if (type == SEF_INIT_LU && (info->flags & SEF_LU_SELF))
      return sef_cb_init_identity_state_transfer(type, info);

  /* Can only handle restart otherwise. */
  if(type != SEF_INIT_RESTART) {
      printf("sef_cb_init_restart_generic: init failed\n");
      return ENOSYS;
  }

  /* Perform instrumentation-supported checkpoint-restart. */
  return sef_llvm_ltckpt_restart(type, info);
}

/*===========================================================================*
 *      	       sef_cb_init_identity_state_transfer                   *
 *===========================================================================*/
int sef_cb_init_identity_state_transfer(int type, sef_init_info_t *info)
{
  extern char *_brksize;
  extern char *_etext;
  int r;
  char *old_brksize, *new_brksize;
  char stack_buff[ST_STACK_REFS_BUFF_SIZE];
  vir_bytes data_start;
  size_t size;

  /* Identity state transfer is for crash recovery and self update only. */
  if(type != SEF_INIT_RESTART && (type != SEF_INIT_LU || !(info->flags & SEF_LU_SELF))) {
      printf("sef_cb_init_identity_state_transfer: state transfer failed\n");
      return ENOSYS;
  }

  /* Save stack refs. */
  sef_llvm_stack_refs_save(stack_buff);

  old_brksize = _brksize;
  data_start = (vir_bytes)&_etext;
#if SEF_ST_DEBUG
  printf("sef_cb_init_identity_state_transfer: _brksize = 0x%08x, _etext = 0x%08x, data_start = 0x%08x\n",
      _brksize, &_etext, data_start);
#endif

  /* Transfer data. */
  size = (size_t)(_brksize - data_start);

  r = sef_copy_state_region(info, data_start, size, data_start,
    TRUE /*may_have_holes*/);
  if (r != OK)
      return r;

  new_brksize = _brksize;

  /* Transfer heap if necessary. */
  if(sef_self_endpoint != VM_PROC_NR && old_brksize != new_brksize) {

#if SEF_ST_DEBUG
      printf("sef_cb_init_identity_state_transfer: brk() for new_brksize = 0x%08x\n",
          new_brksize);
#endif

      /* Extend heap first. */
      _brksize = old_brksize;
      r = sef_llvm_real_brk(new_brksize);
      if(r != OK) {
          printf("sef_cb_init_identity_state_transfer: brk failed\n");
          return EFAULT;
      }

      /* Transfer state on the heap. */
      assert(_brksize == new_brksize);
      size = (size_t)(_brksize - old_brksize);
      r = sef_copy_state_region(info, (vir_bytes) old_brksize, size,
          (vir_bytes) old_brksize, FALSE /*may_have_holes*/);
      if(r != OK) {
          printf("sef_cb_init_identity_state_transfer: extended heap transfer failed\n");
          return r;
      }
  }

  /* Restore stack refs. */
  sef_llvm_stack_refs_restore(stack_buff);

  if (sef_controlled_crash == FALSE) {
      printf("SEF(%d): crash was not controlled, "
	"aborting transparent restart\n", sef_self_endpoint);
      return EGENERIC; /* actual error code does not matter */
  }

  return OK;
}

/*===========================================================================*
 *      	      sef_cb_init_lu_identity_as_restart                     *
 *===========================================================================*/
int sef_cb_init_lu_identity_as_restart(int type, sef_init_info_t *info)
{
  /* Can only handle live update. */
  if(type != SEF_INIT_LU) {
      printf("sef_cb_init_lu_identity_as_restart: init failed\n");
      return ENOSYS;
  }

  /* Resort to restart callback only for identity updates, ignore other cases. */
  if(SEF_LU_IS_IDENTITY_UPDATE(info->flags)) {
      if((info->flags & (SEF_INIT_DEFCB|SEF_INIT_SCRIPT_RESTART))
          || sef_init_cbs.sef_cb_init_restart == sef_cb_init_reset) {
          /* Use stateful restart callback when necessary. */
          return SEF_CB_INIT_RESTART_STATEFUL(type, info);
      }
      return sef_init_cbs.sef_cb_init_restart(type, info);
  }

  return ENOSYS;
}

/*===========================================================================*
 *      	             sef_cb_init_lu_generic                          *
 *===========================================================================*/
int sef_cb_init_lu_generic(int type, sef_init_info_t *info)
{
  /* Can only handle live update. */
  if(type != SEF_INIT_LU) {
      printf("sef_cb_init_lu_generic: init failed\n");
      return ENOSYS;
  }

  /* Resort to restart callback for identity updates. */
  if(SEF_LU_IS_IDENTITY_UPDATE(info->flags)) {
      return sef_cb_init_lu_identity_as_restart(type, info); 
  }

  /* Perform state transfer updates in all the other cases. */
  return sef_st_state_transfer(info);
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

/*===========================================================================*
 *                       sef_cb_init_response_rs_asyn_once		     *
 *===========================================================================*/
int sef_cb_init_response_rs_asyn_once(message *m_ptr)
{
/* This response function is used by VM to avoid a boot-time deadlock. */
  int r;

  /* Inform RS that we completed initialization, asynchronously. */
  r = asynsend3(RS_PROC_NR, m_ptr, AMF_NOREPLY);

  /* Use a blocking reply call next time. */
  sef_setcb_init_response(SEF_CB_INIT_RESPONSE_DEFAULT);

  return r;
}
