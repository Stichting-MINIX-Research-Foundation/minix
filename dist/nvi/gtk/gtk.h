/*	$NetBSD: gtk.h,v 1.1.1.2 2008/05/18 14:31:22 aymeric Exp $ */

typedef struct {
    GtkViScreen  *vi;
    GtkWidget	*main;
    gint    input_func;
    gint    value_changed;
    IPVI    *ipvi;
    int	    resized;
} gtk_vi;
