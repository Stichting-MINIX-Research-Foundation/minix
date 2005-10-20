/* The system call implemented in this file:
 *   m_type:	SYS_VM_SETBUF
 *
 * The parameters for this system call are:
 *    m4_l1:	Start of the buffer
 *    m4_l2:	Length of the buffer
 *    m4_l3:	End of main memory
 */
#include "../system.h"

#define VM_DEBUG 0		/* enable/ disable debug output */

/*===========================================================================*
 *				do_vm_setbuf				     *
 *===========================================================================*/
PUBLIC int do_vm_setbuf(m_ptr)
message *m_ptr;			/* pointer to request message */
{
	vm_base= m_ptr->m4_l1;
	vm_size= m_ptr->m4_l2;
	vm_mem_high= m_ptr->m4_l3;

#if VM_DEBUG
	kprintf("do_vm_setbuf: got 0x%x @ 0x%x for 0x%x\n",
		vm_size, vm_base, vm_mem_high);
#endif

	return OK;
}
