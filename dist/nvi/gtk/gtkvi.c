/*	$NetBSD: gtkvi.c,v 1.1.1.2 2008/05/18 14:31:22 aymeric Exp $ */

/*-
 * Copyright (c) 1999
 *	Sven Verdoolaege.  All rights reserved.
 * Copyright (c) 1996
 *	Rob Zimmermann.  All rights reserved.
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "../common/common.h"
#include "../ipc/ip.h"

#if 0
#include <zvt/zvtterm.h>
#include <zvt/vt.h>
#endif
#include "gtkvi.h"
#include "gtkviscreen.h"
#include "gtkviwindow.h"
#include "extern.h"

static int vi_key_press_event __P((GtkWidget*, GdkEventKey*, GtkVi*));



#if 0
static int
vi_fork(ipvi)
	IPVI	*ipvi;
{
	GtkVi* vi = (GtkVi*)(ipvi->private_data);

	return zvt_term_forkpty(ZVT_TERM(vi->term), 0);
}
#endif

/* 
 * PUBLIC: int gtk_vi_init __P((GtkVi **, int, char*[]));
 */
int
gtk_vi_init(GtkVi **vip, int argc, char **argv)
{
	GtkVi	*vi;
	GtkWidget *term;

	MALLOC_GOTO(NULL, vi, GtkVi*, sizeof(GtkVi));
	memset(vi, 0, sizeof(GtkVi));

#if 0
	term = zvt_term_new();
	gtk_widget_show(term);
	vi->term = term;
	vt_parse_vt(&ZVT_TERM(term)->vx->vt, "test\n", 5);
#endif
	/* doesn't work now; need to know when other process is running
	gtk_signal_connect(GTK_OBJECT(term), "key_press_event",
	    (GtkSignalFunc) vi_key_press_event, vi);
	*/

/*
	vi_create(&vi->ipvi, IP_EX_ALLOWED);
*/
	vi_create(&vi->ipvi, 0);
	vi->ipvi->private_data = vi;

	/* Run vi: the parent returns, the child is the vi process. */
	vi->ipvi->run(vi->ipvi, argc, argv);

	*vip = vi;

	return 0;

alloc_err:
	return 1;
}

#if 0
static int
vi_key_press_event(zvt, event, vi)
	GtkVi *vi;
	GtkWidget *zvt;
	GdkEventKey *event;
{
	gtk_vi_key_press_event(vi, event);

	gtk_signal_emit_stop_by_name(GTK_OBJECT(zvt), "key_press_event");
	/* handled */
	return 1;
}
#endif
