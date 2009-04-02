/* The kernel call implemented in this file:
 *   m_type:	SYS_INT86
 *
 * The parameters for this kernel call are:
 *    m1_p1:	INT86_REG86     
 */

#include "../../system.h"
#include <minix/type.h>
#include <minix/endpoint.h>
#include <minix/portio.h>
#include <ibm/int86.h>

#include "proto.h"

struct reg86u reg86;

/*===========================================================================*
 *				do_int86					     *
 *===========================================================================*/
PUBLIC int do_int86(m_ptr)
register message *m_ptr;	/* pointer to request message */
{
  data_copy(who_e, (vir_bytes) m_ptr->INT86_REG86,
	SYSTEM, (vir_bytes) &reg86, sizeof(reg86));

  level0(int86);

  /* Copy results back to the caller */
  data_copy(SYSTEM, (vir_bytes) &reg86,
	who_e, (vir_bytes) m_ptr->INT86_REG86, sizeof(reg86));

  /* The BIOS call eats interrupts. Call get_randomness to generate some
   * entropy. Normally, get_randomness is called from an interrupt handler.
   * Figuring out the exact source is too complicated. CLOCK_IRQ is normally
   * not very random.
   */
  lock;
  get_randomness(&krandom, CLOCK_IRQ);
  unlock;

  return(OK);
}
