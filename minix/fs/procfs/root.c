/* ProcFS - root.c - generators for static files in the root directory */

#include "inc.h"

#if defined (__i386__)
#include <machine/pci.h>
#endif
#include <minix/dmap.h>

static void root_hz(void);
static void root_uptime(void);
static void root_loadavg(void);
static void root_kinfo(void);
static void root_meminfo(void);
#if defined(__i386__)
static void root_pci(void);
#endif
static void root_dmap(void);
static void root_ipcvecs(void);
static void root_mounts(void);

struct file root_files[] = {
	{ "hz",		REG_ALL_MODE,	(data_t) root_hz	},
	{ "uptime",	REG_ALL_MODE,	(data_t) root_uptime	},
	{ "loadavg",	REG_ALL_MODE,	(data_t) root_loadavg	},
	{ "kinfo",	REG_ALL_MODE,	(data_t) root_kinfo	},
	{ "meminfo",	REG_ALL_MODE,	(data_t) root_meminfo	},
#if defined(__i386__)
	{ "pci",	REG_ALL_MODE,	(data_t) root_pci	},
#endif
	{ "dmap",	REG_ALL_MODE,	(data_t) root_dmap	},
#if defined(__i386__)
	{ "cpuinfo",	REG_ALL_MODE,	(data_t) root_cpuinfo	},
#endif
	{ "ipcvecs",	REG_ALL_MODE,	(data_t) root_ipcvecs	},
	{ "mounts",	REG_ALL_MODE,	(data_t) root_mounts	},
	{ NULL,		0,		NULL			}
};

/*
 * Print the system clock frequency.
 */
static void
root_hz(void)
{

	buf_printf("%lu\n", (unsigned long)sys_hz());
}

/*
 * Print load averages.
 */
static void
root_loadavg(void)
{
	struct load loads[3];
	ldiv_t avg[3];

	if (procfs_getloadavg(loads, 3) != 3)
		return;

	avg[0] = ldiv(100L * loads[0].proc_load / loads[0].ticks, 100);
	avg[1] = ldiv(100L * loads[1].proc_load / loads[1].ticks, 100);
	avg[2] = ldiv(100L * loads[2].proc_load / loads[2].ticks, 100);

	buf_printf("%ld.%02ld %ld.%02ld %ld.%02ld\n",
	    avg[0].quot, avg[0].rem, avg[1].quot, avg[1].rem,
	    avg[2].quot, avg[2].rem);
}

/*
 * Print the current uptime.
 */
static void
root_uptime(void)
{
	ldiv_t division;

	division = ldiv(100L * getticks() / sys_hz(), 100L);

	buf_printf("%ld.%0.2ld\n", division.quot, division.rem);
}

/*
 * Print general kernel information.
 */
static void
root_kinfo(void)
{
	struct kinfo kinfo;

	if (sys_getkinfo(&kinfo) != OK)
		return;

	buf_printf("%u %u\n", kinfo.nr_procs, kinfo.nr_tasks);
}

/*
 * Print general memory information.
 */
static void
root_meminfo(void)
{
	struct vm_stats_info vsi;

	if (vm_info_stats(&vsi) != OK)
		return;

	buf_printf("%u %lu %lu %lu %lu\n", vsi.vsi_pagesize, vsi.vsi_total,
	    vsi.vsi_free, vsi.vsi_largest, vsi.vsi_cached);
}

#if defined(__i386__)
/*
 * Print information about PCI devices present in the system.
 */
static void
root_pci(void)
{
	u16_t vid, did, subvid, subdid;
	u8_t bcr, scr, pifr, rev;
	char *slot_name, *dev_name;
	int r, devind;
	static int first = TRUE;

	/* This should be taken care of behind the scenes by the PCI lib. */
	if (first) {
		pci_init();
		first = FALSE;
	}

	/* Iterate over all devices, printing info for each of them. */
	r = pci_first_dev(&devind, &vid, &did);
	while (r == 1) {
		slot_name = pci_slot_name(devind);
		dev_name = pci_dev_name(vid, did);

		bcr = pci_attr_r8(devind, PCI_BCR);
		scr = pci_attr_r8(devind, PCI_SCR);
		pifr = pci_attr_r8(devind, PCI_PIFR);
		rev = pci_attr_r8(devind, PCI_REV);
		subvid = pci_attr_r16(devind, PCI_SUBVID);
		subdid = pci_attr_r16(devind, PCI_SUBDID);

		buf_printf("%s %x/%x/%x/%x %04X:%04X:%04X:%04X %s\n",
		    slot_name ? slot_name : "-1.-1.-1.-1",
		    bcr, scr, pifr, rev,
		    vid, did, subvid, subdid,
		    dev_name ? dev_name : "");

		r = pci_next_dev(&devind, &vid, &did);
	}
}
#endif /* defined(__i386__) */

/*
 * Print a list of drivers that have been assigned major device numbers.
 */
static void
root_dmap(void)
{
	struct dmap dmap[NR_DEVICES];
	int i;

	if (getsysinfo(VFS_PROC_NR, SI_DMAP_TAB, dmap, sizeof(dmap)) != OK)
		return;

	for (i = 0; i < NR_DEVICES; i++) {
		if (dmap[i].dmap_driver == NONE)
			continue;

		buf_printf("%u %s %u\n", i, dmap[i].dmap_label,
		    dmap[i].dmap_driver);
	}
}

/*
 * Print a list of IPC vectors with their addresses.
 */
static void
root_ipcvecs(void)
{
	extern struct minix_ipcvecs _minix_ipcvecs;

	/*
	 * Only print this if the kernel provides the info; otherwise binaries
	 * will be using their own in-libc vectors that are normal symbols in
	 * the binary.
	 */
	if (!(get_minix_kerninfo()->ki_flags & MINIX_KIF_IPCVECS))
		return;

	/*
	 * Print the vectors with an descriptive name and the additional (k)
	 * to distinguish them from regular symbols.
	 */
#define PRINT_ENTRYPOINT(name) \
	buf_printf("%08lx T %s(k)\n", \
	    (unsigned long)_minix_ipcvecs.name, #name)

	PRINT_ENTRYPOINT(sendrec);
	PRINT_ENTRYPOINT(send);
	PRINT_ENTRYPOINT(notify);
	PRINT_ENTRYPOINT(senda);
	PRINT_ENTRYPOINT(sendnb);
	PRINT_ENTRYPOINT(receive);
	PRINT_ENTRYPOINT(do_kernel_call);
}

/*
 * Print the list of mounted file systems.
 */
static void
root_mounts(void)
{
	struct statvfs buf[NR_MNTS];
	int i, count;

	if ((count = getvfsstat(buf, sizeof(buf), ST_NOWAIT)) < 0)
		return;

	for (i = 0; i < count; i++) {
		buf_printf("%s on %s type %s (%s)\n", buf[i].f_mntfromname,
		    buf[i].f_mntonname, buf[i].f_fstypename,
		    (buf[i].f_flag & ST_RDONLY) ? "ro" : "rw");
	}
}
