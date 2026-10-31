
#define UNPAGED 1	/* for proper kmain() prototype */

#include <assert.h>
#include <stdlib.h>
#include <minix/type.h>
#include <minix/param.h>
#include <minix/minlib.h>
#include <minix/board.h>
#include <sys/reboot.h>
#include <machine/partition.h>
#include "string.h"
#include "direct_utils.h"
#include "serial.h"
#include "glo.h"

#if USE_SYSDEBUG
#define MULTIBOOT_VERBOSE 1
#endif

#define MULTIBOOT2_BOOTLOADER_MAGIC		0x36d76289
extern void get_parameters_mb2(u32_t, kinfo_t *);

/* to-be-built kinfo struct, diagnostics buffer */
kinfo_t kinfo;
struct kmessages kmessages;

/* pg_utils.c uses this; in this phase, there is a 1:1 mapping. */
phys_bytes vir2phys(void *addr) { return (phys_bytes) addr; } 

/* mb_utils.c uses this; we can reach it directly */
//char *video_mem = (char *) MULTIBOOT_VIDEO_BUFFER;

/* String length used for mb_itoa */
#define ITOA_BUFFER_SIZE 20

/* Kernel may use memory */
int kernel_may_alloc = 1;

int mb_set_param(char *bigbuf, char *name, char *value, kinfo_t *cbi) 
{
	char *p = bigbuf;
	char *bufend = bigbuf + MULTIBOOT_PARAM_BUF_SIZE;
	char *q;
	int namelen = strlen(name);
	int valuelen = strlen(value);

	/* Some variables we recognize */
	if(!strcmp(name, SERVARNAME)) { cbi->do_serial_debug = 1; }
	if(!strcmp(name, SERBAUDVARNAME)) { cbi->serial_debug_baud = atoi(value); }

	/* Delete the item if already exists */
	while (*p) {
		if (strncmp(p, name, namelen) == 0 && p[namelen] == '=') {
			q = p;
			while (*q) q++;
			for (q++; q < bufend; q++, p++)
				*p = *q;
			break;
		}
		while (*p++)
			;
		p++;
	}
	
	for (p = bigbuf; p < bufend && (*p || *(p + 1)); p++)
		;
	if (p > bigbuf) p++;
	
	/* Make sure there's enough space for the new parameter */
	if (p + namelen + valuelen + 3 > bufend)
		return -1;
	
	strcpy(p, name);
	p[namelen] = '=';
	strcpy(p + namelen + 1, value);
	p[namelen + valuelen + 1] = 0;
	p[namelen + valuelen + 2] = 0;
	return 0;
}

int overlaps(kinfo_module_t *mod, int n, int cmp_mod)
{
	kinfo_module_t *cmp = &mod[cmp_mod];
	int m;

#define INRANGE(mod, v) ((v) >= mod->mod_start && (v) < mod->mod_end)
#define OVERLAP(mod1, mod2) (INRANGE(mod1, mod2->mod_start) || \
			INRANGE(mod1, mod2->mod_end-1))
	for(m = 0; m < n; m++) {
		kinfo_module_t *thismod = &mod[m];
		if(m == cmp_mod) continue;
		if(OVERLAP(thismod, cmp))
			return 1;
	}
	return 0;
}


kinfo_t *pre_init(u32_t magic, u32_t ebx)
{
	/* Get our own copy boot params pointed to by ebx.
	 * Here we find out whether we should do serial output.
	 */
	if (magic == MULTIBOOT2_BOOTLOADER_MAGIC)
		get_parameters_mb2(ebx, &kinfo);
	else
		panic("Invalid multiboot2 magic: 0x%x\n", magic);

	/* Build proper page tables: identity-map physical RAM, then map the
	 * kernel at its high virtual address.  Long mode and paging were
	 * already enabled by head.S / the EFI loader, but CR0.WP (and CR4.PGE)
	 * are still at the firmware default.  vm_enable_paging() only ORs those
	 * bits in (it does not re-toggle paging), and we must call it: without
	 * CR0.WP, supervisor-mode writes ignore read-only page protections, so
	 * the kernel copying into a read-only user page (e.g. read(2) into a
	 * PROT_READ buffer, or a copy-on-write page) silently succeeds instead
	 * of faulting — breaking EFAULT semantics and COW.
	 */
	pg_clear();
	pg_identity(&kinfo);
	kinfo.freepde_start = pg_mapkernel();
	pg_load();
	vm_enable_paging();

	/* Done, return boot info so it can be passed to kmain(). */
	return &kinfo;
}

void send_diag_sig(void) { }
void minix_shutdown(int how) { arch_shutdown(how); }
void busy_delay_ms(int x) { }
int raise(int sig) { panic("raise(%d)\n", sig); }
