# This file is automatically generated.  DO NOT EDIT!
# Generated from: NetBSD: mknative-binutils,v 1.13 2020/04/04 01:34:53 christos Exp 
# Generated from: NetBSD: mknative.common,v 1.16 2018/04/15 15:13:37 christos Exp 
#
G_libbfd_la_DEPENDENCIES=elf64-riscv.lo elf64.lo elfxx-riscv.lo elf32.lo elf.lo elflink.lo elf-attrs.lo elf-strtab.lo elf-eh-frame.lo dwarf1.lo dwarf2.lo elf32-riscv.lo elf64-gen.lo elf32-gen.lo plugin.lo cpu-riscv.lo  archive64.lo ofiles
G_libbfd_la_OBJECTS=archive.lo archures.lo bfd.lo bfdio.lo bfdwin.lo  cache.lo coff-bfd.lo compress.lo corefile.lo elf-properties.lo  format.lo hash.lo init.lo libbfd.lo linker.lo merge.lo  opncls.lo reloc.lo section.lo simple.lo stab-syms.lo stabs.lo  syms.lo targets.lo binary.lo ihex.lo srec.lo tekhex.lo  verilog.lo
G_DEFS=-DHAVE_CONFIG_H
G_INCLUDES=
G_TDEFAULTS=-DDEFAULT_VECTOR=riscv_elf64_vec -DSELECT_VECS='&riscv_elf64_vec,&riscv_elf32_vec,&elf64_le_vec,&elf64_be_vec,&elf32_le_vec,&elf32_be_vec' -DSELECT_ARCHITECTURES='&bfd_riscv_arch'
G_HAVEVECS=-DHAVE_riscv_elf64_vec -DHAVE_riscv_elf32_vec -DHAVE_elf64_le_vec -DHAVE_elf64_be_vec -DHAVE_elf32_le_vec -DHAVE_elf32_be_vec
