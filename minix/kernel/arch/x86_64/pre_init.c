
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
#include "arch_proto.h"

#if USE_SYSDEBUG
#define MULTIBOOT_VERBOSE 1
#endif

#define MULTIBOOT2_BOOTLOADER_MAGIC		0x36d76289
extern void get_parameters_mb2(vir_bytes, kinfo_t *);

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


kinfo_t *pre_init(u32_t magic, vir_bytes mbi_addr)
{
	/*
	 * Keep a serial breadcrumb trail until the normal console is ready.
	 * A fault here otherwise looks like an unexplained EFI reboot because
	 * the framebuffer/IDT have not yet been installed by MINIX.
	 */
	direct_com_init();
	direct_com_print("MINIX/amd64: pre_init entered\r\n");

	/* Get our own copy of the boot parameters pointed to by mbi_addr. */
	if (magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
		direct_com_print("MINIX/amd64: parsing Multiboot2 information\r\n");
		get_parameters_mb2(mbi_addr, &kinfo);
		direct_com_print("MINIX/amd64: Multiboot2 information parsed\r\n");
	} else
		panic("Invalid multiboot2 magic: 0x%x\n", magic);

	/*
	 * Build proper page tables: identity-map physical RAM, then map the
	 * kernel at its high virtual address.  Long mode and paging were
	 * already enabled by the EFI loader.
	 *
	 * Clear CR4.PGE before changing CR3.  UEFI page tables may contain
	 * global translations; without clearing PGE those entries can survive
	 * a CR3 load and alias the new MINIX mappings.  vm_enable_paging()
	 * restores PGE after the MINIX page tables are active.
	 *
	 * vm_enable_paging() also sets CR0.WP.  Without CR0.WP, supervisor-mode
	 * writes ignore read-only page protections, breaking EFAULT and COW.
	 */
	{
		reg_t cr4 = read_cr4();

		if (cr4 & I386_CR4_PGE)
			write_cr4(cr4 & ~I386_CR4_PGE);
	}

	direct_com_print("MINIX/amd64: clearing bootstrap page tables\r\n");
	pg_clear();
	direct_com_print("MINIX/amd64: building identity map\r\n");
	pg_identity(&kinfo);
	direct_com_print("MINIX/amd64: mapping high kernel\r\n");
	kinfo.freepde_start = pg_mapkernel();
	direct_com_print("MINIX/amd64: loading CR3\r\n");
	pg_load();
	direct_com_print("MINIX/amd64: enabling write protection\r\n");
	vm_enable_paging();
	direct_com_print("MINIX/amd64: pre_init complete\r\n");

	/* Done, return boot info so it can be passed to kmain(). */
	return &kinfo;
}

void send_diag_sig(void) { }
void minix_shutdown(int how) { arch_shutdown(how); }
void busy_delay_ms(int x) { }
int raise(int sig) { panic("raise(%d)\n", sig); }
