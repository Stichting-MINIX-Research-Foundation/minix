#include <stdio.h>
#include <string.h>

#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include "gtkviscreen.h"
#include <gdk/gdkx.h>

#define INTISUCS(c)	((c & ~0x7F) && !(((c) >> 16) & 0x7F))
#define INTUCS(c)	(c)
#ifdef USE_WIDECHAR
#define CHAR_WIDTH(sp, ch)  wcwidth(ch)
#else
#define CHAR_WIDTH(sp, ch)  1
#endif

void * v_strset __P((CHAR_T *s, CHAR_T c, size_t n));

#define DEFAULT_VI_SCREEN_WIDTH_CHARS     80
#define DEFAULT_VI_SCREEN_HEIGHT_LINES    25
#define VI_SCREEN_BORDER_ROOM         1

enum {
  ARG_0,
  ARG_VADJUSTMENT,
};

enum {
    RESIZED,
    LAST_SIGNAL
};

static void gtk_vi_screen_class_init     (GtkViScreenClass   *klass);
static void gtk_vi_screen_set_arg        (GtkObject      *object,
					  GtkArg         *arg,
					  guint           arg_id);
static void gtk_vi_screen_get_arg        (GtkObject      *object,
					  GtkArg         *arg,
					  guint           arg_id);
static void gtk_vi_screen_init           (GtkViScreen        *vi);
static void gtk_vi_screen_destroy        (GtkObject      *object);
static void gtk_vi_screen_realize        (GtkWidget      *widget);
/*
static void gtk_vi_screen_map (GtkWidget *widget);
static void gtk_vi_screen_unmap (GtkWidget *widget);
*/
static void gtk_vi_screen_size_request   (GtkWidget      *widget,
				      GtkRequisition *requisition);
static void gtk_vi_screen_size_allocate  (GtkWidget      *widget,
				      GtkAllocation  *allocation);
/*
static void gtk_vi_screen_adjustment     (GtkAdjustment  *adjustment,
				      GtkViScreen        *text);
*/

static gint gtk_vi_screen_expose            (GtkWidget         *widget,
					 GdkEventExpose    *event);

static void recompute_geometry (GtkViScreen* vi);
static void expose_text (GtkViScreen* vi, GdkRectangle *area, gboolean cursor);
static void draw_lines(GtkViScreen *vi, gint y, gint x, gint ymax, gint xmax);
static void mark_lines(GtkViScreen *vi, gint ymin, gint xmin, gint ymax, gint xmax);

static GtkWidgetClass *parent_class = NULL;
static guint vi_screen_signals[LAST_SIGNAL] = { 0 };

static GdkFont *gb_font;
static GdkFont *tfn;
static GdkFont *tfw;

#define CharAt(scr,y,x)	scr->chars + (y) * scr->cols + x
#define FlagAt(scr,y,x)	(scr->reverse + (y) * scr->cols + x)
#define ColAt(scr,y,x)	(scr->endcol + (y) * scr->cols + x)

#define COLOR_STANDARD	    0x00
#define COLOR_STANDOUT	    0x01

/* XXX */
enum { SA_ALTERNATE, SA_INVERSE };

void
gtk_vi_screen_attribute(GtkViScreen *vi, gint attribute, gint on)
{
    switch (attribute) {
    case SA_INVERSE:
	vi->color = on ? COLOR_STANDOUT : COLOR_STANDARD;
	break;
    }
}

/* col is screen column */
void
gtk_vi_screen_move(GtkViScreen *vi, gint row, gint col)
{
    gint x;
    guchar *endcol;

    endcol = vi->endcol + row*vi->cols;
    for (x = 0; col > endcol[x]; ++x);
    vi->curx = x;
    vi->cury = row;
}

static void
cleartoel (GtkViScreen *vi, guint row, guint col)
{
    CHAR_T *p, *e;

    if (MEMCMP(p = CharAt(vi,row,col), e = CharAt(vi,vi->rows,0), 
		vi->cols - col)) {
	MEMMOVE(p, e, vi->cols - col);
	memset(FlagAt(vi,row,col), COLOR_STANDARD, vi->cols - col);
	mark_lines(vi, row, col, row+1, vi->cols);
    }
}

void
gtk_vi_screen_clrtoel (GtkViScreen *vi)
{
    cleartoel(vi, vi->cury, vi->curx);
}

void
gtk_vi_screen_addstr(GtkViScreen *vi, const char *str, int len)
{
    CHAR_T *p, *end;
    CHAR_T *line;
    guchar *endcol;
    gint col, startcol;
    gint x;

    line = vi->chars + vi->cury*vi->cols; 
    endcol = vi->endcol + vi->cury*vi->cols;
    x = vi->curx;
    startcol = x ? endcol[x-1] : -1;
    for (p = CharAt(vi,vi->cury,vi->curx), end = p + len, col = startcol; 
		 p < end; ++x) {
	*p++ = *str++;
	endcol[x] = ++col;
    }
    memset(FlagAt(vi,vi->cury,vi->curx), vi->color, len);

    mark_lines(vi, vi->cury, startcol+1, vi->cury+1, endcol[x-1]+1);

    if (endcol[x-1] >= vi->cols) {
	if (++vi->cury >= vi->rows) {
	    vi->cury = vi->rows-1;
	    vi->curx = x-1;
	} else {
	    vi->curx = 0;
	}
    } else vi->curx += len;
    if (x < vi->cols) endcol[x] = vi->cols;
}

void
gtk_vi_screen_waddstr(GtkViScreen *vi, const CHAR_T *str, int len)
{
    CHAR_T *p, *end;
    CHAR_T *line;
    guchar *endcol;
    gint col, startcol;
    gint x;

    MEMMOVE(CharAt(vi,vi->cury,vi->curx),str,len);
    memset(FlagAt(vi,vi->cury,vi->curx), vi->color, len);

    line = vi->chars + vi->cury*vi->cols; 
    endcol = vi->endcol + vi->cury*vi->cols;
    x = vi->curx;
    startcol = x ? endcol[x-1] : -1;
    for (col = startcol; x < vi->curx + len; ++x)
	endcol[x] = col += CHAR_WIDTH(NULL, *(line+x));

    mark_lines(vi, vi->cury, startcol+1, vi->cury+1, endcol[x-1]+1);

    if (endcol[x-1] >= vi->cols) {
	if (++vi->cury >= vi->rows) {
	    vi->cury = vi->rows-1;
	    vi->curx = x-1;
	} else {
	    vi->curx = 0;
	}
    } else vi->curx += len;
    if (x < vi->cols) endcol[x] = vi->cols;
}

void
gtk_vi_screen_deleteln(GtkViScreen *vi)
{
    gint y = vi->cury;
    gint rows = vi->rows - (y+1);

    MEMMOVE(CharAt(vi,y,0), CharAt(vi,y+1,0), rows * vi->cols);
    cleartoel(vi,vi->rows-1,0);
    memmove(FlagAt(vi,y,0), FlagAt(vi,y+1,0), rows * vi->cols);
    memmove(ColAt(vi,y,0), ColAt(vi,y+1,0), rows * vi->cols);
    mark_lines(vi, y, 0, vi->rows-1, vi->cols);
}

void
gtk_vi_screen_insertln(GtkViScreen *vi)
{
    gint y = vi->cury;
    gint rows = vi->rows - (y+1);

    MEMMOVE(CharAt(vi,y+1,0), CharAt(vi,y,0), rows * vi->cols);
    cleartoel(vi,y,0);
    memmove(FlagAt(vi,y+1,0), FlagAt(vi,y,0), rows * vi->cols);
    memmove(ColAt(vi,y+1,0), ColAt(vi,y,0), rows * vi->cols);
    mark_lines(vi, y+1, 0, vi->rows, vi->cols);
}

void
gtk_vi_screen_refresh(GtkViScreen *vi)
{
    if (vi->lastx != vi->curx || vi->lasty != vi-> cury) {
	mark_lines(vi, vi->lasty, 
		vi->lastx ? *ColAt(vi,vi->lasty,vi->lastx-1) + 1 : 0, 
		vi->lasty+1, *ColAt(vi,vi->lasty,vi->lastx)+1);
	mark_lines(vi, vi->cury, 
		vi->curx ? *ColAt(vi,vi->cury,vi->curx-1) + 1 : 0, 
		vi->cury+1, *ColAt(vi,vi->cury,vi->curx)+1);
    }
    if (vi->marked_maxy == 0)
	return;
    draw_lines(vi, vi->marked_y, vi->marked_x, vi->marked_maxy, vi->marked_maxx);
    vi->marked_x = vi->cols;
    vi->marked_y = vi->rows;
    vi->marked_maxx = 0;
    vi->marked_maxy = 0;
    vi->lastx = vi->curx;
    vi->lasty = vi->cury;
}

void
gtk_vi_screen_rewrite(GtkViScreen *vi, gint row)
{
    memset(FlagAt(vi,row,0), COLOR_STANDARD, vi->cols);
    mark_lines(vi, row, 0, row+1, vi->cols);
}

GtkType
gtk_vi_screen_get_type (void)
{
  static GtkType vi_screen_type = 0;
  
  if (!vi_screen_type)
    {
      static const GtkTypeInfo vi_screen_info =
      {
	"GtkViScreen",
	sizeof (GtkViScreen),
	sizeof (GtkViScreenClass),
	(GtkClassInitFunc) gtk_vi_screen_class_init,
	(GtkObjectInitFunc) gtk_vi_screen_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      vi_screen_type = gtk_type_unique (GTK_TYPE_WIDGET, &vi_screen_info);
    }
  
  return vi_screen_type;
}

static void
gtk_vi_screen_class_init (GtkViScreenClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  parent_class = gtk_type_class (GTK_TYPE_WIDGET);

  vi_screen_signals[RESIZED] =
    gtk_signal_new ("resized",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE(object_class),
		    GTK_SIGNAL_OFFSET (GtkViScreenClass, resized),
		    gtk_marshal_NONE__INT_INT,
		    GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT, 0);

#ifndef HAVE_PANGO
  gtk_object_class_add_signals(object_class, vi_screen_signals, LAST_SIGNAL);
#endif

  gtk_object_add_arg_type ("GtkViScreen::vadjustment",
			   GTK_TYPE_ADJUSTMENT,
			   GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT,
			   ARG_VADJUSTMENT);

  object_class->set_arg = gtk_vi_screen_set_arg;
  object_class->get_arg = gtk_vi_screen_get_arg;
  object_class->destroy = gtk_vi_screen_destroy;

  widget_class->realize = gtk_vi_screen_realize;
  /*
  widget_class->map = gtk_vi_screen_map;
  widget_class->unmap = gtk_vi_screen_unmap;
  */
  widget_class->size_request = gtk_vi_screen_size_request;
  widget_class->size_allocate = gtk_vi_screen_size_allocate;
  widget_class->expose_event = gtk_vi_screen_expose;

  class->rename = NULL;
  class->resized = NULL;

  gb_font = gdk_font_load ("-*-*-*-*-*-*-16-*-*-*-*-*-gb2312.1980-*");
  /*
  tf = gdk_font_load ("-misc-fixed-*-*-*-*-16-*-*-*-*-*-iso10646-*");
  */
  tfn = gdk_font_load ("-misc-fixed-medium-r-semicondensed--13-120-75-75-c-60-iso10646");
  tfw = gdk_font_load ("-Misc-Fixed-Medium-R-*-*-13-120-75-75-C-120-ISO10646-1");
}

static void
gtk_vi_screen_set_arg (GtkObject        *object,
		      GtkArg           *arg,
		      guint             arg_id)
{
  GtkViScreen *vi_screen;
  
  vi_screen = GTK_VI_SCREEN (object);
  
  switch (arg_id)
    {
    case ARG_VADJUSTMENT:
      gtk_vi_screen_set_adjustment (vi_screen, GTK_VALUE_POINTER (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_vi_screen_get_arg (GtkObject        *object,
		      GtkArg           *arg,
		      guint             arg_id)
{
  GtkViScreen *vi_screen;
  
  vi_screen = GTK_VI_SCREEN (object);
  
  switch (arg_id)
    {
    case ARG_VADJUSTMENT:
      GTK_VALUE_POINTER (*arg) = vi_screen->vadj;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_vi_screen_init (GtkViScreen *vi)
{
  GtkStyle *style;

  GTK_WIDGET_SET_FLAGS (vi, GTK_CAN_FOCUS);

  vi->text_area = NULL;
  vi->chars = 0;
  vi->reverse = 0;
  vi->cols = 0;
  vi->color = COLOR_STANDARD;
  vi->cols = 0;
  vi->rows = 0;

#ifdef HAVE_PANGO
  vi->conx = NULL;
#endif

  style = gtk_style_copy(GTK_WIDGET(vi)->style);
  gdk_font_unref(style->font);
  style->font = gdk_font_load("-*-fixed-*-*-*-*-16-*-*-*-*-*-iso8859-*");
  GTK_WIDGET(vi)->style = style;
}

static void
gtk_vi_screen_destroy (GtkObject *object)
{
  GtkViScreen *vi_screen;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_VI_SCREEN (object));
  
  vi_screen = (GtkViScreen*) object;

  /*
  gtk_signal_disconnect_by_data (GTK_OBJECT (vi_screen->vadj), vi_screen);
  */

  GTK_OBJECT_CLASS(parent_class)->destroy (object);
}

GtkWidget*
gtk_vi_screen_new (GtkAdjustment *vadj)
{
  GtkWidget *vi;

  vi = gtk_widget_new (GTK_TYPE_VI_SCREEN,
			 "vadjustment", vadj,
			 NULL);


  return vi;
}

void
gtk_vi_screen_set_adjustment (GtkViScreen       *vi_screen,
			  GtkAdjustment *vadj)
{
  g_return_if_fail (vi_screen != NULL);
  g_return_if_fail (GTK_IS_VI_SCREEN (vi_screen));
  if (vadj)
    g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
  else
    vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 1.0, 0.0, 1.0, 0.0, 0.0));
  
  if (vi_screen->vadj && (vi_screen->vadj != vadj))
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (vi_screen->vadj), vi_screen);
      gtk_object_unref (GTK_OBJECT (vi_screen->vadj));
    }
  
  if (vi_screen->vadj != vadj)
    {
      vi_screen->vadj = vadj;
      gtk_object_ref (GTK_OBJECT (vi_screen->vadj));
      gtk_object_sink (GTK_OBJECT (vi_screen->vadj));
      
      /*
      gtk_signal_connect (GTK_OBJECT (vi_screen->vadj), "changed",
			  (GtkSignalFunc) gtk_vi_screen_adjustment,
			  vi_screen);
      gtk_signal_connect (GTK_OBJECT (vi_screen->vadj), "value_changed",
			  (GtkSignalFunc) gtk_vi_screen_adjustment,
			  vi_screen);
      gtk_signal_connect (GTK_OBJECT (vi_screen->vadj), "disconnect",
			  (GtkSignalFunc) gtk_vi_screen_disconnect,
			  vi_screen);
      gtk_vi_screen_adjustment (vadj, vi_screen);
      */
    }
}

static void
gtk_vi_screen_realize (GtkWidget *widget)
{
  GtkViScreen *vi;
  GdkWindowAttr attributes;
  gint attributes_mask;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VI_SCREEN (widget));
  
  vi = GTK_VI_SCREEN (widget);
  GTK_WIDGET_SET_FLAGS (vi, GTK_REALIZED);
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_BUTTON_MOTION_MASK |
			    GDK_ENTER_NOTIFY_MASK |
			    GDK_LEAVE_NOTIFY_MASK |
			    GDK_KEY_PRESS_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, vi);
  
  attributes.x = (widget->style->xthickness + VI_SCREEN_BORDER_ROOM);
  attributes.y = (widget->style->ythickness + VI_SCREEN_BORDER_ROOM);
  attributes.width = MAX (1, (gint)widget->allocation.width - (gint)attributes.x * 2);
  attributes.height = MAX (1, (gint)widget->allocation.height - (gint)attributes.y * 2);
  
  vi->text_area = gdk_window_new (widget->window, &attributes, attributes_mask);
  gdk_window_set_user_data (vi->text_area, vi);
  
  widget->style = gtk_style_attach (widget->style, widget->window);
  
  /* Can't call gtk_style_set_background here because it's handled specially */
  gdk_window_set_background (widget->window, &widget->style->base[GTK_STATE_NORMAL]);
  gdk_window_set_background (vi->text_area, &widget->style->base[GTK_STATE_NORMAL]);

  vi->gc = gdk_gc_new (vi->text_area);
  /* What's this ? */
  gdk_gc_set_exposures (vi->gc, TRUE);
  gdk_gc_set_foreground (vi->gc, &widget->style->text[GTK_STATE_NORMAL]);

  vi->reverse_gc = gdk_gc_new (vi->text_area);
  gdk_gc_set_foreground (vi->reverse_gc, &widget->style->base[GTK_STATE_NORMAL]);

  gdk_window_show (vi->text_area);

  recompute_geometry (vi);
}  

static void
gtk_vi_screen_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  gint xthick;
  gint ythick;
  gint char_height;
  gint char_width;
  GtkViScreen *vi;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VI_SCREEN (widget));
  g_return_if_fail (requisition != NULL);
  
  vi = GTK_VI_SCREEN (widget);

  xthick = widget->style->xthickness + VI_SCREEN_BORDER_ROOM;
  ythick = widget->style->ythickness + VI_SCREEN_BORDER_ROOM;
  
  vi->ch_ascent = widget->style->font->ascent;
  vi->ch_height = (widget->style->font->ascent + widget->style->font->descent) + 1;
  vi->ch_width = gdk_text_width (widget->style->font, "A", 1);
  char_height = DEFAULT_VI_SCREEN_HEIGHT_LINES * vi->ch_height;
  char_width = DEFAULT_VI_SCREEN_WIDTH_CHARS * vi->ch_width;
  
  requisition->width  = char_width  + xthick * 2;
  requisition->height = char_height + ythick * 2;
}

static void
gtk_vi_screen_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GtkViScreen *vi;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_VI_SCREEN (widget));
  g_return_if_fail (allocation != NULL);
  
  vi = GTK_VI_SCREEN (widget);
  
  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
      
      gdk_window_move_resize (vi->text_area,
			      widget->style->xthickness + VI_SCREEN_BORDER_ROOM,
			      widget->style->ythickness + VI_SCREEN_BORDER_ROOM,
			      MAX (1, (gint)widget->allocation.width - (gint)(widget->style->xthickness +
							  (gint)VI_SCREEN_BORDER_ROOM) * 2),
			      MAX (1, (gint)widget->allocation.height - (gint)(widget->style->ythickness +
							   (gint)VI_SCREEN_BORDER_ROOM) * 2));
      
      recompute_geometry (vi);
    }
}

/*
static void
gtk_vi_screen_adjustment (GtkAdjustment *adjustment,
			 GtkViScreen       *vi_screen)
{
  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  g_return_if_fail (vi_screen != NULL);
  g_return_if_fail (GTK_IS_VI_SCREEN (vi_screen));

}
*/
  
static gint
gtk_vi_screen_expose (GtkWidget      *widget,
		 GdkEventExpose *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_VI_SCREEN (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  if (event->window == GTK_VI_SCREEN (widget)->text_area)
    {
      expose_text (GTK_VI_SCREEN (widget), &event->area, TRUE);
    }
  
  return FALSE;
}

static void
recompute_geometry (GtkViScreen* vi)
{
    //gint xthickness;
    //gint ythickness;
    gint height;
    gint width;
    gint rows, cols;
    gint i;

    //xthickness = widget->style->xthickness + VI_SCREEN_BORDER_ROOM;
    //ythickness = widget->style->ythickness + VI_SCREEN_BORDER_ROOM;

    gdk_window_get_size (vi->text_area, &width, &height);

    rows = height / vi->ch_height;
    cols = width / vi->ch_width;

    if (rows == vi->rows && cols == vi->cols)
	return;

    vi->marked_x = vi->cols = cols;
    vi->marked_y = vi->rows = rows;
    vi->marked_maxx = 0;
    vi->marked_maxy = 0;

    g_free(vi->chars);
    vi->chars = (CHAR_T*)g_new(gchar, (vi->rows+1)*vi->cols * sizeof(CHAR_T));
    STRSET(vi->chars, L(' '), (vi->rows+1)*vi->cols);
    g_free(vi->endcol);
    vi->endcol = g_new(guchar, vi->rows*vi->cols);
    g_free(vi->reverse);
    vi->reverse = g_new(guchar, vi->rows*vi->cols);
    memset(vi->reverse, 0, vi->rows*vi->cols);

    gtk_signal_emit(GTK_OBJECT(vi), vi_screen_signals[RESIZED], vi->rows, vi->cols);
}

static void
expose_text (GtkViScreen* vi, GdkRectangle *area, gboolean cursor)
{
    gint ymax;
    gint xmax, xmin;

    gdk_window_clear_area (vi->text_area, area->x, area->y, 
			    area->width, area->height);
    ymax = MIN((area->y + area->height + vi->ch_height - 1) / vi->ch_height,
		vi->rows);
    xmin = area->x / vi->ch_width;
    xmax = MIN((area->x + area->width + vi->ch_width - 1) / vi->ch_width,
		vi->cols);
    draw_lines(vi, area->y / vi->ch_height, xmin, ymax, xmax);
}

#define Inverse(screen,y,x) \
    ((*FlagAt(screen,y,x) == COLOR_STANDOUT) ^ \
	(screen->cury == y && screen->curx == x))

static void
draw_lines(GtkViScreen *vi, gint ymin, gint xmin, gint ymax, gint xmax)
{
    gint y, x, len, blen, xpos;
    CHAR_T *line;
    GdkGC *fg, *bg;
    GdkFont *font;
    gchar buf[2];
    gchar *p;
    gboolean pango;

    for (y = ymin, line = vi->chars + y*vi->cols; 
			     y < ymax; ++y, line += vi->cols) {
	for (x = 0, xpos = 0; xpos <= xmin; ++x)
	    xpos += CHAR_WIDTH(NULL, *(line+x));
	--x;
	xpos -= CHAR_WIDTH(NULL, *(line+x));
	for (; xpos < xmax; x+=len, xpos+= blen) {
	    gchar inverse;
	    inverse = Inverse(vi,y,x); 
	    len = 1;
	    if (sizeof(CHAR_T) == sizeof(gchar))
		for (; x+len < xmax && 
		       Inverse(vi,y,x+len) == inverse; ++len);
	    if (inverse) {
		fg = vi->reverse_gc;
		bg = vi->gc;
	    } else {
		bg = vi->reverse_gc;
		fg = vi->gc;
	    }
	    pango = 0;
#ifdef HAVE_PANGO
	    if (INTISUCS(*(line+x))) {
		if (!vi->conx) {
		    PangoFontDescription font_description;

		    font_description.family_name = g_strdup ("monospace");
		    font_description.style = PANGO_STYLE_NORMAL;
		    font_description.variant = PANGO_VARIANT_NORMAL;
		    font_description.weight = 500;
		    font_description.stretch = PANGO_STRETCH_NORMAL;
		    font_description.size = 15000;

		    vi->conx = gdk_pango_context_get();
		    pango_context_set_font_description (vi->conx, 
			&font_description);
		    pango_context_set_lang(vi->conx, "en_US");
		    vi->alist = pango_attr_list_new();
		}
		blen = CHAR_WIDTH(NULL, *(line+x));
		pango = 1;
	    } else 
#endif
	    {
		font = GTK_WIDGET(vi)->style->font;
		if (sizeof(CHAR_T) == sizeof(gchar))
		    p = (gchar*)line+x;
		else {
		    buf[0] = *(line+x);
		    p = buf;
		}
		blen = len;
	    }
	    gdk_draw_rectangle(vi->text_area, bg, 1, xpos * vi->ch_width,
				y * vi->ch_height, blen * vi->ch_width,
				vi->ch_height);
	    /* hack to not display half a wide character that wasn't
	     * removed.
	     */
	    if (!pango)
		gdk_draw_text (vi->text_area, font, fg,
				xpos * vi->ch_width, 
				y * vi->ch_height + vi->ch_ascent, 
				p, blen);
#ifdef HAVE_PANGO
	    else {
		PangoGlyphString *gs;
		GList *list;
		PangoItem *item;
		char buf[3];
		int len;

		len = ucs2utf8(line+x, 1, buf);
		list = pango_itemize(vi->conx, buf, 0, len, vi->alist, NULL);
		item = list->data;
		gs = pango_glyph_string_new ();
		pango_shape(buf, len, &item->analysis, gs);

		gdk_draw_glyphs (vi->text_area, fg, item->analysis.font,
				xpos * vi->ch_width, 
				y * vi->ch_height + vi->ch_ascent, gs);
	    }
#endif
	}
    }
}

static void
mark_lines(GtkViScreen *vi, gint ymin, gint xmin, gint ymax, gint xmax)
{
    if (ymin < vi->marked_y) vi->marked_y = ymin;
    if (xmin < vi->marked_x) vi->marked_x = xmin;
    if (ymax > vi->marked_maxy) vi->marked_maxy = ymax;
    if (xmax > vi->marked_maxx) vi->marked_maxx = xmax;
}
