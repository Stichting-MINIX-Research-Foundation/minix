/*	$NetBSD: menus.mc,v 1.11 2004/09/17 18:16:31 wrstuden Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Written by Philip A. Nelson for Piermont Information Systems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software develooped for the NetBSD Project by
 *      Piermont Information Systems Inc.
 * 4. The name of Piermont Information Systems Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PIERMONT INFORMATION SYSTEMS INC. ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PIERMONT INFORMATION SYSTEMS INC. BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

{

#include "msg_defs.h"

/* Initial code for definitions and includes and  prototypes. */
void do_dynamic (void);
static int msg_init = 0;

}

default x=20, y=10;

allow dynamic menus;

error action { fprintf (stderr, "Testm: Could not initialize curses.\n");
	       exit(1); };

menu root, title "  Main Menu of Test System", x=10;
	display action {
		/* Message initialization */
		if (!msg_init) {
			msg_window (stdscr);
			msg_init = 1;
		}
		msg_display (MSG_welcome);
		wrefresh(stdscr); };
	option  "Do nothing option", 
		action  { }
	;
	option  "Try a sub menu",
		sub menu  submenu
	;
	option  "A scrollable menu",
		sub menu  scrollit
	;
	option  "Another scrollable menu",
		sub menu scrollit2
	;
	option  "Big non-scrollable menu, bombs on small screens",
		sub menu bigscroll
	;
	option  "A menu with no shortcuts",
		sub menu noshort
	; 
	option  "A dynamic menu ...",
		action { do_dynamic (); }
	;
	option  "Run a shell...",
		action (endwin) { system ("/bin/sh"); }
	;
	exit action (endwin)  { printf ("Thanks for playing\n"); };
	help {
                    Main Menu Help Screen

This is help text for the main menu of the menu test system.  This
text should appear verbatim when asked for by use of the ? key by
the user.  This should allow scrolling, if needed.  If the first
character in the help is the newline (as the case for this help),
then that newline is not included in the help text.

Now this tests lines for scrolling:
10
11
12
13
14
15
16
17
18
19
20
21
22
23
24
25
26
27
28
29
30
31
32
33
34
35
36
37
38
39
40
41
42
43
44
45
46
47
48
49
50
51
52
53
54
55
56
57
58
59
60
61
62
63
64
65
66
67
68
69
70
71
72
73
74
75
76
77
78
79
80
};

menu submenu, title "  submenu test";
	option  "upper right", sub menu  upperright;
	option  "lower left", sub menu  lowerleft;
	option  "middle, no title", sub menu middle;
	option  "next menu", next menu nextmenu;

menu upperright, title "upper right", y=2, x=60, no exit;
	option  "Just Exit!", exit;

menu lowerleft, title "lower left", y=19, x=2, no exit;
	option  "Just Exit!", exit;

menu middle, no box;
	option "Just Exit!", exit;

menu nextmenu, title "  A next window! ? for comments", no exit;
	option "Just Exit!:", exit;

menu noshort, title "  No shortcut characters!", no shortcut;
	option "first", action {};
	option "second", action {};
	option "third", action {};

menu scrollit, scrollable, h=4, title "  Scrollable Menu";
	option "option 1", action {};
	option "option 2", action {};
	option "option 3", action {};
	option "option 4", action {};
	option "option 5", action {};
	option "option 6", action {};

menu bigscroll, no scrollable, title "  Non-scrollable Menu";
	option "option 1", action {};
	option "option 2", action {};
	option "option 3", action {};
	option "option 4", action {};
	option "option 5", action {};
	option "option 6", action {};
	option "option 7", action {};
	option "option 8", action {};
	option "option 9", action {};
	option "option 10", action {};
	option "option 11", action {};
	option "option 12", action {};
	option "option 13", action {};
	option "option 14", action {};
	option "option 15", action {};
	option "option 16", action {};
	option "option 17", action {};
	option "option 18", action {};
	option "option 19", action {};
	option "option 20", action {};

menu scrollit2, scrollable, title "  Big scrollable Menu";
	option "option 1", action {};
	option "option 2", action {};
	option "option 3", action {};
	option "option 4", action {};
	option "option 5", action {};
	option "option 6", action {};
	option "option 7", action {};
	option "option 8", action {};
	option "option 9", action {};
	option "option 10", action {};
	option "option 11", action {};
	option "option 12", action {};
	option "option 13", action {};
	option "option 14", action {};
	option "option 15", action {};
	option "option 16", action {};
	option "option 17", action {};
	option "option 18", action {};
	option "option 19", action {};
	option "option 20", action {};
	option "option 21", action {};
	option "option 22", action {};
	option "option 23", action {};
	option "option 24", action {};
	option "option 25", action {};
	option "option 26", action {};
	option "option 27", action {};
	option "option 28", action {};
	option "option 29", action {};
	option "option 30", action {};
	option "option 31", action {};
	option "option 32", action {};
	option "option 33", action {};
	option "option 34", action {};
	option "option 35", action {};
	option "option 36", action {};
	option "option 37", action {};
	option "option 38", action {};
	option "option 39", action {};
	option "option 40", action {};
	option "option 41", action {};
	option "option 42", action {};
	option "option 43", action {};
	option "option 44", action {};
	option "option 45", action {};
	option "option 46", action {};
	option "option 47", action {};
	option "option 48", action {};
	option "option 49", action {};
	option "option 50", action {};
	option "option 51", action {};
