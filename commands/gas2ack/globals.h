#ifndef __GLOBALS_H__
#define __GLOBALS_H__

int syms_is_global(const char * gl);
int syms_add(const char * gl);
int syms_add_global(const char * gl);
void syms_add_global_csl(expression_t * exp);

#endif
