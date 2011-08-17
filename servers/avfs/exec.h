#ifndef _VFS_EXEC_H_
#define _VFS_EXEC_H_ 1

struct exec_info {
    int  proc_e;			/* Process endpoint */
    char *hdr;				/* Exec file's header */
    vir_bytes pc;			/* Entry point of exec file */
    vir_bytes stack_top;		/* Top of the stack */
    vir_bytes frame_len;		/* Stack size */
    uid_t new_uid;			/* Process UID after exec */
    gid_t new_gid;			/* Process GID after exec */
    int load_text;			/* Load text section? */
    int allow_setuid;			/* Allow setuid execution? */
    struct vnode *vp;			/* Exec file's vnode */
    struct stat sb;			/* Exec file's stat structure */
    char progname[PROC_NAME_LEN];	/* Program name */
};

#endif /* !_VFS_EXEC_H_ */
