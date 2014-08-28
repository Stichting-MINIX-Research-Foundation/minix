#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include <minix/gcov.h>

void __gcov_flush(void)
{
        /* A version of __gcov_flush for cases in which no gcc -lgcov
         * is given; i.e. non-gcc or gcc without active gcov.
         */
        ;
}

