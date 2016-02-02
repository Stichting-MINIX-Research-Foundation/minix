#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"
#include "extern.h"
#include <string.h>

/*
 * The sysctl(2) system call, handled by the MIB service.
 */
int
__sysctl(const int * name, unsigned int namelen, void * oldp, size_t * oldlenp,
	const void * newp, size_t newlen)
{
	message m;
	int r;

	memset(&m, 0, sizeof(m));
	m.m_lc_mib_sysctl.oldp = (vir_bytes)oldp;
	m.m_lc_mib_sysctl.oldlen = (oldlenp != NULL) ? *oldlenp : 0;
	m.m_lc_mib_sysctl.newp = (vir_bytes)newp;
	m.m_lc_mib_sysctl.newlen = newlen;
	m.m_lc_mib_sysctl.namelen = namelen;
	m.m_lc_mib_sysctl.namep = (vir_bytes)name;
	if (namelen <= CTL_SHORTNAME)
		memcpy(m.m_lc_mib_sysctl.name, name, sizeof(*name) * namelen);

	r = _syscall(MIB_PROC_NR, MIB_SYSCTL, &m);

	/*
	 * We copy the NetBSD behavior of replying with the old length also if
	 * the call failed, typically with ENOMEM.  This is undocumented
	 * behavior, but unfortunately relied on by sysctl(8) and other NetBSD
	 * userland code.  If the call failed at the IPC level, the resulting
	 * value will be garbage, but it should then not be used anyway.
	 */
	if (oldlenp != NULL)
		*oldlenp = m.m_mib_lc_sysctl.oldlen;

	return r;
}
