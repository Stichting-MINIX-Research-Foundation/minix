/*	$NetBSD: gtkviscreen.h,v 1.1.1.2 2008/05/18 14:31:22 aymeric Exp $ */

#ifndef __GTK_VI_SCREEN_H__
#define __GTK_VI_SCREEN_H__

#include <sys/types.h>
#include "config.h"
#include "port.h"
#include "../common/multibyte.h"

#ifdef HAVE_PANGO
#include <pango/pango.h>
#include <pango/pangox.h>
#else
#define xthickness klass->xthickness
#define ythickness klass->ythickness
#define GTK_CLASS_TYPE(class)	class->type
#endif

#define GTK_TYPE_VI_SCREEN                  (gtk_vi_screen_get_type ())
#define GTK_VI_SCREEN(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_VI_SCREEN, GtkViScreen))
#define GTK_VI_SCREEN_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_VI_SCREEN, GtkViScreenClass))
#define GTK_IS_VI_SCREEN(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_VI_SCREEN))
#define GTK_IS_VI_SCREEN_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_VI_SCREEN))

typedef struct _GtkViScreen           GtkViScreen;
typedef struct _GtkViScreenClass      GtkViScreenClass;

struct _GtkViScreen
{
  GtkWidget widget;

  GdkWindow *text_area;

  GtkAdjustment *vadj;

  GdkGC *gc;
  GdkGC *reverse_gc;

  CHAR_T  *chars;
  guchar  *endcol;	    
  guchar  *reverse;
  guchar  color;

  gint	cols, rows;
  gint	ch_width, ch_height, ch_ascent;
  gint  curx, cury;			 /* character position */
  gint  lastx, lasty;
  gint	marked_x, marked_y, marked_maxx, marked_maxy;

#ifdef HAVE_PANGO
  PangoContext *conx;
  PangoAttrList* alist;
#endif
};

struct _GtkViScreenClass
{
  GtkWidgetClass parent_class;

  void (*rename) (GtkViScreen *vi, gchar *name, gint len);
  void (*resized) (GtkViScreen *vi, gint width, gint height);
};

GtkType    gtk_vi_screen_get_type        (void);
GtkWidget* gtk_vi_screen_new             (GtkAdjustment *vadj);
void	   gtk_vi_screen_set_adjustment (GtkViScreen       *vi_screen,
					  GtkAdjustment *vadj);
void	   gtk_vi_screen_move		  (GtkViScreen *vi, gint row, gint col);
void	   gtk_vi_screen_clrtoel	  (GtkViScreen *vi);
void	   gtk_vi_screen_attribute(GtkViScreen *vi, gint attribute, gint on);
void 	   gtk_vi_screen_addstr	  (GtkViScreen *vi, const char *str, int len);
void	   gtk_vi_screen_deleteln	(GtkViScreen *vi);
void	   gtk_vi_screen_insertln	(GtkViScreen *vi);
void	   gtk_vi_screen_refresh	(GtkViScreen *vi);
void	   gtk_vi_screen_rewrite	(GtkViScreen *vi, gint row);

#endif /* __GTK_VI_SCREEN_H__ */
