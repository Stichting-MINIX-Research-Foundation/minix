#include "kernel/kernel.h"
#include <minix/minlib.h>
#include <minix/const.h>
/*
 * == IMPORTANT == 
 * Routines in this file can not use any variable in kernel BSS, 
 * since before image is extracted, no BSS is allocated. 
 * So pay attention to any external call (including library call).
 * 
 * */
#include <minix/types.h>
#include <minix/type.h>
#include <minix/com.h>
#include <sys/param.h>
#include <machine/partition.h>
#include "../../../boot/image.h"
#include "string.h"
#include "arch_proto.h"
#include "libexec.h"
#include <machine/multiboot.h>

#if USE_SYSDEBUG
#define MULTIBOOT_VERBOSE 1
#endif

/* FIXME: Share this define with kernel linker script */
#define MULTIBOOT_KERNEL_ADDR 0x00200000UL

/* Granularity used in image file and copying */
#define GRAN 512
#define SECT_CEIL(x) ((((x) - 1) / GRAN + 1) * GRAN)

/* String length used for mb_itoa */
#define ITOA_BUFFER_SIZE 20

/* Info on boot modules */
phys_bytes mulitboot_modules_addr;

#define mb_load_phymem(buf, phy, len) \
		phys_copy((phy), (u32_t)(buf), (len))

#define mb_save_phymem(buf, phy, len) \
		phys_copy((u32_t)(buf), (phy), (len))

FORWARD _PROTOTYPE(void mb_print, (char *str));

PRIVATE void mb_itoa(u32_t val, char * out) 
{
	char ret[ITOA_BUFFER_SIZE];
	int i = ITOA_BUFFER_SIZE - 2;
	/* Although there's a library version of itoa(int n), 
	* we can't use it since that implementation relies on BSS segment
	*/
	ret[ITOA_BUFFER_SIZE - 2] = '0';
	if (val) {
		for (; i >= 0; i--) {
			char c;
			if (val == 0) break;
			c = val % 10;
			val = val / 10;
				c += '0';
			ret[i] = c;
		}
	}
	else
		i--;
	ret[ITOA_BUFFER_SIZE - 1] = 0;
	strcpy(out, ret + i + 1);
}

PRIVATE void mb_itox(u32_t val, char *out) 
{
	char ret[9];
	int i = 7;
	/* Convert a number to hex string */
	ret[7] = '0';
	if (val) {
		for (; i >= 0; i--) {
			char c;
			if (val == 0) break;
			c = val & 0xF;
			val = val >> 4;
			if (c > 9)
				c += 'A' - 10;
			else
				c += '0';
			ret[i] = c;
		}	
	}
	else
		i--;
	ret[8] = 0;
	strcpy(out, ret + i + 1);
}

PRIVATE void mb_put_char(char c, int line, int col) 
{
	/* Write a char to vga display buffer. */
	if (line<MULTIBOOT_CONSOLE_LINES&&col<MULTIBOOT_CONSOLE_COLS)
		mb_save_phymem(
			&c, 
			MULTIBOOT_VIDEO_BUFFER 
				+ line * MULTIBOOT_CONSOLE_COLS * 2 
				+ col * 2, 
			1);
}

PRIVATE char mb_get_char(int line, int col) 
{
	char c;
	/* Read a char to from display buffer. */
	if (line < MULTIBOOT_CONSOLE_LINES && col < MULTIBOOT_CONSOLE_COLS)
		mb_load_phymem(
			&c, 
			MULTIBOOT_VIDEO_BUFFER 
			+ line * MULTIBOOT_CONSOLE_COLS * 2 
			+ col * 2, 
			1);
	return c;
}

/* Give non-zero values to avoid them in BSS */
PRIVATE int print_line = 1, print_col = 1;
	
PRIVATE void mb_cls(void) 
{
#if MULTIBOOT_VERBOSE
	int i, j;
	/* Clear screen */
	for (i = 0; i < MULTIBOOT_CONSOLE_LINES; i++ )
		for (j = 0; j < MULTIBOOT_CONSOLE_COLS; j++ )
			mb_put_char(0, i, j);
	print_line = print_col = 0;
#endif
}

PRIVATE void mb_scroll_up(int lines) 
{
	int i, j;
	for (i = 0; i < MULTIBOOT_CONSOLE_LINES - lines; i++ ) {
		for (j = 0; j < MULTIBOOT_CONSOLE_COLS; j++ )
			mb_put_char(mb_get_char(i + lines, j), i, j);
	}
	print_line-= lines;
}

PRIVATE void mb_print(char *str) 
{
#if MULTIBOOT_VERBOSE
	while (*str) {
		if (*str == '\n') {
			str++;
			print_line++;
			print_col = 0;
			continue;
		}
		mb_put_char(*str++, print_line, print_col++);
		if (print_col >= MULTIBOOT_CONSOLE_COLS) {
			print_line++;
			print_col = 0;
		}
		while (print_line >= MULTIBOOT_CONSOLE_LINES)
			mb_scroll_up(1);
	}
#endif /* MULTIBOOT_VERBOSE */
}

PRIVATE void mb_print_hex(u32_t value) 
{
	int i;
	char c;
	char out[9] = "00000000";
	/* Print a hex value */
	for (i = 7; i >= 0; i--) {
		c = value % 0x10;
		value /= 0x10;
		if (c < 10) 
			c += '0';
		else
			c += 'A'-10;
		out[i] = c;
	}
	mb_print(out);
}

PRIVATE int mb_set_param(char *name, char *value) 
{
	char *p = multiboot_param_buf;
	char *q;
	int namelen = strlen(name);
	int valuelen = strlen(value);
	
	/* Delete the item if already exists */
	while (*p) {
		if (strncmp(p, name, namelen) == 0 && p[namelen] == '=') {
			q = p;
			while (*q) q++;
			for (q++; 
				q < multiboot_param_buf + MULTIBOOT_PARAM_BUF_SIZE; 
				q++, p++)
				*p = *q;
			break;
		}
		while (*p++)
			;
		p++;
	}
	
	for (p = multiboot_param_buf;
		p < multiboot_param_buf + MULTIBOOT_PARAM_BUF_SIZE 
			&& (*p || *(p + 1));
		p++)
		;
	if (p > multiboot_param_buf) p++;
	
	/* Make sure there's enough space for the new parameter */
	if (p + namelen + valuelen + 3 
		> multiboot_param_buf + MULTIBOOT_PARAM_BUF_SIZE)
		return -1;
	
	strcpy(p, name);
	p[namelen] = '=';
	strcpy(p + namelen + 1, value);
	p[namelen + valuelen + 1] = 0;
	p[namelen + valuelen + 2] = 0;
	return 0;
}

PRIVATE void get_parameters(multiboot_info_t *mbi) 
{
	char mem_value[40], temp[ITOA_BUFFER_SIZE];
	int i, processor;
	int dev;
	int ctrlr;
	int disk, prim, sub;
	int var_i,value_i;
	char *p;
	const static int dev_cNd0[] = { 0x0300, 0x0800, 0x0A00, 0x0C00, 0x1000 };
	static char mb_cmd_buff[GRAN] = "add some value to avoid me in BSS";
	static char var[GRAN] = "add some value to avoid me in BSS";
	static char value[GRAN] = "add some value to avoid me in BSS";
	for (i = 0; i < MULTIBOOT_PARAM_BUF_SIZE; i++)
		multiboot_param_buf[i] = 0;
	
	if (mbi->flags & MULTIBOOT_INFO_BOOTDEV) {
		disk = ((mbi->boot_device&0xff000000) >> 24)-0x80;
		prim = (mbi->boot_device & 0xff0000) >> 16;
		if (prim == 0xff)
		    prim = 0;
		sub = (mbi->boot_device & 0xff00) >> 8;
		if (sub == 0xff)
		    sub = 0;
		ctrlr = 0;
		dev = dev_cNd0[ctrlr];

		/* Determine the value of rootdev */
		dev += 0x80
		    + (disk * NR_PARTITIONS + prim) * NR_PARTITIONS + sub;

		mb_itoa(dev, temp);
		mb_set_param("rootdev", temp);
		mb_set_param("ramimagedev", temp);
	}
	mb_set_param("hz", "60");
	
	if (mbi->flags & MULTIBOOT_INFO_MEMORY)
	{
		strcpy(mem_value, "800:");
		mb_itox(
			mbi->mem_lower * 1024 > MULTIBOOT_LOWER_MEM_MAX ? 
				MULTIBOOT_LOWER_MEM_MAX : mbi->mem_lower * 1024, 
			temp);
		strcat(mem_value, temp);
		strcat(mem_value, ",100000:");
		mb_itox(mbi->mem_upper * 1024, temp);
		strcat(mem_value, temp);
		mb_set_param("memory", mem_value);
	}
	
	if (mbi->flags&MULTIBOOT_INFO_CMDLINE) {
		/* Override values with cmdline argument */
		p = mb_cmd_buff;
		mb_load_phymem(mb_cmd_buff, mbi->cmdline, GRAN);
		while (*p) {
			var_i = 0;
			value_i = 0;
			while (*p == ' ') p++;
			if (!*p) break;
			while (*p && *p != '=' && var_i < GRAN - 1) 
				var[var_i++] = *p++ ;
			var[var_i] = 0;
			p++; /* skip '=' */
			while (*p && *p != ' ' && value_i < GRAN - 1) 
				value[value_i++] = *p++ ;
			value[value_i] = 0;
			
			mb_set_param(var, value);
		}
	}
}

PRIVATE int mb_clear_memrange(phys_bytes start, phys_bytes end)
{
	int empty = 0;
	int i;

	/* FIXME: use faster function */
	for (i = start; i < end; i++)
		phys_copy((phys_bytes)&empty, i, 1);

	return 0;
}

PRIVATE void mb_extract_image(multiboot_info_t mbi)
{
	multiboot_module_t *mb_module_info;
	multiboot_module_t *module;
	u32_t mods_count = mbi.mods_count;
	int r, i;
	vir_bytes text_vaddr, text_filebytes, text_membytes;
	vir_bytes data_vaddr, data_filebytes, data_membytes;
	phys_bytes text_paddr, data_paddr;
	vir_bytes stack_bytes;
	vir_bytes pc;
	off_t text_offset, data_offset;

	/* Save memory map for kernel tasks */
	r = read_header_elf((const char *)MULTIBOOT_KERNEL_ADDR,
			    &text_vaddr, &text_paddr,
			    &text_filebytes, &text_membytes,
			    &data_vaddr, &data_paddr,
			    &data_filebytes, &data_membytes,
			    &pc, &text_offset, &data_offset);

	for (i = 0; i < NR_TASKS; ++i) {
	    image[i].memmap.text_vaddr = trunc_page(text_vaddr);
	    image[i].memmap.text_paddr = trunc_page(text_paddr);
	    image[i].memmap.text_bytes = text_membytes;
	    image[i].memmap.data_vaddr = trunc_page(data_vaddr);
	    image[i].memmap.data_paddr = trunc_page(data_paddr);
	    image[i].memmap.data_bytes = data_membytes;
	    image[i].memmap.stack_bytes = 0;
	    image[i].memmap.entry = pc;
	}

#ifdef MULTIBOOT_VERBOSE
	mb_print("\nKernel:   ");
	mb_print_hex(trunc_page(text_paddr));
	mb_print("-");
	mb_print_hex(trunc_page(data_paddr) + data_membytes);
	mb_print(" Entry: ");
	mb_print_hex(pc);
#endif

	mb_module_info = ((multiboot_module_t *)mbi.mods_addr);
	module = &mb_module_info[0];

	/* Load boot image services into memory and save memory map */
	for (i = 0; module < &mb_module_info[mods_count]; ++module, ++i) {
	    r = read_header_elf((const char *)module->mod_start,
				&text_vaddr, &text_paddr,
				&text_filebytes, &text_membytes,
				&data_vaddr, &data_paddr,
				&data_filebytes, &data_membytes,
				&pc, &text_offset, &data_offset);
	    if (r) {
		mb_print("fatal: ELF parse failure\n");
		/* Spin here */
		while (1)
			;
	    }

	    stack_bytes = round_page(image[NR_TASKS+i].stack_kbytes * 1024);

	    /* Load text segment */
	    phys_copy(module->mod_start+text_offset, text_paddr,
		      text_filebytes);
	    mb_clear_memrange(text_paddr+text_filebytes,
			      trunc_page(text_paddr) + text_membytes);

	    /* Load data and stack segments */
	    phys_copy(module->mod_start+data_offset, data_paddr,
		      data_filebytes);
	    mb_clear_memrange(data_paddr+data_filebytes,
			      trunc_page(data_paddr) + data_membytes
			      + stack_bytes);

	    /* Save memmap for  non-kernel tasks, so subscript past kernel
	       tasks. */
	    image[NR_TASKS+i].memmap.text_vaddr = trunc_page(text_vaddr);
	    image[NR_TASKS+i].memmap.text_paddr = trunc_page(text_paddr);
	    image[NR_TASKS+i].memmap.text_bytes = text_membytes;
	    image[NR_TASKS+i].memmap.data_vaddr = trunc_page(data_vaddr);
	    image[NR_TASKS+i].memmap.data_paddr = trunc_page(data_paddr);
	    image[NR_TASKS+i].memmap.data_bytes = data_membytes;
	    image[NR_TASKS+i].memmap.stack_bytes = stack_bytes;
	    image[NR_TASKS+i].memmap.entry = pc;

#ifdef MULTIBOOT_VERBOSE
	    mb_print("\n");
	    mb_print_hex(i);
	    mb_print(": ");
	    mb_print_hex(trunc_page(text_paddr));
	    mb_print("-");
	    mb_print_hex(trunc_page(data_paddr) + data_membytes + stack_bytes);
	    mb_print(" Entry: ");
	    mb_print_hex(pc);
	    mb_print(" Stack: ");
	    mb_print_hex(stack_bytes);
	    mb_print(" ");
	    mb_print((char *)module->cmdline);
#endif
	}

	return;
}

PUBLIC phys_bytes pre_init(u32_t ebx)
{
	multiboot_info_t mbi;

	/* Do pre-initialization for multiboot, returning physical address of
	*  of multiboot module info
	*/
	mb_cls();
	mb_print("\nMINIX booting... ");
	mb_load_phymem(&mbi, ebx, sizeof(mbi));
	get_parameters(&mbi);
	mb_print("\nLoading image... ");
	mb_extract_image(mbi);
	return mbi.mods_addr;
}
