/*
 * MXV kernel control ABI, Phase 1.
 *
 * This interface is deliberately limited to a hardware capability query,
 * VMX lifecycle management, and a kernel-owned VM-entry smoke test.  It does
 * not accept guest-supplied code, register state, physical addresses, or RAM.
 */
#ifndef _MINIX_MXVCTL_H
#define _MINIX_MXVCTL_H

#define MXVCTL_API_VERSION       1

#define MXV_QUERY_CAPS           0
#define MXV_VMXON                1
#define MXV_VMXOFF               2
#define MXV_VCPU_CREATE          3
#define MXV_VCPU_DESTROY         4
#define MXV_VCPU_RUN_PROBE       5

#define MXV_CAP_VMX              (1ULL << 0)
#define MXV_CAP_FEATURE_LOCKED   (1ULL << 1)
#define MXV_CAP_VMX_OUTSIDE_SMX  (1ULL << 2)
#define MXV_CAP_VMCS_WB          (1ULL << 3)
#define MXV_CAP_EPT              (1ULL << 4)
#define MXV_CAP_UNRESTRICTED     (1ULL << 5)
#define MXV_CAP_EPT_WB           (1ULL << 6)
#define MXV_CAP_EPT_4LEVEL       (1ULL << 7)
#define MXV_CAP_SECONDARY        (1ULL << 8)
#define MXV_CAP_INVEPT           (1ULL << 9)
#define MXV_CAP_REQUIRED         (MXV_CAP_VMX | MXV_CAP_FEATURE_LOCKED | \
                                  MXV_CAP_VMX_OUTSIDE_SMX | MXV_CAP_VMCS_WB | \
                                  MXV_CAP_EPT | MXV_CAP_UNRESTRICTED | \
                                  MXV_CAP_EPT_WB | MXV_CAP_EPT_4LEVEL | \
                                  MXV_CAP_SECONDARY | MXV_CAP_INVEPT)

/* Intel VM-exit basic reason for VMCALL. */
#define MXV_EXIT_VMCALL          18

/*
 * Phase-1 syscall message layout:
 *   m1_i1   = MXVCTL_API_VERSION
 *   m1_i2   = operation
 *   m1_ull1 = operation argument/result (opaque vCPU handle or capability mask)
 *
 * Phase 1 authorizes this call only for the root system process (RS).  A
 * dedicated mxvd privilege/capability will replace this temporary gate.
 */
#endif /* _MINIX_MXVCTL_H */
