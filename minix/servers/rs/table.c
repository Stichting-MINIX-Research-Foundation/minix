/* This file contains the definition of the boot image info tables.
 *
 * Changes:
 *   Nov 22, 2009: Created  (Cristiano Giuffrida)
 */

#define _TABLE

#include "inc.h"

/* Definition of the boot image priv table. The order of entries in this table
 * reflects the order boot system services are made runnable and initialized
 * at boot time.
 */
struct boot_image_priv boot_image_priv_table[] = {
/*endpoint,     label,   flags, */
{RS_PROC_NR,   "rs",     RSYS_F },
{VM_PROC_NR,   "vm",     VM_F   },
{PM_PROC_NR,   "pm",     SRV_F  },
{SCHED_PROC_NR,"sched",  SRV_F  },
{VFS_PROC_NR,  "vfs",    SRV_F  },
{DS_PROC_NR,   "ds",     SRV_F  },
{TTY_PROC_NR,  "tty",    SRV_F  },
{MEM_PROC_NR,  "memory", SRV_F  },
{MIB_PROC_NR,  "mib",    SRV_F  },
{PFS_PROC_NR,  "pfs",    SRV_F  },
{MFS_PROC_NR,"fs_imgrd", SRV_F  },
{INIT_PROC_NR, "init",   USR_F  },
{NULL_BOOT_NR, "",       0,     } /* null entry */
};

/* Definition of the boot image sys table. */
struct boot_image_sys boot_image_sys_table[] = {
  /*endpoint,         flags                             */
  { RS_PROC_NR,       SRVR_SF                           },
  { VM_PROC_NR,       VM_SF                             },
  { PM_PROC_NR,       SRVR_SF                           },
  { SCHED_PROC_NR,    SRVR_SF                           },
  { VFS_PROC_NR,      SRVR_SF                           },
  { MFS_PROC_NR,      0                                 },
  { DEFAULT_BOOT_NR,  SRV_SF                            } /* default entry */
};

/* Definition of the boot image dev table. */
struct boot_image_dev boot_image_dev_table[] = {
  /*endpoint,        dev_nr       */
  { TTY_PROC_NR,     TTY_MAJOR    },
  { MEM_PROC_NR,     MEMORY_MAJOR },
  { DEFAULT_BOOT_NR, 0            } /* default entry */
};
