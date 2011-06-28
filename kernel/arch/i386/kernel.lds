OUTPUT_ARCH("i386")
ENTRY(MINIX)
SECTIONS
{
	. = 0x200000 + SIZEOF_HEADERS;
	.text . : AT (ADDR(.text) - 0x0000) {
		*(.text)
		*(.text.*)
		. = ALIGN(4096);
	}
	_etext = .;
	etext = .;

	.data . : AT (ADDR(.data) - 0x0000) {
		_rodata = .;
		/* kernel data starts with this magic number */
		SHORT(0x526f);
		*(.rodata)
		*(.rodata.*)
		_erodata = .;
		*(.data)
		*(.data.*)
		. = ALIGN(4096);
	}
	_edata = .;

	.bss . : AT (ADDR(.bss) - 0x0000) {
		*(.bss)
		*(.bss.*)
		*(COMMON)
	}
	_end = .;
	end = .;

	/DISCARD/ :
	{
		*(.eh_frame)
		*(.comment)
		*(.comment.*)
		*(.note)
		*(.note.*)
	}
}
