#define UNPAGED 1	/* for proper kmain() prototype */

// #include <assert.h> // Replaced
// #include <stdlib.h> // Removed
#include <minix/minlib.h>    // Kept
#include <minix/board.h>     // Kept
// #include <sys/reboot.h> // Removed
#include <machine/partition.h> // Kept
// #include "string.h" // Corrected to <string.h> then Replaced

// Added kernel headers
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>

#include "direct_utils.h"    // Kept (local header)
#include "serial.h"          // Kept (local header)
#include "glo.h"             // Kept (local header)


#if USE_SYSDEBUG
#define MULTIBOOT_VERBOSE 1
#endif

/* to-be-built kinfo struct, diagnostics buffer */
kinfo_t kinfo;
struct kmessages kmessages;

/* pg_utils.c uses this; in this phase, there is a 1:1 mapping. */
phys_bytes vir2phys(void *addr) { return (phys_bytes) addr; } 

/* mb_utils.c uses this; we can reach it directly */
char *video_mem = (char *) MULTIBOOT_VIDEO_BUFFER;

/* String length used for mb_itoa */
#define ITOA_BUFFER_SIZE 20

/* Kernel may use memory */
int kernel_may_alloc = 1;

static int mb_set_param(char *bigbuf, char *name, char *value, kinfo_t *cbi) 
{
	char *p = bigbuf;
	char *bufend = bigbuf + MULTIBOOT_PARAM_BUF_SIZE;
	char *q;
	k_size_t namelen = kstrlen(name); // MODIFIED
	k_size_t valuelen = kstrlen(value); // MODIFIED

	/* Some variables we recognize */
	if(!kstrcmp(name, SERVARNAME)) { cbi->do_serial_debug = 1; } // MODIFIED
	if(!kstrcmp(name, SERBAUDVARNAME)) { cbi->serial_debug_baud = 0 /* FIXME: atoi(value) replaced */; } // MODIFIED

	/* Delete the item if already exists */
	while (*p) {
		// MODIFIED strncmp to placeholder
		if ( (/* FIXME: strncmp needs kstrncmp */ (void)0, kstrncmp_placeholder(p, name, namelen)) == 0 && p[namelen] == '=') {
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
	
	(void)kstrlcpy(p, name, namelen + 1); /* FIXME: strcpy was here... Using namelen+1 for kstrlcpy */ // MODIFIED
	p[namelen] = '=';
	(void)kstrlcpy(p + namelen + 1, value, valuelen + 1); /* FIXME: strcpy was here... Using valuelen+1 for kstrlcpy */ // MODIFIED
	p[namelen + valuelen + 1] = 0;
	p[namelen + valuelen + 2] = 0;
	return 0;
}

int overlaps(multiboot_module_t *mod, int n, int cmp_mod)
{
	multiboot_module_t *cmp = &mod[cmp_mod];
	int m;

#define INRANGE(mod, v) ((v) >= mod->mod_start && (v) < mod->mod_end)
#define OVERLAP(mod1, mod2) (INRANGE(mod1, mod2->mod_start) || \
			INRANGE(mod1, mod2->mod_end-1))
	for(m = 0; m < n; m++) {
		multiboot_module_t *thismod = &mod[m];
		if(m == cmp_mod) continue;
		if(OVERLAP(thismod, cmp))
			return 1;
	}
	return 0;
}

void get_parameters(u32_t ebx, kinfo_t *cbi) // u32_t might be undefined
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
	kmemcpy((void *) mbi, (void *) ebx, sizeof(*mbi)); // MODIFIED

	/* Set various bits of info for the higher-level kernel. */
	cbi->mem_high_phys = 0;
	cbi->user_sp = (vir_bytes) &_kern_vir_base;
	cbi->vir_kern_start = (vir_bytes) &_kern_vir_base;
	cbi->bootstrap_start = (vir_bytes) &_kern_unpaged_start;
	cbi->bootstrap_len = (vir_bytes) &_kern_unpaged_end -
		cbi->bootstrap_start;
	cbi->kmess = &kmess;

	/* set some configurable defaults */
	cbi->do_serial_debug = 0;
	cbi->serial_debug_baud = 115200;

	/* parse boot command line */
	if (mbi->mi_flags & MULTIBOOT_INFO_HAS_CMDLINE) {
		static char var[BUF];
		static char value[BUF];

		/* Override values with cmdline argument */
		kmemcpy(cmdline, (void *) mbi->mi_cmdline, BUF); // MODIFIED
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

        /* let higher levels know what we are booting on */
        mb_set_param(cbi->param_buf, ARCHVARNAME, (char *)get_board_arch_name(BOARD_ID_INTEL), cbi);
	mb_set_param(cbi->param_buf, BOARDVARNAME,(char *)get_board_name(BOARD_ID_INTEL) , cbi);

	/* move user stack/data down to leave a gap to catch kernel
	 * stack overflow; and to distinguish kernel and user addresses
	 * at a glance (0xf.. vs 0xe..) 
	 */
	cbi->user_sp = USR_STACKTOP;
	cbi->user_end = USR_DATATOP;

	/* kernel bytes without bootstrap code/data that is currently
	 * still needed but will be freed after bootstrapping.
	 */
	kinfo.kernel_allocated_bytes = (phys_bytes) &_kern_size;
	kinfo.kernel_allocated_bytes -= cbi->bootstrap_len;

	KASSERT_PLACEHOLDER(!(cbi->bootstrap_start % I386_PAGE_SIZE)); // MODIFIED
	cbi->bootstrap_len = rounddown(cbi->bootstrap_len, I386_PAGE_SIZE);
	KASSERT_PLACEHOLDER(mbi->mi_flags & MULTIBOOT_INFO_HAS_MODS); // MODIFIED
	KASSERT_PLACEHOLDER(mbi->mi_mods_count < MULTIBOOT_MAX_MODS); // MODIFIED
	KASSERT_PLACEHOLDER(mbi->mi_mods_count > 0); // MODIFIED
	kmemcpy(&cbi->module_list, (void *) mbi->mi_mods_addr, // MODIFIED
		mbi->mi_mods_count * sizeof(multiboot_module_t));
	
	kmemset(cbi->memmap, 0, sizeof(cbi->memmap)); // MODIFIED
	/* mem_map has a variable layout */
	if(mbi->mi_flags & MULTIBOOT_INFO_HAS_MMAP) {
		cbi->mmap_size = 0;
	        for (mmap = (multiboot_memory_map_t *) mbi->mmap_addr;
       	     (unsigned long) mmap < mbi->mmap_addr + mbi->mmap_length;
       	       mmap = (multiboot_memory_map_t *) 
		      	((unsigned long) mmap + mmap->mm_size + sizeof(mmap->mm_size))) {
			if(mmap->mm_type != MULTIBOOT_MEMORY_AVAILABLE) continue;
			add_memmap(cbi, mmap->mm_base_addr, mmap->mm_length);
		}
	} else {
		KASSERT_PLACEHOLDER(mbi->mi_flags & MULTIBOOT_INFO_HAS_MEMORY); // MODIFIED
		add_memmap(cbi, 0, mbi->mi_mem_lower*1024);
		add_memmap(cbi, 0x100000, mbi->mi_mem_upper*1024);
	}

	/* Sanity check: the kernel nor any of the modules may overlap
	 * with each other. Pretend the kernel is an extra module for a
	 * second.
	 */
	k = mbi->mi_mods_count;
	KASSERT_PLACEHOLDER(k < MULTIBOOT_MAX_MODS); // MODIFIED
	cbi->module_list[k].mod_start = kernbase;
	cbi->module_list[k].mod_end = kernbase + kernsize;
	cbi->mods_with_kernel = mbi->mi_mods_count+1;
	cbi->kern_mod = k;

	for(m = 0; m < cbi->mods_with_kernel; m++) {
#if 0
		kprintf_stub("checking overlap of module %08lx-%08lx\n", // MODIFIED
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

kinfo_t *pre_init(u32_t magic, u32_t ebx) // u32_t might be undefined
{
	KASSERT_PLACEHOLDER(magic == MULTIBOOT_INFO_MAGIC); // MODIFIED

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

void send_diag_sig(void) { }
void minix_shutdown(int how) { arch_shutdown(how); }
void busy_delay_ms(int x) { }
int raise(int sig) { panic("raise(%d)\n", sig); }

// Helper for strncmp placeholder
int kstrncmp_placeholder(const char *s1, const char *s2, k_size_t n) {
    /* FIXME: This is a placeholder for strncmp. It's not a correct implementation. */
    /* A real kstrncmp should be implemented or this logic revisited. */
    if (!s1 || !s2 || n == 0) return 0;
    return kstrcmp(s1, s2);
}
