/*	$NetBSD: attributes.c,v 1.7 2003/03/09 01:08:47 lukem Exp $	*/

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
__RCSID("$NetBSD: attributes.c,v 1.7 2003/03/09 01:08:47 lukem Exp $");

#include <menu.h>

/* defined in menu.c - the default menu struct */
extern MENU _menui_default_menu;

/*
 * Set the menu foreground attribute
 */
int
set_menu_fore(MENU *menu, attr_t attr)
{
	if (menu == NULL)
		_menui_default_menu.fore = attr;
	else
		menu->fore = attr;
        return E_OK;
}

/*
 * Return the menu foreground attribute
 */
char
menu_fore(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.fore;
	else
		return menu->fore;
}

/*
 * Set the menu background attribute
 */
int
set_menu_back(MENU *menu, attr_t attr)
{
	if (menu == NULL)
		_menui_default_menu.back = attr;
	else
		menu->back = attr;
        return E_OK;
}

/*
 * Return the menu background attribute
 */
char
menu_back(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.back;
	else
		return menu->back;
}

/*
 * Set the menu greyed out attribute
 */
int
set_menu_grey(MENU *menu, attr_t attr)
{
	if (menu == NULL)
		_menui_default_menu.grey = attr;
	else
		menu->grey = attr;
        return E_OK;
}

/*
 * Return the menu greyed out attribute
 */
char
menu_grey(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.grey;
	else
		return menu->grey;
}

/*
 * Set the menu pad character - the filler char between name and description
 */
int
set_menu_pad(MENU *menu, int pad)
{
	if (menu == NULL)
		_menui_default_menu.pad = pad;
	else
		menu->pad = pad;
        return E_OK;
}

/*
 * Return the menu pad character
 */
int
menu_pad(MENU *menu)
{
	if (menu == NULL)
		return _menui_default_menu.pad;
	else
		return menu->pad;
}
