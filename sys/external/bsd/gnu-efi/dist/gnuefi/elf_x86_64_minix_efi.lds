/* MINIX version */
/* Same as elf_x86_64_efi.lds, except for OUTPUT_FORMAT below - KEEP IN SYNC */
OUTPUT_FORMAT("elf64-x86-64-minix", "elf64-x86-64-minix", "elf64-x86-64-minix")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
PHDRS
{
  text PT_LOAD;
  data PT_LOAD;
  dyn  PT_DYNAMIC;
}
SECTIONS
{
  . = 0;
  ImageBase = .;
  /* .hash and/or .gnu.hash MUST come first! */
  .hash : { *(.hash) } :text
  .gnu.hash : { *(.gnu.hash) } :text
  . = ALIGN(4096);
  .eh_frame :
  {
    *(.eh_frame)
  } :text
  . = ALIGN(4096);
  .text :
  {
   _text = .;
   *(.text)
   *(.text.*)
   *(.gnu.linkonce.t.*)
   . = ALIGN(16);
  } :text
  _etext = .;
  _text_size = . - _text;
  . = ALIGN(4096);
  .reloc :
  {
   *(.reloc)
  } :text
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
  } :data
  .note.gnu.build-id : { *(.note.gnu.build-id) } :data

  _edata = .;
  _data_size = . - _etext;
  . = ALIGN(4096);
  .dynamic  : { *(.dynamic) } :data :dyn
  . = ALIGN(4096);
  .rela :
  {
    *(.rela.data*)
    *(.rela.got)
    *(.rela.stab)
  } :data
  . = ALIGN(4096);
  .dynsym   : { *(.dynsym) } :data
  . = ALIGN(4096);
  .dynstr   : { *(.dynstr) } :data
  . = ALIGN(4096);
  .ignored.reloc :
  {
    *(.rela.reloc)
    *(.eh_frame)
    *(.note.GNU-stack)
  }
  .comment 0 : { *(.comment) }
}
