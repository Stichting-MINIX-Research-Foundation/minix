/* $DragonFly: src/gnu/usr.bin/cc41/cc_prep/config/i386/dragonfly64.h,v 1.1 2007/01/15 17:53:16 corecode Exp $ */

/* Definitions for AMD x86-64 running DragonFly with ELF format
   Copyright (C) 2002 Free Software Foundation, Inc.
   Contributed by David O'Brien <obrien@FreeBSD.org>
   Adapted from the FreeBSD version.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* $FreeBSD: src/contrib/gcc/config/i386/freebsd64.h,v 1.9 2004/07/28 04:44:23 kan Exp $ */


#undef  TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (x86-64 DragonFly/ELF)");

/* Tell final.c that we don't need a label passed to mcount.  */

#define SUBTARGET_EXTRA_SPECS \
  { "dfbsd_dynamic_linker", DFBSD_DYNAMIC_LINKER }

#undef  MCOUNT_NAME
#define MCOUNT_NAME ".mcount"

/* Provide a LINK_SPEC appropriate for the DragonFly/x86-64 ELF target.
   This is a copy of LINK_SPEC from <i386/dragonfly.h> tweaked for
   the x86-64 target.
   XXX We don't support two arch userland yet  */

#undef	LINK_SPEC
#define LINK_SPEC "\
  %{v:-V} \
  %{assert*} %{R*} %{rpath*} %{defsym*} \
  %{shared:-Bshareable %{h*} %{soname*}} \
    %{!shared: \
      %{!static: \
        %{rdynamic:-export-dynamic} \
	%{!dynamic-linker:-dynamic-linker %(dfbsd_dynamic_linker) }} \
    %{static:-Bstatic}} \
  %{symbolic:-Bsymbolic}"
