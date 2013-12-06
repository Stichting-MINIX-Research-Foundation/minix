/*	$NetBSD: gtk.h,v 1.2 2013/11/22 15:52:05 christos Exp $	*/
typedef struct {
    GtkViScreen  *vi;
    GtkWidget	*main;
    gint    input_func;
    gint    value_changed;
    IPVI    *ipvi;
    int	    resized;
} gtk_vi;
