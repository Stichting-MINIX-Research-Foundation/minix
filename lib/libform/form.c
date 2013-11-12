/*	$NetBSD: form.c,v 1.15 2004/11/24 11:57:09 blymn Exp $	*/

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

#include <sys/cdefs.h>
__RCSID("$NetBSD: form.c,v 1.15 2004/11/24 11:57:09 blymn Exp $");

#include <stdlib.h>
#include <strings.h>
#include <form.h>
#include "internals.h"

extern FIELD _formi_default_field;

FORM _formi_default_form = {
	FALSE, /* true if performing a init or term function */
	FALSE, /* the form is posted */
	FALSE, /* make field list circular if true */
	NULL, /* window for the form */
	NULL, /* subwindow for the form */
	NULL, /* use this window for output */
	NULL, /* user defined pointer */
	0, /* options for the form */
	NULL, /* function called when form posted and
				after page change */
	NULL, /* function called when form is unposted and
				before page change */
	NULL, /* function called when form posted and after
				 current field changes */
	NULL, /* function called when form unposted and
				 before current field changes */
	0, /* number of fields attached */
	0, /* current field */
	0, /* current page of form */
	0, /* number of pages in the form */
	NULL, /* dynamic array of fields that start
					   the pages */
	{NULL, NULL}, /* sorted field list */
	NULL /* array of fields attached to this form. */
};

/*
 * Set the window associated with the form
 */
int
set_form_win(FORM *form, WINDOW *win)
{
	if (form == NULL) {
		_formi_default_form.win = win;
		_formi_default_form.scrwin = win;
	} else {
		if (form->posted == TRUE)
			return E_POSTED;
		else {
			form->win = win;
			form->scrwin = win;
		}
	}

	return E_OK;
}

/*
 * Return the window used by the given form
 */
WINDOW *
form_win(FORM *form)
{
	if (form == NULL)
		return _formi_default_form.win;
	else
		return form->win;
}

/*
 * Set the subwindow for the form.
 */
int
set_form_sub(FORM *form, WINDOW *window)
{
	if (form == NULL) {
		_formi_default_form.subwin = window;
		_formi_default_form.scrwin = window;
	} else {
		if (form->posted == TRUE)
			return E_POSTED;
		else {
			form->subwin = window;
			form->scrwin = window;
		}
	}

	return E_OK;
}

/*
 * Return the subwindow for the given form.
 */
WINDOW *
form_sub(FORM *form)
{
	if (form == NULL)
		return _formi_default_form.subwin;
	else
		return form->subwin;
}

/*
 * Return the minimum size required to contain the form.
 */
int
scale_form(FORM *form, int *rows, int *cols)
{
	int i, max_row, max_col, temp;

	if ((form->fields == NULL) || (form->fields[0] == NULL))
		return E_NOT_CONNECTED;

	max_row = 0;
	max_col = 0;
	
	for (i = 0; i < form->field_count; i++) {
		temp = form->fields[i]->form_row + form->fields[i]->rows;
		max_row = (temp > max_row)? temp : max_row;
		temp = form->fields[i]->form_col + form->fields[i]->cols;
		max_col = (temp > max_col)? temp : max_col;
	}

	(*rows) = max_row;
	(*cols) = max_col;
	
	return E_OK;
}

/*
 * Set the user defined pointer for the form given.
 */
int
set_form_userptr(FORM *form, void *ptr)
{
	if (form == NULL)
		_formi_default_form.userptr = ptr;
	else
		form->userptr = ptr;

	return E_OK;
}

/*
 * Return the user defined pointer associated with the given form.
 */
void *
form_userptr(FORM *form)
{

	if (form == NULL)
		return _formi_default_form.userptr;
	else
		return form->userptr;
}

/*
 * Set the form options to the given ones.
 */
int
set_form_opts(FORM *form, Form_Options options)
{
	if (form == NULL)
		_formi_default_form.opts = options;
	else
		form->opts = options;

	return E_OK;
}

/*
 * Turn the given options on for the form.
 */
int
form_opts_on(FORM *form, Form_Options options)
{
	if (form == NULL)
		_formi_default_form.opts |= options;
	else
		form->opts |= options;

	return E_OK;
}

/*
 * Turn the given options off for the form.
 */
int
form_opts_off(FORM *form, Form_Options options)
{
	if (form == NULL)
		_formi_default_form.opts &= ~options;
	else
		form->opts &= ~options;


	return E_OK;
}

/*
 * Return the options set for the given form.
 */
Form_Options
form_opts(FORM *form)
{
	if (form == NULL)
		return _formi_default_form.opts;
	else
		return form->opts;
}

/*
 * Set the form init function for the given form
 */
int
set_form_init(FORM *form, Form_Hook func)
{
	if (form == NULL)
		_formi_default_form.form_init = func;
	else
		form->form_init = func;

	return E_OK;
}

/*
 * Return the init function associated with the given form.
 */
Form_Hook
form_init(FORM *form)
{
	if (form == NULL)
		return _formi_default_form.form_init;
	else
		return form->form_init;
}

/*
 * Set the function to be called on form termination.
 */
int
set_form_term(FORM *form, Form_Hook function)
{
	if (form == NULL)
		_formi_default_form.form_term = function;
	else
		form->form_term = function;

	return E_OK;
}

/*
 * Return the function defined for the termination function.
 */
Form_Hook
form_term(FORM *form)
{

	if (form == NULL)
		return _formi_default_form.form_term;
	else
		return form->form_term;
}

	
/*
 * Attach the given fields to the form.
 */
int
set_form_fields(FORM *form, FIELD **fields)
{
	int num_fields = 0, i, maxpg = 1, status;

	if (form == NULL)
		return E_BAD_ARGUMENT;

	if (form->posted == TRUE)
		return E_POSTED;

	if (fields == NULL)
		return E_BAD_ARGUMENT;

	while (fields[num_fields] != NULL) {
		if ((fields[num_fields]->parent != NULL) &&
		    (fields[num_fields]->parent != form))
			return E_CONNECTED;
		num_fields++;
	}

	  /* disconnect old fields, if any */
	if (form->fields != NULL) {
		for (i = 0; i < form->field_count; i++) {
			form->fields[i]->parent = NULL;
			form->fields[i]->index = -1;
		}
	}

	  /* kill old page pointers if any */
	if (form->page_starts != NULL)
		free(form->page_starts);

	form->field_count = num_fields;

	  /* now connect the new fields to the form */
	for (i = 0; i < num_fields; i++) {
		fields[i]->parent = form;
		fields[i]->index = i;
		  /* set the page number of the field */
		if (fields[i]->page_break == 1) 
			maxpg++;
		fields[i]->page = maxpg;
	}

	form->fields = fields;
	form->cur_field = 0;
	form->max_page = maxpg;
	if ((status = _formi_find_pages(form)) != E_OK)
		return status;

	  /* sort the fields and set the navigation pointers */
	_formi_sort_fields(form);
	_formi_stitch_fields(form);
	
	return E_OK;
}

/*
 * Return the fields attached to the form given.
 */
FIELD **
form_fields(FORM *form)
{
	if (form == NULL)
		return NULL;

	return form->fields;
}

/*
 * Return the number of fields attached to the given form.
 */
int
field_count(FORM *form)
{
	if (form == NULL)
		return -1;

	return form->field_count;
}

/*
 * Move the given field to the row and column given.
 */
int
move_field(FIELD *fptr, int frow, int fcol)
{
	FIELD *field = (fptr == NULL) ? &_formi_default_field : fptr;

	if (field->parent != NULL)
		return E_CONNECTED;

	field->form_row = frow;
	field->form_col = fcol;

	return E_OK;
}

/*
 * Set the page of the form to the given page.
 */
int
set_form_page(FORM *form, int page)
{
	if (form == NULL)
		return E_BAD_ARGUMENT;

	if (form->in_init == TRUE)
		return E_BAD_STATE;

	if (page > form->max_page)
		return E_BAD_ARGUMENT;

	form->page = page;
	return E_OK;
}

/*
 * Return the maximum page of the form.
 */
int
form_max_page(FORM *form)
{
	if (form == NULL)
		return _formi_default_form.max_page;
	else
		return form->max_page;
}

/*
 * Return the current page of the form.
 */
int
form_page(FORM *form)
{
	if (form == NULL)
		return E_BAD_ARGUMENT;

	return form->page;
}

/*
 * Set the current field to the field given.
 */
int
set_current_field(FORM *form, FIELD *field)
{
	if (form == NULL)
		return E_BAD_ARGUMENT;

	if (form->in_init == TRUE)
		return E_BAD_STATE;

	if (field == NULL)
		return E_INVALID_FIELD;

	if ((field->parent == NULL) || (field->parent != form))
		return E_INVALID_FIELD; /* field is not of this form */

	form->cur_field = field->index;

	  /* XXX update page if posted??? */
	return E_OK;
}

/*
 * Return the current field of the given form.
 */
FIELD *
current_field(FORM *form)
{
	if (form == NULL)
		return NULL;

	if (form->fields == NULL)
		return NULL;

	return form->fields[form->cur_field];
}

/*
 * Allocate a new form with the given fields.
 */
FORM *
new_form(FIELD **fields)
{
	FORM *new;

	if ((new = (FORM *) malloc(sizeof(FORM))) == NULL)
		return NULL;

	
	  /* copy in the defaults... */
	bcopy(&_formi_default_form, new, sizeof(FORM));

	if (new->win == NULL)
		new->scrwin = stdscr; /* something for curses to write to */

	if (fields != NULL) { /* attach the fields, if any */
		if (set_form_fields(new, fields) < 0) {
			free(new); /* field attach failed, back out */
			return NULL;
		}
	}

	return new;
}

/*
 * Free the given form.
 */
int
free_form(FORM *form)
{
	int i;
	
	if (form == NULL)
		return E_BAD_ARGUMENT;

	if (form->posted == TRUE)
		return E_POSTED;

	for (i = 0; i < form->field_count; i++) {
		  /* detach all the fields from the form */
		form->fields[i]->parent = NULL;
		form->fields[i]->index = -1;
	}

	free(form);

	return E_OK;
}

/*
 * Tell if the current field of the form has offscreen data ahead
 */
int
data_ahead(FORM *form)
{
	FIELD *cur;
	
	if ((form == NULL) || (form->fields == NULL)
	    || (form->fields[0] == NULL))
		return FALSE;

	cur = form->fields[form->cur_field];

	  /*XXXX wrong */
	if (cur->cur_line->expanded > cur->cols)
		return TRUE;

	return FALSE;
}

/*
 * Tell if current field of the form has offscreen data behind
 */
int
data_behind(FORM *form)
{
	FIELD *cur;
	
	if ((form == NULL) || (form->fields == NULL)
	    || (form->fields[0] == NULL))
		return FALSE;

	cur = form->fields[form->cur_field];

	if (cur->start_char > 0)
		return TRUE;

	return FALSE;
}

/*
 * Position the form cursor.
 */
int
pos_form_cursor(FORM *form)
{
	FIELD *cur;
	int row, col;
	
	if ((form == NULL) || (form->fields == NULL) ||
	    (form->fields[0] == NULL))
		return E_BAD_ARGUMENT;

	if (form->posted != 1)
		return E_NOT_POSTED;

	cur = form->fields[form->cur_field];
	row = cur->form_row;
	col = cur->form_col;

	  /* if the field is public then show the cursor pos */
	if ((cur->opts & O_PUBLIC) == O_PUBLIC) {
		row += cur->cursor_ypos;
		col += cur->cursor_xpos;
		if (cur->cursor_xpos >= cur->cols) {
			col = cur->form_col;
			row++;
		}
	}
	
#ifdef DEBUG
	fprintf(dbg, "pos_cursor: row=%d, col=%d\n", row, col);
#endif
	
	wmove(form->scrwin, row, col);

	return E_OK;
}
