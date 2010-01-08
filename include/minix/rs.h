#ifndef RS_H
#define RS_H

/*
minix/rs.h

Interface to the reincarnation server
*/

#include <minix/bitmap.h>
#include <minix/com.h>

/* RSS definitions. */
#define RSS_NR_IRQ		16
#define RSS_NR_IO		16

/* RSS flags. */
#define RSS_COPY	0x01	/* Copy the brinary into RS to make it possible
				 * to restart the driver without accessing FS
				 */
#define RSS_IPC_VALID	0x02	/* rss_ipc and rss_ipclen are valid */
#define RSS_REUSE	0x04	/* Try to reuse previously copied binary */

/* Common definitions. */
#define RS_SYS_CALL_MASK_SIZE	 2
#define RS_NR_CONTROL		 8
#define RS_NR_PCI_DEVICE	32
#define RS_NR_PCI_CLASS		 4
#define RS_MAX_LABEL_LEN	16

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
	struct { u16_t vid; u16_t did; } rss_pci_id[RS_NR_PCI_DEVICE];
	int rss_nr_pci_class;
	struct { u32_t class; u32_t mask; } rss_pci_class[RS_NR_PCI_CLASS];
	u32_t rss_system[RS_SYS_CALL_MASK_SIZE];
	struct rss_label rss_label;
	char *rss_ipc;
	size_t rss_ipclen;
	bitchunk_t rss_vm[VM_CALL_MASK_SIZE];
	int rss_nr_control;
	struct rss_label rss_control[RS_NR_CONTROL];
};

/* ACL information for access to PCI devices */
struct rs_pci
{
	char rsp_label[RS_MAX_LABEL_LEN];
	int rsp_endpoint;
	int rsp_nr_device;
	struct { u16_t vid; u16_t did; } rsp_device[RS_NR_PCI_DEVICE];
	int rsp_nr_class;
	struct { u32_t class; u32_t mask; } rsp_class[RS_NR_PCI_CLASS];
};

/* Definition of a public entry of the system process table. */
struct rprocpub {
  short in_use; 		  /* set when the entry is in use */
  unsigned sys_flags; 		  /* sys flags */
  endpoint_t endpoint;		  /* process endpoint number */

  dev_t dev_nr;			  /* major device number */
  int dev_style;		  /* device style */
  long period;			  /* heartbeat period (or zero) */

  char label[RS_MAX_LABEL_LEN];	  /* label of this service */
  char proc_name[RS_MAX_LABEL_LEN]; /* process name of this service */

  bitchunk_t vm_call_mask[VM_CALL_MASK_SIZE]; /* vm call mask */

  struct rs_pci pci_acl;	  /* pci acl */
};

_PROTOTYPE( int minix_rs_lookup, (const char *name, endpoint_t *value));

#endif
