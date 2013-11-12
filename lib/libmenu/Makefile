#	$NetBSD: Makefile,v 1.9 2012/06/27 22:18:07 jdf Exp $
#

CPPFLAGS+=-I${.CURDIR}
.if defined(DEBUG_MENUS)
CFLAGS+=-g
CPPFLAGS+=-DDEBUG
LDFLAGS+=-g
.endif
LIB=	menu

LIBDPLIBS+=	curses	${.CURDIR}/../libcurses

SRCS=	menu.c item.c userptr.c internals.c driver.c post.c attributes.c
MAN=	menu_attributes.3 menu_item_name.3 menu_items.3 menu_userptr.3 \
	menu_cursor.3 menu_item_new.3 menu_mark.3 menu_win.3 menu_driver.3 \
	menu_item_opts.3 menu_new.3 menus.3 menu_format.3 menu_item_userptr.3 \
	menu_opts.3 menu_hook.3 menu_item_value.3 menu_pattern.3 \
	menu_item_current.3 menu_item_visible.3 menu_post.3
MLINKS+=	menu_attributes.3 menu_back.3 \
		menu_attributes.3 menu_fore.3 \
		menu_attributes.3 menu_grey.3 \
		menu_attributes.3 menu_pad.3 \
		menu_attributes.3 set_menu_back.3 \
		menu_attributes.3 set_menu_fore.3 \
		menu_attributes.3 set_menu_grey.3 \
		menu_attributes.3 set_menu_pad.3 \
		menu_item_name.3 item_description.3 \
		menu_item_name.3 item_name.3 \
		menu_items.3 item_count.3 \
		menu_items.3 set_menu_items.3 \
		menu_userptr.3 set_menu_userptr.3 \
		menu_cursor.3 pos_menu_cursor.3 \
		menu_item_new.3 free_item.3 \
		menu_item_new.3 new_item.3 \
		menu_mark.3 menu_unmark.3 \
		menu_mark.3 set_menu_mark.3 \
		menu_mark.3 set_menu_unmark.3 \
		menu_win.3 menu_sub.3 \
		menu_win.3 scale_menu.3 \
		menu_win.3 set_menu_sub.3 \
		menu_win.3 set_menu_win.3 \
		menu_item_opts.3 item_opts.3 \
		menu_item_opts.3 item_opts_off.3 \
		menu_item_opts.3 item_opts_on.3 \
		menu_new.3 new_menu.3 \
		menu_new.3 free_menu.3 \
		menu_format.3 set_menu_format.3 \
		menu_item_userptr.3 item_userptr.3 \
		menu_item_userptr.3 set_item_userptr.3 \
		menu_opts.3 menu_opts_off.3 \
		menu_opts.3 menu_opts_on.3 \
		menu_opts.3 set_menu_opts.3 \
		menu_hook.3 item_init.3 \
		menu_hook.3 item_term.3 \
		menu_hook.3 menu_init.3 \
		menu_hook.3 menu_term.3 \
		menu_hook.3 set_item_init.3 \
		menu_hook.3 set_item_term.3 \
		menu_hook.3 set_menu_init.3 \
		menu_hook.3 set_menu_term.3 \
		menu_item_value.3 item_value.3 \
		menu_item_value.3 set_item_value.3 \
		menu_item_value.3 item_selected.3 \
		menu_item_current.3 current_item.3 \
		menu_item_current.3 item_index.3 \
		menu_item_current.3 set_current_item.3 \
		menu_item_current.3 set_top_row.3 \
		menu_item_current.3 top_row.3 \
		menu_item_visible.3 item_visible.3 \
		menu_post.3 post_menu.3 \
		menu_post.3 unpost_menu.3
INCS=	menu.h eti.h
INCSDIR=/usr/include

.include <bsd.lib.mk>
.include <bsd.subdir.mk>
