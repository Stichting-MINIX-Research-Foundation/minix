$NetBSD$

Fix build with gcc-4.5.

--- gas/app.c.orig	2006-03-10 10:57:18.000000000 +0000
+++ gas/app.c
@@ -563,7 +563,8 @@ do_scrub_chars (int (*get) (char *, int)
 	    {
 	      as_warn (_("end of file in string; '%c' inserted"), quotechar);
 	      state = old_state;
-	      UNGET ('\n');
+              if (from > input_buffer)
+	          UNGET ('\n');
 	      PUT (quotechar);
 	    }
 	  else if (ch == quotechar)
