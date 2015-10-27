/* MIB service - hw.c - implementation of the CTL_HW subtree */

#include "mib.h"

#if defined(__i386__)
static const char mach[] = "i386";	/* machine (cpu) type */
static const char arch[] = "i386";	/* architecture */
#elif defined(__arm__)
static const char mach[] = "evbarm";	/* machine (cpu) type */
static const char arch[] = "evbarm";	/* architecture */
#else
#error "unknown machine architecture"
#endif

/*
 * Implementation of CTL_HW HW_PHYSMEM/HW_PHYSMEM64.
 */
static ssize_t
mib_hw_physmem(struct mib_call * call __unused, struct mib_node * node,
	struct mib_oldp * oldp, struct mib_newp * newp __unused)
{
	struct vm_stats_info vsi;
	u_quad_t physmem64;
	unsigned int physmem;

	if (vm_info_stats(&vsi) != OK)
		return EINVAL;

	physmem64 = (u_quad_t)vsi.vsi_total * vsi.vsi_pagesize;

	if (node->node_size == sizeof(int)) {
		if (physmem64 > UINT_MAX)
			physmem = UINT_MAX;
		else
			physmem = (unsigned int)physmem64;

		return mib_copyout(oldp, 0, &physmem, sizeof(physmem));
	} else
		return mib_copyout(oldp, 0, &physmem64, sizeof(physmem64));
}

/*
 * Implementation of CTL_HW HW_USERMEM/HW_USERMEM64.
 */
static ssize_t
mib_hw_usermem(struct mib_call * call __unused, struct mib_node * node,
	struct mib_oldp * oldp, struct mib_newp * newp __unused)
{
	struct vm_stats_info vsi;
	struct vm_usage_info vui;
	u_quad_t usermem64;
	unsigned int usermem;

	if (vm_info_stats(&vsi) != OK)
		return EINVAL;

	usermem64 = (u_quad_t)vsi.vsi_total * vsi.vsi_pagesize;

	if (vm_info_usage(KERNEL, &vui) != OK)
		return EINVAL;

	if (usermem64 >= vui.vui_total)
		usermem64 -= vui.vui_total;
	else
		usermem64 = 0;

	if (node->node_size == sizeof(int)) {
		if (usermem64 > UINT_MAX)
			usermem = UINT_MAX;
		else
			usermem = (unsigned int)usermem64;

		return mib_copyout(oldp, 0, &usermem, sizeof(usermem));
	} else
		return mib_copyout(oldp, 0, &usermem64, sizeof(usermem64));
}

/*
 * Implementation of CTL_HW HW_NCPUONLINE.
 */
static ssize_t
mib_hw_ncpuonline(struct mib_call * call __unused,
	struct mib_node * node __unused, struct mib_oldp * oldp,
	struct mib_newp * newp __unused)
{
	struct machine machine;
	int ncpuonline;

	if (sys_getmachine(&machine) != OK)
		return EINVAL;

	ncpuonline = machine.processors_count;

	return mib_copyout(oldp, 0, &ncpuonline, sizeof(ncpuonline));
}

/* The CTL_HW nodes. */
static struct mib_node mib_hw_table[] = {
/* 1*/	[HW_MACHINE]		= MIB_STRING(_P | _RO, mach, "machine",
				    "Machine class"),
/* 2*/	/* HW_MODEL: not yet supported */
/* 3*/	[HW_NCPU]		= MIB_INT(_P | _RO, CONFIG_MAX_CPUS,
				    "ncpu", "Number of CPUs configured"),
/* 4*/	[HW_BYTEORDER]		= MIB_INT(_P | _RO, BYTE_ORDER, "byteorder",
				    "System byte order"),
/* 5*/	[HW_PHYSMEM]		= MIB_FUNC(_P | _RO | CTLFLAG_UNSIGNED |
				    CTLTYPE_INT, sizeof(int), mib_hw_physmem,
				    "physmem", "Bytes of physical memory"),
/* 6*/	[HW_USERMEM]		= MIB_FUNC(_P | _RO | CTLFLAG_UNSIGNED |
				    CTLTYPE_INT, sizeof(int), mib_hw_usermem,
				    "usermem", "Bytes of non-kernel memory"),
/* 7*/	[HW_PAGESIZE]		= MIB_INT(_P | _RO, PAGE_SIZE, "pagesize",
				    "Software page size"),
/* 8*/	/* HW_DISKNAMES: not yet supported */
/* 9*/	/* HW_IOSTATS: not yet supported */
/*10*/	[HW_MACHINE_ARCH]	= MIB_STRING(_P | _RO, arch, "machine_arch",
				    "Machine CPU class"),
/*11*/	/* HW_ALIGNBYTES: not yet supported */
/*12*/	/* HW_CNMAGIC: not yet supported */
/*13*/	[HW_PHYSMEM64]		= MIB_FUNC(_P | _RO | CTLTYPE_QUAD,
				    sizeof(u_quad_t), mib_hw_physmem,
				    "physmem64", "Bytes of physical memory"),
/*14*/	[HW_USERMEM64]		= MIB_FUNC(_P | _RO | CTLTYPE_QUAD,
				    sizeof(u_quad_t), mib_hw_usermem,
				    "usermem64", "Bytes of non-kernel memory"),
/*15*/	/* HW_IOSTATNAMES: not yet supported */
/*16*/	[HW_NCPUONLINE]		= MIB_FUNC(_P | _RO | CTLTYPE_INT, sizeof(int),
				    mib_hw_ncpuonline, "ncpuonline",
				    "Number of CPUs online"),
};

/*
 * Initialize the CTL_HW subtree.
 */
void
mib_hw_init(struct mib_node * node)
{

	MIB_INIT_ENODE(node, mib_hw_table);
}
