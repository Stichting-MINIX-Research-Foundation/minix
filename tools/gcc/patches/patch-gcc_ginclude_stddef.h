$NetBSD$

Fix build on NetBSD i386/amd64 after the ansi.h header include protection
name change.

--- gcc/ginclude/stddef.h.orig	2009-04-09 23:23:07.000000000 +0000
+++ gcc/ginclude/stddef.h
@@ -53,6 +53,11 @@ see the files COPYING3 and COPYING.RUNTI
    one less case to deal with in the following.  */
 #if defined (__BSD_NET2__) || defined (____386BSD____) || (defined (__FreeBSD__) && (__FreeBSD__ < 5)) || defined(__NetBSD__)
 #include <machine/ansi.h>
+#if !defined(_MACHINE_ANSI_H_)
+#if defined(_I386_ANSI_H_) || defined(_X86_64_ANSI_H_)
+#define _MACHINE_ANSI_H_
+#endif
+#endif
 #endif
 /* On FreeBSD 5, machine/ansi.h does not exist anymore... */
 #if defined (__FreeBSD__) && (__FreeBSD__ >= 5)
