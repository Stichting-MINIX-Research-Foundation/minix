
#include "kernel/kernel.h"
#include <minix/minlib.h>
#include <minix/const.h>
#include <minix/cpufeature.h>
#include <minix/type.h>
#include <minix/com.h>
#include <sys/types.h>
#include <sys/param.h>
#include <libexec.h>
#include "string.h"
#include "arch_proto.h"
#include "libexec.h"
#include "direct_utils.h"
#include "serial.h"
#include "glo.h"
#include <machine/multiboot.h>

void direct_cls(void)
{
    /* Do nothing */
}

void direct_print_char(char c)
{
	if(c == '\n')
		ser_putc('\r');
	ser_putc(c);
}

void direct_print(const char *str)
{
	while (*str) {
		direct_print_char(*str);
		str++;
	}
}

int direct_read_char(unsigned char *ch)
{
	return 0;
}
