#ifndef _RS_EXEC_H_
#define _RS_EXEC_H_ 1

struct exec_info {
    int  proc_e;			/* Process endpoint */
    char *image;			/* Executable image */
    size_t image_len;			/* Size of executable image */
    vir_bytes pc;			/* Entry point of exec file */
    vir_bytes stack_top;		/* Top of the stack */
    vir_bytes frame_len;		/* Stack size */
    char progname[PROC_NAME_LEN];	/* Program name */
};

#endif /* !_RS_EXEC_H_ */
