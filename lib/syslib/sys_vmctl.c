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
        vir_bytes *len, int *wrflag, endpoint_t *who_s, vir_bytes *mem_s,
        endpoint_t *requestor)
{
  message m;
  int r;

  m.SVMCTL_WHO = SELF;
  m.SVMCTL_PARAM = VMCTL_MEMREQ_GET;
  r = _taskcall(SYSTASK, SYS_VMCTL, &m);
  if(r >= 0) {
	*who = m.SVMCTL_MRG_TARGET;
	*mem = m.SVMCTL_MRG_ADDR;
	*len = m.SVMCTL_MRG_LENGTH;
	*wrflag = m.SVMCTL_MRG_FLAG;
	*who_s = m.SVMCTL_MRG_EP2;
	*mem_s = m.SVMCTL_MRG_ADDR2;
	*requestor = (endpoint_t) m.SVMCTL_MRG_REQUESTOR;
  }
  return r;
}

PUBLIC int sys_vmctl_enable_paging(struct mem_map *map)
{
	message m;
	m.SVMCTL_WHO = SELF;
	m.SVMCTL_PARAM = VMCTL_ENABLE_PAGING;
	m.SVMCTL_VALUE = (int) map;
	return _taskcall(SYSTASK, SYS_VMCTL, &m);
}

PUBLIC int sys_vmctl_get_mapping(int index,
	phys_bytes *addr, phys_bytes *len, int *flags)
{
	int r;
	message m;

	m.SVMCTL_WHO = SELF;
	m.SVMCTL_PARAM = VMCTL_KERN_PHYSMAP;
	m.SVMCTL_VALUE = (int) index;

	r = _taskcall(SYSTASK, SYS_VMCTL, &m);

	if(r != OK)
		return r;

	*addr = m.SVMCTL_MAP_PHYS_ADDR;
	*len = m.SVMCTL_MAP_PHYS_LEN;
	*flags = m.SVMCTL_MAP_FLAGS;

	return OK;
}

PUBLIC int sys_vmctl_reply_mapping(int index, vir_bytes addr)
{
	int r;
	message m;

	m.SVMCTL_WHO = SELF;
	m.SVMCTL_PARAM = VMCTL_KERN_MAP_REPLY;
	m.SVMCTL_VALUE = index;
	m.SVMCTL_MAP_VIR_ADDR = (char *) addr;

	return _taskcall(SYSTASK, SYS_VMCTL, &m);
}
