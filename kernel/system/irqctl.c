/* The system call implemented in this file:
 *   m_type:	SYS_IRQCTL
 *
 * The parameters for this system call are:
 *    m5_c1:	IRQ_REQUEST	(control operation to perform)	
 *    m5_c2:	IRQ_VECTOR	(irq line that must be controlled)
 *    m5_i1:	IRQ_POLICY	(irq policy allows reenabling interrupts)
 *    m5_l3:	IRQ_HOOK_ID	(index of irq hook assigned at kernel)
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

#include "../kernel.h"
#include "../system.h"

/*===========================================================================*
 *				do_irqctl				     *
 *===========================================================================*/
PUBLIC int do_irqctl(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
  /* Dismember the request message. */
  int irq_vec;
  int irq_hook_id;
  int proc_nr;  
  int r = OK;
  irq_hook_t *hook_ptr;

  irq_hook_id = (unsigned) m_ptr->IRQ_HOOK_ID;
  irq_vec = (unsigned) m_ptr->IRQ_VECTOR; 

  /* See what is requested and take needed actions. */
  switch(m_ptr->IRQ_REQUEST) {

  /* Enable or disable IRQs. This is straightforward. */
  case IRQ_ENABLE:           
  case IRQ_DISABLE: 
      if (irq_hook_id >= NR_IRQ_HOOKS) return(EINVAL);
      if (irq_hooks[irq_hook_id].proc_nr != m_ptr->m_source) return(EPERM);
      if (m_ptr->IRQ_REQUEST == IRQ_ENABLE)
          enable_irq(&irq_hooks[irq_hook_id]);	
      else 
          disable_irq(&irq_hooks[irq_hook_id]);	
      break;
  

  /* Control IRQ policies. Set a policy and needed details in the IRQ table.
   * This policy is used by a generic function to handle hardware interrupts. 
   */
  case IRQ_SETPOLICY:  

      /* Check if IRQ line is acceptable. */
      if (irq_vec < 0 || irq_vec >= NR_IRQ_VECTORS) {
 	  kprintf("ST: irq line %d is not acceptable!\n", irq_vec);
          return(EINVAL);
      }

      /* Find a free IRQ hook for this mapping. */
      hook_ptr = NULL;
      for (irq_hook_id=0; irq_hook_id<NR_IRQ_HOOKS; irq_hook_id++) {
          if (irq_hooks[irq_hook_id].proc_nr == NONE) {	
              hook_ptr = &irq_hooks[irq_hook_id];	/* free hook */
              break;
          }
      }
      if (hook_ptr == NULL) return(ENOSPC);

      /* Only caller can request IRQ mappings. Install handler. */
      hook_ptr->proc_nr = m_ptr->m_source;	/* process to notify */   	
      hook_ptr->policy = m_ptr->IRQ_POLICY;	/* policy for interrupts */
      put_irq_handler(hook_ptr, irq_vec, generic_handler);

      /* Return index of the IRQ hook in use. */
      m_ptr->IRQ_HOOK_ID = irq_hook_id;
      break;

  case IRQ_RMPOLICY:  
  	if(irq_hook_id < 0 || irq_hook_id >= NR_IRQ_HOOKS ||
	   irq_hooks[irq_hook_id].proc_nr == NONE) {
  		r = EINVAL;
  	} else {
	 	if(m_ptr->m_source != irq_hooks[irq_hook_id].proc_nr) {
	  		r = EPERM;
	  	} else {
	        	r = rm_irq_handler(irq_vec, irq_hooks[irq_hook_id].id);
	        }
        }
  	break;

  default:
      r = EINVAL;				/* invalid IRQ_REQUEST */
  }
  return(r);
}

