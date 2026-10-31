	-------------------------------------------------
	Building EFI Applications Using the GNU Toolchain
	-------------------------------------------------

		David Mosberger <davidm@hpl.hp.com>

			23 September 1999


		Copyright (c) 1999-2007 Hewlett-Packard Co.
		Copyright (c) 2006-2010 Intel Co.

Last update: 04/09/2007

* Introduction

This document has two parts: the first part describes how to develop
EFI applications for IA-64,x86 and x86_64 using the GNU toolchain and the EFI
development environment contained in this directory.  The second part
describes some of the more subtle aspects of how this development
environment works.



* Part 1: Developing EFI Applications


** Prerequisites:

 To develop x86 and x86_64 EFI applications, the following tools are needed:

	- gcc-3.0 or newer (gcc 2.7.2 is NOT sufficient!)
	  As of gnu-efi-3.0b, the Redhat 8.0 toolchain is known to work,
	  but the Redhat 9.0 toolchain is not currently supported.

	- A version of "objcopy" that supports EFI applications.  To
	  check if your version includes EFI support, issue the
	  command:

		objcopy --help

	  Verify that the line "supported targets" contains the string
	  "efi-app-ia32" and "efi-app-x86_64" and that the "-j" option
	  accepts wildcards. The binutils release binutils-2.24
	  supports Intel64 EFI and accepts wildcard section names.

	- For debugging purposes, it's useful to have a version of
	  "objdump" that supports EFI applications as well.  This
	  allows inspect and disassemble EFI binaries.

 To develop IA-64 EFI applications, the following tools are needed:

	- A version of gcc newer than July 30th 1999 (older versions
	  had problems with generating position independent code).
	  As of gnu-efi-3.0b, gcc-3.1 is known to work well.

	- A version of "objcopy" that supports EFI applications.  To
	  check if your version includes EFI support, issue the
	  command:

		objcopy --help

	  Verify that the line "supported targets" contains the string
	  "efi-app-ia64" and that the "-j" option accepts wildcards.

	- For debugging purposes, it's useful to have a version of
	  "objdump" that supports EFI applications as well.  This
	  allows inspect and disassemble EFI binaries.


** Directory Structure

This EFI development environment contains the following
subdirectories:

 inc:   This directory contains the EFI-related include files.  The
	files are taken from Intel's EFI source distribution, except
	that various fixes were applied to make it compile with the
	GNU toolchain.

 lib:   This directory contains the source code for Intel's EFI library.
	Again, the files are taken from Intel's EFI source
	distribution, with changes to make them compile with the GNU
	toolchain.

 gnuefi: This directory contains the glue necessary to convert ELF64
	binaries to EFI binaries.  Various runtime code bits, such as
	a self-relocator are included as well.  This code has been
	contributed by the Hewlett-Packard Company and is distributed
	under the GNU GPL.

 apps:	This directory contains a few simple EFI test apps.

** Setup

It is necessary to edit the Makefile in the directory containing this
README file before EFI applications can be built.  Specifically, you
should verify that macros CC, AS, LD, AR, RANLIB, and OBJCOPY point to
the appropriate compiler, assembler, linker, ar, and ranlib binaries,
respectively.

If you're working in a cross-development environment, be sure to set
macro ARCH to the desired target architecture ("ia32" for x86, "x86_64" for
x86_64 and "ia64" for IA-64).  For convenience, this can also be done from
the make command line (e.g., "make ARCH=ia64").


** Building

To build the sample EFI applications provided in subdirectory "apps",
simply invoke "make" in the toplevel directory (the directory
containing this README file).  This should build lib/libefi.a and
gnuefi/libgnuefi.a first and then all the EFI applications such as a
apps/t6.efi.


** Running

Just copy the EFI application (e.g., apps/t6.efi) to the EFI
filesystem, boot EFI, and then select "Invoke EFI application" to run
the application you want to test.  Alternatively, you can invoke the
Intel-provided "nshell" application and then invoke your test binary
via the command line interface that "nshell" provides.


** Writing Your Own EFI Application

Suppose you have your own EFI application in a file called
"apps/myefiapp.c".  To get this application built by the GNU EFI build
environment, simply add "myefiapp.efi" to macro TARGETS in
apps/Makefile.  Once this is done, invoke "make" in the top level
directory.  This should result in EFI application apps/myefiapp.efi,
ready for execution.

The GNU EFI build environment allows to write EFI applications as
described in Intel's EFI documentation, except for two differences:

 - The EFI application's entry point is always called "efi_main".  The
   declaration of this routine is:

    EFI_STATUS efi_main (EFI_HANDLE image, EFI_SYSTEM_TABLE *systab);

 - UNICODE string literals must be written as W2U(L"Sample String")
   instead of just L"Sample String".  The W2U() macro is defined in
   <efilib.h>.  This header file also declares the function W2UCpy()
   which allows to convert a wide string into a UNICODE string and
   store the result in a programmer-supplied buffer.

 - Calls to EFI services should be made via uefi_call_wrapper(). This
   ensures appropriate parameter passing for the architecture.


* Part 2: Inner Workings

WARNING: This part contains all the gory detail of how the GNU EFI
toolchain works.  Normal users do not have to worry about such
details.  Reading this part incurs a definite risk of inducing severe
headaches or other maladies.

The basic idea behind the GNU EFI build environment is to use the GNU
toolchain to build a normal ELF binary that, at the end, is converted
to an EFI binary.  EFI binaries are really just PE32+ binaries.  PE
stands for "Portable Executable" and is the object file format
Microsoft is using on its Windows platforms.  PE is basically the COFF
object file format with an MS-DOS2.0 compatible header slapped on in
front of it.  The "32" in PE32+ stands for 32 bits, meaning that PE32
is a 32-bit object file format.  The plus in "PE32+" indicates that
this format has been hacked to allow loading a 4GB binary anywhere in
a 64-bit address space (unlike ELF64, however, this is not a full
64-bit object file format because the entire binary cannot span more
than 4GB of address space).  EFI binaries are plain PE32+ binaries
except that the "subsystem id" differs from normal Windows binaries.
There are two flavors of EFI binaries: "applications" and "drivers"
and each has there own subsystem id and are identical otherwise.  At
present, the GNU EFI build environment supports the building of EFI
applications only, though it would be trivial to generate drivers, as
the only difference is the subsystem id.  For more details on PE32+,
see the spec at

	http://msdn.microsoft.com/library/specs/msdn_pecoff.htm.

In theory, converting a suitable ELF64 binary to PE32+ is easy and
could be accomplished with the "objcopy" utility by specifying option
--target=efi-app-ia32 (x86) or --target=efi-app-ia64 (IA-64).  But
life never is that easy, so here some complicating factors:

 (1) COFF sections are very different from ELF sections.

	ELF binaries distinguish between program headers and sections.
	The program headers describe the memory segments that need to
	be loaded/initialized, whereas the sections describe what
	constitutes those segments.  In COFF (and therefore PE32+) no
	such distinction is made.  Thus, COFF sections need to be page
	aligned and have a size that is a multiple of the page size
	(4KB for EFI), whereas ELF allows sections at arbitrary
	addresses and with arbitrary sizes.

 (2) EFI binaries should be relocatable.

	Since EFI binaries are executed in physical mode, EFI cannot
	guarantee that a given binary can be loaded at its preferred
	address.  EFI does _try_ to load a binary at it's preferred
	address, but if it can't do so, it will load it at another
	address and then relocate the binary using the contents of the
	.reloc section.

 (3) On IA-64, the EFI entry point needs to point to a function
     descriptor, not to the code address of the entry point.

 (4) The EFI specification assumes that wide characters use UNICODE
     encoding.

	ANSI C does not specify the size or encoding that a wide
	character uses.  These choices are "implementation defined".
	On most UNIX systems, the GNU toolchain uses a wchar_t that is
	4 bytes in size.  The encoding used for such characters is
	(mostly) UCS4.

In the following sections, we address how the GNU EFI build
environment addresses each of these issues.


** (1) Accommodating COFF Sections

In order to satisfy the COFF constraint of page-sized and page-aligned
sections, the GNU EFI build environment uses the special linker script
in gnuefi/elf_$(ARCH)_efi.lds where $(ARCH) is the target architecture
("ia32" for x86, "x86_64" for x86_64 and "ia64" for IA-64).
This script is set up to create only eight COFF section, each page aligned
and page sized.These eight sections are used to group together the much
greater number of sections that are typically present in ELF object files.
Specifically:

 .hash (and/or .gnu.hash)
	Collects the ELF .hash info (this section _must_ be the first
	section in order to build a shared object file; the section is
	not actually loaded or used at runtime).

	GNU binutils provides a mechanism to generate different hash info
	via --hash-style=<sysv|gnu|both> option. In this case output
	shared object will contain .hash section, .gnu.hash section or
	both. In order to generate correct output linker script preserves
	both types of hash sections.

 .text
	Collects all sections containing executable code.

 .data
	Collects read-only and read-write data, literal string data,
	global offset tables, the uninitialized data segment (bss) and
	various other sections containing data.

	The reason read-only data is placed here instead of the in
	.text is to make it possible to disassemble the .text section
	without getting garbage due to read-only data.  Besides, since
	EFI binaries execute in physical mode, differences in page
	protection do not matter.

	The reason the uninitialized data is placed in this section is
	that the EFI loader appears to be unable to handle sections
	that are allocated but not loaded from the binary.

 .dynamic, .dynsym, .rela, .rel, .reloc
	These sections contains the dynamic information necessary to
	self-relocate the binary (see below).

A couple of more points worth noting about the linker script:

 o On IA-64, the global pointer symbol (__gp) needs to be placed such
   that the _entire_ EFI binary can be addressed using the signed
   22-bit offset that the "addl" instruction affords.  Specifically,
   this means that __gp should be placed at ImageBase + 0x200000.
   Strictly speaking, only a couple of symbols need to be addressable
   in this fashion, so with some care it should be possible to build
   binaries much larger than 4MB.  To get a list of symbols that need
   to be addressable in this fashion, grep the assembly files in
   directory gnuefi for the string "@gprel".

 o The link address (ImageBase) of the binary is (arbitrarily) set to
   zero.  This could be set to something larger to increase the chance
   of EFI being able to load the binary without requiring relocation.
   However, a start address of 0 makes debugging a wee bit easier
   (great for those of us who can add, but not subtract... ;-).

 o The relocation related sections (.dynamic, .rel, .rela, .reloc)
   cannot be placed inside .data because some tools in the GNU
   toolchain rely on the existence of these sections.

 o Some sections in the ELF binary intentionally get dropped when
   building the EFI binary.  Particularly noteworthy are the dynamic
   relocation sections for the .plabel and .reloc sections.  It would
   be _wrong_ to include these sections in the EFI binary because it
   would result in .reloc and .plabel being relocated twice (once by
   the EFI loader and once by the self-relocator; see below for a
   description of the latter).  Specifically, only the sections
   mentioned with the -j option in the final "objcopy" command are
   retained in the EFI binary (see Make.rules).


** (2) Building Relocatable Binaries

ELF binaries are normally linked for a fixed load address and are thus
not relocatable.  The only kind of ELF object that is relocatable are
shared objects ("shared libraries").  However, even those objects are
usually not completely position independent and therefore require
runtime relocation by the dynamic loader.  For example, IA-64 binaries
normally require relocation of the global offset table.

The approach to building relocatable binaries in the GNU EFI build
environment is to:

 (a) build an ELF shared object

 (b) link it together with a self-relocator that takes care of
     applying the dynamic relocations that may be present in the
     ELF shared object

 (c) convert the resulting image to an EFI binary

The self-relocator is of course architecture dependent.  The x86
version can be found in gnuefi/reloc_ia32.c, the x86_64 version
can be found in gnuefi/reloc_x86_64.c and the IA-64 version can be
found in gnuefi/reloc_ia64.S.

The self-relocator operates as follows: the startup code invokes it
right after EFI has handed off control to the EFI binary at symbol
"_start".  Upon activation, the self-relocator searches the .dynamic
section (whose starting address is given by symbol _DYNAMIC) for the
dynamic relocation information, which can be found in the DT_REL,
DT_RELSZ, and DT_RELENT entries of the dynamic table (DT_RELA,
DT_RELASZ, and DT_RELAENT in the case of rela relocations, as is the
case for IA-64).  The dynamic relocation information points to the ELF
relocation table.  Once this table is found, the self-relocator walks
through it, applying each relocation one by one.  Since the EFI
binaries are fully resolved shared objects, only a subset of all
possible relocations need to be supported.  Specifically, on x86 only
the R_386_RELATIVE relocation is needed.  On IA-64, the relocations
R_IA64_DIR64LSB, R_IA64_REL64LSB, and R_IA64_FPTR64LSB are needed.
Note that the R_IA64_FPTR64LSB relocation requires access to the
dynamic symbol table.  This is why the .dynsym section is included in
the EFI binary.  Another complication is that this relocation requires
memory to hold the function descriptors (aka "procedure labels" or
"plabels").  Each function descriptor uses 16 bytes of memory.  The
IA-64 self-relocator currently reserves a static memory area that can
hold 100 of these descriptors.  If the self-relocator runs out of
space, it causes the EFI binary to fail with error code 5
(EFI_BUFFER_TOO_SMALL).  When this happens, the manifest constant
MAX_FUNCTION_DESCRIPTORS in gnuefi/reloc_ia64.S should be increased
and the application recompiled.  An easy way to count the number of
function descriptors required by an EFI application is to run the
command:

  objdump --dynamic-reloc example.so | fgrep FPTR64 | wc -l

assuming "example" is the name of the desired EFI application.


** (3) Creating the Function Descriptor for the IA-64 EFI Binaries

As mentioned above, the IA-64 PE32+ format assumes that the entry
point of the binary is a function descriptor.  A function descriptors
consists of two double words: the first one is the code entry point
and the second is the global pointer that should be loaded before
calling the entry point.  Since the ELF toolchain doesn't know how to
generate a function descriptor for the entry point, the startup code
in gnuefi/crt0-efi-ia64.S crafts one manually by with the code:

	        .section .plabel, "a"
	_start_plabel:
	        data8   _start
	        data8   __gp

this places the procedure label for entry point _start in a section
called ".plabel".  Now, the only problem is that _start and __gp need
to be relocated _before_ EFI hands control over to the EFI binary.
Fortunately, PE32+ defines a section called ".reloc" that can achieve
this.  Thus, in addition to manually crafting the function descriptor,
the startup code also crafts a ".reloc" section that has will cause
the EFI loader to relocate the function descriptor before handing over
control to the EFI binary (again, see the PECOFF spec mentioned above
for details).

A final question may be why .plabel and .reloc need to go in their own
COFF sections.  The answer is simply: we need to be able to discard
the relocation entries that are generated for these sections.  By
placing them in these sections, the relocations end up in sections
".rela.plabel" and ".rela.reloc" which makes it easy to filter them
out in the filter script.  Also, the ".reloc" section needs to be in
its own section so that the objcopy program can recognize it and can
create the correct directory entries in the PE32+ binary.


** (4) Convenient and Portable Generation of UNICODE String Literals

As of gnu-efi-3.0, we make use (and somewhat abuse) the gcc option
that forces wide characters (WCHAR_T) to use short integers (2 bytes) 
instead of integers (4 bytes). This way we match the Unicode character
size. By abuse, we mean that we rely on the fact that the regular ASCII
characters are encoded the same way between (short) wide characters 
and Unicode and basically only use the first byte. This allows us
to just use them interchangeably.

The gcc option to force short wide characters is : -fshort-wchar

			* * * The End * * *
