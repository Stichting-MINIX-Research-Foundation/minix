/*	$NetBSD: extern.h,v 1.1.1.1 2008/05/18 14:31:22 aymeric Exp $ */

/* Do not edit: automatically built by build/distrib. */
int gtk_vi_init __P((GtkVi **, int, char*[]));
void gtk_vi_quit __P((GtkViWindow*, gint));
void gtk_vi_show_term __P((GtkViWindow*, gint));
void gtk_vi_key_press_event __P((GtkViWindow*, GdkEventKey*));
