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
#define pressed(k) ((F1<=(k) && (k)<=F12 && bit_isset(m->FKEY_FKEYS, ((k)-F1+1))) \
  	|| (SF1<=(k) && (k)<=SF12 && bit_isset(m->FKEY_SFKEYS, ((k)-SF1+1)))) 

PUBLIC int do_fkey_pressed(message *m)
{
  int s;

  /* The notification message does not convey any information, other
   * than that some function keys have been pressed. Ask TTY for details.
   */
  m->m_type = FKEY_CONTROL;
  m->FKEY_REQUEST = FKEY_EVENTS;
  if (OK != (s=sendrec(TTY, m)))
      report("IS", "warning, sendrec to TTY failed", s);

  /* Now check which keys were pressed: F1-F12. */
  if (pressed(F1)) 	proctab_dmp();
  if (pressed(F2))      memmap_dmp();
  if (pressed(F3))	image_dmp();
  if (pressed(F4))	privileges_dmp();
  if (pressed(F5))	monparams_dmp();
  if (pressed(F6))	irqtab_dmp();
  if (pressed(F7))	kmessages_dmp();

  if (pressed(F10))	kenv_dmp();
  if (pressed(F11))	timing_dmp();
  if (pressed(F12))	sched_dmp();

  if (pressed(F9)) { 
  	printf("IS server going into infinite loop... hit 5x a function key\n");
  	printf("Five times a function key is fine as well ...\n");
	while(TRUE) {
		if (OK == nb_receive(ANY, m)) {
			if (s++ >= 5 ) break;
		}
	}
  	printf("IS server back to normal ... \n");
  	return(EDONTREPLY);
  }

  /* Also check Shift F1-F6 keys. */
  if (pressed(SF1))	mproc_dmp();

  if (pressed(SF3))	fproc_dmp();
  if (pressed(SF4))	dtab_dmp();
  if (pressed(SF6))	diagnostics_dmp();

  /* Inhibit sending a reply message. */
  return(EDONTREPLY);
}

