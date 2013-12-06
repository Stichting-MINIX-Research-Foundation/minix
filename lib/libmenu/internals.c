/*	$NetBSD: internals.c,v 1.17 2013/10/18 19:53:59 christos Exp $	*/

/*-
 * Copyright (c) 1998-1999 Brett Lymn (blymn@baea.com.au, brett_lymn@yahoo.com.au)
 * All rights reserved.
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
__RCSID("$NetBSD: internals.c,v 1.17 2013/10/18 19:53:59 christos Exp $");

#include <menu.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "internals.h"

/* internal function prototypes */
static void
_menui_calc_neighbours(MENU *menu, int item_no);
static void _menui_redraw_menu(MENU *menu, int old_top_row, int old_cur_item);

  /*
   * Link all the menu items together to speed up navigation.  We need
   * to calculate the widest item entry, then work out how many columns
   * of items the window will accommodate and then how many rows there will
   * be.  Once the layout is determined the neighbours of each item is
   * calculated and the item structures updated.
   */
int
_menui_stitch_items(MENU *menu)
{
	int i, row_major;

	row_major = ((menu->opts & O_ROWMAJOR) == O_ROWMAJOR);

	if (menu->posted == 1)
		return E_POSTED;
	if (menu->items == NULL)
		return E_BAD_ARGUMENT;

	menu->item_rows = menu->item_count / menu->cols;
	menu->item_cols = menu->cols;
	if (menu->item_count > (menu->item_rows * menu->item_cols))
		menu->item_rows += 1;

	_menui_max_item_size(menu);

	for (i = 0; i < menu->item_count; i++) {
		  /* fill in the row and column value of the item */
		if (row_major) {
			menu->items[i]->row = i / menu->item_cols;
			menu->items[i]->col = i % menu->item_cols;
		} else {
			menu->items[i]->row = i % menu->item_rows;
			menu->items[i]->col = i / menu->item_rows;
		}

		_menui_calc_neighbours(menu, i);
	}

	return E_OK;
}

  /*
   * Calculate the neighbours for an item in menu.
   */
static void
_menui_calc_neighbours(MENU *menu, int item_no)
{
	int neighbour, cycle, row_major, edge;
	ITEM *item;

	row_major = ((menu->opts & O_ROWMAJOR) == O_ROWMAJOR);
	cycle = ((menu->opts & O_NONCYCLIC) != O_NONCYCLIC);
	item = menu->items[item_no];

	if (menu->item_rows < 2) {
		if (cycle) {
			item->up = item;
			item->down = item;
		} else {
			item->up = NULL;
			item->down = NULL;
		}
	} else {

		/* up */
		if (menu->item_cols < 2) {
			if (item_no == 0) {
				if (cycle)
					item->up =
					    menu->items[menu->item_count - 1];
				else
					item->up = NULL;
			} else
				item->up = menu->items[item_no - 1];
		} else {
			edge = 0;
			if (row_major) {
				if (item->row == 0) {
					neighbour =
				    	(menu->item_rows - 1) * menu->item_cols
						+ item->col;
					if (neighbour >= menu->item_count)
						neighbour -= menu->item_cols;
					edge = 1;
				} else
					neighbour = item_no - menu->item_cols;
			} else {
				if (item->row == 0) {
					neighbour = menu->item_rows * item->col
						+ menu->item_rows - 1;
					if (neighbour >= menu->item_count)
						neighbour = menu->item_count - 1;
					edge = 1;
				} else
					neighbour = item_no - 1;
			}


			item->up = menu->items[neighbour];
			if ((!cycle) && (edge == 1))
				item->up = NULL;
		}

		/* Down */
		if (menu->item_cols < 2) {
			if (item_no == (menu->item_count - 1)) {
				if (cycle)
					item->down = menu->items[0];
				else
					item->down = NULL;
			} else
				item->down = menu->items[item_no + 1];
		} else {
			edge = 0;
			if (row_major) {
				if (item->row == menu->item_rows - 1) {
					neighbour = item->col;
					edge = 1;
				} else {
					neighbour = item_no + menu->item_cols;
					if (neighbour >= menu->item_count) {
						neighbour = item->col;
						edge = 1;
					}
				}
			} else {
				if (item->row == menu->item_rows - 1) {
					neighbour = item->col * menu->item_rows;
					edge = 1;
				} else {
					neighbour = item_no + 1;
					if (neighbour >= menu->item_count) {
						neighbour = item->col
						    * menu->item_rows;
						edge = 1;
					}
				}
			}

			item->down = menu->items[neighbour];
			if ((!cycle) && (edge == 1))
				item->down = NULL;
		}
	}

	if (menu->item_cols < 2) {
		if (cycle) {
			item->left = item;
			item->right = item;
		} else {
			item->left = NULL;
			item->right = NULL;
		}
	} else {
		/* left */
		if (menu->item_rows < 2) {
			if (item_no == 0) {
				if (cycle)
					item->left =
					    menu->items[menu->item_count - 1];
				else
					item->left = NULL;
			} else
				item->left = menu->items[item_no - 1];
		} else {
			edge = 0;
			if (row_major) {
				if (item->col == 0) {
					neighbour = item_no + menu->cols - 1;
					if (neighbour >= menu->item_count)
						neighbour = menu->item_count - 1;
					edge = 1;
				} else
					neighbour = item_no - 1;
			} else {
				if (item->col == 0) {
					neighbour = menu->item_rows
					    * (menu->item_cols - 1) + item->row;
					if (neighbour >= menu->item_count)
						neighbour -= menu->item_rows;
					edge = 1;
				} else
					neighbour = item_no - menu->item_rows;
			}

			item->left = menu->items[neighbour];
			if ((!cycle) && (edge == 1))
				item->left = NULL;
		}

		/* right */
		if (menu->item_rows < 2) {
			if (item_no == menu->item_count - 1) {
				if (cycle)
					item->right = menu->items[0];
				else
					item->right = NULL;
			} else
				item->right = menu->items[item_no + 1];
		} else {
			edge = 0;
			if (row_major) {
				if (item->col == menu->item_cols - 1) {
					neighbour = item_no - menu->item_cols
					    + 1;
					edge = 1;
				} else if (item_no == menu->item_count - 1) {
					neighbour = item->row * menu->item_cols;
					edge = 1;
				} else
					neighbour = item_no + 1;
			} else {
				if (item->col == menu->item_cols - 1) {
					neighbour = item->row;
					edge = 1;
				} else {
					neighbour = item_no + menu->item_rows;
					if (neighbour >= menu->item_count) {
						neighbour = item->row;
						edge = 1;
					}
				}
			}

			item->right = menu->items[neighbour];
			if ((!cycle) && (edge == 1))
				item->right = NULL;
		}
	}
}

/*
 * Goto the item pointed to by item and adjust the menu structure
 * accordingly.  Call the term and init functions if required.
 */
int
_menui_goto_item(MENU *menu, ITEM *item, int new_top_row)
{
	int old_top_row = menu->top_row, old_cur_item = menu->cur_item;

	  /* If we get a null then the menu is not cyclic so deny request */
	if (item == NULL)
		return E_REQUEST_DENIED;

	menu->in_init = 1;
	if (menu->top_row != new_top_row) {
		if ((menu->posted == 1) && (menu->menu_term != NULL))
			menu->menu_term(menu);
		menu->top_row = new_top_row;

		if ((menu->posted == 1) && (menu->menu_init != NULL))
			menu->menu_init(menu);
	}

	  /* this looks like wasted effort but it can happen.... */
	if (menu->cur_item != item->index) {

		if ((menu->posted == 1) && (menu->item_term != NULL))
			menu->item_term(menu);

		menu->cur_item = item->index;
		menu->cur_row = item->row;
		menu->cur_col = item->col;

		if (menu->posted == 1)
			_menui_redraw_menu(menu, old_top_row, old_cur_item);

		if ((menu->posted == 1) && (menu->item_init != NULL))
			menu->item_init(menu);

	}

	menu->in_init = 0;
	return E_OK;
}

/*
 * Attempt to match items with the pattern buffer in the direction given
 * by iterating over the menu items.  If a match is found return E_OK
 * otherwise return E_NO_MATCH
 */
int
_menui_match_items(MENU *menu, int direction, int *item_matched)
{
	int i, caseless;

	caseless = ((menu->opts & O_IGNORECASE) == O_IGNORECASE);

	i = menu->cur_item;
	if (direction == MATCH_NEXT_FORWARD) {
		if (++i >= menu->item_count) i = 0;
	} else if (direction == MATCH_NEXT_REVERSE) {
		if (--i < 0) i = menu->item_count - 1;
	}


	do {
		if (menu->items[i]->name.length >= menu->plen) {
			  /* no chance if pattern is longer */
			if (caseless) {
				if (strncasecmp(menu->items[i]->name.string,
						menu->pattern,
						(size_t) menu->plen) == 0) {
					*item_matched = i;
					menu->match_len = menu->plen;
					return E_OK;
				}
			} else {
				if (strncmp(menu->items[i]->name.string,
					    menu->pattern,
					    (size_t) menu->plen) == 0) {
					*item_matched = i;
					menu->match_len = menu->plen;
					return E_OK;
				}
			}
		}

		if ((direction == MATCH_FORWARD) ||
		    (direction == MATCH_NEXT_FORWARD)) {
			if (++i >= menu->item_count) i = 0;
		} else {
			if (--i <= 0) i = menu->item_count - 1;
		}
	} while (i != menu->cur_item);

	menu->match_len = 0; /* match did not succeed - kill the match len. */
	return E_NO_MATCH;
}

/*
 * Attempt to match the pattern buffer against the items.  If c is a
 * printable character then add it to the pattern buffer prior to
 * performing the match.  Direction determines the direction of matching.
 * If the match is successful update the item_matched variable with the
 * index of the item that matched the pattern.
 */
int
_menui_match_pattern(MENU *menu, int c, int direction, int *item_matched)
{
	if (menu == NULL)
		return E_BAD_ARGUMENT;
	if (menu->items == NULL)
		return E_BAD_ARGUMENT;
	if (*menu->items == NULL)
		return E_BAD_ARGUMENT;

	if (isprint(c)) {
		  /* add char to buffer - first allocate room for it */
		if ((menu->pattern = (char *)
		     realloc(menu->pattern,
			     menu->plen + sizeof(char) +
			     ((menu->plen > 0)? 0 : 1)))
		    == NULL)
			return E_SYSTEM_ERROR;
		menu->pattern[menu->plen] = c;
		menu->pattern[++menu->plen] = '\0';

		  /* there is no chance of a match if pattern is longer
		     than all the items */
		if (menu->plen >= menu->max_item_width) {
			menu->pattern[--menu->plen] = '\0';
			return E_NO_MATCH;
		}

		if (_menui_match_items(menu, direction,
					item_matched) == E_NO_MATCH) {
			menu->pattern[--menu->plen] = '\0';
			return E_NO_MATCH;
		} else
			return E_OK;
	} else {
		if (_menui_match_items(menu, direction,
					item_matched) == E_OK) {
			return E_OK;
		} else {
			return E_NO_MATCH;
		}
	}
}

/*
 * Draw an item in the subwindow complete with appropriate highlighting.
 */
void
_menui_draw_item(MENU *menu, int item)
{
	int j, pad_len, mark_len;

	mark_len = max(menu->mark.length, menu->unmark.length);

	wmove(menu->scrwin,
	      menu->items[item]->row - menu->top_row,
	      menu->items[item]->col * (menu->col_width + 1));

	if (menu->cur_item == item)
		wattrset(menu->scrwin, menu->fore);
	if ((menu->items[item]->opts & O_SELECTABLE) != O_SELECTABLE)
		wattron(menu->scrwin, menu->grey);

	  /* deal with the menu mark, if  one is set.
	   * We mark the selected items and write blanks for
	   * all others unless the menu unmark string is set in which
	   * case the unmark string is written.
	   */
	if ((menu->items[item]->selected == 1) ||
	    (((menu->opts & O_ONEVALUE) == O_ONEVALUE) &&
		(menu->cur_item == item))) {
		if (menu->mark.string != NULL) {
			for (j = 0; j < menu->mark.length; j++) {
				waddch(menu->scrwin,
				       menu->mark.string[j]);
			}
		}
		  /* blank any length difference between mark & unmark */
		for (j = menu->mark.length; j < mark_len; j++)
			waddch(menu->scrwin, ' ');
	} else {
		if (menu->unmark.string != NULL) {
			for (j = 0; j < menu->unmark.length; j++) {
				waddch(menu->scrwin,
				       menu->unmark.string[j]);
			}
		}
		  /* blank any length difference between mark & unmark */
		for (j = menu->unmark.length; j < mark_len; j++)
			waddch(menu->scrwin, ' ');
	}

	  /* add the menu name */
	for (j=0; j < menu->items[item]->name.length; j++)
		waddch(menu->scrwin,
		       menu->items[item]->name.string[j]);

	pad_len = menu->col_width - menu->items[item]->name.length
		- mark_len - 1;
	if ((menu->opts & O_SHOWDESC) == O_SHOWDESC) {
		pad_len -= menu->items[item]->description.length - 1;
		for (j = 0; j < pad_len; j++)
			waddch(menu->scrwin, menu->pad);
		for (j = 0; j < menu->items[item]->description.length; j++) {
			waddch(menu->scrwin,
			       menu->items[item]->description.string[j]);
		}
	} else {
		for (j = 0; j < pad_len; j++)
			waddch(menu->scrwin, ' ');
	}
	menu->items[item]->visible = 1;

	  /* kill any special attributes... */
	wattrset(menu->scrwin, menu->back);

	  /*
	   * Fill in the spacing between items, annoying but it looks
	   * odd if the menu items are inverse because the spacings do not
	   * have the same attributes as the items.
	   */
	if ((menu->items[item]->col > 0) &&
	    (menu->items[item]->col < (menu->item_cols - 1))) {
		wmove(menu->scrwin,
		      menu->items[item]->row - menu->top_row,
		      menu->items[item]->col * (menu->col_width + 1) - 1);
		waddch(menu->scrwin, ' ');
	}

	  /* and position the cursor nicely */
	pos_menu_cursor(menu);
}

/*
 * Draw the menu in the subwindow provided.
 */
int
_menui_draw_menu(MENU *menu)
{
	int rowmajor, i, j, k, row = -1, stride;
	int incr, cur_row, offset, row_count;

	rowmajor = ((menu->opts & O_ROWMAJOR) == O_ROWMAJOR);

	if (rowmajor) {
		stride = 1;
		incr = menu->item_cols;
	} else {
		stride = menu->item_rows;
		incr = 1;
	}
	row_count = 0;

	for (i = 0;  i < menu->item_count; i += incr) {
		if (menu->items[i]->row == menu->top_row)
			break;
		row_count++;
		for (j = 0; j < menu->item_cols; j++) {
			offset = j * stride + i;
			if (offset >= menu->item_count)
				break; /* done */
			menu->items[offset]->visible = 0;
		}
	}

	wmove(menu->scrwin, 0, 0);

	menu->col_width = getmaxx(menu->scrwin) / menu->cols;

	for (cur_row = 0; cur_row < menu->rows; cur_row++) {
		for (j = 0; j < menu->cols; j++) {
			offset = j * stride + i;
			if (offset >= menu->item_count) {
			   /* no more items to draw, write background blanks */
				wattrset(menu->scrwin, menu->back);
				if (row < 0) {
					row = menu->items[menu->item_count - 1]->row;
				}

				wmove(menu->scrwin, cur_row,
				      j * (menu->col_width + 1));
				for (k = 0; k < menu->col_width; k++)
					waddch(menu->scrwin, ' ');
			} else {
				_menui_draw_item(menu, offset);
			}
		}

		i += incr;
		row_count++;
	}

	if (row_count < menu->item_rows) {
		for (cur_row = row_count;  cur_row < menu->item_rows; cur_row++) {
			for (j = 0; j < menu->item_cols; j++) {
				offset = j * stride + i;
				if (offset >= menu->item_count)
					break; /* done */
				menu->items[offset]->visible = 0;
			}
			i += incr;
		}
	}

	return E_OK;
}


/*
 * Calculate the widest menu item and stash it in the menu struct.
 *
 */
void
_menui_max_item_size(MENU *menu)
{
	int i, with_desc, width;

	with_desc = ((menu->opts & O_SHOWDESC) == O_SHOWDESC);

	for (i = 0; i < menu->item_count; i++) {
		width = menu->items[i]->name.length
			+ max(menu->mark.length, menu->unmark.length);
		if (with_desc)
			width += menu->items[i]->description.length + 1;

		menu->max_item_width = max(menu->max_item_width, width);
	}
}


/*
 * Redraw the menu on the screen.  If the current item has changed then
 * unhighlight the old item and highlight the new one.
 */
static void
_menui_redraw_menu(MENU *menu, int old_top_row, int old_cur_item)
{

	if (menu->top_row != old_top_row) {
		  /* top row changed - redo the whole menu
		   * XXXX this could be improved if we had wscrl implemented.

		   * XXXX we could scroll the window and just fill in the
		   * XXXX changed lines.
		   */
		wclear(menu->scrwin);
		_menui_draw_menu(menu);
	} else {
		if (menu->cur_item != old_cur_item) {
			  /* redo the old item as a normal one. */
			_menui_draw_item(menu, old_cur_item);
		}
		  /* and then redraw the current item */
		_menui_draw_item(menu, menu->cur_item);
	}
}
