#include "pm.h"
#include <sys/resource.h>
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <minix/syslib.h>
#include "mproc.h"
#include "param.h"

/*===========================================================================*
 *				do_getrusage				     *
 *===========================================================================*/
int do_getrusage()
{
	int res = 0;
	clock_t user_time = 0;
	clock_t sys_time = 0;
	struct rusage r_usage;
	u64_t usec;
	if ((res = sys_datacopy(who_e, (vir_bytes) m_in.RU_RUSAGE_ADDR, SELF,
		(vir_bytes) &r_usage, (vir_bytes) sizeof(r_usage))) < 0)
		return res;

	if ((res = sys_getrusage(who_e, &r_usage)) < 0)
		return res;

	if (m_in.RU_WHO == RUSAGE_CHILDREN) {
		usec = mp->mp_child_utime * 1000000 / sys_hz();
		r_usage.ru_utime.tv_sec = usec / 1000000;
		r_usage.ru_utime.tv_usec = usec % 1000000;
		usec = mp->mp_child_stime * 1000000 / sys_hz();
		r_usage.ru_stime.tv_sec = usec / 1000000;
		r_usage.ru_stime.tv_usec = usec % 1000000;
	}

	return sys_datacopy(SELF, &r_usage, who_e,
		(vir_bytes) m_in.RU_RUSAGE_ADDR, (vir_bytes) sizeof(r_usage));
}
