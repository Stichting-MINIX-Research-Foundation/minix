/*	$NetBSD: internals.h,v 1.10 2004/11/24 11:57:09 blymn Exp $	*/

/*-
 * Copyright (c) 1998-1999 Brett Lymn
 *                         (blymn@baea.com.au, brett_lymn@yahoo.com.au)
 * All rights reserved.
 *
 * This code has been donated to The NetBSD Foundation by the Author.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#include <stdio.h>
#include "form.h"

#ifndef FORMI_INTERNALS_H
#define FORMI_INTERNALS_H 1

#ifdef DEBUG
extern FILE *dbg;
#endif

/* direction definitions for _formi_pos_new_field */
#define _FORMI_BACKWARD 1
#define _FORMI_FORWARD  2

/* define the default options for a form... */
#define DEFAULT_FORM_OPTS (O_VISIBLE | O_ACTIVE | O_PUBLIC | O_EDIT | \
                           O_WRAP | O_BLANK | O_AUTOSKIP | O_NULLOK | \
                           O_PASSOK | O_STATIC)

/* definitions of the flags for the FIELDTYPE structure */
#define _TYPE_NO_FLAGS   0
#define _TYPE_HAS_ARGS   0x01
#define _TYPE_IS_LINKED  0x02
#define _TYPE_IS_BUILTIN 0x04
#define _TYPE_HAS_CHOICE 0x08

typedef struct formi_type_link_struct formi_type_link;

struct formi_type_link_struct
{
	FIELDTYPE *next;
	FIELDTYPE *prev;
};


struct _formi_page_struct
{
	int in_use;
	int first;
	int last;
	int top_left;
	int bottom_right;
};

struct _formi_tab_stops
{
	struct _formi_tab_stops *fwd;
	struct _formi_tab_stops *back;
	unsigned char in_use;
	unsigned pos;
	unsigned size;
};

typedef struct _formi_tab_stops _formi_tab_t;

/* lines structure for the field - keeps start and ends and length of the
 * lines in a field.
 */
struct _formi_field_lines
{
	_FORMI_FIELD_LINES *prev;
	_FORMI_FIELD_LINES *next;
	unsigned allocated;
	unsigned length;
	unsigned expanded;
	char *string;
	unsigned char hard_ret; /* line contains hard return */
	_formi_tab_t *tabs;
};


/* function prototypes */
unsigned
_formi_skip_blanks(char *string, unsigned int start);
int
_formi_add_char(FIELD *cur, unsigned pos, char c);
void
_formi_calculate_tabs(_FORMI_FIELD_LINES *row);
int
_formi_draw_page(FORM *form);
int
_formi_find_pages(FORM *form);
int
_formi_field_choice(FORM *form, int c);
void
_formi_init_field_xpos(FIELD *field);
int
_formi_manipulate_field(FORM *form, int c);
int
_formi_pos_first_field(FORM *form);
int
_formi_pos_new_field(FORM *form, unsigned direction, unsigned use_sorted);
void
_formi_redraw_field(FORM *form, int field);
void
_formi_sort_fields(FORM *form);
void
_formi_stitch_fields(FORM *form);
int
_formi_tab_expanded_length(char *str, unsigned int start, unsigned int end);
int
_formi_update_field(FORM *form, int old_field);
int
_formi_validate_char(FIELD *field, char c);
int
_formi_validate_field(FORM *form);
int
_formi_wrap_field(FIELD *field, _FORMI_FIELD_LINES *pos);
int
_formi_sync_buffer(FIELD *field);

#ifdef DEBUG
int
_formi_create_dbg_file(void);
#endif /* DEBUG */
	
#endif



