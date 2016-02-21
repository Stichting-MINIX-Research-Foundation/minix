#ifndef RS_H
#define RS_H

/*
minix/rs.h

Interface to the reincarnation server
*/

#include <minix/bitmap.h>
#include <minix/com.h>
#include <minix/ipc_filter.h>

#define SERVICE_LOGIN	"service"	/* passwd file entry for services */

/* The following definition should be kept in sync with the actual
 * /etc/master.passwd value for SERVICE_LOGIN for now, and removed altogether
 * once we are able to obtain its value dynamically everywhere.  The value has
 * been chosen so as to avoid creating conflicts with future NetBSD additions
 * to the password files, although one can never be sure.
 */
#define SERVICE_UID		999		/* user ID for services */

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
#define RSS_BATCH	0x20	/* batch mode */
#define RSS_SELF_LU	0x40	/* perform self update */
#define RSS_ASR_LU	0x80	/* perform ASR update */
#define RSS_FORCE_SELF_LU	0x0100	/* force self update */
#define RSS_PREPARE_ONLY_LU	0x0200	/* request prepare-only update */
#define RSS_FORCE_INIT_CRASH    0x0400  /* force crash at initialization time (for debugging) */
#define RSS_FORCE_INIT_FAIL     0x0800  /* force failure at initialization time (for debugging) */
#define RSS_FORCE_INIT_TIMEOUT  0x1000  /* force timeout at initialization time (for debugging) */
#define RSS_FORCE_INIT_DEFCB    0x2000  /* force default cb at initialization time (for debugging) */
#define RSS_SYS_BASIC_CALLS	0x4000	/* include basic kernel calls */
#define RSS_VM_BASIC_CALLS	0x8000	/* include basic vm calls */
#define RSS_NOMMAP_LU          0x10000  /* don't inherit mmapped regions */
#define RSS_DETACH             0x20000  /* detach on update/restart */
#define RSS_NORESTART          0x40000  /* don't restart */
#define RSS_FORCE_INIT_ST      0x80000  /* force state transfer at initialization time */
#define RSS_NO_BIN_EXP        0x100000  /* suppress binary exponential offset */

/* Common definitions. */
#define RS_NR_CONTROL		 8
#define RS_NR_PCI_DEVICE	32
#define RS_NR_PCI_CLASS		 4
#define RS_MAX_LABEL_LEN	16
#define RS_MAX_IPCF_STR_LEN	 (RS_MAX_LABEL_LEN+12)
#define RS_MAX_IPC_FILTERS	 4

/* CPU special values */
#define RS_CPU_DEFAULT		-1 /* use the default cpu or do not change the current one */
#define RS_CPU_BSP		-2 /* use the bootstrap cpu */

/* Labels are copied over separately. */
struct rss_label
{
	char *l_addr;
	size_t l_len;
};

struct rs_pci_id {
	u16_t vid;
	u16_t did;
	u16_t sub_vid;
	u16_t sub_did;
};
#define NO_SUB_VID	0xffff
#define NO_SUB_DID	0xffff

struct rs_pci_class {
	u32_t pciclass;
	u32_t mask;
};

/* State-related data. */
struct rs_ipc_filter_el {
	int flags;
	char m_label[RS_MAX_LABEL_LEN];
	int m_type;
};
struct rs_state_data {
	size_t size;
	void *ipcf_els;
	size_t ipcf_els_size;
	int ipcf_els_gid;
	void *eval_addr;
	size_t eval_len;
	int eval_gid;
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
	long rss_period;
	char *rss_script;
	size_t rss_scriptlen;
	long rss_asr_count;
	long rss_restarts;
	long rss_heap_prealloc_bytes;
	long rss_map_prealloc_bytes;
	int rss_nr_irq;
	int rss_irq[RSS_NR_IRQ];
	int rss_nr_io;
	struct { unsigned base; unsigned len; } rss_io[RSS_NR_IO];
	int rss_nr_pci_id;
	struct rs_pci_id rss_pci_id[RS_NR_PCI_DEVICE];
	int rss_nr_pci_class;
	struct rs_pci_class rss_pci_class[RS_NR_PCI_CLASS];
	bitchunk_t rss_system[SYS_CALL_MASK_SIZE];
	struct rss_label rss_label;
	struct rss_label rss_trg_label;
	char *rss_ipc;
	size_t rss_ipclen;
	bitchunk_t rss_vm[VM_CALL_MASK_SIZE];
	int rss_nr_control;
	struct rss_label rss_control[RS_NR_CONTROL];
	struct rs_state_data rss_state_data;
	int devman_id;
	char *rss_progname;
	size_t rss_prognamelen;
	int rss_nr_domain;
	int rss_domain[NR_DOMAIN];
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
	struct rs_pci_id rsp_device[RS_NR_PCI_DEVICE];
	int rsp_nr_class;
	struct rs_pci_class rsp_class[RS_NR_PCI_CLASS];
};

/* Definition of a public entry of the system process table. */
struct rprocpub {
  short in_use; 		  /* set when the entry is in use */
  unsigned sys_flags; 		  /* sys flags */
  endpoint_t endpoint;		  /* process endpoint number */
  endpoint_t old_endpoint;	  /* old instance endpoint number (for VM, when updating) */
  endpoint_t new_endpoint;	  /* new instance endpoint number (for VM, when updating) */

  devmajor_t dev_nr;		  /* major device number or NO_DEV */
  int nr_domain;		  /* number of socket driver domains */
  int domain[NR_DOMAIN];	  /* set of socket driver domains */

  char label[RS_MAX_LABEL_LEN];	  /* label of this service */
  char proc_name[RS_MAX_LABEL_LEN]; /* process name of this service */

  bitchunk_t vm_call_mask[VM_CALL_MASK_SIZE]; /* vm call mask */

  struct rs_pci pci_acl;	  /* pci acl */
  int devman_id;
};

/* Return whether the given boot process is a user process, as opposed to a
 * system process. Only usable by core services during SEF initialization.
 */
#define IS_RPUB_BOOT_USR(rpub) ((rpub)->endpoint == INIT_PROC_NR)

/* Sys flag values. */
#define SF_CORE_SRV     0x001    /* set for core system services */
#define SF_SYNCH_BOOT   0X002    /* set when process needs synch boot init */
#define SF_NEED_COPY    0x004    /* set when process needs copy to start */
#define SF_USE_COPY     0x008    /* set when process has a copy in memory */
#define SF_NEED_REPL    0x010    /* set when process needs replica to start */
#define SF_USE_REPL     0x020    /* set when process has a replica */
#define SF_VM_UPDATE    0x040    /* set when process needs vm update */
#define SF_VM_ROLLBACK  0x080    /* set when vm update is a rollback */
#define SF_VM_NOMMAP    0x100    /* set when vm update ignores mmapped regions */
#define SF_USE_SCRIPT   0x200    /* set when process has restart script */
#define SF_DET_RESTART  0x400    /* set when process detaches on restart */
#define SF_NORESTART    0x800    /* set when process should not be restarted */
#define SF_NO_BIN_EXP  0x1000    /* set when we should ignore binary exp. offset */

#define IMM_SF          \
    (SF_NO_BIN_EXP | SF_CORE_SRV | SF_SYNCH_BOOT | SF_NEED_COPY | SF_NEED_REPL) /* immutable */

int minix_rs_lookup(const char *name, endpoint_t *value);

#endif
