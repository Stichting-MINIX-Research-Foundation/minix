/*	$NetBSD: gtkviwindow.c,v 1.1.1.2 2008/05/18 14:31:22 aymeric Exp $ */

/* change further to gtkviwindow have no knowledge of ipvi */
#include "config.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#if 0
#ifdef HAVE_ZVT
#include <zvt/zvtterm.h>
#include <zvt/vt.h>
#endif
#endif

#include "../common/common.h"
#include "../ipc/ip.h"

#include "gtkvi.h"
#include "gtkviscreen.h"
#include "gtkviwindow.h"
#include "extern.h"

enum {
    RENAME,
    LAST_SIGNAL
};

static void gtk_vi_window_class_init     (GtkViWindowClass   *klass);
static void gtk_vi_window_init           (GtkViWindow        *vi);
static void gtk_vi_window_destroy 	 (GtkObject *object);

static int vi_key_press_event __P((GtkWidget*, GdkEventKey*, GtkViWindow*));
static void vi_map __P((GtkWidget *, GtkWidget*));
static void vi_resized __P((GtkWidget *, int, int, IPVIWIN*));
static void vi_adjustment_value_changed __P((GtkAdjustment *, IPVIWIN *));

static void vi_input_func __P((gpointer , gint , GdkInputCondition));

static void vi_init_window __P((GtkViWindow *window, int));

static int vi_addstr __P((IPVIWIN*, const char *, u_int32_t));
static int vi_waddstr __P((IPVIWIN*, const CHAR_T *, u_int32_t));
static int vi_attribute __P((IPVIWIN*,u_int32_t  ,u_int32_t   ));
static int vi_bell __P((IPVIWIN*));
static int vi_busyon __P((IPVIWIN*, const char *, u_int32_t));
static int vi_busyoff __P((IPVIWIN*));
static int vi_clrtoeol __P((IPVIWIN*));
static int vi_deleteln __P((IPVIWIN*));
static int vi_discard __P((IPVIWIN*));
static int vi_editopt __P((IPVIWIN*, const char *, u_int32_t,
                            const char *, u_int32_t, u_int32_t));
static int vi_insertln __P((IPVIWIN*));
static int vi_move __P((IPVIWIN*, u_int32_t, u_int32_t));
static int vi_quit __P((IPVIWIN*));
static int vi_redraw __P((IPVIWIN*));
static int vi_refresh __P((IPVIWIN*));
static int vi_rename __P((IPVIWIN*, const char *, u_int32_t));
static int vi_rewrite __P((IPVIWIN*, u_int32_t));
static int vi_scrollbar __P((IPVIWIN*, u_int32_t, u_int32_t , u_int32_t ));
static int vi_select __P((IPVIWIN*, const char *, u_int32_t));
static int vi_split __P((IPVIWIN*));
static int vi_ex_init __P((IPVIWIN*));
static int vi_vi_init __P((IPVIWIN*));
static int vi_fork __P((IPVIWIN*));

static GtkWidgetClass *parent_class = NULL;
static guint vi_window_signals[LAST_SIGNAL] = { 0 };

GtkType
gtk_vi_window_get_type (void)
{
  static GtkType vi_window_type = 0;
  
  if (!vi_window_type)
    {
      static const GtkTypeInfo vi_window_info =
      {
	"GtkViWindow",
	sizeof (GtkViWindow),
	sizeof (GtkViWindowClass),
	(GtkClassInitFunc) gtk_vi_window_class_init,
	(GtkObjectInitFunc) gtk_vi_window_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      vi_window_type = gtk_type_unique (GTK_TYPE_NOTEBOOK, &vi_window_info);
    }
  
  return vi_window_type;
}

static void
gtk_vi_window_class_init (GtkViWindowClass *class)
{
  GtkObjectClass *object_class;
  
  object_class = (GtkObjectClass*) class;
  parent_class = gtk_type_class (GTK_TYPE_WIDGET);

  vi_window_signals[RENAME] =
    gtk_signal_new ("rename",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE(object_class),
		    GTK_SIGNAL_OFFSET (GtkViScreenClass, rename),
		    gtk_marshal_VOID__STRING,
		    GTK_TYPE_NONE, 1, GTK_TYPE_STRING, 0);

#ifndef HAVE_PANGO
  gtk_object_class_add_signals(object_class, vi_window_signals, LAST_SIGNAL);
#endif

  object_class->destroy = gtk_vi_window_destroy;
}

static void 
gtk_vi_window_init (GtkViWindow *vi)
{
}

GtkWidget *
gtk_vi_window_new (GtkVi *vi)
{
    GtkViWindow* window;
    GtkWidget *vi_widget;
    GtkWidget *vscroll;
    GtkWidget *table;
    GtkWidget *term;
    int	       fd;
#ifdef HAVE_ZVT
    int	       pty[2];
#endif

    window = gtk_type_new(gtk_vi_window_get_type());

    window->vi = vi;
    //vi->vi_window = GTK_WIDGET(window);

    vi_widget = gtk_vi_screen_new(NULL);
    gtk_widget_show(GTK_WIDGET(vi_widget));
    /*
    vi->vi = vi_widget;
    */
    window->vi_screen = vi_widget;

    vscroll = gtk_vscrollbar_new(GTK_VI_SCREEN(vi_widget)->vadj);
    gtk_widget_show(vscroll);

    table = gtk_table_new(2, 2, FALSE);
    gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(vi_widget),
	0, 1, 0, 1);
    gtk_table_attach(GTK_TABLE(table), vscroll, 1, 2, 0, 1,
	(GtkAttachOptions)0, GTK_FILL, 0, 0);
    gtk_widget_show(table);
    gtk_signal_connect(GTK_OBJECT(table), "map", GTK_SIGNAL_FUNC(vi_map), 
			vi_widget/*->ipvi*/);
    window->table = table;


    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(window), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(window), FALSE);
    gtk_notebook_append_page(GTK_NOTEBOOK(window), table, NULL);

    term = 0;
    fd = -1;

#if 0
#ifdef HAVE_ZVT
    term = zvt_term_new();
    zvt_term_set_blink(ZVT_TERM(term), FALSE);
    zvt_term_get_ptys(ZVT_TERM(term), 0, pty);
    fd = pty[1]; /* slave */
    gtk_widget_show(term);
    gtk_notebook_append_page(GTK_NOTEBOOK(window), term, NULL);
#endif
#endif
    window->term = term;

    vi_init_window(window, fd);

    gtk_signal_connect(GTK_OBJECT(vi_widget), "resized",
	GTK_SIGNAL_FUNC(vi_resized), window->ipviwin);
    gtk_signal_connect(GTK_OBJECT(vi_widget), "key_press_event",
	(GtkSignalFunc) vi_key_press_event, window);
    window->value_changed = 
	gtk_signal_connect(GTK_OBJECT(GTK_VI_SCREEN(vi_widget)->vadj), 
	    "value_changed",
	    (GtkSignalFunc) vi_adjustment_value_changed, window->ipviwin);

    return GTK_WIDGET(window);
}

static void
gtk_vi_window_destroy (GtkObject *object)
{
  GtkViWindow *vi_window;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_VI_WINDOW (object));
  
  vi_window = (GtkViWindow*) object;

  if (vi_window->table) {
    gtk_signal_disconnect_by_data(GTK_OBJECT(vi_window->table), 
				  vi_window->vi_screen);
    vi_window->table = 0;
  }

  if (vi_window->vi_screen) {
    gtk_signal_disconnect_by_data(GTK_OBJECT(vi_window->vi_screen), 
				  vi_window->ipviwin);
    gtk_signal_disconnect(GTK_OBJECT(GTK_VI_SCREEN(vi_window->vi_screen)->vadj), 
	vi_window->value_changed);
    gtk_widget_destroy(vi_window->vi_screen);
    vi_window->vi_screen = 0;
  }

  GTK_OBJECT_CLASS(parent_class)->destroy (object);
}

void
gtk_vi_window_scrollbar(GtkViWindow *vi, guint top, guint size, guint max)
{
    GtkViScreen *vi_screen;
    /* work around gcc bug */
    volatile guint mymax = max;
    volatile guint mysize = size;

    vi_screen = GTK_VI_SCREEN(vi->vi_screen);
    vi_screen->vadj->value = top;
    vi_screen->vadj->upper = mymax;
    vi_screen->vadj->page_size =
	vi_screen->vadj->page_increment = mysize;
    gtk_signal_handler_block(GTK_OBJECT(vi_screen->vadj), vi->value_changed);
    gtk_adjustment_changed(vi_screen->vadj);
    /*
    gtk_adjustment_value_changed(vi_screen->vadj);
    */
    gtk_signal_handler_unblock(GTK_OBJECT(vi_screen->vadj), vi->value_changed);
}

/*
 * PUBLIC: void gtk_vi_quit __P((GtkViWindow*, gint));
 */
void
gtk_vi_quit(vi, write)
    GtkViWindow *vi;
    gint write;
{
    if (write)
	vi->ipviwin->wq(vi->ipviwin);
    else
	vi->ipviwin->quit(vi->ipviwin);
}

/*
 * PUBLIC: void gtk_vi_show_term __P((GtkViWindow*, gint));
 */
void
gtk_vi_show_term(window, show)
    GtkViWindow *window;
    gint show;
{
    gtk_notebook_set_page(GTK_NOTEBOOK(window), show ? 1 : 0);
}

/*
 * PUBLIC: void gtk_vi_key_press_event __P((GtkViWindow*, GdkEventKey*));
 */
void
gtk_vi_key_press_event(window, event)
    GtkViWindow *window;
    GdkEventKey *event;
{
#if 0
    static struct {
	guint key;
	gint offset;
    } table[] = {
	{GDK_Home,	GTK_STRUCT_OFFSET(IPVI, c_bol)	    },
	//{VI_C_BOTTOM,	GTK_STRUCT_OFFSET(IPVI, c_bottom)   },
	{GDK_End,  	GTK_STRUCT_OFFSET(IPVI, c_eol)	    },
	{GDK_Insert,	GTK_STRUCT_OFFSET(IPVI, c_insert)   },
	{GDK_Left, 	GTK_STRUCT_OFFSET(IPVI, c_left)     },
	{GDK_Right,	GTK_STRUCT_OFFSET(IPVI, c_right)    },
	//{VI_C_TOP,  	GTK_STRUCT_OFFSET(IPVI, c_top)	    },
    };
#endif
    static struct {
	guint	keyval;
	char	key;
    } table[] = {
	{ GDK_Left,	    'h' },
	{ GDK_Right,	    'l' },
	{ GDK_Up,	    'k' },
	{ GDK_Down,	    'j' },
	{ GDK_Page_Up,	    'B' - '@' },
	{ GDK_Page_Down,    'F' - '@' },
    };
    char key = event->keyval;
    int i;

#if 0
    for (i = 0; i < sizeof(table)/sizeof(*table); ++i)
	if (table[i].key == event->keyval) {
	    int (*fun) __P((IPVI*)) = 
		*(int (**) __P((IPVI*)) )(((char *)vi->ipvi)+table[i].offset);
	    fun(vi->ipvi);
	    return;
	}
#endif
    for (i = 0; i < sizeof(table)/sizeof(*table); ++i)
	if (table[i].keyval == event->keyval) {
	    window->ipviwin->string(window->ipviwin, &table[i].key, 1);
	    return;
	}

    if (event->state & GDK_CONTROL_MASK) {
	if ((key >= 'a') && (key <= 'z'))
	    key -= 'a' - 'A';
	key -= '@';
    }
    /*
    fprintf(stderr, "key_press %d %d %d %c %p\n", 
	event->length, event->keyval, event->keyval, key, ipvi);
    */
    if (event->length > 0)
	window->ipviwin->string(window->ipviwin, &key, 1);
}



static int
vi_key_press_event(vi_screen, event, vi)
    GtkViWindow *vi;
    GtkWidget *vi_screen;
    GdkEventKey *event;
{
    gint handled;

    handled = gtk_accel_groups_activate (GTK_OBJECT (vi), 
		    event->keyval, (GdkModifierType) event->state);
    if (handled)
	return 1;

    gtk_vi_key_press_event(vi, event);
    gtk_signal_emit_stop_by_name(GTK_OBJECT(vi_screen), "key_press_event");
    /* handled */
    return 1;
}

static void
vi_map(table, vi_screen)
	GtkWidget *vi_screen;
	GtkWidget *table;
{
	gtk_widget_grab_focus(vi_screen);
}

static void
vi_resized(vi_screen, rows, cols, ipviwin)
    int rows,cols;
    IPVIWIN *ipviwin;
    GtkWidget *vi_screen;
{
	GtkViWindow *vi_window = GTK_VI_WINDOW((GtkVi*)(ipviwin->private_data));

	ipviwin->resize(ipviwin, rows, cols);
	vi_window->resized = 1;
}

static void 
vi_adjustment_value_changed (adjustment, ipviwin)
    GtkAdjustment *adjustment;
    IPVIWIN *ipviwin;
{
	GtkViWindow *vi_window = GTK_VI_WINDOW((GtkVi*)(ipviwin->private_data));

	if (vi_window->resized)
		ipviwin->c_settop(ipviwin, adjustment->value);
}


static void 
vi_input_func (gpointer data, gint source, GdkInputCondition condition)
{
    IPVIWIN *ipviwin = (IPVIWIN *) data;

    (void)ipviwin->input(ipviwin, source);
}

static void
vi_init_window (GtkViWindow *window, int fd)
{
    static struct ip_si_operations ipsi_ops_gtk = {
	vi_addstr,
	vi_attribute,
	vi_bell,
	vi_busyoff,
	vi_busyon,
	vi_clrtoeol,
	vi_deleteln,
	vi_discard,
	vi_editopt,
	vi_insertln,
	vi_move,
	vi_quit,
	vi_redraw,
	vi_refresh,
	vi_rename,
	vi_rewrite,
	vi_scrollbar,
	vi_select,
	vi_split,
	(IPFunc_a)vi_waddstr,
    };
    GtkVi *vi = window->vi;

    vi->ipvi->new_window(vi->ipvi, &window->ipviwin, fd);

    window->ipviwin->private_data = window;
    window->ipviwin->set_ops(window->ipviwin, &ipsi_ops_gtk);
    window->input_func = gtk_input_add_full(window->ipviwin->ifd, 
			    GDK_INPUT_READ, 
			    vi_input_func, 0, (gpointer)window->ipviwin, 0);
}

static int
vi_addstr(ipviwin, str, len)
	IPVIWIN	*ipviwin;
	const char *str;
	u_int32_t len;
{
	GtkViWindow* vi = (GtkViWindow*)(ipviwin->private_data);

	gtk_vi_screen_addstr(GTK_VI_SCREEN(vi->vi_screen), str, len);
	return (0);
}

static int
vi_waddstr(ipviwin, str, len)
	IPVIWIN	*ipviwin;
	const CHAR_T *str;
	u_int32_t len;
{
	GtkViWindow* vi = (GtkViWindow*)(ipviwin->private_data);

	gtk_vi_screen_waddstr(GTK_VI_SCREEN(vi->vi_screen), str, len/sizeof(CHAR_T));
	return (0);
}

static int
vi_attribute(ipviwin,attribute,on)
	IPVIWIN	*ipviwin;
	u_int32_t   attribute, on;
{
	GtkViWindow* window = (GtkViWindow*)(ipviwin->private_data);

	if (attribute == SA_ALTERNATE) {
		gtk_vi_show_term(window, !on);
	}
	else
		gtk_vi_screen_attribute(GTK_VI_SCREEN(window->vi_screen), attribute, on);
	return (0);
}

static int
vi_bell(ipbp)
	IPVIWIN *ipbp;
{
    /*
    fprintf(stderr, "vi_bell\n");
    */
#if 0
	/*
	 * XXX
	 * Future... implement visible bell.
	 */
	XBell(XtDisplay(__vi_screen->area), 0);
#endif
	return (0);
}

static int 
vi_busyon (IPVIWIN* ipviwin, const char *a, u_int32_t s)
{
    /*
    fprintf(stderr, "vi_busyon\n");
    */
#if 0
	__vi_set_cursor(__vi_screen, 1);
#endif
	return (0);
}

static int
vi_busyoff(ipbp)
	IPVIWIN *ipbp;
{
    /*
    fprintf(stderr, "vi_busyoff\n");
    */
#if 0
	__vi_set_cursor(__vi_screen, 0);
#endif
	return (0);
}

static int
vi_clrtoeol(ipviwin)
	IPVIWIN	*ipviwin;
{
	GtkViWindow* vi = (GtkViWindow*)(ipviwin->private_data);

	gtk_vi_screen_clrtoel(GTK_VI_SCREEN(vi->vi_screen));
	return 0;
}

static int
vi_deleteln(ipviwin)
	IPVIWIN	*ipviwin;
{
	GtkViWindow* vi = (GtkViWindow*)(ipviwin->private_data);

	gtk_vi_screen_deleteln(GTK_VI_SCREEN(vi->vi_screen));
	return (0);
}

static int 
vi_editopt (IPVIWIN* a, const char *b, u_int32_t c,
                            const char *d, u_int32_t e, u_int32_t f)
{
    /*
    fprintf(stderr, "%p %p vi_editopt\n", a, a->private_data);
    */
#if 0
	/* XXX: Nothing. */
#endif
	return (0);
}


static int
vi_discard(ipbp)
	IPVIWIN *ipbp;
{
    /*
    fprintf(stderr, "vi_discard\n");
    */
#if 0
	/* XXX: Nothing. */
#endif
	return (0);
}

static int
vi_insertln(ipviwin)
	IPVIWIN	*ipviwin;
{
	GtkViWindow* vi = (GtkViWindow*)(ipviwin->private_data);

    gtk_vi_screen_insertln(GTK_VI_SCREEN(vi->vi_screen));
    return (0);
}

static int
vi_move(ipviwin, row, col)
	IPVIWIN	*ipviwin;
	u_int32_t row;
	u_int32_t col;
{
	GtkViWindow* vi = (GtkViWindow*)(ipviwin->private_data);

	gtk_vi_screen_move(GTK_VI_SCREEN(vi->vi_screen), row, col);
	return (0);
}

static int
vi_redraw(ipviwin)
	IPVIWIN	*ipviwin;
{
	GtkViWindow* vi = (GtkViWindow*)(ipviwin->private_data);

	gtk_widget_draw(GTK_WIDGET(vi->vi_screen), NULL);
	return (0);
}

static int
vi_refresh(ipviwin)
	IPVIWIN	*ipviwin;
{
	GtkViWindow* vi = (GtkViWindow*)(ipviwin->private_data);

	gtk_vi_screen_refresh(GTK_VI_SCREEN(vi->vi_screen));
	return (0);
}

static int
vi_quit(ipviwin)
	IPVIWIN	*ipviwin;
{
	GtkViWindow* vi = (GtkViWindow*)(ipviwin->private_data);

	gtk_input_remove(vi->input_func);
	gtk_widget_destroy(GTK_WIDGET(vi));
	return (0);
}

static int
vi_rename(ipviwin, str, len)
	IPVIWIN	*ipviwin;
	const char *str;
	u_int32_t len;
{
	GtkViWindow* vi = (GtkViWindow*)(ipviwin->private_data);

	gchar* name = g_strndup(str, len);
	gtk_signal_emit_by_name(GTK_OBJECT(vi), "rename", name);
	g_free(name);
	return (0);
}

static int
vi_rewrite(ipviwin, row)
	IPVIWIN	*ipviwin;
	u_int32_t row;
{
	GtkViWindow* vi = (GtkViWindow*)(ipviwin->private_data);

	gtk_vi_screen_rewrite(GTK_VI_SCREEN(vi->vi_screen), row);
	return (0);
}


static int
vi_scrollbar(ipviwin, top, size, max)
	IPVIWIN	*ipviwin;
	u_int32_t top, size, max;
{
	GtkViWindow* vi = (GtkViWindow*)(ipviwin->private_data);

	gtk_vi_window_scrollbar(vi, top, size, max);

	return (0);
}

static int vi_select (IPVIWIN* a, const char * b, u_int32_t c)
{
    /*
    fprintf(stderr, "vi_select\n");
    */
#if 0
	/* XXX: Nothing. */
#endif
	return (0);
}

static int
vi_split(ipbp)
	IPVIWIN *ipbp;
{
    fprintf(stderr, "vi_split\n");
#if 0
	/* XXX: Nothing. */
#endif
	return (0);
}

static int
vi_ex_init(ipviwin)
	IPVIWIN	*ipviwin;
{
	GtkViWindow* vi = (GtkViWindow*)(ipviwin->private_data);

/*
	gtk_vi_show_term(vi, 1);
*/
	return 0;
}

static int
vi_vi_init(ipviwin)
	IPVIWIN	*ipviwin;
{
	GtkViWindow* vi = (GtkViWindow*)(ipviwin->private_data);

/*
	gtk_vi_show_term(vi, 0);
*/
	return 0;
}
