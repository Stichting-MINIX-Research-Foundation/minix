/*	$NetBSD: menu.h,v 1.13 2004/03/22 19:01:09 jdc Exp $	*/

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

#ifndef	_MENU_H_
#define	_MENU_H_

#include <curses.h>
#include <eti.h>

/* requests for the menu_driver call */
#define REQ_BASE_NUM      (KEY_MAX + 0x200)
#define REQ_LEFT_ITEM     (KEY_MAX + 0x201)
#define REQ_RIGHT_ITEM    (KEY_MAX + 0x202)
#define REQ_UP_ITEM       (KEY_MAX + 0x203)
#define REQ_DOWN_ITEM     (KEY_MAX + 0x204)
#define REQ_SCR_ULINE     (KEY_MAX + 0x205)
#define REQ_SCR_DLINE     (KEY_MAX + 0x206)
#define REQ_SCR_DPAGE     (KEY_MAX + 0x207)
#define REQ_SCR_UPAGE     (KEY_MAX + 0x208)
#define REQ_FIRST_ITEM    (KEY_MAX + 0x209)
#define REQ_LAST_ITEM     (KEY_MAX + 0x20a)
#define REQ_NEXT_ITEM     (KEY_MAX + 0x20b)
#define REQ_PREV_ITEM     (KEY_MAX + 0x20c)
#define REQ_TOGGLE_ITEM   (KEY_MAX + 0x20d)
#define REQ_CLEAR_PATTERN (KEY_MAX + 0x20e)
#define REQ_BACK_PATTERN  (KEY_MAX + 0x20f)
#define REQ_NEXT_MATCH    (KEY_MAX + 0x210)
#define REQ_PREV_MATCH    (KEY_MAX + 0x211)

#define MAX_COMMAND       (KEY_MAX + 0x211) /* last menu driver request
					       - for application defined
					       commands */

/* Menu options */
typedef unsigned int OPTIONS;

/* and the values they can have */
#define O_ONEVALUE   (0x1)
#define O_SHOWDESC   (0x2)
#define O_ROWMAJOR   (0x4)
#define O_IGNORECASE (0x8)
#define O_SHOWMATCH  (0x10)
#define O_NONCYCLIC  (0x20)
#define O_SELECTABLE (0x40)
#define O_RADIO      (0x80)

typedef struct __menu_str {
        char *string;
        int length;
} MENU_STR;

typedef struct __menu MENU;
typedef struct __item ITEM;

typedef void (*Menu_Hook) (MENU *);

struct __item {
        MENU_STR name;
        MENU_STR description;
        char *userptr;
        int visible;  /* set if item is visible */
        int selected; /* set if item has been selected */
	int row; /* menu row this item is on */
	int col; /* menu column this item is on */
        OPTIONS opts;
        MENU *parent; /* menu this item is bound to */
	int index; /* index number for this item, if attached */
	  /* The following are the item's neighbours - makes menu
	     navigation easier */
	ITEM *left;
	ITEM *right;
	ITEM *up;
	ITEM *down;
};

struct __menu {
        int rows; /* max number of rows to be displayed */
        int cols; /* max number of columns to be displayed */
	int item_rows; /* number of item rows we have */
	int item_cols; /* number of item columns we have */
        int cur_row; /* current cursor row */
        int cur_col; /* current cursor column */
        MENU_STR mark; /* menu mark string */
        MENU_STR unmark; /* menu unmark string */
        OPTIONS opts; /* options for the menu */
        char *pattern; /* the pattern buffer */
	int plen;  /* pattern buffer length */
	int match_len; /* length of pattern matched */
        int posted; /* set if menu is posted */
        attr_t fore; /* menu foreground */
        attr_t back; /* menu background */
        attr_t grey; /* greyed out (nonselectable) menu item */
        int pad;  /* filler char between name and description */
        char *userptr;
	int top_row; /* the row that is at the top of the menu */
	int max_item_width; /* widest item */
	int col_width; /* width of the menu columns - this is not always
			  the same as the widest item */
        int item_count; /* number of items attached */
        ITEM **items; /* items associated with this menu */
        int  cur_item; /* item cursor is currently positioned at */
        int in_init; /* set when processing an init or term function call */
        Menu_Hook menu_init; /* call this when menu is posted */
        Menu_Hook menu_term; /* call this when menu is unposted */
        Menu_Hook item_init; /* call this when menu posted & after
				       current item changes */
        Menu_Hook item_term; /* call this when menu unposted & just
				       before current item changes */
        WINDOW *menu_win; /* the menu window */
        WINDOW *menu_subwin; /* the menu subwindow */
	WINDOW *scrwin; /* the window to write to */
};


/* Public function prototypes. */
__BEGIN_DECLS
int  menu_driver(MENU *, int);
int scale_menu(MENU *, int *, int *);
int set_top_row(MENU *, int);
int pos_menu_cursor(MENU *);
int top_row(MENU *);

int  free_menu(MENU *);
char menu_back(MENU *);
char menu_fore(MENU *);
void menu_format(MENU *, int *, int *);
char menu_grey(MENU *);
Menu_Hook menu_init(MENU *);
char *menu_mark(MENU *);
OPTIONS menu_opts(MENU *);
int menu_opts_off(MENU *, OPTIONS);
int menu_opts_on(MENU *, OPTIONS);
int menu_pad(MENU *);
char *menu_pattern(MENU *);
WINDOW *menu_sub(MENU *);
Menu_Hook menu_term(MENU *);
char *menu_unmark (MENU *);
char *menu_userptr(MENU *);
WINDOW *menu_win(MENU *);
MENU *new_menu(ITEM **);
int post_menu(MENU *);
int set_menu_back(MENU *, attr_t);
int set_menu_fore(MENU *, attr_t);
int set_menu_format(MENU *, int, int);
int set_menu_grey(MENU *, attr_t);
int set_menu_init(MENU *, Menu_Hook);
int set_menu_items(MENU *, ITEM **);
int set_menu_mark(MENU *, char *);
int set_menu_opts(MENU *, OPTIONS);
int set_menu_pad(MENU *, int);
int set_menu_pattern(MENU *, char *);
int set_menu_sub(MENU *, WINDOW *);
int set_menu_term(MENU *, Menu_Hook);
int set_menu_unmark(MENU *, char *);
int set_menu_userptr(MENU *, char *);
int  set_menu_win(MENU *, WINDOW *);
int unpost_menu(MENU *);

ITEM *current_item(MENU *);
int free_item(ITEM *);
int item_count(MENU *);
char *item_description(ITEM *);
int item_index(ITEM *);
Menu_Hook item_init(MENU *);
char *item_name(ITEM *);
OPTIONS item_opts(ITEM *);
int item_opts_off(ITEM *, OPTIONS);
int item_opts_on(ITEM *, OPTIONS);
int item_selected(MENU *, int **); /* return the item index of selected */
Menu_Hook item_term(MENU *);
char *item_userptr(ITEM *);
int item_value(ITEM *);
int item_visible(ITEM *);
ITEM **menu_items(MENU *);
ITEM *new_item(char *, char *);
int set_current_item(MENU *, ITEM *);
int set_item_init(MENU *, Menu_Hook);
int set_item_opts(ITEM *, OPTIONS);
int set_item_term(MENU *, Menu_Hook);
int set_item_userptr(ITEM *, char *);
int set_item_value(ITEM *, int);

__END_DECLS

#endif /* !_MENU_H_ */
