/* ProcFS - root.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"

#if defined (__i386__)
#include <machine/pci.h>
#endif
#include <minix/dmap.h>
#include "cpuinfo.h"
#include "mounts.h"

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

/*===========================================================================*
 *				root_hz					     *
 *===========================================================================*/
static void root_hz(void)
{
	/* Print the system clock frequency.
	 */

	buf_printf("%lu\n", (long) sys_hz());
}

/*===========================================================================*
 *				root_loadavg				     *
 *===========================================================================*/
static void root_loadavg(void)
{
	/* Print load averages.
	 */
	struct load loads[3];
	ldiv_t avg[3];

	if (procfs_getloadavg(loads, 3) != 3)
		return;

	avg[0] = ldiv(100L * loads[0].proc_load / loads[0].ticks, 100);
	avg[1] = ldiv(100L * loads[1].proc_load / loads[1].ticks, 100);
	avg[2] = ldiv(100L * loads[2].proc_load / loads[2].ticks, 100);

	buf_printf("%ld.%0.2ld %ld.%02ld %ld.%02ld\n",
		avg[0].quot, avg[0].rem, avg[1].quot, avg[1].rem,
		avg[2].quot, avg[2].rem);
}

/*===========================================================================*
 *				root_uptime				     *
 *===========================================================================*/
static void root_uptime(void)
{
	/* Print the current uptime.
	 */
	clock_t ticks;
	ldiv_t division;

	if (getticks(&ticks) != OK)
		return;
	division = ldiv(100L * ticks / sys_hz(), 100L);

	buf_printf("%ld.%0.2ld\n", division.quot, division.rem);
}

/*===========================================================================*
 *				root_kinfo				     *
 *===========================================================================*/
static void root_kinfo(void)
{
	/* Print general kernel information.
	 */
	struct kinfo kinfo;

	if (sys_getkinfo(&kinfo) != OK)
		return;

	buf_printf("%u %u\n", kinfo.nr_procs, kinfo.nr_tasks);
}

/*===========================================================================*
 *				root_meminfo				     *
 *===========================================================================*/
static void root_meminfo(void)
{
	/* Print general memory information.
	 */
	struct vm_stats_info vsi;

	if (vm_info_stats(&vsi) != OK)
		return;

	buf_printf("%u %lu %lu %lu %lu\n", vsi.vsi_pagesize,
		vsi.vsi_total, vsi.vsi_free, vsi.vsi_largest, vsi.vsi_cached);
}

/*===========================================================================*
 *				root_pci				     *
 *===========================================================================*/
#if defined(__i386__)
static void root_pci(void)
{
	/* Print information about PCI devices present in the system.
	 */
	u16_t vid, did;
	u8_t bcr, scr, pifr;
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

		buf_printf("%s %x/%x/%x %04X:%04X %s\n",
			slot_name ? slot_name : "-",
			bcr, scr, pifr, vid, did,
			dev_name ? dev_name : "");

		r = pci_next_dev(&devind, &vid, &did);
	}
}
#endif /* defined(__i386__) */

/*===========================================================================*
 *				root_dmap				     *
 *===========================================================================*/
static void root_dmap(void)
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

/*===========================================================================*
 *				root_ipcvecs				     *
 *===========================================================================*/
static void root_ipcvecs(void)
{
	extern struct minix_kerninfo *_minix_kerninfo;
	extern struct minix_ipcvecs _minix_ipcvecs;

	/* only print this if the kernel provides the info; otherwise binaries
	 * will be using their own in-libc vectors that are normal symbols in the
	 * binary.
	 */
	if(!_minix_kerninfo || !(_minix_kerninfo->ki_flags & MINIX_KIF_IPCVECS))
		return;

	/* print the vectors with an descriptive name and the additional (k)
	 * to distinguish them from regular symbols.
	 */
#define PRINT_ENTRYPOINT(name) \
	buf_printf("%08lx T %s(k)\n", _minix_ipcvecs.name, #name)

	PRINT_ENTRYPOINT(sendrec);
	PRINT_ENTRYPOINT(send);
	PRINT_ENTRYPOINT(notify);
	PRINT_ENTRYPOINT(senda);
	PRINT_ENTRYPOINT(sendnb);
	PRINT_ENTRYPOINT(receive);
	PRINT_ENTRYPOINT(do_kernel_call);
}

