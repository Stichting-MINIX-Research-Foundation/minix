/*	$NetBSD: field.c,v 1.29 2015/09/07 15:50:49 joerg Exp $	*/
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
__RCSID("$NetBSD: field.c,v 1.29 2015/09/07 15:50:49 joerg Exp $");

#include <sys/param.h>
#include <stdlib.h>
#include <strings.h>
#include <stdarg.h>
#include <form.h>
#include "internals.h"

extern FORM _formi_default_form;

FIELD _formi_default_field = {
	0, /* rows in the field */
	0, /* columns in the field */
	0, /* dynamic rows */
	0, /* dynamic columns */
	0, /* maximum growth */
	0, /* starting row in the form subwindow */
	0, /* starting column in the form subwindow */
	0, /* number of off screen rows */
	0, /* index of this field in form fields array. */
	0, /* number of buffers associated with this field */
	FALSE, /* set to true if buffer 0 has changed. */
	NO_JUSTIFICATION, /* justification style of the field */
	FALSE, /* set to true if field is in overlay mode */
	NULL, /* pointer to the current line cursor is on */
	0, /* starting char in string (horiz scroll) */
	NULL, /* starting line in field (vert scroll) */
	0, /* number of rows actually used in field */
	0, /* actual pos of cursor in row, not same as x pos due to tabs */
	0, /* x pos of cursor in field */
	0, /* y pos of cursor in field */
	0, /* start of a new page on the form if 1 */
	0, /* number of the page this field is on */
	A_NORMAL, /* character attributes for the foreground */
	A_NORMAL, /* character attributes for the background */
	' ', /* padding character */
	DEFAULT_FORM_OPTS, /* options for the field */
	NULL, /* the form this field is bound to, if any */
	NULL, /* field above this one */
	NULL, /* field below this one */
	NULL, /* field to the left of this one */
	NULL, /* field to the right of this one */
	NULL,  /* user defined pointer. */
	NULL, /* used if fields are linked */
	NULL, /* type struct for the field */
	{NULL, NULL}, /* circle queue glue for sorting fields */
	NULL, /* args for field type. */
	NULL, /* pointer to the array of lines structures. */
	NULL, /* list of lines available for reuse */
	NULL, /* array of buffers for the field */
};

/* internal function prototypes */
static int
field_buffer_init(FIELD *field, int buffer, unsigned int len);
static FIELD *
_formi_create_field(FIELD *, int, int, int, int, int, int);

   
/*
 * Set the userptr for the field
 */
int
set_field_userptr(FIELD *field, void *ptr)
{
	FIELD *fp = (field == NULL) ? &_formi_default_field : field;

	fp->userptr = ptr;

	return E_OK;
}

/*
 * Return the userptr for the field.
 */

void *
field_userptr(FIELD *field)
{
	if (field == NULL)
		return _formi_default_field.userptr;
	else
		return field->userptr;
}

/*
 * Set the options for the designated field.
 */
int
set_field_opts(FIELD *field, Form_Options options)
{
	int i;
	
	FIELD *fp = (field == NULL) ? &_formi_default_field : field;

	  /* not allowed to set opts if the field is the current one */
	if ((field != NULL) && (field->parent != NULL) &&
	    (field->parent->cur_field == field->index))
		return E_CURRENT;
	
	if ((options & O_STATIC) == O_STATIC) {
		for (i = 0; i < fp->nbuf; i++) {
			if (fp->buffers[i].length > fp->cols)
				fp->buffers[i].string[fp->cols] = '\0';
		}
	}
	
	fp->opts = options;

	  /* if appropriate, redraw the field */
	if ((field != NULL) && (field->parent != NULL)
	    && (field->parent->posted == 1)) {
		_formi_redraw_field(field->parent, field->index);
		pos_form_cursor(field->parent);
		wrefresh(field->parent->scrwin);
	}
	
	return E_OK;
}

/*
 * Turn on the passed field options.
 */
int
field_opts_on(FIELD *field, Form_Options options)
{
	int i;
	
	FIELD *fp = (field == NULL) ? &_formi_default_field : field;

	  /* not allowed to set opts if the field is the current one */
	if ((field != NULL) && (field->parent != NULL) &&
	    (field->parent->cur_field == field->index))
		return E_CURRENT;
	
	if ((options & O_STATIC) == O_STATIC) {
		for (i = 0; i < fp->nbuf; i++) {
			if (fp->buffers[i].length > fp->cols)
				fp->buffers[i].string[fp->cols] = '\0';
		}
	}
	
	fp->opts |= options;
	
	  /* if appropriate, redraw the field */
	if ((field != NULL) && (field->parent != NULL)
	    && (field->parent->posted == 1)) {
		_formi_redraw_field(field->parent, field->index);
		pos_form_cursor(field->parent);
		wrefresh(field->parent->scrwin);
	}

	return E_OK;
}

/*
 * Turn off the passed field options.
 */
int
field_opts_off(FIELD *field, Form_Options options)
{
	FIELD *fp = (field == NULL) ? &_formi_default_field : field;

	  /* not allowed to set opts if the field is the current one */
	if ((field != NULL) && (field->parent != NULL) &&
	    (field->parent->cur_field == field->index))
		return E_CURRENT;
	
	fp->opts &= ~options;
	
	  /* if appropriate, redraw the field */
	if ((field != NULL) && (field->parent != NULL)
	    && (field->parent->posted == 1)) {
		_formi_redraw_field(field->parent, field->index);
		pos_form_cursor(field->parent);
		wrefresh(field->parent->scrwin);
	}
	
	return E_OK;
}

/*
 * Return the field options associated with the passed field.
 */
Form_Options
field_opts(FIELD *field)
{
	if (field == NULL)
		return _formi_default_field.opts;
	else
		return field->opts;
}

/*
 * Set the justification for the passed field.
 */
int
set_field_just(FIELD *field, int justification)
{
	FIELD *fp = (field == NULL) ? &_formi_default_field : field;

	  /*
	   * not allowed to set justification if the field is
	   * the current one
	   */
	if ((field != NULL) && (field->parent != NULL) &&
	    (field->parent->cur_field == field->index))
		return E_CURRENT;
	
	if ((justification < MIN_JUST_STYLE) /* check justification valid */
	    || (justification > MAX_JUST_STYLE))
		return E_BAD_ARGUMENT;

	  /* only allow justification on static, single row fields */
	if (((fp->opts & O_STATIC) != O_STATIC) ||
	    ((fp->rows + fp->nrows) > 1))
		return E_BAD_ARGUMENT;
	
	fp->justification = justification;

	_formi_init_field_xpos(fp);
	
	return E_OK;
}

/*
 * Return the justification style of the field passed.
 */
int
field_just(FIELD *field)
{
	if (field == NULL)
		return _formi_default_field.justification;
	else
		return field->justification;
}

/*
 * Return information about the field passed.
 */
int
field_info(FIELD *field, int *rows, int *cols, int *frow, int *fcol,
	   int *nrow, int *nbuf)
{
	if (field == NULL)
		return E_BAD_ARGUMENT;

	*rows = field->rows;
	*cols = field->cols;
	*frow = field->form_row;
	*fcol = field->form_col;
	*nrow = field->nrows;
	*nbuf = field->nbuf;

	return E_OK;
}

/*
 * Report the dynamic field information.
 */
int
dynamic_field_info(FIELD *field, int *drows, int *dcols, int *max)
{
	if (field == NULL)
		return E_BAD_ARGUMENT;

	if ((field->opts & O_STATIC) == O_STATIC) {
		*drows = field->rows;
		*dcols = field->cols;
	} else {
		*drows = field->drows;
		*dcols = field->dcols;
	}
	
	*max = field->max;

	return E_OK;
}

/*
 * Init all the field variables, perform wrapping and other tasks
 * after the field buffer is set.
 */
static int
field_buffer_init(FIELD *field, int buffer, unsigned int len)
{
	int status;
	char *newp;
	
	if (buffer == 0) {
		field->start_char = 0;
		field->start_line = 0;
		field->row_xpos = 0;
		field->cursor_xpos = 0;
		field->cursor_ypos = 0;
		field->row_count = 1; /* must be at least one row  XXX need to shift old rows (if any) to free list??? */
		field->alines->length = len;
		if ((newp = realloc(field->alines->string,
				    (size_t) len + 1)) == NULL)
			return E_SYSTEM_ERROR;
		field->alines->string = newp;
		field->alines->allocated = len + 1;
		strlcpy(field->alines->string, field->buffers[buffer].string,
			(size_t) len + 1);
		field->alines->expanded =
			_formi_tab_expanded_length(field->alines->string,
						   0, field->alines->length);

		field->start_line = field->alines;
		field->cur_line = field->alines;
		
		  /* we have to hope the wrap works - if it does not then the
		     buffer is pretty much borked */
		status = _formi_wrap_field(field, field->cur_line);
		if (status != E_OK)
			return status;

		  /*
		   * calculate the tabs for a single row field, the
		   * multiline case is handled when the wrap is done.
		   */
		if (field->row_count == 1)
			_formi_calculate_tabs(field->alines);

		  /* redraw the field to reflect the new contents. If the field
		   * is attached....
		   */
		if ((field->parent != NULL) && (field->parent->posted == 1)) {
			_formi_redraw_field(field->parent, field->index);
			  /* make sure cursor goes back to current field */
			pos_form_cursor(field->parent);
		}
	}

	return E_OK;
}


/*
 * Set the field buffer to the string that results from processing
 * the given format (fmt) using sprintf.
 */
int
set_field_printf(FIELD *field, int buffer, char *fmt, ...)
{
	int len;
	va_list args;
	
	if (field == NULL)
		return E_BAD_ARGUMENT;

	if (buffer >= field->nbuf)
		return E_BAD_ARGUMENT;

	va_start(args, fmt);
	  /* check for buffer already existing, free the storage */
	if (field->buffers[buffer].allocated != 0)
		free(field->buffers[buffer].string);

	len = vasprintf(&field->buffers[buffer].string, fmt, args);
	va_end(args);
	if (len < 0)
		return E_SYSTEM_ERROR;
	
	field->buffers[buffer].length = len;
	field->buffers[buffer].allocated = len + 1;
	if (((field->opts & O_STATIC) == O_STATIC) && (len > field->cols)
	    && ((field->rows + field->nrows) == 1))
		len = field->cols;

	field->buffers[buffer].string[len] = '\0';
	return field_buffer_init(field, buffer, (unsigned int) len);
}
	
/*
 * Set the value of the field buffer to the value given.
 */

int
set_field_buffer(FIELD *field, int buffer, const char *value)
{
	unsigned int len;
	int status;
	
	if (field == NULL)
		return E_BAD_ARGUMENT;

	if (buffer >= field->nbuf) /* make sure buffer is valid */
		return E_BAD_ARGUMENT;

	len = (unsigned int) strlen(value);
	if (((field->opts & O_STATIC) == O_STATIC) && (len > field->cols)
	    && ((field->rows + field->nrows) == 1))
		len = field->cols;

#ifdef DEBUG
	if (_formi_create_dbg_file() != E_OK)
		return E_SYSTEM_ERROR;

	fprintf(dbg,
		"set_field_buffer: entry: len = %d, value = %s, buffer=%d\n",
		len, value, buffer);
	fprintf(dbg, "set_field_buffer: entry: string = ");
	if (field->buffers[buffer].string != NULL)
		fprintf(dbg, "%s, len = %d\n", field->buffers[buffer].string,
			field->buffers[buffer].length);
	else
		fprintf(dbg, "(null), len = 0\n");
	fprintf(dbg, "set_field_buffer: entry: lines.len = %d\n",
		field->alines[0].length);
#endif
	
	if ((field->buffers[buffer].string =
	     (char *) realloc(field->buffers[buffer].string,
			      (size_t) len + 1)) == NULL)
		return E_SYSTEM_ERROR;

	strlcpy(field->buffers[buffer].string, value, (size_t) len + 1);
	field->buffers[buffer].length = len;
	field->buffers[buffer].allocated = len + 1;
	status = field_buffer_init(field, buffer, len);

#ifdef DEBUG
	fprintf(dbg, "set_field_buffer: exit: len = %d, value = %s\n",
		len, value);
	fprintf(dbg, "set_field_buffer: exit: string = %s, len = %d\n",
		field->buffers[buffer].string, field->buffers[buffer].length);
	fprintf(dbg, "set_field_buffer: exit: lines.len = %d\n",
		field->alines[0].length);
#endif

	return status;
}

/*
 * Return the requested field buffer to the caller.
 */
char *
field_buffer(FIELD *field, int buffer)
{

	char *reformat, *p;
	_FORMI_FIELD_LINES *linep;
	size_t bufsize, pos;
	
	if (field == NULL)
		return NULL;

	if (buffer >= field->nbuf)
		return NULL;

	  /*
	   * We force a sync from the line structs to the buffer here.
	   * Traditional libform say we don't need to because it is
	   * done on a REQ_VALIDATE but NetBSD libform previously did
	   * not enforce this because the buffer contents were always
	   * current.  Changes to line handling make this no longer so
	   * - the line structs may contain different data to the
	   * buffer if unsynced.
	   */
	if (_formi_sync_buffer(field) != E_OK)
		return NULL;
	
	if ((field->opts & O_REFORMAT) != O_REFORMAT)
		return field->buffers[buffer].string;

	if (field->row_count <= 1)
		return strdup(field->buffers[buffer].string);

	/*
	 * create a single string containing each line,
	 * separated by newline, last line having no
	 * newline, but NUL terminated.
	 */
	bufsize = pos = 0;
	reformat = NULL;
	for (linep = field->alines; linep; linep = linep->next) {
		size_t len = strlen(linep->string);
		if (len + 1 >= bufsize - pos) {
			bufsize += MAX(1024, 2 * len);
			p = realloc(reformat, bufsize);
			if (p == NULL) {
				free(reformat);
				return NULL;
			}
			reformat = p;
		}
		memcpy(reformat + pos, linep->string, len);
		pos += len;
		reformat[pos++] = linep->next ? '\n' : '\0';
	}
	return reformat;
}

/*
 * Set the buffer 0 field status.
 */
int
set_field_status(FIELD *field, int status)
{

	if (field == NULL)
		return E_BAD_ARGUMENT;

	if (status != FALSE)
		field->buf0_status = TRUE;
	else
		field->buf0_status = FALSE;

	return E_OK;
}

/*
 * Return the buffer 0 status flag for the given field.
 */
int
field_status(FIELD *field)
{

	if (field == NULL) /* the default buffer 0 never changes :-) */
		return FALSE;

	return field->buf0_status;
}

/*
 * Set the maximum growth for a dynamic field.
 */
int
set_max_field(FIELD *fptr, int max)
{
	FIELD *field = (fptr == NULL)? &_formi_default_field : fptr;

	if ((field->opts & O_STATIC) == O_STATIC) /* check if field dynamic */
		return E_BAD_ARGUMENT;

	if (max < 0) /* negative numbers are bad.... */
		return E_BAD_ARGUMENT;
	
	field->max = max;
	return E_OK;
}

/*
 * Set the field foreground character attributes.
 */
int
set_field_fore(FIELD *fptr, chtype attribute)
{
	FIELD *field = (fptr == NULL)? &_formi_default_field : fptr;

	field->fore = attribute;
	return E_OK;
}

/*
 * Return the foreground character attribute for the given field.
 */
chtype
field_fore(FIELD *field)
{
	if (field == NULL)
		return _formi_default_field.fore;
	else
		return field->fore;
}

/*
 * Set the background character attribute for the given field.
 */
int
set_field_back(FIELD *field, chtype attribute)
{
	if (field == NULL)
		_formi_default_field.back = attribute;
	else
		field->back = attribute;

	return E_OK;
}

/*
 * Get the background character attribute for the given field.
 */
chtype
field_back(FIELD *field)
{
	if (field == NULL)
		return _formi_default_field.back;
	else
		return field->back;
}

/*
 * Set the pad character for the given field.
 */
int
set_field_pad(FIELD *field, int pad)
{
	if (field == NULL)
		_formi_default_field.pad = pad;
	else
		field->pad = pad;

	return E_OK;
}

/*
 * Return the padding character for the given field.
 */
int
field_pad(FIELD *field)
{
	if (field == NULL)
		return _formi_default_field.pad;
	else
		return field->pad;
}

/*
 * Set the field initialisation function hook.
 */
int
set_field_init(FORM *form, Form_Hook function)
{
	if (form == NULL)
		_formi_default_form.field_init = function;
	else
		form->field_init = function;

	return E_OK;
}

/*
 * Return the function hook for the field initialisation.
 */
Form_Hook
field_init(FORM *form)
{
	if (form == NULL)
		return _formi_default_form.field_init;
	else
		return form->field_init;
}

/*
 * Set the field termination function hook.
 */
int
set_field_term(FORM *form, Form_Hook function)
{
	if (form == NULL)
		_formi_default_form.field_term = function;
	else
		form->field_term = function;

	return E_OK;
}

/*
 * Return the function hook defined for the field termination.
 */
Form_Hook
field_term(FORM *form)
{
	if (form == NULL)
		return _formi_default_form.field_term;
	else
		return form->field_term;
}

/*
 * Set the page flag on the given field to indicate it is the start of a
 * new page.
 */
int
set_new_page(FIELD *fptr, int page)
{
	FIELD *field = (fptr == NULL)? &_formi_default_field : fptr;

	if (field->parent != NULL) /* check if field is connected to a form */
		return E_CONNECTED;
	
	field->page_break = (page != FALSE);
	return E_OK;
}

/*
 * Return the page status for the given field.  TRUE is returned if the
 * field is the start of a new page.
 */
int
new_page(FIELD *field)
{
	if (field == NULL)
		return _formi_default_field.page_break;
	else
		return field->page_break;
}

/*
 * Return the index of the field in the form fields array.
 */
int
field_index(FIELD *field)
{
	if (field == NULL)
		return E_BAD_ARGUMENT;

	if (field->parent == NULL)
		return E_NOT_CONNECTED;

	return field->index;
}

/*
 * Internal function that does most of the work to create a new field.
 * The new field is initialised from the information in the prototype
 * field passed.
 * Returns NULL on error.
 */
static FIELD *
_formi_create_field(FIELD *prototype, int rows, int cols, int frow,
		    int fcol, int nrows, int nbuf)
{
	FIELD *new;
	
	if ((rows <= 0) || (cols <= 0) || (frow < 0) || (fcol < 0) ||
	    (nrows < 0) || (nbuf < 0))
		return NULL;
	
	if ((new = (FIELD *)malloc(sizeof(FIELD))) == NULL) {
		return NULL;
	}

	  /* copy in the default field info */
	bcopy(prototype, new, sizeof(FIELD));

	new->nbuf = nbuf + 1;
	new->rows = rows;
	new->cols = cols;
	new->form_row = frow;
	new->form_col = fcol;
	new->nrows = nrows;
	new->link = new;
	return new;
}

/*
 * Create a new field structure.
 */
FIELD *
new_field(int rows, int cols, int frow, int fcol, int nrows, int nbuf)
{
	FIELD *new;
	size_t buf_len;
	int i;
	

	if ((new = _formi_create_field(&_formi_default_field, rows, cols,
				       frow, fcol, nrows, nbuf)) == NULL)
		return NULL;
	
	buf_len = (nbuf + 1) * sizeof(FORM_STR);
	
	if ((new->buffers = (FORM_STR *)malloc(buf_len)) == NULL) {
		free(new);
		return NULL;
	}

	  /* Initialise the strings to a zero length string */
	for (i = 0; i < nbuf + 1; i++) {
		if ((new->buffers[i].string =
		     (char *) malloc(sizeof(char))) == NULL) {
			free(new->buffers);
			free(new);
			return NULL;
		}
		new->buffers[i].string[0] = '\0';
		new->buffers[i].length = 0;
		new->buffers[i].allocated = 1;
	}

	if ((new->alines = (_FORMI_FIELD_LINES *)
	     malloc(sizeof(struct _formi_field_lines))) == NULL) {
		free(new->buffers);
		free(new);
		return NULL;
	}

	new->alines->prev = NULL;
	new->alines->next = NULL;
	new->alines->allocated = 0;
	new->alines->length = 0;
	new->alines->expanded = 0;
	new->alines->string = NULL;
	new->alines->hard_ret = FALSE;
	new->alines->tabs = NULL;
	new->start_line = new->alines;
	new->cur_line = new->alines;
	
	return new;
}

/*
 * Duplicate the given field, including its buffers.
 */
FIELD *
dup_field(FIELD *field, int frow, int fcol)
{
	FIELD *new;
	size_t row_len, buf_len;
	
	if (field == NULL)
		return NULL;

	  /* XXXX this right???? */
	if ((new = _formi_create_field(field, (int) field->rows,
				       (int ) field->cols,
				       frow, fcol, (int) field->nrows,
				       field->nbuf - 1)) == NULL)
		return NULL;

	row_len = (field->rows + field->nrows + 1) * field->cols;
	buf_len = (field->nbuf + 1) * row_len * sizeof(FORM_STR);
	
	if ((new->buffers = (FORM_STR *)malloc(buf_len)) == NULL) {
		free(new);
		return NULL;
	}

	  /* copy the buffers from the source field into the new copy */
	bcopy(field->buffers, new->buffers, buf_len);

	return new;
}

/*
 * Create a new field at the specified location by duplicating the given
 * field.  The buffers are shared with the parent field.
 */
FIELD *
link_field(FIELD *field, int frow, int fcol)
{
	FIELD *new;

	if (field == NULL)
		return NULL;
	
	if ((new = _formi_create_field(field, (int) field->rows,
				       (int) field->cols,
				       frow, fcol, (int) field->nrows,
				       field->nbuf - 1)) == NULL)
		return NULL;

	new->link = field->link;
	field->link = new;
	
	  /* we are done.  The buffer pointer was copied during the field
	     creation. */
	return new;
}

/*
 * Release all storage allocated to the field
 */
int
free_field(FIELD *field)
{
	FIELD *flink;
	int i;
	_formi_tab_t *ts, *nts;
	
	if (field == NULL)
		return E_BAD_ARGUMENT;

	if (field->parent != NULL)
		return E_CONNECTED;

	if (field->link == field) { /* check if field linked */
		  /* no it is not - release the buffers */
		free(field->buffers);
		  /* free the tab structures */
		for (i = 0; i < field->row_count - 1; i++) {
			if (field->alines[i].tabs != NULL) {
				ts = field->alines[i].tabs;
				while (ts != NULL) {
					nts = ts->fwd;
					free(ts);
					ts = nts;
				}
			}
		}
	} else {
		  /* is linked, traverse the links to find the field referring
		   * to the one to be freed.
		   */
		for (flink = field->link; flink != field; flink = flink->link);
		flink->link = field->link;
	}

	free(field);
	return E_OK;
}

	
