#include "syslib.h"

PUBLIC int sys_vmctl(endpoint_t who, int param, u32_t value)
{
  message m;
  int r;

  m.SVMCTL_WHO = who;
  m.SVMCTL_PARAM = param;
  m.SVMCTL_VALUE = value;
  r = _taskcall(SYSTASK, SYS_VMCTL, &m);
  return(r);
}

PUBLIC int sys_vmctl_get_pagefault_i386(endpoint_t *who, u32_t *cr2, u32_t *err)
{
  message m;
  int r;

  m.SVMCTL_WHO = SELF;
  m.SVMCTL_PARAM = VMCTL_GET_PAGEFAULT;
  r = _taskcall(SYSTASK, SYS_VMCTL, &m);
  if(r == OK) {
	*who = m.SVMCTL_PF_WHO;
	*cr2 = m.SVMCTL_PF_I386_CR2;
	*err = m.SVMCTL_PF_I386_ERR;
  }
  return(r);
}

PUBLIC int sys_vmctl_get_cr3_i386(endpoint_t who, u32_t *cr3)
{
  message m;
  int r;

  m.SVMCTL_WHO = who;
  m.SVMCTL_PARAM = VMCTL_I386_GETCR3;
  r = _taskcall(SYSTASK, SYS_VMCTL, &m);
  if(r == OK) {
	*cr3 = m.SVMCTL_VALUE;
  }
  return(r);
}

PUBLIC int sys_vmctl_get_memreq(endpoint_t *who, vir_bytes *mem,
	vir_bytes *len, int *wrflag)
{
  message m;
  int r;

  m.SVMCTL_WHO = SELF;
  m.SVMCTL_PARAM = VMCTL_MEMREQ_GET;
  r = _taskcall(SYSTASK, SYS_VMCTL, &m);
  if(r == OK) {
	*who = m.SVMCTL_MRG_EP;
	*mem = (vir_bytes) m.SVMCTL_MRG_ADDR;
	*len = m.SVMCTL_MRG_LEN;
	*wrflag = m.SVMCTL_MRG_WRITE;
  }
  return r;
}
