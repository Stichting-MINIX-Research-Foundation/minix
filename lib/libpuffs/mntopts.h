#ifndef _MNTOPTS_H_
#define _MNTOPTS_H_

struct mntopt {
        const char *m_option;   /* option name */
        int m_inverse;          /* if a negative option, eg "dev" */
        int m_flag;             /* bit to set, eg. MNT_RDONLY */
        int m_altloc;           /* 1 => set bit in altflags */
};



#endif
