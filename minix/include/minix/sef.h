/* Prototypes for System Event Framework (SEF) functions. */

#ifndef _SEF_H
#define _SEF_H

#include <minix/ipc.h>

/* SEF entry points for system processes. */
void sef_startup(void);
int sef_receive_status(endpoint_t src, message *m_ptr, int *status_ptr);
endpoint_t sef_self(void);
void sef_cancel(void);
void __dead sef_exit(int status);
int sef_getrndseed (void);
int sef_munmap(void *addrstart, vir_bytes len, int type);
#define sef_receive(src, m_ptr) sef_receive_status(src, m_ptr, NULL)

/* SEF global definitions. */
#define SEF_STATE_TRANSFER_GID          0

/* SEF Debug. */
#include <stdio.h>
#define sef_dprint                      printf
#define sef_debug_begin()               (void)(NULL)
#define sef_debug_end()                 (void)(NULL)

/*===========================================================================*
 *				  SEF Init				     *
 *===========================================================================*/
/* What to intercept. */
#define INTERCEPT_SEF_INIT_REQUESTS 1
#define SEF_INIT_REQUEST_TYPE RS_INIT
#define IS_SEF_INIT_REQUEST(mp, status) ((mp)->m_type == RS_INIT \
     && (mp)->m_source == RS_PROC_NR)

#define SEF_COPY_OLD_TO_NEW     0x001
#define SEF_COPY_NEW_TO_NEW     0x002
#define SEF_COPY_DEST_OFFSET    0x004
#define SEF_COPY_SRC_OFFSET     0X008

/* Type definitions. */
typedef struct {
    int flags;
    cp_grant_id_t rproctab_gid;
    endpoint_t endpoint;
    endpoint_t old_endpoint;
    int restarts;
    void* init_buff_start;
    void* init_buff_cleanup_start;
    size_t init_buff_len;
    int copy_flags;
    int prepare_state;
} sef_init_info_t;

/* Callback type definitions. */
typedef int(*sef_cb_init_t)(int type, sef_init_info_t *info);
typedef int(*sef_cb_init_response_t)(message *m_ptr);

/* Callback registration helpers. */
void sef_setcb_init_fresh(sef_cb_init_t cb);
void sef_setcb_init_lu(sef_cb_init_t cb);
void sef_setcb_init_restart(sef_cb_init_t cb);
void sef_setcb_init_response(sef_cb_init_response_t cb);

/* Predefined callback implementations. */
int sef_cb_init_null(int type, sef_init_info_t *info);
int sef_cb_init_response_null(message *m_ptr);

int sef_cb_init_fail(int type, sef_init_info_t *info);
int sef_cb_init_reset(int type, sef_init_info_t *info);
int sef_cb_init_crash(int type, sef_init_info_t *info);
int sef_cb_init_timeout(int type, sef_init_info_t *info);
int sef_cb_init_restart_generic(int type, sef_init_info_t *info);
int sef_cb_init_identity_state_transfer(int type, sef_init_info_t *info);
int sef_cb_init_lu_identity_as_restart(int type, sef_init_info_t *info);
int sef_cb_init_lu_generic(int type, sef_init_info_t *info);
int sef_cb_init_response_rs_reply(message *m_ptr);
int sef_cb_init_response_rs_asyn_once(message *m_ptr);

/* Macros for predefined callback implementations. */
#define SEF_CB_INIT_FRESH_NULL          sef_cb_init_null
#define SEF_CB_INIT_LU_NULL             sef_cb_init_null
#define SEF_CB_INIT_RESTART_NULL        sef_cb_init_null
#define SEF_CB_INIT_RESPONSE_NULL       sef_cb_init_response_null
#define SEF_CB_INIT_RESTART_STATEFUL    sef_cb_init_restart_generic

#define SEF_CB_INIT_FRESH_DEFAULT       sef_cb_init_null
#define SEF_CB_INIT_LU_DEFAULT          sef_cb_init_lu_generic
#define SEF_CB_INIT_RESTART_DEFAULT     sef_cb_init_reset
#define SEF_CB_INIT_RESPONSE_DEFAULT    sef_cb_init_response_rs_reply

/* Init types. */
#define SEF_INIT_FRESH                  0    /* init fresh */
#define SEF_INIT_LU                     1    /* init after live update */
#define SEF_INIT_RESTART                2    /* init after restart */

/* Init flags (live update flags can be used as init flags as well). */
#define SEF_INIT_CRASH	 	      0x1
#define SEF_INIT_FAIL		      0x2
#define SEF_INIT_TIMEOUT	      0x4
#define SEF_INIT_DEFCB  	      0x8
#define SEF_INIT_SCRIPT_RESTART	     0x10
#define SEF_INIT_ST                  0x20    /* force state transfer init */

/* Debug. */
#define SEF_INIT_DEBUG_DEFAULT 		0
#define SEF_INIT_ALLOW_DEBUG_INIT_FLAGS 1

#ifndef SEF_INIT_DEBUG
#define SEF_INIT_DEBUG                  SEF_INIT_DEBUG_DEFAULT
#endif

#define sef_init_dprint                 sef_dprint
#define sef_init_debug_begin            sef_debug_begin
#define sef_init_debug_end              sef_debug_end

/*===========================================================================*
 *				  SEF Ping				     *
 *===========================================================================*/
/* What to intercept. */
#define INTERCEPT_SEF_PING_REQUESTS 1
#define SEF_PING_REQUEST_TYPE NOTIFY_MESSAGE
#define IS_SEF_PING_REQUEST(mp, status) (is_ipc_notify(status) \
    && (mp)->m_source == RS_PROC_NR)

/* Callback type definitions. */
typedef void(*sef_cb_ping_reply_t)(endpoint_t source);

/* Callback registration helpers. */
void sef_setcb_ping_reply(sef_cb_ping_reply_t cb);

/* Predefined callback implementations. */
void sef_cb_ping_reply_null(endpoint_t source);

void sef_cb_ping_reply_pong(endpoint_t source);

/* Macros for predefined callback implementations. */
#define SEF_CB_PING_REPLY_NULL          sef_cb_ping_reply_null

#define SEF_CB_PING_REPLY_DEFAULT       sef_cb_ping_reply_pong

/* Debug. */
#define SEF_PING_DEBUG_DEFAULT 0

#ifndef SEF_PING_DEBUG
#define SEF_PING_DEBUG                  SEF_PING_DEBUG_DEFAULT
#endif

#define sef_ping_dprint                 sef_dprint
#define sef_ping_debug_begin            sef_debug_begin
#define sef_ping_debug_end              sef_debug_end

/*===========================================================================*
 *				SEF Live update				     *
 *===========================================================================*/
/* What to intercept. */
#define INTERCEPT_SEF_LU_REQUESTS 1
#define SEF_LU_REQUEST_TYPE RS_LU_PREPARE
#define IS_SEF_LU_REQUEST(mp, status) ((mp)->m_type == RS_LU_PREPARE \
    && (mp)->m_source == RS_PROC_NR)

/* Callback type definitions. */
typedef  int(*sef_cb_lu_prepare_t)(int);
typedef  int(*sef_cb_lu_state_isvalid_t)(int, int);
typedef void(*sef_cb_lu_state_changed_t)(int, int);
typedef void(*sef_cb_lu_state_dump_t)(int);
typedef  int(*sef_cb_lu_state_save_t)(int, int);
typedef  int(*sef_cb_lu_response_t)(message *m_ptr);

/* Callback registration helpers. */
void sef_setcb_lu_prepare(sef_cb_lu_prepare_t cb);
void sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_t cb);
void sef_setcb_lu_state_changed(sef_cb_lu_state_changed_t cb);
void sef_setcb_lu_state_dump(sef_cb_lu_state_dump_t cb);
void sef_setcb_lu_state_save(sef_cb_lu_state_save_t cb);
void sef_setcb_lu_response(sef_cb_lu_response_t cb);

/* Predefined callback implementations. */
int sef_cb_lu_prepare_null(int state);
int sef_cb_lu_state_isvalid_null(int state, int flags);
void sef_cb_lu_state_changed_null(int old_state, int state);
void sef_cb_lu_state_dump_null(int state);
int sef_cb_lu_state_save_null(int state, int flags);
int sef_cb_lu_response_null(message *m_ptr);

int sef_cb_lu_prepare_always_ready(int state);
int sef_cb_lu_prepare_never_ready(int state);
int sef_cb_lu_prepare_crash(int state);
int sef_cb_lu_prepare_eval(int state);
int sef_cb_lu_state_isvalid_standard(int state, int flags);
int sef_cb_lu_state_isvalid_workfree(int state, int flags);
int sef_cb_lu_state_isvalid_workfree_self(int state, int flags);
int sef_cb_lu_state_isvalid_generic(int state, int flags);
void sef_cb_lu_state_dump_eval(int state);
int sef_cb_lu_response_rs_reply(message *m_ptr);

/* Macros for predefined callback implementations. */
#define SEF_CB_LU_PREPARE_NULL          sef_cb_lu_prepare_null
#define SEF_CB_LU_STATE_ISVALID_NULL    sef_cb_lu_state_isvalid_null
#define SEF_CB_LU_STATE_CHANGED_NULL    sef_cb_lu_state_changed_null
#define SEF_CB_LU_STATE_DUMP_NULL       sef_cb_lu_state_dump_null
#define SEF_CB_LU_STATE_SAVE_NULL       sef_cb_lu_state_save_null
#define SEF_CB_LU_RESPONSE_NULL         sef_cb_lu_response_null

#define SEF_CB_LU_PREPARE_DEFAULT       sef_cb_lu_prepare_null
#define SEF_CB_LU_STATE_ISVALID_DEFAULT sef_cb_lu_state_isvalid_generic
#define SEF_CB_LU_STATE_CHANGED_DEFAULT sef_cb_lu_state_changed_null
#define SEF_CB_LU_STATE_DUMP_DEFAULT    sef_cb_lu_state_dump_eval
#define SEF_CB_LU_STATE_SAVE_DEFAULT    sef_cb_lu_state_save_null
#define SEF_CB_LU_RESPONSE_DEFAULT      sef_cb_lu_response_rs_reply

/* Standard live update states. */
#define SEF_LU_STATE_NULL               0    /* null state */
#define SEF_LU_STATE_WORK_FREE          1    /* no work in progress */
#define SEF_LU_STATE_REQUEST_FREE       2    /* no request in progress */
#define SEF_LU_STATE_PROTOCOL_FREE      3    /* no protocol in progress */
#define SEF_LU_STATE_EVAL               4    /* evaluate expression */

#define SEF_LU_STATE_UNREACHABLE        5    /* unreachable state */
#define SEF_LU_STATE_PREPARE_CRASH      6    /* crash at prepare time */

#define SEF_LU_STATE_STD_BASE           (SEF_LU_STATE_WORK_FREE)
#define SEF_LU_STATE_DEBUG_BASE         (SEF_LU_STATE_UNREACHABLE)
#define SEF_LU_STATE_CUSTOM_BASE        (SEF_LU_STATE_PREPARE_CRASH+1)

#define SEF_LU_STATE_IS_STANDARD(s)     ((s) >= SEF_LU_STATE_STD_BASE \
    && (s) < SEF_LU_STATE_CUSTOM_BASE \
    && (SEF_LU_ALWAYS_ALLOW_DEBUG_STATES || !SEF_LU_STATE_IS_DEBUG(s)))
#define SEF_LU_STATE_IS_DEBUG(s)     ((s) >= SEF_LU_STATE_DEBUG_BASE \
    && (s) < SEF_LU_STATE_CUSTOM_BASE)

#define SEF_LU_STATE_EVAL_MAX_LEN          512

/* Live update flags (can be used as init flags as well). */
#define SEF_LU_SELF          	      0x0100    /* this is a self update */
#define SEF_LU_ASR           	      0x0200    /* this is an ASR update */
#define SEF_LU_MULTI         	      0x0400    /* this is a multi-component update */
#define SEF_LU_INCLUDES_VM     	      0x0800    /* the update includes VM */
#define SEF_LU_INCLUDES_RS     	      0x1000    /* the update includes RS */
#define SEF_LU_PREPARE_ONLY           0x2000    /* prepare only, no actual update taking place */
#define SEF_LU_NOMMAP	      	      0x4000    /* update doesn't inherit mmapped regions */
#define SEF_LU_DETACHED      	      0x8000    /* update detaches the old instance */

#define SEF_LU_IS_IDENTITY_UPDATE(F) (((F) & (SEF_LU_SELF|SEF_LU_NOMMAP|SEF_LU_ASR|SEF_INIT_ST)) == SEF_LU_SELF)

/* Debug. */
#define SEF_LU_DEBUG_DEFAULT             0
#define SEF_LU_ALWAYS_ALLOW_DEBUG_STATES 1

#ifndef SEF_LU_DEBUG
#define SEF_LU_DEBUG            SEF_LU_DEBUG_DEFAULT
#endif

#define sef_lu_dprint           sef_dprint
#define sef_lu_debug_begin      sef_debug_begin
#define sef_lu_debug_end        sef_debug_end

/*===========================================================================*
 *				  SEF Signal				     *
 *===========================================================================*/
/* What to intercept. */
#define INTERCEPT_SEF_SIGNAL_REQUESTS 1
#define SEF_SIGNAL_REQUEST_TYPE SIGS_SIGNAL_RECEIVED
#define IS_SEF_SIGNAL_REQUEST(mp, status) \
    (((mp)->m_type == SIGS_SIGNAL_RECEIVED && (mp)->m_source < INIT_PROC_NR) \
    || (is_ipc_notify(status) && (mp)->m_source == SYSTEM))

/* Callback type definitions. */
typedef void(*sef_cb_signal_handler_t)(int signo);
typedef  int(*sef_cb_signal_manager_t)(endpoint_t target, int signo);

/* Callback registration helpers. */
void sef_setcb_signal_handler(sef_cb_signal_handler_t cb);
void sef_setcb_signal_manager(sef_cb_signal_manager_t cb);

/* Predefined callback implementations. */
void sef_cb_signal_handler_null(int signo);
int sef_cb_signal_manager_null(endpoint_t target, int signo);

void sef_cb_signal_handler_term(int signo);
void sef_cb_signal_handler_posix_default(int signo);

/* Macros for predefined callback implementations. */
#define SEF_CB_SIGNAL_HANDLER_NULL      sef_cb_signal_handler_null
#define SEF_CB_SIGNAL_MANAGER_NULL      sef_cb_signal_manager_null

#define SEF_CB_SIGNAL_HANDLER_DEFAULT   sef_cb_signal_handler_null
#define SEF_CB_SIGNAL_MANAGER_DEFAULT   sef_cb_signal_manager_null

/* Debug. */
#define SEF_SIGNAL_DEBUG_DEFAULT 0

#ifndef SEF_SIGNAL_DEBUG
#define SEF_SIGNAL_DEBUG                SEF_SIGNAL_DEBUG_DEFAULT
#endif

#define sef_signal_dprint               sef_dprint
#define sef_signal_debug_begin          sef_debug_begin
#define sef_signal_debug_end            sef_debug_end

/*===========================================================================*
 *				  SEF GCOV				     *
 *===========================================================================*/
/* What to intercept. */
#define INTERCEPT_SEF_GCOV_REQUESTS 1
#define SEF_GCOV_REQUEST_TYPE COMMON_REQ_GCOV_DATA
#define IS_SEF_GCOV_REQUEST(mp, status) \
    ((mp)->m_type == COMMON_REQ_GCOV_DATA && (mp)->m_source == VFS_PROC_NR)

/* Callback type definitions. */
typedef  int(*sef_cb_gcov_t)(message *msg);

/* Callback registration helpers. */
void sef_setcb_gcov(sef_cb_gcov_t cb);

/* Macros for predefined callback implementations. */
#define SEF_CB_GCOV_FLUSH_DEFAULT        do_gcov_flush_impl

/*===========================================================================*
 *			     SEF Fault Injection			     *
 *===========================================================================*/
/* What to intercept. */
#define INTERCEPT_SEF_FI_REQUESTS 1
#define SEF_FI_REQUEST_TYPE COMMON_REQ_FI_CTL
#define IS_SEF_FI_REQUEST(mp, status) \
    (m_ptr->m_type == COMMON_REQ_FI_CTL)

/* Fault injection tool support. */
#define SEF_FI_ALLOW_EDFI               1

/*===========================================================================*
 *                          SEF State Transfer                               *
 *===========================================================================*/
#define SEF_LU_STATE_EVAL_MAX_LEN          512

/* State transfer helpers. */
int sef_copy_state_region_ctl(sef_init_info_t *info,
    vir_bytes *src_address, vir_bytes *dst_address);
int sef_copy_state_region(sef_init_info_t *info,
    vir_bytes address, size_t size, vir_bytes dst_address, int may_have_holes);
int sef_st_state_transfer(sef_init_info_t *info);

/* Callback prototypes to be passed to the State Transfer framwork. */
int sef_old_state_table_lookup(sef_init_info_t *info, void *addr);
int sef_old_state_table_lookup_opaque(void *info_opaque, void *addr);
int sef_copy_state_region_opaque(void *info_opaque, uint32_t address,
    size_t size, uint32_t dst_address);

/* Debug. */
#define SEF_ST_DEBUG_DEFAULT 		0

#ifndef SEF_ST_DEBUG
#define SEF_ST_DEBUG                    SEF_ST_DEBUG_DEFAULT
#endif

/*===========================================================================*
 *                               SEF LLVM                                    *
 *===========================================================================*/
/* LLVM helpers. */
int sef_llvm_magic_enabled(void);
int sef_llvm_real_brk(char *newbrk);
int sef_llvm_state_cleanup(void);
void sef_llvm_dump_eval(char *expr);
int sef_llvm_eval_bool(char *expr, char *result);
void *sef_llvm_state_table_addr(void);
size_t sef_llvm_state_table_size(void);
void sef_llvm_stack_refs_save(char *stack_buff);
void sef_llvm_stack_refs_restore(char *stack_buff);
int sef_llvm_state_transfer(sef_init_info_t *info);
int sef_llvm_add_special_mem_region(void *addr, size_t len, const char* name);
int sef_llvm_del_special_mem_region_by_addr(void *addr);
void sef_llvm_ds_st_init(void);
void *sef_llvm_ac_mmap(void *buf, size_t len, int prot, int flags, int fd,
	off_t offset);
int sef_llvm_ac_munmap(void *buf, size_t len);

int sef_llvm_ltckpt_enabled(void);
int sef_llvm_get_ltckpt_offset(void);
int sef_llvm_ltckpt_restart(int type, sef_init_info_t *info);

#if !defined(USE_LIVEUPDATE)
#undef INTERCEPT_SEF_LU_REQUESTS
#undef SEF_LU_DEBUG
#endif

#endif /* _SEF_H */

