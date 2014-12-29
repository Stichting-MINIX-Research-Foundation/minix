/* ProcFS - service.c - the service subdirectory */

#include "inc.h"

#include <minix/rs.h>
#include "rs/const.h"
#include "rs/type.h"

enum policy {
	POL_NONE	= 0x00,	/*     user	| endpoint	*/
	POL_RESET	= 0x01,	/* visible	|  change	*/
	POL_RESTART	= 0x02,	/* transparent	| preserved	*/
	POL_LIVE_UPDATE	= 0x04	/* transparent	| preserved	*/
};

struct policies {
	#define MAX_POL_FORMAT_SZ 20
	char formatted[MAX_POL_FORMAT_SZ];
	enum policy supported;
};

static struct rprocpub rprocpub[NR_SYS_PROCS];
static struct rproc rproc[NR_SYS_PROCS];
static struct policies policies[NR_SYS_PROCS];

static struct inode *service_node;

/* Updates the policies state from RS. Always returns an ASCIIZ string.  */
static const char *
service_get_policies(struct policies * pol, index_t slot)
{
#if 1 /* The following should be retrieved from RS and formated instead. */
	int pos;
	char *ref_label;
	static const struct {
		const char *label;
		const char *policy_str;
	} def_pol[] = {
		/* audio */
                { .label = "es1370", .policy_str = "reset" },
                { .label = "es1371", .policy_str = "reset" },
                { .label = "sb16", .policy_str = "reset" },
		/* bus */
                { .label = "i2c", .policy_str = "restart" },
                { .label = "pci", .policy_str = "restart" },
                { .label = "ti1225", .policy_str = "restart" },
		/* clock */
                { .label = "readclock.drv", .policy_str = "restart" },
		/* eeprom */
                { .label = "cat24c256", .policy_str = "restart" },
		/* examples */
                { .label = "hello", .policy_str = "restart" },
		/* hid */
                { .label = "pckbd", .policy_str = "reset" },
		/* iommu */
                { .label = "amddev", .policy_str = "" },
		/* net */
		{ .label = "3c90x", .policy_str = "restart" },
                { .label = "atl2", .policy_str = "restart" },
                { .label = "dec21140A", .policy_str = "restart" },
                { .label = "dp8390", .policy_str = "restart" },
                { .label = "dpeth", .policy_str = "restart" },
                { .label = "e1000", .policy_str = "restart" },
                { .label = "fxp", .policy_str = "restart" },
                { .label = "lance", .policy_str = "restart" },
                { .label = "lan8710a", .policy_str = "restart" },
                { .label = "orinoco", .policy_str = "restart" },
                { .label = "rtl8139", .policy_str = "restart" },
                { .label = "rtl8169", .policy_str = "restart" },
                { .label = "uds", .policy_str = "reset" },
                { .label = "virtio_net", .policy_str = "restart" },
		/* power */
                { .label = "acpi", .policy_str = "" },
                { .label = "tps65217", .policy_str = "" },
                { .label = "tps65590", .policy_str = "" },
		/* printer */
                { .label = "printer", .policy_str = "restart" },
		/* sensors */
                { .label = "bmp085", .policy_str = "" },
                { .label = "sht21", .policy_str = "restart" },
                { .label = "tsl2550", .policy_str = "restart" },
		/* storage */
                { .label = "ahci", .policy_str = "reset" },
                { .label = "at_wini", .policy_str = "reset" },
                { .label = "fbd", .policy_str = "reset" },
                { .label = "filter", .policy_str = "reset" },
                { .label = "floppy", .policy_str = "reset" },
                { .label = "memory", .policy_str = "restart" },
                { .label = "mmc", .policy_str = "reset" },
                { .label = "virtio_blk", .policy_str = "reset" },
                { .label = "vnd", .policy_str = "reset" },
		/* system */
                { .label = "gpio", .policy_str = "restart" },
                { .label = "log", .policy_str = "restart" },
                { .label = "random", .policy_str = "restart" },
		/* tty */
                { .label = "pty", .policy_str = "restart" },
                { .label = "tty", .policy_str = "" },
		/* usb */
                { .label = "usbd", .policy_str = "" },
                { .label = "usb_hub", .policy_str = "" },
                { .label = "usb_storage", .policy_str = "" },
		/* video */
                { .label = "fb", .policy_str = "" },
                { .label = "tda19988", .policy_str = "" },
		/* vmm_guest */
                { .label = "vbox", .policy_str = "" },
		/* fs */
                { .label = "ext2", .policy_str = "" },
                { .label = "hgfs", .policy_str = "" },
                { .label = "isofs", .policy_str = "" },
                { .label = "mfs", .policy_str = "" },
                { .label = "pfs", .policy_str = "" },
                { .label = "procfs", .policy_str = "" },
                { .label = "vbfs", .policy_str = "" },
		/* net */
                { .label = "inet", .policy_str = "reset" },
                { .label = "lwip", .policy_str = "" },
		/* servers */
                { .label = "devman", .policy_str = "" },
                { .label = "ds", .policy_str = "" },
                { .label = "input", .policy_str = "reset" },
                { .label = "ipc", .policy_str = "restart" },
                { .label = "is", .policy_str = "restart" },
                { .label = "pm", .policy_str = "" },
                { .label = "rs", .policy_str = "" },
                { .label = "sched", .policy_str = "" },
                { .label = "vfs", .policy_str = "" },
                { .label = "vm", .policy_str = "" },
		//{ .label = "", .policy_str = "" },
	};

	/* Find the related policy, based on the file name of the service. */
	ref_label = strrchr(rprocpub[slot].proc_name, '/');
	if (NULL == ref_label)
		ref_label = rprocpub[slot].proc_name;

	memset(pol[slot].formatted, 0, MAX_POL_FORMAT_SZ);
	for(pos = 0; pos < (sizeof(def_pol) / sizeof(def_pol[0])); pos++) {
		if (0 == strcmp(ref_label, def_pol[pos].label)) {
			(void)strncpy(pol[slot].formatted, def_pol[pos].policy_str, MAX_POL_FORMAT_SZ);
			pol[slot].formatted[MAX_POL_FORMAT_SZ-1] = '\0';
			break;
		}
	}
#else
	/* Should do something sensible, based on flags from RS/SEF. */
#endif

	return pol[slot].formatted;
}

/*
 * Update the contents of the service directory, by first updating the RS
 * tables and then updating the directory contents.
 */
static void
service_update(void)
{
	struct inode *node;
	struct inode_stat stat;
	index_t slot;

	/* There is not much we can do if either of these calls fails. */
	(void)getsysinfo(RS_PROC_NR, SI_PROCPUB_TAB, rprocpub,
	    sizeof(rprocpub));
	(void)getsysinfo(RS_PROC_NR, SI_PROC_TAB, rproc, sizeof(rproc));

	/*
	 * As with PIDs, we make two passes.  Delete first, then add.  This
	 * prevents problems in the hypothetical case that between updates, one
	 * slot ends up with the label name of a previous, different slot.
	 */
	for (slot = 0; slot < NR_SYS_PROCS; slot++) {
		if ((node = get_inode_by_index(service_node, slot)) == NULL)
			continue;

		/*
		 * If the slot is no longer in use, or the label name does not
		 * match, the node must be deleted.
		 */
		if (!(rproc[slot].r_flags & RS_IN_USE) ||
		    strcmp(get_inode_name(node), rprocpub[slot].label))
			delete_inode(node);
	}

	memset(&stat, 0, sizeof(stat));
	stat.mode = REG_ALL_MODE;
	stat.uid = SUPER_USER;
	stat.gid = SUPER_USER;

	for (slot = 0; slot < NR_SYS_PROCS; slot++) {
		if (!(rproc[slot].r_flags & RS_IN_USE) ||
		    get_inode_by_index(service_node, slot) != NULL)
			continue;

		node = add_inode(service_node, rprocpub[slot].label, slot,
		    &stat, (index_t)0, (cbdata_t)slot);

		if (node == NULL)
			out_of_inodes();
	}
}

/*
 * Initialize the service directory.
 */
void
service_init(void)
{
	struct inode *root, *node;
	struct inode_stat stat;

	root = get_root_inode();

	memset(&stat, 0, sizeof(stat));
	stat.mode = DIR_ALL_MODE;
	stat.uid = SUPER_USER;
	stat.gid = SUPER_USER;

	service_node = add_inode(root, "service", NO_INDEX, &stat,
	    NR_SYS_PROCS, NULL);

	if (service_node == NULL)
		panic("unable to create service node");
}

/*
 * A lookup request is being performed.  If it is in the service directory,
 * update the tables.  We do this lazily, to reduce overhead.
 */
void
service_lookup(struct inode * parent, clock_t now)
{
	static clock_t last_update = 0;

	if (parent != service_node)
		return;

	if (last_update != now) {
		service_update();

		last_update = now;
	}
}

/*
 * A getdents request is being performed.  If it is in the service directory,
 * update the tables.
 */
void
service_getdents(struct inode * node)
{

	if (node != service_node)
		return;

	service_update();
}

/*
 * A read request is being performed.  If it is on a file in the service
 * directory, process the read request.  We rely on the fact that any read
 * call will have been preceded by a lookup, so its table entry has been
 * updated very recently.
 */
void
service_read(struct inode * node)
{
	struct inode *parent;
	index_t slot;
	struct rprocpub *rpub;
	struct rproc *rp;

	if (get_parent_inode(node) != service_node)
		return;

	slot = get_inode_index(node);
	rpub = &rprocpub[slot];
	rp = &rproc[slot];

	/* TODO: add a large number of other fields! */
	buf_printf("filename: %s\n", rpub->proc_name);
	buf_printf("endpoint: %d\n", rpub->endpoint);
	buf_printf("restarts: %d\n", rp->r_restarts);
	buf_printf("policies: %s\n", service_get_policies(policies, slot));
}
