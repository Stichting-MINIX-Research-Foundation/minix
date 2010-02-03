/*
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/glxgears/glxgears.c,v 1.3tsi Exp $ */

/*
 * This is a port of the infamous "gears" demo to straight GLX (i.e. no GLUT)
 * Port by Brian Paul  23 March 2001
 *
 * Exact timing added by Behdad Esfahbod to achieve a fixed speed regardless
 * of frame rate.  November 2003
 *
 * Printer support added by Roland Mainz <roland.mainz@nrubsig.org>. April 2004
 *
 * This version modified by Ian Smith, 30 Sept 2007, to make ubgears.
 * ubgears is cusoimised for use in the UnixBench benchmarking suite.
 * Some redundant stuff is gone, and the -time option is added.
 * Mainly it's forked so we don't use the host's version, which could change
 * from platform to platform.
 *
 * Command line options:
 *    -display         Set X11 display for output.
 *    -info            Print additional GLX information.
 *    -time <t>        Run for <t> seconds and produce a performance report.
 *    -h               Print this help page.
 *    -v               Verbose output.
 *
 */


#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <sys/time.h>
#include <sched.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265
#endif /* !M_PI */

/* Turn a NULL pointer string into an empty string */
#define NULLSTR(x) (((x)!=NULL)?(x):(""))
#define Log(x) { if(verbose) printf x; }
#define Msg(x) { printf x; }

/* Globla vars */
/* program name (from argv[0]) */
static const char *ProgramName;

/* verbose output what the program is doing */
static Bool  verbose = False;

/* time in microseconds to run for; -1 means forever. */
static int   runTime = -1;

/* Time at which start_time(void) was called. */
static struct timeval clockStart;

/* XXX this probably isn't very portable */

/* return current time (in seconds) */
static void
start_time(void)
{
   (void) gettimeofday(&clockStart, 0);
}

/*
 * return time (in microseconds) since start_time(void) was called.
 *
 * The older version of this function randomly returned negative results.
 * This version won't, up to 2000 seconds and some.
 */
static long
current_time(void)
{
   struct timeval tv;
   long secs, micros;

   (void) gettimeofday(&tv, 0);

   secs = tv.tv_sec - clockStart.tv_sec;
   micros = tv.tv_usec - clockStart.tv_usec;
   if (micros < 0) {
       --secs;
       micros += 1000000;
   }
   return secs * 1000000 + micros;
}

static
void usage(void)
{
   fprintf (stderr, "usage:  %s [options]\n", ProgramName);
   fprintf (stderr, "-display\tSet X11 display for output.\n");
   fprintf (stderr, "-info\t\tPrint additional GLX information.\n");
   fprintf (stderr, "-time t\t\tRun for t seconds and report performance.\n");
   fprintf (stderr, "-h\t\tPrint this help page.\n");
   fprintf (stderr, "-v\t\tVerbose output.\n");
   fprintf (stderr, "\n");
   exit(EXIT_FAILURE);
}


static GLfloat view_rotx = 20.0, view_roty = 30.0, view_rotz = 0.0;
static GLint gear1, gear2, gear3;
static GLfloat angle = 0.0;
static GLint speed = 60;
static GLboolean printInfo = GL_FALSE;

/*
 *
 *  Draw a gear wheel.  You'll probably want to call this function when
 *  building a display list since we do a lot of trig here.
 *
 *  Input:  inner_radius - radius of hole at center
 *          outer_radius - radius at center of teeth
 *          width - width of gear
 *          teeth - number of teeth
 *          tooth_depth - depth of tooth
 */
static void
gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
     GLint teeth, GLfloat tooth_depth)
{
   GLint i;
   GLfloat r0, r1, r2, maxr2, minr2;
   GLfloat angle, da;
   GLfloat u, v, len;

   r0 = inner_radius;
   r1 = outer_radius - tooth_depth / 2.0;
   maxr2 = r2 = outer_radius + tooth_depth / 2.0;
   minr2 = r2;

   da = 2.0 * M_PI / teeth / 4.0;

   glShadeModel(GL_FLAT);

   glNormal3f(0.0, 0.0, 1.0);

   /* draw front face */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      if (i < teeth) {
         glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
         glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                    width * 0.5);
      }
   }
   glEnd();

   /* draw front sides of teeth */
   glBegin(GL_QUADS);
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                 width * 0.5);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                 width * 0.5);
      r2 = minr2;
   }
   r2 = maxr2;
   glEnd();

   glNormal3f(0.0, 0.0, -1.0);

   /* draw back face */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      if (i < teeth) {
         glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                    -width * 0.5);
         glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      }
   }
   glEnd();

   /* draw back sides of teeth */
   glBegin(GL_QUADS);
   da = 2.0 * M_PI / teeth / 4.0;
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                 -width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                 -width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
      r2 = minr2;
   }
   r2 = maxr2;
   glEnd();

   /* draw outward faces of teeth */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i < teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;

      glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
      glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
      u = r2 * cos(angle + da) - r1 * cos(angle);
      v = r2 * sin(angle + da) - r1 * sin(angle);
      len = sqrt(u * u + v * v);
      u /= len;
      v /= len;
      glNormal3f(v, -u, 0.0);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
      glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
      glNormal3f(cos(angle + 1.5 * da), sin(angle + 1.5 * da), 0.0);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                 width * 0.5);
      glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
                 -width * 0.5);
      u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
      v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
      glNormal3f(v, -u, 0.0);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                 width * 0.5);
      glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
                 -width * 0.5);
      glNormal3f(cos(angle + 3.5 * da), sin(angle + 3.5 * da), 0.0);
      r2 = minr2;
   }
   r2 = maxr2;

   glVertex3f(r1 * cos(0), r1 * sin(0), width * 0.5);
   glVertex3f(r1 * cos(0), r1 * sin(0), -width * 0.5);

   glEnd();

   glShadeModel(GL_SMOOTH);

   /* draw inside radius cylinder */
   glBegin(GL_QUAD_STRIP);
   for (i = 0; i <= teeth; i++) {
      angle = i * 2.0 * M_PI / teeth;
      glNormal3f(-cos(angle), -sin(angle), 0.0);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
      glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
   }
   glEnd();
}


static void
draw(void)
{
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glPushMatrix();
   glRotatef(view_rotx, 1.0, 0.0, 0.0);
   glRotatef(view_roty, 0.0, 1.0, 0.0);
   glRotatef(view_rotz, 0.0, 0.0, 1.0);

   glPushMatrix();
   glTranslatef(-3.0, -2.0, 0.0);
   glRotatef(angle, 0.0, 0.0, 1.0);
   glCallList(gear1);
   glPopMatrix();

   glPushMatrix();
   glTranslatef(3.1, -2.0, 0.0);
   glRotatef(-2.0 * angle - 9.0, 0.0, 0.0, 1.0);
   glCallList(gear2);
   glPopMatrix();

   glPushMatrix();
   glTranslatef(-3.1, 4.2, 0.0);
   glRotatef(-2.0 * angle - 25.0, 0.0, 0.0, 1.0);
   glCallList(gear3);
   glPopMatrix();

   glPopMatrix();
}


/* new window size or exposure */
static void
reshape(int width, int height)
{
   GLfloat h = (GLfloat) height / (GLfloat) width;

   glViewport(0, 0, (GLint) width, (GLint) height);
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();
   /* fit width and height */
   if (h >= 1.0)
     glFrustum(-1.0, 1.0, -h, h, 5.0, 60.0);
   else
     glFrustum(-1.0/h, 1.0/h, -1.0, 1.0, 5.0, 60.0);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glTranslatef(0.0, 0.0, -40.0);
}


static void
init(void)
{
   static GLfloat pos[4] = { 5.0, 5.0, 10.0, 0.0 };
   static GLfloat red[4] = { 0.8, 0.1, 0.0, 1.0 };
   static GLfloat green[4] = { 0.0, 0.8, 0.2, 1.0 };
   static GLfloat blue[4] = { 0.2, 0.2, 1.0, 1.0 };

   glLightfv(GL_LIGHT0, GL_POSITION, pos);
   glEnable(GL_CULL_FACE);
   glEnable(GL_LIGHTING);
   glEnable(GL_LIGHT0);
   glEnable(GL_DEPTH_TEST);

   /* make the gears */
   gear1 = glGenLists(1);
   glNewList(gear1, GL_COMPILE);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, red);
   gear(1.0, 4.0, 1.0, 20, 0.7);
   glEndList();

   gear2 = glGenLists(1);
   glNewList(gear2, GL_COMPILE);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, green);
   gear(0.5, 2.0, 2.0, 10, 0.7);
   glEndList();

   gear3 = glGenLists(1);
   glNewList(gear3, GL_COMPILE);
   glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, blue);
   gear(1.3, 2.0, 0.5, 10, 0.7);
   glEndList();

   glEnable(GL_NORMALIZE);
}


/*
 * Create an RGB, double-buffered window.
 * Return the window and context handles.
 */
static void
make_window( Display *dpy, Screen *scr,
             const char *name,
             int x, int y, int width, int height,
             Window *winRet, GLXContext *ctxRet)
{
   int attrib[] = { GLX_RGBA,
                   GLX_RED_SIZE, 1,
                   GLX_GREEN_SIZE, 1,
                   GLX_BLUE_SIZE, 1,
                   GLX_DOUBLEBUFFER,
                   GLX_DEPTH_SIZE, 1,
                   None };
   int scrnum;
   XSetWindowAttributes attr;
   unsigned long mask;
   Window root;
   Window win;
   GLXContext ctx;
   XVisualInfo *visinfo;
   GLint max[2] = { 0, 0 };

   scrnum = XScreenNumberOfScreen(scr);
   root   = XRootWindow(dpy, scrnum);

   visinfo = glXChooseVisual( dpy, scrnum, attrib );
   if (!visinfo) {
      fprintf(stderr, "%s: Error: couldn't get an RGB, Double-buffered visual.\n", ProgramName);
      exit(EXIT_FAILURE);
   }

   /* window attributes */
   attr.background_pixel = 0;
   attr.border_pixel = 0;
   attr.colormap = XCreateColormap( dpy, root, visinfo->visual, AllocNone);
   attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
   mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

   win = XCreateWindow( dpy, root, x, y, width, height,
                       0, visinfo->depth, InputOutput,
                       visinfo->visual, mask, &attr );

   /* set hints and properties */
   {
      XSizeHints sizehints;
      sizehints.x = x;
      sizehints.y = y;
      sizehints.width  = width;
      sizehints.height = height;
      sizehints.flags = USSize | USPosition;
      XSetNormalHints(dpy, win, &sizehints);
      XSetStandardProperties(dpy, win, name, name,
                              None, (char **)NULL, 0, &sizehints);
   }

   ctx = glXCreateContext( dpy, visinfo, NULL, True );
   if (!ctx) {
      fprintf(stderr, "%s: Error: glXCreateContext failed.\n", ProgramName);
      exit(EXIT_FAILURE);
   }

   XFree(visinfo);

   XMapWindow(dpy, win);
   glXMakeCurrent(dpy, win, ctx);

   /* Check for maximum size supported by the GL rasterizer */
   glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max);
   if (printInfo)
      printf("GL_MAX_VIEWPORT_DIMS=%d/%d\n", (int)max[0], (int)max[1]);
   if (width > max[0] || height > max[1]) {
      fprintf(stderr, "%s: Error: Requested window size (%d/%d) larger than "
              "maximum supported by GL engine (%d/%d).\n",
              ProgramName, width, height, (int)max[0], (int)max[1]);
      exit(EXIT_FAILURE);
   }

   *winRet = win;
   *ctxRet = ctx;
}

static void
event_loop(Display *dpy, Window win)
{
   while (1) {
      /* Process interactive events */
      while (XPending(dpy) > 0) {
         XEvent event;
         XNextEvent(dpy, &event);
         switch (event.type) {
         case Expose:
            Log(("Event: Expose\n"));
            /* we'll redraw below */
            break;
         case ConfigureNotify:
            Log(("Event: ConfigureNotify\n"));
            reshape(event.xconfigure.width, event.xconfigure.height);
            break;
         }
      }

      {
         /* Time at which we started measuring. */
         static long startTime = 0;

         /* Time of the previous frame. */
         static long lastFrame = 0;

         /* Time of the previous FPS report. */
         static long lastFps = 0;

         /* Number of frames we've done. */
         static int frames = 0;

         /* Number of frames we've done in the measured run. */
         static long runFrames = 0;

         long t = current_time();
         long useconds;

         if (!lastFrame)
            lastFrame = t;
         if (!lastFps)
            lastFps = t;

         /* How many microseconds since the previous frame? */
         useconds = t - lastFrame;
         if (!useconds) /* assume 100FPS if we don't have timer */
            useconds = 10000;

         /* Calculate how far the gears need to move and redraw. */
         angle = angle + ((double)speed * useconds) / 1000000.0;
         if (angle > 360.0)
            angle = angle - 360.0; /* don't lose precision! */
         draw();
         glXSwapBuffers(dpy, win);

         /* Done this frame. */
         lastFrame = t;
         frames++;

         /* Every 5 seconds, print the FPS. */
         if (t - lastFps >= 5000000L) {
            GLfloat seconds = (t - lastFps) / 1000000.0;
            GLfloat fps = frames / seconds;

            printf("%d frames in %3.1f seconds = %6.3f FPS\n", frames, seconds,
                   fps);
            lastFps = t;
            frames = 0;

            /*
             * Set the start time now -- ie. after one report.  This
             * gives us pump-priming time before we start for real.
             */
            if (runTime > 0 && startTime == 0) {
                printf("Start timing!\n");
                startTime = t;
            }
         }

         if (startTime > 0)
             ++runFrames;

         /* If our run time is done, finish. */
         if (runTime > 0 && startTime > 0 && t - startTime > runTime) {
             double time = (double) (t - startTime) / 1000000.0;
             fprintf(stderr, "COUNT|%ld|1|fps\n", runFrames);
             fprintf(stderr, "TIME|%.1f\n", time);
             exit(0);
         }

         /* Need to give cpu away in order to get precise timing next cycle,
          * otherwise, gettimeofday would return almost the same value. */
         sched_yield();
      }
   }
}


int
main(int argc, char *argv[])
{
   Bool           use_threadsafe_api = False;
   Display       *dpy;
   Window         win;
   Screen        *screen;
   GLXContext     ctx;
   char          *dpyName            = NULL;
   int            i;
   XRectangle     winrect;

   ProgramName = argv[0];

   for (i = 1; i < argc; i++) {
      const char *arg = argv[i];
      int         len = strlen(arg);

      if (strcmp(argv[i], "-display") == 0) {
         if (++i >= argc)
            usage();
         dpyName = argv[i];
      }
      else if (strcmp(argv[i], "-info") == 0) {
         printInfo = GL_TRUE;
      }
      else if (strcmp(argv[i], "-time") == 0) {
         if (++i >= argc)
            usage();
         runTime = atoi(argv[i]) * 1000000;
      }
      else if (!strncmp("-v", arg, len)) {
         verbose   = True;
         printInfo = GL_TRUE;
      }
      else if( !strncmp("-debug_use_threadsafe_api", arg, len) )
      {
         use_threadsafe_api = True;
      }
      else if (!strcmp(argv[i], "-h")) {
         usage();
      }
      else
      {
        fprintf(stderr, "%s: Unsupported option '%s'.\n", ProgramName, argv[i]);
        usage();
      }
   }

   /* Init X threading API on demand (for debugging) */
   if( use_threadsafe_api )
   {
      if( !XInitThreads() )
      {
         fprintf(stderr, "%s: XInitThreads() failure.\n", ProgramName);
         exit(EXIT_FAILURE);
      }
   }

   dpy = XOpenDisplay(dpyName);
   if (!dpy) {
      fprintf(stderr, "%s: Error: couldn't open display '%s'\n", ProgramName, dpyName);
      return EXIT_FAILURE;
   }

   screen = XDefaultScreenOfDisplay(dpy);

   winrect.x      = 0;
   winrect.y      = 0;
   winrect.width  = 300;
   winrect.height = 300;

   Log(("Window x=%d, y=%d, width=%d, height=%d\n",
       (int)winrect.x, (int)winrect.y, (int)winrect.width, (int)winrect.height));

   make_window(dpy, screen, "ubgears", winrect.x, winrect.y, winrect.width, winrect.height, &win, &ctx);
   reshape(winrect.width, winrect.height);

   if (printInfo) {
      printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
      printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
      printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
      printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));
   }

   init();

   start_time();
   event_loop(dpy, win);

   glXDestroyContext(dpy, ctx);

   XDestroyWindow(dpy, win);
   XCloseDisplay(dpy);

   return EXIT_SUCCESS;
}

