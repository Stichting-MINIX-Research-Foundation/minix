/*	$NetBSD: main.c,v 1.6 2004/09/17 18:16:44 wrstuden Exp $	*/

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

/* main sysinst program. */

#include "menu_defs.h"
#include "msg_defs.h"

int main(void);

int main(void)
{

	/* Menu processing */
	process_menu (MENU_root, NULL);
	
	return 0;
}
	
/* Dynamic menu suff! */

char  ent_text[5][50] = {"name: ", "strt: ", "city: ", "opt 4", "NUM: "};

/* opt processing routines .. */

int opt_1 (struct menudesc *m, void *p);

int opt_1 (struct menudesc *m, void *p)
{
	msg_clear();
	msg_prompt (MSG_name, "", &ent_text[0][6], 40);
	msg_clear();
	return 0;
}

int opt_2 (struct menudesc *m, void *p);

int opt_2 (struct menudesc *m, void *p)
{
	msg_clear();
	msg_prompt (MSG_street, "", &ent_text[1][6], 40);
	msg_clear();
	return 0;
}

int opt_3 (struct menudesc *m, void *p);

int opt_3 (struct menudesc *m, void *p)
{
	msg_clear();
	msg_prompt (MSG_city, "", &ent_text[2][6], 40);
	msg_clear();
	return 0;
}


menu_ent  mymenu [5] = {
		{ ent_text[0], OPT_NOMENU, 0, opt_1},
		{ ent_text[1], OPT_NOMENU, 0, opt_2},
		{ ent_text[2], OPT_NOMENU, 0, opt_3},
		{ ent_text[3], OPT_NOMENU, 0, NULL},
		{ ent_text[4], OPT_NOMENU, 0, NULL} };

int num = 0;


void do_dynamic(void);
void dyn_disp (struct menudesc *, void *);
void dyn_disp (struct menudesc *m, void *p)
{
    sprintf (&ent_text[4][5], "%d", num++);
}

void do_dynamic(void)
{
	int menu_no;

	num = 0;
	menu_no = new_menu ("  A test dynamic menu! ", mymenu, 5, 10, 10,
		0, 55, MC_SCROLL, dyn_disp, NULL, NULL,
		"Make sure you try at least one option before exiting.\n"
		"Then look at what changes.\n", "Done now!");
	if (menu_no < 0) {
		endwin();
		(void) fprintf (stderr, "Dynamic memu creation failure. \n");
		exit (1);
	}
	process_menu (menu_no, NULL);
	free_menu (menu_no);
}

