/*
 *   The Minix hardware interrupt system.
 *   
 *   This file contains routines for managing the interrupt
 *   controller.
 *  
 *   put_irq_handler: register an interrupt handler.
 *   rm_irq_handler:  deregister an interrupt handler.
 *   intr_handle:     handle a hardware interrupt.
 *                    called by the system dependent part when an
 *                    external interrupt occures.                     
 *   enable_irq:      enable hook for IRQ.
 *   disable_irq:     disable hook for IRQ.
 */

#include "kernel.h"
#include "proc.h"
#include <minix/com.h>
#include <archconst.h>

/* number of lists of IRQ hooks, one list per supported line. */
PUBLIC irq_hook_t* irq_handlers[NR_IRQ_VECTORS] = {0};
/*===========================================================================*
 *				put_irq_handler				     *
 *===========================================================================*/
/* Register an interrupt handler.  */
PUBLIC void put_irq_handler( irq_hook_t* hook, int irq, irq_handler_t handler)
{
  int id;
  irq_hook_t **line;
  
  if( irq < 0 || irq >= NR_IRQ_VECTORS )
	minix_panic("invalid call to put_irq_handler", irq);

  line = &irq_handlers[irq];
  id = 1;
  
  while ( *line != NULL ) {
	if(hook == *line) return; /* extra initialization */
	line = &(*line)->next;
	id <<= 1;                 /* max 32 hooks for one line. */
  }
  
  if(id == 0)
	minix_panic("Too many handlers for irq", irq);

  hook->next = NULL;
  hook->handler = handler;
  hook->irq = irq;
  hook->id = id;
  *line = hook;
  irq_use |= 1 << irq;  /* this does not work for irq >= 32 */
  
  /* And as last enable the irq at the hardware.
   *
   * Internal this activates the line or source of the given interrupt.
   */
  intr_unmask(hook);  
}

/*===========================================================================*
 *				rm_irq_handler				     *
 *===========================================================================*/
/* Unregister an interrupt handler.  */
PUBLIC void rm_irq_handler( irq_hook_t* hook ) {
  int irq = hook->irq; 
  int id = hook->id;
  irq_hook_t **line;

  if( irq < 0 || irq >= NR_IRQ_VECTORS ) 
	minix_panic("invalid call to rm_irq_handler", irq);

  /* disable the irq.  */
  intr_mask(hook);
    
  /* remove the hook.  */
  line = &irq_handlers[irq];
  
  while( (*line) != NULL ) {
	if((*line)->id == id) {      
		(*line) = (*line)->next;      
		if(!irq_handlers[irq])
			irq_use &= ~(1 << irq);
		return;
    	}          
	line = &(*line)->next;
  }
  /* When the handler is not found, normally return here. */
}

/*===========================================================================*
 *				intr_handle				     *
 *===========================================================================*/
PUBLIC void intr_handle(irq_hook_t *hook)
{
/* Call the interrupt handlers for an interrupt with the given hook list.
 * The assembly part of the handler has already masked the IRQ, reenabled the
 * controller(s) and enabled interrupts.
 */

  /* Call list of handlers for an IRQ. */
  while( hook != NULL ) {
    /* For each handler in the list, mark it active by setting its ID bit,
     * call the function, and unmark it if the function returns true.
     */
    irq_actids[hook->irq] |= hook->id;
    
    /* Call the hooked function. */
    if( (*hook->handler)(hook) )
      irq_actids[hook->irq] &= ~hook->id;
    
    /* Next hooked function. */
    hook = hook->next;
  }
  
  /* The assembly code will now disable interrupts, unmask the IRQ if and only
   * if all active ID bits are cleared, and restart a process.
   */
}

/* Enable/Disable a interrupt line.  */
PUBLIC void enable_irq(hook)
irq_hook_t* hook;
{
  if((irq_actids[hook->irq] &= ~hook->id) == 0) {
    intr_unmask(hook);
    return; 
  }
}

/* Return true if the interrupt was enabled before call.  */
PUBLIC int disable_irq(hook)
irq_hook_t* hook;
{
  if(irq_actids[hook->irq] & hook->id)  /* already disabled */
    return 0;
  irq_actids[hook->irq] |= hook->id;
  intr_mask(hook);   
  return TRUE;
}

