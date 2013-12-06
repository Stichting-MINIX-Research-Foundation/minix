/*	$NetBSD: internals.c,v 1.37 2013/11/26 01:17:00 christos Exp $	*/

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
__RCSID("$NetBSD: internals.c,v 1.37 2013/11/26 01:17:00 christos Exp $");

#include <limits.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>
#include "internals.h"
#include "form.h"

#ifdef DEBUG
/*
 *  file handle to write debug info to, this will be initialised when
 *  the form is first posted.
 */
FILE *dbg = NULL;

/*
 * map the request numbers to strings for debug
 */
char *reqs[] = {
	"NEXT_PAGE", "PREV_PAGE", "FIRST_PAGE",	"LAST_PAGE", "NEXT_FIELD",
	"PREV_FIELD", "FIRST_FIELD", "LAST_FIELD", "SNEXT_FIELD",
	"SPREV_FIELD", "SFIRST_FIELD", "SLAST_FIELD", "LEFT_FIELD",
	"RIGHT_FIELD", "UP_FIELD", "DOWN_FIELD", "NEXT_CHAR", "PREV_CHAR",
	"NEXT_LINE", "PREV_LINE", "NEXT_WORD", "PREV_WORD", "BEG_FIELD",
	"END_FIELD", "BEG_LINE", "END_LINE", "LEFT_CHAR", "RIGHT_CHAR",
	"UP_CHAR", "DOWN_CHAR", "NEW_LINE", "INS_CHAR", "INS_LINE",
	"DEL_CHAR", "DEL_PREV", "DEL_LINE", "DEL_WORD", "CLR_EOL",
	"CLR_EOF", "CLR_FIELD", "OVL_MODE", "INS_MODE", "SCR_FLINE",
	"SCR_BLINE", "SCR_FPAGE", "SCR_BPAGE", "SCR_FHPAGE", "SCR_BHPAGE",
	"SCR_FCHAR", "SCR_BCHAR", "SCR_HFLINE", "SCR_HBLINE", "SCR_HFHALF",
	"SCR_HBHALF", "VALIDATION", "PREV_CHOICE", "NEXT_CHOICE" };
#endif

/* define our own min function - this is not generic but will do here
 * (don't believe me?  think about what value you would get
 * from min(x++, y++)
 */
#define min(a,b) (((a) > (b))? (b) : (a))

/* for the line joining function... */
#define JOIN_NEXT    1
#define JOIN_NEXT_NW 2 /* next join, don't wrap the joined line */
#define JOIN_PREV    3
#define JOIN_PREV_NW 4 /* previous join, don't wrap the joined line */

/* for the bump_lines function... */
#define _FORMI_USE_CURRENT -1 /* indicates current cursor pos to be used */

/* used in add_char for initial memory allocation for string in row */
#define INITIAL_LINE_ALLOC 16

unsigned
field_skip_blanks(unsigned int start, _FORMI_FIELD_LINES **rowp);
static void
_formi_do_char_validation(FIELD *field, FIELDTYPE *type, char c, int *ret_val);
static void
_formi_do_validation(FIELD *field, FIELDTYPE *type, int *ret_val);
static int
_formi_join_line(FIELD *field, _FORMI_FIELD_LINES **rowp, int direction);
void
_formi_hscroll_back(FIELD *field, _FORMI_FIELD_LINES *row, unsigned int amt);
void
_formi_hscroll_fwd(FIELD *field, _FORMI_FIELD_LINES *row, unsigned int amt);
static void
_formi_scroll_back(FIELD *field, unsigned int amt);
static void
_formi_scroll_fwd(FIELD *field, unsigned int amt);
static int
_formi_set_cursor_xpos(FIELD *field, int no_scroll);
static int
find_sow(unsigned int offset, _FORMI_FIELD_LINES **rowp);
static int
split_line(FIELD *field, bool hard_split, unsigned pos,
	   _FORMI_FIELD_LINES **rowp);
static bool
check_field_size(FIELD *field);
static int
add_tab(FORM *form, _FORMI_FIELD_LINES *row, unsigned int i, char c);
static int
tab_size(_FORMI_FIELD_LINES *row, unsigned int i);
static unsigned int
tab_fit_len(_FORMI_FIELD_LINES *row, unsigned int len);
static int
tab_fit_window(FIELD *field, unsigned int pos, unsigned int window);
static void
add_to_free(FIELD *field, _FORMI_FIELD_LINES *line);
static void
adjust_ypos(FIELD *field, _FORMI_FIELD_LINES *line);
static _FORMI_FIELD_LINES *
copy_row(_FORMI_FIELD_LINES *row);
static void
destroy_row_list(_FORMI_FIELD_LINES *start);

/*
 * Calculate the cursor y position to make the given row appear on the
 * field.  This may be as simple as just changing the ypos (if at all) but
 * may encompass resetting the start_line of the field to place the line
 * at the bottom of the field.  The field is assumed to be a multi-line one.
 */
static void
adjust_ypos(FIELD *field, _FORMI_FIELD_LINES *line)
{
	unsigned ypos;
	_FORMI_FIELD_LINES *rs;
	
	ypos = 0;
	rs = field->alines;
	while (rs != line) {
		rs = rs->next;
		ypos++;
	}

	field->cursor_ypos = ypos;
	field->start_line = field->alines;
	if (ypos > (field->rows - 1)) {
		  /*
		   * cur_line off the end of the field,
		   * adjust start_line so fix this.
		   */
		field->cursor_ypos = field->rows - 1;
		ypos = ypos - (field->rows - 1);
		while (ypos > 0) {
			ypos--;
			field->start_line = field->start_line->next;
		}
	}
}

			
/*
 * Delete the given row and add it to the free list of the given field.
 */
static void
add_to_free(FIELD *field, _FORMI_FIELD_LINES *line)
{
	_FORMI_FIELD_LINES *saved;

	saved = line;

	  /* don't remove if only one line... */
	if ((line->prev == NULL) && (line->next == NULL))
		return;

	if (line->prev == NULL) {
		/* handle top of list */
		field->alines = line->next;
		field->alines->prev = NULL;

		if (field->cur_line == saved)
			field->cur_line = field->alines;
		if (field->start_line == saved)
			field->start_line = saved;
	} else if (line->next == NULL) {
		/* handle bottom of list */
		line->prev->next = NULL;
		if (field->cur_line == saved)
			field->cur_line = saved->prev;
		if (field->start_line == saved)
			field->cur_line = saved->prev;
	} else {
		saved->next->prev = saved->prev;
		saved->prev->next = saved->next;
		if (field->cur_line == saved)
			field->cur_line = saved->prev;
		if (field->start_line == saved)
			field->start_line = saved;
	}

	saved->next = field->free;
	field->free = saved;
	saved->prev = NULL;
	if (saved->next != NULL)
		saved->next->prev = line;
}

/*
 * Duplicate the given row, return the pointer to the new copy or
 * NULL if the copy fails.
 */
static _FORMI_FIELD_LINES *
copy_row(_FORMI_FIELD_LINES *row)
{
	_FORMI_FIELD_LINES *new;
	_formi_tab_t *tp, *newt;

	if ((new = (_FORMI_FIELD_LINES *) malloc(sizeof(_FORMI_FIELD_LINES)))
	    == NULL) {
		return NULL;
	}

	memcpy(new, row, sizeof(_FORMI_FIELD_LINES));
	
	  /* nuke the pointers from the source row so we don't get confused */
	new->next = NULL;
	new->prev = NULL;
	new->tabs = NULL;

	if ((new->string = (char *) malloc((size_t)new->allocated)) == NULL) {
		free(new);
		return NULL;
	}

	memcpy(new->string, row->string, (size_t) row->length + 1);

	if (row->tabs != NULL) {
		tp = row->tabs;
		if ((new->tabs = (_formi_tab_t *) malloc(sizeof(_formi_tab_t)))
		    == NULL) {
			free(new->string);
			free(new);
			return NULL;
		}

		memcpy(new->tabs, row->tabs, sizeof(_formi_tab_t));
		new->tabs->back = NULL;
		new->tabs->fwd = NULL;
		
		tp = tp->fwd;
		newt = new->tabs;

		while (tp != NULL) {
			if ((newt->fwd =
			     (_formi_tab_t *) malloc(sizeof(_formi_tab_t)))
			    == NULL) {
				/* error... unwind allocations */
				tp = new->tabs;
				while (tp != NULL) {
					newt = tp->fwd;
					free(tp);
					tp = newt;
				}

				free(new->string);
				free(new);
				return NULL;
			}

			memcpy(newt->fwd, tp, sizeof(_formi_tab_t));
			newt->fwd->back = newt;
			newt = newt->fwd;
			newt->fwd = NULL;
			tp = tp->fwd;
		}
	}
		
	return new;
}
	
/*
 * Initialise the row offset for a field, depending on the type of
 * field it is and the type of justification used.  The justification
 * is only used on static single line fields, everything else will
 * have the cursor_xpos set to 0.
 */
void
_formi_init_field_xpos(FIELD *field)
{
	  /* not static or is multi-line which are not justified, so 0 it is */
	if (((field->opts & O_STATIC) != O_STATIC) ||
	    ((field->rows + field->nrows) != 1)) {
		field->cursor_xpos = 0;
		return;
	}

	switch (field->justification) {
	case JUSTIFY_RIGHT:
		field->cursor_xpos = field->cols - 1;
		break;

	case JUSTIFY_CENTER:
		field->cursor_xpos = (field->cols - 1) / 2;
		break;

	default: /* assume left justify */
		field->cursor_xpos = 0;
		break;
	}
}


/*
 * Open the debug file if it is not already open....
 */
#ifdef DEBUG
int
_formi_create_dbg_file(void)
{
	if (dbg == NULL) {
		dbg = fopen("___form_dbg.out", "w");
		if (dbg == NULL) {
			fprintf(stderr, "Cannot open debug file!\n");
			return E_SYSTEM_ERROR;
		}
	}

	return E_OK;
}
#endif

/*
 * Check the sizing of the field, if the maximum size is set for a
 * dynamic field then check that the number of rows or columns does
 * not exceed the set maximum.  The decision to check the rows or
 * columns is made on the basis of how many rows are in the field -
 * one row means the max applies to the number of columns otherwise it
 * applies to the number of rows.  If the row/column count is less
 * than the maximum then return TRUE.
 *
 */
static bool
check_field_size(FIELD *field)
{
	if ((field->opts & O_STATIC) != O_STATIC) {
		  /* dynamic field */
		if (field->max == 0) /* unlimited */
			return TRUE;

		if (field->rows == 1) {
			return (field->alines->length < field->max);
		} else {
			return (field->row_count <= field->max);
		}
	} else {
		if ((field->rows + field->nrows) == 1) {
			return (field->alines->length <= field->cols);
		} else {
			return (field->row_count <= (field->rows
						     + field->nrows));
		}
	}
}

/*
 * Set the form's current field to the first valid field on the page.
 * Assume the fields have been sorted and stitched.
 */
int
_formi_pos_first_field(FORM *form)
{
	FIELD *cur;
	int old_page;

	old_page = form->page;

	  /* scan forward for an active page....*/
	while (form->page_starts[form->page].in_use == 0) {
		form->page++;
		if (form->page > form->max_page) {
			form->page = old_page;
			return E_REQUEST_DENIED;
		}
	}

	  /* then scan for a field we can use */
	cur = form->fields[form->page_starts[form->page].first];
	while ((cur->opts & (O_VISIBLE | O_ACTIVE))
	       != (O_VISIBLE | O_ACTIVE)) {
		cur = TAILQ_NEXT(cur, glue);
		if (cur == NULL) {
			form->page = old_page;
			return E_REQUEST_DENIED;
		}
	}

	form->cur_field = cur->index;
	return E_OK;
}

/*
 * Set the field to the next active and visible field, the fields are
 * traversed in index order in the direction given.  If the parameter
 * use_sorted is TRUE then the sorted field list will be traversed instead
 * of using the field index.
 */
int
_formi_pos_new_field(FORM *form, unsigned direction, unsigned use_sorted)
{
	FIELD *cur;
	int i;

	i = form->cur_field;
	cur = form->fields[i];

	do {
		if (direction == _FORMI_FORWARD) {
			if (use_sorted == TRUE) {
				if ((form->wrap == FALSE) &&
				    (cur == TAILQ_LAST(&form->sorted_fields,
					_formi_sort_head)))
					return E_REQUEST_DENIED;
				cur = TAILQ_NEXT(cur, glue);
				i = cur->index;
			} else {
				if ((form->wrap == FALSE) &&
				    ((i + 1) >= form->field_count))
					return E_REQUEST_DENIED;
				i++;
				if (i >= form->field_count)
					i = 0;
			}
		} else {
			if (use_sorted == TRUE) {
				if ((form->wrap == FALSE) &&
				    (cur == TAILQ_FIRST(&form->sorted_fields)))
					return E_REQUEST_DENIED;
				cur = TAILQ_PREV(cur, _formi_sort_head, glue);
				i = cur->index;
			} else {
				if ((form->wrap == FALSE) && (i <= 0))
					return E_REQUEST_DENIED;
				i--;
				if (i < 0)
					i = form->field_count - 1;
			}
		}

		if ((form->fields[i]->opts & (O_VISIBLE | O_ACTIVE))
			== (O_VISIBLE | O_ACTIVE)) {
			form->cur_field = i;
			return E_OK;
		}
	}
	while (i != form->cur_field);

	return E_REQUEST_DENIED;
}

/*
 * Destroy the list of line structs passed by freeing all allocated
 * memory.
 */
static void
destroy_row_list(_FORMI_FIELD_LINES *start)
{
	_FORMI_FIELD_LINES *temp, *row;
	_formi_tab_t *tt, *tp;

	row = start;
	while (row != NULL) {
		if (row->tabs != NULL) {
			  /* free up the tab linked list... */
			tp = row->tabs;
			while (tp != NULL) {
				tt = tp->fwd;
				free(tp);
				tp = tt;
			}
		}

		if (row->string != NULL)
			free(row->string);

		temp = row->next;
		free(row);
		row = temp;
	}
}

/*
 * Word wrap the contents of the field's buffer 0 if this is allowed.
 * If the wrap is successful, that is, the row count nor the buffer
 * size is exceeded then the function will return E_OK, otherwise it
 * will return E_REQUEST_DENIED.
 */
int
_formi_wrap_field(FIELD *field, _FORMI_FIELD_LINES *loc)
{
	int width, wrap_err;
	unsigned int pos, saved_xpos, saved_ypos, saved_cur_xpos;
	unsigned int saved_row_count;
	_FORMI_FIELD_LINES *saved_row, *row, *row_backup, *saved_cur_line;
	_FORMI_FIELD_LINES *saved_start_line, *temp;

	if ((field->opts & O_STATIC) == O_STATIC) {
		if ((field->rows + field->nrows) == 1) {
			return E_OK; /* cannot wrap a single line */
		}
		width = field->cols;
	} else {
		  /* if we are limited to one line then don't try to wrap */
		if ((field->drows + field->nrows) == 1) {
			return E_OK;
		}

		  /*
		   * hueristic - if a dynamic field has more than one line
		   * on the screen then the field grows rows, otherwise
		   * it grows columns, effectively a single line field.
		   * This is documented AT&T behaviour.
		   */
		if (field->rows > 1) {
			width = field->cols;
		} else {
			return E_OK;
		}
	}

	row = loc;

	  /* if we are not at the top of the field then back up one
	   * row because we may be able to merge the current row into
	   * the one above.
	   */
	if (row->prev != NULL)
		row = row->prev;

	saved_row = row;
	saved_xpos = field->row_xpos;
	saved_cur_xpos = field->cursor_xpos;
	saved_ypos = field->cursor_ypos;
	saved_row_count = field->row_count;
	
	  /*
	   * Save a copy of the lines affected, just in case things
	   * don't work out.
	   */
	if ((row_backup = copy_row(row)) == NULL)
		return E_SYSTEM_ERROR;

	temp = row_backup;
	row = row->next;

	saved_cur_line = temp;
	saved_start_line = temp;
	
	while (row != NULL) {
		if ((temp->next = copy_row(row)) == NULL) {
			  /* a row copy failed... free up allocations */
			destroy_row_list(row_backup);
			return E_SYSTEM_ERROR;
		}

		temp->next->prev = temp;
		temp = temp->next;
		
		if (row == field->start_line)
			saved_start_line = temp;
		if (row == field->cur_line)
			saved_cur_line = temp;

		row = row->next;
	}

	row = saved_row;
	while (row != NULL) {
		pos = row->length - 1;
		if (row->expanded < width) {
			  /* line may be too short, try joining some lines */
			if ((row->hard_ret == TRUE) && (row->next != NULL)) {
				/*
				 * Skip the line if it has a hard return
				 * and it is not the last, we cannot join
				 * anything to it.
				 */
				row = row->next;
				continue;
			}
			
			if (row->next == NULL) {
				/*
				 * If there are no more lines and this line
				 * is too short then our job is over.
				 */
				break;
			}

			if (_formi_join_line(field, &row,
					     JOIN_NEXT_NW) == E_OK) {
				continue;
			} else
				break;
		} else if (row->expanded > width) {
			  /* line is too long, split it */

			  /*
			   * split on first whitespace before current word
			   * if the line has tabs we need to work out where
			   * the field border lies when the tabs are expanded.
			   */
			if (row->tabs == NULL) {
				pos = width - 1;
				if (pos >= row->expanded)
					pos = row->expanded - 1;
			} else {
				pos = tab_fit_len(row, field->cols);
			}

			if ((!isblank((unsigned char)row->string[pos])) &&
			    ((field->opts & O_WRAP) == O_WRAP)) {
				if (!isblank((unsigned char)row->string[pos - 1]))
					pos = find_sow((unsigned int) pos,
						       &row);
				/*
				 * If we cannot split the line then return
				 * NO_ROOM so the driver can tell that it
				 * should not autoskip (if that is enabled)
				 */
				if ((pos == 0)
				    || (!isblank((unsigned char)row->string[pos - 1]))) {
					wrap_err = E_NO_ROOM;
					goto restore_and_exit;
				}
			}

			  /* if we are at the end of the string and it has
			   * a trailing blank, don't wrap the blank.
			   */
			if ((row->next == NULL) && (pos == row->length - 1) &&
			    (isblank((unsigned char)row->string[pos])) &&
			    row->expanded <= field->cols)
				continue;

			  /*
			   * otherwise, if we are still sitting on a
			   * blank but not at the end of the line
			   * move forward one char so the blank
			   * is on the line boundary.
			   */
			if ((isblank((unsigned char)row->string[pos])) &&
			    (pos != row->length - 1))
				pos++;

			if (split_line(field, FALSE, pos, &row) != E_OK) {
				wrap_err = E_REQUEST_DENIED;
				goto restore_and_exit;
			}
		} else
			  /* line is exactly the right length, do next one */
			row = row->next;
	}

	  /* Check if we have not run out of room */
	if ((((field->opts & O_STATIC) == O_STATIC) &&
	     field->row_count > (field->rows + field->nrows)) ||
	    ((field->max != 0) && (field->row_count > field->max))) {

		wrap_err = E_REQUEST_DENIED;

	  restore_and_exit:
		if (saved_row->prev == NULL) {
			field->alines = row_backup;
		} else {
			saved_row->prev->next = row_backup;
			row_backup->prev = saved_row->prev;
		}

		field->row_xpos = saved_xpos;
		field->cursor_xpos = saved_cur_xpos;
		field->cursor_ypos = saved_ypos;
		field->row_count = saved_row_count;
		field->start_line = saved_start_line;
		field->cur_line = saved_cur_line;

		destroy_row_list(saved_row);
		return wrap_err;
	}

	destroy_row_list(row_backup);
	return E_OK;
}

/*
 * Join the two lines that surround the location pos, the type
 * variable indicates the direction of the join, JOIN_NEXT will join
 * the next line to the current line, JOIN_PREV will join the current
 * line to the previous line, the new lines will be wrapped unless the
 * _NW versions of the directions are used.  Returns E_OK if the join
 * was successful or E_REQUEST_DENIED if the join cannot happen.
 */
static int
_formi_join_line(FIELD *field, _FORMI_FIELD_LINES **rowp, int direction)
{
	int old_len, count;
	struct _formi_field_lines *saved;
	char *newp;
	_FORMI_FIELD_LINES *row = *rowp;
#ifdef DEBUG
	int dbg_ok = FALSE;

	if (_formi_create_dbg_file() == E_OK) {
		dbg_ok = TRUE;
	}

	if (dbg_ok == TRUE) {
		fprintf(dbg, "join_line: working on row %p, row_count = %d\n",
			row, field->row_count);
	}
#endif

	if ((direction == JOIN_NEXT) || (direction == JOIN_NEXT_NW)) {
		  /*
		   * See if there is another line following, or if the
		   * line contains a hard return then we don't join
		   * any lines to it.
		   */
		if ((row->next == NULL) || (row->hard_ret == TRUE)) {
			return E_REQUEST_DENIED;
		}

#ifdef DEBUG
		if (dbg_ok == TRUE) {
			fprintf(dbg,
			"join_line: join_next before length = %d, expanded = %d",
				row->length, row->expanded);
			fprintf(dbg,
				" :: next row length = %d, expanded = %d\n",
				row->length, row->expanded);
		}
#endif

		if (row->allocated < (row->length + row->next->length + 1)) {
			if ((newp = realloc(row->string, (size_t)(row->length +
							  row->next->length
							  + 1))) == NULL)
				return E_REQUEST_DENIED;
			row->string = newp;
			row->allocated = row->length + row->next->length + 1;
		}

		strcat(row->string, row->next->string);
		old_len = row->length;
		row->length += row->next->length;
		if (row->length > 0)
			row->expanded =
				_formi_tab_expanded_length(row->string, 0,
							   row->length - 1);
		else
			row->expanded = 0;
		
		_formi_calculate_tabs(row);
		row->hard_ret = row->next->hard_ret;

		  /* adjust current line if it is on the row being eaten */
		if (field->cur_line == row->next) {
			field->cur_line = row;
			field->row_xpos += old_len;
			field->cursor_xpos =
				_formi_tab_expanded_length(row->string, 0,
							   field->row_xpos);
			if (field->cursor_xpos > 0)
				field->cursor_xpos--;
			
			if (field->cursor_ypos > 0)
				field->cursor_ypos--;
			else {
				if (field->start_line->prev != NULL)
					field->start_line =
						field->start_line->prev;
			}
		}

		  /* remove joined line record from the row list */
		add_to_free(field, row->next);

#ifdef DEBUG
		if (dbg_ok == TRUE) {
			fprintf(dbg,
				"join_line: exit length = %d, expanded = %d\n",
				row->length, row->expanded);
		}
#endif
	} else {
		if (row->prev == NULL) {
			return E_REQUEST_DENIED;
		}

		saved = row->prev;

		  /*
		   * Don't try to join if the line above has a hard
		   * return on it.
		   */
		if (saved->hard_ret == TRUE) {
			return E_REQUEST_DENIED;
		}

#ifdef DEBUG
		if (dbg_ok == TRUE) {
			fprintf(dbg,
			"join_line: join_prev before length = %d, expanded = %d",
				row->length, row->expanded);
			fprintf(dbg,
				" :: prev row length = %d, expanded = %d\n",
				saved->length, saved->expanded);
		}
#endif

		if (saved->allocated < (row->length + saved->length + 1)) {
			if ((newp = realloc(saved->string,
					    (size_t) (row->length +
						      saved->length
						      + 1))) == NULL)
				return E_REQUEST_DENIED;
			saved->string = newp;
			saved->allocated = row->length + saved->length + 1;
		}

		strcat(saved->string, row->string);
		old_len = saved->length;
		saved->length += row->length;
		if (saved->length > 0)
			saved->expanded =
				_formi_tab_expanded_length(saved->string, 0,
							   saved->length - 1);
		else
			saved->length = 0;

		saved->hard_ret = row->hard_ret;

		  /* adjust current line if it was on the row being eaten */
		if (field->cur_line == row) {
			field->cur_line = saved;
			field->row_xpos += old_len;
			field->cursor_xpos =
				_formi_tab_expanded_length(saved->string, 0,
							   field->row_xpos);
			if (field->cursor_xpos > 0)
				field->cursor_xpos--;
		}

		add_to_free(field, row);

#ifdef DEBUG
		if (dbg_ok == TRUE) {
			fprintf(dbg,
				"join_line: exit length = %d, expanded = %d\n",
				saved->length, saved->expanded);
		}
#endif
		row = saved;
	}


	  /*
	   * Work out where the line lies in the field in relation to
	   * the cursor_ypos.  First count the rows from the start of
	   * the field until we hit the row we just worked on.
	   */
	saved = field->start_line;
	count = 0;
	while (saved->next != NULL) {
		if (saved == row)
			break;
		count++;
		saved = saved->next;
	}

	  /* now check if we need to adjust cursor_ypos */
	if (field->cursor_ypos > count) {
		field->cursor_ypos--;
	}
	
	field->row_count--;
	*rowp = row;

	  /* wrap the field if required, if this fails undo the change */
	if ((direction == JOIN_NEXT) || (direction == JOIN_PREV)) {
		if (_formi_wrap_field(field, row) != E_OK) {
			return E_REQUEST_DENIED;
		}
	}

	return E_OK;
}

/*
 * Split the line at the given position, if possible.  If hard_split is
 * TRUE then split the line regardless of the position, otherwise don't
 * split at the beginning of a line.
 */
static int
split_line(FIELD *field, bool hard_split, unsigned pos,
	   _FORMI_FIELD_LINES **rowp)
{
	struct _formi_field_lines *new_line;
	char *newp;
	_FORMI_FIELD_LINES *row = *rowp;
#ifdef DEBUG
	short dbg_ok = FALSE;
#endif

	  /* if asked to split right where the line already starts then
	   * just return - nothing to do unless we are appending a line
	   * to the buffer.
	   */
	if ((pos == 0) && (hard_split == FALSE))
		return E_OK;

#ifdef DEBUG
	if (_formi_create_dbg_file() == E_OK) {
		fprintf(dbg, "split_line: splitting line at %d\n", pos);
		dbg_ok = TRUE;
	}
#endif

	  /* Need an extra line struct, check free list first */
	if (field->free != NULL) {
		new_line = field->free;
		field->free = new_line->next;
		if (field->free != NULL)
			field->free->prev = NULL;
	} else {
		if ((new_line = (struct _formi_field_lines *)
		     malloc(sizeof(struct _formi_field_lines))) == NULL)
			return E_SYSTEM_ERROR;
		new_line->prev = NULL;
		new_line->next = NULL;
		new_line->allocated = 0;
		new_line->length = 0;
		new_line->expanded = 0;
		new_line->string = NULL;
		new_line->hard_ret = FALSE;
		new_line->tabs = NULL;
	}

#ifdef DEBUG
	if (dbg_ok == TRUE) {
		fprintf(dbg,
	"split_line: enter: length = %d, expanded = %d\n",
			row->length, row->expanded);
	}
#endif

	assert((row->length < INT_MAX) && (row->expanded < INT_MAX));


	  /* add new line to the row list */
	new_line->next = row->next;
	new_line->prev = row;
	row->next = new_line;
	if (new_line->next != NULL)
		new_line->next->prev = new_line;

	new_line->length = row->length - pos;
	if (new_line->length >= new_line->allocated) {
		if ((newp = realloc(new_line->string,
				    (size_t) new_line->length + 1)) == NULL)
			return E_SYSTEM_ERROR;
		new_line->string = newp;
		new_line->allocated = new_line->length + 1;
	}

	strcpy(new_line->string, &row->string[pos]);

	row->length = pos;
	row->string[pos] = '\0';

	if (row->length != 0)
		row->expanded = _formi_tab_expanded_length(row->string, 0,
							   row->length - 1);
	else
		row->expanded = 0;
	_formi_calculate_tabs(row);

	if (new_line->length != 0)
		new_line->expanded =
			_formi_tab_expanded_length(new_line->string, 0,
						   new_line->length - 1);
	else
		new_line->expanded = 0;

	_formi_calculate_tabs(new_line);

	  /*
	   * If the given row was the current line then adjust the
	   * current line pointer if necessary
	   */
	if ((field->cur_line == row) && (field->row_xpos >= pos)) {
		field->cur_line = new_line;
		field->row_xpos -= pos;
		field->cursor_xpos =
			_formi_tab_expanded_length(new_line->string, 0,
						   field->row_xpos);
		if (field->cursor_xpos > 0)
			field->cursor_xpos--;
		
		field->cursor_ypos++;
		if (field->cursor_ypos >= field->rows) {
			if (field->start_line->next != NULL) {
				field->start_line = field->start_line->next;
				field->cursor_ypos = field->rows - 1;
			}
			else
				assert(field->start_line->next == NULL);
		}
	}

	  /*
	   * If the line split had a hard return then replace the
	   * current line's hard return with a soft return and carry
	   * the hard return onto the line after.
	   */
	if (row->hard_ret == TRUE) {
		new_line->hard_ret = TRUE;
		row->hard_ret = FALSE;
	}

	  /*
	   * except where we are doing a hard split then the current
	   * row must have a hard return on it too...
	   */
	if (hard_split == TRUE) {
		row->hard_ret = TRUE;
	}
	
	assert(((row->expanded < INT_MAX) &&
		(new_line->expanded < INT_MAX) &&
		(row->length < INT_MAX) &&
		(new_line->length < INT_MAX)));

#ifdef DEBUG
	if (dbg_ok == TRUE) {
		fprintf(dbg, "split_line: exit: ");
		fprintf(dbg, "row.length = %d, row.expanded = %d, ",
			row->length, row->expanded);
		fprintf(dbg,
			"next_line.length = %d, next_line.expanded = %d, ",
			new_line->length, new_line->expanded);
		fprintf(dbg, "row_count = %d\n", field->row_count + 1);
	}
#endif

	field->row_count++;
	*rowp = new_line;
	
	return E_OK;
}

/*
 * skip the blanks in the given string, start at the index start and
 * continue forward until either the end of the string or a non-blank
 * character is found.  Return the index of either the end of the string or
 * the first non-blank character.
 */
unsigned
_formi_skip_blanks(char *string, unsigned int start)
{
	unsigned int i;

	i = start;
	
	while ((string[i] != '\0') && isblank((unsigned char)string[i]))
		i++;

	return i;
}

/*
 * Skip the blanks in the string associated with the given row, pass back
 * the row and the offset at which the first non-blank is found.  If no
 * non-blank character is found then return the index to the last
 * character on the last line.
 */

unsigned
field_skip_blanks(unsigned int start, _FORMI_FIELD_LINES **rowp)
{
	unsigned int i;
	_FORMI_FIELD_LINES *row, *last = NULL;

	row = *rowp;
	i = start;

	do {
		i = _formi_skip_blanks(&row->string[i], i);
		if (!isblank((unsigned char)row->string[i])) {
			last = row;
			row = row->next;
			  /*
			   * don't reset if last line otherwise we will
			   * not be at the end of the string.
			   */
			if (row != NULL)
				i = 0;
		} else
			break;
	}
	while (row != NULL);

	  /*
	   * If we hit the end of the row list then point at the last row
	   * otherwise we return the row we found the blank on.
	   */
	if (row == NULL)
		*rowp = last;
	else
		*rowp = row;

	return i;
}

/*
 * Return the index of the top left most field of the two given fields.
 */
static int
_formi_top_left(FORM *form, int a, int b)
{
	  /* lower row numbers always win here.... */
	if (form->fields[a]->form_row < form->fields[b]->form_row)
		return a;

	if (form->fields[a]->form_row > form->fields[b]->form_row)
		return b;

	  /* rows must be equal, check columns */
	if (form->fields[a]->form_col < form->fields[b]->form_col)
		return a;

	if (form->fields[a]->form_col > form->fields[b]->form_col)
		return b;

	  /* if we get here fields must be in exactly the same place, punt */
	return a;
}

/*
 * Return the index to the field that is the bottom-right-most of the
 * two given fields.
 */
static int
_formi_bottom_right(FORM *form, int a, int b)
{
	  /* check the rows first, biggest row wins */
	if (form->fields[a]->form_row > form->fields[b]->form_row)
		return a;
	if (form->fields[a]->form_row < form->fields[b]->form_row)
		return b;

	  /* rows must be equal, check cols, biggest wins */
	if (form->fields[a]->form_col > form->fields[b]->form_col)
		return a;
	if (form->fields[a]->form_col < form->fields[b]->form_col)
		return b;

	  /* fields in the same place, punt */
	return a;
}

/*
 * Find the end of the current word in the string str, starting at
 * offset - the end includes any trailing whitespace.  If the end of
 * the string is found before a new word then just return the offset
 * to the end of the string.  If do_join is TRUE then lines will be
 * joined (without wrapping) until either the end of the field or the
 * end of a word is found (whichever comes first).
 */
static int
find_eow(FIELD *cur, unsigned int offset, bool do_join,
	 _FORMI_FIELD_LINES **rowp)
{
	int start;
	_FORMI_FIELD_LINES *row;

	row = *rowp;
	start = offset;

	do {
		  /* first skip any non-whitespace */
		while ((row->string[start] != '\0')
		       && !isblank((unsigned char)row->string[start]))
			start++;

		  /* see if we hit the end of the string */
		if (row->string[start] == '\0') {
			if (do_join == TRUE) {
				if (row->next == NULL)
					return start;
				
				if (_formi_join_line(cur, &row, JOIN_NEXT_NW)
				    != E_OK)
					return E_REQUEST_DENIED;
			} else {
				do {
					if (row->next == NULL) {
						*rowp = row;
						return start;
					} else {
						row = row->next;
						start = 0;
					}
				} while (row->length == 0);
			}
		}
	} while (!isblank((unsigned char)row->string[start]));

	do {
		  /* otherwise skip the whitespace.... */
		while ((row->string[start] != '\0')
		       && isblank((unsigned char)row->string[start]))
			start++;

		if (row->string[start] == '\0') {
			if (do_join == TRUE) {
				if (row->next == NULL)
					return start;
				
				if (_formi_join_line(cur, &row, JOIN_NEXT_NW)
				    != E_OK)
					return E_REQUEST_DENIED;
			} else {
				do {
					if (row->next == NULL) {
						*rowp = row;
						return start;
					} else {
						row = row->next;
						start = 0;
					}
				} while (row->length == 0);
			}
		}
	} while (isblank((unsigned char)row->string[start]));

	*rowp = row;
	return start;
}

/*
 * Find the beginning of the current word in the string str, starting
 * at offset.
 */
static int
find_sow(unsigned int offset, _FORMI_FIELD_LINES **rowp)
{
	int start;
	char *str;
	_FORMI_FIELD_LINES *row;

	row = *rowp;
	str = row->string;
	start = offset;

	do {
		if (start > 0) {
			if (isblank((unsigned char)str[start]) ||
			    isblank((unsigned char)str[start - 1])) {
				if (isblank((unsigned char)str[start - 1]))
					start--;
				  /* skip the whitespace.... */
				while ((start >= 0) &&
				    isblank((unsigned char)str[start]))
					start--;
			}
		}
		
		  /* see if we hit the start of the string */
		if (start < 0) {
			do {
				if (row->prev == NULL) {
					*rowp = row;
					start = 0;
					return start;
				} else {
					row = row->prev;
					str = row->string;
					if (row->length > 0)
						start = row->length - 1;
					else
						start = 0;
				}
			} while (row->length == 0);
		}
	} while (isblank((unsigned char)row->string[start]));

	  /* see if we hit the start of the string */
	if (start < 0) {
		*rowp = row;
		return 0;
	}

	  /* now skip any non-whitespace */
	do {
		while ((start >= 0) && !isblank((unsigned char)str[start]))
			start--;

		
		if (start < 0) {
			do {
				if (row->prev == NULL) {
					*rowp = row;
					start = 0;
					return start;
				} else {
					row = row->prev;
					str = row->string;
					if (row->length > 0)
						start = row->length - 1;
					else
						start = 0;
				}
			} while (row->length == 0);
		}
	} while (!isblank((unsigned char)str[start]));
	
	if (start > 0) {
		start++; /* last loop has us pointing at a space, adjust */
		if (start >= row->length) {
			if (row->next != NULL) {
				start = 0;
				row = row->next;
			} else {
				start = row->length - 1;
			}
		}
	}
	
	if (start < 0)
		start = 0;

	*rowp = row;
	return start;
}

/*
 * Scroll the field forward the given number of lines.
 */
static void
_formi_scroll_fwd(FIELD *field, unsigned int amt)
{
	unsigned int count;
	_FORMI_FIELD_LINES *end_row;

	end_row = field->start_line;
	  /* walk the line structs forward to find the bottom of the field */
	count = field->rows - 1;
	while ((count > 0) && (end_row->next != NULL))
	{
		count--;
		end_row = end_row->next;
	}

	  /* check if there are lines to scroll */
	if ((count > 0) && (end_row->next == NULL))
		return;

	  /*
	   * ok, lines to scroll - do this by walking both the start_line
	   * and the end_row at the same time for amt lines, we stop when
	   * either we have done the number of lines or end_row hits the
	   * last line in the field.
	   */
	count = amt;
	while ((count > 0) && (end_row->next != NULL)) {
		count--;
		field->start_line = field->start_line->next;
		end_row = end_row->next;
	}
}

/*
 * Scroll the field backward the given number of lines.
 */
static void
_formi_scroll_back(FIELD *field, unsigned int amt)
{
	unsigned int count;

	  /* check for lines above */
	if (field->start_line->prev == NULL)
		return;

	  /*
	   * Backward scroll is easy, follow row struct chain backward until
	   * the number of lines done or we reach the top of the field.
	   */
	count = amt;
	while ((count > 0) && (field->start_line->prev != NULL)) {
		count--;
		field->start_line = field->start_line->prev;
	}
}

/*
 * Scroll the field forward the given number of characters.
 */
void
_formi_hscroll_fwd(FIELD *field, _FORMI_FIELD_LINES *row, int unsigned amt)
{
	unsigned int end, scroll_amt, expanded;
	_formi_tab_t *ts;


	if ((row->tabs == NULL) || (row->tabs->in_use == FALSE)) {
		  /* if the line has no tabs things are easy... */
		end =  field->start_char + field->cols + amt - 1;
		scroll_amt = amt;
		if (end > row->length) {
			end = row->length;
			scroll_amt = end - field->start_char - field->cols + 1;
		}
	} else {
		  /*
		   * If there are tabs we need to add on the scroll amount,
		   * find the last char position that will fit into
		   * the field and finally fix up the start_char.  This
		   * is a lot of work but handling the case where there
		   * are not enough chars to scroll by amt is difficult.
		   */
		end = field->start_char + field->row_xpos + amt;
		if (end >= row->length)
			end = row->length - 1;
		else {
			expanded = _formi_tab_expanded_length(
				row->string,
				field->start_char + amt,
				field->start_char + field->row_xpos + amt);
			ts = row->tabs;
			  /* skip tabs to the lhs of our starting point */
			while ((ts != NULL) && (ts->in_use == TRUE)
			       && (ts->pos < end))
				ts = ts->fwd;

			while ((expanded <= field->cols)
			       && (end < row->length)) {
				if (row->string[end] == '\t') {
					assert((ts != NULL)
					       && (ts->in_use == TRUE));
					if (ts->pos == end) {
						if ((expanded + ts->size)
						    > field->cols)
							break;
						expanded += ts->size;
						ts = ts->fwd;
					}
					else
						assert(ts->pos == end);
				} else
					expanded++;
				end++;
			}
		}

		scroll_amt = tab_fit_window(field, end, field->cols);
		if (scroll_amt < field->start_char)
			scroll_amt = 1;
		else
			scroll_amt -= field->start_char;

		scroll_amt = min(scroll_amt, amt);
	}

	field->start_char += scroll_amt;
	field->cursor_xpos =
		_formi_tab_expanded_length(row->string,
					   field->start_char,
					   field->row_xpos
					   + field->start_char) - 1;

}

/*
 * Scroll the field backward the given number of characters.
 */
void
_formi_hscroll_back(FIELD *field, _FORMI_FIELD_LINES *row, unsigned int amt)
{
	field->start_char -= min(field->start_char, amt);
	field->cursor_xpos =
		_formi_tab_expanded_length(row->string, field->start_char,
					   field->row_xpos
					   + field->start_char) - 1;
	if (field->cursor_xpos >= field->cols) {
		field->row_xpos = 0;
		field->cursor_xpos = 0;
	}
}

/*
 * Find the different pages in the form fields and assign the form
 * page_starts array with the information to find them.
 */
int
_formi_find_pages(FORM *form)
{
	int i, cur_page = 0;

	if ((form->page_starts = (_FORMI_PAGE_START *)
	     malloc((form->max_page + 1) * sizeof(_FORMI_PAGE_START))) == NULL)
		return E_SYSTEM_ERROR;

	  /* initialise the page starts array */
	memset(form->page_starts, 0,
	       (form->max_page + 1) * sizeof(_FORMI_PAGE_START));

	for (i =0; i < form->field_count; i++) {
		if (form->fields[i]->page_break == 1)
			cur_page++;
		if (form->page_starts[cur_page].in_use == 0) {
			form->page_starts[cur_page].in_use = 1;
			form->page_starts[cur_page].first = i;
			form->page_starts[cur_page].last = i;
			form->page_starts[cur_page].top_left = i;
			form->page_starts[cur_page].bottom_right = i;
		} else {
			form->page_starts[cur_page].last = i;
			form->page_starts[cur_page].top_left =
				_formi_top_left(form,
						form->page_starts[cur_page].top_left,
						i);
			form->page_starts[cur_page].bottom_right =
				_formi_bottom_right(form,
						    form->page_starts[cur_page].bottom_right,
						    i);
		}
	}

	return E_OK;
}

/*
 * Completely redraw the field of the given form.
 */
void
_formi_redraw_field(FORM *form, int field)
{
	unsigned int pre, post, flen, slen, i, j, start, line;
	unsigned int tab, cpos, len;
	char *str, c;
	FIELD *cur;
	_FORMI_FIELD_LINES *row;
#ifdef DEBUG
	char buffer[100];
#endif

	cur = form->fields[field];
	flen = cur->cols;
	slen = 0;
	start = 0;
	line = 0;

	for (row = cur->start_line; ((row != NULL) && (line < cur->rows));
	     row = row->next, line++) {
		wmove(form->scrwin, (int) (cur->form_row + line),
		      (int) cur->form_col);
		if ((cur->rows + cur->nrows) == 1) {
			if ((cur->cols + cur->start_char) >= row->length)
				len = row->length;
			else
				len = cur->cols + cur->start_char;
			if (row->string != NULL)
				slen = _formi_tab_expanded_length(
					row->string, cur->start_char, len);
			else
				slen = 0;
			
			if (slen > cur->cols)
				slen = cur->cols;
			slen += cur->start_char;
		} else
			slen = row->expanded;

		if ((cur->opts & O_STATIC) == O_STATIC) {
			switch (cur->justification) {
			case JUSTIFY_RIGHT:
				post = 0;
				if (flen < slen)
					pre = 0;
				else
					pre = flen - slen;
				break;

			case JUSTIFY_CENTER:
				if (flen < slen) {
					pre = 0;
					post = 0;
				} else {
					pre = flen - slen;
					post = pre = pre / 2;
					  /* get padding right if
					     centring is not even */
					if ((post + pre + slen) < flen)
						post++;
				}
				break;

			case NO_JUSTIFICATION:
			case JUSTIFY_LEFT:
			default:
				pre = 0;
				if (flen <= slen)
					post = 0;
				else {
					post = flen - slen;
					if (post > flen)
						post = flen;
				}
				break;
			}
		} else {
			  /* dynamic fields are not justified */
			pre = 0;
			if (flen <= slen)
				post = 0;
			else {
				post = flen - slen;
				if (post > flen)
					post = flen;
			}

			  /* but they do scroll.... */

			if (pre > cur->start_char - start)
				pre = pre - cur->start_char + start;
			else
				pre = 0;

			if (slen > cur->start_char) {
				slen -= cur->start_char;
				if (slen > flen)
					post = 0;
				else
					post = flen - slen;

				if (post > flen)
					post = flen;
			} else {
				slen = 0;
				post = flen - pre;
			}
		}

		if (form->cur_field == field)
			wattrset(form->scrwin, cur->fore);
		else
			wattrset(form->scrwin, cur->back);

		str = &row->string[cur->start_char];

#ifdef DEBUG
		if (_formi_create_dbg_file() == E_OK) {
			fprintf(dbg,
  "redraw_field: start=%d, pre=%d, slen=%d, flen=%d, post=%d, start_char=%d\n",
				start, pre, slen, flen, post, cur->start_char);
			if (str != NULL) {
				if (row->expanded != 0) {
					strncpy(buffer, str, flen);
				} else {
					strcpy(buffer, "(empty)");
				}
			} else {
				strcpy(buffer, "(null)");
			}
			buffer[flen] = '\0';
			fprintf(dbg, "redraw_field: %s\n", buffer);
		}
#endif

		for (i = start + cur->start_char; i < pre; i++)
			waddch(form->scrwin, cur->pad);

#ifdef DEBUG
		fprintf(dbg, "redraw_field: will add %d chars\n",
			min(slen, flen));
#endif
		for (i = 0, cpos = cur->start_char; i < min(slen, flen);
		     i++, str++, cpos++) 
		{
			c = *str;
			tab = 0; /* just to shut gcc up */
#ifdef DEBUG
			fprintf(dbg, "adding char str[%d]=%c\n",
				cpos + cur->start_char,	c);
#endif
			if (((cur->opts & O_PUBLIC) != O_PUBLIC)) {
				if (c == '\t')
					tab = add_tab(form, row, cpos,
						      cur->pad);
				else
					waddch(form->scrwin, cur->pad);
			} else if ((cur->opts & O_VISIBLE) == O_VISIBLE) {
				if (c == '\t')
					tab = add_tab(form, row, cpos, ' ');
				else
					waddch(form->scrwin, c);
			} else {
				if (c == '\t')
					tab = add_tab(form, row, cpos, ' ');
				else
					waddch(form->scrwin, ' ');
			}

			  /*
			   * If we have had a tab then skip forward
			   * the requisite number of chars to keep
			   * things in sync.
			   */
			if (c == '\t')
				i += tab - 1;
		}

		for (i = 0; i < post; i++)
			waddch(form->scrwin, cur->pad);
	}

	for (i = line; i < cur->rows; i++) {
		wmove(form->scrwin, (int) (cur->form_row + i),
		      (int) cur->form_col);

		if (form->cur_field == field)
			wattrset(form->scrwin, cur->fore);
		else
			wattrset(form->scrwin, cur->back);

		for (j = 0; j < cur->cols; j++) {
			waddch(form->scrwin, cur->pad);
		}
	}

	wattrset(form->scrwin, cur->back);
	return;
}

/*
 * Add the correct number of the given character to simulate a tab
 * in the field.
 */
static int
add_tab(FORM *form, _FORMI_FIELD_LINES *row, unsigned int i, char c)
{
	int j;
	_formi_tab_t *ts = row->tabs;

	while ((ts != NULL) && (ts->pos != i))
		ts = ts->fwd;

	assert(ts != NULL);

	for (j = 0; j < ts->size; j++)
		waddch(form->scrwin, c);

	return ts->size;
}


/*
 * Display the fields attached to the form that are on the current page
 * on the screen.
 *
 */
int
_formi_draw_page(FORM *form)
{
	int i;

	if (form->page_starts[form->page].in_use == 0)
		return E_BAD_ARGUMENT;

	wclear(form->scrwin);

	for (i = form->page_starts[form->page].first;
	     i <= form->page_starts[form->page].last; i++)
		_formi_redraw_field(form, i);

	return E_OK;
}

/*
 * Add the character c at the position pos in buffer 0 of the given field
 */
int
_formi_add_char(FIELD *field, unsigned int pos, char c)
{
	char *new, old_c;
	unsigned int new_size;
	int status;
	_FORMI_FIELD_LINES *row, *temp, *next_temp;

	row = field->cur_line;

	  /*
	   * If buffer has not had a string before, set it to a blank
	   * string.  Everything should flow from there....
	   */
	if (row->string == NULL) {
		if ((row->string = (char *) malloc((size_t)INITIAL_LINE_ALLOC))
		    == NULL)
			return E_SYSTEM_ERROR;
		row->string[0] = '\0';
		row->allocated = INITIAL_LINE_ALLOC;
		row->length = 0;
		row->expanded = 0;
	}

	if (_formi_validate_char(field, c) != E_OK) {
#ifdef DEBUG
		fprintf(dbg, "add_char: char %c failed char validation\n", c);
#endif
		return E_INVALID_FIELD;
	}

	if ((c == '\t') && (field->cols <= 8)) {
#ifdef DEBUG
		fprintf(dbg, "add_char: field too small for a tab\n");
#endif
		return E_NO_ROOM;
	}

#ifdef DEBUG
	fprintf(dbg, "add_char: pos=%d, char=%c\n", pos, c);
	fprintf(dbg, "add_char enter: xpos=%d, row_pos=%d, start=%d\n",
		field->cursor_xpos, field->row_xpos, field->start_char);
	fprintf(dbg, "add_char enter: length=%d(%d), allocated=%d\n",
		row->expanded, row->length, row->allocated);
	fprintf(dbg, "add_char enter: %s\n", row->string);
	fprintf(dbg, "add_char enter: buf0_status=%d\n", field->buf0_status);
#endif
	if (((field->opts & O_BLANK) == O_BLANK) &&
	    (field->buf0_status == FALSE) &&
	    ((field->row_xpos + field->start_char) == 0)) {
		row = field->alines;
		if (row->next != NULL) {
			  /* shift all but one line structs to free list */
			temp = row->next;
			do {
				next_temp = temp->next;
				add_to_free(field, temp);
				temp = next_temp;
			} while (temp != NULL);
		}

		row->length = 0;
		row->string[0] = '\0';
		pos = 0;
		field->start_char = 0;
		field->start_line = row;
		field->cur_line = row;
		field->row_count = 1;
		field->row_xpos = 0;
		field->cursor_ypos = 0;
		row->expanded = 0;
		row->length = 0;
		_formi_init_field_xpos(field);
	}


	if ((field->overlay == 0)
	    || ((field->overlay == 1) && (pos >= row->length))) {
		  /* first check if the field can have more chars...*/
		if (check_field_size(field) == FALSE)
			return E_REQUEST_DENIED;

		if (row->length + 2
		    >= row->allocated) {
			new_size = row->allocated + 16 - (row->allocated % 16);
			if ((new = (char *) realloc(row->string,
						  (size_t) new_size )) == NULL)
				return E_SYSTEM_ERROR;
			row->allocated = new_size;
			row->string = new;
		}
	}

	if ((field->overlay == 0) && (row->length > pos)) {
		bcopy(&row->string[pos], &row->string[pos + 1],
		      (size_t) (row->length - pos + 1));
	}

	old_c = row->string[pos];
	row->string[pos] = c;
	if (pos >= row->length) {
		  /* make sure the string is terminated if we are at the
		   * end of the string, the terminator would be missing
		   * if we are are at the end of the field.
		   */
		row->string[pos + 1] = '\0';
	}

	  /* only increment the length if we are inserting characters
	   * OR if we are at the end of the field in overlay mode.
	   */
	if ((field->overlay == 0)
	    || ((field->overlay == 1) && (pos >= row->length))) {
		row->length++;
	}

	_formi_calculate_tabs(row);
	row->expanded = _formi_tab_expanded_length(row->string, 0,
						   row->length - 1);

	  /* wrap the field, if needed */
	status = _formi_wrap_field(field, row);

	row = field->cur_line;
	pos = field->row_xpos;

	  /*
	   * check the wrap worked or that we have not exceeded the
	   * max field size - this can happen if the field is re-wrapped
	   * and the row count is increased past the set limit.
	   */
	if ((status != E_OK) || (check_field_size(field) == FALSE)) {
		if ((field->overlay == 0)
		    || ((field->overlay == 1)
			&& (pos >= (row->length - 1) /*XXXX- append check???*/))) {
			  /*
			   * wrap failed for some reason, back out the
			   * char insert
			   */
			bcopy(&row->string[pos + 1], &row->string[pos],
			      (size_t) (row->length - pos));
			row->length--;
			if (pos > 0)
				pos--;
		} else if (field->overlay == 1) {
			  /* back out character overlay */
			row->string[pos] = old_c;
		}

		_formi_calculate_tabs(row);

		_formi_wrap_field(field, row);
		  /*
		   * If we are here then either the status is bad or we
		   * simply ran out of room.  If the status is E_OK then
		   * we ran out of room, let the form driver know this.
		   */
		if (status == E_OK)
			status = E_REQUEST_DENIED;

	} else {
		field->buf0_status = TRUE;
		field->row_xpos++;
		if ((field->rows + field->nrows) == 1) {
			status = _formi_set_cursor_xpos(field, FALSE);
		} else {
			field->cursor_xpos =
				_formi_tab_expanded_length(
					row->string, 0,	field->row_xpos - 1);

			  /*
			   * Annoying corner case - if we are right in
			   * the bottom right corner of the field we
			   * need to scroll the field one line so the
			   * cursor is positioned correctly in the
			   * field.
			   */
			if ((field->cursor_xpos >= field->cols) &&
			    (field->cursor_ypos == (field->rows - 1))) {
				field->cursor_ypos--;
				field->start_line = field->start_line->next;
			}
		}
	}

	assert((field->cursor_xpos <= field->cols)
	       && (field->cursor_ypos < 400000));

#ifdef DEBUG
	fprintf(dbg, "add_char exit: xpos=%d, row_pos=%d, start=%d\n",
		field->cursor_xpos, field->row_xpos, field->start_char);
	fprintf(dbg, "add_char_exit: length=%d(%d), allocated=%d\n",
		row->expanded, row->length, row->allocated);
	fprintf(dbg, "add_char exit: ypos=%d, start_line=%p\n",
		field->cursor_ypos, field->start_line);
	fprintf(dbg,"add_char exit: %s\n", row->string);
	fprintf(dbg, "add_char exit: buf0_status=%d\n", field->buf0_status);
	fprintf(dbg, "add_char exit: status = %s\n",
		(status == E_OK)? "OK" : "FAILED");
#endif
	return status;
}

/*
 * Set the position of the cursor on the screen in the row depending on
 * where the current position in the string is and the justification
 * that is to be applied to the field.  Justification is only applied
 * to single row, static fields.
 */
static int
_formi_set_cursor_xpos(FIELD *field, int noscroll)
{
	int just, pos;

	just = field->justification;
	pos = field->start_char + field->row_xpos;

#ifdef DEBUG
	fprintf(dbg,
	  "cursor_xpos enter: pos %d, start_char %d, row_xpos %d, xpos %d\n",
		pos, field->start_char, field->row_xpos, field->cursor_xpos);
#endif

	  /*
	   * make sure we apply the correct justification to non-static
	   * fields.
	   */
	if (((field->rows + field->nrows) != 1) ||
	    ((field->opts & O_STATIC) != O_STATIC))
		just = JUSTIFY_LEFT;

	switch (just) {
	case JUSTIFY_RIGHT:
		field->cursor_xpos = field->cols - 1
			- _formi_tab_expanded_length(
				field->cur_line->string, 0,
				field->cur_line->length - 1)
			+ _formi_tab_expanded_length(
				field->cur_line->string, 0,
				field->row_xpos);
		break;

	case JUSTIFY_CENTER:
		field->cursor_xpos = ((field->cols - 1) 
			- _formi_tab_expanded_length(
				field->cur_line->string, 0,
				field->cur_line->length - 1) + 1) / 2
			+ _formi_tab_expanded_length(field->cur_line->string,
						     0, field->row_xpos);

		if (field->cursor_xpos > (field->cols - 1))
			field->cursor_xpos = (field->cols - 1);
		break;

	default:
		field->cursor_xpos = _formi_tab_expanded_length(
			field->cur_line->string,
			field->start_char,
			field->row_xpos	+ field->start_char);
		if ((field->cursor_xpos <= (field->cols - 1)) &&
		    ((field->start_char + field->row_xpos)
		     < field->cur_line->length))
			field->cursor_xpos--;

		if (field->cursor_xpos > (field->cols - 1)) {
			if ((field->opts & O_STATIC) == O_STATIC) {
				field->start_char = 0;

				if (field->row_xpos
				    == (field->cur_line->length - 1)) {
					field->cursor_xpos = field->cols - 1;
				} else {
					field->cursor_xpos =
						_formi_tab_expanded_length(
						  field->cur_line->string,
						  field->start_char,
						  field->row_xpos
						    + field->start_char
						  - 1) - 1;
				}
			} else {
				if (noscroll == FALSE) {
					field->start_char =
						tab_fit_window(
							field,
							field->start_char
							+ field->row_xpos,
							field->cols);
					field->row_xpos = pos
						- field->start_char;
					field->cursor_xpos =
						_formi_tab_expanded_length(
						   field->cur_line->string,
						   field->start_char,
						   field->row_xpos
						      + field->start_char - 1);
				} else {
					field->cursor_xpos = (field->cols - 1);
				}
			}

		}
		break;
	}

#ifdef DEBUG
	fprintf(dbg,
	  "cursor_xpos exit: pos %d, start_char %d, row_xpos %d, xpos %d\n",
		pos, field->start_char, field->row_xpos, field->cursor_xpos);
#endif
	return E_OK;
}

/*
 * Manipulate the text in a field, this takes the given form and performs
 * the passed driver command on the current text field.  Returns 1 if the
 * text field was modified.
 */
int
_formi_manipulate_field(FORM *form, int c)
{
	FIELD *cur;
	char *str, saved;
	unsigned int start, end, pos, status, old_count, size;
	unsigned int old_xpos, old_row_pos;
	int len, wb;
	bool eat_char;
	_FORMI_FIELD_LINES *row, *rs;

	cur = form->fields[form->cur_field];
	if (cur->cur_line->string == NULL)
		return E_REQUEST_DENIED;

#ifdef DEBUG
	fprintf(dbg, "entry: request is REQ_%s\n", reqs[c - REQ_MIN_REQUEST]);
	fprintf(dbg,
	"entry: xpos=%d, row_pos=%d, start_char=%d, length=%d, allocated=%d\n",
		cur->cursor_xpos, cur->row_xpos, cur->start_char,
		cur->cur_line->length,	cur->cur_line->allocated);
	fprintf(dbg, "entry: start_line=%p, ypos=%d\n", cur->start_line,
		cur->cursor_ypos);
	fprintf(dbg, "entry: string=");
	if (cur->cur_line->string == NULL)
		fprintf(dbg, "(null)\n");
	else
		fprintf(dbg, "\"%s\"\n", cur->cur_line->string);
#endif

	  /* Cannot manipulate a null string! */
	if (cur->cur_line->string == NULL)
		return E_REQUEST_DENIED;

	saved = '\0';
	row = cur->cur_line;

	switch (c) {
	case REQ_RIGHT_CHAR:
		  /*
		   * The right_char request performs the same function
		   * as the next_char request except that the cursor is
		   * not wrapped if it is at the end of the line, so
		   * check if the cursor is at the end of the line and
		   * deny the request otherwise just fall through to
		   * the next_char request handler.
		   */
		if (cur->cursor_xpos >= cur->cols - 1)
			return E_REQUEST_DENIED;

		  /* FALLTHRU */

	case REQ_NEXT_CHAR:
		  /* for a dynamic field allow an offset of one more
		   * char so we can insert chars after end of string.
		   * Static fields cannot do this so deny request if
		   * cursor is at the end of the field.
		   */
		if (((cur->opts & O_STATIC) == O_STATIC) &&
		    (cur->row_xpos == cur->cols - 1) &&
		    ((cur->rows + cur->nrows) == 1))
			return E_REQUEST_DENIED;

		if (((cur->rows + cur->nrows) == 1) &&
		    (cur->row_xpos + cur->start_char + 1) > row->length)
			return E_REQUEST_DENIED;

		if ((cur->rows + cur->nrows) == 1) {
			cur->row_xpos++;
			_formi_set_cursor_xpos(cur, (c == REQ_RIGHT_CHAR));
		} else {
			if (cur->cursor_xpos >= (row->expanded - 1)) {
				if ((row->next == NULL) ||
				    (c == REQ_RIGHT_CHAR))
					return E_REQUEST_DENIED;

				cur->cursor_xpos = 0;
				cur->row_xpos = 0;
				cur->cur_line = cur->cur_line->next;
				if (cur->cursor_ypos == (cur->rows - 1))
					cur->start_line =
						cur->start_line->next;
				else
					cur->cursor_ypos++;
			} else {
				old_xpos = cur->cursor_xpos;
				old_row_pos = cur->row_xpos;
				if (row->string[cur->row_xpos] == '\t')
					cur->cursor_xpos += tab_size(row,
							cur->row_xpos);
				else
					cur->cursor_xpos++;
				cur->row_xpos++;
				if (cur->cursor_xpos
				    >= row->expanded) {
					if ((row->next == NULL) ||
					    (c == REQ_RIGHT_CHAR)) {
						cur->cursor_xpos = old_xpos;
						cur->row_xpos = old_row_pos;
						return E_REQUEST_DENIED;
					}

					cur->cursor_xpos = 0;
					cur->row_xpos = 0;
					cur->cur_line = cur->cur_line->next;
					if (cur->cursor_ypos
					    == (cur->rows - 1))
						cur->start_line =
							cur->start_line->next;
					else
						cur->cursor_ypos++;
				}
			}
		}

		break;

	case REQ_LEFT_CHAR:
		  /*
		   * The behaviour of left_char is the same as prev_char
		   * except that the cursor will not wrap if it has
		   * reached the LHS of the field, so just check this
		   * and fall through if we are not at the LHS.
		   */
		if (cur->cursor_xpos == 0)
			return E_REQUEST_DENIED;

		  /* FALLTHRU */
	case REQ_PREV_CHAR:
		if ((cur->rows + cur->nrows) == 1) {
			if (cur->row_xpos == 0) {
				if (cur->start_char > 0)
					cur->start_char--;
				else
					return E_REQUEST_DENIED;
			} else {
				cur->row_xpos--;
				_formi_set_cursor_xpos(cur, FALSE);
			}
		} else {
			if ((cur->cursor_xpos == 0) &&
			    (cur->cursor_ypos == 0) &&
			    (cur->start_line->prev == NULL))
				return E_REQUEST_DENIED;

			pos = cur->row_xpos;
			if (cur->cursor_xpos > 0) {
				if (row->string[pos] == '\t') {
					size = tab_size(row, pos);
					if (size > cur->cursor_xpos) {
						cur->cursor_xpos = 0;
						cur->row_xpos = 0;
					} else {
						cur->row_xpos--;
						cur->cursor_xpos -= size;
					}
				} else {
					cur->cursor_xpos--;
					cur->row_xpos--;
				}
			} else {
				cur->cur_line = cur->cur_line->prev;
				if (cur->cursor_ypos > 0)
					cur->cursor_ypos--;
				else
					cur->start_line =
						cur->start_line->prev;
				row = cur->cur_line;
				if (row->expanded > 0) {
					cur->cursor_xpos = row->expanded - 1;
				} else {
					cur->cursor_xpos = 0;
				}

				if (row->length > 0)
					cur->row_xpos = row->length - 1;
				else
					cur->row_xpos = 0;
			}
		}

		break;

	case REQ_DOWN_CHAR:
		  /*
		   * The down_char request has the same functionality as
		   * the next_line request excepting that the field is not
		   * scrolled if the cursor is at the bottom of the field.
		   * Check to see if the cursor is at the bottom of the field
		   * and if it is then deny the request otherwise fall
		   * through to the next_line handler.
		   */
		if (cur->cursor_ypos >= cur->rows - 1)
			return E_REQUEST_DENIED;

		  /* FALLTHRU */

	case REQ_NEXT_LINE:
		if ((row->next == NULL) || (cur->cur_line->next == NULL))
			return E_REQUEST_DENIED;

		cur->cur_line = cur->cur_line->next;
		if ((cur->cursor_ypos + 1) >= cur->rows) {
			cur->start_line = cur->start_line->next;
		} else
			cur->cursor_ypos++;
		row = cur->cur_line;

		if (row->length == 0) {
			cur->row_xpos = 0;
			cur->cursor_xpos = 0;
		} else {
			if (cur->cursor_xpos > (row->expanded - 1))
				cur->cursor_xpos = row->expanded - 1;

			cur->row_xpos =	tab_fit_len(row, cur->cursor_xpos + 1);
			if (cur->row_xpos == 0)
				cur->cursor_xpos = 0;
			else
				cur->cursor_xpos =
					_formi_tab_expanded_length(
						row->string, 0, cur->row_xpos);
			if (cur->cursor_xpos > 0)
				cur->cursor_xpos--;
		}
		break;

	case REQ_UP_CHAR:
		  /*
		   * The up_char request has the same functionality as
		   * the prev_line request excepting the field is not
		   * scrolled, check if the cursor is at the top of the
		   * field, if it is deny the request otherwise fall
		   * through to the prev_line handler.
		   */
		if (cur->cursor_ypos == 0)
			return E_REQUEST_DENIED;

		  /* FALLTHRU */

	case REQ_PREV_LINE:
		if (cur->cur_line->prev == NULL)
			return E_REQUEST_DENIED;

		if (cur->cursor_ypos == 0) {
			if (cur->start_line->prev == NULL)
				return E_REQUEST_DENIED;
			cur->start_line = cur->start_line->prev;
		} else
			cur->cursor_ypos--;

		cur->cur_line = cur->cur_line->prev;
		row = cur->cur_line;

		if (row->length == 0) {
			cur->row_xpos = 0;
			cur->cursor_xpos = 0;
		} else {
			if (cur->cursor_xpos > (row->expanded - 1))
				cur->cursor_xpos = row->expanded - 1;

			cur->row_xpos =	tab_fit_len(row, cur->cursor_xpos + 1);
			cur->cursor_xpos =
				_formi_tab_expanded_length(row->string,
							   0, cur->row_xpos);
			if (cur->cursor_xpos > 0)
				cur->cursor_xpos--;
		}
		break;

	case REQ_NEXT_WORD:
		start = cur->row_xpos + cur->start_char;
		str = row->string;

		wb = find_eow(cur, start, FALSE, &row);
		if (wb < 0)
			return wb;

		start = wb;
		  /* check if we hit the end */
		if (str[start] == '\0')
			return E_REQUEST_DENIED;

		  /* otherwise we must have found the start of a word...*/
		if ((cur->rows + cur->nrows) == 1) {
			  /* single line field */
			size = _formi_tab_expanded_length(str,
				cur->start_char, start);
			if (size < cur->cols) {
				cur->row_xpos = start - cur->start_char;
			} else {
				cur->start_char = start;
				cur->row_xpos = 0;
			}
			_formi_set_cursor_xpos(cur, FALSE);
		} else {
			  /* multiline field */
			cur->cur_line = row;
			adjust_ypos(cur, row);

			cur->row_xpos = start;
			cur->cursor_xpos =
				_formi_tab_expanded_length(
					row->string, 0, cur->row_xpos) - 1;
		}
		break;
		
	case REQ_PREV_WORD:
		start = cur->start_char + cur->row_xpos;
		if (cur->start_char > 0)
			start--;

		if ((start == 0) && (row->prev == NULL))
			return E_REQUEST_DENIED;

		if (start == 0) {
			row = row->prev;
			if (row->length > 0)
				start = row->length - 1;
			else
				start = 0;
		}

		str = row->string;

		start = find_sow(start, &row);
		
		if ((cur->rows + cur->nrows) == 1) {
			  /* single line field */
			size = _formi_tab_expanded_length(str,
				cur->start_char, start);
			
			if (start > cur->start_char) {
				cur->row_xpos = start - cur->start_char;
			} else {
				cur->start_char = start;
				cur->row_xpos = 0;
			}
			_formi_set_cursor_xpos(cur, FALSE);
		} else {
			  /* multiline field */
			cur->cur_line = row;
			adjust_ypos(cur, row);
			cur->row_xpos = start;
			cur->cursor_xpos =
				_formi_tab_expanded_length(
					row->string, 0,
					cur->row_xpos) - 1;
		}
		
		break;

	case REQ_BEG_FIELD:
		cur->start_char = 0;
		while (cur->start_line->prev != NULL)
			cur->start_line = cur->start_line->prev;
		cur->cur_line = cur->start_line;
		cur->row_xpos = 0;
		_formi_init_field_xpos(cur);
		cur->cursor_ypos = 0;
		break;
		
	case REQ_BEG_LINE:
		cur->row_xpos = 0;
		_formi_init_field_xpos(cur);
		cur->start_char = 0;
		break;
			
	case REQ_END_FIELD:
		while (cur->cur_line->next != NULL)
			cur->cur_line = cur->cur_line->next;
		
		if (cur->row_count > cur->rows) {
			cur->start_line = cur->cur_line;
			pos = cur->rows - 1;
			while (pos > 0) {
				cur->start_line = cur->start_line->prev;
				pos--;
			}
			cur->cursor_ypos = cur->rows - 1;
		} else {
			cur->cursor_ypos = cur->row_count - 1;
		}

		  /* we fall through here deliberately, we are on the
		   * correct row, now we need to get to the end of the
		   * line.
		   */
		  /* FALLTHRU */
		
	case REQ_END_LINE:
		row = cur->cur_line;

		if ((cur->rows + cur->nrows) == 1) {
			if (row->expanded > cur->cols - 1) {
				if ((cur->opts & O_STATIC) != O_STATIC) {
					cur->start_char = tab_fit_window(
						cur, row->length,
						cur->cols) + 1;
					cur->row_xpos = row->length
						- cur->start_char;
				} else {
					cur->start_char = 0;
					cur->row_xpos = cur->cols - 1;
				}
			} else {
				cur->row_xpos = row->length + 1;
				cur->start_char = 0;
			}
			_formi_set_cursor_xpos(cur, FALSE);
		} else {
			cur->row_xpos = row->length - 1;
			cur->cursor_xpos = row->expanded - 1;
			if (row->next == NULL) {
				cur->row_xpos++;
				cur->cursor_xpos++;
			}
		}
		break;
		
	case REQ_NEW_LINE:
		start = cur->start_char	+ cur->row_xpos;
		if ((status = split_line(cur, TRUE, start, &row)) != E_OK)
			return status;
		cur->cur_line->hard_ret = TRUE;
		cur->cursor_xpos = 0;
		cur->row_xpos = 0;
		break;
		
	case REQ_INS_CHAR:
		if ((status = _formi_add_char(cur, cur->start_char
					      + cur->row_xpos,
					      cur->pad)) != E_OK)
			return status;
		break;
		
	case REQ_INS_LINE:
		if ((status = split_line(cur, TRUE, 0, &row)) != E_OK)
			return status;
		cur->cur_line->hard_ret = TRUE;
		break;
		
	case REQ_DEL_CHAR:
		row = cur->cur_line;
		start = cur->start_char + cur->row_xpos;
		end = row->length - 1;
		if ((start >= row->length) && (row->next == NULL))
			return E_REQUEST_DENIED;
		
		if ((start == row->length - 1) || (row->length == 0)) {
			if ((cur->rows + cur->nrows) > 1) {
				/*
				 * Firstly, check if the current line has
				 * a hard return.  In this case we just
				 * want to "delete" the hard return and
				 * re-wrap the field.  The hard return
				 * does not occupy a character space in
				 * the buffer but we must make it appear
				 * like it does for a deletion.
				 */
				if (row->hard_ret == TRUE) {
					row->hard_ret = FALSE;
					if (_formi_join_line(cur, &row,
							     JOIN_NEXT)
					    != E_OK) {
						row->hard_ret = TRUE;
						return 0;
					} else {
						return 1;
					}
				}
					
				/*
				 * If we have more than one row, join the
				 * next row to make things easier unless
				 * we are at the end of the string, in
				 * that case the join would fail but we
				 * really want to delete the last char
				 * in the field.
				 */
				if (row->next != NULL) {
					if (_formi_join_line(cur, &row,
							     JOIN_NEXT_NW)
					    != E_OK) {
						return E_REQUEST_DENIED;
					}
				}
			}
		}
			
		saved = row->string[start];
		bcopy(&row->string[start + 1], &row->string[start],
		      (size_t) (end - start + 1));
		row->string[end] = '\0';
		row->length--;
		if (row->length > 0)
			row->expanded = _formi_tab_expanded_length(
				row->string, 0, row->length - 1);
		else
			row->expanded = 0;

		  /*
		   * recalculate tabs for a single line field, multiline
		   * fields will do this when the field is wrapped.
		   */
		if ((cur->rows + cur->nrows) == 1)
			_formi_calculate_tabs(row);
		  /*
		   * if we are at the end of the string then back the
		   * cursor pos up one to stick on the end of the line
		   */
		if (start == row->length) {
			if (row->length > 1) {
				if ((cur->rows + cur->nrows) == 1) {
					pos = cur->row_xpos + cur->start_char;
					cur->start_char =
						tab_fit_window(
							cur,
							cur->start_char + cur->row_xpos,
							cur->cols);
					cur->row_xpos = pos - cur->start_char
						- 1;
					_formi_set_cursor_xpos(cur, FALSE);
				} else {
					if (cur->row_xpos == 0) {
						if (row->next != NULL) {
							if (_formi_join_line(
								cur, &row,
								JOIN_PREV_NW)
							    != E_OK) {
								return E_REQUEST_DENIED;
							}
						} else {
							if (cur->row_count > 1)
								cur->row_count--;
						}

					}
					
					cur->row_xpos = start - 1;
					cur->cursor_xpos =
						_formi_tab_expanded_length(
							row->string,
							0, cur->row_xpos - 1);
					if ((cur->cursor_xpos > 0)
					    && (start != (row->expanded - 1)))
						cur->cursor_xpos--;
				}
				
				start--;
			} else {
				start = 0;
				cur->row_xpos = 0;
				_formi_init_field_xpos(cur);
			}
		}
		
		if ((cur->rows + cur->nrows) > 1) {
			if (_formi_wrap_field(cur, row) != E_OK) {
				bcopy(&row->string[start],
				      &row->string[start + 1],
				      (size_t) (end - start));
				row->length++;
				row->string[start] = saved;
				_formi_wrap_field(cur, row);
				return E_REQUEST_DENIED;
			}
		}
		break;
			
	case REQ_DEL_PREV:
		if ((cur->cursor_xpos == 0) && (cur->start_char == 0)
		    && (cur->start_line->prev == NULL)
		    && (cur->cursor_ypos == 0))
			   return E_REQUEST_DENIED;

		row = cur->cur_line;
		start = cur->row_xpos + cur->start_char;
		end = row->length - 1;
		eat_char = TRUE;

		if ((cur->start_char + cur->row_xpos) == 0) {
			if (row->prev == NULL)
				return E_REQUEST_DENIED;

			  /*
			   * If we are a multiline field then check if
			   * the line above has a hard return.  If it does
			   * then just "eat" the hard return and re-wrap
			   * the field.
			   */
			if (row->prev->hard_ret == TRUE) {
				row->prev->hard_ret = FALSE;
				if (_formi_join_line(cur, &row,
						     JOIN_PREV) != E_OK) {
					row->prev->hard_ret = TRUE;
					return 0;
				}

				eat_char = FALSE;
			} else {
				start = row->prev->length;
				  /*
				   * Join this line to the previous
				   * one.
				   */
				if (_formi_join_line(cur, &row,
						     JOIN_PREV_NW) != E_OK) {
					return 0;
				}
				end = row->length - 1;
			}
		}

		if (eat_char == TRUE) {
			  /*
			   * eat a char from the buffer.  Normally we do
			   * this unless we have deleted a "hard return"
			   * in which case we just want to join the lines
			   * without losing a char.
			   */
			saved = row->string[start - 1];
			bcopy(&row->string[start], &row->string[start - 1],
			      (size_t) (end - start + 1));
			row->length--;
			row->string[row->length] = '\0';
			row->expanded = _formi_tab_expanded_length(
				row->string, 0, row->length - 1);
		}

		if ((cur->rows + cur->nrows) == 1) {
			_formi_calculate_tabs(row);
			pos = cur->row_xpos + cur->start_char;
			if (pos > 0)
				pos--;
			cur->start_char =
				tab_fit_window(cur,
					       cur->start_char + cur->row_xpos,
					       cur->cols);
			cur->row_xpos = pos - cur->start_char;
			_formi_set_cursor_xpos(cur, FALSE);
		} else {
			if (eat_char == TRUE) {
				cur->row_xpos--;
				if (cur->row_xpos > 0)
					cur->cursor_xpos =
						_formi_tab_expanded_length(
							row->string, 0,
							cur->row_xpos - 1);
				else
					cur->cursor_xpos = 0;
			}
			
			if ((_formi_wrap_field(cur, row) != E_OK)) {
				bcopy(&row->string[start - 1],
				      &row->string[start],
				      (size_t) (end - start));
				row->length++;
				row->string[start - 1] = saved;
				row->string[row->length] = '\0';
				_formi_wrap_field(cur, row);
				return E_REQUEST_DENIED;
			}
		}
		break;
			
	case REQ_DEL_LINE:
		if (((cur->rows + cur->nrows) == 1) ||
		    (cur->row_count == 1)) {
			  /* single line case */
			row->length = 0;
			row->expanded = row->length = 0;
			cur->row_xpos = 0;
			_formi_init_field_xpos(cur);
			cur->cursor_ypos = 0;
		} else {
			  /* multiline field */
			old_count = cur->row_count;
			cur->row_count--;
			if (cur->row_count == 0)
				cur->row_count = 1;

			if (old_count == 1) {
				row->expanded = row->length = 0;
				cur->cursor_xpos = 0;
				cur->row_xpos = 0;
				cur->cursor_ypos = 0;
			} else
				add_to_free(cur, row);
			
			if (row->next == NULL) {
				if (cur->cursor_ypos == 0) {
					if (cur->start_line->prev != NULL) {
						cur->start_line =
							cur->start_line->prev;
					}
				} else {
					cur->cursor_ypos--;
				}
			}

			if (old_count > 1) {
				if (cur->cursor_xpos > row->expanded) {
					cur->cursor_xpos = row->expanded - 1;
					cur->row_xpos = row->length - 1;
				}

				cur->start_line = cur->alines;
				rs = cur->start_line;
				cur->cursor_ypos = 0;
				while (rs != row) {
					if (cur->cursor_ypos < cur->rows)
						cur->cursor_ypos++;
					else
						cur->start_line =
							cur->start_line->next;
					rs = rs->next;
				}
			} 
		}
		break;

	case REQ_DEL_WORD:
		start = cur->start_char + cur->row_xpos;
		str = row->string;

		wb = find_eow(cur, start, TRUE, &row);
		if (wb < 0)
			return wb;

		end = wb;

		  /*
		   * If not at the start of a word then find the start,
		   * we cannot blindly call find_sow because this will
		   * skip back a word if we are already at the start of
		   * a word.
		   */
		if ((start > 0)
		    && !(isblank((unsigned char)str[start - 1]) &&
			!isblank((unsigned char)str[start])))
			start = find_sow(start, &row);
		str = row->string;
		  /* XXXX hmmmm what if start and end on diff rows? XXXX */
		bcopy(&str[end], &str[start],
		      (size_t) (row->length - end + 1));
		len = end - start;
		row->length -= len;

		if ((cur->rows + cur->nrows) > 1) {
			row = cur->start_line + cur->cursor_ypos;
			if (row->next != NULL) {
				/*
				 * if not on the last row we need to
				 * join on the next row so the line
				 * will be re-wrapped.
				 */
				_formi_join_line(cur, &row, JOIN_NEXT_NW);
			}
			_formi_wrap_field(cur, row);
			cur->row_xpos = start;
			cur->cursor_xpos = _formi_tab_expanded_length(
				row->string, 0, cur->row_xpos);
			if (cur->cursor_xpos > 0)
				cur->cursor_xpos--;
		} else {
			_formi_calculate_tabs(row);
			cur->row_xpos = start - cur->start_char;
			if (cur->row_xpos > 0)
				cur->row_xpos--;
			_formi_set_cursor_xpos(cur, FALSE);
		}
		break;
		
	case REQ_CLR_EOL:
		row->string[cur->row_xpos + 1] = '\0';
		row->length = cur->row_xpos + 1;
		row->expanded = cur->cursor_xpos + 1;
		break;

	case REQ_CLR_EOF:
		row = cur->cur_line->next;
		while (row != NULL) {
			rs = row->next;
			add_to_free(cur, row);
			row = rs;
			cur->row_count--;
		}
		break;
		
	case REQ_CLR_FIELD:
		row = cur->alines->next;
		cur->cur_line = cur->alines;
		cur->start_line = cur->alines;
		
		while (row != NULL) {
			rs = row->next;
			add_to_free(cur, row);
			row = rs;
		}

		cur->alines->string[0] = '\0';
		cur->alines->length = 0;
		cur->alines->expanded = 0;
		cur->row_count = 1;
		cur->cursor_ypos = 0;
		cur->row_xpos = 0;
		_formi_init_field_xpos(cur);
		cur->start_char = 0;
		break;
		
	case REQ_OVL_MODE:
		cur->overlay = 1;
		break;
		
	case REQ_INS_MODE:
		cur->overlay = 0;
		break;
		
	case REQ_SCR_FLINE:
		_formi_scroll_fwd(cur, 1);
		break;
		
	case REQ_SCR_BLINE:
		_formi_scroll_back(cur, 1);
		break;
		
	case REQ_SCR_FPAGE:
		_formi_scroll_fwd(cur, cur->rows);
		break;
		
	case REQ_SCR_BPAGE:
		_formi_scroll_back(cur, cur->rows);
		break;
		
	case REQ_SCR_FHPAGE:
		_formi_scroll_fwd(cur, cur->rows / 2);
		break;
		
	case REQ_SCR_BHPAGE:
		_formi_scroll_back(cur, cur->rows / 2);
		break;
		
	case REQ_SCR_FCHAR:
		_formi_hscroll_fwd(cur, row, 1);
		break;
		
	case REQ_SCR_BCHAR:
		_formi_hscroll_back(cur, row, 1);
		break;
		
	case REQ_SCR_HFLINE:
		_formi_hscroll_fwd(cur, row, cur->cols);
		break;
		
	case REQ_SCR_HBLINE:
		_formi_hscroll_back(cur, row, cur->cols);
		break;
		
	case REQ_SCR_HFHALF:
		_formi_hscroll_fwd(cur, row, cur->cols / 2);
		break;
		
	case REQ_SCR_HBHALF:
		_formi_hscroll_back(cur, row, cur->cols / 2);
		break;

	default:
		return 0;
	}
	
#ifdef DEBUG
	fprintf(dbg,
	 "exit: cursor_xpos=%d, row_xpos=%d, start_char=%d, length=%d, allocated=%d\n",
		cur->cursor_xpos, cur->row_xpos, cur->start_char,
		cur->cur_line->length,	cur->cur_line->allocated);
	fprintf(dbg, "exit: start_line=%p, ypos=%d\n", cur->start_line,
		cur->cursor_ypos);
	fprintf(dbg, "exit: string=\"%s\"\n", cur->cur_line->string);
	assert ((cur->cursor_xpos < INT_MAX) && (cur->row_xpos < INT_MAX)
		&& (cur->cursor_xpos >= cur->row_xpos));
#endif
	return 1;
}

/*
 * Validate the given character by passing it to any type character
 * checking routines, if they exist.
 */
int
_formi_validate_char(FIELD *field, char c)
{
	int ret_val;
	
	if (field->type == NULL)
		return E_OK;

	ret_val = E_INVALID_FIELD;
	_formi_do_char_validation(field, field->type, c, &ret_val);

	return ret_val;
}

		
/*
 * Perform the validation of the character, invoke all field_type validation
 * routines.  If the field is ok then update ret_val to E_OK otherwise
 * ret_val is not changed.
 */
static void
_formi_do_char_validation(FIELD *field, FIELDTYPE *type, char c, int *ret_val)
{
	if ((type->flags & _TYPE_IS_LINKED) == _TYPE_IS_LINKED) {
		_formi_do_char_validation(field, type->link->next, c, ret_val);
		_formi_do_char_validation(field, type->link->prev, c, ret_val);
	} else {
		if (type->char_check == NULL)
			*ret_val = E_OK;
		else {
			if (type->char_check((int)(unsigned char) c,
					     field->args) == TRUE)
				*ret_val = E_OK;
		}
	}
}

/*
 * Validate the current field.  If the field validation returns success then
 * return E_OK otherwise return E_INVALID_FIELD.
 *
 */
int
_formi_validate_field(FORM *form)
{
	FIELD *cur;
	int ret_val, count;
	

	if ((form == NULL) || (form->fields == NULL) ||
	    (form->fields[0] == NULL))
		return E_INVALID_FIELD;

	cur = form->fields[form->cur_field];

	  /*
	   * Sync the buffer if it has been modified so the field
	   * validation routines can use it and because this is
	   * the correct behaviour according to AT&T implementation.
	   */
	if ((cur->buf0_status == TRUE)
	    && ((ret_val = _formi_sync_buffer(cur)) != E_OK))
			return ret_val;

	  /*
	   * If buffer is untouched then the string pointer may be
	   * NULL, see if this is ok or not.
	   */
	if (cur->buffers[0].string == NULL) {
		if ((cur->opts & O_NULLOK) == O_NULLOK)
			return E_OK;
		else
			return E_INVALID_FIELD;
	}
	
	count = _formi_skip_blanks(cur->buffers[0].string, 0);

	  /* check if we have a null field, depending on the nullok flag
	   * this may be acceptable or not....
	   */
	if (cur->buffers[0].string[count] == '\0') {
		if ((cur->opts & O_NULLOK) == O_NULLOK)
			return E_OK;
		else
			return E_INVALID_FIELD;
	}
	
	  /* check if an unmodified field is ok */
	if (cur->buf0_status == 0) {
		if ((cur->opts & O_PASSOK) == O_PASSOK)
			return E_OK;
		else
			return E_INVALID_FIELD;
	}

	  /* if there is no type then just accept the field */
	if (cur->type == NULL)
		return E_OK;

	ret_val = E_INVALID_FIELD;
	_formi_do_validation(cur, cur->type, &ret_val);
	
	return ret_val;
}

/*
 * Perform the validation of the field, invoke all field_type validation
 * routines.  If the field is ok then update ret_val to E_OK otherwise
 * ret_val is not changed.
 */
static void
_formi_do_validation(FIELD *field, FIELDTYPE *type, int *ret_val)
{
	if ((type->flags & _TYPE_IS_LINKED) == _TYPE_IS_LINKED) {
		_formi_do_validation(field, type->link->next, ret_val);
		_formi_do_validation(field, type->link->prev, ret_val);
	} else {
		if (type->field_check == NULL)
			*ret_val = E_OK;
		else {
			if (type->field_check(field, field_buffer(field, 0))
			    == TRUE)
				*ret_val = E_OK;
		}
	}
}

/*
 * Select the next/previous choice for the field, the driver command
 * selecting the direction will be passed in c.  Return 1 if a choice
 * selection succeeded, 0 otherwise.
 */
int
_formi_field_choice(FORM *form, int c)
{
	FIELDTYPE *type;
	FIELD *field;
	
	if ((form == NULL) || (form->fields == NULL) ||
	    (form->fields[0] == NULL) ||
	    (form->fields[form->cur_field]->type == NULL))
		return 0;
	
	field = form->fields[form->cur_field];
	type = field->type;
	
	switch (c) {
	case REQ_NEXT_CHOICE:
		if (type->next_choice == NULL)
			return 0;
		else
			return type->next_choice(field,
						 field_buffer(field, 0));

	case REQ_PREV_CHOICE:
		if (type->prev_choice == NULL)
			return 0;
		else
			return type->prev_choice(field,
						 field_buffer(field, 0));

	default: /* should never happen! */
		return 0;
	}
}

/*
 * Update the fields if they have changed.  The parameter old has the
 * previous current field as the current field may have been updated by
 * the driver.  Return 1 if the form page needs updating.
 *
 */
int
_formi_update_field(FORM *form, int old_field)
{
	int cur, i;

	cur = form->cur_field;
	
	if (old_field != cur) {
		if (!((cur >= form->page_starts[form->page].first) &&
		      (cur <= form->page_starts[form->page].last))) {
			  /* not on same page any more */
			for (i = 0; i < form->max_page; i++) {
				if ((form->page_starts[i].in_use == 1) &&
				    (form->page_starts[i].first <= cur) &&
				    (form->page_starts[i].last >= cur)) {
					form->page = i;
					return 1;
				}
			}
		}
	}

	_formi_redraw_field(form, old_field);
	_formi_redraw_field(form, form->cur_field);
	return 0;
}

/*
 * Compare function for the field sorting
 *
 */
static int
field_sort_compare(const void *one, const void *two)
{
	const FIELD *a, *b;
	int tl;
	
	  /* LINTED const castaway; we don't modify these! */	
	a = (const FIELD *) *((const FIELD **) one);
	b = (const FIELD *) *((const FIELD **) two);

	if (a == NULL)
		return 1;

	if (b == NULL)
		return -1;
	
	  /*
	   * First check the page, we want the fields sorted by page.
	   *
	   */
	if (a->page != b->page)
		return ((a->page > b->page)? 1 : -1);

	tl = _formi_top_left(a->parent, a->index, b->index);

	  /*
	   * sort fields left to right, top to bottom so the top left is
	   * the lesser value....
	   */
	return ((tl == a->index)? -1 : 1);
}
	
/*
 * Sort the fields in a form ready for driver traversal.
 */
void
_formi_sort_fields(FORM *form)
{
	FIELD **sort_area;
	int i;
	
	TAILQ_INIT(&form->sorted_fields);

	if ((sort_area = malloc(sizeof(*sort_area) * form->field_count))
	    == NULL)
		return;

	bcopy(form->fields, sort_area,
	      (size_t) (sizeof(FIELD *) * form->field_count));
	qsort(sort_area, (size_t) form->field_count, sizeof(FIELD *),
	      field_sort_compare);
	
	for (i = 0; i < form->field_count; i++)
		TAILQ_INSERT_TAIL(&form->sorted_fields, sort_area[i], glue);

	free(sort_area);
}

/*
 * Set the neighbours for all the fields in the given form.
 */
void
_formi_stitch_fields(FORM *form)
{
	int above_row, below_row, end_above, end_below, cur_row, real_end;
	FIELD *cur, *above, *below;

	  /*
	   * check if the sorted fields circle queue is empty, just
	   * return if it is.
	   */
	if (TAILQ_EMPTY(&form->sorted_fields))
		return;
	
	  /* initially nothing is above..... */
	above_row = -1;
	end_above = TRUE;
	above = NULL;

	  /* set up the first field as the current... */
	cur = TAILQ_FIRST(&form->sorted_fields);
	cur_row = cur->form_row;
	
	  /* find the first field on the next row if any */
	below = TAILQ_NEXT(cur, glue);
	below_row = -1;
	end_below = TRUE;
	real_end = TRUE;
	while (below != NULL) {
		if (below->form_row != cur_row) {
			below_row = below->form_row;
			end_below = FALSE;
			real_end = FALSE;
			break;
		}
		below = TAILQ_NEXT(below, glue);
	}

	  /* walk the sorted fields, setting the neighbour pointers */
	while (cur != NULL) {
		if (cur == TAILQ_FIRST(&form->sorted_fields))
			cur->left = NULL;
		else
			cur->left = TAILQ_PREV(cur, _formi_sort_head, glue);

		if (cur == TAILQ_LAST(&form->sorted_fields, _formi_sort_head))
			cur->right = NULL;
		else
			cur->right = TAILQ_NEXT(cur, glue);

		if (end_above == TRUE)
			cur->up = NULL;
		else {
			cur->up = above;
			above = TAILQ_NEXT(above, glue);
			if (above_row != above->form_row) {
				end_above = TRUE;
				above_row = above->form_row;
			}
		}

		if (end_below == TRUE)
			cur->down = NULL;
		else {
			cur->down = below;
			below = TAILQ_NEXT(below, glue);
			if (below == NULL) {
				end_below = TRUE;
				real_end = TRUE;
			} else if (below_row != below->form_row) {
				end_below = TRUE;
				below_row = below->form_row;
			}
		}

		cur = TAILQ_NEXT(cur, glue);
		if ((cur != NULL)
		    && (cur_row != cur->form_row)) {
			cur_row = cur->form_row;
			if (end_above == FALSE) {
				for (; above !=
				    TAILQ_FIRST(&form->sorted_fields);
				    above = TAILQ_NEXT(above, glue)) {
					if (above->form_row != above_row) {
						above_row = above->form_row;
						break;
					}
				}
			} else if (above == NULL) {
				above = TAILQ_FIRST(&form->sorted_fields);
				end_above = FALSE;
				above_row = above->form_row;
			} else
				end_above = FALSE;

			if (end_below == FALSE) {
				while (below_row == below->form_row) {
					below = TAILQ_NEXT(below, glue);
					if (below == NULL) {
						real_end = TRUE;
						end_below = TRUE;
						break;
					}
				}

				if (below != NULL)
					below_row = below->form_row;
			} else if (real_end == FALSE)
				end_below = FALSE;
			
		}
	}
}

/*
 * Calculate the length of the displayed line allowing for any tab
 * characters that need to be expanded.  We assume that the tab stops
 * are 8 characters apart.  The parameters start and end are the
 * character positions in the string str we want to get the length of,
 * the function returns the number of characters from the start
 * position to the end position that should be displayed after any
 * intervening tabs have been expanded.
 */
int
_formi_tab_expanded_length(char *str, unsigned int start, unsigned int end)
{
	int len, start_len, i;

	  /* if we have a null string then there is no length */
	if (str[0] == '\0')
		return 0;
	
	len = 0;
	start_len = 0;

	  /*
	   * preceding tabs affect the length tabs in the span, so
	   * we need to calculate the length including the stuff before
	   * start and then subtract off the unwanted bit.
	   */
	for (i = 0; i <= end; i++) {
		if (i == start) /* stash preamble length for later */
			start_len = len;

		if (str[i] == '\0')
			break;
		
		if (str[i] == '\t')
			len = len - (len % 8) + 8;
		else
			len++;
	}

#ifdef DEBUG
	if (dbg != NULL) {
		fprintf(dbg,
		    "tab_expanded: start=%d, end=%d, expanded=%d (diff=%d)\n",
			start, end, (len - start_len), (end - start));
	}
#endif
	
	return (len - start_len);
}

/*
 * Calculate the tab stops on a given line in the field and set up
 * the tabs list with the results.  We do this by scanning the line for tab
 * characters and if one is found, noting the position and the number of
 * characters to get to the next tab stop.  This information is kept to
 * make manipulating the field (scrolling and so on) easier to handle.
 */
void
_formi_calculate_tabs(_FORMI_FIELD_LINES *row)
{
	_formi_tab_t *ts = row->tabs, *old_ts = NULL, **tsp;
	int i, j;

	  /*
	   * If the line already has tabs then invalidate them by
	   * walking the list and killing the in_use flag.
	   */
	for (; ts != NULL; ts = ts->fwd)
		ts->in_use = FALSE;


	  /*
	   * Now look for tabs in the row and record the info...
	   */
	tsp = &row->tabs;
	for (i = 0, j = 0; i < row->length; i++, j++) {
		if (row->string[i] == '\t') {
			if (*tsp == NULL) {
				if ((*tsp = (_formi_tab_t *)
				     malloc(sizeof(_formi_tab_t))) == NULL)
					return;
				(*tsp)->back = old_ts;
				(*tsp)->fwd = NULL;
			}
			
			(*tsp)->in_use = TRUE;
			(*tsp)->pos = i;
			(*tsp)->size = 8 - (j % 8);
			j += (*tsp)->size - 1;
			old_ts = *tsp;
			tsp = &(*tsp)->fwd;
		}
	}
}

/*
 * Return the size of the tab padding for a tab character at the given
 * position.  Return 1 if there is not a tab char entry matching the
 * given location.
 */
static int
tab_size(_FORMI_FIELD_LINES *row, unsigned int i)
{
	_formi_tab_t *ts;

	ts = row->tabs;
	while ((ts != NULL) && (ts->pos != i))
		ts = ts->fwd;

	if (ts == NULL)
		return 1;
	else
		return ts->size;
}

/*
 * Find the character offset that corresponds to longest tab expanded
 * string that will fit into the given window.  Walk the string backwards
 * evaluating the sizes of any tabs that are in the string.  Note that
 * using this function on a multi-line window will produce undefined
 * results - it is really only required for a single row field.
 */
static int
tab_fit_window(FIELD *field, unsigned int pos, unsigned int window)
{
	int scroll_amt, i;
	_formi_tab_t *ts;
	
	  /* first find the last tab */
	ts = field->alines->tabs;

	  /*
	   * unless there are no tabs - just return the window size,
	   * if there is enough room, otherwise 0.
	   */
	if (ts == NULL) {
		if (field->alines->length < window)
			return 0;
		else
			return field->alines->length - window + 1;
	}
		
	while ((ts->fwd != NULL) && (ts->fwd->in_use == TRUE))
		ts = ts->fwd;

	  /*
	   * now walk backwards finding the first tab that is to the
	   * left of our starting pos.
	   */
	while ((ts != NULL) && (ts->in_use == TRUE) && (ts->pos > pos))
		ts = ts->back;
	
	scroll_amt = 0;
	for (i = pos; i >= 0; i--) {
		if (field->alines->string[i] == '\t') {
			assert((ts != NULL) && (ts->in_use == TRUE));
			if (ts->pos == i) {
				if ((scroll_amt + ts->size) > window) {
					break;
				}
				scroll_amt += ts->size;
				ts = ts->back;
			}
			else
				assert(ts->pos == i);
		} else {
			scroll_amt++;
			if (scroll_amt > window)
				break;
		}
	}

	return ++i;
}

/*
 * Return the position of the last character that will fit into the
 * given width after tabs have been expanded for a given row of a given
 * field.
 */
static unsigned int
tab_fit_len(_FORMI_FIELD_LINES *row, unsigned int width)
{
	unsigned int pos, len, row_pos;
	_formi_tab_t *ts;

	ts = row->tabs;
	pos = 0;
	len = 0;
	row_pos = 0;

	if (width == 0)
		return 0;

	while ((len < width) && (pos < row->length)) {
		if (row->string[pos] == '\t') {
			assert((ts != NULL) && (ts->in_use == TRUE));
			if (ts->pos == row_pos) {
				if ((len + ts->size) > width)
					break;
				len += ts->size;
				ts = ts->fwd;
			}
			else
				assert(ts->pos == row_pos);
		} else
			len++;
		pos++;
		row_pos++;
	}

	if (pos > 0)
		pos--;
	return pos;
}

/*
 * Sync the field line structures with the contents of buffer 0 for that
 * field.  We do this by walking all the line structures and concatenating
 * all the strings into one single string in buffer 0.
 */
int
_formi_sync_buffer(FIELD *field) 
{
	_FORMI_FIELD_LINES *line;
	char *nstr, *tmp;
	unsigned length;

	if (field->alines == NULL)
		return E_BAD_ARGUMENT;

	if (field->alines->string == NULL)
		return E_BAD_ARGUMENT;

	  /*
	   * init nstr up front, just in case there are no line contents,
	   * this could happen if the field just contains hard returns.
	   */
	if ((nstr = malloc(sizeof(char))) == NULL)
		return E_SYSTEM_ERROR;
	nstr[0] = '\0';
	
	line = field->alines;
	length = 1; /* allow for terminating null */
	
	while (line != NULL) {
		if (line->length != 0) {
			if ((tmp = realloc(nstr,
					   (size_t) (length + line->length)))
			    == NULL) {
				if (nstr != NULL)
					free(nstr);
				return (E_SYSTEM_ERROR);
			}

			nstr = tmp;
			strcat(nstr, line->string);
			length += line->length;
		}

		line = line->next;
	}

	if (field->buffers[0].string != NULL)
		free(field->buffers[0].string);
	field->buffers[0].allocated = length;
	field->buffers[0].length = length - 1;
	field->buffers[0].string = nstr;
	return E_OK;
}

		
			
