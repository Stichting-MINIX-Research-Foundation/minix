/*	$NetBSD: userptr.c,v 1.9 2003/03/09 01:08:48 lukem Exp $	*/

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
__RCSID("$NetBSD: userptr.c,v 1.9 2003/03/09 01:08:48 lukem Exp $");

#include <menu.h>
#include <stdlib.h>
#include <string.h>

/* the following is defined in menu.c */
extern MENU _menui_default_menu;

/* the following is defined in item.c */
extern ITEM _menui_default_item;

/*
 * Set the item user pointer data
 */
int
set_item_userptr(ITEM *param_item, char *userptr)
{
	ITEM *item = (param_item != NULL) ? param_item : &_menui_default_item;
	
        item->userptr = userptr;
        return E_OK;
}


/*
 * Return the item user pointer
 */
char *
item_userptr(ITEM *item)
{
	if (item == NULL)
		return _menui_default_item.userptr;
	else
		return item->userptr;
}

/*
 * Return the user pointer for the given menu
 */
char *
menu_userptr(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.userptr;
	else
		return menu->userptr;
}

/*
 * Set the user pointer for the given menu
 */
int
set_menu_userptr(MENU *param_menu, char *userptr)
{
	MENU *menu = (param_menu != NULL) ? param_menu : &_menui_default_menu;

        menu->userptr = userptr;

        return E_OK;
}

