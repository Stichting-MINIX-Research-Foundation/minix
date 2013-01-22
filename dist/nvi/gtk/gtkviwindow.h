/*	$NetBSD: gtkviwindow.h,v 1.1.1.2 2008/05/18 14:31:22 aymeric Exp $ */

#ifndef __GTK_VI_WINDOW_H__
#define __GTK_VI_WINDOW_H__

#ifndef HAVE_PANGO
#define gtk_marshal_VOID__STRING gtk_marshal_NONE__STRING
#define GTK_CLASS_TYPE(class)	class->type
#endif

#define GTK_TYPE_VI_WINDOW                  (gtk_vi_window_get_type ())
#define GTK_VI_WINDOW(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_VI_WINDOW, GtkViWindow))
#define GTK_VI_WINDOW_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_VI_WINDOW, GtkViWindowClass))
#define GTK_IS_VI_WINDOW(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_VI_WINDOW))
#define GTK_IS_VI_WINDOW_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_VI_WINDOW))

typedef struct _GtkViWindow           GtkViWindow;
typedef struct _GtkViWindowClass      GtkViWindowClass;

struct _GtkViWindow
{
  GtkNotebook	notebook;

  GtkWidget *term;

  GtkVi	    *vi;
  GtkWidget *table;
  GtkWidget *vi_screen;
  gint      value_changed;
  int	    resized;

  gint      input_func;
  IPVIWIN   *ipviwin;
};

struct _GtkViWindowClass
{
  GtkNotebookClass  parent_class;
};

GtkType     gtk_vi_window_get_type (void);
GtkWidget * gtk_vi_window_new (GtkVi *vi);
void 	    gtk_vi_window_scrollbar(GtkViWindow *vi, guint top, guint size, guint max);

#endif /* __GTK_VI_WINDOW_H__ */
