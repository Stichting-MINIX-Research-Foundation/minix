#	$NetBSD: Makefile,v 1.10 2012/03/21 05:38:49 matt Exp $
#

WARNS=	2

CPPFLAGS+=-I${.CURDIR}
.if defined(DEBUG_FORMS)
CFLAGS+=-gdwarf-2
CPPFLAGS+=-DDEBUG
LDFLAGS+=-g
.endif
LIB=	form

LIBDPLIBS+=	curses	${.CURDIR}/../libcurses

SRCS=	driver.c field_types.c internals.c field.c form.c post.c type_alnum.c \
        type_alpha.c type_integer.c type_numeric.c type_enum.c type_regex.c \
	type_ipv4.c type_ipv6.c
MAN=	form_cursor.3 form_data.3 form_driver.3 form_field.3 \
	form_field_attributes.3 form_field_buffer.3 form_field_info.3 \
	form_field_just.3 form_field_new.3 form_field_opts.3 \
	form_field_userptr.3 form_field_validation.3 form_fieldtype.3 \
	form_hook.3 form_new.3 form_new_page.3 form_opts.3 form_page.3 \
	form_post.3 form_userptr.3 form_win.3 forms.3
MLINKS+=	forms.3 form.3 form_page.3 current_field.3 \
		form_data.3 data_ahead.3 form_field_new.3 dup_field.3 \
		form_field_info.3 dynamic_field_info.3 \
		form_page.3 current_field.3 form_data.3 data_ahead.3 \
		form_data.3 data_behind.3 form_field_new.3 dup_field.3 \
		form_field_info.3 dynamic_field_info.3 \
		form_field_validation.3 field_arg.3 \
		form_field_attributes.3 field_back.3 \
		form_field_buffer.3 field_buffer.3 \
		form_field.3 field_count.3 \
		form_field_attributes.3 field_fore.3 \
		form_page.3 field_index.3 form_field_info.3 field_info.3 \
		form_hook.3 field_init.3 form_field_just.3 field_just.3 \
		form_field_opts.3 field_opts.3 \
		form_field_opts.3 field_opts_off.3 \
		form_field_opts.3 field_opts_on.3 \
		form_field_attributes.3 field_pad.3 \
		form_field_buffer.3 field_status.3 form_hook.3 field_term.3 \
		form_field_validation.3 field_type.3 \
		form_field_userptr.3 field_userptr.3 \
		form_field.3 form_fields.3 \
		form_hook.3 form_init.3 form_page.3 form_max_page.3 \
		form_opts.3 form_opts_off.3 \
		form_opts.3 form_opts_on.3 \
		form_win.3 form_sub.3 form_hook.3 form_term.3 \
		form_field_new.3 free_field.3 \
		form_fieldtype.3 free_fieldtype.3 \
		form_new.3 free_form.3 form_field_new.3 link_field.3 \
		form_fieldtype.3 link_fieldtype.3 form_field.3 move_field.3 \
		form_field_new.3 new_field.3 \
		form_fieldtype.3 new_fieldtype.3 \
		form_new.3 new_form.3 form_new_page.3 new_page.3 \
		form_cursor.3 pos_form_cursor.3 form_post.3 post_form.3 \
		form_win.3 scale_form.3 form_page.3 set_current_field.3 \
		form_field_attributes.3 set_field_back.3 \
		form_field_buffer.3 set_field_buffer.3 \
		form_field_attributes.3 set_field_fore.3 \
		form_hook.3 set_field_init.3 \
		form_field_just.3 set_field_just.3 \
		form_field_opts.3 set_field_opts.3 \
		form_field_attributes.3 set_field_pad.3 \
		form_field_buffer.3 set_field_printf.3 \
		form_field_buffer.3 set_field_status.3 \
		form_hook.3 set_field_term.3 \
		form_field_validation.3 set_field_type.3 \
		form_field_userptr.3 set_field_userptr.3 \
		form_fieldtype.3 set_fieldtype_arg.3 \
		form_fieldtype.3 set_fieldtype_choice.3 \
		form_field.3 set_form_fields.3 \
		form_hook.3 set_form_init.3 form_opts.3 set_form_opts.3 \
		form_page.3 set_form_page.3 form_win.3 set_form_sub.3 \
		form_hook.3 set_form_term.3 form_userptr.3 set_form_userptr.3 \
		form_win.3 set_form_win.3 form_field_buffer.3 set_max_field.3 \
		form_new_page.3 set_new_page.3 form_post.3 unpost_form.3

INCS=	form.h
INCSDIR=/usr/include

.include <bsd.lib.mk>
.include <bsd.subdir.mk>
