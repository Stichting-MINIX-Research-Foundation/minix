/*
minix/rs.h

Interface to the reincarnation server
*/

#define RSS_NR_IRQ		16
#define RSS_NR_IO		16
#define RSS_NR_PCI_ID		16
#define RSS_NR_PCI_CLASS	 4
#define RSS_NR_SYSTEM		 2

/* Arguments needed to start a new driver or server */
struct rs_start
{
	unsigned rss_flags;
	char *rss_cmd;
	size_t rss_cmdlen;
	uid_t rss_uid;
	int rss_nice;
	int rss_major;
	long rss_period;
	char *rss_script;
	size_t rss_scriptlen;
	int rss_nr_irq;
	int rss_irq[RSS_NR_IRQ];
	int rss_nr_io;
	struct { unsigned base; unsigned len; } rss_io[RSS_NR_IO];
	int rss_nr_pci_id;
	struct { u16_t vid; u16_t did; } rss_pci_id[RSS_NR_PCI_ID];
	int rss_nr_pci_class;
	struct { u32_t class; u32_t mask; } rss_pci_class[RSS_NR_PCI_CLASS];
	u32_t rss_system[RSS_NR_SYSTEM];
	char *rss_label;
	size_t rss_labellen;
	char *rss_ipc;
	size_t rss_ipclen;
};

#define RF_COPY		0x01	/* Copy the brinary into RS to make it possible
				 * to restart the driver without accessing FS
				 */
#define RF_IPC_VALID	0x02	/* rss_ipc and rss_ipclen are valid */

#define RSP_LABEL_SIZE	16
#define RSP_NR_DEVICE	16
#define RSP_NR_CLASS	 4

/* ACL information for access to PCI devices */
struct rs_pci
{
	char rsp_label[RSP_LABEL_SIZE];		/* Name of the driver */
	int rsp_endpoint;
	int rsp_nr_device;
	struct { u16_t vid; u16_t did; } rsp_device[RSP_NR_DEVICE];
	int rsp_nr_class;
	struct { u32_t class; u32_t mask; } rsp_class[RSP_NR_CLASS];
};

