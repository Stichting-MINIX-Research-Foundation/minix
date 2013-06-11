/* This file takes care of GETRUSAGE system call
 */

#include "fs.h"
#include "fproc.h"
#include "glo.h"
#include <minix/callnr.h>
#include <minix/com.h>
#include <sys/resource.h>

/*===========================================================================*
 *				do_getrusage				     *
 *===========================================================================*/
int do_getrusage(message *UNUSED(m_out))
{
	int res;
	struct rusage r_usage;

	if ((res = sys_datacopy(who_e, (vir_bytes) m_in.RU_RUSAGE_ADDR, SELF,
		(vir_bytes) &r_usage, (vir_bytes) sizeof(r_usage))) < 0)
		return res;

	r_usage.ru_inblock = fp->in_blocks;
	r_usage.ru_oublock = fp->out_blocks;
	r_usage.ru_ixrss = fp->text_size;
	r_usage.ru_idrss = fp->data_size;
	r_usage.ru_isrss = DEFAULT_STACK_LIMIT;

	return sys_datacopy(SELF, (vir_bytes) &r_usage, who_e,
		(vir_bytes) m_in.RU_RUSAGE_ADDR, (phys_bytes) sizeof(r_usage));
}
