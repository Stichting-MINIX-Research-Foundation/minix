/*
 * Get Parameters from Multiboot2 Information.
 */
#define UNPAGED 1	/* for proper kmain() prototype */

#define	__MINIX_MULTIBOOT2	1

#include <assert.h>
#include <stdlib.h>
#include <minix/minlib.h>
#include <minix/board.h>
#include <sys/reboot.h>
#include <machine/partition.h>
#include "string.h"
#include "direct_utils.h"
#include "serial.h"
#include "glo.h"
#include <machine/multiboot2.h>

#if USE_SYSDEBUG
#define MULTIBOOT_VERBOSE 1
#endif

extern int mb_set_param(char *bigbuf, char *name, char *value, kinfo_t *cbi);
extern int overlaps(kinfo_module_t *mod, int n, int cmp_mod);

/* to-be-built kinfo struct, diagnostics buffer */
extern kinfo_t kinfo;
extern struct kmessages kmessages;

extern char _kern_phys_base, _kern_vir_base, _kern_size,
	_kern_unpaged_start, _kern_unpaged_end;

/* pg_utils.c uses this; in this phase, there is a 1:1 mapping. */
//phys_bytes vir2phys(void *addr) { return (phys_bytes) addr; } 

/* String length used for mb_itoa */
#define ITOA_BUFFER_SIZE 20



static void do_tag_cmdline(kinfo_t *cbi, struct multiboot_tag_string *tag_cmdline)
{
#define BUF 1024
	static char cmdline[BUF];
	static char var[BUF];
	static char value[BUF];
	int var_i,value_i, m, k;
	char *p;

	assert(tag_cmdline->size < BUF);
	/* Override values with cmdline argument */
	memcpy(cmdline, (void *) tag_cmdline->string, BUF);
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

static void do_tag_framebuffer(kinfo_t *cbi, struct multiboot_tag_framebuffer *tag_fb)
{
	cbi->fb.framebuffer_addr = tag_fb->common.framebuffer_addr;
	cbi->fb.framebuffer_pitch = tag_fb->common.framebuffer_pitch;
	cbi->fb.framebuffer_width = tag_fb->common.framebuffer_width;
	cbi->fb.framebuffer_height = tag_fb->common.framebuffer_height;
	cbi->fb.framebuffer_bpp = tag_fb->common.framebuffer_bpp;
	cbi->fb.framebuffer_type = tag_fb->common.framebuffer_type;
	if (tag_fb->common.framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED) {
		cbi->fb.framebuffer_palette_addr = 
			(uint32_t)tag_fb->framebuffer_palette;
		cbi->fb.framebuffer_palette_num_colors = 
			tag_fb->framebuffer_palette_num_colors;
	} else if (tag_fb->common.framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
		cbi->fb.framebuffer_red_field_position =
			tag_fb->framebuffer_red_field_position;
		cbi->fb.framebuffer_red_mask_size =
			tag_fb->framebuffer_red_mask_size;
		cbi->fb.framebuffer_green_field_position =
			tag_fb->framebuffer_green_field_position;
		cbi->fb.framebuffer_green_mask_size =
			tag_fb->framebuffer_green_mask_size;
		cbi->fb.framebuffer_blue_field_position =
			tag_fb->framebuffer_blue_field_position;
		cbi->fb.framebuffer_blue_mask_size =
			tag_fb->framebuffer_blue_mask_size;
	}
}

static void do_tag_module(kinfo_t *cbi, struct multiboot_tag_module *tag_module)
{
	int k = cbi->module_count;
	
	assert(k < MULTIBOOT_MAX_MODS);
	cbi->module_list[k].mod_start = tag_module->mod_start;
	cbi->module_list[k].mod_end = tag_module->mod_end;
	cbi->module_count++;
}

static void do_tag_mmap(kinfo_t *cbi, struct multiboot_tag_mmap *tag_mmap)
{
	struct multiboot_mmap_entry	*mmap;
	u64_t base_addr, length;	/* NB: mmap->addr/len are 64-bit; an
					 * uint32_t here truncated phys addresses
					 * >4GB (and lengths), feeding bogus low
					 * addresses to add_memmap(). */

	for (mmap = tag_mmap->entries;
			(caddr_t)mmap < (caddr_t)tag_mmap + tag_mmap->size;
			mmap = (struct multiboot_mmap_entry *)
				((caddr_t) mmap + tag_mmap->entry_size)) {
		if(mmap->type != MULTIBOOT_MEMORY_AVAILABLE) continue;
		base_addr = mmap->addr;
		length = mmap->len;
		add_memmap(cbi, base_addr, length);
	}
}

static void do_tag_efi_mmap(kinfo_t *cbi, struct multiboot_tag_efi_mmap *tag_efi_mmap)
{
	// TODO
}

static void do_tag_acpi_new(kinfo_t *cbi, struct multiboot_tag_new_acpi *tag_new_acpi)
{
	cbi->rsdp_p = &tag_new_acpi->rsdp;
}

void get_parameters_mb2(u32_t ebx, kinfo_t *cbi) 
{
	struct multiboot_tag *tag, *tag_end;
	multiboot_uint32_t size;
	int m, k;
	phys_bytes kernbase = (phys_bytes) &_kern_phys_base,
	kernsize = (phys_bytes) &_kern_size;
	
	/* Kernel may use memory */
	kernel_may_alloc = 1;

	memset(cbi, 0, sizeof(kinfo_t));
	
	/* Set various bits of info for the higher-level kernel. */
	cbi->boot_mode = 1;
	cbi->mb_version = 2;
	
	cbi->module_count = 0;
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

	size = *(multiboot_uint32_t *)(uintptr_t)ebx;
	tag = (struct multiboot_tag *)(uintptr_t)(ebx + 8);
	tag_end = (struct multiboot_tag *)(uintptr_t)(ebx + size);
	while (tag < tag_end) {
		if (tag->type == MULTIBOOT_TAG_TYPE_CMDLINE) {
			//reset();
			do_tag_cmdline(cbi, (struct multiboot_tag_string *)tag);
		} else if (tag->type == MULTIBOOT_TAG_TYPE_MODULE) {
			//reset();
			do_tag_module(cbi, (struct multiboot_tag_module *)tag);
		} else if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
			//reset();
			do_tag_mmap(cbi, (struct multiboot_tag_mmap *)tag);
		} else if (tag->type == MULTIBOOT_TAG_TYPE_EFI_MMAP) {
			//reset();
			do_tag_efi_mmap(cbi, (struct multiboot_tag_efi_mmap *)tag);
		} else if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
			//reset();
			do_tag_framebuffer(cbi, (struct multiboot_tag_framebuffer *)tag);
		} else if (tag->type == MULTIBOOT_TAG_TYPE_ACPI_NEW) {
			//reset();
			do_tag_acpi_new(cbi, (struct multiboot_tag_new_acpi *)tag);
		} else if (tag->type == MULTIBOOT_TAG_TYPE_END) {
			break;
		} else {
			/* not supported */
			//reset();
		}
		tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag
			+ ((tag->size + 7) & ~7));
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

	assert(!(cbi->bootstrap_start % I386_PAGE_SIZE));
	cbi->bootstrap_len = rounddown(cbi->bootstrap_len, I386_PAGE_SIZE);
	assert(cbi->module_count < MULTIBOOT_MAX_MODS);
	assert(cbi->module_count > 0);
	

	/* Sanity check: the kernel nor any of the modules may overlap
	 * with each other. Pretend the kernel is an extra module for a
	 * second.
	 */
	k = cbi->module_count;
	assert(k < MULTIBOOT_MAX_MODS);
	cbi->module_list[k].mod_start = kernbase;
	cbi->module_list[k].mod_end = kernbase + kernsize;
	cbi->mods_with_kernel = cbi->module_count+1;
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

	/* The legacy 0xA0000-0x100000 region (VGA framebuffer + option/BIOS
	 * ROM shadow) is not usable RAM on PC platforms even when the firmware
	 * memory map reports it as available: under QEMU those pages are
	 * ROM/MMIO and silently drop writes, so any page handed to VM from here
	 * stays full of garbage and corrupts whatever structure lands on it
	 * (slabs, page tables, user heaps).  Carve it out of usable memory. */
	cut_memmap(cbi, 0xA0000, 0x100000);
}

