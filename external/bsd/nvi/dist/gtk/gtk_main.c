/*-
 * Copyright (c) 1999
 *	Sven Verdoolaege.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/queue.h>
#include <bitstring.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../common/common.h"
#include "../ipc/ip.h"

#include <gtk/gtk.h>
#include "gtkvi.h"
#include "gtkviwindow.h"
#include "gtkviscreen.h"
#include "gtk_extern.h"

static void vi_destroyed __P((GtkWidget*,GtkWidget*));
static void vi_rename __P((GtkWidget*,gchar*,GtkWidget*));
static void vi_quit __P((GtkViWindow*, int));

static void win_toplevel(GtkViWindow *win);
static void create_toplevel(GtkVi *vi);

static GtkItemFactoryEntry menu_items[] = {
    { "/_File",	   	NULL,	    NULL,	    0,	"<Branch>" },
    { "/File/E_xit",	NULL,	    vi_quit,	    1,  NULL },
    { "/File/_Quit",	NULL,	    vi_quit,	    0,  NULL },
    { "/_Window",   	NULL,	    NULL,	    0,	"<Branch>" },
    { "/Window/New Window",	NULL,	 win_toplevel,	0, NULL },
#if 0 /*wrong argument anyway*/
    { "/Window/Show Terminal",	NULL,    gtk_vi_show_term,    1,  NULL },
    { "/Window/Show Vi",	NULL,    gtk_vi_show_term,    0,  NULL },
#endif
};

static int n_toplevel = 0;

int
main(int argc, char **argv)
{
	GtkVi	*vi;

	gtk_set_locale ();

	gtk_init (&argc, &argv);

	gtk_vi_init(&vi, argc, argv);

	create_toplevel(vi);

	gtk_main();

	return 0;
}

static
void win_toplevel(GtkViWindow *win)
{
    create_toplevel(win->vi);
}

static 
void create_toplevel(GtkVi *vi)
{
	GtkWidget *window;
	GtkWidget *box, *menubar;
	GtkWidget *vi_window;
	gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);
	GtkItemFactory *factory;
	GtkAccelGroup *accel;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	++n_toplevel;

	box = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), box);
	gtk_widget_show(box);

	vi_window = gtk_vi_window_new(vi);

	accel = gtk_accel_group_new();
	factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel);
	gtk_item_factory_create_items (factory, nmenu_items, menu_items, (gpointer)vi_window);
	gtk_accel_group_attach(accel, GTK_OBJECT(window));
	menubar = gtk_item_factory_get_widget (factory, "<main>");
	gtk_widget_show(menubar);
	gtk_box_pack_start(GTK_BOX(box), menubar, FALSE, FALSE, 0);

	gtk_accel_group_attach(accel, GTK_OBJECT(vi_window));
	gtk_widget_show(vi_window);

	gtk_signal_connect(GTK_OBJECT(vi_window), "rename",
			   GTK_SIGNAL_FUNC(vi_rename),
			   window);
	gtk_signal_connect(GTK_OBJECT(GTK_VI_WINDOW(vi_window)->vi_screen), "destroy",
			   GTK_SIGNAL_FUNC(vi_destroyed),
			   window);
	gtk_box_pack_start(GTK_BOX(box), vi_window, TRUE, TRUE, 0);

	/*
	gtk_widget_grab_focus(GTK_WIDGET(vi->vi));
	*/

	gtk_widget_show(window);
}

static void
vi_quit(GtkViWindow *vi, gint write)
{
	gtk_vi_quit(vi, write);
}

static void
vi_destroyed(GtkWidget *vi, GtkWidget *window)
{
	gtk_widget_destroy(window);
	if (!--n_toplevel)
		gtk_main_quit();
}

static void
vi_rename(GtkWidget *vi, gchar *name, GtkWidget *window)
{
	gtk_window_set_title(GTK_WINDOW(window), name);
}
