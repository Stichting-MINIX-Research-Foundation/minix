/* This file contains the definition of the boot image info tables.
 *
 * Changes:
 *   Nov 22, 2009: Created  (Cristiano Giuffrida)
 */

#define _TABLE

#include "inc.h"

/* Define kernel calls that processes are allowed to make.
 * 
 * Calls are unordered lists, converted by RS to bitmasks
 * once at runtime.
 */
#define FS_KC   SYS_KILL, SYS_VIRCOPY, SYS_SAFECOPYFROM, SYS_SAFECOPYTO, \
    SYS_UMAP, SYS_GETINFO, SYS_EXIT, SYS_TIMES, SYS_SETALARM, \
    SYS_PRIVCTL, SYS_TRACE , SYS_SETGRANT, SYS_PROFBUF, SYS_SYSCTL, \
    SYS_SAFEMAP, SYS_SAFEREVMAP, SYS_SAFEUNMAP
#define DRV_KC	FS_KC, SYS_SEGCTL, SYS_IRQCTL, SYS_INT86, SYS_DEVIO, \
    SYS_SDEVIO, SYS_VDEVIO, SYS_SETGRANT, SYS_PROFBUF, SYS_SYSCTL

PRIVATE int
  pm_kc[] =   { SYS_ALL_C, SYS_NULL_C },
  vfs_kc[] =  { FS_KC, SYS_NULL_C },
  rs_kc[] =   { SYS_ALL_C, SYS_NULL_C },
  ds_kc[] =   { SYS_ALL_C, SYS_NULL_C },
  vm_kc[] =   { SYS_ALL_C, SYS_NULL_C },
  tty_kc[] =  { DRV_KC, SYS_PHYSCOPY, SYS_ABORT, SYS_IOPENABLE,
      SYS_READBIOS, SYS_NULL_C },
  mem_kc[] =  { DRV_KC, SYS_PHYSCOPY, SYS_IOPENABLE, SYS_NULL_C},
  log_kc[] =  { DRV_KC, SYS_NULL_C },
  mfs_kc[] =  { FS_KC, SYS_NULL_C },
  pfs_kc[] =  { FS_KC, SYS_NULL_C },
  rusr_kc[] = { SYS_NULL_C },
  no_kc[] =   { SYS_NULL_C }; /* no kernel call */

/* Define VM calls that processes are allowed to make.
 * 
 * Calls are unordered lists, converted by RS to bitmasks
 * once at runtime.
 */
PRIVATE int
  pm_vmc[] =   { VM_BASIC_CALLS, VM_EXIT, VM_FORK, VM_BRK, VM_EXEC_NEWMEM,
      VM_PUSH_SIG, VM_WILLEXIT, VM_ADDDMA, VM_DELDMA, VM_GETDMA,
      VM_NOTIFY_SIG, SYS_NULL_C },
  vfs_vmc[] =  { VM_BASIC_CALLS, SYS_NULL_C },
  rs_vmc[] =   { VM_BASIC_CALLS, VM_RS_SET_PRIV, SYS_NULL_C },
  ds_vmc[] =   { VM_BASIC_CALLS, SYS_NULL_C },
  vm_vmc[] =   { SYS_NULL_C },
  tty_vmc[] =  { VM_BASIC_CALLS, SYS_NULL_C },
  mem_vmc[] =  { VM_BASIC_CALLS, SYS_NULL_C },
  log_vmc[] =  { VM_BASIC_CALLS, SYS_NULL_C },
  mfs_vmc[] =  { VM_BASIC_CALLS, SYS_NULL_C },
  pfs_vmc[] =  { VM_BASIC_CALLS, SYS_NULL_C },
  rusr_vmc[] = { VM_BASIC_CALLS, SYS_NULL_C },
  no_vmc[] =   { SYS_NULL_C }; /* no vm call */

/* Definition of the boot image priv table. The order of entries in this table
 * reflects the order boot system services are made runnable and initialized
 * at boot time.
 */
PUBLIC struct boot_image_priv boot_image_priv_table[] = {
  /*endpoint,     label,      flags,  traps,  ipcto,  kcalls,  vmcalls  */
  { RS_PROC_NR,   "rs",       RSYS_F, RSYS_T, RSYS_M, rs_kc,   rs_vmc   },
  { VM_PROC_NR,   "vm",       VM_F,   SRV_T,  SRV_M,  vm_kc,   vm_vmc   },
  { PM_PROC_NR,   "pm",       SRV_F,  SRV_T,  SRV_M,  pm_kc,   pm_vmc   },
  { VFS_PROC_NR,  "vfs",      SRV_F,  SRV_T,  SRV_M,  vfs_kc,  vfs_vmc  },
  { DS_PROC_NR,   "ds",       SRV_F,  SRV_T,  SRV_M,  ds_kc,   ds_vmc   },
  { TTY_PROC_NR,  "tty",      SRV_F,  SRV_T,  SRV_M,  tty_kc,  tty_vmc  },
  { MEM_PROC_NR,  "memory",   SRV_F,  SRV_T,  SRV_M,  mem_kc,  mem_vmc  },
  { LOG_PROC_NR,  "log",      SRV_F,  SRV_T,  SRV_M,  log_kc,  log_vmc  },
  { MFS_PROC_NR,  "fs_imgrd", SRV_F,  SRV_T,  SRV_M,  mfs_kc,  mfs_vmc  },
  { PFS_PROC_NR,  "pfs",      SRV_F,  SRV_T,  SRV_M,  pfs_kc,  pfs_vmc  },
  { INIT_PROC_NR, "init",     RUSR_F, RUSR_T, RUSR_M, rusr_kc, rusr_vmc },
  { NULL_BOOT_NR, "",         0,      0,      0,      no_kc,   no_vmc   }
};

/* Definition of the boot image sys table. */
PUBLIC struct boot_image_sys boot_image_sys_table[] = {
  /*endpoint,         flags                             */
  { RS_PROC_NR,       SRV_SF                            },
  { VM_PROC_NR,       VM_SF                             },
  { LOG_PROC_NR,      SRVC_SF                           },
  { MFS_PROC_NR,      SF_USE_COPY | SF_NEED_COPY        },
  { PFS_PROC_NR,      SRVC_SF                           },
  { DEFAULT_BOOT_NR,  SRV_SF                            } /* default entry */
};

/* Definition of the boot image dev table. */
PUBLIC struct boot_image_dev boot_image_dev_table[] = {
  /*endpoint,         dev_nr,       dev_style,  period  */
  { TTY_PROC_NR,      TTY_MAJOR,    STYLE_TTY,       0  },
  { MEM_PROC_NR,      MEMORY_MAJOR, STYLE_DEV,       0  },
  { LOG_PROC_NR,      LOG_MAJOR,    STYLE_DEV,       0  },
  { DEFAULT_BOOT_NR,  0,            STYLE_NDEV,      0  } /* default entry */
};

