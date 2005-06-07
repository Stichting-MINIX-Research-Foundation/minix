/* This file contains information dump procedures. During the initialization 
 * of the Information Service 'known' function keys are registered at the TTY
 * server in order to receive a notification if one is pressed. Here, the 
 * corresponding dump procedure is called.  
 *
 * The entry points into this file are
 *   handle_fkey:	handle a function key pressed notification
 */

#include "is.h"


/*===========================================================================*
 *				handle_fkey				     *
 *===========================================================================*/
PUBLIC int do_fkey_pressed(message *m)
{
    if (F1 <= m->NOTIFY_ARG && m->NOTIFY_ARG <= F12) {
        switch(m->NOTIFY_ARG) {
            case  F1:	proctab_dmp();		break;
            case  F2:	memmap_dmp();		break;
            case  F3:	image_dmp();		break;
            case  F4:	sendmask_dmp();		break;
            case  F5:	monparams_dmp();	break;
            case  F6:	irqtab_dmp();		break;
            case  F7:	kmessages_dmp();	break;
            case  F8:	timing_dmp();		break;

            case F10:	kenv_dmp();		break;
            case F11:	memchunks_dmp();	break;
            case F12:	sched_dmp();		break;
            default: 
            	printf("IS: unhandled notify for F%d (code %d)\n", 
            		m->NOTIFY_FLAGS, m->NOTIFY_ARG);
        }
    }

    if (SF1 <= m->NOTIFY_ARG && m->NOTIFY_ARG <= SF12) {
        switch(m->NOTIFY_ARG) {

            case SF1:	mproc_dmp();		break;

            case SF3:	fproc_dmp();		break;
            case SF4:	dtab_dmp();		break;

            case SF6:	diagnostics_dmp();	break;
            default: 
            	printf("IS: unhandled notify for Shift-F%d (code %d)\n", 
            		m->NOTIFY_FLAGS, m->NOTIFY_ARG);
        }
    }
    return(EDONTREPLY);
}

