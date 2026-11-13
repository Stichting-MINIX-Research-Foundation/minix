/*	$NetBSD: efidebug.h,v 1.1.1.2 2018/08/16 18:17:47 jmcneill Exp $	*/

#ifndef _EFI_DEBUG_H
#define _EFI_DEBUG_H

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    efidebug.h

Abstract:

    EFI library debug functions



Revision History

--*/

extern UINTN     EFIDebug;

#if EFI_DEBUG

    #define DBGASSERT(a)        DbgAssert(__FILE__, __LINE__, #a)
    #define DEBUG(a)            DbgPrint a
    
#else

    #define DBGASSERT(a)
    #define DEBUG(a)
    
#endif

#if EFI_DEBUG_CLEAR_MEMORY

    #define DBGSETMEM(a,l)      SetMem(a,l,(CHAR8)BAD_POINTER)

#else

    #define DBGSETMEM(a,l)

#endif

#define D_INIT        0x00000001          // Initialization style messages
#define D_WARN        0x00000002          // Warnings
#define D_LOAD        0x00000004          // Load events
#define D_FS          0x00000008          // EFI File system
#define D_POOL        0x00000010          // Alloc & Free's
#define D_PAGE        0x00000020          // Alloc & Free's
#define D_INFO        0x00000040          // Verbose
#define D_VAR         0x00000100          // Variable
#define D_PARSE       0x00000200          // Command parsing
#define D_BM          0x00000400          // Boot manager
#define D_BLKIO       0x00001000          // BlkIo Driver
#define D_BLKIO_ULTRA 0x00002000          // BlkIo Driver
#define D_NET         0x00004000          // SNI Driver
#define D_NET_ULTRA   0x00008000          // SNI Driver
#define D_TXTIN       0x00010000          // Simple Input Driver
#define D_TXTOUT      0x00020000          // Simple Text Output Driver
#define D_ERROR_ATA	  0x00040000		  		// ATA error messages 
#define D_ERROR       0x80000000          // Error

#define D_RESERVED    0x7fffC880          // Bits not reserved above

//
// Current Debug level of the system, value of EFIDebug
//
//#define EFI_DBUG_MASK   (D_ERROR | D_WARN | D_LOAD | D_BLKIO | D_INIT)
#define EFI_DBUG_MASK   (D_ERROR)

//
//
//

#if EFI_DEBUG

    #define ASSERT(a)               if(!(a))       DBGASSERT(a)
    #define ASSERT_LOCKED(l)        if(!(l)->Lock) DBGASSERT(l not locked)
    #define ASSERT_STRUCT(p,t)      DBGASSERT(t not structure), p

#else

    #define ASSERT(a)               
    #define ASSERT_LOCKED(l)        
    #define ASSERT_STRUCT(p,t)      

#endif

//
// Prototypes
//

INTN
DbgAssert (
    CONST CHAR8   *file,
    INTN          lineno,
    CONST CHAR8   *string
    );

INTN
DbgPrint (
    INTN          mask,
    CONST CHAR8   *format,
    ...
    );

//
// Instruction Set Architectures definitions for debuggers
//

typedef INTN EFI_EXCEPTION_TYPE;

// IA32
#define EXCEPT_IA32_DIVIDE_ERROR    0
#define EXCEPT_IA32_DEBUG           1
#define EXCEPT_IA32_NMI             2
#define EXCEPT_IA32_BREAKPOINT      3
#define EXCEPT_IA32_OVERFLOW        4
#define EXCEPT_IA32_BOUND           5
#define EXCEPT_IA32_INVALID_OPCODE  6
#define EXCEPT_IA32_DOUBLE_FAULT    8
#define EXCEPT_IA32_INVALID_TSS     10
#define EXCEPT_IA32_SEG_NOT_PRESENT 11
#define EXCEPT_IA32_STACK_FAULT     12
#define EXCEPT_IA32_GP_FAULT        13
#define EXCEPT_IA32_PAGE_FAULT      14
#define EXCEPT_IA32_FP_ERROR        16
#define EXCEPT_IA32_ALIGNMENT_CHECK 17
#define EXCEPT_IA32_MACHINE_CHECK   18
#define EXCEPT_IA32_SIMD            19

typedef struct {
    UINT16  Fcw;
    UINT16  Fsw;
    UINT16  Ftw;
    UINT16  Opcode;
    UINT32  Eip;
    UINT16  Cs;
    UINT16  Reserved1;
    UINT32  DataOffset;
    UINT16  Ds;
    UINT8   Reserved2[10];
    UINT8   St0Mm0[10], Reserved3[6];
    UINT8   St1Mm1[10], Reserved4[6];
    UINT8   St2Mm2[10], Reserved5[6];
    UINT8   St3Mm3[10], Reserved6[6];
    UINT8   St4Mm4[10], Reserved7[6];
    UINT8   St5Mm5[10], Reserved8[6];
    UINT8   St6Mm6[10], Reserved9[6];
    UINT8   St7Mm7[10], Reserved10[6];
    UINT8   Xmm0[16];
    UINT8   Xmm1[16];
    UINT8   Xmm2[16];
    UINT8   Xmm3[16];
    UINT8   Xmm4[16];
    UINT8   Xmm5[16];
    UINT8   Xmm6[16];
    UINT8   Xmm7[16];
    UINT8   Reserved11[14 * 16];
} EFI_FX_SAVE_STATE_IA32;

typedef struct {
    UINT32                 ExceptionData;
    EFI_FX_SAVE_STATE_IA32 FxSaveState;
    UINT32                 Dr0;
    UINT32                 Dr1;
    UINT32                 Dr2;
    UINT32                 Dr3;
    UINT32                 Dr6;
    UINT32                 Dr7;
    UINT32                 Cr0;
    UINT32                 Cr1;
    UINT32                 Cr2;
    UINT32                 Cr3;
    UINT32                 Cr4;
    UINT32                 Eflags;
    UINT32                 Ldtr;
    UINT32                 Tr;
    UINT32                 Gdtr[2];
    UINT32                 Idtr[2];
    UINT32                 Eip;
    UINT32                 Gs;
    UINT32                 Fs;
    UINT32                 Es;
    UINT32                 Ds;
    UINT32                 Cs;
    UINT32                 Ss;
    UINT32                 Edi;
    UINT32                 Esi;
    UINT32                 Ebp;
    UINT32                 Esp;
    UINT32                 Ebx;
    UINT32                 Edx;
    UINT32                 Ecx;
    UINT32                 Eax;
} EFI_SYSTEM_CONTEXT_IA32;

// X64
#define EXCEPT_X64_DIVIDE_ERROR    0
#define EXCEPT_X64_DEBUG           1
#define EXCEPT_X64_NMI             2
#define EXCEPT_X64_BREAKPOINT      3
#define EXCEPT_X64_OVERFLOW        4
#define EXCEPT_X64_BOUND           5
#define EXCEPT_X64_INVALID_OPCODE  6
#define EXCEPT_X64_DOUBLE_FAULT    8
#define EXCEPT_X64_INVALID_TSS     10
#define EXCEPT_X64_SEG_NOT_PRESENT 11
#define EXCEPT_X64_STACK_FAULT     12
#define EXCEPT_X64_GP_FAULT        13
#define EXCEPT_X64_PAGE_FAULT      14
#define EXCEPT_X64_FP_ERROR        16
#define EXCEPT_X64_ALIGNMENT_CHECK 17
#define EXCEPT_X64_MACHINE_CHECK   18
#define EXCEPT_X64_SIMD            19

typedef struct {
    UINT16  Fcw;
    UINT16  Fsw;
    UINT16  Ftw;
    UINT16  Opcode;
    UINT64  Rip;
    UINT64  DataOffset;
    UINT8   Reserved1[8];
    UINT8   St0Mm0[10], Reserved2[6];
    UINT8   St1Mm1[10], Reserved3[6];
    UINT8   St2Mm2[10], Reserved4[6];
    UINT8   St3Mm3[10], Reserved5[6];
    UINT8   St4Mm4[10], Reserved6[6];
    UINT8   St5Mm5[10], Reserved7[6];
    UINT8   St6Mm6[10], Reserved8[6];
    UINT8   St7Mm7[10], Reserved9[6];
    UINT8   Xmm0[16];
    UINT8   Xmm1[16];
    UINT8   Xmm2[16];
    UINT8   Xmm3[16];
    UINT8   Xmm4[16];
    UINT8   Xmm5[16];
    UINT8   Xmm6[16];
    UINT8   Xmm7[16];
    UINT8   Reserved11[14 * 16];
} EFI_FX_SAVE_STATE_X64;

typedef struct {
    UINT64                ExceptionData;
    EFI_FX_SAVE_STATE_X64 FxSaveState;
    UINT64                Dr0;
    UINT64                Dr1;
    UINT64                Dr2;
    UINT64                Dr3;
    UINT64                Dr6;
    UINT64                Dr7;
    UINT64                Cr0;
    UINT64                Cr1;
    UINT64                Cr2;
    UINT64                Cr3;
    UINT64                Cr4;
    UINT64                Cr8;
    UINT64                Rflags;
    UINT64                Ldtr;
    UINT64                Tr;
    UINT64                Gdtr[2];
    UINT64                Idtr[2];
    UINT64                Rip;
    UINT64                Gs;
    UINT64                Fs;
    UINT64                Es;
    UINT64                Ds;
    UINT64                Cs;
    UINT64                Ss;
    UINT64                Rdi;
    UINT64                Rsi;
    UINT64                Rbp;
    UINT64                Rsp;
    UINT64                Rbx;
    UINT64                Rdx;
    UINT64                Rcx;
    UINT64                Rax;
    UINT64                R8;
    UINT64                R9;
    UINT64                R10;
    UINT64                R11;
    UINT64                R12;
    UINT64                R13;
    UINT64                R14;
    UINT64                R15;
} EFI_SYSTEM_CONTEXT_X64;

/// IA64
#define EXCEPT_IPF_VHTP_TRANSLATION               0
#define EXCEPT_IPF_INSTRUCTION_TLB                1
#define EXCEPT_IPF_DATA_TLB                       2
#define EXCEPT_IPF_ALT_INSTRUCTION_TLB            3
#define EXCEPT_IPF_ALT_DATA_TLB                   4
#define EXCEPT_IPF_DATA_NESTED_TLB                5
#define EXCEPT_IPF_INSTRUCTION_KEY_MISSED         6
#define EXCEPT_IPF_DATA_KEY_MISSED                7
#define EXCEPT_IPF_DIRTY_BIT                      8
#define EXCEPT_IPF_INSTRUCTION_ACCESS_BIT         9
#define EXCEPT_IPF_DATA_ACCESS_BIT                10
#define EXCEPT_IPF_BREAKPOINT                     11
#define EXCEPT_IPF_EXTERNAL_INTERRUPT             12
#define EXCEPT_IPF_PAGE_NOT_PRESENT               20
#define EXCEPT_IPF_KEY_PERMISSION                 21
#define EXCEPT_IPF_INSTRUCTION_ACCESS_RIGHTS      22
#define EXCEPT_IPF_DATA_ACCESS_RIGHTS             23
#define EXCEPT_IPF_GENERAL_EXCEPTION              24
#define EXCEPT_IPF_DISABLED_FP_REGISTER           25
#define EXCEPT_IPF_NAT_CONSUMPTION                26
#define EXCEPT_IPF_SPECULATION                    27
#define EXCEPT_IPF_DEBUG                          29
#define EXCEPT_IPF_UNALIGNED_REFERENCE            30
#define EXCEPT_IPF_UNSUPPORTED_DATA_REFERENCE     31
#define EXCEPT_IPF_FP_FAULT                       32
#define EXCEPT_IPF_FP_TRAP                        33
#define EXCEPT_IPF_LOWER_PRIVILEGE_TRANSFER_TRAP  34
#define EXCEPT_IPF_TAKEN_BRANCH                   35
#define EXCEPT_IPF_SINGLE_STEP                    36
#define EXCEPT_IPF_IA32_EXCEPTION                 45
#define EXCEPT_IPF_IA32_INTERCEPT                 46
#define EXCEPT_IPF_IA32_INTERRUPT                 47

typedef struct {
    UINT64  Reserved;
    UINT64  R1;
    UINT64  R2;
    UINT64  R3;
    UINT64  R4;
    UINT64  R5;
    UINT64  R6;
    UINT64  R7;
    UINT64  R8;
    UINT64  R9;
    UINT64  R10;
    UINT64  R11;
    UINT64  R12;
    UINT64  R13;
    UINT64  R14;
    UINT64  R15;
    UINT64  R16;
    UINT64  R17;
    UINT64  R18;
    UINT64  R19;
    UINT64  R20;
    UINT64  R21;
    UINT64  R22;
    UINT64  R23;
    UINT64  R24;
    UINT64  R25;
    UINT64  R26;
    UINT64  R27;
    UINT64  R28;
    UINT64  R29;
    UINT64  R30;
    UINT64  R31;
    UINT64  F2[2];
    UINT64  F3[2];
    UINT64  F4[2];
    UINT64  F5[2];
    UINT64  F6[2];
    UINT64  F7[2];
    UINT64  F8[2];
    UINT64  F9[2];
    UINT64  F10[2];
    UINT64  F11[2];
    UINT64  F12[2];
    UINT64  F13[2];
    UINT64  F14[2];
    UINT64  F15[2];
    UINT64  F16[2];
    UINT64  F17[2];
    UINT64  F18[2];
    UINT64  F19[2];
    UINT64  F20[2];
    UINT64  F21[2];
    UINT64  F22[2];
    UINT64  F23[2];
    UINT64  F24[2];
    UINT64  F25[2];
    UINT64  F26[2];
    UINT64  F27[2];
    UINT64  F28[2];
    UINT64  F29[2];
    UINT64  F30[2];
    UINT64  F31[2];
    UINT64  Pr;
    UINT64  B0;
    UINT64  B1;
    UINT64  B2;
    UINT64  B3;
    UINT64  B4;
    UINT64  B5;
    UINT64  B6;
    UINT64  B7;
    UINT64  ArRsc;
    UINT64  ArBsp;
    UINT64  ArBspstore;
    UINT64  ArRnat;
    UINT64  ArFcr;
    UINT64  ArEflag;
    UINT64  ArCsd;
    UINT64  ArSsd;
    UINT64  ArCflg;
    UINT64  ArFsr;
    UINT64  ArFir;
    UINT64  ArFdr;
    UINT64  ArCcv;
    UINT64  ArUnat;
    UINT64  ArFpsr;
    UINT64  ArPfs;
    UINT64  ArLc;
    UINT64  ArEc;
    UINT64  CrDcr;
    UINT64  CrItm;
    UINT64  CrIva;
    UINT64  CrPta;
    UINT64  CrIpsr;
    UINT64  CrIsr;
    UINT64  CrIip;
    UINT64  CrIfa;
    UINT64  CrItir;
    UINT64  CrIipa;
    UINT64  CrIfs;
    UINT64  CrIim;
    UINT64  CrIha;
    UINT64  Dbr0;
    UINT64  Dbr1;
    UINT64  Dbr2;
    UINT64  Dbr3;
    UINT64  Dbr4;
    UINT64  Dbr5;
    UINT64  Dbr6;
    UINT64  Dbr7;
    UINT64  Ibr0;
    UINT64  Ibr1;
    UINT64  Ibr2;
    UINT64  Ibr3;
    UINT64  Ibr4;
    UINT64  Ibr5;
    UINT64  Ibr6;
    UINT64  Ibr7;
    UINT64  IntNat;
} EFI_SYSTEM_CONTEXT_IPF;

// EBC
#define EXCEPT_EBC_UNDEFINED            0
#define EXCEPT_EBC_DIVIDE_ERROR         1
#define EXCEPT_EBC_DEBUG                2
#define EXCEPT_EBC_BREAKPOINT           3
#define EXCEPT_EBC_OVERFLOW             4
#define EXCEPT_EBC_INVALID_OPCODE       5
#define EXCEPT_EBC_STACK_FAULT          6
#define EXCEPT_EBC_ALIGNMENT_CHECK      7
#define EXCEPT_EBC_INSTRUCTION_ENCODING 8
#define EXCEPT_EBC_BAD_BREAK            9
#define EXCEPT_EBC_STEP                 10
#define MAX_EBC_EXCEPTION               EXCEPT_EBC_STEP

typedef struct {
    UINT64  R0;
    UINT64  R1;
    UINT64  R2;
    UINT64  R3;
    UINT64  R4;
    UINT64  R5;
    UINT64  R6;
    UINT64  R7;
    UINT64  Flags;
    UINT64  ControlFlags;
    UINT64  Ip;
} EFI_SYSTEM_CONTEXT_EBC;

// ARM
#define EXCEPT_ARM_RESET                    0
#define EXCEPT_ARM_UNDEFINED_INSTRUCTION    1
#define EXCEPT_ARM_SOFTWARE_INTERRUPT       2
#define EXCEPT_ARM_PREFETCH_ABORT           3
#define EXCEPT_ARM_DATA_ABORT               4
#define EXCEPT_ARM_RESERVED                 5
#define EXCEPT_ARM_IRQ                      6
#define EXCEPT_ARM_FIQ                      7
#define MAX_ARM_EXCEPTION                   EXCEPT_ARM_FIQ

typedef struct {
    UINT32  R0;
    UINT32  R1;
    UINT32  R2;
    UINT32  R3;
    UINT32  R4;
    UINT32  R5;
    UINT32  R6;
    UINT32  R7;
    UINT32  R8;
    UINT32  R9;
    UINT32  R10;
    UINT32  R11;
    UINT32  R12;
    UINT32  SP;
    UINT32  LR;
    UINT32  PC;
    UINT32  CPSR;
    UINT32  DFSR;
    UINT32  DFAR;
    UINT32  IFSR;
    UINT32  IFAR;
} EFI_SYSTEM_CONTEXT_ARM;


typedef union {
    EFI_SYSTEM_CONTEXT_EBC  *SystemContextEbc;
    EFI_SYSTEM_CONTEXT_IA32 *SystemContextIa32;
    EFI_SYSTEM_CONTEXT_X64  *SystemContextX64;
    EFI_SYSTEM_CONTEXT_IPF  *SystemContextIpf;
    EFI_SYSTEM_CONTEXT_ARM  *SystemContextArm;
} EFI_SYSTEM_CONTEXT;

typedef
VOID
(EFIAPI *EFI_EXCEPTION_CALLBACK)(
    IN     EFI_EXCEPTION_TYPE               ExceptionType,
    IN OUT EFI_SYSTEM_CONTEXT               SystemContext);

typedef
VOID
(EFIAPI *EFI_PERIODIC_CALLBACK)(
    IN OUT EFI_SYSTEM_CONTEXT               SystemContext);

typedef enum {
    IsaIa32 = EFI_IMAGE_MACHINE_IA32,
    IsaX64 = EFI_IMAGE_MACHINE_X64,
    IsaIpf = EFI_IMAGE_MACHINE_IA64,
    IsaEbc = EFI_IMAGE_MACHINE_EBC,
    IsaArm = EFI_IMAGE_MACHINE_ARMTHUMB_MIXED,
//	IsaArm64 = EFI_IMAGE_MACHINE_AARCH64
} EFI_INSTRUCTION_SET_ARCHITECTURE;

//
// DEBUG_IMAGE_INFO
//

#define EFI_DEBUG_IMAGE_INFO_TABLE_GUID \
    { 0x49152e77, 0x1ada, 0x4764, {0xb7, 0xa2, 0x7a, 0xfe, 0xfe, 0xd9, 0x5e, 0x8b} }

#define EFI_DEBUG_IMAGE_INFO_UPDATE_IN_PROGRESS 0x01
#define EFI_DEBUG_IMAGE_INFO_TABLE_MODIFIED     0x02
#define EFI_DEBUG_IMAGE_INFO_INITIAL_SIZE       (EFI_PAGE_SIZE / sizeof (UINTN))
#define EFI_DEBUG_IMAGE_INFO_TYPE_NORMAL        0x01

typedef struct {
UINT64                Signature;
EFI_PHYSICAL_ADDRESS  EfiSystemTableBase;
UINT32                Crc32;
} EFI_SYSTEM_TABLE_POINTER;

typedef struct {
UINT32                    ImageInfoType;
EFI_LOADED_IMAGE_PROTOCOL *LoadedImageProtocolInstance;
EFI_HANDLE                *ImageHandle;
} EFI_DEBUG_IMAGE_INFO_NORMAL;

typedef union {
UINT32                      *ImageInfoType;
EFI_DEBUG_IMAGE_INFO_NORMAL *NormalImage;
} EFI_DEBUG_IMAGE_INFO;

typedef struct {
volatile UINT32       UpdateStatus;
UINT32                TableSize;
EFI_DEBUG_IMAGE_INFO  *EfiDebugImageInfoTable;
} EFI_DEBUG_IMAGE_INFO_TABLE_HEADER;

//
// EFI_DEBUGGER_PROTOCOL
//

#define EFI_DEBUG_SUPPORT_PROTOCOL_GUID \
    { 0x2755590c, 0x6f3c, 0x42fa, {0x9e, 0xa4, 0xa3, 0xba, 0x54, 0x3c, 0xda, 0x25} }

INTERFACE_DECL(_EFI_DEBUG_SUPPORT_PROTOCOL);

typedef
EFI_STATUS
(EFIAPI *EFI_GET_MAXIMUM_PROCESSOR_INDEX)(
    IN struct _EFI_DEBUG_SUPPORT_PROTOCOL  *This,
    OUT UINTN                              *MaxProcessorIndex);

typedef
EFI_STATUS
(EFIAPI *EFI_REGISTER_PERIODIC_CALLBACK)(
    IN struct _EFI_DEBUG_SUPPORT_PROTOCOL  *This,
    IN UINTN                               ProcessorIndex,
    IN EFI_PERIODIC_CALLBACK               PeriodicCallback);

typedef
EFI_STATUS
(EFIAPI *EFI_REGISTER_EXCEPTION_CALLBACK)(
    IN struct _EFI_DEBUG_SUPPORT_PROTOCOL  *This,
    IN UINTN                               ProcessorIndex,
    IN EFI_EXCEPTION_CALLBACK              ExceptionCallback,
    IN EFI_EXCEPTION_TYPE                  ExceptionType);

typedef
EFI_STATUS
(EFIAPI *EFI_INVALIDATE_INSTRUCTION_CACHE)(
    IN struct _EFI_DEBUG_SUPPORT_PROTOCOL  *This,
    IN UINTN                               ProcessorIndex,
    IN VOID                                *Start,
    IN UINT64                              Length);

typedef struct _EFI_DEBUG_SUPPORT_PROTOCOL {
    EFI_INSTRUCTION_SET_ARCHITECTURE  Isa;
    EFI_GET_MAXIMUM_PROCESSOR_INDEX   GetMaximumProcessorIndex;
    EFI_REGISTER_PERIODIC_CALLBACK    RegisterPeriodicCallback;
    EFI_REGISTER_EXCEPTION_CALLBACK   RegisterExceptionCallback;
    EFI_INVALIDATE_INSTRUCTION_CACHE  InvalidateInstructionCache;
} EFI_DEBUG_SUPPORT_PROTOCOL;

#endif
