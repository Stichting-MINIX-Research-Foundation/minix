#ifndef RS_H
#define RS_H

/*
minix/rs.h

Interface to the reincarnation server
*/

#include <minix/bitmap.h>
#include <minix/com.h>

#define SERVICE_LOGIN	"service"	/* passwd file entry for services */

/* RSS definitions. */
#define RSS_NR_IRQ		16
#define RSS_NR_IO		16
#define RSS_IRQ_ALL		(RSS_NR_IRQ+1)
#define RSS_IO_ALL		(RSS_NR_IO+1)
#define RSS_IPC_ALL		"IPC_ALL"
#define RSS_IPC_ALL_SYS		"IPC_ALL_SYS"

/* RSS flags. */
#define RSS_COPY	0x01	/* keep an in-memory copy of the binary */
#define RSS_REUSE	0x04	/* Try to reuse previously copied binary */
#define RSS_NOBLOCK	0x08	/* unblock caller immediately */
#define RSS_REPLICA	0x10	/* keep a replica of the service */
#define RSS_SELF_LU	0x20	/* perform self update */
#define RSS_SYS_BASIC_CALLS	0x40	/* include basic kernel calls */
#define RSS_VM_BASIC_CALLS	0x80	/* include basic vm calls */
#define RSS_NO_BIN_EXP	0x100	/* suppress binary exponential offset */

/* Common definitions. */
#define RS_NR_CONTROL		 8
#define RS_NR_PCI_DEVICE	32
#define RS_NR_PCI_CLASS		 4
#define RS_MAX_LABEL_LEN	16

/* CPU special values */
#define RS_CPU_DEFAULT		-1 /* use the default cpu or do not change the current one */
#define RS_CPU_BSP		-2 /* use the bootstrap cpu */

/* Labels are copied over separately. */
struct rss_label
{
	char *l_addr;
	size_t l_len;
};

/* Arguments needed to start a new driver or server */
struct rs_start
{
	unsigned rss_flags;
	char *rss_cmd;
	size_t rss_cmdlen;
	uid_t rss_uid;
	endpoint_t rss_sigmgr;
	endpoint_t rss_scheduler;
	int rss_priority;
	int rss_quantum;
	int rss_major;
	int rss_dev_style;
	long rss_period;
	char *rss_script;
	size_t rss_scriptlen;
	int rss_nr_irq;
	int rss_irq[RSS_NR_IRQ];
	int rss_nr_io;
	struct { unsigned base; unsigned len; } rss_io[RSS_NR_IO];
	int rss_nr_pci_id;
	struct { u16_t vid; u16_t did; } rss_pci_id[RS_NR_PCI_DEVICE];
	int rss_nr_pci_class;
	struct { u32_t pciclass; u32_t mask; } rss_pci_class[RS_NR_PCI_CLASS];
	bitchunk_t rss_system[SYS_CALL_MASK_SIZE];
	struct rss_label rss_label;
	char *rss_ipc;
	size_t rss_ipclen;
	bitchunk_t rss_vm[VM_CALL_MASK_SIZE];
	int rss_nr_control;
	struct rss_label rss_control[RS_NR_CONTROL];
	int devman_id;
	/*
	 * SMP specific data
	 *
	 * must be at the end of the structure for binary compatibility with
	 * non-smp sysytems
	 */
	int rss_cpu;
};

/* ACL information for access to PCI devices */
struct rs_pci
{
	char rsp_label[RS_MAX_LABEL_LEN];
	int rsp_endpoint;
	int rsp_nr_device;
	struct { u16_t vid; u16_t did; } rsp_device[RS_NR_PCI_DEVICE];
	int rsp_nr_class;
	struct { u32_t pciclass; u32_t mask; } rsp_class[RS_NR_PCI_CLASS];
};

/* Definition of a public entry of the system process table. */
struct rprocpub {
  short in_use; 		  /* set when the entry is in use */
  unsigned sys_flags; 		  /* sys flags */
  endpoint_t endpoint;		  /* process endpoint number */

  int dev_flags;		  /* device flags */
  dev_t dev_nr;			  /* major device number */
  int dev_style;		  /* device style */
  int dev_style2;		  /* device style for next major dev number */

  char label[RS_MAX_LABEL_LEN];	  /* label of this service */
  char proc_name[RS_MAX_LABEL_LEN]; /* process name of this service */

  bitchunk_t vm_call_mask[VM_CALL_MASK_SIZE]; /* vm call mask */

  struct rs_pci pci_acl;	  /* pci acl */
  int devman_id;
};

int minix_rs_lookup(const char *name, endpoint_t *value);

#endif
