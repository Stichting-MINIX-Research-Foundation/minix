/*	$NetBSD: defun.h,v 1.1.1.4 2008/09/02 07:50:26 christos Exp $	*/

/* defun.h -- declaration for defuns.
   Id: defun.h,v 1.2 2004/04/11 17:56:47 karl Exp

   Copyright (C) 1999 Free Software Foundation, Inc.

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

   Written by Karl Heinz Marbaise <kama@hippo.fido.de>.  */

#ifndef DEFUN_H
#define DEFUN_H

#include "insertion.h"

extern enum insertion_type get_base_type (int type);
extern void cm_defun (void);

#endif /* !DEFUN_H */

