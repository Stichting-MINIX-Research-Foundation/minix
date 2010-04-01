/* The kernel call implemented in this file:
 *   m_type:	SYS_INT86
 *
 * The parameters for this kernel call are:
 *    m1_p1:	INT86_REG86     
 */

#include "kernel/system.h"
#include <minix/type.h>
#include <minix/endpoint.h>
#include <machine/int86.h>

#include "proto.h"

struct reg86u reg86;

/*===========================================================================*
 *				do_int86					     *
 *===========================================================================*/
PUBLIC int do_int86(struct proc * caller, message * m_ptr)
{
  data_copy(caller->p_endpoint, (vir_bytes) m_ptr->INT86_REG86,
	KERNEL, (vir_bytes) &reg86, sizeof(reg86));

  int86();

  /* Copy results back to the caller */
  data_copy(KERNEL, (vir_bytes) &reg86,
	caller->p_endpoint, (vir_bytes) m_ptr->INT86_REG86, sizeof(reg86));

  /* The BIOS call eats interrupts. Call get_randomness to generate some
   * entropy. Normally, get_randomness is called from an interrupt handler.
   * Figuring out the exact source is too complicated. CLOCK_IRQ is normally
   * not very random.
   */
  get_randomness(&krandom, CLOCK_IRQ);

  return(OK);
}
