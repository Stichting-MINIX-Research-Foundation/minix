$NetBSD$

--- gcc/gcov.c.orig	Sat Jan  9 00:05:06 2010
+++ gcc/gcov.c
@@ -58,6 +58,10 @@ along with Gcov; see the file COPYING3.  If not see
 
 #define STRING_SIZE 200
 
+#ifdef _MINIX
+#define block_t gcc_block_t
+#endif
+
 struct function_info;
 struct block_info;
 struct source_info;
