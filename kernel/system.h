/* Function prototypes for the system library.  The prototypes in this file 
 * are undefined to NULL if the kernel call is not enabled in config.h.
 * The implementation is contained in src/kernel/system/.  
 *
 * The system library allows to access system services by doing a kernel call.
 * System calls are transformed into request messages to the SYS task that is 
 * responsible for handling the call. By convention, sys_call() is transformed 
 * into a message with type SYS_CALL that is handled in a function do_call(). 
 * 
 * Changes:
 *   Mar 01, 2010   SYS_CLEAR and SYS_EXIT split (Cristiano Giuffrida)
 *   Jul 30, 2005   created SYS_INT86 to support BIOS driver  (Philip Homburg) 
 *   Jul 13, 2005   created SYS_PRIVCTL to manage services  (Jorrit N. Herder) 
 *   Jul 09, 2005   updated SYS_KILL to signal services  (Jorrit N. Herder) 
 *   Jun 21, 2005   created SYS_NICE for nice(2) kernel call  (Ben J. Gras)
 *   Jun 21, 2005   created SYS_MEMSET to speed up exec(2)  (Ben J. Gras)
 *   Jan 20, 2005   updated SYS_COPY for virtual_copy()  (Jorrit N. Herder)
 *   Oct 24, 2004   created SYS_GETKSIG to support PM  (Jorrit N. Herder) 
 *   Oct 10, 2004   created handler for unused calls  (Jorrit N. Herder) 
 *   Sep 09, 2004   updated SYS_EXIT to let services exit  (Jorrit N. Herder) 
 *   Aug 25, 2004   rewrote SYS_SETALARM to clean up code  (Jorrit N. Herder)
 *   Jul 13, 2004   created SYS_SEGCTL to support drivers  (Jorrit N. Herder) 
 *   May 24, 2004   created SYS_SDEVIO to support drivers  (Jorrit N. Herder) 
 *   May 24, 2004   created SYS_GETINFO to retrieve info  (Jorrit N. Herder) 
 *   Apr 18, 2004   created SYS_VDEVIO to support drivers  (Jorrit N. Herder) 
 *   Feb 24, 2004   created SYS_IRQCTL to support drivers  (Jorrit N. Herder) 
 *   Feb 02, 2004   created SYS_DEVIO to support drivers  (Jorrit N. Herder) 
 */ 

#ifndef SYSTEM_H
#define SYSTEM_H

/* Common includes for the system library. */
#include "debug.h"
#include "kernel.h"
#include "proto.h"
#include "proc.h"

_PROTOTYPE( int do_exec, (struct proc * caller, message *m_ptr) );
#if ! USE_EXEC
#define do_exec NULL
#endif

_PROTOTYPE( int do_fork, (struct proc * caller, message *m_ptr) );
#if ! USE_FORK
#define do_fork NULL
#endif

_PROTOTYPE( int do_newmap, (struct proc * caller, message *m_ptr) );
#if ! USE_NEWMAP
#define do_newmap NULL
#endif

_PROTOTYPE( int do_clear, (struct proc * caller, message *m_ptr) );
#if ! USE_CLEAR
#define do_clear NULL
#endif

_PROTOTYPE( int do_trace, (struct proc * caller, message *m_ptr) );
#if ! USE_TRACE
#define do_trace NULL
#endif

_PROTOTYPE( int do_runctl, (struct proc * caller, message *m_ptr) );
#if ! USE_RUNCTL
#define do_runctl NULL
#endif

_PROTOTYPE( int do_update, (struct proc * caller, message *m_ptr) );
#if ! USE_UPDATE
#define do_update NULL
#endif

_PROTOTYPE( int do_exit, (struct proc * caller, message *m_ptr) );
#if ! USE_EXIT
#define do_exit NULL
#endif

_PROTOTYPE( int do_copy, (struct proc * caller, message *m_ptr) );
#define do_vircopy 	do_copy
#if ! (USE_VIRCOPY || USE_PHYSCOPY)
#define do_copy NULL
#endif

_PROTOTYPE( int do_umap, (struct proc * caller, message *m_ptr) );
#if ! USE_UMAP
#define do_umap NULL
#endif

_PROTOTYPE( int do_memset, (struct proc * caller, message *m_ptr) );
#if ! USE_MEMSET
#define do_memset NULL
#endif

_PROTOTYPE( int do_abort, (struct proc * caller, message *m_ptr) );
#if ! USE_ABORT
#define do_abort NULL
#endif

_PROTOTYPE( int do_getinfo, (struct proc * caller, message *m_ptr) );
#if ! USE_GETINFO
#define do_getinfo NULL
#endif

_PROTOTYPE( int do_privctl, (struct proc * caller, message *m_ptr) );
#if ! USE_PRIVCTL
#define do_privctl NULL
#endif

_PROTOTYPE( int do_segctl, (struct proc * caller, message *m_ptr) );
#if ! USE_SEGCTL
#define do_segctl NULL
#endif

_PROTOTYPE( int do_irqctl, (struct proc * caller, message *m_ptr) );
#if ! USE_IRQCTL
#define do_irqctl NULL
#endif

_PROTOTYPE( int do_devio, (struct proc * caller, message *m_ptr) );
#if ! USE_DEVIO
#define do_devio NULL
#endif

_PROTOTYPE( int do_vdevio, (struct proc * caller, message *m_ptr) );
#if ! USE_VDEVIO
#define do_vdevio NULL
#endif

_PROTOTYPE( int do_int86, (struct proc * caller, message *m_ptr) );

_PROTOTYPE( int do_sdevio, (struct proc * caller, message *m_ptr) );
#if ! USE_SDEVIO
#define do_sdevio NULL
#endif

_PROTOTYPE( int do_kill, (struct proc * caller, message *m_ptr) );
#if ! USE_KILL
#define do_kill NULL
#endif

_PROTOTYPE( int do_getksig, (struct proc * caller, message *m_ptr) );
#if ! USE_GETKSIG
#define do_getksig NULL
#endif

_PROTOTYPE( int do_endksig, (struct proc * caller, message *m_ptr) );
#if ! USE_ENDKSIG
#define do_endksig NULL
#endif

_PROTOTYPE( int do_sigsend, (struct proc * caller, message *m_ptr) );
#if ! USE_SIGSEND
#define do_sigsend NULL
#endif

_PROTOTYPE( int do_sigreturn, (struct proc * caller, message *m_ptr) );
#if ! USE_SIGRETURN
#define do_sigreturn NULL
#endif

_PROTOTYPE( int do_times, (struct proc * caller, message *m_ptr) );
#if ! USE_TIMES
#define do_times NULL
#endif

_PROTOTYPE( int do_setalarm, (struct proc * caller, message *m_ptr) );
#if ! USE_SETALARM
#define do_setalarm NULL
#endif

_PROTOTYPE( int do_stime, (struct proc * caller, message *m_ptr) );

_PROTOTYPE( int do_vtimer, (struct proc * caller, message *m_ptr) );
#if ! USE_VTIMER
#define do_vtimer NULL
#endif

_PROTOTYPE( int do_safecopy_to, (struct proc * caller, message *m_ptr) );
_PROTOTYPE( int do_safecopy_from, (struct proc * caller, message *m_ptr) );
_PROTOTYPE( int do_vsafecopy, (struct proc * caller, message *m_ptr) );
_PROTOTYPE( int do_iopenable, (struct proc * caller, message *m_ptr) );
_PROTOTYPE( int do_vmctl, (struct proc * caller, message *m_ptr) );
_PROTOTYPE( int do_setgrant, (struct proc * caller, message *m_ptr) );
_PROTOTYPE( int do_readbios, (struct proc * caller, message *m_ptr) );

_PROTOTYPE( int do_safemap, (struct proc * caller, message *m_ptr) );
_PROTOTYPE( int do_saferevmap, (struct proc * caller, message *m_ptr) );
_PROTOTYPE( int do_safeunmap, (struct proc * caller, message *m_ptr) );

_PROTOTYPE( int do_sprofile, (struct proc * caller, message *m_ptr) );
#if ! SPROFILE
#define do_sprofile NULL
#endif

_PROTOTYPE( int do_cprofile, (struct proc * caller, message *m_ptr) );
_PROTOTYPE( int do_profbuf, (struct proc * caller, message *m_ptr) );

_PROTOTYPE( int do_getmcontext, (struct proc * caller, message *m_ptr) );
_PROTOTYPE( int do_setmcontext, (struct proc * caller, message *m_ptr) );
#if ! USE_MCONTEXT
#define do_getmcontext NULL
#define do_setmcontext NULL
#endif

_PROTOTYPE( int do_schedule,    (struct proc * caller, message *m_ptr) );
_PROTOTYPE( int do_schedctl, (struct proc * caller, message *m_ptr) );

_PROTOTYPE( int do_statectl, (struct proc * caller, message *m_ptr) );
#if ! USE_STATECTL
#define do_statectl NULL
#endif

#endif	/* SYSTEM_H */

