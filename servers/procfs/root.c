/* ProcFS - root.c - by Alen Stojanov and David van Moolenbroek */

#include "inc.h"
#include <machine/pci.h>

FORWARD _PROTOTYPE( void root_hz, (void)				);
FORWARD _PROTOTYPE( void root_uptime, (void)				);
FORWARD _PROTOTYPE( void root_loadavg, (void)				);
FORWARD _PROTOTYPE( void root_kinfo, (void)				);
FORWARD _PROTOTYPE( void root_meminfo, (void)				);
FORWARD _PROTOTYPE( void root_pci, (void)				);

struct file root_files[] = {
	{ "hz",		REG_ALL_MODE,	(data_t) root_hz	},
	{ "uptime",	REG_ALL_MODE,	(data_t) root_uptime	},
	{ "loadavg",	REG_ALL_MODE,	(data_t) root_loadavg	},
	{ "kinfo",	REG_ALL_MODE,	(data_t) root_kinfo	},
	{ "meminfo",	REG_ALL_MODE,	(data_t) root_meminfo	},
	{ "pci",	REG_ALL_MODE,	(data_t) root_pci	},
	{ NULL,		0,		NULL			}
};

/*===========================================================================*
 *				root_hz					     *
 *===========================================================================*/
PRIVATE void root_hz(void)
{
	/* Print the system clock frequency.
	 */

	buf_printf("%lu\n", (long) sys_hz());
}

/*===========================================================================*
 *				root_loadavg				     *
 *===========================================================================*/
PRIVATE void root_loadavg(void)
{
	/* Print load averages.
	 */
	double avg[3];

	if (procfs_getloadavg(avg, 3) != 3)
		return;

	buf_printf("%.2lf %.2lf %.2lf\n", avg[0], avg[1], avg[2]);
}

/*===========================================================================*
 *				root_uptime				     *
 *===========================================================================*/
PRIVATE void root_uptime(void)
{
	/* Print the current uptime.
	 */
	clock_t ticks;

	if (getuptime(&ticks) != OK)
		return;

	buf_printf("%.2lf\n", (double) ticks / (double) sys_hz());
}

/*===========================================================================*
 *				root_kinfo				     *
 *===========================================================================*/
PRIVATE void root_kinfo(void)
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
PRIVATE void root_meminfo(void)
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
PRIVATE void root_pci(void)
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
