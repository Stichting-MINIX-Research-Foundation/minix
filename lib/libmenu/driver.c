/*	$NetBSD: driver.c,v 1.9 2003/03/09 01:08:48 lukem Exp $	*/

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
__RCSID("$NetBSD: driver.c,v 1.9 2003/03/09 01:08:48 lukem Exp $");

#include <menu.h>
#include <ctype.h>
#include <stdlib.h>
#include "internals.h"

/*
 * The guts of the menu library.  This function processes the character
 * in c and performs actions based on the value of the character.  If the
 * character is a normal one then the driver attempts to match the character
 * against the items.  If the character is a recognised request then the
 * request is processed by the driver, if the character is not a recognised
 * request and is not printable then it assumed to be a user defined command.
 */
int
menu_driver(MENU *menu, int c)
{
	int drv_top_row, drv_scroll, i, it, status = E_OK;
	ITEM *drv_new_item;

	i = 0;
	
	if (menu == NULL)
		return E_BAD_ARGUMENT;
	if (menu->posted == 0)
		return E_NOT_POSTED;
	if (menu->items == NULL)
		return E_NOT_CONNECTED;
	if (*menu->items == NULL)
		return E_NOT_CONNECTED;
	if (menu->in_init == 1)
		return E_BAD_STATE;

	  /* this one should never happen but just in case.... */
	if (menu->items[menu->cur_item] == NULL)
		return E_SYSTEM_ERROR;

	drv_new_item = menu->items[menu->cur_item];
	it = menu->cur_item;
	drv_top_row = menu->top_row;
	
	if ((c > REQ_BASE_NUM) && (c <= MAX_COMMAND)) {
		  /* is a known driver request  - first check if the pattern
		   * buffer needs to be cleared, we do this on non-search
		   * type requests.
		   */
		if (! ((c == REQ_BACK_PATTERN) || (c == REQ_NEXT_MATCH) ||
		       (c == REQ_PREV_MATCH))) {
			if ((c == REQ_CLEAR_PATTERN)
			    && (menu->pattern == NULL))
				return E_REQUEST_DENIED;
			free(menu->pattern);
			menu->pattern = NULL;
			menu->plen = 0;
			menu->match_len = 0;
		}
		
		switch (c) {
		  case REQ_LEFT_ITEM:
			  drv_new_item = drv_new_item->left;
			  break;
		  case REQ_RIGHT_ITEM:
			  drv_new_item = drv_new_item->right;
			  break;
		  case REQ_UP_ITEM:
			  drv_new_item = drv_new_item->up;
			  break;
		  case REQ_DOWN_ITEM:
			  drv_new_item = drv_new_item->down;
			  break;
		  case REQ_SCR_ULINE:
			  if (drv_top_row == 0)
				  return E_REQUEST_DENIED;
			  drv_top_row--;
			  drv_new_item = drv_new_item->up;
			  break;
		  case REQ_SCR_DLINE:
			  drv_top_row++;
			  if ((drv_top_row + menu->rows - 1)> menu->item_rows)
				  return E_REQUEST_DENIED;
			  drv_new_item = drv_new_item->down;
			  break;
		  case REQ_SCR_DPAGE:
			  drv_scroll = menu->item_rows - menu->rows
				  - menu->top_row;
			  if (drv_scroll > menu->rows) {
				  drv_scroll = menu->rows;
			  }
			  
			  if (drv_scroll <= 0) {
				  return E_REQUEST_DENIED;
			  } else {
				  drv_top_row += drv_scroll;
				  while (drv_scroll-- > 0)
					  drv_new_item = drv_new_item->down;
			  }
			  break;
		  case REQ_SCR_UPAGE:
			  if (menu->rows < menu->top_row) {
				  drv_scroll = menu->rows;
			  } else {
				  drv_scroll = menu->top_row;
			  }
			  if (drv_scroll == 0)
				  return E_REQUEST_DENIED;

			  drv_top_row -= drv_scroll;
			  while (drv_scroll-- > 0)
				  drv_new_item = drv_new_item->up;
			  break;
		  case REQ_FIRST_ITEM:
			  drv_new_item = menu->items[0];
			  break;
		  case REQ_LAST_ITEM:
			  drv_new_item = menu->items[menu->item_count - 1];
			  break;
		  case REQ_NEXT_ITEM:
			  if ((menu->cur_item + 1) >= menu->item_count) {
				  if ((menu->opts & O_NONCYCLIC)
				      == O_NONCYCLIC) {
					  return E_REQUEST_DENIED;
				  } else {
					  drv_new_item = menu->items[0];
				  }
			  } else {
				  drv_new_item =
					  menu->items[menu->cur_item + 1];
			  }
			  break;
		  case REQ_PREV_ITEM:
			  if (menu->cur_item == 0) {
				  if ((menu->opts & O_NONCYCLIC)
				      == O_NONCYCLIC) {
					  return E_REQUEST_DENIED;
				  } else {
					  drv_new_item = menu->items[
						  menu->item_count - 1];
				  }
			  } else {
				  drv_new_item =
					  menu->items[menu->cur_item - 1];
			  }
			  break;
		  case REQ_TOGGLE_ITEM:
			  if ((menu->opts & (O_RADIO | O_ONEVALUE)) != 0) {
			      if ((menu->opts & O_RADIO) == O_RADIO) {
				  if ((drv_new_item->opts & O_SELECTABLE)
							!= O_SELECTABLE)
					  return E_NOT_SELECTABLE;

				    /* don't deselect selected item */
				  if (drv_new_item->selected == 1)
					  return E_REQUEST_DENIED;
				  
				  /* deselect all items */
			          for (i = 0; i < menu->item_count; i++) {
				      if ((menu->items[i]->selected) &&
					  (drv_new_item->index != i)) {
				          menu->items[i]->selected ^= 1;
					  _menui_draw_item(menu,
						menu->items[i]->index);
				      }
				  }

				    /* turn on selected item */
				  drv_new_item->selected ^= 1;
				  _menui_draw_item(menu, drv_new_item->index);
			      } else {
			      	  return E_REQUEST_DENIED;
			      }
			  } else {
				  if ((drv_new_item->opts
				       & O_SELECTABLE) == O_SELECTABLE) {
					    /* toggle select flag */
					  drv_new_item->selected ^= 1;
					    /* update item in menu */
					  _menui_draw_item(menu,
						drv_new_item->index);
				  } else {
					  return E_NOT_SELECTABLE;
				  }
			  }
			  break;
		  case REQ_CLEAR_PATTERN:
			    /* this action is taken before the
			       case statement */
			  break;
		  case REQ_BACK_PATTERN:
			  if (menu->pattern == NULL)
				  return E_REQUEST_DENIED;
			  
			  if (menu->plen == 0)
				  return E_REQUEST_DENIED;
			  menu->pattern[menu->plen--] = '\0';
			  break;
		  case REQ_NEXT_MATCH:
			  if (menu->pattern == NULL)
				  return E_REQUEST_DENIED;

			  status = _menui_match_pattern(menu, 0,
							 MATCH_NEXT_FORWARD,
							 &it);
			  drv_new_item = menu->items[it];
			  break;
		  case REQ_PREV_MATCH:
			  if (menu->pattern == NULL)
				  return E_REQUEST_DENIED;

			  status = _menui_match_pattern(menu, 0,
							 MATCH_NEXT_REVERSE,
							 &it);
			  drv_new_item = menu->items[it];
			  break; 
		}
	} else if (c > MAX_COMMAND) {
		  /* must be a user command */
		return E_UNKNOWN_COMMAND;
	} else if (isprint((unsigned char) c)) {
		  /* otherwise search items for the character. */
		status = _menui_match_pattern(menu, (unsigned char) c,
					       MATCH_FORWARD, &it);
		drv_new_item = menu->items[it];

		  /* update the position of the cursor if we are doing
		   * show match and the current item has not changed.  If
		   * we don't do this here it won't get done since the
		   * display will not be updated due to the current item
		   * not changing.
		   */
		if ((drv_new_item->index == menu->cur_item)
		    && ((menu->opts & O_SHOWMATCH) == O_SHOWMATCH)) {
			pos_menu_cursor(menu);
		}
		
			    
	} else {
		  /* bad character */
		return E_BAD_ARGUMENT;
	}

	if (drv_new_item == NULL)
		return E_REQUEST_DENIED;

	if (drv_new_item->row < drv_top_row) drv_top_row = drv_new_item->row;
	if (drv_new_item->row >= (drv_top_row + menu->rows))
		drv_top_row = drv_new_item->row - menu->rows + 1;
	
	if ((drv_new_item->index != menu->cur_item)
	    || (drv_top_row != menu->top_row))
		_menui_goto_item(menu, drv_new_item, drv_top_row);

	return status;
}

		
	
		
	
