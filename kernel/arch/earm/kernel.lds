OUTPUT_ARCH("arm")
ENTRY(__k_unpaged_MINIX)

_kern_phys_base = 0x80200000;	/* phys 4MB aligned for convenient remapping */
_kern_vir_base =  0xF0400000;	/* map kernel high for max. user vir space */
_kern_offset = (_kern_vir_base - _kern_phys_base);

__k_unpaged__kern_offset = _kern_offset;
__k_unpaged__kern_vir_base = _kern_vir_base;
__k_unpaged__kern_phys_base = _kern_phys_base;

SECTIONS
{
	. = _kern_phys_base;
	__k_unpaged__kern_unpaged_start = .;

	.unpaged_text ALIGN(4096) : { unpaged_*.o(.text) }
	.unpaged_data ALIGN(4096) : { unpaged_*.o(.data .rodata*) }
	__k_unpaged__kern_unpaged__edata = .;
	__k_unpaged__edata = .;
	.unpaged_bss  ALIGN(4096) : { unpaged_*.o(.bss COMMON) }
	__k_unpaged__kern_unpaged_end = .;
	__k_unpaged__end = .;

	. += _kern_offset;

	. = ALIGN(4096); usermapped_start = .;
	.usermapped_glo : AT(ADDR(.usermapped_glo) - _kern_offset) { usermapped_glo*.o }
	. = ALIGN(4096); usermapped_nonglo_start = .;
	.usermapped : AT(ADDR(.usermapped) - _kern_offset) { usermapped_*.o }
	. = ALIGN(4096); usermapped_end = .;
	.text             : AT(ADDR(.text) - _kern_offset) { *(.text*) }
	_etext = .;
	.data ALIGN(4096) : AT(ADDR(.data) - _kern_offset) { *(.data .rodata* ) }
	. = ALIGN(4096);
	_edata = .;
	.bss ALIGN(4096)  : AT(ADDR(.bss) - _kern_offset) { *(.bss* COMMON)
		__k_unpaged__kern_size = . - _kern_vir_base;
		_kern_size = __k_unpaged__kern_size;
		. += 4096;
	}
	_end = .;

      /DISCARD/ :
       {
               *(.ARM.exidx*)
       }

}
