/*	$NetBSD: elf_x86_64_fbsd_efi.lds,v 1.1.1.2 2018/08/16 18:17:47 jmcneill Exp $	*/

/* Same as elf_x86_64_efi.lds, except for OUTPUT_FORMAT below - KEEP IN SYNC */
OUTPUT_FORMAT("elf64-x86-64-freebsd", "elf64-x86-64-freebsd", "elf64-x86-64-freebsd")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
SECTIONS
{
  . = 0;
  ImageBase = .;
  /* .hash and/or .gnu.hash MUST come first! */
  .hash : { *(.hash) }
  .gnu.hash : { *(.gnu.hash) }
  . = ALIGN(4096);
  .eh_frame : 
  { 
    *(.eh_frame)
  }
  . = ALIGN(4096);
  .text :
  {
   _text = .;
   *(.text)
   . = ALIGN(16);
  }
  _etext = .;
  _text_size = . - _text;
  .reloc :
  {
   *(.reloc)
  }
  . = ALIGN(4096);
  .data :
  {
   _data = .;
   *(.rodata*)
   *(.got.plt)
   *(.got)
   *(.data*)
   *(.sdata)
   /* the EFI loader doesn't seem to like a .bss section, so we stick
      it all into .data: */
   *(.sbss)
   *(.scommon)
   *(.dynbss)
   *(.bss)
   *(COMMON)
   *(.rel.local)
  }
  .note.gnu.build-id : { *(.note.gnu.build-id) }

  . = ALIGN(4096);
  .dynamic  : { *(.dynamic) }
  . = ALIGN(4096);
  .rela :
  {
    *(.rela.data*)
    *(.rela.got)
    *(.rela.stab)
  }
  _edata = .;
  _data_size = . - _etext;
  . = ALIGN(4096);
  .dynsym   : { *(.dynsym) }
  . = ALIGN(4096);
  .dynstr   : { *(.dynstr) }
  . = ALIGN(4096);
  .ignored.reloc :
  {
    *(.rela.reloc)
  }
}
