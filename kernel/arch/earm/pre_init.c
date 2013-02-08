
#define UNPAGED 1	/* for proper kmain() prototype */

#include "kernel/kernel.h"
#include <assert.h>
#include <stdlib.h>
#include <minix/minlib.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/com.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include "string.h"
#include "arch_proto.h"
#include "libexec.h"
#include "direct_utils.h"
#include "serial.h"
#include "glo.h"
#include <machine/multiboot.h>

#if USE_SYSDEBUG
#define MULTIBOOT_VERBOSE 1
#endif

/* to-be-built kinfo struct, diagnostics buffer */
kinfo_t kinfo;
struct kmessages kmessages;

/* pg_utils.c uses this; in this phase, there is a 1:1 mapping. */
phys_bytes vir2phys(void *addr) { return (phys_bytes) addr; } 

static void setup_mbi(multiboot_info_t *mbi);

/* String length used for mb_itoa */
#define ITOA_BUFFER_SIZE 20

/* Kernel may use memory */
int kernel_may_alloc = 1;

extern u32_t _edata;
extern u32_t _end;

static int mb_set_param(char *bigbuf, char *name, char *value, kinfo_t *cbi)
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

int overlaps(multiboot_module_t *mod, int n, int cmp_mod)
{
	multiboot_module_t *cmp = &mod[cmp_mod];
	int m;

#define INRANGE(mod, v) ((v) >= mod->mod_start && (v) <= thismod->mod_end)
#define OVERLAP(mod1, mod2) (INRANGE(mod1, mod2->mod_start) || \
			INRANGE(mod1, mod2->mod_end))
	for(m = 0; m < n; m++) {
		multiboot_module_t *thismod = &mod[m];
		if(m == cmp_mod) continue;
		if(OVERLAP(thismod, cmp))
			return 1;
	}
	return 0;
}

/* XXX: hard-coded stuff for modules */
#define MB_MODS_NR 12
#define MB_MODS_BASE  0x90000000
#define MB_PARAM_MOD  0x96000000
#define MB_MODS_ALIGN 0x00800000 /*  8 MB */
#define MB_MODS_SIZE  0x00004000 /* 16 KB */
#define MB_MMAP_START 0x80000000
#define MB_MMAP_SIZE  0x20000000 /* 512 MB */

multiboot_module_t mb_modlist[MB_MODS_NR];
multiboot_memory_map_t mb_memmap;

void setup_mbi(multiboot_info_t *mbi)
{
	memset(mbi, 0, sizeof(*mbi));
	mbi->flags = MULTIBOOT_INFO_MODS | MULTIBOOT_INFO_MEM_MAP |
			MULTIBOOT_INFO_CMDLINE;
	mbi->mods_count = MB_MODS_NR;
	mbi->mods_addr = (u32_t)&mb_modlist;

	int i;
	for (i = 0; i < MB_MODS_NR; ++i) {
	    mb_modlist[i].mod_start = MB_MODS_BASE + i * MB_MODS_ALIGN;
	    mb_modlist[i].mod_end = mb_modlist[i].mod_start + MB_MODS_ALIGN - 1;	    mb_modlist[i].cmdline = 0;
	}

	/* Final 'module' is actually a string holding the boot cmdline */
	mbi->cmdline = MB_PARAM_MOD;

	mbi->mmap_addr = (void*)&mb_memmap;
	mbi->mmap_length = sizeof(mb_memmap);

	mb_memmap.size = sizeof(multiboot_memory_map_t);
	mb_memmap.addr = MB_MMAP_START;
	mb_memmap.len  = MB_MMAP_SIZE;
	mb_memmap.type = MULTIBOOT_MEMORY_AVAILABLE;
}

void get_parameters(u32_t ebx, kinfo_t *cbi) 
{
	multiboot_memory_map_t *mmap;
	multiboot_info_t *mbi = &cbi->mbi;
	int var_i,value_i, m, k;
	char *p;
	extern char _kern_phys_base, _kern_vir_base, _kern_size,
		_kern_unpaged_start, _kern_unpaged_end;
	phys_bytes kernbase = (phys_bytes) &_kern_phys_base,
		kernsize = (phys_bytes) &_kern_size;
#define BUF 1024
	static char cmdline[BUF];

	/* get our own copy of the multiboot info struct and module list */
	//memcpy((void *) mbi, (void *) ebx, sizeof(*mbi));
	setup_mbi(mbi);

	/* Set various bits of info for the higher-level kernel. */
	cbi->mem_high_phys = 0;
	cbi->user_sp = (vir_bytes) &_kern_vir_base;
	cbi->vir_kern_start = (vir_bytes) &_kern_vir_base;
	cbi->bootstrap_start = (vir_bytes) &_kern_unpaged_start;
	cbi->bootstrap_len = (vir_bytes) &_kern_unpaged_end -
		cbi->bootstrap_start;
	cbi->kmess = &kmess;

	/* set some configurable defaults */
	cbi->do_serial_debug = 1;
	cbi->serial_debug_baud = 115200;

	/* parse boot command line */
	if (mbi->flags&MULTIBOOT_INFO_CMDLINE) {
		static char var[BUF];
		static char value[BUF];

		/* Override values with cmdline argument */
		memcpy(cmdline, (void *) mbi->cmdline, BUF);
		p = cmdline;
		while (*p) {
			var_i = 0;
			value_i = 0;
			while (*p == ' ') p++;
			if (!*p) break;
			while (*p && *p != '=' && *p != ' ' && var_i < BUF - 1) 
				var[var_i++] = *p++ ;
			var[var_i] = 0;
			if (*p++ != '=') continue; /* skip if not name=value */
			while (*p && *p != ' ' && value_i < BUF - 1) 
				value[value_i++] = *p++ ;
			value[value_i] = 0;
			
			mb_set_param(cbi->param_buf, var, value, cbi);
		}
	}

	/* round user stack down to leave a gap to catch kernel
	 * stack overflow; and to distinguish kernel and user addresses
	 * at a glance (0xf.. vs 0xe..) 
	 */
	cbi->user_sp &= 0xF0000000;
	cbi->user_end = cbi->user_sp;

	/* kernel bytes without bootstrap code/data that is currently
	 * still needed but will be freed after bootstrapping.
	 */
	kinfo.kernel_allocated_bytes = (phys_bytes) &_kern_size;
	kinfo.kernel_allocated_bytes -= cbi->bootstrap_len;

	assert(!(cbi->bootstrap_start % ARM_PAGE_SIZE));
	cbi->bootstrap_len = rounddown(cbi->bootstrap_len, ARM_PAGE_SIZE);
	assert(mbi->flags & MULTIBOOT_INFO_MODS);
	assert(mbi->mods_count < MULTIBOOT_MAX_MODS);
	assert(mbi->mods_count > 0);
	memcpy(&cbi->module_list, (void *) mbi->mods_addr,
		mbi->mods_count * sizeof(multiboot_module_t));
	
	memset(cbi->memmap, 0, sizeof(cbi->memmap));
	/* mem_map has a variable layout */
	if(mbi->flags & MULTIBOOT_INFO_MEM_MAP) {
		cbi->mmap_size = 0;
	        for (mmap = (multiboot_memory_map_t *) mbi->mmap_addr;
       	     (unsigned long) mmap < mbi->mmap_addr + mbi->mmap_length;
       	       mmap = (multiboot_memory_map_t *) 
		      	((unsigned long) mmap + mmap->size + sizeof(mmap->size))) {
			if(mmap->type != MULTIBOOT_MEMORY_AVAILABLE) continue;
			add_memmap(cbi, mmap->addr, mmap->len);
		}
	} else {
		assert(mbi->flags & MULTIBOOT_INFO_MEMORY);
		add_memmap(cbi, 0, mbi->mem_lower_unused*1024);
		add_memmap(cbi, 0x100000, mbi->mem_upper_unused*1024);
	}

	/* Sanity check: the kernel nor any of the modules may overlap
	 * with each other. Pretend the kernel is an extra module for a
	 * second.
	 */
	k = mbi->mods_count;
	assert(k < MULTIBOOT_MAX_MODS);
	cbi->module_list[k].mod_start = kernbase;
	cbi->module_list[k].mod_end = kernbase + kernsize;
	cbi->mods_with_kernel = mbi->mods_count+1;
	cbi->kern_mod = k;

	for(m = 0; m < cbi->mods_with_kernel; m++) {
#if 0
		printf("checking overlap of module %08lx-%08lx\n",
		  cbi->module_list[m].mod_start, cbi->module_list[m].mod_end);
#endif
		if(overlaps(cbi->module_list, cbi->mods_with_kernel, m))
			panic("overlapping boot modules/kernel");
		/* We cut out the bits of memory that we know are
		 * occupied by the kernel and boot modules.
		 */
		cut_memmap(cbi,
			cbi->module_list[m].mod_start, 
			cbi->module_list[m].mod_end);
	}
}

kinfo_t *pre_init(u32_t magic, u32_t ebx)
{
	/* Clear BSS */
	memset(&_edata, 0, (u32_t)&_end - (u32_t)&_edata);

	/* Get our own copy boot params pointed to by ebx.
	 * Here we find out whether we should do serial output.
	 */
	get_parameters(ebx, &kinfo);

	/* Make and load a pagetable that will map the kernel
	 * to where it should be; but first a 1:1 mapping so
	 * this code stays where it should be.
	 */
	pg_clear();
	pg_identity(&kinfo);
	kinfo.freepde_start = pg_mapkernel();
	pg_load();
	vm_enable_paging();

	/* Done, return boot info so it can be passed to kmain(). */
	return &kinfo;
}

int send_sig(endpoint_t proc_nr, int sig_nr) { return 0; }
void minix_shutdown(timer_t *t) { arch_shutdown(RBT_PANIC); }
void busy_delay_ms(int x) { }
int raise(int n) { panic("raise(%d)\n", n); }
