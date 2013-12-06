/*	$NetBSD: gtkvi.h,v 1.2 2013/11/22 15:52:05 christos Exp $	*/
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
