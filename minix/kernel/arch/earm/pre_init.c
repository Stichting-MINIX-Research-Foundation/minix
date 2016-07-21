#define UNPAGED 1	/* for proper kmain() prototype */

#include "kernel/kernel.h"
#include <assert.h>
#include <stdlib.h>
#include <minix/minlib.h>
#include <minix/const.h>
#include <minix/type.h>
#include <minix/board.h>
#include <minix/com.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include "string.h"
#include "arch_proto.h"
#include "direct_utils.h"
#include "bsp_serial.h"
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

static void setup_mbi(multiboot_info_t *mbi, char *bootargs);

/* String length used for mb_itoa */
#define ITOA_BUFFER_SIZE 20

/* Kernel may use memory */
int kernel_may_alloc = 1;

/* kernel bss */
extern u32_t _edata;
extern u32_t _end;

/* kernel unpaged bss */
extern char _kern_unpaged_edata;
extern char _kern_unpaged_end;

/**
 *
 * The following function combines a few things together
 * that can well be done using standard libc like strlen/strstr
 * and such but these are not available in pre_init stage. 
 *
 * The function expects content to be in the form of space separated
 * key value pairs.
 * param content the contents to search in
 * param key the key to find (this *should* include the key/value delimiter)
 * param value a pointer to an initialized char * of at least value_max_len length
 * param value_max_len the maximum length of the value to store in value including
 *       the end char
 *
**/
int find_value(char * content,char * key,char *value,int value_max_len){

	char *iter,*keyp;
	int key_len,content_len,match_len,value_len;

	/* return if the input is invalid */
	if  (key == NULL || content == NULL || value == NULL) {
		return 1;
	}

	/* find the key and content length */
	key_len = content_len =0;
	for(iter = key ; *iter != '\0'; iter++, key_len++);
	for(iter = content ; *iter != '\0'; iter++, content_len++);

	/* return if key or content length invalid */
	if (key_len == 0 || content_len == 0) {
		return 1;
	}

	/* now find the key in the contents */
	match_len =0;
	for (iter = content ,keyp=key; match_len < key_len && *iter != '\0' ; iter++) {
		if (*iter == *keyp) {
			match_len++;
			keyp++;
			continue;
		} 
		/* The current key does not match the value , reset */
		match_len =0;
		keyp=key;
	}

	if (match_len == key_len) {
		printf("key found at %d %s\n", match_len, &content[match_len]);
		value_len = 0;
		/* copy the content to the value char iter already points to the first 
		   char value */
		while(*iter != '\0' && *iter != ' ' && value_len  + 1< value_max_len) {
			*value++ = *iter++;
			value_len++;
		}
		*value='\0';
		return 0;
	}
	return 1; /* not found */
}

static int mb_set_param(char *bigbuf,char *name,char *value, kinfo_t *cbi)
{
	/* bigbuf contains a list of key=value pairs separated by \0 char.
	 * The list itself is ended by a second \0 terminator*/
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
			/* let q point to the end of the entry */
			while (*q) q++; 
			/* now copy the remained of the buffer */
			for (q++; q < bufend; q++, p++)
				*p = *q;
			break;
		}

		/* find the end of the buffer */
		while (*p++);
		p++;
	}
	

	/* find the first empty spot */
	for (p = bigbuf; p < bufend && (*p || *(p + 1)); p++);

	/* unless we are the first entry step over the delimiter */
	if (p > bigbuf) p++;
	
	/* Make sure there's enough space for the new parameter */
	if (p + namelen + valuelen + 3 > bufend) {
		return -1;
	}
	
	strcpy(p, name);
	p[namelen] = '=';
	strcpy(p + namelen + 1, value);
	p[namelen + valuelen + 1] = 0;
	p[namelen + valuelen + 2] = 0; /* end with a second delimiter */
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
		if(OVERLAP(thismod, cmp)) {
			return 1;
		}
	}
	return 0;
}

/* XXX: hard-coded stuff for modules */
#define MB_MODS_NR NR_BOOT_MODULES
#define MB_MODS_BASE  0x82000000
#define MB_MODS_ALIGN 0x00800000 /* 8 MB */
#define MB_MMAP_START 0x80000000
#define MB_MMAP_SIZE  0x10000000 /* 256 MB */

multiboot_module_t mb_modlist[MB_MODS_NR];
multiboot_memory_map_t mb_memmap;

void setup_mbi(multiboot_info_t *mbi, char *bootargs)
{
	memset(mbi, 0, sizeof(*mbi));
	mbi->flags = MULTIBOOT_INFO_MODS | MULTIBOOT_INFO_MEM_MAP |
			MULTIBOOT_INFO_CMDLINE;
	mbi->mi_mods_count = MB_MODS_NR;
	mbi->mods_addr = (u32_t)&mb_modlist;

	int i;
	for (i = 0; i < MB_MODS_NR; ++i) {
		mb_modlist[i].mod_start = MB_MODS_BASE + i * MB_MODS_ALIGN;
		mb_modlist[i].mod_end = mb_modlist[i].mod_start + MB_MODS_ALIGN
		    - ARM_PAGE_SIZE;
		mb_modlist[i].cmdline = 0;
	}

	/* morph the bootargs into multiboot */
	mbi->cmdline = (u32_t) bootargs;

	mbi->mmap_addr =(u32_t)&mb_memmap;
	mbi->mmap_length = sizeof(mb_memmap);

	mb_memmap.size = sizeof(multiboot_memory_map_t);
	mb_memmap.mm_base_addr = MB_MMAP_START;
	mb_memmap.mm_length  = MB_MMAP_SIZE;
	mb_memmap.type = MULTIBOOT_MEMORY_AVAILABLE;
}

void get_parameters(kinfo_t *cbi, char *bootargs)
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
	setup_mbi(mbi, bootargs);

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
			while (*p == ' ') p++; /* skip spaces */
			if (!*p) break; /* is this the end? */
			while (*p && *p != '=' && *p != ' ' && var_i < BUF - 1)
				var[var_i++] = *p++ ;
			var[var_i] = 0;
			if (*p++ != '=') continue; /* skip if not name=value */
			while (*p && *p != ' ' && value_i < BUF - 1) {
				value[value_i++] = *p++ ;
			}
			value[value_i] = 0;
			
			mb_set_param(cbi->param_buf, var, value, cbi);
		}
	}

	/* let higher levels know what we are booting on */
	mb_set_param(cbi->param_buf, ARCHVARNAME, (char *)get_board_arch_name(machine.board_id), cbi);
	mb_set_param(cbi->param_buf, BOARDVARNAME,(char *)get_board_name(machine.board_id) , cbi);
	

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

	assert(!(cbi->bootstrap_start % ARM_PAGE_SIZE));
	cbi->bootstrap_len = rounddown(cbi->bootstrap_len, ARM_PAGE_SIZE);
	assert(mbi->flags & MULTIBOOT_INFO_MODS);
	assert(mbi->mi_mods_count < MULTIBOOT_MAX_MODS);
	assert(mbi->mi_mods_count > 0);
	memcpy(&cbi->module_list, (void *) mbi->mods_addr,
		mbi->mi_mods_count * sizeof(multiboot_module_t));
	
	memset(cbi->memmap, 0, sizeof(cbi->memmap));
	/* mem_map has a variable layout */
	if(mbi->flags & MULTIBOOT_INFO_MEM_MAP) {
		cbi->mmap_size = 0;
	        for (mmap = (multiboot_memory_map_t *) mbi->mmap_addr;
       	     (unsigned long) mmap < mbi->mmap_addr + mbi->mmap_length;
       	       mmap = (multiboot_memory_map_t *) 
		      	((unsigned long) mmap + mmap->size + sizeof(mmap->size))) {
			if(mmap->type != MULTIBOOT_MEMORY_AVAILABLE) continue;
			add_memmap(cbi, mmap->mm_base_addr, mmap->mm_length);
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
	k = mbi->mi_mods_count;
	assert(k < MULTIBOOT_MAX_MODS);
	cbi->module_list[k].mod_start = kernbase;
	cbi->module_list[k].mod_end = kernbase + kernsize;
	cbi->mods_with_kernel = mbi->mi_mods_count+1;
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

/* 
 * During low level init many things are not supposed to work
 * serial being one of them. We therefore can't rely on the
 * serial to debug. POORMANS_FAILURE_NOTIFICATION can be used
 * before we setup our own vector table and will result in calling
 * the bootloader's debugging methods that will hopefully show some
 * information like the currnet PC at on the serial.
 */
#define POORMANS_FAILURE_NOTIFICATION  asm volatile("svc #00\n")

/* use the passed cmdline argument to determine the machine id */
void set_machine_id(char *cmdline)
{

	char boardname[20];
	memset(boardname,'\0',20);
	if (find_value(cmdline,"board_name=",boardname,20)){
		/* we expect the bootloader to pass a board_name as argument
		 * this however did not happen and given we still are in early
		 * boot we can't use the serial. We therefore generate an interrupt
		 * and hope the bootloader will do something nice with it */
		POORMANS_FAILURE_NOTIFICATION;
	}  
	machine.board_id = get_board_id_by_short_name(boardname);

	if (machine.board_id ==0){
		/* same thing as above there is no safe escape */
		POORMANS_FAILURE_NOTIFICATION;
	}
}

kinfo_t *pre_init(int argc, char **argv)
{
	char *bootargs;
	/* This is the main "c" entry point into the kernel. It gets called
	   from head.S */
	   
	/* Clear BSS */
	memset(&_edata, 0, (u32_t)&_end - (u32_t)&_edata);
        memset(&_kern_unpaged_edata, 0, (u32_t)&_kern_unpaged_end - (u32_t)&_kern_unpaged_edata);

	/* we get called in a c like fashion where the first arg
         * is the program name (load address) and the rest are
	 * arguments. by convention the second argument is the
	 *  command line */
	if (argc != 2) {
		POORMANS_FAILURE_NOTIFICATION;
	}

	bootargs = argv[1];
	set_machine_id(bootargs);
	bsp_ser_init();
	/* Get our own copy boot params pointed to by ebx.
	 * Here we find out whether we should do serial output.
	 */
	get_parameters(&kinfo, bootargs);

	/* Make and load a pagetable that will map the kernel
	 * to where it should be; but first a 1:1 mapping so
	 * this code stays where it should be.
	 */
	dcache_clean(); /* clean the caches */
	pg_clear();
	pg_identity(&kinfo);
	kinfo.freepde_start = pg_mapkernel();
	pg_load();
	vm_enable_paging();

	/* Done, return boot info so it can be passed to kmain(). */
	return &kinfo;
}

/* pre_init gets executed at the memory location where the kernel was loaded by the boot loader.
 * at that stage we only have a minimum set of functionality present (all symbols gets renamed to
 * ensure this). The following methods are used in that context. Once we jump to kmain they are no
 * longer used and the "real" implementations are visible
 */
void send_diag_sig(void) { }
void minix_shutdown(int how) { arch_shutdown(how); }
void busy_delay_ms(int x) { }
int raise(int n) { panic("raise(%d)\n", n); }
int kern_phys_map_ptr( phys_bytes base_address, vir_bytes io_size, int vm_flags,
struct kern_phys_map * priv, vir_bytes ptr) { return -1; };
struct machine machine; /* pre init stage machine */
