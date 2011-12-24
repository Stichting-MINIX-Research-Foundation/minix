#ifndef __VFS_PARAM_H__
#define __VFS_PARAM_H__

/* The following names are synonyms for the variables in the input message. */
#define addr	      m1_i3
#define buffer	      m1_p1
#define child_endpt      m1_i2
#define co_mode	      m1_i1
#define fd	      m1_i1
#define fd2	      m1_i2
#define group	      m1_i3
#define ls_fd	      m2_i1
#define mk_mode	      m1_i2
#define mk_z0	      m1_i3
#define mode	      m3_i2
#define c_mode        m1_i3
#define c_name        m1_p1
#define name	      m3_p1
#define flength       m2_l1
#define name1	      m1_p1
#define name2	      m1_p2
#define	name_length   m3_i1
#define name1_length  m1_i1
#define name2_length  m1_i2
#define nbytes        m1_i2
#define owner	      m1_i2
#define pathname      m3_ca1
#define pid	      m1_i3
#define ENDPT	      m1_i1
#define offset_lo     m2_l1
#define offset_high   m2_l2
#define ctl_req       m4_l1
#define mount_flags   m1_i3
#define pipe_flags    m1_i3
#define request       m1_i2
#define sig	      m1_i2
#define endpt1	      m1_i1
#define fs_label      m1_p3
#define umount_label  m3_ca1
#define tp	      m2_l1
#define utime_actime  m2_l1
#define utime_modtime m2_l2
#define utime_file    m2_p1
#define utime_length  m2_i1
#define utime_strlen  m2_i2
#define utimens_fd    m2_i1
#define utimens_ansec m2_i2
#define utimens_mnsec m2_i3
#define utimens_flags m2_s1
#define whence	      m2_i2
#define svrctl_req    m2_i1
#define svrctl_argp   m2_p1
#define md_label	m2_p1
#define md_label_len	m2_l1
#define md_major	m2_i1
#define md_style	m2_i2
#define md_flags	m2_i3

/* The following names are synonyms for the variables in the output message. */
#define reply_type    m_type
#define reply_l1      m2_l1
#define reply_l2      m2_l2
#define reply_i1      m1_i1
#define reply_i2      m1_i2

#endif
