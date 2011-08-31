
#define EMEM_ENTRIES	16
#define EMEM_SIZE	24	/* size in bytes of e820_memory struct */
#define MEM_ENTRIES	3

#ifndef __ASSEMBLY__

typedef struct {		/* One chunk of free memory. */
	u32_t	base;		/* Start byte. */
	u32_t	size;		/* Number of bytes. */
} memory;

EXTERN memory mem[MEM_ENTRIES];		/* List of available memory. */

typedef struct {		/* One chunk of free memory. */
	u32_t	base_lo;	/* Start byte. */
	u32_t	base_hi;
	u32_t	size_lo;	/* Number of bytes. */
	u32_t	size_hi;	/* Number of bytes. */
	u32_t	type;
	u32_t	acpi_attrs;
} e820_memory;

EXTERN e820_memory emem[EMEM_ENTRIES];	/* List of available memory. */
EXTERN int emem_entries;
#endif
