/* Prototypes for System Event Framework (SEF) functions. */

#ifndef _SEF_H
#define _SEF_H

/* SEF entry points for system processes. */
_PROTOTYPE( void sef_startup, (void) );
_PROTOTYPE( int sef_receive, (endpoint_t src, message *m_ptr) );

/* SEF Debug. */
#include <stdio.h>
#define sef_dprint                      printf
#define sef_debug_begin()               (void)(NULL)
#define sef_debug_end()                 fflush(stdout)

/*===========================================================================*
 *				  SEF Ping				     *
 *===========================================================================*/
/* What to intercept. */
#define INTERCEPT_SEF_PING_REQUESTS 1
#define IS_SEF_PING_REQUEST(mp) (is_notify((mp)->m_type) \
    && (mp)->m_source == RS_PROC_NR)

/* Callback type definitions. */
typedef void(*sef_cb_ping_reply_t)(message *m_ptr);

/* Callback registration helpers. */
_PROTOTYPE( void sef_setcb_ping_reply, (sef_cb_ping_reply_t cb));

/* Predefined callback implementations. */
_PROTOTYPE( void sef_cb_ping_reply_null, (message *m_ptr) );

_PROTOTYPE( void sef_cb_ping_reply_pong, (message *m_ptr) );

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
#define IS_SEF_LU_REQUEST(mp) ((mp)->m_type == RS_LU_PREPARE \
    && (mp)->m_source == RS_PROC_NR)

/* Global helpers. */
_PROTOTYPE( void sef_lu_ready, (int result) );

/* Callback type definitions. */
typedef void(*sef_cb_lu_prepare_t)(int);
typedef  int(*sef_cb_lu_state_isvalid_t)(int);
typedef void(*sef_cb_lu_state_changed_t)(int, int);
typedef void(*sef_cb_lu_state_dump_t)(int);
typedef  int(*sef_cb_lu_ready_pre_t)(int);

/* Callback registration helpers. */
_PROTOTYPE( void sef_setcb_lu_prepare, (sef_cb_lu_prepare_t cb) );
_PROTOTYPE( void sef_setcb_lu_state_isvalid, (sef_cb_lu_state_isvalid_t cb) );
_PROTOTYPE( void sef_setcb_lu_state_changed, (sef_cb_lu_state_changed_t cb) );
_PROTOTYPE( void sef_setcb_lu_state_dump, (sef_cb_lu_state_dump_t cb) );
_PROTOTYPE( void sef_setcb_lu_ready_pre, (sef_cb_lu_ready_pre_t cb) );

/* Predefined callback implementations. */
_PROTOTYPE( void sef_cb_lu_prepare_null, (int state) );
_PROTOTYPE(  int sef_cb_lu_state_isvalid_null, (int state) );
_PROTOTYPE( void sef_cb_lu_state_changed_null, (int old_state, int state) );
_PROTOTYPE( void sef_cb_lu_state_dump_null, (int state) );
_PROTOTYPE(  int sef_cb_lu_ready_pre_null, (int result) );

_PROTOTYPE( void sef_cb_lu_prepare_always_ready, (int state) );
_PROTOTYPE(  int sef_cb_lu_state_isvalid_standard, (int state) );

/* Macros for predefined callback implementations. */
#define SEF_CB_LU_PREPARE_NULL          sef_cb_lu_prepare_null
#define SEF_CB_LU_STATE_ISVALID_NULL    sef_cb_lu_state_isvalid_null
#define SEF_CB_LU_STATE_CHANGED_NULL    sef_cb_lu_state_changed_null
#define SEF_CB_LU_STATE_DUMP_NULL       sef_cb_lu_state_dump_null
#define SEF_CB_LU_READY_PRE_NULL        sef_cb_lu_ready_pre_null

#define SEF_CB_LU_PREPARE_DEFAULT       sef_cb_lu_prepare_null
#define SEF_CB_LU_STATE_ISVALID_DEFAULT sef_cb_lu_state_isvalid_null
#define SEF_CB_LU_STATE_CHANGED_DEFAULT sef_cb_lu_state_changed_null
#define SEF_CB_LU_STATE_DUMP_DEFAULT    sef_cb_lu_state_dump_null
#define SEF_CB_LU_READY_PRE_DEFAULT     sef_cb_lu_ready_pre_null

/* Standard live update states. */
#define SEF_LU_STATE_NULL               0    /* null state */
#define SEF_LU_STATE_WORK_FREE          1    /* no work in progress */
#define SEF_LU_STATE_REQUEST_FREE       2    /* no request in progress */
#define SEF_LU_STATE_PROTOCOL_FREE      3    /* no protocol in progress */
#define SEF_LU_STATE_CUSTOM_BASE        (SEF_LU_STATE_PROTOCOL_FREE+1)
#define SEF_LU_STATE_IS_STANDARD(s)     ((s) > SEF_LU_STATE_NULL \
    && (s) < SEF_LU_STATE_CUSTOM_BASE)

/* Debug. */
#define SEF_LU_DEBUG_DEFAULT 1

#ifndef SEF_LU_DEBUG
#define SEF_LU_DEBUG            SEF_LU_DEBUG_DEFAULT
#endif

#define sef_lu_dprint           sef_dprint
#define sef_lu_debug_begin      sef_debug_begin
#define sef_lu_debug_end        sef_debug_end

#endif /* _SEF_H */

