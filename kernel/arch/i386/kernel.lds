OUTPUT_ARCH("i386")
ENTRY(MINIX)
SECTIONS
{
	. = 0x0;
	_text = .;
	begtext = .;
	.text.head . : AT (0x0) {
		*(.text.head)
	}
	.text . : AT (ADDR(.text) - 0x0000) {
		*(.text)
		*(.text.*)
		. = ALIGN(4096);
	}
	_etext = .;
	etext = .;

	_data = .;
	begdata = .;
	.data . : AT (ADDR(.data) - 0x0000) {
		_rodata = .;
		/* kernel data starts with this magic number */
		SHORT(0x526f);
		*(.rodata)
		*(.rodata.*)
		_erodata = .;
		*(.data)
		*(.data.*)
	}
	_edata = .;
	_bss = .;
	begbss = .;
	.bss . : AT (ADDR(.bss) - 0x0000) {
		*(.bss)
		*(.bss.*)
		*(COMMON)
	}
	_ebss = .;
	endbss = .;
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
