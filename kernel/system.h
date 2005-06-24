/* Function prototypes for the system library. The implementation is contained
 * in src/kernel/system/. The system library allows to access system services
 * by doing a system call. System calls are  transformed into request messages
 * to the SYS task that is responsible for handling the call. By convention, a
 * sys_call() is transformed into a message with type SYS_CALL that is handled
 * in a function named do_call(). 
 */ 

#ifndef SYSTEM_H
#define SYSTEM_H

/* Common includes for the system library. */
#include <minix/com.h>
#include <minix/config.h>
#include "proc.h"

_PROTOTYPE( int do_exec, (message *m_ptr) );		/* process control */
_PROTOTYPE( int do_fork, (message *m_ptr) );
_PROTOTYPE( int do_newmap, (message *m_ptr) );
_PROTOTYPE( int do_xit, (message *m_ptr) );

_PROTOTYPE( int do_copy, (message *m_ptr) );		/* copying */
#define do_vircopy 	do_copy
#define do_physcopy 	do_copy
_PROTOTYPE( int do_vcopy, (message *m_ptr) );		
#define do_virvcopy 	do_vcopy
#define do_physvcopy 	do_vcopy
_PROTOTYPE( int do_umap, (message *m_ptr) );
_PROTOTYPE( int do_physzero, (message *m_ptr) );

_PROTOTYPE( int do_unused, (message *m_ptr) );		/* miscellaneous */
_PROTOTYPE( int do_abort, (message *m_ptr) );
_PROTOTYPE( int do_getinfo, (message *m_ptr) );

_PROTOTYPE( int do_svrctl, (message *m_ptr) );		/* system control */
_PROTOTYPE( int do_iopenable, (message *m_ptr) );
_PROTOTYPE( int do_segctl, (message *m_ptr) );

_PROTOTYPE( int do_irqctl, (message *m_ptr) );		/* device I/O */
_PROTOTYPE( int do_devio, (message *m_ptr) );
_PROTOTYPE( int do_vdevio, (message *m_ptr) );
_PROTOTYPE( int do_sdevio, (message *m_ptr) );

_PROTOTYPE( int do_kill, (message *m_ptr) );		/* signal handling */
_PROTOTYPE( int do_getsig, (message *m_ptr) );
_PROTOTYPE( int do_endsig, (message *m_ptr) );
_PROTOTYPE( int do_sigsend, (message *m_ptr) );
_PROTOTYPE( int do_sigreturn, (message *m_ptr) );

_PROTOTYPE( int do_times, (message *m_ptr) );		/* clock functions */
_PROTOTYPE( int do_setalarm, (message *m_ptr) );	
#define         do_flagalrm 	do_setalarm
#define         do_signalrm 	do_setalarm
#define         do_syncalrm 	do_setalarm

_PROTOTYPE( int do_trace, (message *m_ptr) );		/* process tracing */


#endif	/* SYSTEM_H */
