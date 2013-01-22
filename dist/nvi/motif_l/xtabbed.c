/*	$NetBSD: xtabbed.c,v 1.1.1.2 2008/05/18 14:31:33 aymeric Exp $ */

/* ***********************************************************************
 * This module implements a motif tabbed window widget.
 * The source is copied from the Free Widget Foundation
 * This file is divided into thse parts
 *	o - Conversion routines for the X resource manager
 *	o - Routines for drawing rotated text
 *	o - A motif widget for tabbed windows
 *	o - A driver for the above in the flavor of the xt utilities module
 * ***********************************************************************
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <X11/StringDefs.h>
#include <X11/IntrinsicP.h>
#if defined(VMS_HOST)
#include <DECW$INCLUDE/shape.h>
#else
#include <X11/extensions/shape.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <math.h>


/* ***********************************************************************
 * "rotated.h"
 * ***********************************************************************
 */

/* ************************************************************************ */


/* Header file for the `xvertext 5.0' routines.

   Copyright (c) 1993 Alan Richardson (mppa3@uk.ac.sussex.syma) */


/* ************************************************************************ */

#ifndef _XVERTEXT_INCLUDED_ 
#define _XVERTEXT_INCLUDED_


#define XV_VERSION      5.0
#define XV_COPYRIGHT \
      "xvertext routines Copyright (c) 1993 Alan Richardson"


/* ---------------------------------------------------------------------- */


/* text alignment */

#define NONE             0
#define TLEFT            1
#define TCENTRE          2
#define TRIGHT           3
#define MLEFT            4
#define MCENTRE          5
#define MRIGHT           6
#define BLEFT            7
#define BCENTRE          8
#define BRIGHT           9


/* ---------------------------------------------------------------------- */

/* this shoulf be C++ compliant, thanks to 
     vlp@latina.inesc.pt (Vasco Lopes Paulo) */

#if defined(__cplusplus) || defined(c_plusplus)

extern "C" {
static float   XRotVersion(char*, int);
static void    XRotSetMagnification(float);
static void    XRotSetBoundingBoxPad(int);
static int     XRotDrawString(Display*, XFontStruct*, float,
                       Drawable, GC, int, int, char*);
static int     XRotDrawImageString(Display*, XFontStruct*, float,
                            Drawable, GC, int, int, char*);
static int     XRotDrawAlignedString(Display*, XFontStruct*, float,
                              Drawable, GC, int, int, char*, int);
static int     XRotDrawAlignedImageString(Display*, XFontStruct*, float,
                                   Drawable, GC, int, int, char*, int);
static XPoint *XRotTextExtents(Display*, XFontStruct*, float,
			int, int, char*, int);
}

#else

static float   XRotVersion(char *str, int n);
static void    XRotSetMagnification(float m);
static void    XRotSetBoundingBoxPad(int p);
static int     XRotDrawString(Display *dpy, XFontStruct *font, float angle, Drawable drawable, GC gc, int x, int y, char *str);
static int     XRotDrawImageString(Display *dpy, XFontStruct *font, float angle, Drawable drawable, GC gc, int x, int y, char *str);
static int     XRotDrawAlignedString(Display *dpy, XFontStruct *font, float angle, Drawable drawable, GC gc, int x, int y, char *text, int align);
static int     XRotDrawAlignedImageString(Display *dpy, XFontStruct *font, float angle, Drawable drawable, GC gc, int x, int y, char *text, int align);
static XPoint *XRotTextExtents(Display *dpy, XFontStruct *font, float angle, int x, int y, char *text, int align);

#endif /* __cplusplus */

/* ---------------------------------------------------------------------- */


#endif /* _XVERTEXT_INCLUDED_ */




/* ***********************************************************************
 * "strarray.h"
 * ***********************************************************************
 */

#ifndef _strarray_h_
#define _strarray_h_
/*
   StringArray
   ===========
   The type |StringArray| represents an array of |String|s, with the
   proviso that by convention the last member of a |StringArray| is
   always a |NULL| pointer. There is a converter that can construct a
   |StringArray| from a single string.


   cvtStringToStringArray
   ======================
   The converter from |String| to |StringArray| makes a copy of the
   passed string and then replaces all occurences of the delimiter
   with a nul byte. The |StringArray| is filled with pointers to the
   parts of the string.

   The delimiter character is the first character in the string.


   newStringArray
   ==============
   The function |newStringArray| makes a copy of a |StringArray|. It
   allocates new space for the array itself and for the strings that
   it contains.


   freeStringArray
   ===============
   |freeStringArray| deallocates the array and all strings it
   contains. Note that this works for StringArrays produced with
   |newStringArray|, but not for those created by
   |cvtStringToStringArray|!

*/


typedef String * StringArray;

static Boolean cvtStringToStringArray(
#if NeedFunctionPrototypes
    Display *display,
    XrmValuePtr args,
    Cardinal *num_args,
    XrmValuePtr from,
    XrmValuePtr to,
    XtPointer *converter_data
#endif
);


StringArray newStringArray(
#if NeedFunctionPrototypes
    StringArray a
#endif
);


void freeStringArray(
#if NeedFunctionPrototypes
    StringArray a
#endif
);


#endif /* _strarray_h_ */


/* ***********************************************************************
 * "XmTabs.h"
 * ***********************************************************************
 */

/* Generated by wbuild from "XmTabs.w"
** (generator version Revision: 8.5 of Date: 2001/06/25 15:19:28)
*/
#ifndef _XmTabs_H_
#define _XmTabs_H_
typedef enum {
	    XfwfUpTabs, XfwfDownTabs, XfwfLeftTabs, XfwfRightTabs,
	} TabsOrientation;

#ifndef XtNorientation
#define XtNorientation "orientation"
#endif
#ifndef XtCOrientation
#define XtCOrientation "Orientation"
#endif
#ifndef XtRTabsOrientation
#define XtRTabsOrientation "TabsOrientation"
#endif

#ifndef XtNlefttabs
#define XtNlefttabs "lefttabs"
#endif
#ifndef XtCLefttabs
#define XtCLefttabs "Lefttabs"
#endif
#ifndef XtRInt
#define XtRInt "Int"
#endif

#ifndef XtNrighttabs
#define XtNrighttabs "righttabs"
#endif
#ifndef XtCRighttabs
#define XtCRighttabs "Righttabs"
#endif
#ifndef XtRInt
#define XtRInt "Int"
#endif

#ifndef XtNlabels
#define XtNlabels "labels"
#endif
#ifndef XtCLabels
#define XtCLabels "Labels"
#endif
#ifndef XtRStringArray
#define XtRStringArray "StringArray"
#endif

#ifndef XtNtabWidthPercentage
#define XtNtabWidthPercentage "tabWidthPercentage"
#endif
#ifndef XtCTabWidthPercentage
#define XtCTabWidthPercentage "TabWidthPercentage"
#endif
#ifndef XtRInt
#define XtRInt "Int"
#endif

#ifndef XtNcornerwidth
#define XtNcornerwidth "cornerwidth"
#endif
#ifndef XtCCornerwidth
#define XtCCornerwidth "Cornerwidth"
#endif
#ifndef XtRCardinal
#define XtRCardinal "Cardinal"
#endif

#ifndef XtNcornerheight
#define XtNcornerheight "cornerheight"
#endif
#ifndef XtCCornerheight
#define XtCCornerheight "Cornerheight"
#endif
#ifndef XtRCardinal
#define XtRCardinal "Cardinal"
#endif

#ifndef XtNtextmargin
#define XtNtextmargin "textmargin"
#endif
#ifndef XtCTextmargin
#define XtCTextmargin "Textmargin"
#endif
#ifndef XtRInt
#define XtRInt "Int"
#endif

#ifndef XtNtabcolor
#define XtNtabcolor "tabcolor"
#endif
#ifndef XtCTabcolor
#define XtCTabcolor "Tabcolor"
#endif
#ifndef XtRPixel
#define XtRPixel "Pixel"
#endif

#ifndef XtNfont
#define XtNfont "font"
#endif
#ifndef XtCFont
#define XtCFont "Font"
#endif
#ifndef XtRFontStruct
#define XtRFontStruct "FontStruct"
#endif

#ifndef XtNactivateCallback
#define XtNactivateCallback "activateCallback"
#endif
#ifndef XtCActivateCallback
#define XtCActivateCallback "ActivateCallback"
#endif
#ifndef XtRCallback
#define XtRCallback "Callback"
#endif

typedef struct _XmTabsClassRec *XmTabsWidgetClass;
typedef struct _XmTabsRec *XmTabsWidget;
#endif /*_XmTabs_H_*/


/* ***********************************************************************
 * "XmTabsP.h"
 * ***********************************************************************
 */

/* Generated by wbuild from "XmTabs.w"
** (generator version Revision: 8.5 of Date: 2001/06/25 15:19:28)
*/
#ifndef _XmTabsP_H_
#define _XmTabsP_H_

/* raz modified 22 Jul 96 for bluestone */
#include <Xm/XmP.h>
#if ! defined(MGR_ShadowThickness)
#include <Xm/ManagerP.h>
#endif

typedef void (*border_highlight_Proc)(
#if NeedFunctionPrototypes
void
#endif
);
#define XtInherit_border_highlight ((border_highlight_Proc) _XtInherit)
typedef void (*border_unhighlight_Proc)(
#if NeedFunctionPrototypes
void
#endif
);
#define XtInherit_border_unhighlight ((border_unhighlight_Proc) _XtInherit)
typedef struct {
/* Constraint resources */
/* Private constraint variables */
int dummy;
} XmTabsConstraintPart;

typedef struct _XmTabsConstraintRec {
XmManagerConstraintPart xmManager;
XmTabsConstraintPart xmTabs;
} XmTabsConstraintRec;


typedef struct {
/* methods */
border_highlight_Proc border_highlight;
border_unhighlight_Proc border_unhighlight;
/* class variables */
} XmTabsClassPart;

typedef struct _XmTabsClassRec {
CoreClassPart core_class;
CompositeClassPart composite_class;
ConstraintClassPart constraint_class;
XmManagerClassPart xmManager_class;
XmTabsClassPart xmTabs_class;
} XmTabsClassRec;

typedef struct {
/* resources */
TabsOrientation  orientation;
int  lefttabs;
int  righttabs;
StringArray  labels;
int  tabWidthPercentage;
Cardinal  cornerwidth;
Cardinal  cornerheight;
int  textmargin;
Pixel  tabcolor;
XFontStruct * font;
XtCallbackList  activateCallback;
/* private state */
GC  textgc;
GC  topgc;
GC  bottomgc;
GC  backgc;
GC  fillgc;
int * tabwidths;
int * offsets;
} XmTabsPart;

typedef struct _XmTabsRec {
CorePart core;
CompositePart composite;
ConstraintPart constraint;
XmManagerPart xmManager;
XmTabsPart xmTabs;
} XmTabsRec;

#endif /* _XmTabsP_H_ */


/* ***********************************************************************
 * A motif widget for tabbed windows
 * ***********************************************************************
 */

static void activate(
#if NeedFunctionPrototypes
Widget,XEvent*,String*,Cardinal*
#endif
);

static XtActionsRec actionsList[] = {
{"activate", activate},
};

static char defaultTranslations[] = "\
<Btn1Down>,<Btn1Up>: activate() \n\
";
static void _resolve_inheritance(
#if NeedFunctionPrototypes
WidgetClass
#endif
);
static void class_initialize(
#if NeedFunctionPrototypes
void
#endif
);
static void initialize(
#if NeedFunctionPrototypes
Widget ,Widget,ArgList ,Cardinal *
#endif
);
static Boolean  set_values(
#if NeedFunctionPrototypes
Widget ,Widget ,Widget,ArgList ,Cardinal *
#endif
);
static void realize(
#if NeedFunctionPrototypes
Widget,XtValueMask *,XSetWindowAttributes *
#endif
);
static void resize(
#if NeedFunctionPrototypes
Widget
#endif
);
static void expose(
#if NeedFunctionPrototypes
Widget,XEvent *,Region 
#endif
);
static void border_highlight(
#if NeedFunctionPrototypes
void
#endif
);
static void border_unhighlight(
#if NeedFunctionPrototypes
void
#endif
);
static void destroy(
#if NeedFunctionPrototypes
Widget
#endif
);
#define min(a, b) ((a )<(b )?(a ):(b ))


#define abs(x) ((x )<0 ?-(x ):(x ))


static void compute_tabsizes(
#if NeedFunctionPrototypes
Widget
#endif
);
static void comp_hor_tab_shape(
#if NeedFunctionPrototypes
Widget,int ,XPoint  p[12],int *,int *,int *
#endif
);
static void comp_ver_tab_shape(
#if NeedFunctionPrototypes
Widget,int ,XPoint  p[12],int *,int *,int *
#endif
);
static void draw_border(
#if NeedFunctionPrototypes
Widget,XPoint  poly[12]
#endif
);
static void draw_hor_tab(
#if NeedFunctionPrototypes
Widget,Region ,int 
#endif
);
static void draw_ver_tab(
#if NeedFunctionPrototypes
Widget,Region ,int 
#endif
);
static void create_topgc(
#if NeedFunctionPrototypes
Widget
#endif
);
static void create_bottomgc(
#if NeedFunctionPrototypes
Widget
#endif
);
static void create_textgc(
#if NeedFunctionPrototypes
Widget
#endif
);
static void create_fillgc(
#if NeedFunctionPrototypes
Widget
#endif
);
static void create_backgc(
#if NeedFunctionPrototypes
Widget
#endif
);
static void copy_bg(
#if NeedFunctionPrototypes
Widget,int ,XrmValue *
#endif
);
static void set_shape(
#if NeedFunctionPrototypes
Widget
#endif
);
#define done(type, value) do {\
	if (to->addr != NULL) {\
	    if (to->size < sizeof(type)) {\
	        to->size = sizeof(type);\
	        return False;\
	    }\
	    *(type*)(to->addr) = (value);\
        } else {\
	    static type static_val;\
	    static_val = (value);\
	    to->addr = (XtPointer)&static_val;\
        }\
        to->size = sizeof(type);\
        return True;\
    }while (0 )


static Boolean  cvtStringToTabsOrientation(
#if NeedFunctionPrototypes
Display *,XrmValuePtr ,Cardinal *,XrmValuePtr ,XrmValuePtr ,XtPointer *
#endif
);
static Boolean  cvtTabsOrientationToString(
#if NeedFunctionPrototypes
Display *,XrmValuePtr ,Cardinal *,XrmValuePtr ,XrmValuePtr ,XtPointer *
#endif
);
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void compute_tabsizes(Widget self)
#else
static void compute_tabsizes(self)Widget self;
#endif
{
    int maxwd, basewidth, delta, i, n = ((XmTabsWidget)self)->xmTabs.lefttabs + ((XmTabsWidget)self)->xmTabs.righttabs + 1;
    int sum, len, h, length, breadth, shad = ((XmTabsWidget)self)->xmManager.shadow_thickness;

    if (((XmTabsWidget)self)->xmTabs.offsets) XtFree((XtPointer) ((XmTabsWidget)self)->xmTabs.offsets);
    if (((XmTabsWidget)self)->xmTabs.tabwidths) XtFree((XtPointer) ((XmTabsWidget)self)->xmTabs.tabwidths);
    ((XmTabsWidget)self)->xmTabs.offsets = (XtPointer) XtMalloc(n * sizeof(*((XmTabsWidget)self)->xmTabs.offsets));
    ((XmTabsWidget)self)->xmTabs.tabwidths = (XtPointer) XtMalloc(n * sizeof(*((XmTabsWidget)self)->xmTabs.tabwidths));

    if (((XmTabsWidget)self)->xmTabs.orientation == XfwfUpTabs || ((XmTabsWidget)self)->xmTabs.orientation == XfwfDownTabs) {
	length = ((XmTabsWidget)self)->core.width;
	breadth = ((XmTabsWidget)self)->core.height;
    } else {
	length = ((XmTabsWidget)self)->core.height;
	breadth = ((XmTabsWidget)self)->core.width;
    }
    if (((XmTabsWidget)self)->xmTabs.tabWidthPercentage != 0) {		/* Fixed width tabs */
	basewidth = ((XmTabsWidget)self)->xmTabs.tabWidthPercentage * length/100;
	if (n > 1) delta = (length - basewidth)/(((XmTabsWidget)self)->xmTabs.lefttabs + ((XmTabsWidget)self)->xmTabs.righttabs);
	for (i = 0; i < n; i++) {
	    ((XmTabsWidget)self)->xmTabs.tabwidths[i] = basewidth;
	    ((XmTabsWidget)self)->xmTabs.offsets[i] = i * delta;
	}
    } else if (((XmTabsWidget)self)->xmTabs.labels == NULL) {		/* Empty tabs */
	basewidth = length/n;
	delta = (length - basewidth)/(((XmTabsWidget)self)->xmTabs.lefttabs + ((XmTabsWidget)self)->xmTabs.righttabs);
	for (i = 0; i < n; i++) {
	    ((XmTabsWidget)self)->xmTabs.tabwidths[i] = basewidth;
	    ((XmTabsWidget)self)->xmTabs.offsets[i] = i * delta;
	}
    } else {					/* Variable width tabs */
	sum = 0;
	h = 2 * (((XmTabsWidget)self)->xmTabs.cornerwidth + shad + ((XmTabsWidget)self)->xmTabs.textmargin);
	maxwd = length - n * (shad + ((XmTabsWidget)self)->xmTabs.textmargin);
	for (i = 0; i < n; i++) {
	    len = strlen(((XmTabsWidget)self)->xmTabs.labels[i]);
	    ((XmTabsWidget)self)->xmTabs.tabwidths[i] = min(maxwd, XTextWidth(((XmTabsWidget)self)->xmTabs.font,((XmTabsWidget)self)->xmTabs.labels[i],len) + h);
	    sum += ((XmTabsWidget)self)->xmTabs.tabwidths[i];
	}
	((XmTabsWidget)self)->xmTabs.offsets[0] = 0;
	if (length >= sum)
	    delta = (length - sum)/(n - 1);	/* Between tabs */
	else
	    delta = -((sum - length + n - 2)/(n - 1)); /* Round down! */
	for (i = 1; i < n; i++)
	    ((XmTabsWidget)self)->xmTabs.offsets[i] = ((XmTabsWidget)self)->xmTabs.offsets[i-1] + ((XmTabsWidget)self)->xmTabs.tabwidths[i-1] + delta;
    }
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void comp_hor_tab_shape(Widget self,int  i,XPoint  p[12],int * x0,int * x1,int * midy)
#else
static void comp_hor_tab_shape(self,i,p,x0,x1,midy)Widget self;int  i;XPoint  p[12];int * x0;int * x1;int * midy;
#endif
{
    int shad = ((XmTabsWidget)self)->xmManager.shadow_thickness;
    int k = min(((XmTabsWidget)self)->xmTabs.cornerheight, (((XmTabsWidget)self)->core.height - shad)/2);
    /*
     *                4 o-------------o 5
     *                 /               \
     *              3 o                 o 6
     *                |                 |
     *              2 o                 o 7
     *             1 /                   \ 8
     *   0 o--------o                     o--------o 9
     *  11 o---------------------------------------o 10
     *
     *  11 o---------------------------------------o 10
     *   0 o--------o                     o--------o 9
     *             1 \                   / 8
     *              2 o                 o 7
     *                |                 |
     *              3 o                 o 6
     *                 \               /
     *                4 o-------------o 5
     */
    p[0].x = 0;
    p[1].x = ((XmTabsWidget)self)->xmTabs.offsets[i];
    p[2].x = ((XmTabsWidget)self)->xmTabs.offsets[i] + ((XmTabsWidget)self)->xmTabs.cornerwidth;
    p[3].x = ((XmTabsWidget)self)->xmTabs.offsets[i] + ((XmTabsWidget)self)->xmTabs.cornerwidth;
    p[4].x = ((XmTabsWidget)self)->xmTabs.offsets[i] + 2 * ((XmTabsWidget)self)->xmTabs.cornerwidth;
    p[5].x = ((XmTabsWidget)self)->xmTabs.offsets[i] + ((XmTabsWidget)self)->xmTabs.tabwidths[i] - 2 * ((XmTabsWidget)self)->xmTabs.cornerwidth;
    p[6].x = ((XmTabsWidget)self)->xmTabs.offsets[i] + ((XmTabsWidget)self)->xmTabs.tabwidths[i] - ((XmTabsWidget)self)->xmTabs.cornerwidth;
    p[7].x = ((XmTabsWidget)self)->xmTabs.offsets[i] + ((XmTabsWidget)self)->xmTabs.tabwidths[i] - ((XmTabsWidget)self)->xmTabs.cornerwidth;
    p[8].x = ((XmTabsWidget)self)->xmTabs.offsets[i] + ((XmTabsWidget)self)->xmTabs.tabwidths[i];
    p[9].x = ((XmTabsWidget)self)->core.width;
    p[10].x = ((XmTabsWidget)self)->core.width;
    p[11].x = 0;

    if (((XmTabsWidget)self)->xmTabs.orientation == XfwfUpTabs) {
	p[0].y = ((XmTabsWidget)self)->core.height - shad;
	p[1].y = ((XmTabsWidget)self)->core.height - shad;
	p[2].y = ((XmTabsWidget)self)->core.height - shad - k;
	p[3].y = k;
	p[4].y = 0;
	p[5].y = 0;
	p[6].y = k;
	p[7].y = ((XmTabsWidget)self)->core.height - shad - k;
	p[8].y = ((XmTabsWidget)self)->core.height - shad;
	p[9].y = ((XmTabsWidget)self)->core.height - shad;
	p[10].y = ((XmTabsWidget)self)->core.height;
	p[11].y = ((XmTabsWidget)self)->core.height;
    } else {
	p[0].y = shad;
	p[1].y = shad;
	p[2].y = shad + k;
	p[3].y = ((XmTabsWidget)self)->core.height - k;
	p[4].y = ((XmTabsWidget)self)->core.height;
	p[5].y = ((XmTabsWidget)self)->core.height;
	p[6].y = ((XmTabsWidget)self)->core.height - k;
	p[7].y = shad + k;
	p[8].y = shad;
	p[9].y = shad;
	p[10].y = 0;
	p[11].y = 0;
    }
    *x0 = p[4].x;
    *x1 = p[5].x;
    *midy = (p[1].y + p[4].y)/2;
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void comp_ver_tab_shape(Widget self,int  i,XPoint  p[12],int * y0,int * y1,int * midx)
#else
static void comp_ver_tab_shape(self,i,p,y0,y1,midx)Widget self;int  i;XPoint  p[12];int * y0;int * y1;int * midx;
#endif
{
    int shad = ((XmTabsWidget)self)->xmManager.shadow_thickness;
    int k = min(((XmTabsWidget)self)->xmTabs.cornerheight, (((XmTabsWidget)self)->core.width - shad)/2);
    /*
     *       0 o_o 11  11 o_o 0
     *         | |        | |
     *       1 o |        | o 1
     *     3 2/  |        |  \2 3
     *     o-o   |        |   o-o
     *    /      |        |      \
     * 4 o       |        |       o 4
     *   |       |        |       |
     * 5 o       |        |       o 5
     *    \      |        |      /
     *     o-o   |        |   o-o
     *     6 7\  |        |  /7 6  
     *       8 o |        | o 8
     *         | |        | |
     *       9 o_o 10  10 o_o 9
     */
    if (((XmTabsWidget)self)->xmTabs.orientation == XfwfLeftTabs) {
	p[0].x = ((XmTabsWidget)self)->core.width - shad;
	p[1].x = ((XmTabsWidget)self)->core.width - shad;
	p[2].x = ((XmTabsWidget)self)->core.width - shad - k;
	p[3].x = k;
	p[4].x = 0;
	p[5].x = 0;
	p[6].x = k;
	p[7].x = ((XmTabsWidget)self)->core.width - shad - k;
	p[8].x = ((XmTabsWidget)self)->core.width - shad;
	p[9].x = ((XmTabsWidget)self)->core.width - shad;
	p[10].x = ((XmTabsWidget)self)->core.width;
	p[11].x = ((XmTabsWidget)self)->core.width;
    } else {
	p[0].x = shad;
	p[1].x = shad;
	p[2].x = shad + k;
	p[3].x = ((XmTabsWidget)self)->core.width - k;
	p[4].x = ((XmTabsWidget)self)->core.width;
	p[5].x = ((XmTabsWidget)self)->core.width;
	p[6].x = ((XmTabsWidget)self)->core.width - k;
	p[7].x = shad + k;
	p[8].x = shad;
	p[9].x = shad;
	p[10].x = 0;
	p[11].x = 0;
    }
    p[0].y = 0;
    p[1].y = ((XmTabsWidget)self)->xmTabs.offsets[i];
    p[2].y = ((XmTabsWidget)self)->xmTabs.offsets[i] + ((XmTabsWidget)self)->xmTabs.cornerwidth;
    p[3].y = ((XmTabsWidget)self)->xmTabs.offsets[i] + ((XmTabsWidget)self)->xmTabs.cornerwidth;
    p[4].y = ((XmTabsWidget)self)->xmTabs.offsets[i] + 2 * ((XmTabsWidget)self)->xmTabs.cornerwidth;
    p[5].y = ((XmTabsWidget)self)->xmTabs.offsets[i] + ((XmTabsWidget)self)->xmTabs.tabwidths[i] - 2 * ((XmTabsWidget)self)->xmTabs.cornerwidth;
    p[6].y = ((XmTabsWidget)self)->xmTabs.offsets[i] + ((XmTabsWidget)self)->xmTabs.tabwidths[i] - ((XmTabsWidget)self)->xmTabs.cornerwidth;
    p[7].y = ((XmTabsWidget)self)->xmTabs.offsets[i] + ((XmTabsWidget)self)->xmTabs.tabwidths[i] - ((XmTabsWidget)self)->xmTabs.cornerwidth;
    p[8].y = ((XmTabsWidget)self)->xmTabs.offsets[i] + ((XmTabsWidget)self)->xmTabs.tabwidths[i];
    p[9].y = ((XmTabsWidget)self)->core.height;
    p[10].y = ((XmTabsWidget)self)->core.height;
    p[11].y = 0;
    *y0 = p[4].y;
    *y1 = p[5].y;
    *midx = (p[1].x + p[4].x)/2;
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void draw_border(Widget self,XPoint  poly[12])
#else
static void draw_border(self,poly)Widget self;XPoint  poly[12];
#endif
{
    Display *dpy = XtDisplay(self);
    Window win = XtWindow(self);

    if (((XmTabsWidget)self)->xmTabs.orientation == XfwfUpTabs) {
	XDrawLines(dpy, win, ((XmTabsWidget)self)->xmTabs.topgc, poly, 6, CoordModeOrigin);
	XDrawLines(dpy, win, ((XmTabsWidget)self)->xmTabs.bottomgc, poly + 5, 4, CoordModeOrigin);
	XDrawLines(dpy, win, ((XmTabsWidget)self)->xmTabs.topgc, poly + 8, 2, CoordModeOrigin);
    } else {
	XDrawLines(dpy, win, ((XmTabsWidget)self)->xmTabs.bottomgc, poly, 2, CoordModeOrigin);
	XDrawLines(dpy, win, ((XmTabsWidget)self)->xmTabs.topgc, poly + 1, 4, CoordModeOrigin);
	XDrawLines(dpy, win, ((XmTabsWidget)self)->xmTabs.bottomgc, poly + 4, 6, CoordModeOrigin);
    }
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void draw_hor_tab(Widget self,Region  region,int  i)
#else
static void draw_hor_tab(self,region,i)Widget self;Region  region;int  i;
#endif
{
    XPoint p[12];
    Display *dpy = XtDisplay(self);
    Window win = XtWindow(self);
    Region clip;
    int x0, x1, midy;

    comp_hor_tab_shape(self, i, p, &x0, &x1, &midy);
    clip = XPolygonRegion(p, XtNumber(p), WindingRule);
    if (region) XIntersectRegion(clip, region, clip);
    if (XEmptyRegion(clip)) return;

    XSetRegion(dpy, ((XmTabsWidget)self)->xmTabs.textgc, clip);
    XSetRegion(dpy, ((XmTabsWidget)self)->xmTabs.topgc, clip);
    XSetRegion(dpy, ((XmTabsWidget)self)->xmTabs.bottomgc, clip);
    if (i == ((XmTabsWidget)self)->xmTabs.lefttabs) {
	XSetRegion(dpy, ((XmTabsWidget)self)->xmTabs.backgc, clip);
	XFillPolygon(dpy, win, ((XmTabsWidget)self)->xmTabs.backgc,
		     p, XtNumber(p), Convex, CoordModeOrigin);
    } else {
	XSetRegion(dpy, ((XmTabsWidget)self)->xmTabs.fillgc, clip);
	XFillPolygon(dpy, win, ((XmTabsWidget)self)->xmTabs.fillgc,
		     p, XtNumber(p), Convex, CoordModeOrigin);
    }
    if (((XmTabsWidget)self)->xmTabs.labels) {
	int w, y, x, len = strlen(((XmTabsWidget)self)->xmTabs.labels[i]);
	y = midy - (((XmTabsWidget)self)->xmTabs.font->ascent + ((XmTabsWidget)self)->xmTabs.font->descent)/2 + ((XmTabsWidget)self)->xmTabs.font->ascent;
	w = XTextWidth(((XmTabsWidget)self)->xmTabs.font, ((XmTabsWidget)self)->xmTabs.labels[i], len);
	if (i == ((XmTabsWidget)self)->xmTabs.lefttabs
	    || ((XmTabsWidget)self)->xmTabs.tabWidthPercentage <= 100/(((XmTabsWidget)self)->xmTabs.lefttabs + ((XmTabsWidget)self)->xmTabs.righttabs + 1))
	    x = (x0 + x1 - w)/2;		/* Centered text */
	else if (i < ((XmTabsWidget)self)->xmTabs.lefttabs)
	    x = x0 + ((XmTabsWidget)self)->xmTabs.textmargin;		/* Left aligned text */
	else
	    x = x1 - ((XmTabsWidget)self)->xmTabs.textmargin - w;		/* Right aligned text */
	XDrawString(dpy, win, ((XmTabsWidget)self)->xmTabs.textgc, x, y, ((XmTabsWidget)self)->xmTabs.labels[i], len);
    }
    draw_border(self, p);
    XDestroyRegion(clip);
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void draw_ver_tab(Widget self,Region  region,int  i)
#else
static void draw_ver_tab(self,region,i)Widget self;Region  region;int  i;
#endif
{
    Display *dpy = XtDisplay(self);
    Window win = XtWindow(self);
    XPoint p[12];
    Region clip;
    int y0, y1, midx;

    comp_ver_tab_shape(self, i, p, &y0, &y1, &midx);
    clip = XPolygonRegion(p, XtNumber(p), WindingRule);
    if (region) XIntersectRegion(clip, region, clip);
    if (XEmptyRegion(clip)) return;

    XSetRegion(dpy, ((XmTabsWidget)self)->xmTabs.textgc, clip);
    XSetRegion(dpy, ((XmTabsWidget)self)->xmTabs.topgc, clip);
    XSetRegion(dpy, ((XmTabsWidget)self)->xmTabs.bottomgc, clip);
    if (i == ((XmTabsWidget)self)->xmTabs.lefttabs) {
	XSetRegion(dpy, ((XmTabsWidget)self)->xmTabs.backgc, clip);
	XFillPolygon(dpy, win, ((XmTabsWidget)self)->xmTabs.backgc,
		     p, XtNumber(p), Convex, CoordModeOrigin);
    } else {
	XSetRegion(dpy, ((XmTabsWidget)self)->xmTabs.fillgc, clip);
	XFillPolygon(dpy, win, ((XmTabsWidget)self)->xmTabs.fillgc,
		     p, XtNumber(p), Convex, CoordModeOrigin);
    }
    if (((XmTabsWidget)self)->xmTabs.labels) {
	int y, align;
	float angle = ((XmTabsWidget)self)->xmTabs.orientation == XfwfLeftTabs ? 90.0 : -90.0;
	if (i == ((XmTabsWidget)self)->xmTabs.lefttabs
	    || ((XmTabsWidget)self)->xmTabs.tabWidthPercentage <= 100/(((XmTabsWidget)self)->xmTabs.lefttabs + ((XmTabsWidget)self)->xmTabs.righttabs + 1)) {
	    y = (y0 + y1)/2;
	    align = MCENTRE;
	} else if (i < ((XmTabsWidget)self)->xmTabs.lefttabs) {
	    y = y0 + ((XmTabsWidget)self)->xmTabs.textmargin;
	    align = ((XmTabsWidget)self)->xmTabs.orientation == XfwfLeftTabs ? MRIGHT : MLEFT;
	} else {
	    y = y1 - ((XmTabsWidget)self)->xmTabs.textmargin;
	    align = ((XmTabsWidget)self)->xmTabs.orientation == XfwfLeftTabs ? MLEFT : MRIGHT;
	}
	XRotDrawAlignedString
	    (dpy, ((XmTabsWidget)self)->xmTabs.font, angle, win, ((XmTabsWidget)self)->xmTabs.textgc, midx, y, ((XmTabsWidget)self)->xmTabs.labels[i], align);
    }
    draw_border(self, p);
    XDestroyRegion(clip);
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void create_topgc(Widget self)
#else
static void create_topgc(self)Widget self;
#endif
{
    XtGCMask mask = GCForeground | GCLineWidth;
    XGCValues values;

    if (((XmTabsWidget)self)->xmTabs.topgc != NULL) XFreeGC(XtDisplay(self), ((XmTabsWidget)self)->xmTabs.topgc);
    values.foreground = ((XmTabsWidget)self)->xmManager.top_shadow_color;
    values.line_width = 2 * ((XmTabsWidget)self)->xmManager.shadow_thickness;
    ((XmTabsWidget)self)->xmTabs.topgc = XCreateGC(XtDisplay(self), RootWindowOfScreen(XtScreen(self)),
		       mask, &values);
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void create_bottomgc(Widget self)
#else
static void create_bottomgc(self)Widget self;
#endif
{
    XtGCMask mask = GCForeground | GCLineWidth;
    XGCValues values;

    if (((XmTabsWidget)self)->xmTabs.bottomgc != NULL) XFreeGC(XtDisplay(self), ((XmTabsWidget)self)->xmTabs.bottomgc);
    values.foreground = ((XmTabsWidget)self)->xmManager.bottom_shadow_color;
    values.line_width = 2 * ((XmTabsWidget)self)->xmManager.shadow_thickness;
    ((XmTabsWidget)self)->xmTabs.bottomgc = XCreateGC(XtDisplay(self), RootWindowOfScreen(XtScreen(self)),
			  mask, &values);
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void create_textgc(Widget self)
#else
static void create_textgc(self)Widget self;
#endif
{
    XtGCMask mask = GCForeground | GCFont;
    XGCValues values;

    if (((XmTabsWidget)self)->xmTabs.textgc != NULL) XFreeGC(XtDisplay(self), ((XmTabsWidget)self)->xmTabs.textgc);
    values.foreground = ((XmTabsWidget)self)->xmManager.foreground;
    values.font = ((XmTabsWidget)self)->xmTabs.font->fid;
    ((XmTabsWidget)self)->xmTabs.textgc = XCreateGC(XtDisplay(self), RootWindowOfScreen(XtScreen(self)),
			mask, &values);
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void create_fillgc(Widget self)
#else
static void create_fillgc(self)Widget self;
#endif
{
    XtGCMask mask = GCForeground;
    XGCValues values;

    if (((XmTabsWidget)self)->xmTabs.fillgc != NULL) XFreeGC(XtDisplay(self), ((XmTabsWidget)self)->xmTabs.fillgc);
    values.foreground = ((XmTabsWidget)self)->xmTabs.tabcolor;
    ((XmTabsWidget)self)->xmTabs.fillgc = XCreateGC(XtDisplay(self), RootWindowOfScreen(XtScreen(self)),
			mask, &values);
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void create_backgc(Widget self)
#else
static void create_backgc(self)Widget self;
#endif
{
    XtGCMask mask = GCForeground;
    XGCValues values;

    if (((XmTabsWidget)self)->xmTabs.backgc != NULL) XFreeGC(XtDisplay(self), ((XmTabsWidget)self)->xmTabs.backgc);
    values.foreground = ((XmTabsWidget)self)->core.background_pixel;
    ((XmTabsWidget)self)->xmTabs.backgc = XCreateGC(XtDisplay(self), RootWindowOfScreen(XtScreen(self)),
			mask, &values);
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void copy_bg(Widget self,int  offset,XrmValue * value)
#else
static void copy_bg(self,offset,value)Widget self;int  offset;XrmValue * value;
#endif
{
    value->addr = (XtPointer) &((XmTabsWidget)self)->core.background_pixel;
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void set_shape(Widget self)
#else
static void set_shape(self)Widget self;
#endif
{
    int x0, x1, midy, y0, y1, midx, i;
    Region region, clip;
    XPoint poly[12];

    if (! XtIsRealized(self)) return;

    region = XCreateRegion();

    switch (((XmTabsWidget)self)->xmTabs.orientation) {
    case XfwfUpTabs:
    case XfwfDownTabs:
	for (i = 0; i <= ((XmTabsWidget)self)->xmTabs.lefttabs + ((XmTabsWidget)self)->xmTabs.righttabs; i++) {
	    comp_hor_tab_shape(self, i, poly, &x0, &x1, &midy);
	    clip = XPolygonRegion(poly, XtNumber(poly), WindingRule);
	    XUnionRegion(region, clip, region);
	    XDestroyRegion(clip);
	}
	break;
    case XfwfLeftTabs:
    case XfwfRightTabs:
	for (i = 0; i <= ((XmTabsWidget)self)->xmTabs.lefttabs + ((XmTabsWidget)self)->xmTabs.righttabs; i++) {
	    comp_ver_tab_shape(self, i, poly, &y0, &y1, &midx);
	    clip = XPolygonRegion(poly, XtNumber(poly), WindingRule);
	    XUnionRegion(region, clip, region);
	    XDestroyRegion(clip);
	}
	break;
    }
    XShapeCombineRegion(XtDisplay(self), XtWindow(self), ShapeBounding,
			0, 0, region, ShapeSet);
    XDestroyRegion(region);
}

/*ARGSUSED*/
#if NeedFunctionPrototypes
static Boolean  cvtStringToTabsOrientation(Display * display,XrmValuePtr  args,Cardinal * num_args,XrmValuePtr  from,XrmValuePtr  to,XtPointer * converter_data)
#else
static Boolean  cvtStringToTabsOrientation(display,args,num_args,from,to,converter_data)Display * display;XrmValuePtr  args;Cardinal * num_args;XrmValuePtr  from;XrmValuePtr  to;XtPointer * converter_data;
#endif
{
    TabsOrientation a = XfwfUpTabs;
    char *s = (char*) from->addr;
    static struct {
	char *name;
	TabsOrientation orient;
    } strings[] = {
    	{ "up",  XfwfUpTabs },
    	{ "uptabs",  XfwfUpTabs },
    	{ "down",  XfwfDownTabs },
    	{ "downtabs",  XfwfDownTabs },
    	{ "left",  XfwfLeftTabs },
    	{ "lefttabs",  XfwfLeftTabs },
    	{ "right",  XfwfRightTabs },
    	{ "righttabs",  XfwfRightTabs },
    };
    int i;

    if (*num_args != 0)
	XtAppErrorMsg
	    (XtDisplayToApplicationContext(display),
	     "cvtStringToTabsOrientation", "wrongParameters", "XtToolkitError",
	     "String to TabsOrientation conversion needs no arguments",
	     (String*) NULL, (Cardinal*) NULL);

    for (i=0; i<XtNumber(strings); i++)
	if ( strcmp( s, strings[i].name ) == 0 ) {
	    a |= strings[i].orient;
	    break;
    }

    if ( i >= XtNumber(strings) )
	XtDisplayStringConversionWarning(display, s, "TabsOrientation");
    done(TabsOrientation, a);
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static Boolean  cvtTabsOrientationToString(Display * display,XrmValuePtr  args,Cardinal * num_args,XrmValuePtr  from,XrmValuePtr  to,XtPointer * converter_data)
#else
static Boolean  cvtTabsOrientationToString(display,args,num_args,from,to,converter_data)Display * display;XrmValuePtr  args;Cardinal * num_args;XrmValuePtr  from;XrmValuePtr  to;XtPointer * converter_data;
#endif
{
    TabsOrientation *a = (TabsOrientation*) from->addr;

    if (*num_args != 0)
	XtAppErrorMsg
	    (XtDisplayToApplicationContext(display),
	     "cvtTabsOrientationToString", "wrongParameters", "XtToolkitError",
	     "TabsOrientation to String conversion needs no arguments",
	     (String*) NULL, (Cardinal*) NULL);
    switch (*a) {
    case XfwfUpTabs: done(String, "up");
    case XfwfDownTabs: done(String, "down");
    case XfwfLeftTabs: done(String, "left");
    case XfwfRightTabs: done(String, "right");
    }
    XtAppErrorMsg
	(XtDisplayToApplicationContext(display),
	 "cvtTabsOrientationToString", "illParameters", "XtToolkitError",
	     "TabsOrientation to String conversion got illegal argument",
	     (String*) NULL, (Cardinal*) NULL);
    return TRUE;
}

static XtResource resources[] = {
{XtNorientation,XtCOrientation,XtRTabsOrientation,sizeof(((XmTabsRec*)NULL)->xmTabs.orientation),XtOffsetOf(XmTabsRec,xmTabs.orientation),XtRImmediate,(XtPointer)XfwfUpTabs },
{XtNlefttabs,XtCLefttabs,XtRInt,sizeof(((XmTabsRec*)NULL)->xmTabs.lefttabs),XtOffsetOf(XmTabsRec,xmTabs.lefttabs),XtRImmediate,(XtPointer)0 },
{XtNrighttabs,XtCRighttabs,XtRInt,sizeof(((XmTabsRec*)NULL)->xmTabs.righttabs),XtOffsetOf(XmTabsRec,xmTabs.righttabs),XtRImmediate,(XtPointer)0 },
{XtNlabels,XtCLabels,XtRStringArray,sizeof(((XmTabsRec*)NULL)->xmTabs.labels),XtOffsetOf(XmTabsRec,xmTabs.labels),XtRImmediate,(XtPointer)NULL },
{XtNtabWidthPercentage,XtCTabWidthPercentage,XtRInt,sizeof(((XmTabsRec*)NULL)->xmTabs.tabWidthPercentage),XtOffsetOf(XmTabsRec,xmTabs.tabWidthPercentage),XtRImmediate,(XtPointer)50 },
{XtNcornerwidth,XtCCornerwidth,XtRCardinal,sizeof(((XmTabsRec*)NULL)->xmTabs.cornerwidth),XtOffsetOf(XmTabsRec,xmTabs.cornerwidth),XtRImmediate,(XtPointer)3 },
{XtNcornerheight,XtCCornerheight,XtRCardinal,sizeof(((XmTabsRec*)NULL)->xmTabs.cornerheight),XtOffsetOf(XmTabsRec,xmTabs.cornerheight),XtRImmediate,(XtPointer)3 },
{XtNtextmargin,XtCTextmargin,XtRInt,sizeof(((XmTabsRec*)NULL)->xmTabs.textmargin),XtOffsetOf(XmTabsRec,xmTabs.textmargin),XtRImmediate,(XtPointer)3 },
{XtNtabcolor,XtCTabcolor,XtRPixel,sizeof(((XmTabsRec*)NULL)->xmTabs.tabcolor),XtOffsetOf(XmTabsRec,xmTabs.tabcolor),XtRCallProc,(XtPointer)copy_bg },
{XtNfont,XtCFont,XtRFontStruct,sizeof(((XmTabsRec*)NULL)->xmTabs.font),XtOffsetOf(XmTabsRec,xmTabs.font),XtRString,(XtPointer)XtDefaultFont },
{XtNactivateCallback,XtCActivateCallback,XtRCallback,sizeof(((XmTabsRec*)NULL)->xmTabs.activateCallback),XtOffsetOf(XmTabsRec,xmTabs.activateCallback),XtRImmediate,(XtPointer)NULL },
};

XmTabsClassRec xmTabsClassRec = {
{ /* core_class part */
/* superclass   	*/  (WidgetClass) &xmManagerClassRec,
/* class_name   	*/  "XmTabs",
/* widget_size  	*/  sizeof(XmTabsRec),
/* class_initialize 	*/  class_initialize,
/* class_part_initialize*/  _resolve_inheritance,
/* class_inited 	*/  FALSE,
/* initialize   	*/  initialize,
/* initialize_hook 	*/  NULL,
/* realize      	*/  realize,
/* actions      	*/  actionsList,
/* num_actions  	*/  1,
/* resources    	*/  resources,
/* num_resources 	*/  11,
/* xrm_class    	*/  NULLQUARK,
/* compres_motion 	*/  True ,
/* compress_exposure 	*/  XtExposeCompressMultiple ,
/* compress_enterleave 	*/  True ,
/* visible_interest 	*/  False ,
/* destroy      	*/  destroy,
/* resize       	*/  resize,
/* expose       	*/  expose,
/* set_values   	*/  set_values,
/* set_values_hook 	*/  NULL,
/* set_values_almost 	*/  XtInheritSetValuesAlmost,
/* get_values+hook 	*/  NULL,
/* accept_focus 	*/  XtInheritAcceptFocus,
/* version      	*/  XtVersion,
/* callback_private 	*/  NULL,
/* tm_table      	*/  defaultTranslations,
/* query_geometry 	*/  XtInheritQueryGeometry,
/* display_acceleator 	*/  XtInheritDisplayAccelerator,
/* extension    	*/  NULL 
},
{ /* composite_class part */
XtInheritGeometryManager,
XtInheritChangeManaged,
XtInheritInsertChild,
XtInheritDeleteChild,
NULL
},
{ /* constraint_class part */
/* constraint_resources     */  NULL,
/* num_constraint_resources */  0,
/* constraint_size          */  sizeof(XmTabsConstraintRec),
/* constraint_initialize    */  NULL,
/* constraint_destroy       */  NULL,
/* constraint_set_values    */  NULL,
/* constraint_extension     */  NULL 
},
{ /* XmManager class part */
#define manager_extension extension
/* translations                 */  XtInheritTranslations ,
/* syn_resources                */  NULL ,
/* num_syn_resources            */  0 ,
/* syn_constraint_resources     */  NULL ,
/* num_syn_constraint_resources */  0 ,
/* parent_process               */  XmInheritParentProcess,
/* manager_extension            */  NULL ,
},
{ /* XmTabs_class part */
border_highlight,
border_unhighlight,
},
};
WidgetClass xmTabsWidgetClass = (WidgetClass) &xmTabsClassRec;
/*ARGSUSED*/
static void activate(Widget self, XEvent *event, String *params, Cardinal *num_params)
{
    int x0, x1, dummy, i, x, y;
    XPoint poly[12];

    switch (((XmTabsWidget)self)->xmTabs.orientation) {
    case XfwfUpTabs:
    case XfwfDownTabs:
	x = event->xbutton.x;
	comp_hor_tab_shape(self, ((XmTabsWidget)self)->xmTabs.lefttabs, poly, &x0, &x1, &dummy);
	if (x0 <= x && x < x1) {
	    XtCallCallbackList(self, ((XmTabsWidget)self)->xmTabs.activateCallback, (XtPointer) 0);
	    return;
	}
	for (i = -1; i >= -((XmTabsWidget)self)->xmTabs.lefttabs; i--) {
	    comp_hor_tab_shape(self, i + ((XmTabsWidget)self)->xmTabs.lefttabs, poly, &x0, &x1, &dummy);
	    if (x0 <= x && x < x1) {
		XtCallCallbackList(self, ((XmTabsWidget)self)->xmTabs.activateCallback, (XtPointer) i);
		return;
	    }
	}
	for (i = 1; i <= ((XmTabsWidget)self)->xmTabs.righttabs; i++) {
	    comp_hor_tab_shape(self, i + ((XmTabsWidget)self)->xmTabs.lefttabs, poly, &x0, &x1, &dummy);
	    if (x0 <= x && x < x1) {
		XtCallCallbackList(self, ((XmTabsWidget)self)->xmTabs.activateCallback, (XtPointer) i);
		return;
	    }
	}
	break;
    case XfwfLeftTabs:
    case XfwfRightTabs:
	y = event->xbutton.y;
	comp_ver_tab_shape(self, ((XmTabsWidget)self)->xmTabs.lefttabs, poly, &x0, &x1, &dummy);
	if (x0 <= y && y < x1) {
	    XtCallCallbackList(self, ((XmTabsWidget)self)->xmTabs.activateCallback, (XtPointer) 0);
	    return;
	}
	for (i = -1; i >= -((XmTabsWidget)self)->xmTabs.lefttabs; i--) {
	    comp_ver_tab_shape(self, i + ((XmTabsWidget)self)->xmTabs.lefttabs, poly, &x0, &x1, &dummy);
	    if (x0 <= y && y < x1) {
		XtCallCallbackList(self, ((XmTabsWidget)self)->xmTabs.activateCallback, (XtPointer) i);
		return;
	    }
	}
	for (i = 1; i <= ((XmTabsWidget)self)->xmTabs.righttabs; i++) {
	    comp_ver_tab_shape(self, i + ((XmTabsWidget)self)->xmTabs.lefttabs, poly, &x0, &x1, &dummy);
	    if (x0 <= y && y < x1) {
		XtCallCallbackList(self, ((XmTabsWidget)self)->xmTabs.activateCallback, (XtPointer) i);
		return;
	    }
	}
	break;
    }
}

static void _resolve_inheritance(WidgetClass class)
{
  XmTabsWidgetClass c = (XmTabsWidgetClass) class;
  XmTabsWidgetClass super;
  static CompositeClassExtensionRec extension_rec = {
    NULL, NULLQUARK, XtCompositeExtensionVersion,
    sizeof(CompositeClassExtensionRec), True};
  CompositeClassExtensionRec *ext;
  ext = (XtPointer)XtMalloc(sizeof(*ext));
  *ext = extension_rec;
  ext->next_extension = c->composite_class.extension;
  c->composite_class.extension = ext;
  if (class == xmTabsWidgetClass) return;
  super = (XmTabsWidgetClass)class->core_class.superclass;
  if (c->xmTabs_class.border_highlight == XtInherit_border_highlight)
    c->xmTabs_class.border_highlight = super->xmTabs_class.border_highlight;
  if (c->xmTabs_class.border_unhighlight == XtInherit_border_unhighlight)
    c->xmTabs_class.border_unhighlight = super->xmTabs_class.border_unhighlight;
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void class_initialize(void)
#else
static void class_initialize(void)
#endif
{
    XtSetTypeConverter(XtRString, "StringArray",
		       cvtStringToStringArray,
		       NULL, 0, XtCacheNone, NULL);
    XtSetTypeConverter(XtRString, "TabsOrientation",
		       cvtStringToTabsOrientation,
		       NULL, 0, XtCacheNone, NULL);
    XtSetTypeConverter("TabsOrientation", XtRString,
		       cvtTabsOrientationToString,
		       NULL, 0, XtCacheNone, NULL);
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void initialize(Widget  request,Widget self,ArgList  args,Cardinal * num_args)
#else
static void initialize(request,self,args,num_args)Widget  request;Widget self;ArgList  args;Cardinal * num_args;
#endif
{
    String *h;
    int i;

    ((XmTabsWidget)self)->xmManager.traversal_on = FALSE;
    ((XmTabsWidget)self)->xmTabs.topgc = NULL;
    create_topgc(self);
    ((XmTabsWidget)self)->xmTabs.bottomgc = NULL;
    create_bottomgc(self);
    ((XmTabsWidget)self)->xmTabs.textgc = NULL;
    create_textgc(self);
    ((XmTabsWidget)self)->xmTabs.fillgc = NULL;
    create_fillgc(self);
    ((XmTabsWidget)self)->xmTabs.backgc = NULL;
    create_backgc(self);
    if (((XmTabsWidget)self)->xmTabs.labels) {
	h = (String*) XtMalloc((((XmTabsWidget)self)->xmTabs.lefttabs + ((XmTabsWidget)self)->xmTabs.righttabs + 1) * sizeof(*h));
	for (i = ((XmTabsWidget)self)->xmTabs.lefttabs + ((XmTabsWidget)self)->xmTabs.righttabs; i >= 0; i--)
	    h[i] = XtNewString(((XmTabsWidget)self)->xmTabs.labels[i]);
	((XmTabsWidget)self)->xmTabs.labels = h;
    }
    if (((XmTabsWidget)self)->xmTabs.tabWidthPercentage < 0 || ((XmTabsWidget)self)->xmTabs.tabWidthPercentage > 100) {
	XtAppWarning(XtWidgetToApplicationContext(self),
		     "tabWidthPercentage out of range; reset to 50");
	((XmTabsWidget)self)->xmTabs.tabWidthPercentage = 50;
    }
    ((XmTabsWidget)self)->xmTabs.offsets = NULL;
    ((XmTabsWidget)self)->xmTabs.tabwidths = NULL;
    compute_tabsizes(self);
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static Boolean  set_values(Widget  old,Widget  request,Widget self,ArgList  args,Cardinal * num_args)
#else
static Boolean  set_values(old,request,self,args,num_args)Widget  old;Widget  request;Widget self;ArgList  args;Cardinal * num_args;
#endif
{
    Bool redraw = FALSE, resize_labels = FALSE;
    String *h;
    int i;

    if (((XmTabsWidget)self)->core.background_pixel != ((XmTabsWidget)old)->core.background_pixel
	|| ((XmTabsWidget)self)->core.background_pixmap != ((XmTabsWidget)old)->core.background_pixmap
	|| ((XmTabsWidget)self)->xmManager.shadow_thickness != ((XmTabsWidget)old)->xmManager.shadow_thickness) {
	create_topgc(self);
	create_bottomgc(self);
	create_backgc(self);
    }
    if (((XmTabsWidget)self)->xmManager.foreground != ((XmTabsWidget)old)->xmManager.foreground || ((XmTabsWidget)self)->xmTabs.font != ((XmTabsWidget)old)->xmTabs.font) {
	create_textgc(self);
	redraw = TRUE;
    }
    if (((XmTabsWidget)self)->xmTabs.tabcolor != ((XmTabsWidget)old)->xmTabs.tabcolor) {
	create_fillgc(self);
	redraw = TRUE;
    }
    if ((((XmTabsWidget)self)->xmTabs.textmargin != ((XmTabsWidget)old)->xmTabs.textmargin && ((XmTabsWidget)self)->xmTabs.tabWidthPercentage == 0)
	|| ((XmTabsWidget)self)->xmTabs.cornerwidth != ((XmTabsWidget)old)->xmTabs.cornerwidth
	|| ((XmTabsWidget)self)->xmTabs.cornerheight != ((XmTabsWidget)old)->xmTabs.cornerheight) {
	resize_labels = TRUE;
    }
    if (((XmTabsWidget)self)->xmTabs.labels != ((XmTabsWidget)old)->xmTabs.labels) {
	if (((XmTabsWidget)self)->xmTabs.labels) {
	    h = (String*) XtMalloc((((XmTabsWidget)self)->xmTabs.lefttabs + ((XmTabsWidget)self)->xmTabs.righttabs + 1) * sizeof(*h));
	    for (i = ((XmTabsWidget)self)->xmTabs.lefttabs + ((XmTabsWidget)self)->xmTabs.righttabs; i >= 0; i--)
		h[i] = XtNewString(((XmTabsWidget)self)->xmTabs.labels[i]);
	    ((XmTabsWidget)self)->xmTabs.labels = h;
	}
	if (((XmTabsWidget)old)->xmTabs.labels) {
	    for (i = ((XmTabsWidget)old)->xmTabs.lefttabs + ((XmTabsWidget)old)->xmTabs.righttabs; i >= 0; i--)
		XtFree(((XmTabsWidget)old)->xmTabs.labels[i]);
	    XtFree((XtPointer) ((XmTabsWidget)old)->xmTabs.labels);
	}
	resize_labels = TRUE;
    }
    if (((XmTabsWidget)self)->xmTabs.tabWidthPercentage < 0 || ((XmTabsWidget)self)->xmTabs.tabWidthPercentage > 100) {
	XtAppWarning(XtWidgetToApplicationContext(self),
		     "tabWidthPercentage out of range; reset to 50");
	((XmTabsWidget)self)->xmTabs.tabWidthPercentage = 50;
    }
    if (((XmTabsWidget)old)->xmTabs.tabWidthPercentage != ((XmTabsWidget)self)->xmTabs.tabWidthPercentage)
	resize_labels = TRUE;
    if (((XmTabsWidget)self)->xmTabs.lefttabs != ((XmTabsWidget)old)->xmTabs.lefttabs || ((XmTabsWidget)self)->xmTabs.righttabs != ((XmTabsWidget)old)->xmTabs.righttabs)
	redraw = TRUE;
    if (resize_labels) {
	compute_tabsizes(self);
	redraw = TRUE;
    }
    return redraw;
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void realize(Widget self,XtValueMask * mask,XSetWindowAttributes * attributes)
#else
static void realize(self,mask,attributes)Widget self;XtValueMask * mask;XSetWindowAttributes * attributes;
#endif
{
    *mask |= CWBitGravity;
    attributes->bit_gravity = ForgetGravity;
    xmManagerClassRec.core_class.realize(self, mask, attributes);
    set_shape(self);
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void resize(Widget self)
#else
static void resize(self)Widget self;
#endif
{
    if (XtIsRealized(self))
	XClearArea(XtDisplay(self), XtWindow(self), 0, 0, 0, 0, True);
    compute_tabsizes(self);
    set_shape(self);
    if ( xmManagerClassRec.core_class.resize ) xmManagerClassRec.core_class.resize(self);
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void expose(Widget self,XEvent * event,Region  region)
#else
static void expose(self,event,region)Widget self;XEvent * event;Region  region;
#endif
{
    int i;

    if (! XtIsRealized(self)) return;

    switch (((XmTabsWidget)self)->xmTabs.orientation) {
    case XfwfUpTabs:
    case XfwfDownTabs:
	for (i = 0; i < ((XmTabsWidget)self)->xmTabs.lefttabs; i++)
	    draw_hor_tab(self, region, i);
	for (i = ((XmTabsWidget)self)->xmTabs.lefttabs + ((XmTabsWidget)self)->xmTabs.righttabs; i > ((XmTabsWidget)self)->xmTabs.lefttabs; i--)
	    draw_hor_tab(self, region, i);
	draw_hor_tab(self, region, ((XmTabsWidget)self)->xmTabs.lefttabs);
	break;
    case XfwfLeftTabs:
    case XfwfRightTabs:
	for (i = 0; i < ((XmTabsWidget)self)->xmTabs.lefttabs; i++)
	    draw_ver_tab(self, region, i);
	for (i = ((XmTabsWidget)self)->xmTabs.lefttabs + ((XmTabsWidget)self)->xmTabs.righttabs; i > ((XmTabsWidget)self)->xmTabs.lefttabs; i--)
	    draw_ver_tab(self, region, i);
	draw_ver_tab(self, region, ((XmTabsWidget)self)->xmTabs.lefttabs);
	break;
    }
    /* Focus highlight? */
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void border_highlight(void)
#else
static void border_highlight(void)
#endif
{
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void border_unhighlight(void)
#else
static void border_unhighlight(void)
#endif
{
}
/*ARGSUSED*/
#if NeedFunctionPrototypes
static void destroy(Widget self)
#else
static void destroy(self)Widget self;
#endif
{
    int i;

    if (((XmTabsWidget)self)->xmTabs.labels) {
	for (i = ((XmTabsWidget)self)->xmTabs.lefttabs + ((XmTabsWidget)self)->xmTabs.righttabs; i >= 0; i--)
	    XtFree(((XmTabsWidget)self)->xmTabs.labels[i]);
	XtFree((XtPointer) ((XmTabsWidget)self)->xmTabs.labels);
    }
    if (((XmTabsWidget)self)->xmTabs.offsets)
	XtFree((XtPointer) ((XmTabsWidget)self)->xmTabs.offsets);
    if (((XmTabsWidget)self)->xmTabs.tabwidths)
	XtFree((XtPointer) ((XmTabsWidget)self)->xmTabs.tabwidths);
}


/* ***********************************************************************
 * Routines for drawing rotated text
 * ***********************************************************************
 */

/* xvertext 5.0, Copyright (c) 1993 Alan Richardson (mppa3@uk.ac.sussex.syma)
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both the
 * copyright notice and this permission notice appear in supporting
 * documentation.  All work developed as a consequence of the use of
 * this program should duly acknowledge such use. No representations are
 * made about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * 8 Jun '95: [Bert Bos] added GCClipXOrigin|GCClipYOrigin|GCClipMask
 * when calling XCopyGC()
 */

/* ********************************************************************** */


/* BETTER: xvertext now does rotation at any angle!!
 *
 * BEWARE: function arguments have CHANGED since version 2.0!!
 */

/* ********************************************************************** */



/* ---------------------------------------------------------------------- */


/* Make sure cache size is set */

#ifndef CACHE_SIZE_LIMIT
#define CACHE_SIZE_LIMIT 0
#endif /*CACHE_SIZE_LIMIT */
    
/* Make sure a cache method is specified */

#ifndef CACHE_XIMAGES
#ifndef CACHE_BITMAPS
#define CACHE_BITMAPS
#endif /*CACHE_BITMAPS*/
#endif /*CACHE_XIMAGES*/


/* ---------------------------------------------------------------------- */


/* Debugging macros */

#ifdef DEBUG
static int debug=1;
#else
static int debug=0;
#endif /*DEBUG*/

#define DEBUG_PRINT1(a) if (debug) printf (a)
#define DEBUG_PRINT2(a, b) if (debug) printf (a, b)
#define DEBUG_PRINT3(a, b, c) if (debug) printf (a, b, c)
#define DEBUG_PRINT4(a, b, c, d) if (debug) printf (a, b, c, d)
#define DEBUG_PRINT5(a, b, c, d, e) if (debug) printf (a, b, c, d, e)


/* ---------------------------------------------------------------------- */


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


/* ---------------------------------------------------------------------- */


/* A structure holding everything needed for a rotated string */

typedef struct rotated_text_item_template {
    Pixmap bitmap;
    XImage *ximage;
    
    char *text;
    char *font_name;
    Font fid;
    float angle;
    int align;
    float magnify;
    
    int cols_in;
    int rows_in;
    int cols_out;
    int rows_out;
    
    int nl;
    int max_width;
    float *corners_x;
    float *corners_y;
    
    long int size;
    int cached;

    struct rotated_text_item_template *next;
} RotatedTextItem;

RotatedTextItem *first_text_item=NULL;


/* ---------------------------------------------------------------------- */


/* A structure holding current magnification and bounding box padding */

static struct style_template {
    float magnify;
    int bbx_pad;
} style={
    1.,
    0
    };


/* ---------------------------------------------------------------------- */


static char            *my_strdup(char *str);
static char            *my_strtok(char *str1, char *str2);

static float                   XRotVersion(char *str, int n);
static void                    XRotSetMagnification(float m);
static void                    XRotSetBoundingBoxPad(int p);
static int                     XRotDrawString(Display *dpy, XFontStruct *font, float angle, Drawable drawable, GC gc, int x, int y, char *str);
static int                     XRotDrawImageString(Display *dpy, XFontStruct *font, float angle, Drawable drawable, GC gc, int x, int y, char *str);
static int                     XRotDrawAlignedString(Display *dpy, XFontStruct *font, float angle, Drawable drawable, GC gc, int x, int y, char *text, int align);
static int                     XRotDrawAlignedImageString(Display *dpy, XFontStruct *font, float angle, Drawable drawable, GC gc, int x, int y, char *text, int align);
static XPoint                 *XRotTextExtents(Display *dpy, XFontStruct *font, float angle, int x, int y, char *text, int align);

static XImage          *MakeXImage(Display *dpy, int w, int h);
static int              XRotPaintAlignedString(Display *dpy, XFontStruct *font, float angle, Drawable drawable, GC gc, int x, int y, char *text, int align, int bg);
static int              XRotDrawHorizontalString(Display *dpy, XFontStruct *font, Drawable drawable, GC gc, int x, int y, char *text, int align, int bg);
static RotatedTextItem *XRotRetrieveFromCache(Display *dpy, XFontStruct *font, float angle, char *text, int align);
static RotatedTextItem *XRotCreateTextItem(Display *dpy, XFontStruct *font, float angle, char *text, int align);
static void             XRotAddToLinkedList(Display *dpy, RotatedTextItem *item);
static void             XRotFreeTextItem(Display *dpy, RotatedTextItem *item);
static XImage          *XRotMagnifyImage(Display *dpy, XImage *ximage);


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/* Routine to mimic `strdup()' (some machines don't have it)              */
/**************************************************************************/

static char *my_strdup(char *str)
{
    char *s;
    
    if(str==NULL)
	return NULL;
    
    s=(char *)malloc((unsigned)(strlen(str)+1));
    if(s!=NULL) 
	strcpy(s, str);
    
    return s;
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/* Routine to replace `strtok' : this one returns a zero length string if */
/* it encounters two consecutive delimiters                               */
/**************************************************************************/

static char *my_strtok(char *str1, char *str2)
{
    char *ret;
    int i, j, stop;
    static int start, len;
    static char *stext;
    
    if(str2==NULL)
	return NULL;
    
    /* initialise if str1 not NULL */
    if(str1!=NULL) {
	start=0;
	stext=str1;
	len=strlen(str1);
    }
    
    /* run out of tokens ? */
    if(start>=len)
	return NULL;
    
    /* loop through characters */
    for(i=start; i<len; i++) {
	/* loop through delimiters */
	stop=0;
	for(j=0; j<strlen(str2); j++)
	    if(stext[i]==str2[j])
		stop=1;
	
	if(stop)
	    break;
    }
    
    stext[i]='\0';
    
    ret=stext+start;
    
    start=i+1;
    
    return ret;
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/* Return version/copyright information                                   */
/**************************************************************************/
static
float XRotVersion(char *str, int n)
{
    if(str!=NULL)
	strncpy(str, XV_COPYRIGHT, n);
    return XV_VERSION;
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/* Set the font magnification factor for all subsequent operations        */
/**************************************************************************/
static
void XRotSetMagnification(float m)
{
    if(m>0.)
	style.magnify=m;
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/* Set the padding used when calculating bounding boxes                   */
/**************************************************************************/
static
void XRotSetBoundingBoxPad(int p)
{
    if(p>=0)
	style.bbx_pad=p;
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/*  Create an XImage structure and allocate memory for it                 */
/**************************************************************************/

static XImage *MakeXImage(Display *dpy, int w, int h)
{
    XImage *I;
    char *data;
    
    /* reserve memory for image */
    data=(char *)calloc((unsigned)(((w-1)/8+1)*h), 1);
    if(data==NULL)
	return NULL;
    
    /* create the XImage */
    I=XCreateImage(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), 1, XYBitmap,
                   0, data, w, h, 8, 0);
    if(I==NULL)
	return NULL;
    
    I->byte_order=I->bitmap_bit_order=MSBFirst;
    return I;
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/*  A front end to XRotPaintAlignedString:                                */
/*      -no alignment, no background                                      */
/**************************************************************************/
static
int XRotDrawString(Display *dpy, XFontStruct *font, float angle, Drawable drawable, GC gc, int x, int y, char *str)
{
    return (XRotPaintAlignedString(dpy, font, angle, drawable, gc,
				   x, y, str, NONE, 0));
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/*  A front end to XRotPaintAlignedString:                                */
/*      -no alignment, paints background                                  */
/**************************************************************************/
static
int XRotDrawImageString(Display *dpy, XFontStruct *font, float angle, Drawable drawable, GC gc, int x, int y, char *str)
{
    return(XRotPaintAlignedString(dpy, font, angle, drawable, gc,
				  x, y, str, NONE, 1));
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/*  A front end to XRotPaintAlignedString:                                */
/*      -does alignment, no background                                    */
/**************************************************************************/
static 
int XRotDrawAlignedString(Display *dpy, XFontStruct *font, float angle, Drawable drawable, GC gc, int x, int y, char *text, int align)
{
    return(XRotPaintAlignedString(dpy, font, angle, drawable, gc,
				  x, y, text, align, 0));
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/*  A front end to XRotPaintAlignedString:                                */
/*      -does alignment, paints background                                */
/**************************************************************************/
static
int XRotDrawAlignedImageString(Display *dpy, XFontStruct *font, float angle, Drawable drawable, GC gc, int x, int y, char *text, int align)
{
    return(XRotPaintAlignedString(dpy, font, angle, drawable, gc,
				  x, y, text, align, 1));
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/*  Aligns and paints a rotated string                                    */
/**************************************************************************/

static int XRotPaintAlignedString(Display *dpy, XFontStruct *font, float angle, Drawable drawable, GC gc, int x, int y, char *text, int align, int bg)
{
    int i;
    GC my_gc;
    int xp, yp;
    float hot_x, hot_y;
    float hot_xp, hot_yp;
    float sin_angle, cos_angle;
    RotatedTextItem *item;
    Pixmap bitmap_to_paint;
    
    /* return early for NULL/empty strings */
    if(text==NULL)
        return 0;
    
    if(strlen(text)==0)
	return 0;

    /* manipulate angle to 0<=angle<360 degrees */
    while(angle<0)
        angle+=360;
    
    while(angle>=360)
        angle-=360;
    
    angle*=M_PI/180;
    
    /* horizontal text made easy */
    if(angle==0. && style.magnify==1.) 
	return(XRotDrawHorizontalString(dpy, font, drawable, gc, x, y,
					text, align, bg));
    
    /* get a rotated bitmap */
    item=XRotRetrieveFromCache(dpy, font, angle, text, align);
    if(item==NULL)
	return NULL;
    
    /* this gc has similar properties to the user's gc */
    my_gc=XCreateGC(dpy, drawable, NULL, 0);
    XCopyGC(dpy, gc, GCForeground|GCBackground|GCFunction|GCPlaneMask
	    |GCClipXOrigin|GCClipYOrigin|GCClipMask, my_gc);

    /* alignment : which point (hot_x, hot_y) relative to bitmap centre
       coincides with user's specified point? */
    
    /* y position */
    if(align==TLEFT || align==TCENTRE || align==TRIGHT)
        hot_y=(float)item->rows_in/2*style.magnify;
    else if(align==MLEFT || align==MCENTRE || align==MRIGHT)
	hot_y=0;
    else if(align==BLEFT || align==BCENTRE || align==BRIGHT)
	hot_y = -(float)item->rows_in/2*style.magnify;
    else
	hot_y = -((float)item->rows_in/2-(float)font->descent)*style.magnify;
    
    /* x position */
    if(align==TLEFT || align==MLEFT || align==BLEFT || align==NONE)
	hot_x = -(float)item->max_width/2*style.magnify;
    else if(align==TCENTRE || align==MCENTRE || align==BCENTRE)
	hot_x=0;
    else
        hot_x=(float)item->max_width/2*style.magnify;
    
    /* pre-calculate sin and cos */
    sin_angle=sin(angle);
    cos_angle=cos(angle);
    
    /* rotate hot_x and hot_y around bitmap centre */
    hot_xp= hot_x*cos_angle - hot_y*sin_angle;
    hot_yp= hot_x*sin_angle + hot_y*cos_angle;
    
    /* text background will be drawn using XFillPolygon */
    if(bg) {
	GC depth_one_gc;
	XPoint *xpoints;
	Pixmap empty_stipple;
	
	/* reserve space for XPoints */
	xpoints=(XPoint *)malloc((unsigned)(4*item->nl*sizeof(XPoint)));
	if(!xpoints)
	    return 1;
	
	/* rotate corner positions */
	for(i=0; i<4*item->nl; i++) {
	    xpoints[i].x=(float)x + ( (item->corners_x[i]-hot_x)*cos_angle + 
				      (item->corners_y[i]+hot_y)*sin_angle);
	    xpoints[i].y=(float)y + (-(item->corners_x[i]-hot_x)*sin_angle + 
				      (item->corners_y[i]+hot_y)*cos_angle);
	}
	
	/* we want to swap foreground and background colors here;
	   XGetGCValues() is only available in R4+ */
	
	empty_stipple=XCreatePixmap(dpy, drawable, 1, 1, 1);
	
	depth_one_gc=XCreateGC(dpy, empty_stipple, NULL, 0);
	XSetForeground(dpy, depth_one_gc, 0);
	XFillRectangle(dpy, empty_stipple, depth_one_gc, 0, 0, 2, 2);

	XSetStipple(dpy, my_gc, empty_stipple);
	XSetFillStyle(dpy, my_gc, FillOpaqueStippled);
	
	XFillPolygon(dpy, drawable, my_gc, xpoints, 4*item->nl, Nonconvex,
		     CoordModeOrigin);
	
	/* free our resources */
	free((char *)xpoints);
	XFreeGC(dpy, depth_one_gc);
	XFreePixmap(dpy, empty_stipple);
    }
    
    /* where should top left corner of bitmap go ? */
    xp=(float)x-((float)item->cols_out/2 +hot_xp);
    yp=(float)y-((float)item->rows_out/2 -hot_yp);
    
    /* by default we draw the rotated bitmap, solid */
    bitmap_to_paint=item->bitmap;

    /* handle user stippling */
#ifndef X11R3
    {
	GC depth_one_gc;
	XGCValues values;
	Pixmap new_bitmap, inverse;
	
	/* try and get some GC properties */
	if(XGetGCValues(dpy, gc, 
			GCStipple|GCFillStyle|GCForeground|GCBackground|
			GCTileStipXOrigin|GCTileStipYOrigin,
			&values)) {

	    /* only do this if stippling requested */
	    if((values.fill_style==FillStippled ||
		values.fill_style==FillOpaqueStippled) && !bg) {

		/* opaque stipple: draw rotated text in background colour */
		if(values.fill_style==FillOpaqueStippled) {
		    XSetForeground(dpy, my_gc, values.background);
		    XSetFillStyle(dpy, my_gc, FillStippled);
		    XSetStipple(dpy, my_gc, item->bitmap);
		    XSetTSOrigin(dpy, my_gc, xp, yp);
		    XFillRectangle(dpy, drawable, my_gc, xp, yp,
				   item->cols_out, item->rows_out);
		    XSetForeground(dpy, my_gc, values.foreground);
		}

		/* this will merge the rotated text and the user's stipple */
		new_bitmap=XCreatePixmap(dpy, drawable,
					 item->cols_out, item->rows_out, 1);

		/* create a GC */
		depth_one_gc=XCreateGC(dpy, new_bitmap, NULL, 0);
		XSetForeground(dpy, depth_one_gc, 1);
		XSetBackground(dpy, depth_one_gc, 0);

		/* set the relative stipple origin */
		XSetTSOrigin(dpy, depth_one_gc, 
			     values.ts_x_origin-xp, values.ts_y_origin-yp);

		/* fill the whole bitmap with the user's stipple */
		XSetStipple(dpy, depth_one_gc, values.stipple);
		XSetFillStyle(dpy, depth_one_gc, FillOpaqueStippled);
		XFillRectangle(dpy, new_bitmap, depth_one_gc,
			       0, 0, item->cols_out, item->rows_out);

		/* set stipple origin back to normal */
		XSetTSOrigin(dpy, depth_one_gc, 0, 0);

		/* this will contain an inverse copy of the rotated text */
		inverse=XCreatePixmap(dpy, drawable,
				      item->cols_out, item->rows_out, 1);

		/* invert text */
		XSetFillStyle(dpy, depth_one_gc, FillSolid);
		XSetFunction(dpy, depth_one_gc, GXcopyInverted);
		XCopyArea(dpy, item->bitmap, inverse, depth_one_gc,
			  0, 0, item->cols_out, item->rows_out, 0, 0);

		/* now delete user's stipple everywhere EXCEPT on text */
                XSetForeground(dpy, depth_one_gc, 0);
                XSetBackground(dpy, depth_one_gc, 1);
		XSetStipple(dpy, depth_one_gc, inverse);
		XSetFillStyle(dpy, depth_one_gc, FillStippled);
		XSetFunction(dpy, depth_one_gc, GXcopy);
		XFillRectangle(dpy, new_bitmap, depth_one_gc,
                               0, 0, item->cols_out, item->rows_out);

		/* free resources */
		XFreePixmap(dpy, inverse);
		XFreeGC(dpy, depth_one_gc);

		/* this is the new bitmap */
		bitmap_to_paint=new_bitmap;
	    }
	}
    }
#endif /*X11R3*/

    /* paint text using stipple technique */
    XSetFillStyle(dpy, my_gc, FillStippled);
    XSetStipple(dpy, my_gc, bitmap_to_paint);
    XSetTSOrigin(dpy, my_gc, xp, yp);
    XFillRectangle(dpy, drawable, my_gc, xp, yp, 
		   item->cols_out, item->rows_out);
    
    /* free our resources */
    XFreeGC(dpy, my_gc);

    /* stippled bitmap no longer needed */
    if(bitmap_to_paint!=item->bitmap)
	XFreePixmap(dpy, bitmap_to_paint);

#ifdef CACHE_XIMAGES
    XFreePixmap(dpy, item->bitmap);
#endif /*CACHE_XIMAGES*/

    /* if item isn't cached, destroy it completely */
    if(!item->cached) 
	XRotFreeTextItem(dpy,item);

    /* we got to the end OK! */
    return 0;
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/*  Draw a horizontal string in a quick fashion                           */
/**************************************************************************/

static int XRotDrawHorizontalString(Display *dpy, XFontStruct *font, Drawable drawable, GC gc, int x, int y, char *text, int align, int bg)
{
    GC my_gc;
    int nl=1, i;
    int height;
    int xp, yp;
    char *str1, *str2, *str3;
    char *str2_a="\0", *str2_b="\n\0";
    int dir, asc, desc;
    XCharStruct overall;

    DEBUG_PRINT1("**\nHorizontal text.\n");

    /* this gc has similar properties to the user's gc (including stipple) */
    my_gc=XCreateGC(dpy, drawable, NULL, 0);
    XCopyGC(dpy, gc,
	    GCForeground|GCBackground|GCFunction|GCStipple|GCFillStyle|
	    GCTileStipXOrigin|GCTileStipYOrigin|GCPlaneMask|
	    GCClipXOrigin|GCClipYOrigin|GCClipMask, my_gc);
    XSetFont(dpy, my_gc, font->fid);
	
    /* count number of sections in string */
    if(align!=NONE)
	for(i=0; i<strlen(text)-1; i++)
	    if(text[i]=='\n')
		nl++;
    
    /* ignore newline characters if not doing alignment */
    if(align==NONE)
	str2=str2_a;
    else
	str2=str2_b;
    
    /* overall font height */
    height=font->ascent+font->descent;
    
    /* y position */
    if(align==TLEFT || align==TCENTRE || align==TRIGHT)
	yp=y+font->ascent;
    else if(align==MLEFT || align==MCENTRE || align==MRIGHT)
	yp=y-nl*height/2+font->ascent;
    else if(align==BLEFT || align==BCENTRE || align==BRIGHT)
	yp=y-nl*height+font->ascent;
    else
	yp=y;
    
    str1=my_strdup(text);
    if(str1==NULL)
	return 1;
    
    str3=my_strtok(str1, str2);
    
    /* loop through each section in the string */
    do {
        XTextExtents(font, str3, strlen(str3), &dir, &asc, &desc,
                     &overall);

	/* where to draw section in x ? */
	if(align==TLEFT || align==MLEFT || align==BLEFT || align==NONE)
	    xp=x;
	else if(align==TCENTRE || align==MCENTRE || align==BCENTRE)
	    xp=x-overall.rbearing/2;
	else
	    xp=x-overall.rbearing;
	
	/* draw string onto bitmap */
	if(!bg)
	    XDrawString(dpy, drawable, my_gc, xp, yp, str3, strlen(str3));
	else
	    XDrawImageString(dpy, drawable, my_gc, xp, yp, str3, strlen(str3));
	
	/* move to next line */
	yp+=height;
	
	str3=my_strtok((char *)NULL, str2);
    }
    while(str3!=NULL);
    
    free(str1);
    XFreeGC(dpy, my_gc);

    return 0;
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/*   Query cache for a match with this font/text/angle/alignment          */
/*       request, otherwise arrange for its creation                      */
/**************************************************************************/

static RotatedTextItem *XRotRetrieveFromCache(Display *dpy, XFontStruct *font, float angle, char *text, int align)
{
    Font fid;
    char *font_name=NULL;
    unsigned long name_value;
    RotatedTextItem *item=NULL;
    RotatedTextItem *i1=first_text_item;
    
    /* get font name, if it exists */
    if(XGetFontProperty(font, XA_FONT, &name_value)) {
	DEBUG_PRINT1("got font name OK\n");
	font_name=XGetAtomName(dpy, name_value);
	fid=0;
    }
#ifdef CACHE_FID
    /* otherwise rely (unreliably?) on font ID */
    else {
	DEBUG_PRINT1("can't get fontname, caching FID\n");
	font_name=NULL;
	fid=font->fid;
    }
#else
    /* not allowed to cache font ID's */
    else {
	DEBUG_PRINT1("can't get fontname, can't cache\n");
	font_name=NULL;
	fid=0;
    }
#endif /*CACHE_FID*/
    
    /* look for a match in cache */

    /* matching formula:
       identical text;
       identical fontname (if defined, font ID's if not);
       angles close enough (<0.00001 here, could be smaller);
       HORIZONTAL alignment matches, OR it's a one line string;
       magnifications the same */

    while(i1 && !item) {
	/* match everything EXCEPT fontname/ID */
	if(strcmp(text, i1->text)==0 &&
	   fabs(angle-i1->angle)<0.00001 &&
	   style.magnify==i1->magnify &&
	   (i1->nl==1 ||
	    ((align==0)?9:(align-1))%3==
	      ((i1->align==0)?9:(i1->align-1))%3)) {

	    /* now match fontname/ID */
	    if(font_name!=NULL && i1->font_name!=NULL) {
		if(strcmp(font_name, i1->font_name)==0) {
		    item=i1;
		    DEBUG_PRINT1("Matched against font names\n");
		}
		else
		    i1=i1->next;
	    }
#ifdef CACHE_FID
	    else if(font_name==NULL && i1->font_name==NULL) {
		if(fid==i1->fid) {
		    item=i1;
		    DEBUG_PRINT1("Matched against FID's\n");
                }
		else
                    i1=i1->next;
	    }
#endif /*CACHE_FID*/
	    else
		i1=i1->next;
	}
	else
	    i1=i1->next;
    }
    
    if(item)
	DEBUG_PRINT1("**\nFound target in cache.\n");
    if(!item)
	DEBUG_PRINT1("**\nNo match in cache.\n");

    /* no match */
    if(!item) {
	/* create new item */
	item=XRotCreateTextItem(dpy, font, angle, text, align);
	if(!item)
	    return NULL;

	/* record what it shows */
	item->text=my_strdup(text);

	/* fontname or ID */
	if(font_name!=NULL) {
	    item->font_name=my_strdup(font_name);
	    item->fid=0;
	}
	else {
	    item->font_name=NULL;
	    item->fid=fid;
	}

	item->angle=angle;
	item->align=align;
	item->magnify=style.magnify;

	/* cache it */
	XRotAddToLinkedList(dpy, item);
    }

    if(font_name)
	XFree(font_name);

    /* if XImage is cached, need to recreate the bitmap */

#ifdef CACHE_XIMAGES
    {
	GC depth_one_gc;

	/* create bitmap to hold rotated text */
	item->bitmap=XCreatePixmap(dpy, DefaultRootWindow(dpy),
				   item->cols_out, item->rows_out, 1);
	
	/* depth one gc */
	depth_one_gc=XCreateGC(dpy, item->bitmap, NULL, 0);
	XSetBackground(dpy, depth_one_gc, 0);
	XSetForeground(dpy, depth_one_gc, 1);

	/* make the text bitmap from XImage */
	XPutImage(dpy, item->bitmap, depth_one_gc, item->ximage, 0, 0, 0, 0,
		  item->cols_out, item->rows_out);

	XFreeGC(dpy, depth_one_gc);
    }
#endif /*CACHE_XIMAGES*/
    
    return item;
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/*  Create a rotated text item                                            */
/**************************************************************************/

static RotatedTextItem *XRotCreateTextItem(Display *dpy, XFontStruct *font, float angle, char *text, int align)
{
    RotatedTextItem *item=NULL;
    Pixmap canvas;
    GC font_gc;
    XImage *I_in;
    register int i, j;
    char *str1, *str2, *str3;
    char *str2_a="\0", *str2_b="\n\0";
    int height;
    int byte_w_in, byte_w_out;
    int xp, yp;
    float sin_angle, cos_angle;
    int it, jt;
    float di, dj;
    int ic=0;
    float xl, xr, xinc;
    int byte_out;
    int dir, asc, desc;
    XCharStruct overall;
    int old_cols_in=0, old_rows_in=0;
    
    /* allocate memory */
    item=(RotatedTextItem *)malloc((unsigned)sizeof(RotatedTextItem));
    if(!item)
	return NULL;
	
    /* count number of sections in string */
    item->nl=1;
    if(align!=NONE)
	for(i=0; i<strlen(text)-1; i++)
	    if(text[i]=='\n')
		item->nl++;
    
    /* ignore newline characters if not doing alignment */
    if(align==NONE)
	str2=str2_a;
    else
	str2=str2_b;
    
    /* find width of longest section */
    str1=my_strdup(text);
    if(str1==NULL)
	return NULL;
    
    str3=my_strtok(str1, str2);

    XTextExtents(font, str3, strlen(str3), &dir, &asc, &desc,
		 &overall);
    
    item->max_width=overall.rbearing;
    
    /* loop through each section */
    do {
	str3=my_strtok((char *)NULL, str2);

	if(str3!=NULL) {
	    XTextExtents(font, str3, strlen(str3), &dir, &asc, &desc,
			 &overall);

	    if(overall.rbearing>item->max_width)
		item->max_width=overall.rbearing;
	}
    }
    while(str3!=NULL);
    
    free(str1);
    
    /* overall font height */
    height=font->ascent+font->descent;
    
    /* dimensions horizontal text will have */
    item->cols_in=item->max_width;
    item->rows_in=item->nl*height;
    
    /* bitmap for drawing on */
    canvas=XCreatePixmap(dpy, DefaultRootWindow(dpy),
			 item->cols_in, item->rows_in, 1);
    
    /* create a GC for the bitmap */
    font_gc=XCreateGC(dpy, canvas, NULL, 0);
    XSetBackground(dpy, font_gc, 0);
    XSetFont(dpy, font_gc, font->fid);
    
    /* make sure the bitmap is blank */
    XSetForeground(dpy, font_gc, 0);
    XFillRectangle(dpy, canvas, font_gc, 0, 0, 
		   item->cols_in+1, item->rows_in+1);
    XSetForeground(dpy, font_gc, 1);
    
    /* pre-calculate sin and cos */
    sin_angle=sin(angle);
    cos_angle=cos(angle);
    
    /* text background will be drawn using XFillPolygon */
    item->corners_x=
	(float *)malloc((unsigned)(4*item->nl*sizeof(float)));
    if(!item->corners_x)
	return NULL;
    
    item->corners_y=
	(float *)malloc((unsigned)(4*item->nl*sizeof(float)));
    if(!item->corners_y)
	return NULL;
    
    /* draw text horizontally */
    
    /* start at top of bitmap */
    yp=font->ascent;
    
    str1=my_strdup(text);
    if(str1==NULL)
	return NULL;
    
    str3=my_strtok(str1, str2);
    
    /* loop through each section in the string */
    do {
	XTextExtents(font, str3, strlen(str3), &dir, &asc, &desc,
		&overall);

	/* where to draw section in x ? */
	if(align==TLEFT || align==MLEFT || align==BLEFT || align==NONE)
	    xp=0;
	else if(align==TCENTRE || align==MCENTRE || align==BCENTRE)
	    xp=(item->max_width-overall.rbearing)/2;
	else
            xp=item->max_width-overall.rbearing;

	/* draw string onto bitmap */
	XDrawString(dpy, canvas, font_gc, xp, yp, str3, strlen(str3));
	
	/* keep a note of corner positions of this string */
	item->corners_x[ic]=((float)xp-(float)item->cols_in/2)*style.magnify;
	item->corners_y[ic]=((float)(yp-font->ascent)-(float)item->rows_in/2)
	    *style.magnify;
	item->corners_x[ic+1]=item->corners_x[ic];
	item->corners_y[ic+1]=item->corners_y[ic]+(float)height*style.magnify;
	item->corners_x[item->nl*4-1-ic]=item->corners_x[ic]+
	    (float)overall.rbearing*style.magnify;
	item->corners_y[item->nl*4-1-ic]=item->corners_y[ic];
	item->corners_x[item->nl*4-2-ic]=
	    item->corners_x[item->nl*4-1-ic];
	item->corners_y[item->nl*4-2-ic]=item->corners_y[ic+1];
	
	ic+=2;
	
	/* move to next line */
	yp+=height;
	
	str3=my_strtok((char *)NULL, str2);
    }
    while(str3!=NULL);
    
    free(str1);
    
    /* create image to hold horizontal text */
    I_in=MakeXImage(dpy, item->cols_in, item->rows_in);
    if(I_in==NULL)
	return NULL;
    
    /* extract horizontal text */
    XGetSubImage(dpy, canvas, 0, 0, item->cols_in, item->rows_in,
		 1, XYPixmap, I_in, 0, 0);
    I_in->format=XYBitmap;
    
    /* magnify horizontal text */
    if(style.magnify!=1.) {
	I_in=XRotMagnifyImage(dpy, I_in);

	old_cols_in=item->cols_in;
	old_rows_in=item->rows_in;
	item->cols_in=(float)item->cols_in*style.magnify;
	item->rows_in=(float)item->rows_in*style.magnify;
    }

    /* how big will rotated text be ? */
    item->cols_out=fabs((float)item->rows_in*sin_angle) +
	fabs((float)item->cols_in*cos_angle) +0.99999 +2;

    item->rows_out=fabs((float)item->rows_in*cos_angle) +
	fabs((float)item->cols_in*sin_angle) +0.99999 +2;

    if(item->cols_out%2==0)
	item->cols_out++;
    
    if(item->rows_out%2==0)
	item->rows_out++;
    
    /* create image to hold rotated text */
    item->ximage=MakeXImage(dpy, item->cols_out, item->rows_out);
    if(item->ximage==NULL)
	return NULL;
    
    byte_w_in=(item->cols_in-1)/8+1;
    byte_w_out=(item->cols_out-1)/8+1;
    
    /* we try to make this bit as fast as possible - which is why it looks
       a bit over-the-top */
    
    /* vertical distance from centre */
    dj=0.5-(float)item->rows_out/2;

    /* where abouts does text actually lie in rotated image? */
    if(angle==0 || angle==M_PI/2 || 
       angle==M_PI || angle==3*M_PI/2) {
	xl=0;
	xr=(float)item->cols_out;
	xinc=0;
    }
    else if(angle<M_PI) {
	xl=(float)item->cols_out/2+
	    (dj-(float)item->rows_in/(2*cos_angle))/
		tan(angle)-2;
	xr=(float)item->cols_out/2+
	    (dj+(float)item->rows_in/(2*cos_angle))/
		tan(angle)+2;
	xinc=1./tan(angle);
    }
    else {
	xl=(float)item->cols_out/2+
	    (dj+(float)item->rows_in/(2*cos_angle))/
		tan(angle)-2;
	xr=(float)item->cols_out/2+
	    (dj-(float)item->rows_in/(2*cos_angle))/
		tan(angle)+2;
	
	xinc=1./tan(angle);
    }

    /* loop through all relevent bits in rotated image */
    for(j=0; j<item->rows_out; j++) {
	
	/* no point re-calculating these every pass */
	di=(float)((xl<0)?0:(int)xl)+0.5-(float)item->cols_out/2;
	byte_out=(item->rows_out-j-1)*byte_w_out;
	
	/* loop through meaningful columns */
	for(i=((xl<0)?0:(int)xl); 
	    i<((xr>=item->cols_out)?item->cols_out:(int)xr); i++) {
	    
	    /* rotate coordinates */
	    it=(float)item->cols_in/2 + ( di*cos_angle + dj*sin_angle);
	    jt=(float)item->rows_in/2 - (-di*sin_angle + dj*cos_angle);
	    
            /* set pixel if required */
            if(it>=0 && it<item->cols_in && jt>=0 && jt<item->rows_in)
                if((I_in->data[jt*byte_w_in+it/8] & 128>>(it%8))>0)
                    item->ximage->data[byte_out+i/8]|=128>>i%8;
	    
	    di+=1;
	}
	dj+=1;
	xl+=xinc;
	xr+=xinc;
    }
    XDestroyImage(I_in);
    
    if(style.magnify!=1.) {
	item->cols_in=old_cols_in;
	item->rows_in=old_rows_in;
    }


#ifdef CACHE_BITMAPS

    /* create a bitmap to hold rotated text */
    item->bitmap=XCreatePixmap(dpy, DefaultRootWindow(dpy),
			       item->cols_out, item->rows_out, 1);
    
    /* make the text bitmap from XImage */
    XPutImage(dpy, item->bitmap, font_gc, item->ximage, 0, 0, 0, 0,
	      item->cols_out, item->rows_out);

    XDestroyImage(item->ximage);

#endif /*CACHE_BITMAPS*/

    XFreeGC(dpy, font_gc);
    XFreePixmap(dpy, canvas);

    return item;
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/*  Adds a text item to the end of the cache, removing as many items      */
/*      from the front as required to keep cache size below limit         */
/**************************************************************************/

static void XRotAddToLinkedList(Display *dpy, RotatedTextItem *item)
{
    
    static long int current_size=0;
    static RotatedTextItem *last=NULL;
    RotatedTextItem *i1=first_text_item, *i2=NULL;

#ifdef CACHE_BITMAPS

    /* I don't know how much memory a pixmap takes in the server -
           probably this + a bit more we can't account for */

    item->size=((item->cols_out-1)/8+1)*item->rows_out;

#else

    /* this is pretty much the size of a RotatedTextItem */

    item->size=((item->cols_out-1)/8+1)*item->rows_out +
	sizeof(XImage) + strlen(item->text) + 
	    item->nl*8*sizeof(float) + sizeof(RotatedTextItem);

    if(item->font_name!=NULL)
	item->size+=strlen(item->font_name);
    else
	item->size+=sizeof(Font);

#endif /*CACHE_BITMAPS */

#ifdef DEBUG
    /* count number of items in cache, for debugging */
    {
	int i=0;

	while(i1) {
	    i++;
	    i1=i1->next;
	}
	DEBUG_PRINT2("Cache has %d items.\n", i);
	i1=first_text_item;
    }
#endif

    DEBUG_PRINT4("current cache size=%ld, new item=%ld, limit=%ld\n",
		 current_size, item->size, CACHE_SIZE_LIMIT*1024);

    /* if this item is bigger than whole cache, forget it */
    if(item->size>CACHE_SIZE_LIMIT*1024) {
	DEBUG_PRINT1("Too big to cache\n\n");
	item->cached=0;
	return;
    }

    /* remove elements from cache as needed */
    while(i1 && current_size+item->size>CACHE_SIZE_LIMIT*1024) {

	DEBUG_PRINT2("Removed %d bytes\n", i1->size);

	if(i1->font_name!=NULL)
	    DEBUG_PRINT5("  (`%s'\n   %s\n   angle=%f align=%d)\n",
			 i1->text, i1->font_name, i1->angle, i1->align);

#ifdef CACHE_FID
	if(i1->font_name==NULL)
	    DEBUG_PRINT5("  (`%s'\n  FID=%ld\n   angle=%f align=%d)\n",
                         i1->text, i1->fid, i1->angle, i1->align);
#endif /*CACHE_FID*/

	current_size-=i1->size;

	i2=i1->next;

	/* free resources used by the unlucky item */
	XRotFreeTextItem(dpy, i1);

	/* remove it from linked list */
	first_text_item=i2;
	i1=i2;
    }

    /* add new item to end of linked list */
    if(first_text_item==NULL) {
	item->next=NULL;
	first_text_item=item;
	last=item;
    }
    else {
	item->next=NULL;
	last->next=item;
	last=item;
    }

    /* new cache size */
    current_size+=item->size;

    item->cached=1;

    DEBUG_PRINT1("Added item to cache.\n");
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/*  Free the resources used by a text item                                */
/**************************************************************************/

static void XRotFreeTextItem(Display *dpy, RotatedTextItem *item)
{
    free(item->text);

    if(item->font_name!=NULL)
	free(item->font_name);

    free((char *)item->corners_x);
    free((char *)item->corners_y);

#ifdef CACHE_BITMAPS
    XFreePixmap(dpy, item->bitmap);
#else
    XDestroyImage(item->ximage);
#endif /* CACHE_BITMAPS */

    free((char *)item);
}


/* ---------------------------------------------------------------------- */


/**************************************************************************/
/* Magnify an XImage using bilinear interpolation                         */
/**************************************************************************/

static XImage *XRotMagnifyImage(Display *dpy, XImage *ximage)
{
    int i, j;
    float x, y;
    float u,t;
    XImage *I_out;
    int cols_in, rows_in;
    int cols_out, rows_out;
    register int i2, j2;
    float z1, z2, z3, z4;
    int byte_width_in, byte_width_out;
    float mag_inv;

    /* size of input image */
    cols_in=ximage->width;
    rows_in=ximage->height;

    /* size of final image */
    cols_out=(float)cols_in*style.magnify;
    rows_out=(float)rows_in*style.magnify;

    /* this will hold final image */
    I_out=MakeXImage(dpy, cols_out, rows_out);
    if(I_out==NULL)
	return NULL;

    /* width in bytes of input, output images */
    byte_width_in=(cols_in-1)/8+1;
    byte_width_out=(cols_out-1)/8+1;

    /* for speed */
    mag_inv=1./style.magnify;

    y=0.;

    /* loop over magnified image */
    for(j2=0; j2<rows_out; j2++) {
	x=0;
	j=y;

	for(i2=0; i2<cols_out; i2++) {
	    i=x;

	    /* bilinear interpolation - where are we on bitmap ? */
	    /* right edge */
	    if(i==cols_in-1 && j!=rows_in-1) {
		t=0;
		u=y-(float)j;

		z1=(ximage->data[j*byte_width_in+i/8] & 128>>(i%8))>0;
		z2=z1;
		z3=(ximage->data[(j+1)*byte_width_in+i/8] & 128>>(i%8))>0;
		z4=z3;
	    }
	    /* top edge */
	    else if(i!=cols_in-1 && j==rows_in-1) {
		t=x-(float)i;
		u=0;

		z1=(ximage->data[j*byte_width_in+i/8] & 128>>(i%8))>0;
		z2=(ximage->data[j*byte_width_in+(i+1)/8] & 128>>((i+1)%8))>0;
		z3=z2;
		z4=z1;
	    }
	    /* top right corner */
	    else if(i==cols_in-1 && j==rows_in-1) {
		u=0;
		t=0;

		z1=(ximage->data[j*byte_width_in+i/8] & 128>>(i%8))>0;
		z2=z1;
		z3=z1;
		z4=z1;
	    }
	    /* somewhere `safe' */
	    else {
		t=x-(float)i;
		u=y-(float)j;

		z1=(ximage->data[j*byte_width_in+i/8] & 128>>(i%8))>0;
		z2=(ximage->data[j*byte_width_in+(i+1)/8] & 128>>((i+1)%8))>0;
		z3=(ximage->data[(j+1)*byte_width_in+(i+1)/8] &
		    128>>((i+1)%8))>0;
		z4=(ximage->data[(j+1)*byte_width_in+i/8] & 128>>(i%8))>0;
	    }

	    /* if interpolated value is greater than 0.5, set bit */
	    if(((1-t)*(1-u)*z1 + t*(1-u)*z2 + t*u*z3 + (1-t)*u*z4)>0.5)
		I_out->data[j2*byte_width_out+i2/8]|=128>>i2%8;

	    x+=mag_inv;
	}
	y+=mag_inv;
    }
    
    /* destroy original */
    XDestroyImage(ximage);

    /* return big image */
    return I_out;
}



/* ---------------------------------------------------------------------- */


/**************************************************************************/
/* Calculate the bounding box some text will have when painted            */
/**************************************************************************/
static
XPoint *XRotTextExtents(Display *dpy, XFontStruct *font, float angle, int x, int y, char *text, int align)
{
    register int i;
    char *str1, *str2, *str3;
    char *str2_a="\0", *str2_b="\n\0";
    int height;
    float sin_angle, cos_angle;
    int nl, max_width;
    int cols_in, rows_in;
    float hot_x, hot_y;
    XPoint *xp_in, *xp_out;
    int dir, asc, desc;
    XCharStruct overall;
    
    /* manipulate angle to 0<=angle<360 degrees */
    while(angle<0)
        angle+=360;
    
    while(angle>360)
        angle-=360;
    
    angle*=M_PI/180;
    
    /* count number of sections in string */
    nl=1;
    if(align!=NONE)
	for(i=0; i<strlen(text)-1; i++)
	    if(text[i]=='\n')
		nl++;
    
    /* ignore newline characters if not doing alignment */
    if(align==NONE)
	str2=str2_a;
    else
	str2=str2_b;
    
    /* find width of longest section */
    str1=my_strdup(text);
    if(str1==NULL)
	return NULL;
    
    str3=my_strtok(str1, str2);

    XTextExtents(font, str3, strlen(str3), &dir, &asc, &desc,
		 &overall);

    max_width=overall.rbearing;
    
    /* loop through each section */
    do {
	str3=my_strtok((char *)NULL, str2);

	if(str3!=NULL) {
	    XTextExtents(font, str3, strlen(str3), &dir, &asc, &desc,
			 &overall);

	    if(overall.rbearing>max_width)
		max_width=overall.rbearing;
	}
    }
    while(str3!=NULL);
    
    free(str1);
    
    /* overall font height */
    height=font->ascent+font->descent;
    
    /* dimensions horizontal text will have */
    cols_in=max_width;
    rows_in=nl*height;
    
    /* pre-calculate sin and cos */
    sin_angle=sin(angle);
    cos_angle=cos(angle);
    
    /* y position */
    if(align==TLEFT || align==TCENTRE || align==TRIGHT)
        hot_y=(float)rows_in/2*style.magnify;
    else if(align==MLEFT || align==MCENTRE || align==MRIGHT)
	hot_y=0;
    else if(align==BLEFT || align==BCENTRE || align==BRIGHT)
	hot_y = -(float)rows_in/2*style.magnify;
    else
	hot_y = -((float)rows_in/2-(float)font->descent)*style.magnify;
    
    /* x position */
    if(align==TLEFT || align==MLEFT || align==BLEFT || align==NONE)
	hot_x = -(float)max_width/2*style.magnify;
    else if(align==TCENTRE || align==MCENTRE || align==BCENTRE)
	hot_x=0;
    else
        hot_x=(float)max_width/2*style.magnify;
    
    /* reserve space for XPoints */
    xp_in=(XPoint *)malloc((unsigned)(5*sizeof(XPoint)));
    if(!xp_in)
	return NULL;

    xp_out=(XPoint *)malloc((unsigned)(5*sizeof(XPoint)));
    if(!xp_out)
	return NULL;

    /* bounding box when horizontal, relative to bitmap centre */
    xp_in[0].x = -(float)cols_in*style.magnify/2-style.bbx_pad;
    xp_in[0].y= (float)rows_in*style.magnify/2+style.bbx_pad;
    xp_in[1].x= (float)cols_in*style.magnify/2+style.bbx_pad;
    xp_in[1].y= (float)rows_in*style.magnify/2+style.bbx_pad;
    xp_in[2].x= (float)cols_in*style.magnify/2+style.bbx_pad;
    xp_in[2].y = -(float)rows_in*style.magnify/2-style.bbx_pad;
    xp_in[3].x = -(float)cols_in*style.magnify/2-style.bbx_pad;
    xp_in[3].y = -(float)rows_in*style.magnify/2-style.bbx_pad;
    xp_in[4].x=xp_in[0].x;
    xp_in[4].y=xp_in[0].y;
	
    /* rotate and translate bounding box */
    for(i=0; i<5; i++) {
	xp_out[i].x=(float)x + ( ((float)xp_in[i].x-hot_x)*cos_angle +
				 ((float)xp_in[i].y+hot_y)*sin_angle);
	xp_out[i].y=(float)y + (-((float)xp_in[i].x-hot_x)*sin_angle +
				 ((float)xp_in[i].y+hot_y)*cos_angle);
    }

    free((char *)xp_in);

    return xp_out;
}



/* ***********************************************************************
 * Conversion routines for the X resource manager
 * ***********************************************************************
 */

#if defined(__STDC__)
static
Boolean	strtocard(  Display *dsp,
		    XrmValue *args,
		    Cardinal *num_args,
		    XrmValue *from,
		    XrmValue *to,
		    XtPointer *unused
		    )
#else
static
Boolean	strtocard(  dsp, args, num_args, from, to, unused )
Display *dsp;
XrmValue *args;
Cardinal *num_args;
XrmValue *from;
XrmValue *to;
XtPointer *unused;
#endif
{
    static Cardinal temp;

    if ( to->addr == NULL ) {
	to->addr = (XtPointer) &temp;
	to->size = sizeof(Cardinal);
    }

    *((Cardinal *) to->addr) = atoi( from->addr );
    return True;
}


#define done_bert(type, value) \
    do {\
	if (to->addr != NULL) {\
	    if (to->size < sizeof(type)) {\
	        to->size = sizeof(type);\
	        return False;\
	    }\
	    *(type*)(to->addr) = (value);\
        } else {\
	    static type static_val;\
	    static_val = (value);\
	    to->addr = (XtPointer)&static_val;\
        }\
        to->size = sizeof(type);\
        return True;\
    } while (0)
static
Boolean cvtStringToStringArray(Display *display, XrmValuePtr args, Cardinal *num_args, XrmValuePtr from, XrmValuePtr to, XtPointer *converter_data)
{
    String t, s;
    StringArray a = NULL;
    Cardinal i;
    char delim;

    if (*num_args != 0)
	XtAppErrorMsg(XtDisplayToApplicationContext(display),
		      "cvtStringToStringArray", "wrongParameters",
		      "XtToolkitError",
		      "String to StringArray conversion needs no arguments",
		      (String*) NULL, (Cardinal*) NULL);

    delim = ((String) from->addr)[0];
    s = XtNewString((String) from->addr + 1);
    i = 0;
    while (s && *s) {
	t = strchr(s, delim);
        if (t) *t = '\0';
	a = (StringArray) XtRealloc((String) a, (i + 1) * sizeof(*a));
	a[i] = s;
	i++;
        s = t ? t + 1 : NULL;
    }
    a = (StringArray) XtRealloc((String) a, (i + 1) * sizeof(*a));
    a[i] = NULL;
    done_bert(StringArray, a);
}


/* ***********************************************************************
 * A driver for the above in the flavor of the xt utilities module
 * ***********************************************************************
 */

#define TABHT 25

typedef struct tab_data {
    Widget	form;
    int		cur,
		num_tabs;
    void	(*activate_func)();
} *TabData;


#if defined(__STDC__)
static void handle_click( Widget w, TabData td, XtPointer call_data )
#else
static void handle_click(w, td, call_data)
    Widget w;
    TabData td;
    XtPointer call_data;
#endif
{
    int tab = (int) call_data;

    /* note that the tab is relative to the current tab.
     * if tab is 0, the user clicked on the current one.
     * there is nothing to do
     */
    if (tab == 0) return;
    td->cur += tab;

    /* Change tabs.  We must manually inform the UI which tab is current  */
    XtVaSetValues( w,
		   XtNlefttabs, td->cur,
		   XtNrighttabs, td->num_tabs - td->cur - 1,
		   NULL
		   );

    (*td->activate_func)( td->form, td->cur );
}


/*
 * PUBLIC: Widget __vi_CreateTabbedFolder
 * PUBLIC:     __P((String, Widget, String, int, void (*)(Widget, int)));
 */
#if defined(__STDC__)
Widget	__vi_CreateTabbedFolder( String name,
				Widget parent,
				String tab_labels,
				int num_tabs,
				void (*activate_func)()
				)
#else
Widget	__vi_CreateTabbedFolder( name, parent, tab_labels, num_tabs, activate_func )
String	name;
String	tab_labels;
Widget	parent;
int	num_tabs;
void	(*activate_func)();
#endif
{
    Widget	tabs;
    TabData	td = (TabData) malloc( sizeof(struct tab_data) );
    int		i;

    XtAppSetTypeConverter(  XtDisplayToApplicationContext(XtDisplay(parent)),
			    XtRString,
			    XtRCardinal,
			    strtocard,
			    NULL,
			    0,
			    XtCacheNone,
			    NULL
			    );

    /* init our internal structure */
    td->cur		= 0;
    td->num_tabs	= num_tabs;
    td->activate_func	= activate_func;

    /* tabs go on the top */
    tabs = XtVaCreateManagedWidget( "tabs",
				     xmTabsWidgetClass,
				     parent,
				     XtNlefttabs,	0,
				     XtNrighttabs,	num_tabs-1,
				     XtNorientation,	XfwfUpTabs,
				     XmNtopAttachment,	XmATTACH_FORM,
				     XmNleftAttachment,	XmATTACH_FORM,
				     XmNleftOffset,	TABHT/4,
				     XmNrightAttachment,XmATTACH_FORM,
				     XmNrightOffset,	TABHT/4,
				     XmNbottomAttachment,XmATTACH_OPPOSITE_FORM,
				     XmNbottomOffset,	-TABHT,
				     XtNlabels,		tab_labels,
				     XtVaTypedArg,	XtNlabels,
							XtRString,
							tab_labels,
							strlen(tab_labels) + 1,
				     NULL
				     );

    /* add the callback */
    XtAddCallback( tabs,
		   XtNactivateCallback,
		   (XtCallbackProc) handle_click,
		   td
		   );

    /* another form to hold the controls */
    td->form = XtVaCreateWidget( "form",
				 xmFormWidgetClass,
				 parent,
				 XmNtopAttachment,	XmATTACH_WIDGET,
				 XmNtopWidget,		tabs,
				 XmNleftAttachment,	XmATTACH_FORM,
				 XmNbottomAttachment,	XmATTACH_FORM,
				 XmNrightAttachment,	XmATTACH_FORM,
				 NULL
				 );

    /* done */
    return td->form;
}
