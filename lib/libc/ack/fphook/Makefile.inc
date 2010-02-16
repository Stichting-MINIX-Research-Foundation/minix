# The ACK ANSI C compiler has an nice trick to reduce the size of programs
# that do not use floating point.  If a program uses floating point then the
# compiler generates an external reference to the label '_fp_hook'.  This makes
# the loader bring in the floating point printing and conversion routines
# '_f_print' and 'strtod' from the library libd.a.  Otherwise two dummy
# routines are found in libc.a.  (The printf and scanf need floating point
# for the %f formats, whether you use them or not.)

# fphook sources
.PATH: ${.CURDIR}/ack/fphook

CPPFLAGS.fltpr.c+=-I${.CURDIR}/stdio

SRCS+=fltpr.c strtod.c
