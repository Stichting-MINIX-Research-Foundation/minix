#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <minix/com.h>
#include <stdio.h>

int main(void)
{
    printf("Running ASID allocator kernel test...\n");
    if (sys_vmctl(SELF, VMCTL_RUN_ASID_TEST, 0) != 0) {
        printf("ASID allocator test failed.\n");
        return 1;
    }

    return 0;
}

