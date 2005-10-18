/* Function prototypes for the system library.  The prototypes in this file 
 * are undefined to do_unused if the kernel call is not enabled in config.h. 
 * The implementation is contained in src/kernel/system/.  
 *
 * The system library allows to access system services by doing a kernel call.
 * System calls are transformed into request messages to the SYS task that is 
 * responsible for handling the call. By convention, sys_call() is transformed 
 * into a message with type SYS_CALL that is handled in a function do_call(). 
 * 
 * Changes:
 *   Jul 30, 2005   created SYS_INT86 to support BIOS driver  (Philip Homburg) 
 *   Jul 13, 2005   created SYS_PRIVCTL to manage services  (Jorrit N. Herder) 
 *   Jul 09, 2005   updated SYS_KILL to signal services  (Jorrit N. Herder) 
 *   Jun 21, 2005   created SYS_NICE for nice(2) kernel call  (Ben J. Gras)
 *   Jun 21, 2005   created SYS_MEMSET to speed up exec(2)  (Ben J. Gras)
 *   Apr 12, 2005   updated SYS_VCOPY for virtual_copy()  (Jorrit N. Herder)
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
#include "kernel.h"
#include "proto.h"
#include "proc.h"

/* Default handler for unused kernel calls. */
_PROTOTYPE( int do_unused, (message *m_ptr) );

_PROTOTYPE( int do_exec, (message *m_ptr) );		
#if ! USE_EXEC
#define do_exec do_unused
#endif

_PROTOTYPE( int do_fork, (message *m_ptr) );
#if ! USE_FORK
#define do_fork do_unused
#endif

_PROTOTYPE( int do_newmap, (message *m_ptr) );
#if ! USE_NEWMAP
#define do_newmap do_unused
#endif

_PROTOTYPE( int do_exit, (message *m_ptr) );
#if ! USE_EXIT
#define do_exit do_unused
#endif

_PROTOTYPE( int do_trace, (message *m_ptr) );	
#if ! USE_TRACE
#define do_trace do_unused
#endif

_PROTOTYPE( int do_nice, (message *m_ptr) );
#if ! USE_NICE
#define do_nice do_unused
#endif

_PROTOTYPE( int do_copy, (message *m_ptr) );	
#define do_vircopy 	do_copy
#define do_physcopy 	do_copy
#if ! (USE_VIRCOPY || USE_PHYSCOPY)
#define do_copy do_unused
#endif

_PROTOTYPE( int do_vcopy, (message *m_ptr) );		
#define do_virvcopy 	do_vcopy
#define do_physvcopy 	do_vcopy
#if ! (USE_VIRVCOPY || USE_PHYSVCOPY)
#define do_vcopy do_unused
#endif

_PROTOTYPE( int do_umap, (message *m_ptr) );
#if ! USE_UMAP
#define do_umap do_unused
#endif

_PROTOTYPE( int do_memset, (message *m_ptr) );
#if ! USE_MEMSET
#define do_memset do_unused
#endif

_PROTOTYPE( int do_abort, (message *m_ptr) );
#if ! USE_ABORT
#define do_abort do_unused
#endif

_PROTOTYPE( int do_getinfo, (message *m_ptr) );
#if ! USE_GETINFO
#define do_getinfo do_unused
#endif

_PROTOTYPE( int do_privctl, (message *m_ptr) );	
#if ! USE_PRIVCTL
#define do_privctl do_unused
#endif

_PROTOTYPE( int do_segctl, (message *m_ptr) );
#if ! USE_SEGCTL
#define do_segctl do_unused
#endif

_PROTOTYPE( int do_irqctl, (message *m_ptr) );
#if ! USE_IRQCTL
#define do_irqctl do_unused
#endif

_PROTOTYPE( int do_devio, (message *m_ptr) );
#if ! USE_DEVIO
#define do_devio do_unused
#endif

_PROTOTYPE( int do_vdevio, (message *m_ptr) );
#if ! USE_VDEVIO
#define do_vdevio do_unused
#endif

_PROTOTYPE( int do_int86, (message *m_ptr) );

_PROTOTYPE( int do_sdevio, (message *m_ptr) );
#if ! USE_SDEVIO
#define do_sdevio do_unused
#endif

_PROTOTYPE( int do_kill, (message *m_ptr) );
#if ! USE_KILL
#define do_kill do_unused
#endif

_PROTOTYPE( int do_getksig, (message *m_ptr) );
#if ! USE_GETKSIG
#define do_getksig do_unused
#endif

_PROTOTYPE( int do_endksig, (message *m_ptr) );
#if ! USE_ENDKSIG
#define do_endksig do_unused
#endif

_PROTOTYPE( int do_sigsend, (message *m_ptr) );
#if ! USE_SIGSEND
#define do_sigsend do_unused
#endif

_PROTOTYPE( int do_sigreturn, (message *m_ptr) );
#if ! USE_SIGRETURN
#define do_sigreturn do_unused
#endif

_PROTOTYPE( int do_times, (message *m_ptr) );		
#if ! USE_TIMES
#define do_times do_unused
#endif

_PROTOTYPE( int do_setalarm, (message *m_ptr) );	
#if ! USE_SETALARM
#define do_setalarm do_unused
#endif

#endif	/* SYSTEM_H */

