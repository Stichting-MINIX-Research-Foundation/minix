#	$NetBSD: Makefile,v 1.68 2012/04/21 14:39:35 roy Exp $
#	@(#)Makefile	8.2 (Berkeley) 1/2/94

.include <bsd.own.mk>

WARNS=	2

CPPFLAGS+=-I${.CURDIR} -I${NETBSDSRCDIR}/lib/libterminfo
.if defined(DEBUG_CURSES)
CPPFLAGS+=-g -DDEBUG
.endif
.if defined(SMALL)
CPPFLAGS+=-DSMALL
.endif
LIB=	curses
SRCS=	acs.c addbytes.c addch.c addchnstr.c addnstr.c attributes.c \
	background.c bell.c border.c box.c chgat.c clear.c clearok.c \
	clrtobot.c clrtoeol.c color.c copywin.c cr_put.c \
	ctrace.c cur_hash.c curs_set.c \
	curses.c delch.c deleteln.c delwin.c echochar.c erase.c fileio.c \
	flushok.c fullname.c getch.c getstr.c getyx.c id_subwins.c idlok.c \
	idcok.c inch.c inchstr.c initscr.c insch.c insdelln.c insertln.c \
	instr.c keypad.c keyname.c leaveok.c line.c meta.c move.c \
	mvwin.c newwin.c nodelay.c notimeout.c overlay.c overwrite.c pause.c \
	printw.c putchar.c refresh.c resize.c scanw.c screen.c scroll.c \
	scrollok.c setterm.c standout.c timeout.c toucholap.c touchwin.c \
	tstp.c tty.c unctrl.c underscore.c

MAN=	curses.3 curses_addch.3 curses_addchstr.3 curses_addstr.3 \
	curses_attributes.3 curses_background.3 curses_border.3 \
	curses_chgat.3 curses_clear.3 curses_color.3 \
	curses_cursor.3 curses_default_colors.3 \
	curses_delch.3 curses_deleteln.3 curses_echochar.3 curses_fileio.3 \
	curses_inch.3 curses_input.3 curses_insertch.3 curses_insertln.3 \
	curses_insdelln.3 curses_keyname.3 curses_line.3 curses_pad.3 \
	curses_print.3 curses_refresh.3 curses_scanw.3 curses_screen.3 \
	curses_scroll.3 curses_standout.3 curses_termcap.3 curses_touch.3 \
	curses_tty.3 curses_underscore.3 curses_window.3
INCS=	curses.h unctrl.h
INCSDIR=/usr/include

LIBDPLIBS+=	terminfo	${.CURDIR}/../libterminfo

.if !defined(DISABLE_WCHAR)
CPPFLAGS+=-DHAVE_WCHAR
SRCS+= cchar.c add_wch.c add_wchstr.c addwstr.c echo_wchar.c ins_wch.c \
	insstr.c ins_wstr.c get_wch.c get_wstr.c in_wch.c in_wchstr.c \
	inwstr.c
.else
CPPFLAGS+=-DDISABLE_WCHAR
.endif

MLINKS+= curses_addch.3 addch.3 curses_addchstr.3 addchnstr.3 \
	 curses_addchstr.3 addchstr.3 curses_addstr.3 addnstr.3 \
	 curses_addstr.3 addstr.3 \
	 curses_default_colors.3 assume_default_colors.3 \
	 curses_attributes.3 attr_get.3 curses_attributes.3 attr_off.3 \
	 curses_attributes.3 attr_on.3 curses_attributes.3 attr_set.3 \
	 curses_attributes.3 attron.3 curses_attributes.3 attroff.3 \
	 curses_attributes.3 attrset.3 curses_tty.3 beep.3 \
	 curses_background.3 bkgd.3 curses_background.3 bkgdset.3 \
	 curses_border.3 border.3 curses_border.3 box.3 \
	 curses_chgat.3 chgat.3 curses_chgat.3 mvchgat.3 \
	 curses_chgat.3 mvwchgat.3 curses_chgat.3 wchgat.3 \
	 curses_color.3 can_change_color.3 curses_tty.3 cbreak.3 \
	 curses_clear.3 clear.3 curses_clear.3 clearok.3 \
	 curses_clear.3 clrtobot.3 curses_clear.3 clrtoeol.3 \
	 curses_color.3 color_content.3 curses_attributes.3 color_set.3 \
	 curses_window.3 copywin.3 curses_tty.3 curs_set.3 \
	 curses_input.3 define_key.3 \
	 curses_tty.3 delay_output.3  curses_tty.3 def_prog_mode.3 \
	 curses_tty.3 def_shell_mode.3 curses_delch.3 delch.3 \
	 curses_deleteln.3 deleteln.3 curses_screen.3 delscreen.3 \
	 curses_window.3 delwin.3 curses_window.3 derwin.3 \
	 curses_refresh.3 doupdate.3 curses_window.3 dupwin.3 \
	 curses_tty.3 echo.3 curses_echochar.3 echochar.3 \
	 curses_screen.3 endwin.3 curses_clear.3 erase.3 \
	 curses_tty.3 erasechar.3 \
	 curses_tty.3 flash.3 curses_tty.3 flushinp.3 \
	 curses_refresh.3 flushok.3 \
	 curses_termcap.3 fullname.3 curses_attributes.3 getattrs.3  \
	 curses_background.3 getbkgd.3 \
	 curses_input.3 getch.3 curses_cursor.3 getcury.3 \
	 curses_cursor.3 getcurx.3 curses_cursor.3 getbegy.3 \
	 curses_cursor.3 getbegx.3 curses_cursor.3 getmaxx.3 \
	 curses_cursor.3 getmaxy.3 curses_cursor.3 getmaxyx.3 \
	 curses_input.3 getnstr.3 \
	 curses_cursor.3 getpary.3 curses_cursor.3 getparx.3 \
	 curses_cursor.3 getparyx.3 curses_fileio.3 getwin.3 \
	 curses_input.3 getstr.3 \
	 curses_tty.3 gettmode.3 curses_cursor.3 getyx.3 \
	 curses_color.3 has_colors.3 curses_tty.3 has_ic.3 \
	 curses_tty.3 halfdelay.3 curses_tty.3 has_il.3 \
	 curses_line.3 hline.3 curses_tty.3 idcok.3 \
	 curses_tty.3 idlok.3 curses_inch.3 inch.3 \
	 curses_inch.3 inchnstr.3 curses_inch.3 inchstr.3 \
	 curses_inch.3 innstr.3 curses_color.3 init_color.3 \
	 curses_color.3 init_pair.3 curses_screen.3 initscr.3 \
	 curses_insertch.3 insch.3 curses_insdelln.3 insdelln.3 \
	 curses_insertln.3 insertln.3 curses_inch.3 instr.3 \
	 curses_tty.3 intrflush.3 curses_touch.3 is_linetouched.3 \
	 curses_touch.3 is_wintouched.3 curses_screen.3 isendwin.3 \
	 curses_keyname.3 keyname.3 \
	 curses_input.3 keypad.3 curses_tty.3 killchar.3 \
	 curses_refresh.3 leaveok.3 \
	 curses_tty.3 meta.3 curses_cursor.3 move.3 \
	 curses_addch.3 mvaddch.3 curses_addchstr.3 mvaddchnstr.3 \
	 curses_addchstr.3 mvaddchstr.3 curses_addstr.3 mvaddnstr.3 \
	 curses_addstr.3 mvaddstr.3 \
	 curses_cursor.3 mvcur.3 curses_window.3 mvderwin.3 \
	 curses_input.3 mvgetch.3 \
	 curses_input.3 mvgetnstr.3 curses_input.3 mvgetstr.3 \
	 curses_line.3 mvhline.3 curses_print.3 mvprintw.3 \
	 curses_line.3 mvvline.3 curses_addch.3 mvwaddch.3 \
	 curses_addchstr.3 mvwaddchnstr.3 curses_addchstr.3 mvwaddchstr.3 \
	 curses_addstr.3 mvwaddnstr.3 curses_addstr.3 mvwaddstr.3 \
	 curses_input.3 mvwgetch.3 \
	 curses_input.3 mvwgetnstr.3 curses_input.3 mvwgetstr.3 \
	 curses_line.3 mvwhline.3 curses_window.3 mvwin.3 \
	 curses_inch.3 mvinchnstr.3 \
	 curses_inch.3 mvinchstr.3 curses_inch.3 mvinnstr.3 \
	 curses_inch.3 mvinstr.3 curses_inch.3 mvwinchnstr.3 \
	 curses_inch.3 mvwinchstr.3 curses_inch.3 mvwinnstr.3 \
	 curses_inch.3 mvwinstr.3 curses_print.3 mvwprintw.3 \
	 curses_line.3 mvwvline.3 curses_tty.3 napms.3 \
	 curses_pad.3 newpad.3 curses_screen.3 newterm.3 \
	 curses_window.3 newwin.3 curses_tty.3 nl.3 \
	 curses_tty.3 nocbreak.3 curses_input.3 nodelay.3 \
	 curses_tty.3 noecho.3 curses_tty.3 nonl.3 \
	 curses_tty.3 noqiflush.3 \
	 curses_tty.3 noraw.3 curses_input.3 notimeout.3 \
	 curses_window.3 overlay.3 curses_window.3 overwrite.3 \
	 curses_color.3 pair_content.3 curses_echochar.3 pechochar.3 \
	 curses_pad.3 pnoutrefresh.3 \
	 curses_pad.3 prefresh.3 curses_print.3 printw.3 \
	 curses_fileio.3 putwin.3 curses_tty.3 qiflush.3 \
	 curses_tty.3 raw.3 curses_refresh.3 refresh.3 \
	 curses_tty.3 reset_prog_mode.3 curses_tty.3 reset_shell_mode.3 \
	 curses_tty.3 resetty.3 curses_screen.3 resizeterm.3 \
	 curses_tty.3 savetty.3 curses_scanw.3 scanw.3 \
	 curses_scroll.3 scrl.3 curses_scroll.3 scroll.3 \
	 curses_scroll.3 scrollok.3 curses_scroll.3 setscrreg.3 \
	 curses_screen.3 set_term.3 curses_screen.3 setterm.3 \
	 curses_standout.3 standend.3 curses_standout.3 standout.3 \
	 curses_color.3 start_color.3 curses_pad.3 subpad.3 \
	 curses_window.3 subwin.3 curses_input.3 timeout.3 \
	 curses_touch.3 touchline.3 curses_touch.3 touchoverlap.3 \
	 curses_touch.3 touchwin.3 curses_print.3 unctrl.3 \
	 curses_underscore.3 underend.3 curses_underscore.3 underscore.3 \
	 curses_input.3 ungetch.3 curses_touch.3 untouchwin.3 \
	 curses_default_colors.3 use_default_colors.3 curses_line.3 vline.3 \
	 curses_addch.3 waddch.3 curses_addchstr.3 waddchnstr.3 \
	 curses_addchstr.3 waddchstr.3 curses_addstr.3 waddnstr.3 \
	 curses_addstr.3 waddstr.3 \
	 curses_attributes.3 wattr_get.3 curses_attributes.3 wattr_off.3 \
	 curses_attributes.3 wattr_on.3 curses_attributes.3 wattr_set.3 \
	 curses_attributes.3 wattron.3 curses_attributes.3 wattroff.3 \
	 curses_attributes.3 wattrset.3 curses_background.3 wbkgd.3 \
	 curses_background.3 wbkgdset.3 curses_border.3 wborder.3 \
	 curses_clear.3 wclear.3 curses_clear.3 wclrtobot.3 \
	 curses_clear.3 wclrtoeol.3 curses_attributes.3 wcolor_set.3 \
	 curses_delch.3 wdelch.3 curses_deleteln.3 wdeleteln.3 \
	 curses_echochar.3 wechochar.3 \
	 curses_clear.3 werase.3 curses_input.3 wgetch.3 \
	 curses_input.3 wgetnstr.3 curses_input.3 wgetstr.3 \
	 curses_line.3 whline.3 curses_inch.3 winch.3 \
	 curses_inch.3 winchnstr.3 curses_inch.3 winchstr.3 \
	 curses_inch.3 winnstr.3 curses_insertch.3 winsch.3 \
	 curses_insdelln.3 winsdelln.3 curses_insertln.3 winsertln.3 \
	 curses_inch.3 winstr.3 curses_cursor.3 wmove.3 \
	 curses_refresh.3 wnoutrefresh.3 curses_print.3 wprintw.3 \
	 curses_refresh.3 wrefresh.3 curses_window.3 wresize.3 \
	 curses_scanw.3 wscanw.3 curses_scroll.3 wscrl.3 \
	 curses_scroll.3 wsetscrreg.3 curses_standout.3 wstandend.3 \
	 curses_standout.3 wstandout.3 curses_input.3 wtimeout.3 \
	 curses_touch.3 wtouchln.3 curses_underscore.3 wunderend.3 \
	 curses_underscore.3 wunderscore.3 curses_line.3 wvline.3

.if make(install)
SUBDIR+= PSD.doc
.endif

fileio.h: shlib_version genfileioh.awk
	${TOOL_AWK} -f ${.CURDIR}/genfileioh.awk < ${.CURDIR}/shlib_version > ${.CURDIR}/fileio.h

.include <bsd.lib.mk>
.include <bsd.subdir.mk>
