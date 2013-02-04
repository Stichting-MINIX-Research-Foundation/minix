#ifndef _LIBEXEC_H_
#define _LIBEXEC_H_ 1

#include <sys/exec_elf.h>

struct exec_info;

typedef int (*libexec_loadfunc_t)(struct exec_info *execi,
	off_t offset, off_t vaddr, size_t len);

typedef int (*libexec_clearfunc_t)(struct exec_info *execi,
	off_t vaddr, size_t len);

typedef int (*libexec_allocfunc_t)(struct exec_info *execi,
	off_t vaddr, size_t len);

typedef int (*libexec_procclearfunc_t)(struct exec_info *execi);

struct exec_info {
    /* Filled in by libexec caller */
    endpoint_t  proc_e;                 /* Process endpoint */
    char *hdr;                          /* Header or full image */
    size_t hdr_len;                     /* Size of hdr */
    vir_bytes frame_len;                /* Stack size */
    char progname[PROC_NAME_LEN];       /* Program name */
    uid_t new_uid;                      /* Process UID after exec */
    gid_t new_gid;                      /* Process GID after exec */
    int allow_setuid;                   /* Allow set{u,g}id execution? */
    vir_bytes stack_size;		/* Desired stack size */
    vir_bytes load_offset;		/* Desired load offset */
    off_t filesize;			/* How big is the file */

    /* Callback pointers for use by libexec */
    libexec_loadfunc_t copymem;		/* Copy callback */
    libexec_clearfunc_t clearmem;	/* Clear callback */
    libexec_allocfunc_t allocmem_prealloc; /* Alloc callback */
    libexec_allocfunc_t allocmem_ondemand; /* Alloc callback */
    libexec_procclearfunc_t clearproc;	/* Clear process callback */
    void *opaque;			/* Callback data */

    /* Filled in by libexec load function */
    vir_bytes load_base;		/* Where executable is loaded */
    vir_bytes pc;                       /* Entry point of exec file */
    vir_bytes stack_high;		/* High stack addr */
};

int elf_has_interpreter(char *exec_hdr, int hdr_len, char *interp, int maxsz);
int elf_phdr(char *exec_hdr, int hdr_len, vir_bytes *phdr);

void libexec_patch_ptr(char stack[ARG_MAX], vir_bytes base);
int libexec_pm_newexec(endpoint_t proc_e, struct exec_info *execi);

typedef int (*libexec_exec_loadfunc_t)(struct exec_info *execi);
int libexec_load_elf(struct exec_info *execi);

int libexec_copy_memcpy(struct exec_info *execi, off_t offset, off_t vaddr, size_t len);
int libexec_clear_memset(struct exec_info *execi, off_t vaddr, size_t len);
int libexec_alloc_mmap_prealloc(struct exec_info *execi, off_t vaddr, size_t len);
int libexec_alloc_mmap_ondemand(struct exec_info *execi, off_t vaddr, size_t len);
int libexec_clearproc_vm_procctl(struct exec_info *execi);
int libexec_clear_sys_memset(struct exec_info *execi, off_t vaddr, size_t len);

#endif /* !_LIBEXEC_H_ */
