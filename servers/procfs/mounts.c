#include "inc.h"
#include "vfs/vmnt.h"

extern struct mproc mproc[NR_PROCS];

/*===========================================================================*
 *                              root_mtab                                    *
 *===========================================================================*/
void
root_mounts(void)
{
	struct vmnt vmnt[NR_MNTS];
	struct vmnt *vmp;
	struct mproc *rmp;
        int slot;

        if (getsysinfo(VFS_PROC_NR, SI_VMNT_TAB, vmnt, sizeof(vmnt)) != OK)
                return;

        for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; vmp++) {
                if (vmp->m_dev == NO_DEV)
                        continue;
		if (vmp->m_fs_e == PFS_PROC_NR)
			continue; /* Skip (special case) */

		slot = _ENDPOINT_P(vmp->m_fs_e);
		if (slot < 0 || slot >= NR_PROCS)
			continue;
		rmp = &mproc[slot];
                buf_printf("%s on %s type %s (%s)\n", vmp->m_mount_dev,
			vmp->m_mount_path, rmp->mp_name,
			(vmp->m_flags & VMNT_READONLY) ? "ro" : "rw");
        }
}
