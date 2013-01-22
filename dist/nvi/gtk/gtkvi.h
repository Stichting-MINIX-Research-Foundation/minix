/*	$NetBSD: gtkvi.h,v 1.1.1.2 2008/05/18 14:31:22 aymeric Exp $ */

#ifndef __GTK_VI_H__
#define __GTK_VI_H__

typedef struct _GtkVi           GtkVi;

struct _GtkVi
{
//    GtkWidget	*term;
//    GtkWidget	*vi;	    /* XXX */
//    GtkWidget	*vi_window;
    IPVI    *ipvi;
};
#endif /* __GTK_VI_H__ */
