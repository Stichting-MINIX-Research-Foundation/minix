#ifndef _VFS_EXEC_H_
#define _VFS_EXEC_H_ 1

struct exec_info {
    int  proc_e;			/* Process endpoint */
    char *hdr;				/* Exec file's header */
    int hdr_len;			/* How many bytes are in hdr */
    vir_bytes pc;			/* Entry point of exec file */
    vir_bytes stack_top;		/* Top of the stack */
    vir_bytes frame_len;		/* Stack size */
    uid_t new_uid;			/* Process UID after exec */
    gid_t new_gid;			/* Process GID after exec */
    int load_text;			/* Load text section? */
    int setugid;			/* Allow set{u,g}id execution? */
    struct vnode *vp;			/* Exec file's vnode */
    struct vmnt *vmp;			/* Exec file's vmnt */
    struct stat sb;			/* Exec file's stat structure */
    char progname[PROC_NAME_LEN];	/* Program name */
    int userflags;			/* exec() flags from userland */

    /* fields only used by elf and in VFS */
    int is_dyn;				/* Dynamically linked executable */
    vir_bytes elf_phdr;			/* Program header location */
    vir_bytes elf_base;			/* Userland addr load address */
    int elf_main_fd;			/* Dyn: FD of main program execuatble */
    char execname[PATH_MAX];		/* Full executable invocation */
};

#endif /* !_VFS_EXEC_H_ */
