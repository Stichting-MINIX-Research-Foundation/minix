#ifndef __VFS_WORK_H__
#define __VFS_WORK_H__

struct job {
  struct fproc *j_fp;
  message j_m_in;
  int j_err_code;
  void *(*j_func)(void *arg);
  struct job *j_next;
};

#endif
