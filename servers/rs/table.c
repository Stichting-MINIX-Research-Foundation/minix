/* This file contains the definition of the boot image info tables.
 *
 * Changes:
 *   Nov 22, 2009: Created  (Cristiano Giuffrida)
 */

#define _TABLE

#include "inc.h"

/* Define kernel calls that processes are allowed to make. This is not looking
 * very nice, but we need to define the access rights on a per call basis.
 * 
 * Calls are unordered lists, converted by RS to bitmasks
 * once at runtime.
 */
#define FS_KC SYS_KILL, SYS_VIRCOPY, SYS_SAFECOPYFROM, SYS_SAFECOPYTO, \
    SYS_VIRVCOPY, SYS_UMAP, SYS_GETINFO, SYS_EXIT, SYS_TIMES, SYS_SETALARM, \
    SYS_PRIVCTL, SYS_TRACE , SYS_SETGRANT, SYS_PROFBUF, SYS_SYSCTL
#define DRV_KC	FS_KC, SYS_SEGCTL, SYS_IRQCTL, SYS_INT86, SYS_DEVIO, \
    SYS_SDEVIO, SYS_VDEVIO, SYS_SETGRANT, SYS_PROFBUF, SYS_SYSCTL

PRIVATE int
  fs_kc[] = { FS_KC, SYS_NULL_C },
  pm_kc[] = { SYS_ALL_C, SYS_NULL_C },
  ds_kc[] = { SYS_ALL_C, SYS_NULL_C },
  vm_kc[] = { SYS_ALL_C, SYS_NULL_C },
  drv_kc[] = { DRV_KC, SYS_NULL_C },
  tty_kc[] = { DRV_KC, SYS_PHYSCOPY, SYS_ABORT, SYS_IOPENABLE,
      SYS_READBIOS, SYS_NULL_C },
  mem_kc[] = { DRV_KC, SYS_PHYSCOPY, SYS_PHYSVCOPY, SYS_IOPENABLE, SYS_NULL_C },
  rusr_kc[] = { SYS_NULL_C },

  no_kc[] = { SYS_NULL_C }; /* no kernel call */

/* Definition of the boot image priv table. */
PUBLIC struct boot_image_priv boot_image_priv_table[] = {
  /*endpoint,    priv flags,  traps,  ipcto,  kcalls    */
  { VM_PROC_NR,       VM_F,   SRV_T,  SRV_M,  vm_kc     },
  { PM_PROC_NR,       SRV_F,  SRV_T,  SRV_M,  pm_kc     },
  { FS_PROC_NR,       SRV_F,  SRV_T,  SRV_M,  fs_kc     },
  { DS_PROC_NR,       SRV_F,  SRV_T,  SRV_M,  ds_kc     },
  { TTY_PROC_NR,      SRV_F,  SRV_T,  SRV_M,  tty_kc    },
  { MEM_PROC_NR,      SRV_F,  SRV_T,  SRV_M,  mem_kc    },
  { LOG_PROC_NR,      SRV_F,  SRV_T,  SRV_M,  drv_kc    },
  { MFS_PROC_NR,      SRV_F,  SRV_T,  SRV_M,  fs_kc     },
  { INIT_PROC_NR,     RUSR_F, RUSR_T, RUSR_M, rusr_kc   },
  { NULL_BOOT_NR,     0,      0,      0,      no_kc     } /* null entry */
};

/* Definition of the boot image sys table. */
PUBLIC struct boot_image_sys boot_image_sys_table[] = {
  /*endpoint,         sys flags                         */
  { LOG_PROC_NR,      SRVC_SF                           },
  { MFS_PROC_NR,      SRVC_SF                           },
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

