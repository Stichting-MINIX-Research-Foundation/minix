/*	$NetBSD: termdep.h,v 1.1.1.5 2008/09/02 07:50:08 christos Exp $	*/

/* termdep.h -- system things that terminal.c depends on.
   Id: termdep.h,v 1.2 2004/04/11 17:56:46 karl Exp

   Copyright (C) 1993, 1996, 1997, 1998, 2001, 2002 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

#ifndef INFO_TERMDEP_H
#define INFO_TERMDEP_H

/* NeXT supplies <termios.h> but it is broken.  Probably Autoconf should
   have a separate test, but anyway ... */
#ifdef NeXT
#undef HAVE_TERMIOS_H
#endif

#ifdef HAVE_TERMIOS_H
#  include <termios.h>
#else
#  if defined (HAVE_TERMIO_H)
#    include <termio.h>
#    if defined (HAVE_SYS_PTEM_H)
#      if defined (M_UNIX) || !defined (M_XENIX)
#        include <sys/stream.h>
#        include <sys/ptem.h>
#        undef TIOCGETC
#      else /* M_XENIX */
#        define tchars tc
#      endif /* M_XENIX */
#    endif /* HAVE_SYS_PTEM_H */
#  else /* !HAVE_TERMIO_H */
#    include <sgtty.h>
#  endif /* !HAVE_TERMIO_H */
#endif /* !HAVE_TERMIOS_H */

#ifdef GWINSZ_IN_SYS_IOCTL
#  include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_TTOLD_H
#  include <sys/ttold.h>
#endif /* HAVE_SYS_TTOLD_H */

#endif /* not INFO_TERMDEP_H */
