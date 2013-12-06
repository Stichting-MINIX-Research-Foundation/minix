/*	$NetBSD: menu.c,v 1.18 2012/12/30 12:27:09 blymn Exp $	*/

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
__RCSID("$NetBSD: menu.c,v 1.18 2012/12/30 12:27:09 blymn Exp $");

#include <ctype.h>
#include <menu.h>
#include <string.h>
#include <stdlib.h>
#include "internals.h"

MENU _menui_default_menu = {
	16,         /* number of item rows that will fit in window */
        1,          /* number of columns of items that will fit in window */
	0,          /* number of rows of items we have */
	0,          /* number of columns of items we have */
        0,          /* current cursor row */
        0,          /* current cursor column */
        {NULL, 0},  /* mark string */
        {NULL, 0},  /* unmark string */
        O_ONEVALUE, /* menu options */
        NULL,       /* the pattern buffer */
	0,          /* length of pattern buffer */
	0,          /* the length of matched buffer */
        0,          /* is the menu posted? */
        A_REVERSE, /* menu foreground */
        A_NORMAL,   /* menu background */
        A_UNDERLINE,      /* unselectable menu item */
        ' ',        /* filler between name and description */
        NULL,       /* user defined pointer */
	0,          /* top row of menu */
	0,          /* widest item in the menu */
	0,          /* the width of a menu column */
	0,          /* number of items attached to the menu */
        NULL,       /* items in the menu */
        0,          /* current menu item */
	0,          /* currently in a hook function */
        NULL,       /* function called when menu posted */
        NULL,       /* function called when menu is unposted */
        NULL,       /* function called when current item changes */
        NULL,       /* function called when current item changes */
        NULL,       /* the menu window */
	NULL,       /* the menu subwindow */
	NULL,       /* the window to write to */
};


	
/*
 * Set the menu mark character
 */
int
set_menu_mark(MENU *m, char *mark)
{
	MENU *menu = m;
	
	if (m == NULL) menu = &_menui_default_menu;
	
          /* if there was an old mark string, free it first */
        if (menu->mark.string != NULL) free(menu->mark.string);

        if ((menu->mark.string = (char *) malloc(strlen(mark) + 1)) == NULL)
                return E_SYSTEM_ERROR;

        strcpy(menu->mark.string, mark);
	menu->mark.length = strlen(mark);

	  /* max item size may have changed - recalculate. */
	_menui_max_item_size(menu);
        return E_OK;
}

/*
 * Return the menu mark string for the menu.
 */
char *
menu_mark(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.mark.string;
	else
		return menu->mark.string;
}

/*
 * Set the menu unmark character
 */
int
set_menu_unmark(MENU *m, char *mark)
{
	MENU *menu = m;

	if (m == NULL) menu = &_menui_default_menu;
	
          /* if there was an old mark string, free it first */
        if (menu->unmark.string != NULL) free(menu->unmark.string);

        if ((menu->unmark.string = (char *) malloc(strlen(mark) + 1)) == NULL)
                return E_SYSTEM_ERROR;

        strcpy(menu->unmark.string, mark);
	menu->unmark.length = strlen(mark);
	  /* max item size may have changed - recalculate. */
	_menui_max_item_size(menu);
        return E_OK;
}

/*
 * Return the menu unmark string for the menu.
 */
char *
menu_unmark(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.unmark.string;
	else
		return menu->unmark.string;
}

/*
 * Set the menu window to the window passed.
 */
int
set_menu_win(MENU *menu, WINDOW *win)
{
	if (menu == NULL) {
		_menui_default_menu.menu_win = win;
		_menui_default_menu.scrwin = win;
	} else {
		if (menu->posted == TRUE) {
			return E_POSTED;
		} else {
			menu->menu_win = win;
			menu->scrwin = win;
		}
	}
	
        return E_OK;
}

/*
 * Return the pointer to the menu window
 */
WINDOW *
menu_win(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.menu_win;
	else
		return menu->menu_win;
}

/*
 * Set the menu subwindow for the menu.
 */
int
set_menu_sub(MENU *menu, WINDOW *sub)
{
	if (menu == NULL) {
		_menui_default_menu.menu_subwin = sub;
		_menui_default_menu.scrwin = sub;
	} else {
		if (menu->posted == TRUE)
			return E_POSTED;
		
		menu->menu_subwin = sub;
		menu->scrwin = sub;
	}
	
        return E_OK;
}

/*
 * Return the subwindow pointer for the menu
 */
WINDOW *
menu_sub(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.menu_subwin;
	else
		return menu->menu_subwin;
}

/*
 * Set the maximum number of rows and columns of items that may be displayed.
 */
int
set_menu_format(MENU *param_menu, int rows, int cols)
{
	MENU *menu = (param_menu != NULL) ? param_menu : &_menui_default_menu;
	
        menu->rows = rows;
        menu->cols = cols;

	if (menu->items != NULL)
		  /* recalculate the item neighbours */
		return _menui_stitch_items(menu);

	return E_OK;
}

/*
 * Return the max number of rows and cols that may be displayed.
 */
void
menu_format(MENU *param_menu, int *rows, int *cols)
{
	MENU *menu = (param_menu != NULL) ? param_menu : &_menui_default_menu;

        *rows = menu->rows;
        *cols = menu->cols;
}

/*
 * Set the user defined function to call when a menu is posted.
 */
int
set_menu_init(MENU *menu, Menu_Hook func)
{
	if (menu == NULL)
		_menui_default_menu.menu_init = func;
	else
		menu->menu_init = func;
        return E_OK;
}

/*
 * Return the pointer to the menu init function.
 */
Menu_Hook
menu_init(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.menu_init;
	else
		return menu->menu_init;
}

/*
 * Set the user defined function called when a menu is unposted.
 */
int
set_menu_term(MENU *menu, Menu_Hook func)
{
	if (menu == NULL)
		_menui_default_menu.menu_term = func;
	else
		menu->menu_term = func;
        return E_OK;
}

/*
 * Return the user defined menu termination function pointer.
 */
Menu_Hook
menu_term(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.menu_term;
	else
		return menu->menu_term;
}

/*
 * Return the current menu options set.
 */
OPTIONS
menu_opts(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.opts;
	else
		return menu->opts;
}

/*
 * Set the menu options to the given options.
 */
int
set_menu_opts(MENU *param_menu, OPTIONS opts)
{
	int i, seen;
	MENU *menu = (param_menu != NULL) ? param_menu : &_menui_default_menu;
	OPTIONS old_opts = menu->opts;
	
        menu->opts = opts;

	  /*
	   * If the radio option is selected then make sure only one
	   * item is actually selected in the items.
	   */
	if (((opts & O_RADIO) == O_RADIO) && (menu->items != NULL) &&
	    (menu->items[0] != NULL)) {
		seen = 0;
		for (i = 0; i < menu->item_count; i++) {
			if (menu->items[i]->selected == 1) {
				if (seen == 0) {
					seen = 1;
				} else {
					menu->items[i]->selected = 0;
				}
			}
		}

		  /* if none selected, select the first item */
		if (seen == 0)
			menu->items[0]->selected = 1;
	}

 	if ((menu->opts & O_ROWMAJOR) != (old_opts &  O_ROWMAJOR))
		  /* changed menu layout - need to recalc neighbours */
		_menui_stitch_items(menu);
	
        return E_OK;
}

/*
 * Turn on the options in menu given by opts.
 */
int
menu_opts_on(MENU *param_menu, OPTIONS opts)
{
	int i, seen;
	MENU *menu = (param_menu != NULL) ? param_menu : &_menui_default_menu;
	OPTIONS old_opts = menu->opts;

        menu->opts |= opts;

	  /*
	   * If the radio option is selected then make sure only one
	   * item is actually selected in the items.
	   */
	if (((opts & O_RADIO) == O_RADIO) && (menu->items != NULL) &&
	    (menu->items[0] != NULL)) {
		seen = 0;
		for (i = 0; i < menu->item_count; i++) {
			if (menu->items[i]->selected == 1) {
				if (seen == 0) {
					seen = 1;
				} else {
					menu->items[i]->selected = 0;
				}
			}
		}
		  /* if none selected then select the top item */
		if (seen == 0)
			menu->items[0]->selected = 1;
	}

	if ((menu->items != NULL) &&
	    (menu->opts & O_ROWMAJOR) != (old_opts &  O_ROWMAJOR))
		  /* changed menu layout - need to recalc neighbours */
		_menui_stitch_items(menu);
	
        return E_OK;
}

/*
 * Turn off the menu options given in opts.
 */
int
menu_opts_off(MENU *param_menu, OPTIONS opts)
{
	MENU *menu = (param_menu != NULL) ? param_menu : &_menui_default_menu;
	OPTIONS old_opts = menu->opts;

        menu->opts &= ~(opts);
	
	if ((menu->items != NULL ) &&
	    (menu->opts & O_ROWMAJOR) != (old_opts &  O_ROWMAJOR))
		  /* changed menu layout - need to recalc neighbours */
		_menui_stitch_items(menu);
	
        return E_OK;
}

/*
 * Return the menu pattern buffer.
 */
char *
menu_pattern(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.pattern;
	else
		return menu->pattern;
}

/*
 * Set the menu pattern buffer to pat and attempt to match the pattern in
 * the item list.
 */
int
set_menu_pattern(MENU *param_menu, char *pat)
{
	MENU *menu = (param_menu != NULL) ? param_menu : &_menui_default_menu;
	char *p = pat;
	
	  /* check pattern is all printable characters */
	while (*p)
		if (!isprint((unsigned char) *p++)) return E_BAD_ARGUMENT;
	
        if ((menu->pattern = (char *) realloc(menu->pattern,
                                     sizeof(char) * strlen(pat) + 1)) == NULL)
                return E_SYSTEM_ERROR;

        strcpy(menu->pattern, pat);
	menu->plen = strlen(pat);
	
          /* search item list for pat here */
	return _menui_match_items(menu, MATCH_FORWARD, &menu->cur_item);
}

/*
 * Allocate a new menu structure and fill it in.
 */
MENU *
new_menu(ITEM **items)
{
        MENU *the_menu;
        char mark[2];

        if ((the_menu = (MENU *)malloc(sizeof(MENU))) == NULL)
                return NULL;

          /* copy the defaults */
	(void)memcpy(the_menu, &_menui_default_menu, sizeof(MENU));

	  /* set a default window if none already set. */
	if (the_menu->menu_win == NULL)
		the_menu->scrwin = stdscr;

	  /* make a private copy of the mark string */
	if (_menui_default_menu.mark.string != NULL) {
		if ((the_menu->mark.string =
		     (char *) malloc((unsigned) _menui_default_menu.mark.length + 1))
		    == NULL) {
			free(the_menu);
			return NULL;
		}

		strlcpy(the_menu->mark.string, _menui_default_menu.mark.string,
			(unsigned) _menui_default_menu.mark.length + 1);
	}
	
	  /* make a private copy of the unmark string too */
	if (_menui_default_menu.unmark.string != NULL) {
		if ((the_menu->unmark.string =
		     (char *) malloc((unsigned) _menui_default_menu.unmark.length + 1))
		    == NULL) {
			free(the_menu);
			return NULL;
		}

		strlcpy(the_menu->unmark.string,
			_menui_default_menu.unmark.string,
			(unsigned) _menui_default_menu.unmark.length+ 1 );
	}

	/* default mark needs to be set */
	mark[0] = '-';
	mark[1] = '\0';

	set_menu_mark(the_menu, mark);

          /* now attach the items, if any */
        if (items != NULL) {
		if(set_menu_items(the_menu, items) < 0) {
			if (the_menu->mark.string != NULL)
				free(the_menu->mark.string);
			if (the_menu->unmark.string != NULL)
				free(the_menu->unmark.string);
			free(the_menu);
			return NULL;
		}
	}
	
	return the_menu;
}

/*
 * Free up storage allocated to the menu object and destroy it.
 */
int
free_menu(MENU *menu)
{
	int i;

	if (menu == NULL)
		return E_BAD_ARGUMENT;
	
	if (menu->posted != 0)
		return E_POSTED;
	
	if (menu->pattern != NULL)
		free(menu->pattern);

	if (menu->mark.string != NULL)
		free(menu->mark.string);

	if (menu->items != NULL) {
		  /* disconnect the items from this menu */
		for (i = 0; i < menu->item_count; i++) {
			menu->items[i]->parent = NULL;
		}
	}
	
	free(menu);
	return E_OK;
}

/*
 * Calculate the minimum window size for the menu.
 */
int
scale_menu(MENU *param_menu, int *rows, int *cols)
{
	MENU *menu = (param_menu != NULL) ? param_menu : &_menui_default_menu;
	
	if (menu->items == NULL)
		return E_BAD_ARGUMENT;

	  /* calculate the max item size */
	_menui_max_item_size(menu);

	*rows = menu->rows;
	*cols = menu->cols * menu->max_item_width;

	  /*
	   * allow for spacing between columns...
	   */
	*cols += (menu->cols - 1);
	
	return E_OK;
}

/*
 * Set the menu item list to the one given.
 */
int
set_menu_items(MENU *param_menu, ITEM **items)
{
	MENU *menu = (param_menu != NULL) ? param_menu : &_menui_default_menu;
	int i, new_count = 0, sel_count = 0;
	
	  /* don't change if menu is posted */
	if (menu->posted == 1)
		return E_POSTED;

	  /* count the new items and validate none are connected already */
	while (items[new_count] != NULL) {
		if ((items[new_count]->parent != NULL) &&
		    (items[new_count]->parent != menu))
			return E_CONNECTED;
		if (items[new_count]->selected == 1)
			sel_count++;
		new_count++;
	}

	  /*
	   * don't allow multiple selected items if menu is radio
	   * button style.
	   */
	if (((menu->opts & O_RADIO) == O_RADIO) &&
	    (sel_count > 1))
		return E_BAD_ARGUMENT;
	
	  /* if there were items connected then disconnect them. */
	if (menu->items != NULL) {
		for (i = 0; i < menu->item_count; i++) {
			menu->items[i]->parent = NULL;
			menu->items[i]->index = -1;
		}
	}

	menu->item_count = new_count;

	  /* connect the new items to the menu */
	for (i = 0; i < new_count; i++) {
		items[i]->parent = menu;
		items[i]->index = i;
	}

	menu->items = items;
	menu->cur_item = 0; /* reset current item just in case */
	menu->top_row = 0; /* and the top row too */
	if (menu->pattern != NULL) { /* and the pattern buffer....sigh */
		free(menu->pattern);
		menu->plen = 0;
		menu->match_len = 0;
	}
	
	  /*
	   * make sure at least one item is selected on a radio
	   * button style menu.
	   */
	if (((menu->opts & O_RADIO) == O_RADIO) && (sel_count == 0))
		menu->items[0]->selected = 1;
	
	
	_menui_stitch_items(menu); /* recalculate the item neighbours */
	
	return E_OK;
}

/*
 * Return the pointer to the menu items array.
 */
ITEM **
menu_items(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.items;
	else
		return menu->items;
}

/*
 * Return the count of items connected to the menu
 */
int
item_count(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.item_count;
	else
		return menu->item_count;
}

/*
 * Set the menu top row to be the given row.  The current item becomes the
 * leftmost item on that row in the menu.
 */
int
set_top_row(MENU *param_menu, int row)
{
	MENU *menu = (param_menu != NULL) ? param_menu : &_menui_default_menu;
	int i, cur_item, state = E_SYSTEM_ERROR;
	
	if (row > menu->item_rows)
		return E_BAD_ARGUMENT;

	if (menu->items == NULL)
		return E_NOT_CONNECTED;

	if (menu->in_init == 1)
		return E_BAD_STATE;

	cur_item = 0;
	
	for (i = 0; i < menu->item_count; i++) {
		  /* search for first item that matches row - this will be
		     the current item. */
		if (row == menu->items[i]->row) {
			cur_item = i;
			state = E_OK;
			break; /* found what we want - no need to go further */
		}
	}

	menu->in_init = 1; /* just in case we call the init/term routines */
	
	if (menu->posted == 1) {
		if (menu->menu_term != NULL)
			menu->menu_term(menu);
		if (menu->item_term != NULL)
			menu->item_term(menu);
	}

	menu->cur_item = cur_item;
	menu->top_row = row;

	if (menu->posted == 1) {
		if (menu->menu_init != NULL)
			menu->menu_init(menu);
		if (menu->item_init != NULL)
			menu->item_init(menu);
	}

	menu->in_init = 0;
		
	  /* this should always be E_OK unless we are really screwed up */
	return state;
}

/*
 * Return the current top row number.
 */
int
top_row(MENU *param_menu)
{
	MENU *menu = (param_menu != NULL) ? param_menu : &_menui_default_menu;

	if (menu->items == NULL)
		return E_NOT_CONNECTED;
	
	return menu->top_row;
}

/*
 * Position the cursor at the correct place in the menu.
 *
 */
int
pos_menu_cursor(MENU *menu)
{
	int movx, maxmark;
	
	if (menu == NULL)
		return E_BAD_ARGUMENT;

	maxmark = max(menu->mark.length, menu->unmark.length);
	movx = maxmark + (menu->items[menu->cur_item]->col
		* (menu->col_width + 1));
	
	if (menu->match_len > 0)
		movx += menu->match_len - 1;
	
	wmove(menu->scrwin,
	      menu->items[menu->cur_item]->row - menu->top_row, movx);

	return E_OK;
}
