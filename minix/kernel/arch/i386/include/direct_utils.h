#ifndef MB_UTILS_H
#define MB_UTILS_H

#include "kernel/kernel.h"

void direct_cls(void);
void direct_print(const char*);
void direct_print_char(char);
int direct_read_char(unsigned char*);

void direct_com_init(void);
void direct_com_print(const char*);
void direct_com_print_char(char);
int direct_com_read_char(unsigned char*);

#endif
