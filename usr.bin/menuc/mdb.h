/*	$NetBSD: mdb.h,v 1.9 2012/03/06 16:55:18 mbalmer Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Written by Philip A. Nelson for Piermont Information Systems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Piermont Information Systems Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PIERMONT INFORMATION SYSTEMS INC. ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PIERMONT INFORMATION SYSTEMS INC. BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* mdb.h - definitions for the menu database. */

#ifndef MDB_H
#define MDB_H

/* forward declaration */
typedef struct menu_info menu_info;

/* The declarations for the balanced binary trees. */

typedef struct id_rec {
	/* The balanced binary tree fields. */
	char  *id;      /* The name. */
	short balance;  /* For the balanced tree. */
	struct id_rec *left, *right; /* Tree pointers. */
  
	/* Other information fields. */
	menu_info *info;
	int menu_no;
} id_rec;


/* menu definitions records. */

typedef struct action {
	char *code;
	int   endwin;
} action;

typedef struct optn_info {
	char *name;
	int   name_is_code;
	int   menu;
	int   issub;
	int   doexit;
	action optact;
	struct optn_info *next;
} optn_info;
	
struct menu_info {
	char *title;
	char *helpstr;
	char *exitstr;
	int mopt;
	int y, x;
	int h, w;
	int numopt;
	int name_is_code;
	optn_info *optns;
	action postact;
	action exitact;
};

/* defines for mopt */
#define MC_NOEXITOPT 1
#define MC_NOBOX 2
#define MC_SCROLL 4
#define MC_NOSHORTCUT 8
#define MC_NOCLEAR 16
#define MC_DFLTEXIT 32
#define MC_ALWAYS_SCROLL 64
#define MC_SUBMENU 128
#define MC_VALID 0x10000
#endif
