/*	$NetBSD: post.c,v 1.12 2003/04/19 12:52:39 blymn Exp $	*/

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
__RCSID("$NetBSD: post.c,v 1.12 2003/04/19 12:52:39 blymn Exp $");

#include <menu.h>
#include <stdlib.h>
#include "internals.h"

/*
 * Post the menu to the screen.  Call any defined init routines and then
 * draw the menu on the screen.
 */
int
post_menu(MENU *menu)
{
	int maxx, maxy, i;
	
	if (menu == NULL)
		return E_BAD_ARGUMENT;
	if (menu->posted == 1)
		return E_POSTED;
	if (menu->in_init == 1)
		return E_BAD_STATE;
	if (menu->items == NULL)
		return E_NOT_CONNECTED;
	if (*menu->items == NULL)
		return E_NOT_CONNECTED;

	menu->in_init = 1;
	
	if (menu->menu_init != NULL)
		menu->menu_init(menu);
	if (menu->item_init != NULL)
		menu->item_init(menu);

	menu->in_init = 0;

	getmaxyx(menu->scrwin, maxy, maxx);
	if ((maxx == ERR) || (maxy == ERR)) return E_SYSTEM_ERROR;

	if ((menu->cols * menu->max_item_width + menu->cols - 1) > maxx)
		return E_NO_ROOM;

	if ((menu->opts & O_RADIO) != O_RADIO) {
		for (i = 0; i < menu->item_count; i++) {
			menu->items[i]->selected = 0;
		}
	}
	
	menu->posted = 1;
	return _menui_draw_menu(menu);
	
}

/*
 * Unpost the menu.  Call any defined termination routines and remove the
 * menu from the screen.
 */
int
unpost_menu(MENU *menu)
{
	if (menu == NULL)
		return E_BAD_ARGUMENT;
	if (menu->posted != 1)
		return E_NOT_POSTED;
	if (menu->in_init == 1)
		return E_BAD_STATE;
	if (menu->item_term != NULL)
		menu->item_term(menu);

	if (menu->menu_term != NULL)
		menu->menu_term(menu);

	menu->posted = 0;
	werase(menu->scrwin);
	wrefresh(menu->scrwin);
	return E_OK;
}

	
