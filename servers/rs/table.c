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
#define FS_KC   SYS_BASIC_CALLS, SYS_TRACE, SYS_UMAP, SYS_VIRCOPY, SYS_KILL
#define DRV_KC	SYS_BASIC_CALLS, SYS_TRACE, SYS_UMAP, SYS_VIRCOPY, SYS_SEGCTL, \
    SYS_IRQCTL, SYS_INT86, SYS_DEVIO, SYS_SDEVIO, SYS_VDEVIO

PRIVATE int
  pm_kc[] =   { SYS_ALL_C, SYS_NULL_C },
  sched_kc[] ={ SYS_ALL_C, SYS_NULL_C },
  vfs_kc[] =  { FS_KC, SYS_NULL_C },
  rs_kc[] =   { SYS_ALL_C, SYS_NULL_C },
  ds_kc[] =   { SYS_ALL_C, SYS_NULL_C },
  vm_kc[] =   { SYS_ALL_C, SYS_NULL_C },
  tty_kc[] =  { DRV_KC, SYS_KILL, SYS_PHYSCOPY, SYS_ABORT, SYS_IOPENABLE,
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
  sched_vmc[] ={ VM_BASIC_CALLS, SYS_NULL_C },
  vfs_vmc[] =  { VM_BASIC_CALLS, SYS_NULL_C },
  rs_vmc[] =   { VM_BASIC_CALLS, VM_RS_SET_PRIV, VM_RS_UPDATE, VM_RS_MEMCTL,
      SYS_NULL_C },
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
 * at boot time, except for the fact that kernel-scheduled services are 
 * handled before user-scheduled ones.
 */
PUBLIC struct boot_image_priv boot_image_priv_table[] = {
/*endpoint,     label,   flags,  traps,  ipcto,  sigmgr,  sched,    kcalls,  vmcalls, T */
{RS_PROC_NR,   "rs",     RSYS_F, RSYS_T, RSYS_M, RSYS_SM, KERN_SCH, rs_kc,   rs_vmc,  0 },
{VM_PROC_NR,   "vm",     VM_F,   SRV_T,  SRV_M,  SRV_SM,  KERN_SCH, vm_kc,   vm_vmc,  0 },
{PM_PROC_NR,   "pm",     SRV_F,  SRV_T,  SRV_M,  SRV_SM,  KERN_SCH, pm_kc,   pm_vmc,  0 },
{SCHED_PROC_NR,"sched",  SRV_F,  SRV_T,  SRV_M,  SRV_SM,  KERN_SCH, sched_kc, sched_vmc, 0 },
{VFS_PROC_NR,  "vfs",    SRV_F,  SRV_T,  SRV_M,  SRV_SM,  KERN_SCH, vfs_kc,  vfs_vmc, 0 },
{DS_PROC_NR,   "ds",     SRV_F,  SRV_T,  SRV_M,  SRV_SM,  KERN_SCH, ds_kc,   ds_vmc,  0 },
{TTY_PROC_NR,  "tty",    SRV_F,  SRV_T,  SRV_M,  SRV_SM,  USER_SCH, tty_kc,  tty_vmc, 0 },
{MEM_PROC_NR,  "memory", SRV_F,  SRV_T,  SRV_M,  SRV_SM,  KERN_SCH, mem_kc,  mem_vmc, 0 },
{LOG_PROC_NR,  "log",    SRV_F,  SRV_T,  SRV_M,  SRV_SM,  USER_SCH, log_kc,  log_vmc, 0 },
{MFS_PROC_NR,"fs_imgrd", SRV_F,  SRV_T,  SRV_M,  SRV_SM,  KERN_SCH, mfs_kc,  mfs_vmc, 0 },
{PFS_PROC_NR,  "pfs",    SRV_F,  SRV_T,  SRV_M,  SRV_SM,  USER_SCH, pfs_kc,  pfs_vmc, 0 },
{INIT_PROC_NR, "init",   RUSR_F, RUSR_T, RUSR_M, RUSR_SM, NONE,     rusr_kc, rusr_vmc,0 },
{NULL_BOOT_NR, "",       0,      0,      0,      0,       0,        no_kc,   no_vmc,  0 }
};

/* Definition of the boot image sys table. */
PUBLIC struct boot_image_sys boot_image_sys_table[] = {
  /*endpoint,         flags                             */
  { RS_PROC_NR,       SRVR_SF                           },
  { VM_PROC_NR,       VM_SF                             },
  { PM_PROC_NR,       SRVR_SF                           },
  { VFS_PROC_NR,      SRVR_SF                           },
  { LOG_PROC_NR,      SRV_SF       | SF_USE_REPL        },
  { MFS_PROC_NR,      SF_NEED_COPY | SF_USE_COPY        },
  { PFS_PROC_NR,      SRV_SF       | SF_USE_COPY        },
  { DEFAULT_BOOT_NR,  SRV_SF                            } /* default entry */
};

/* Definition of the boot image dev table. */
PUBLIC struct boot_image_dev boot_image_dev_table[] = {
  /*endpoint,        flags,   dev_nr,       dev_style,  dev_style2 */
  { TTY_PROC_NR,     SRV_DF,  TTY_MAJOR,    STYLE_TTY,  STYLE_CTTY },
  { MEM_PROC_NR,     SRV_DF,  MEMORY_MAJOR, STYLE_DEV,  STYLE_NDEV },
  { LOG_PROC_NR,     SRV_DF,  LOG_MAJOR,    STYLE_DEVA, STYLE_NDEV },
  { DEFAULT_BOOT_NR, SRV_DF,  0,            STYLE_NDEV, STYLE_NDEV } /* default
                                                                      * entry
                                                                      */
};

