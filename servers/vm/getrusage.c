#include <sys/resource.h>
#include "proto.h"
#include "vmproc.h"
#include "glo.h"

/*===========================================================================*
 *				do_getrusage		     		     *
 *===========================================================================*/
int do_getrusage(message *m)
{
	int res, slot;
	struct vmproc *vmp;
	struct rusage r_usage;
	if ((res = vm_isokendpt(m->m_source, &slot)) != OK)
		return ESRCH;

	vmp = &vmproc[slot];

	if ((res = sys_datacopy(m->m_source, (vir_bytes) m->RU_RUSAGE_ADDR,
		SELF, (vir_bytes) &r_usage, (vir_bytes) sizeof(r_usage))) < 0)
		return res;

	r_usage.ru_maxrss = vmp->vm_total_max;
	r_usage.ru_minflt = vmp->vm_minor_page_fault;
	r_usage.ru_majflt = vmp->vm_major_page_fault;

	return sys_datacopy(SELF, &r_usage, m->m_source,
		(vir_bytes) m->RU_RUSAGE_ADDR, (vir_bytes) sizeof(r_usage));
}
