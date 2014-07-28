#include <minix/sysutil.h>

u32_t sqrt_approx(u32_t in)
{
        int b, v = 0;
        for(b = (sizeof(in)*8)/2-1; b >= 0; b--) {
                u32_t n = v | (1UL << b);
                if(n*n <= in)
                        v = n;
        }

        return v;
}

