/* The system call implemented in this file:
 *   m_type:	SYS_IRQCTL
 *
 * The parameters for this system call are:
 *    m5_c1:	IRQ_REQUEST	(control operation to perform)	
 *    m5_c2:	IRQ_VECTOR	(irq line that must be controlled)
 *    m5_i1:	IRQ_POLICY	(flags to control the IRQCTL request)
 *    m5_i2:	IRQ_PROC_NR	(process number to notify)
 *    m5_l1:	IRQ_PORT	(port to write to / read from)
 *    m5_l2:	IRQ_VIR_ADDR	(virtual address at caller)
 *    m5_l3:	IRQ_MASK_VAL	(value to be written or strobe mask)
 *
 * Author:
 *    Jorrit N. Herder <jnherder@cs.vu.nl>
 */

#include "../kernel.h"
#include "../system.h"


/*===========================================================================*
 *			       do_irqctl				     *
 *===========================================================================*/
PUBLIC int do_irqctl(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
  /* Dismember the request message. */
  int irq = m_ptr->IRQ_VECTOR;        	/* which IRQ vector */
  int policy = m_ptr->IRQ_POLICY;      	/* policy field with flags */
  long port = m_ptr->IRQ_PORT;        	/* port to read or write */
  vir_bytes vir_addr = m_ptr->IRQ_VIR_ADDR; 	/* address at caller */
  phys_bytes phys_addr = 0;		/* calculate physical address */
  long mask_val = m_ptr->IRQ_MASK_VAL;	/* mask or value to be written */
  int proc_nr = m_ptr->IRQ_PROC_NR;   	/* process number to forward to */

  /* Check if IRQ line is acceptable. */
  if ((unsigned) irq >= NR_IRQ_VECTORS) {
      kprintf("ST: irq line %d is not acceptable!\n", irq);
      return(EINVAL);
  }

  /* See what is requested and take needed actions. */
  switch(m_ptr->IRQ_REQUEST) {

  /* Enable or disable IRQs. This is straightforward. */
  case IRQ_ENABLE: {          
      enable_irq(&irqtab[irq].hook);	
      break;
  }
  case IRQ_DISABLE: {
      disable_irq(&irqtab[irq].hook); 
      break;
  }

  /* Control IRQ policies. Set a policy and needed details in the IRQ table.
   * This policy is used by a generic function to handle hardware interrupts. 
   * The generic_handler() is contained in system.c.
   */
  case IRQ_SETPOLICY: { 

      if (proc_nr == NONE)  {    		/* remove irqtab entry */
          if (irqtab[irq].proc_nr != m_ptr->m_source) {
              return(EPERM);     		/* only owner may do so */
          }
          kprintf("ST: notify: cannot remove entry for IRQ %d\n",irq);
          return(ENOSYS);        		/* not yet supported */
      }
      else {                     		/* install generic handler */
        if (irqtab[irq].proc_nr != NONE) { 	/* IRQ entry already taken */
            kprintf("ST: notify: slot for IRQ %d already taken\n", irq);
            return(EBUSY);       		/* cannot overwrite entry */
        }
        if (proc_nr == SELF)			/* check for magic proc nr */
            proc_nr = m_ptr->m_source;   	/* set caller's proc nr */
        if (! isokprocn(proc_nr)) {		/* check if proc nr is ok */
            kprintf("ST: notify: invalid proc_nr: %d\n", proc_nr);
            return(EINVAL);
        }
        if (policy & IRQ_READ_PORT) {	/* get phys_addr at caller */
            switch(policy & (IRQ_BYTE|IRQ_WORD|IRQ_LONG)) {
            case IRQ_BYTE: phys_addr=numap_local(proc_nr,vir_addr,sizeof( u8_t));
            		   break;
            case IRQ_WORD: phys_addr=numap_local(proc_nr,vir_addr,sizeof(u16_t));
            		   break;
            case IRQ_LONG: phys_addr=numap_local(proc_nr,vir_addr,sizeof(u32_t));
            		   break;
            default: return(EINVAL);		/* wrong type flags */
            }
            if (phys_addr==0) return(EFAULT);	/* invalid address */
        }
        /* Arguments seem to be OK, register them in the IRQ table. */
        irqtab[irq].policy = policy;		/* policy for interrupts */
        irqtab[irq].proc_nr = proc_nr;		/* process number to notify */
        irqtab[irq].port = port;		/* port to read or write */
        irqtab[irq].addr = phys_addr;		/* address to store status */
        irqtab[irq].mask_val = mask_val;	/* strobe mask or value */
        put_irq_handler(&irqtab[irq].hook, irq, generic_handler);
      }
      break;
  }
  default:
      return(EINVAL);				/* invalid IRQ_REQUEST */
  }
  return(OK);
}

