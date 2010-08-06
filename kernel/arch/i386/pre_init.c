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
#include <minix/a.out.h>
#include <machine/partition.h>
#include "../../../boot/image.h"
#include "string.h"
#include "proto.h"
#include "multiboot.h"

/* Granularity used in image file and copying */
#define GRAN 512
#define SECT_CEIL(x) ((((x) - 1) / GRAN + 1) * GRAN)

/* String length used for mb_itoa */
#define ITOA_BUFFER_SIZE 20

/* The a.out headers to pass to kernel. 
 * Not using struct exec because only using short form */
extern char a_out_headers[];

#define mb_load_phymem(buf, phy, len) \
		phys_copy((phy), PTR2PHY(buf), (len))

#define mb_save_phymem(buf, phy, len) \
		phys_copy(PTR2PHY(buf), (phy), (len))

FORWARD _PROTOTYPE(void mb_print, (char *str));

PRIVATE void mb_phys_move(u32_t src, u32_t dest, u32_t len) 
{
	char data[GRAN + 1];
	int i;
	/* Move upward (start moving from tail), block by block 
	* len should be aligned to GRAN 
	*/
	if (len % GRAN) {
		mb_print("fatal: not aligned phys move");
		/* Spin here */
		while (1)
			;
	}

	len /= GRAN;
	for (i = len - 1; i >= 0; i--) {
		mb_load_phymem(data, src + i * GRAN, GRAN);
		mb_save_phymem(data, dest + i * GRAN, GRAN);
	}
}

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
	int i, j;
	/* Clear screen */
	for (i = 0; i < MULTIBOOT_CONSOLE_LINES; i++ )
		for (j = 0; j < MULTIBOOT_CONSOLE_COLS; j++ )
			mb_put_char(0, i, j);
	print_line = print_col = 0;
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
	int i, r, processor;
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
		sub = 0xff;
		disk = ((mbi->boot_device&0xff000000) >> 24)-0x80;
		prim = (mbi->boot_device&0xff0000) == 0xff0000 ? 
			0 : (mbi->boot_device & 0xff0000) >> 16;
		ctrlr = 0;
		dev = dev_cNd0[ctrlr];
		/* Determine the value of rootdev */
		if ((mbi->boot_device & 0xff00) == 0xff00) {
			dev += disk * (NR_PARTITIONS + 1) + (prim + 1);
		} else {
			sub = (mbi->boot_device & 0xff00) >> 8;
			dev += 0x80
				 + (disk * NR_PARTITIONS + prim) * NR_PARTITIONS 
				 + sub;
		}
		mb_itoa(dev, temp);
		mb_set_param("rootdev", temp);
		mb_set_param("ramimagedev", temp);
	}
	mb_set_param("ramsize", "0");
	mb_set_param("hz", "60");
	processor = getprocessor();
	if (processor == 1586) processor = 686;
	mb_itoa(processor, temp);
	mb_set_param("processor", temp);
	mb_set_param("bus", "at");
	mb_set_param("video", "ega");
	mb_set_param("chrome", "color");
	
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
	
	/* FIXME: this is dummy value, 
	 * we can't get real image file name from multiboot */
	mb_set_param("image", "boot/image_latest");
	
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

PRIVATE void mb_extract_image(void)
{
	int i;
	u32_t text_addr[NR_BOOT_PROCS];
	u32_t imghdr_addr = MULTIBOOT_LOAD_ADDRESS;
	int off_sum = 0;
	struct exec *aout_hdr;
	int empty, clear_size, j;
	u32_t p;
	/* Extract the image to align segments and clear up BSS 
	 */
	for (i = 0; i < LAST_SPECIAL_PROC_NR + 2; i++) {
		aout_hdr = (struct exec *) (a_out_headers + A_MINHDR * i);
		mb_load_phymem(aout_hdr, imghdr_addr + IM_NAME_MAX + 1, A_MINHDR);
		text_addr[i] = imghdr_addr + GRAN;
		if (aout_hdr->a_flags & A_SEP) {
			off_sum += CLICK_CEIL(aout_hdr->a_total)
					- SECT_CEIL(aout_hdr->a_data)
					+ CLICK_CEIL(aout_hdr->a_text)
					- SECT_CEIL(aout_hdr->a_text)
					- GRAN;
			imghdr_addr += SECT_CEIL(aout_hdr->a_text)
					+ SECT_CEIL(aout_hdr->a_data)
					+ GRAN;
		} else {
			off_sum += CLICK_CEIL(aout_hdr->a_total) 
					- SECT_CEIL(aout_hdr->a_data + aout_hdr->a_text)
					- GRAN;
			imghdr_addr += SECT_CEIL(aout_hdr->a_text + aout_hdr->a_data)
					+ GRAN;
		}
	}
	for (i = LAST_SPECIAL_PROC_NR + 1; i >= 0;i--) {
		struct exec * aout_hdr = (struct exec *) (a_out_headers + A_MINHDR * i);
		if (aout_hdr->a_flags & A_SEP)
			off_sum -= CLICK_CEIL(aout_hdr->a_total) 
					- SECT_CEIL(aout_hdr->a_data)
					+ CLICK_CEIL(aout_hdr->a_text)
					- SECT_CEIL(aout_hdr->a_text)
					- GRAN;
		else
			off_sum -= CLICK_CEIL(aout_hdr->a_total) 
					- SECT_CEIL(aout_hdr->a_data + aout_hdr->a_text)
					- GRAN;
		if (i > 0) { /* if not kernel */
			if (aout_hdr->a_flags & A_SEP)	{
				mb_phys_move(text_addr[i], text_addr[i] + off_sum, 
					SECT_CEIL(aout_hdr->a_text));
				mb_phys_move(text_addr[i] + SECT_CEIL(aout_hdr->a_text), 
					text_addr[i] + off_sum + CLICK_CEIL(aout_hdr->a_text), 
					SECT_CEIL(aout_hdr->a_data));
			} else {
				mb_phys_move(text_addr[i], text_addr[i] + off_sum, 
					SECT_CEIL(aout_hdr->a_text + aout_hdr->a_data));
			}
		}
		aout_hdr->a_syms = text_addr[i] + off_sum;
		
		/* Clear out for expanded text, BSS and stack */
		empty = 0;
		if (aout_hdr->a_flags & A_SEP) {
			p = text_addr[i] + off_sum
				+ CLICK_CEIL(aout_hdr->a_text) 
				+ aout_hdr->a_data;
			clear_size = CLICK_CEIL(aout_hdr->a_total) - aout_hdr->a_data;
		} else {
			p = text_addr[i] + off_sum
				+ aout_hdr->a_text
				+ aout_hdr->a_data;
			clear_size = CLICK_CEIL(aout_hdr->a_total) 
				- aout_hdr->a_data
				- aout_hdr->a_text;
		}
		/* FIXME: use faster function */
		for (j = 0; j < clear_size; j++)
			mb_save_phymem(&empty, p + j, 1);
	}
}

PUBLIC u32_t pre_init(u32_t ebx)
{
	multiboot_info_t mbi;
	/* Do pre-initialization for multiboot, returning physical address of
	* a_out_headers 
	*/
	mb_cls();
	mb_print("\nMINIX booting... ");
	mb_load_phymem(&mbi, ebx, sizeof(mbi));
	get_parameters(&mbi);
	mb_print("\nLoading image... ");
	mb_extract_image();
	return PTR2PHY(a_out_headers);
}
