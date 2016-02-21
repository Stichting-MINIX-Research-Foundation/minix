
#define _MINIX_SYSTEM 1

#include <stdarg.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <err.h>
#include <unistd.h>
#include <limits.h>
#include <lib.h>
#include <minix/config.h>
#include <minix/com.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/rs.h>
#include <minix/syslib.h>
#include <minix/bitmap.h>
#include <minix/paths.h>
#include <minix/sef.h>
#include <minix/dmap.h>
#include <minix/priv.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <configfile.h>

#include <machine/archtypes.h>
#include <minix/timers.h>

#include "config.h"
#include "proto.h"

static int class_recurs;       /* Nesting level of class statements */
#define MAX_CLASS_RECURS        100     /* Max nesting level for classes */

#include "parse.h"

static void do_service(config_t *cpe, config_t *config, struct rs_config *);

static void do_class(config_t *cpe, config_t *config, struct rs_config *rs_config)
{
	config_t *cp, *cp1;

	if (class_recurs > MAX_CLASS_RECURS)
	{
		fatal(
		"do_class: nesting level too high for class '%s' at %s:%d",
			cpe->word, cpe->file, cpe->line);
	}
	class_recurs++;

	/* Process classes */
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_class: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_uid: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}

		/* Find entry for the class */
		for (cp= config; cp; cp= cp->next)
		{
			if (!(cp->flags & CFG_SUBLIST))
			{
				fatal("do_class: expected list at %s:%d",
					cp->file, cp->line);
			}
			cp1= cp->list;
			if ((cp1->flags & CFG_STRING) ||
				(cp1->flags & CFG_SUBLIST))
			{
				fatal("do_class: expected word at %s:%d",
					cp1->file, cp1->line);
			}

			/* At this place we expect the word KW_SERVICE */
			if (strcmp(cp1->word, KW_SERVICE) != 0)
				fatal("do_class: exected word '%S' at %s:%d",
					KW_SERVICE, cp1->file, cp1->line);

			cp1= cp1->next;
			if ((cp1->flags & CFG_STRING) ||
				(cp1->flags & CFG_SUBLIST))
			{
				fatal("do_class: expected word at %s:%d",
					cp1->file, cp1->line);
			}

			/* At this place we expect the name of the service */
			if (strcmp(cp1->word, cpe->word) == 0)
				break;
		}
		if (cp == NULL)
		{
			fatal(
			"do_class: no entry found for class '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		}
		do_service(cp1->next, config, rs_config);
	}

	class_recurs--;
}

static void do_uid(config_t *cpe, struct rs_start *rs_start)
{
	uid_t uid;
	struct passwd *pw;
	char *check;

	/* Process a uid */
	if (cpe->next != NULL)
	{
		fatal("do_uid: just one uid/login expected at %s:%d",
			cpe->file, cpe->line);
	}	

	if (cpe->flags & CFG_SUBLIST)
	{
		fatal("do_uid: unexpected sublist at %s:%d",
			cpe->file, cpe->line);
	}
	if (cpe->flags & CFG_STRING)
	{
		fatal("do_uid: unexpected string at %s:%d",
			cpe->file, cpe->line);
	}
	pw= getpwnam(cpe->word);
	if (pw != NULL)
		uid= pw->pw_uid;
	else
	{
		if (!strncmp(cpe->word, KW_SELF, strlen(KW_SELF)+1))
		{
			uid= getuid();	/* Real uid */
		}
		else {
			uid= strtol(cpe->word, &check, 0);
			if (check[0] != '\0')
			{
				fatal("do_uid: bad uid/login '%s' at %s:%d",
					cpe->word, cpe->file, cpe->line);
			}
		}
	}

	rs_start->rss_uid= uid;
}

static void do_sigmgr(config_t *cpe, struct rs_start *rs_start)
{
	endpoint_t sigmgr_ep;
	int r;

	/* Process a signal manager value */
	if (cpe->next != NULL)
	{
		fatal("do_sigmgr: just one sigmgr value expected at %s:%d",
			cpe->file, cpe->line);
	}	
	

	if (cpe->flags & CFG_SUBLIST)
	{
		fatal("do_sigmgr: unexpected sublist at %s:%d",
			cpe->file, cpe->line);
	}
	if (cpe->flags & CFG_STRING)
	{
		fatal("do_sigmgr: unexpected string at %s:%d",
			cpe->file, cpe->line);
	}

	if(!strcmp(cpe->word, "SELF")) {
		sigmgr_ep = SELF;
	}
	else {
		if((r = minix_rs_lookup(cpe->word, &sigmgr_ep))) {
			fatal("do_sigmgr: unknown sigmgr %s at %s:%d",
			cpe->word, cpe->file, cpe->line);
		}
	}

	rs_start->rss_sigmgr= sigmgr_ep;
}

static void do_type(config_t *cpe, struct rs_config *rs_config)
{
	if (cpe->next != NULL)
	{
		fatal("do_type: just one type value expected at %s:%d",
			cpe->file, cpe->line);
	}	
	

	if (cpe->flags & CFG_SUBLIST)
	{
		fatal("do_type: unexpected sublist at %s:%d",
			cpe->file, cpe->line);
	}
	if ((cpe->flags & CFG_STRING))
	{
		fatal("do_type: unexpected string at %s:%d",
			cpe->file, cpe->line);
	}

	if(rs_config->type)
		fatal("do_type: another type at %s:%d",
			cpe->file, cpe->line);

	if(!strcmp(cpe->word, KW_NET))
		rs_config->type = KW_NET;
	else
		fatal("do_type: odd type at %s:%d",
			cpe->file, cpe->line);
}

static void do_descr(config_t *cpe, struct rs_config *rs_config)
{
	if (cpe->next != NULL)
	{
		fatal("do_descr: just one description expected at %s:%d",
			cpe->file, cpe->line);
	}	
	

	if (cpe->flags & CFG_SUBLIST)
	{
		fatal("do_descr: unexpected sublist at %s:%d",
			cpe->file, cpe->line);
	}
	if (!(cpe->flags & CFG_STRING))
	{
		fatal("do_descr: expected string at %s:%d",
			cpe->file, cpe->line);
	}

	if(rs_config->descr)
		fatal("do_descr: another descr at %s:%d",
			cpe->file, cpe->line);
	rs_config->descr = cpe->word;
}

static void do_scheduler(config_t *cpe, struct rs_start *rs_start)
{
	endpoint_t scheduler_ep;
	int r;

	/* Process a scheduler value */
	if (cpe->next != NULL)
	{
		fatal("do_scheduler: just one scheduler value expected at %s:%d",
			cpe->file, cpe->line);
	}	
	

	if (cpe->flags & CFG_SUBLIST)
	{
		fatal("do_scheduler: unexpected sublist at %s:%d",
			cpe->file, cpe->line);
	}
	if (cpe->flags & CFG_STRING)
	{
		fatal("do_scheduler: unexpected string at %s:%d",
			cpe->file, cpe->line);
	}

	if(!strcmp(cpe->word, "KERNEL")) {
		scheduler_ep = KERNEL;
	}
	else {
		if((r = minix_rs_lookup(cpe->word, &scheduler_ep))) {
			fatal("do_scheduler: unknown scheduler %s at %s:%d",
			cpe->word, cpe->file, cpe->line);
		}
	}

	rs_start->rss_scheduler= scheduler_ep;
}

static void do_priority(config_t *cpe, struct rs_start *rs_start)
{
	int priority_val;
	char *check;

	/* Process a priority value */
	if (cpe->next != NULL)
	{
		fatal("do_priority: just one priority value expected at %s:%d",
			cpe->file, cpe->line);
	}	
	

	if (cpe->flags & CFG_SUBLIST)
	{
		fatal("do_priority: unexpected sublist at %s:%d",
			cpe->file, cpe->line);
	}
	if (cpe->flags & CFG_STRING)
	{
		fatal("do_priority: unexpected string at %s:%d",
			cpe->file, cpe->line);
	}
	priority_val= strtol(cpe->word, &check, 0);
	if (check[0] != '\0')
	{
		fatal("do_priority: bad priority value '%s' at %s:%d",
			cpe->word, cpe->file, cpe->line);
	}

	if (priority_val < 0 || priority_val >= NR_SCHED_QUEUES)
	{
		fatal("do_priority: priority %d out of range at %s:%d",
			priority_val, cpe->file, cpe->line);
	}
	rs_start->rss_priority= priority_val;
}

static void do_quantum(config_t *cpe, struct rs_start *rs_start)
{
	int quantum_val;
	char *check;

	/* Process a quantum value */
	if (cpe->next != NULL)
	{
		fatal("do_quantum: just one quantum value expected at %s:%d",
			cpe->file, cpe->line);
	}	
	

	if (cpe->flags & CFG_SUBLIST)
	{
		fatal("do_quantum: unexpected sublist at %s:%d",
			cpe->file, cpe->line);
	}
	if (cpe->flags & CFG_STRING)
	{
		fatal("do_quantum: unexpected string at %s:%d",
			cpe->file, cpe->line);
	}
	quantum_val= strtol(cpe->word, &check, 0);
	if (check[0] != '\0')
	{
		fatal("do_quantum: bad quantum value '%s' at %s:%d",
			cpe->word, cpe->file, cpe->line);
	}

	if (quantum_val <= 0)
	{
		fatal("do_quantum: quantum %d out of range at %s:%d",
			quantum_val, cpe->file, cpe->line);
	}
	rs_start->rss_quantum= quantum_val;
}

static void do_cpu(config_t *cpe, struct rs_start *rs_start)
{
	int cpu;
	char *check;

	/* Process a quantum value */
	if (cpe->next != NULL)
	{
		fatal("do_cpu: just one value expected at %s:%d",
			cpe->file, cpe->line);
	}	
	

	if (cpe->flags & CFG_SUBLIST)
	{
		fatal("do_cpu: unexpected sublist at %s:%d",
			cpe->file, cpe->line);
	}
	if (cpe->flags & CFG_STRING)
	{
		fatal("do_cpu: unexpected string at %s:%d",
			cpe->file, cpe->line);
	}
	cpu= strtol(cpe->word, &check, 0);
	if (check[0] != '\0')
	{
		fatal("do_cpu: bad value '%s' at %s:%d",
			cpe->word, cpe->file, cpe->line);
	}

	if (cpu < 0)
	{
		fatal("do_cpu: %d out of range at %s:%d",
			cpu, cpe->file, cpe->line);
	}
	rs_start->rss_cpu= cpu;
}

static void do_irq(config_t *cpe, struct rs_start *rs_start)
{
	int irq;
	int first;
	char *check;

	/* Process a list of IRQs */
	first = TRUE;
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_irq: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_irq: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}

		/* No IRQ allowed? (default) */
		if(!strcmp(cpe->word, KW_NONE)) {
			if(!first || cpe->next) {
				fatal("do_irq: %s keyword not allowed in list",
				KW_NONE);
			}
			break;
		}

		/* All IRQs are allowed? */
		if(!strcmp(cpe->word, KW_ALL)) {
			if(!first || cpe->next) {
				fatal("do_irq: %s keyword not allowed in list",
				KW_ALL);
			}
			rs_start->rss_nr_irq = RSS_IO_ALL;
			break;
		}

		/* Set single IRQs as specified in the configuration. */
		irq= strtoul(cpe->word, &check, 0);
		if (check[0] != '\0')
		{
			fatal("do_irq: bad irq '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		}
		if (rs_start->rss_nr_irq >= RSS_NR_IRQ)
			fatal("do_irq: too many IRQs (max %d)", RSS_NR_IRQ);
		rs_start->rss_irq[rs_start->rss_nr_irq]= irq;
		rs_start->rss_nr_irq++;
		first = FALSE;
	}
}

static void do_io(config_t *cpe, struct rs_start *rs_start)
{
	unsigned base, len;
	int first;
	char *check;

	/* Process a list of I/O ranges */
	first = TRUE;
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_io: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_io: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}

		/* No range allowed? (default) */
		if(!strcmp(cpe->word, KW_NONE)) {
			if(!first || cpe->next) {
				fatal("do_io: %s keyword not allowed in list",
				KW_NONE);
			}
			break;
		}

		/* All ranges are allowed? */
		if(!strcmp(cpe->word, KW_ALL)) {
			if(!first || cpe->next) {
				fatal("do_io: %s keyword not allowed in list",
				KW_ALL);
			}
			rs_start->rss_nr_io = RSS_IO_ALL;
			break;
		}

		/* Set single ranges as specified in the configuration. */
		base= strtoul(cpe->word, &check, 0x10);
		len= 1;
		if (check[0] == ':')
		{
			len= strtoul(check+1, &check, 0x10);
		}
		if (check[0] != '\0')
		{
			fatal("do_io: bad I/O range '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		}

		if (rs_start->rss_nr_io >= RSS_NR_IO)
			fatal("do_io: too many I/O ranges (max %d)", RSS_NR_IO);
		rs_start->rss_io[rs_start->rss_nr_io].base= base;
		rs_start->rss_io[rs_start->rss_nr_io].len= len;
		rs_start->rss_nr_io++;
		first = FALSE;
	}
}

static void do_pci_device(config_t *cpe, struct rs_start *rs_start)
{
	u16_t vid, did, sub_vid, sub_did;
	char *check, *check2;

	/* Process a list of PCI device IDs */
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_pci_device: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_pci_device: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}
		vid= strtoul(cpe->word, &check, 0x10);
		if (check[0] != ':' && /* LEGACY: */ check[0] != '/') {
			fatal("do_pci_device: bad ID '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		}
		did= strtoul(check+1, &check, 0x10);
		if (check[0] == '/') {
			sub_vid= strtoul(check+1, &check, 0x10);
			if (check[0] == ':')
				sub_did= strtoul(check+1, &check2, 0x10);
			if (check[0] != ':' || check2[0] != '\0') {
				fatal("do_pci_device: bad ID '%s' at %s:%d",
					cpe->word, cpe->file, cpe->line);
			}
		} else if (check[0] != '\0') {
			fatal("do_pci_device: bad ID '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		} else {
			sub_vid = NO_SUB_VID;
			sub_did = NO_SUB_DID;
		}
		if (rs_start->rss_nr_pci_id >= RS_NR_PCI_DEVICE)
		{
			fatal("do_pci_device: too many device IDs (max %d)",
				RS_NR_PCI_DEVICE);
		}
		rs_start->rss_pci_id[rs_start->rss_nr_pci_id].vid= vid;
		rs_start->rss_pci_id[rs_start->rss_nr_pci_id].did= did;
		rs_start->rss_pci_id[rs_start->rss_nr_pci_id].sub_vid= sub_vid;
		rs_start->rss_pci_id[rs_start->rss_nr_pci_id].sub_did= sub_did;
		rs_start->rss_nr_pci_id++;
	}
}

static void do_pci_class(config_t *cpe, struct rs_start *rs_start)
{
	u8_t baseclass, subclass, interface;
	u32_t class_id, mask;
	char *check;

	/* Process a list of PCI device class IDs */
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_pci_device: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_pci_device: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}

		baseclass= strtoul(cpe->word, &check, 0x10);
		subclass= 0;
		interface= 0;
		mask= 0xff0000;
		if (check[0] == '/')
		{
			subclass= strtoul(check+1, &check, 0x10);
			mask= 0xffff00;
			if (check[0] == '/')
			{
				interface= strtoul(check+1, &check, 0x10);
				mask= 0xffffff;
			}
		}

		if (check[0] != '\0')
		{
			fatal("do_pci_class: bad class ID '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		}
		class_id= (baseclass << 16) | (subclass << 8) | interface;
		if (rs_start->rss_nr_pci_class >= RS_NR_PCI_CLASS)
		{
			fatal("do_pci_class: too many class IDs (max %d)",
				RS_NR_PCI_CLASS);
		}
		rs_start->rss_pci_class[rs_start->rss_nr_pci_class].pciclass=
			class_id;
		rs_start->rss_pci_class[rs_start->rss_nr_pci_class].mask= mask;
		rs_start->rss_nr_pci_class++;
	}
}

static void do_pci(config_t *cpe, struct rs_start *rs_start)
{
	if (cpe == NULL)
		return;	/* Empty PCI statement */

	if (cpe->flags & CFG_SUBLIST)
	{
		fatal("do_pci: unexpected sublist at %s:%d",
			cpe->file, cpe->line);
	}
	if (cpe->flags & CFG_STRING)
	{
		fatal("do_pci: unexpected string at %s:%d",
			cpe->file, cpe->line);
	}

	if (strcmp(cpe->word, KW_DEVICE) == 0)
	{
		do_pci_device(cpe->next, rs_start);
		return;
	}
	if (strcmp(cpe->word, KW_CLASS) == 0)
	{
		do_pci_class(cpe->next, rs_start);
		return;
	}
	fatal("do_pci: unexpected word '%s' at %s:%d",
		cpe->word, cpe->file, cpe->line);
}

static void do_ipc(config_t *cpe, struct rs_start *rs_start)
{
	char *list;
	const char *word;
	char *word_all = RSS_IPC_ALL;
	char *word_all_sys = RSS_IPC_ALL_SYS;
	size_t listsize, wordlen;
	int first;

	list= NULL;
	listsize= 1;
	list= malloc(listsize);
	if (list == NULL)
		fatal("do_ipc: unable to malloc %d bytes", listsize);
	list[0]= '\0';

	/* Process a list of process names that are allowed to be
	 * contacted
	 */
	first = TRUE;
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_ipc: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_ipc: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}
		word = cpe->word;

		/* All (system) ipc targets are allowed? */
		if(!strcmp(word, KW_ALL) || !strcmp(word, KW_ALL_SYS)) {
			if(!first || cpe->next) {
				fatal("do_ipc: %s keyword not allowed in list",
				word);
			}
			word = !strcmp(word, KW_ALL) ? word_all : word_all_sys;
		}

		wordlen= strlen(word);

		listsize += 1 + wordlen;
		list= realloc(list, listsize);
		if (list == NULL)
		{
			fatal("do_ipc: unable to realloc %d bytes",
				listsize);
		}
		strcat(list, " ");
		strcat(list, word);
		first = FALSE;
	}
#if 0
	printf("do_ipc: got list '%s'\n", list);
#endif

	if (rs_start->rss_ipc)
		fatal("do_ipc: req_ipc is set");
        rs_start->rss_ipc = list+1;
	rs_start->rss_ipclen= strlen(rs_start->rss_ipc);
}


struct
{
	char *label;
	int call_nr;
} vm_table[] =
{
	{ "EXIT",		VM_EXIT },
	{ "FORK",		VM_FORK },
	{ "EXEC_NEWMEM",	VM_EXEC_NEWMEM },
	{ "PUSH_SIG",		0 },
	{ "WILLEXIT",		VM_WILLEXIT },
	{ "ADDDMA",		VM_ADDDMA },
	{ "DELDMA",		VM_DELDMA },
	{ "GETDMA",		VM_GETDMA },
	{ "REMAP",		VM_REMAP },
	{ "REMAP_RO",		VM_REMAP_RO },
	{ "SHM_UNMAP",		VM_SHM_UNMAP },
	{ "GETPHYS",		VM_GETPHYS },
	{ "GETREF",		VM_GETREF },
	{ "RS_SET_PRIV",	VM_RS_SET_PRIV },
	{ "INFO",		VM_INFO },
	{ "RS_UPDATE",		VM_RS_UPDATE },
	{ "RS_MEMCTL",		VM_RS_MEMCTL },
	{ "PROCCTL",		VM_PROCCTL },
	{ "MAPCACHEPAGE",	VM_MAPCACHEPAGE },
	{ "SETCACHEPAGE",	VM_SETCACHEPAGE },
	{ "FORGETCACHEPAGE",	VM_FORGETCACHEPAGE },
	{ "CLEARCACHE",		VM_CLEARCACHE },
	{ "VFS_MMAP",		VM_VFS_MMAP },
	{ "VFS_REPLY",		VM_VFS_REPLY },
	{ "GETRUSAGE",		VM_GETRUSAGE },
	{ "RS_PREPARE",		VM_RS_PREPARE },
	{ NULL,			0 },
};

static void do_vm(config_t *cpe, struct rs_start *rs_start)
{
	int i, first;

	first = TRUE;
	for (; cpe; cpe = cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_vm: unexpected sublist at %s:%d",
			      cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_vm: unexpected string at %s:%d",
			      cpe->file, cpe->line);
		}

		/* Only basic calls allowed? (default). */
		if(!strcmp(cpe->word, KW_BASIC)) {
			if(!first || cpe->next) {
				fatal("do_vm: %s keyword not allowed in list",
				KW_NONE);
			}
			break;
		}

		/* No calls allowed? */
		if(!strcmp(cpe->word, KW_NONE)) {
			if(!first || cpe->next) {
				fatal("do_vm: %s keyword not allowed in list",
				KW_NONE);
			}
			rs_start->rss_flags &= ~RSS_VM_BASIC_CALLS;
			break;
		}

		/* All calls are allowed? */
		if(!strcmp(cpe->word, KW_ALL)) {
			if(!first || cpe->next) {
				fatal("do_vm: %s keyword not allowed in list",
				KW_ALL);
			}
			for (i = 0; i < NR_VM_CALLS; i++)
				SET_BIT(rs_start->rss_vm, i);
			break;
		}

		/* Set single calls as specified in the configuration. */
		for (i = 0; vm_table[i].label != NULL; i++)
			if (!strcmp(cpe->word, vm_table[i].label))
				break;
		if (vm_table[i].label == NULL) {
			warning("do_vm: ignoring unknown call '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		} else if(vm_table[i].call_nr) {
			SET_BIT(rs_start->rss_vm,
				vm_table[i].call_nr - VM_RQ_BASE);
		}

		first = FALSE;
	}
}

struct
{
	char *label;
	int call_nr;
} system_tab[]=
{
	{ "PRIVCTL",		SYS_PRIVCTL },
	{ "TRACE",		SYS_TRACE },
	{ "KILL",		SYS_KILL },
	{ "UMAP",		SYS_UMAP },
	{ "VIRCOPY",		SYS_VIRCOPY },
	{ "PHYSCOPY",		SYS_PHYSCOPY },
	{ "UMAP_REMOTE",	SYS_UMAP_REMOTE },
	{ "VUMAP",		SYS_VUMAP },
	{ "IRQCTL",		SYS_IRQCTL },
	{ "DEVIO",		SYS_DEVIO },
	{ "SDEVIO",		SYS_SDEVIO },
	{ "VDEVIO",		SYS_VDEVIO },
	{ "ABORT",		SYS_ABORT },
	{ "IOPENABLE",		SYS_IOPENABLE },
	{ "READBIOS",		SYS_READBIOS },
	{ "STIME",		SYS_STIME },
	{ "VMCTL",		SYS_VMCTL },
	{ "MEMSET",		SYS_MEMSET },
	{ "PADCONF",		SYS_PADCONF },
	{ NULL,		0 }
};

static void do_system(config_t *cpe, struct rs_start *rs_start)
{
	int i, first;

	/* Process a list of 'system' calls that are allowed */
	first = TRUE;
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_system: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_system: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}

		/* Only basic calls allowed? (default). */
		if(!strcmp(cpe->word, KW_BASIC)) {
			if(!first || cpe->next) {
				fatal("do_system: %s keyword not allowed in list",
				KW_NONE);
			}
			break;
		}

		/* No calls allowed? */
		if(!strcmp(cpe->word, KW_NONE)) {
			if(!first || cpe->next) {
				fatal("do_system: %s keyword not allowed in list",
				KW_NONE);
			}
			rs_start->rss_flags &= ~RSS_SYS_BASIC_CALLS;
			break;
		}

		/* All calls are allowed? */
		if(!strcmp(cpe->word, KW_ALL)) {
			if(!first || cpe->next) {
				fatal("do_system: %s keyword not allowed in list",
				KW_ALL);
			}
			for (i = 0; i < NR_SYS_CALLS; i++)
				SET_BIT(rs_start->rss_system, i);
			break;
		}

		/* Set single calls as specified in the configuration. */
		for (i = 0; system_tab[i].label != NULL; i++)
			if (!strcmp(cpe->word, system_tab[i].label))
				break;
		if (system_tab[i].label == NULL) {
		   warning("do_system: ignoring unknown call '%s' at %s:%d",
				cpe->word, cpe->file, cpe->line);
		} else {
			SET_BIT(rs_start->rss_system,
				system_tab[i].call_nr - KERNEL_CALL);
		}
		first = FALSE;
	}
}

static void do_control(config_t *cpe, struct rs_start *rs_start)
{
	int nr_control = 0;

	/* Process a list of 'control' labels. */
	for (; cpe; cpe= cpe->next)
	{
		if (cpe->flags & CFG_SUBLIST)
		{
			fatal("do_control: unexpected sublist at %s:%d",
				cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING)
		{
			fatal("do_control: unexpected string at %s:%d",
				cpe->file, cpe->line);
		}
		if (nr_control >= RS_NR_CONTROL)
		{
			fatal(
			"do_control: RS_NR_CONTROL is too small (%d needed)",
				nr_control+1);
		}

		rs_start->rss_control[nr_control].l_addr = (char*) cpe->word;
		rs_start->rss_control[nr_control].l_len = strlen(cpe->word);
		rs_start->rss_nr_control = ++nr_control;
	}
}

static const struct {
	const char *name;
	int domain;
} domain_tab[] = {
	/* PF_UNSPEC should not be in this table. */
	{ "LOCAL",	PF_LOCAL	},
	{ "INET",	PF_INET		},
	{ "IMPLINK",	PF_IMPLINK	},
	{ "PUP",	PF_PUP		},
	{ "CHAOS",	PF_CHAOS	},
	{ "NS",		PF_NS		},
	{ "ISO",	PF_ISO		},
	{ "ECMA",	PF_ECMA		},
	{ "DATAKIT",	PF_DATAKIT	},
	{ "CCITT",	PF_CCITT	},
	{ "SNA",	PF_SNA		},
	{ "DECnet",	PF_DECnet	},
	{ "DLI",	PF_DLI		},
	{ "LAT",	PF_LAT		},
	{ "HYLINK",	PF_HYLINK	},
	{ "APPLETALK",	PF_APPLETALK	},
	{ "OROUTE",	PF_OROUTE	},
	{ "LINK",	PF_LINK		},
	{ "XTP",	PF_XTP		},
	{ "COIP",	PF_COIP		},
	{ "CNT",	PF_CNT		},
	{ "RTIP",	PF_RTIP		},
	{ "IPX",	PF_IPX		},
	{ "INET6",	PF_INET6	},
	{ "PIP",	PF_PIP		},
	{ "ISDN",	PF_ISDN		},
	{ "NATM",	PF_NATM		},
	{ "ARP",	PF_ARP		},
	{ "KEY",	PF_KEY		},
	{ "BLUETOOTH",	PF_BLUETOOTH	},
	/* There is no PF_IEEE80211. */
	{ "MPLS",	PF_MPLS		},
	{ "ROUTE",	PF_ROUTE	},
};

/*
 * Process a list of 'domain' protocol families for socket drivers.
 */
static void
do_domain(config_t * cpe, struct rs_start * rs_start)
{
	unsigned int i;
	int nr_domain, domain;

	for (nr_domain = 0; cpe != NULL; cpe = cpe->next) {
		if (cpe->flags & CFG_SUBLIST) {
			fatal("do_domain: unexpected sublist at %s:%d",
			    cpe->file, cpe->line);
		}
		if (cpe->flags & CFG_STRING) {
			fatal("do_domain: unexpected string at %s:%d",
			    cpe->file, cpe->line);
		}
		if (nr_domain >= __arraycount(rs_start->rss_domain)) {
			fatal("do_domain: NR_DOMAIN is too small (%d needed)",
			    nr_domain + 1);
		}

		for (i = 0; i < __arraycount(domain_tab); i++)
			if (!strcmp(domain_tab[i].name, (char *)cpe->word))
				break;
		if (i < __arraycount(domain_tab))
			domain = domain_tab[i].domain;
		else
			domain = atoi((char *)cpe->word);

		if (domain <= 0 || domain >= PF_MAX) {
			fatal("do_domain: unknown domain %s at %s:%d",
			    (char *)cpe->word, cpe->file, cpe->line);
		}

		rs_start->rss_domain[nr_domain] = domain;
		rs_start->rss_nr_domain = ++nr_domain;
	}
}

static void do_service(config_t *cpe, config_t *config, struct rs_config *rs_config)
{
	struct rs_start *rs_start = &rs_config->rs_start;
	config_t *cp;

	/* At this point we expect one sublist that contains the varios
	 * resource allocations
	 */
	if (!(cpe->flags & CFG_SUBLIST))
	{
		fatal("do_service: expected list at %s:%d",
			cpe->file, cpe->line);
	}
	if (cpe->next != NULL)
	{
		cpe= cpe->next;
		fatal("do_service: expected end of list at %s:%d",
			cpe->file, cpe->line);
	}
	cpe= cpe->list;

	/* Process the list */
	for (cp= cpe; cp; cp= cp->next)
	{
		if (!(cp->flags & CFG_SUBLIST))
		{
			fatal("do_service: expected list at %s:%d",
				cp->file, cp->line);
		}
		cpe= cp->list;
		if ((cpe->flags & CFG_STRING) || (cpe->flags & CFG_SUBLIST))
		{
			fatal("do_service: expected word at %s:%d",
				cpe->file, cpe->line);
		}

		if (strcmp(cpe->word, KW_CLASS) == 0)
		{
			do_class(cpe->next, config, rs_config);
			continue;
		}
		if (strcmp(cpe->word, KW_UID) == 0)
		{
			do_uid(cpe->next, rs_start);
			continue;
		}
		if (strcmp(cpe->word, KW_SIGMGR) == 0)
		{
			do_sigmgr(cpe->next, rs_start);
			continue;
		}
		if (strcmp(cpe->word, KW_TYPE) == 0)
		{
			do_type(cpe->next, rs_config);
			continue;
		}
		if (strcmp(cpe->word, KW_DESCR) == 0)
		{
			do_descr(cpe->next, rs_config);
			continue;
		}
		if (strcmp(cpe->word, KW_SCHEDULER) == 0)
		{
			do_scheduler(cpe->next, rs_start);
			continue;
		}
		if (strcmp(cpe->word, KW_PRIORITY) == 0)
		{
			do_priority(cpe->next, rs_start);
			continue;
		}
		if (strcmp(cpe->word, KW_QUANTUM) == 0)
		{
			do_quantum(cpe->next, rs_start);
			continue;
		}
		if (strcmp(cpe->word, KW_CPU) == 0)
		{
			do_cpu(cpe->next, rs_start);
			continue;
		}
		if (strcmp(cpe->word, KW_IRQ) == 0)
		{
			do_irq(cpe->next, rs_start);
			continue;
		}
		if (strcmp(cpe->word, KW_IO) == 0)
		{
			do_io(cpe->next, rs_start);
			continue;
		}
		if (strcmp(cpe->word, KW_PCI) == 0)
		{
			do_pci(cpe->next, rs_start);
			continue;
		}
		if (strcmp(cpe->word, KW_SYSTEM) == 0)
		{
			do_system(cpe->next, rs_start);
			continue;
		}
		if (strcmp(cpe->word, KW_IPC) == 0)
		{
			do_ipc(cpe->next, rs_start);
			continue;
		}
		if (strcmp(cpe->word, KW_VM) == 0)
		{
			do_vm(cpe->next, rs_start);
			continue;
		}
		if (strcmp(cpe->word, KW_CONTROL) == 0)
		{
			do_control(cpe->next, rs_start);
			continue;
		}
		if (strcmp(cpe->word, KW_DOMAIN) == 0)
		{
			do_domain(cpe->next, rs_start);
			continue;
		}
	}
}

static const char *do_config(const char *label, char *filename, struct rs_config *rs_config)
{
	config_t *config, *cp, *cpe;
	struct passwd *pw;
	struct rs_start *rs_start = &rs_config->rs_start;

	if(!(config= config_read(filename, 0, NULL)))
		return NULL; /* config file read failed. */

	/* Set clean rs_start defaults. */
	memset(rs_config, 0, sizeof(*rs_config));
	if(!(pw= getpwnam(SERVICE_LOGIN)))
		fatal("no passwd file entry for '%s'", SERVICE_LOGIN);
	rs_start->rss_uid= pw->pw_uid;
	rs_start->rss_sigmgr= DSRV_SM;
	rs_start->rss_scheduler= DSRV_SCH;
	rs_start->rss_priority= DSRV_Q;
	rs_start->rss_quantum= DSRV_QT;
	rs_start->rss_cpu = DSRV_CPU;
	rs_start->rss_flags = RSS_VM_BASIC_CALLS | RSS_SYS_BASIC_CALLS;

	/* Find an entry for our service */
	for (cp= config; cp; cp= cp->next)
	{
		if (!(cp->flags & CFG_SUBLIST))
		{
			fatal("do_config: expected list at %s:%d",
				cp->file, cp->line);
		}
		cpe= cp->list;
		if ((cpe->flags & CFG_STRING) || (cpe->flags & CFG_SUBLIST))
		{
			fatal("do_config: expected word at %s:%d",
				cpe->file, cpe->line);
		}

		/* At this place we expect the word KW_SERVICE */
		if (strcmp(cpe->word, KW_SERVICE) != 0)
			fatal("do_config: exected word '%S' at %s:%d",
				KW_SERVICE, cpe->file, cpe->line);

		cpe= cpe->next;
		if ((cpe->flags & CFG_STRING) || (cpe->flags & CFG_SUBLIST))
		{
			fatal("do_config: expected word at %s:%d",
				cpe->file, cpe->line);
		}

		/* At this place we expect the name of the service. */
		if (!label || strcmp(cpe->word, label) == 0) {
			label = cpe->word;
			break;
		}
	}
	if (cp == NULL)
	{
		fprintf(stderr, "minix-service: service '%s' not found in "
		    "'%s'\n", label, filename);
		exit(1);
	}

	cpe= cpe->next;

	do_service(cpe, config, rs_config);

	{
		char *default_ipc = RSS_IPC_ALL_SYS;
		if(!rs_start->rss_ipc) {
		      rs_start->rss_ipc= default_ipc;
		      rs_start->rss_ipclen= strlen(default_ipc);
		}
	}

	/* config file read ok. */
	return label;
}

/* returns failure */
const char *parse_config(char *progname, int custom_config, char *req_config,
	struct rs_config *rs_config)
{
        char *specificconfig, *specific_pkg_config;
	const char *l;

	/* Config file specified? */
        if(custom_config)
          return do_config(progname, req_config, rs_config);

	/* No specific config file. */
        if(asprintf(&specificconfig, "%s/%s", _PATH_SYSTEM_CONF_DIR,
              progname) < 0) {
              errx(1, "no memory");
        }

        if(asprintf(&specific_pkg_config, "%s/%s", _PATH_SYSTEM_CONF_PKG_DIR,
              progname) < 0) {
              errx(1, "no memory");
        }

        /* Try specific config filename first, in base system
	 * and package locations, * and only if it fails, the global
	 * system one.
         */
	if((l=do_config(progname, specific_pkg_config, rs_config))) return l;
	if((l=do_config(progname, specificconfig, rs_config))) return l;
	if((l=do_config(progname, req_config, rs_config))) return l;

	return NULL;
}

