/*
 * dec21041.c
 *
 * This file contains an ethernet device driver for DEC  21140A
 * fast ethernet controllers as emulated by VirtualPC 2007. It is not 
 * intended to support the real card, as much more error checking
 * and testing would be needed. It supports both bridged and NAT mode.
 *
 * Created:	Mar 2008 by Nicolas Tittley <first.last@ google's mail>
 */

#include <minix/drivers.h>
#include <minix/netdriver.h>

#include <assert.h>
#include <machine/pci.h>
#include <minix/syslib.h>
#include <minix/endpoint.h>
#include <minix/com.h>
#include <minix/sef.h>
#include <minix/ds.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <stdlib.h>

#include "dec21140A.h"


static u32_t io_inl(u16_t);
static void io_outl(u16_t, u32_t);
static void do_conf(const message *);
static void do_get_stat_s(message *);
static void do_interrupt(const dpeth_t *);
static void do_reply(dpeth_t *);
static void do_vread_s(const message *, int);
static void do_watchdog(void *);

static void de_update_conf(dpeth_t *);
static int de_probe(dpeth_t *, int skip);
static void de_conf_addr(dpeth_t *);
static void de_first_init(dpeth_t *);
static void de_reset(const dpeth_t *);
static void de_hw_conf(const dpeth_t *);
static void de_start(const dpeth_t *);
static void de_setup_frame(const dpeth_t *);
static u16_t de_read_rom(const dpeth_t *, u8_t, u8_t);
static int de_calc_iov_size(iovec_dat_s_t *);
static void de_next_iov(iovec_dat_s_t *);
static void do_vwrite_s(const message *, int);
static void de_get_userdata_s(int, cp_grant_id_t, vir_bytes, int, void
	*);

/* Error messages */
static char str_CopyErrMsg[]  = "unable to read/write user data";
static char str_SendErrMsg[]  = "send failed";
static char str_SizeErrMsg[]  = "illegal packet size";
static char str_UmapErrMsg[]  = "Unable to sys_umap";
static char str_BusyErrMsg[]  = "Send/Recv failed: busy";
static char str_StatErrMsg[]  = "Unable to send stats";
static char str_AlignErrMsg[] = "Bad align of buffer/descriptor";
static char str_DevName[]     = "dec21140A:eth#?";

static dpeth_t de_state;
static int de_instance;

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);

/*===========================================================================*
 *				main					     *
 *===========================================================================*/
int main(int argc, char *argv[])
{
  dpeth_t *dep;
  message m;
  int ipc_status;
  int r;

  /* SEF local startup. */
  env_setargs(argc, argv);
  sef_local_startup();
  
  while (TRUE)
    {
      if ((r= netdriver_receive(ANY, &m, &ipc_status)) != OK)
	panic("netdriver_receive failed: %d", r);

		if(is_ipc_notify(ipc_status)) {
			switch(_ENDPOINT_P(m.m_source)) {
				case CLOCK:
					do_watchdog(&m);
					break;

	case HARDWARE:
	  dep = &de_state;
	  if (dep->de_mode == DEM_ENABLED) {
	    do_interrupt(dep);
	    if (dep->de_flags & (DEF_ACK_SEND | DEF_ACK_RECV))
	    do_reply(dep);
	    sys_irqenable(&dep->de_hook);
	  }
	  break;
	 default:
	 	printf("ignoring notify from %d\n", m.m_source);
	 	break;
			}
			continue;
		}
      
      switch (m.m_type)
	{
	case DL_WRITEV_S:  do_vwrite_s(&m, FALSE); break;
	case DL_READV_S:   do_vread_s(&m, FALSE);  break;	  
	case DL_CONF:      do_conf(&m);            break;  
	case DL_GETSTAT_S: do_get_stat_s(&m);      break;

	default:  
		printf("message 0x%lx; %d from %d\n",
			m.m_type, m.m_type-DL_RQ_BASE, m.m_source);
		panic("illegal message: %d", m.m_type);
	}
    }
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_lu(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* Register live update callbacks. */
  sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
  sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_workfree);

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler_term);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int type, sef_init_info_t *UNUSED(info))
{
/* Initialize the DEC 21140A driver. */
  int fkeys, sfkeys;
  long v;

  v = 0;
  (void) env_parse("instance", "d", 0, &v, 0, 255);
  de_instance = (int) v;

  /* Request function key for debug dumps */
  fkeys = sfkeys = 0;
  bit_set(sfkeys, DE_FKEY);
  if ((fkey_map(&fkeys, &sfkeys)) != OK) 
    printf("%s: error using Shift+F%d key(%d)\n", str_DevName, DE_FKEY, errno);

  /* Announce we are up! */
  netdriver_announce();

  return OK;
}

static void do_get_stat_s(message * mp)
{
  int rc;
  dpeth_t *dep;

  dep = &de_state;

  if ((rc = sys_safecopyto(mp->m_source, mp->DL_GRANT, 0UL,
			(vir_bytes)&dep->de_stat,
			sizeof(dep->de_stat))) != OK)
        panic(str_CopyErrMsg, rc);

  mp->m_type = DL_STAT_REPLY;
  rc = send(mp->m_source, mp);
  if( rc != OK )
    panic(str_StatErrMsg, rc);
  return;
}

static void do_conf(const message * mp)
{
  int r;
  dpeth_t *dep;
  message reply_mess;

  dep = &de_state;

  strncpy(dep->de_name, str_DevName, strlen(str_DevName));
  dep->de_name[strlen(dep->de_name)-1] = '0' + de_instance;

  if (dep->de_mode == DEM_DISABLED) {
    de_update_conf(dep); 
    pci_init();
    if (dep->de_mode == DEM_ENABLED && !de_probe(dep, de_instance)) {
	printf("%s: warning no ethernet card found at 0x%04X\n",
	       dep->de_name, dep->de_base_port);
	dep->de_mode = DEM_DISABLED;
    }
  }

  r = OK;

  /* 'de_mode' may change if probe routines fail, test again */
  switch (dep->de_mode) {

  case DEM_DISABLED:
    r = ENXIO;       /* Device is OFF or hardware probe failed */
    break;

  case DEM_ENABLED:
    if (dep->de_flags == DEF_EMPTY) {
	de_first_init(dep);
	dep->de_flags |= DEF_ENABLED;
	de_reset(dep);
	de_hw_conf(dep);
	de_setup_frame(dep);
	de_start(dep);
    }

    /* TODO CHECK PROMISC AND MULTI */
    dep->de_flags &= NOT(DEF_PROMISC | DEF_MULTI | DEF_BROAD);
    if (mp->DL_MODE & DL_PROMISC_REQ)
	dep->de_flags |= DEF_PROMISC | DEF_MULTI | DEF_BROAD;
    if (mp->DL_MODE & DL_MULTI_REQ) dep->de_flags |= DEF_MULTI;
    if (mp->DL_MODE & DL_BROAD_REQ) dep->de_flags |= DEF_BROAD;
    break;

  case DEM_SINK:
    DEBUG(printf("%s running in sink mode\n", str_DevName));
    memset(dep->de_address.ea_addr, 0, sizeof(ether_addr_t));
    de_conf_addr(dep);
    break;

  default:	break;
  }

  reply_mess.m_type = DL_CONF_REPLY;
  reply_mess.DL_STAT = r;
  if(r == OK){
    *(ether_addr_t *) reply_mess.DL_HWADDR = dep->de_address;
  }
  
  if (send(mp->m_source, &reply_mess) != OK)
    panic(str_SendErrMsg, mp->m_source);

  return;
}

static void do_reply(dpeth_t * dep)
{
  message reply;
  int r, flags = DL_NOFLAGS;

  if (dep->de_flags & DEF_ACK_SEND) flags |= DL_PACK_SEND;
  if (dep->de_flags & DEF_ACK_RECV) flags |= DL_PACK_RECV;

  reply.m_type = DL_TASK_REPLY;
  reply.DL_FLAGS = flags;
  reply.DL_COUNT = dep->de_read_s;

  r = send(dep->de_client, &reply);

  if(r < 0)
    panic(str_SendErrMsg, r);

  dep->de_read_s = 0;
  dep->de_flags &= NOT(DEF_ACK_SEND | DEF_ACK_RECV);
  return;
}

static void do_watchdog(void *UNUSED(message))
{
  /* nothing here yet */
  return;
}

static int de_probe(dpeth_t *dep, int skip)
{
  int i, r, devind;
  u16_t vid, did, temp16;

  DEBUG(printf("PROBING..."));
  
  r= pci_first_dev(&devind, &vid, &did);
  if (r == 0)
    return FALSE;

  while (skip--)
    {
      r= pci_next_dev(&devind, &vid, &did);
      if (!r)
	return FALSE;
    }

  pci_reserve(devind);

  dep->de_base_port = pci_attr_r32(devind, PCI_BAR) & 0xffffffe0;
  dep->de_irq = pci_attr_r8(devind, PCI_ILR);

  if (dep->de_base_port < DE_MIN_BASE_ADDR)
    panic("de_probe: base address invalid: %d", dep->de_base_port);

  DEBUG(printf("%s: using I/O address 0x%lx, IRQ %d\n",
	       dep->de_name, (unsigned long)dep->de_base_port, 
	       dep->de_irq));

  dep->de_type = pci_attr_r8(devind, PCI_REV);

  /* device validation. We support only the DEC21140A */
  if(dep->de_type != DEC_21140A){
    dep->de_type = DE_TYPE_UNKNOWN;
    printf("%s: unsupported device\n", str_DevName);
    return FALSE;
  }

  de_reset(dep);

  DEBUG(printf("Reading SROM...\n"));

  for(i=0;i<(1<<SROM_BITWIDTH)-1;i++){
    temp16 = de_read_rom(dep, i, SROM_BITWIDTH);
    dep->srom[i*2] = temp16 & 0xFF;
    dep->srom[i*2+1] = temp16 >> 8;
  }

  /* TODO: validate SROM content */
  /* acquire MAC addr */
  DEBUG(printf("Using MAC addr= "));
  for(i=0;i<6;i++){
    dep->de_address.ea_addr[i] = dep->srom[i+DE_SROM_EA_OFFSET];
    DEBUG(printf("%02X%c",dep->de_address.ea_addr[i],i!=5?'-':'\n'));
  }
  DEBUG(printf("probe success\n"));
  return TRUE;
}

static u16_t de_read_rom(const dpeth_t *dep, u8_t addr, u8_t nbAddrBits){
  u16_t retVal = 0;
  int i;
  u32_t csr = 0;
  u32_t csr2 = 0; /* csr2 is used to hold constant values that are
		     setup in the init phase, it makes this a little
		     more readable, the following macro is also just
		     to clear up the code a little.*/

  #define EMIT do { io_outl(CSR_ADDR(dep, CSR9), csr | csr2); io_outl(CSR_ADDR(dep, CSR1), 0);} while(0)

  /* init */
  csr = 0;                 EMIT;
  csr = CSR9_SR;           EMIT;
  csr = CSR9_SR | CSR9_RD; EMIT;

  csr2 = CSR9_SR | CSR9_RD;
  csr = 0;                 EMIT;
  csr2 |= CSR9_CS;

  csr = 0;                 EMIT;
  csr = CSR9_SRC;          EMIT;
  csr = 0;                 EMIT;

  /* cmd 110 - Read */
  csr = CSR9_DI;            EMIT;
  csr = CSR9_DI | CSR9_SRC; EMIT;
  csr = CSR9_DI;            EMIT;
  csr = CSR9_DI | CSR9_SRC; EMIT;
  csr = CSR9_DI;            EMIT;
  csr = 0;                  EMIT;
  csr = CSR9_SRC;           EMIT;
  csr = 0;                  EMIT;

  /* addr to read */
  for(i=nbAddrBits;i!=0;i--){
    csr = (addr&(1<<(i-1))) != 0 ? CSR9_DI : 0;  EMIT;
    csr ^= CSR9_SRC; EMIT;
    csr ^= CSR9_SRC; EMIT;
  }

  /* actual read */
  retVal=0;
  for(i=0;i<16;i++){
    retVal <<= 1;
    csr = CSR9_SRC; EMIT;
    retVal |= (io_inl(CSR_ADDR(dep, CSR9)) & CSR9_DO) == 0 ? 0 : 1;
    csr = 0; EMIT;
  }

  /* clean up */
  csr = 0;                 EMIT;

#undef EMIT
  return retVal;
}

static void de_update_conf(dpeth_t * dep)
{
  static char dpc_fmt[] = "x:d:x";
  char ec_key[16];
  long val;

  strlcpy(ec_key, "DEETH0", sizeof(ec_key));
  ec_key[5] += de_instance;

  dep->de_mode = DEM_ENABLED;
  switch (env_parse(ec_key, dpc_fmt, 0, &val, 0x000L, 0x3FFL)) {
  case EP_OFF:	dep->de_mode = DEM_DISABLED;	break;
  case EP_ON:  dep->de_mode = DEM_SINK; break;
  }
  dep->de_base_port = 0;
  
  return;
}

static void do_vread_s(const message * mp, int from_int)
{
  u8_t *buffer;
  u32_t size;
  int r, ix = 0;
  vir_bytes bytes;
  dpeth_t *dep = NULL;
  de_loc_descr_t *descr = NULL;
  iovec_dat_s_t *iovp = NULL;

  dep = &de_state;

  dep->de_client = mp->m_source;

  if (dep->de_mode == DEM_ENABLED) {    

    descr = &dep->descr[DESCR_RECV][dep->cur_descr[DESCR_RECV]];  

    /* check if packet is in the current descr and only there */
    if(  !( !(descr->descr->des[DES0] & DES0_OWN) &&  
	    (descr->descr->des[DES0] & DES0_FS)   &&
	    (descr->descr->des[DES0] & DES0_LS)     ))
      goto suspend;


    /*TODO: multi-descr msgs...*/
    /* We only support packets contained in a single descriptor.
       Setting the descriptor buffer size to less then
       ETH_MAX_PACK_SIZE will result in multi-descriptor
       packets that we won't be able to handle 
    */
    assert(!(descr->descr->des[DES0]&DES0_OWN));
    assert(descr->descr->des[DES0]&DES0_FS);
    assert(descr->descr->des[DES0]&DES0_LS);

    /* Check for abnormal messages. We assert here
       because this driver is for a virtualized 
       envrionment where we will not get bad packets
    */
    assert(!(descr->descr->des[DES0]&DES0_ES));
    assert(!(descr->descr->des[DES0]&DES0_RE));


    /* Setup the iovec entry to allow copying into
       client layer
    */
    dep->de_read_iovec.iod_proc_nr = mp->m_source;
    de_get_userdata_s(mp->m_source, (cp_grant_id_t) mp->DL_GRANT, 0,
		      mp->DL_COUNT, dep->de_read_iovec.iod_iovec);
    dep->de_read_iovec.iod_iovec_s = mp->DL_COUNT;
    dep->de_read_iovec.iod_grant = (cp_grant_id_t) mp->DL_GRANT;
    dep->de_read_iovec.iod_iovec_offset = 0;
    size = de_calc_iov_size(&dep->de_read_iovec);
    if (size < ETH_MAX_PACK_SIZE) 
      panic(str_SizeErrMsg, size);

    /* Copy buffer to user area  and clear ownage */
    size = (descr->descr->des[DES0]&DES0_FL)>>DES0_FL_SHIFT;

    /*TODO: Complain to MS */
    /*HACK: VPC2007 returns packet of invalid size. Ethernet standard
      specify 46 bytes as the minimum for valid payload. However, this is 
      artificial in so far as for certain packet types, notably ARP, less
      then 46 bytes are needed to contain the full information. In a non 
      virtualized environment the 46 bytes rule is enforced in order to give
      guarantee in the collison detection scheme. Of course, this being a 
      driver for a VPC2007, we won't have collisions and I can only suppose
      MS decided to cut packet size to true minimum, regardless of the 
      46 bytes payload standard. Note that this seems to not happen in 
      bridged mode. Note also, that the card does not return runt or 
      incomplete frames to us, so this hack is safe
    */    
    if(size<60){
      bzero(&descr->buf1[size], 60-size);
      size=60;
    }
    /* End ugly hack */

    iovp = &dep->de_read_iovec;
    buffer = descr->buf1;
    dep->bytes_rx += size;
    dep->de_stat.ets_packetR++;
    dep->de_read_s = size;

    do {   
      bytes = iovp->iod_iovec[ix].iov_size;	/* Size of buffer */
      if (bytes >= size) 
	bytes = size;

      r= sys_safecopyto(iovp->iod_proc_nr, iovp->iod_iovec[ix].iov_grant, 0,
			(vir_bytes)buffer, bytes);
      if (r != OK)
	panic(str_CopyErrMsg, r);
      buffer += bytes;
      
      if (++ix >= IOVEC_NR) {	/* Next buffer of IO vector */
	de_next_iov(iovp);
	ix = 0;
      }
    } while ((size -= bytes) > 0);

    descr->descr->des[DES0]=DES0_OWN;
    dep->cur_descr[DESCR_RECV]++;
    if(dep->cur_descr[DESCR_RECV] >= DE_NB_RECV_DESCR)
      dep->cur_descr[DESCR_RECV] = 0;	  

    DEBUG(printf("Read returned size = %d\n", size));

    /* Reply information */
    dep->de_flags |= DEF_ACK_RECV;
    dep->de_flags &= NOT(DEF_READING);
  }

  if(!from_int){
    do_reply(dep);
  }
  return;

 suspend:
  if(from_int){
    assert(dep->de_flags & DEF_READING);
    return;
  }

  assert(!(dep->de_flags & DEF_READING));
  dep->rx_return_msg = *mp;
  dep->de_flags |= DEF_READING;
  do_reply(dep);
  return;
}

static void de_conf_addr(dpeth_t * dep)
{
  static char ea_fmt[] = "x:x:x:x:x:x";
  char ea_key[16];
  int ix;
  long val;

  strlcpy(ea_key, "DEETH0_EA", sizeof(ea_key));
  ea_key[5] += de_instance;

  for (ix = 0; ix < SA_ADDR_LEN; ix++) {
	val = dep->de_address.ea_addr[ix];
	if (env_parse(ea_key, ea_fmt, ix, &val, 0x00L, 0xFFL) != EP_SET)
		break;
	dep->de_address.ea_addr[ix] = val;
  }

  if (ix != 0 && ix != SA_ADDR_LEN)
	env_parse(ea_key, "?", 0, &val, 0L, 0L);
  return;
}

static void de_first_init(dpeth_t *dep)
{
  int i,j,r;
  vir_bytes descr_vir = (vir_bytes)dep->sendrecv_descr_buf;
  vir_bytes buffer_vir = (vir_bytes)dep->sendrecv_buf;
  de_loc_descr_t *loc_descr;
  u32_t temp;


  for(i=0;i<2;i++){
    loc_descr = &dep->descr[i][0];
    for(j=0; j < (i==DESCR_RECV ? DE_NB_RECV_DESCR : DE_NB_SEND_DESCR); j++){

      /* assign buffer space for descriptor */
      loc_descr->descr = (void*)descr_vir;
      descr_vir += sizeof(de_descr_t);

      /* assign space for buffer */
      loc_descr->buf1 = (u8_t*)buffer_vir; 
      buffer_vir += (i==DESCR_RECV ? DE_RECV_BUF_SIZE : DE_SEND_BUF_SIZE);
      loc_descr->buf2 = 0;
      loc_descr++;
    }
  }

  /* Now that we have buffer space and descriptors, we need to
     obtain their physical address to pass to the hardware
  */
  for(i=0;i<2;i++){
    loc_descr = &dep->descr[i][0];
    temp = (i==DESCR_RECV ? DE_RECV_BUF_SIZE : DE_SEND_BUF_SIZE);
    for(j=0; j < (i==DESCR_RECV ? DE_NB_RECV_DESCR : DE_NB_SEND_DESCR); j++){
      /* translate buffers physical address */
      r = sys_umap(SELF, VM_D, (vir_bytes)loc_descr->buf1, temp, 
		   &(loc_descr->descr->des[DES_BUF1]));
      if(r != OK) panic("umap failed: %d", r);      
      loc_descr->descr->des[DES_BUF2] = 0;
      memset(&loc_descr->descr->des[DES0],0,sizeof(u32_t));
      loc_descr->descr->des[DES1] = temp;
      if(j==( (i==DESCR_RECV?DE_NB_RECV_DESCR:DE_NB_SEND_DESCR)-1))
	loc_descr->descr->des[DES1] |= DES1_ER;
      if(i==DESCR_RECV)
	loc_descr->descr->des[DES0] |= DES0_OWN;
      loc_descr++;
    }
  }
  
  /* record physical location of two first descriptor */
  r = sys_umap(SELF, VM_D, (vir_bytes)dep->descr[DESCR_RECV][0].descr, 
	       sizeof(de_descr_t), &dep->sendrecv_descr_phys_addr[DESCR_RECV]);
  if(r != OK) panic(str_UmapErrMsg, r);

  r = sys_umap(SELF, VM_D, (vir_bytes)dep->descr[DESCR_TRAN][0].descr,
	       sizeof(de_descr_t), &dep->sendrecv_descr_phys_addr[DESCR_TRAN]);
  if(r != OK) panic(str_UmapErrMsg, r);

  DEBUG(printf("Descr: head tran=[%08X] head recv=[%08X]\n",
	       dep->sendrecv_descr_phys_addr[DESCR_TRAN],
	       dep->sendrecv_descr_phys_addr[DESCR_RECV]));

  /* check alignment just to be extra safe */
  for(i=0;i<2;i++){
    loc_descr = &dep->descr[i][0];
    for(j=0;j< (i==DESCR_RECV?DE_NB_RECV_DESCR:DE_NB_SEND_DESCR);j++){
      r = sys_umap(SELF, VM_D, (vir_bytes)&(loc_descr->descr),
			sizeof(de_descr_t), &temp);
      if(r != OK)
	panic(str_UmapErrMsg, r);

      if( ((loc_descr->descr->des[DES_BUF1] & 0x3) != 0) ||
	  ((loc_descr->descr->des[DES_BUF2] & 0x3) != 0) ||
	  ((temp&0x3)!=0) )
	panic(str_AlignErrMsg, temp);

      loc_descr++;
    }
  }
  
  /* Init default values */
  dep->cur_descr[DESCR_TRAN]=1;
  dep->cur_descr[DESCR_RECV]=0;
  dep->bytes_rx = 0;
  dep->bytes_tx = 0;
  
  /* Set the interrupt handler policy. Request interrupts not to be reenabled
   * automatically. Return the IRQ line number when an interrupt occurs.
   */
  dep->de_hook = dep->de_irq;
  sys_irqsetpolicy(dep->de_irq, 0, &dep->de_hook);
  sys_irqenable(&dep->de_hook);
}

static void do_interrupt(const dpeth_t *dep){  
  u32_t val;
  val = io_inl(CSR_ADDR(dep, CSR5));

  if(val & CSR5_AIS){
    panic("Abnormal Int CSR5=: %d", val);
  }

  if( (dep->de_flags & DEF_READING) && (val & CSR5_RI) ){
    do_vread_s(&dep->rx_return_msg, TRUE);
  }
 
  if( (dep->de_flags & DEF_SENDING) && (val & CSR5_TI) ){
    do_vwrite_s(&dep->tx_return_msg, TRUE);
  }
  
  /* ack and reset interrupts */
  io_outl(CSR_ADDR(dep, CSR5), 0xFFFFFFFF);
  return;
}

static void de_reset(const dpeth_t *dep){
  io_outl(CSR_ADDR(dep, CSR0), CSR0_SWR);
}

static void de_hw_conf(const dpeth_t *dep){
  u32_t val;

  /* CSR0 - global host bus prop */
  val = CSR0_BAR | CSR0_CAL_8;
  io_outl(CSR_ADDR(dep, CSR0), val);

  /* CSR3 - Receive list BAR */
  val = dep->sendrecv_descr_phys_addr[DESCR_RECV];
  io_outl(CSR_ADDR(dep, CSR3), val);

  /* CSR4 - Transmit list BAR */
  val = dep->sendrecv_descr_phys_addr[DESCR_TRAN];
  io_outl(CSR_ADDR(dep, CSR4), val);

  /* CSR7 - interrupt mask */
  val = CSR7_TI | CSR7_RI | CSR7_AI;
  io_outl(CSR_ADDR(dep, CSR7), val);

  /* CSR6 - operating mode register */
  val = CSR6_MBO | CSR6_PS | CSR6_FD | CSR6_HBD | 
    CSR6_PCS | CSR6_SCR | CSR6_TR_00;
  io_outl(CSR_ADDR(dep, CSR6), val);
}

static void de_start(const dpeth_t *dep){  
  u32_t val;
  val = io_inl(CSR_ADDR(dep, CSR6)) | CSR6_ST | CSR6_SR;
  io_outl(CSR_ADDR(dep, CSR6), val);
}

static void de_setup_frame(const dpeth_t *dep){
  int i;
  u32_t val;

  /* this is not perfect... we assume pass all multicast and only
     filter non-multicast frames */
  dep->descr[DESCR_TRAN][0].buf1[0] = 0xFF;
  dep->descr[DESCR_TRAN][0].buf1[1] = 0xFF;
  dep->descr[DESCR_TRAN][0].buf1[4] = 0xFF;
  dep->descr[DESCR_TRAN][0].buf1[5] = 0xFF;
  dep->descr[DESCR_TRAN][0].buf1[8] = 0xFF;
  dep->descr[DESCR_TRAN][0].buf1[9] = 0xFF;
  for(i=1;i<16;i++){
    memset(&(dep->descr[DESCR_TRAN][0].buf1[12*i]), 0, 12);
    dep->descr[DESCR_TRAN][0].buf1[12*i+0] = dep->de_address.ea_addr[0];
    dep->descr[DESCR_TRAN][0].buf1[12*i+1] = dep->de_address.ea_addr[1];
    dep->descr[DESCR_TRAN][0].buf1[12*i+4] = dep->de_address.ea_addr[2];
    dep->descr[DESCR_TRAN][0].buf1[12*i+5] = dep->de_address.ea_addr[3];
    dep->descr[DESCR_TRAN][0].buf1[12*i+8] = dep->de_address.ea_addr[4];
    dep->descr[DESCR_TRAN][0].buf1[12*i+9] = dep->de_address.ea_addr[5];
  }

  dep->descr[DESCR_TRAN][0].descr->des[DES0] = DES0_OWN;
  dep->descr[DESCR_TRAN][0].descr->des[DES1] = DES1_SET | 
    DE_SETUP_FRAME_SIZE | DES1_IC;

  /* start transmit process to process setup frame */
  val = io_inl(CSR_ADDR(dep, CSR6)) | CSR6_ST;
  io_outl(CSR_ADDR(dep, CSR6), val); 
  io_outl(CSR_ADDR(dep, CSR1), 0xFFFFFFFF);

  return;
}

static int de_calc_iov_size(iovec_dat_s_t * iovp){
  int size, ix;
  size = ix = 0;
  
  do{
    size += iovp->iod_iovec[ix].iov_size;
    if (++ix >= IOVEC_NR) {
      de_next_iov(iovp);
      ix = 0;
    }
  } while (ix < iovp->iod_iovec_s);
  return size;
}

static void de_get_userdata_s(int user_proc, cp_grant_id_t grant,
	vir_bytes offset, int count, void *loc_addr){
  int rc;
  vir_bytes len;

  len = (count > IOVEC_NR ? IOVEC_NR : count) * sizeof(iovec_t);
  rc = sys_safecopyfrom(user_proc, grant, 0, (vir_bytes)loc_addr, len);
  if (rc != OK)
    panic(str_CopyErrMsg, rc);
  return;
}

static void de_next_iov(iovec_dat_s_t * iovp){

  iovp->iod_iovec_s -= IOVEC_NR;
  iovp->iod_iovec_offset += IOVEC_NR * sizeof(iovec_t);
  de_get_userdata_s(iovp->iod_proc_nr, iovp->iod_grant, iovp->iod_iovec_offset,
	     iovp->iod_iovec_s, iovp->iod_iovec);
  return;
}

static void do_vwrite_s(const message * mp, int from_int){
  static u8_t setupDone = 0;
  int size, r, bytes, ix, totalsize;
  dpeth_t *dep;
  iovec_dat_s_t *iovp = NULL;
  de_loc_descr_t *descr = NULL;
  u8_t *buffer = NULL;

  dep = &de_state;

  dep->de_client = mp->m_source;

  if (dep->de_mode == DEM_ENABLED) {
    
    if (!from_int && (dep->de_flags & DEF_SENDING))
      panic(str_BusyErrMsg);

    descr = &dep->descr[DESCR_TRAN][dep->cur_descr[DESCR_TRAN]];

    if(( descr->descr->des[DES0] & DES0_OWN)!=0)
      goto suspend;

    if(!setupDone && (dep->cur_descr[DESCR_TRAN] == 0) ){
      dep->descr[DESCR_TRAN][0].descr->des[DES0] = 0;
      setupDone=1;
    }

    buffer = descr->buf1;
    iovp = &dep->de_write_iovec;
    iovp->iod_proc_nr = mp->m_source;
    de_get_userdata_s(mp->m_source, mp->DL_GRANT, 0,
		      mp->DL_COUNT, iovp->iod_iovec);
    iovp->iod_iovec_s = mp->DL_COUNT;
    iovp->iod_grant = (cp_grant_id_t) mp->DL_GRANT;
    iovp->iod_iovec_offset = 0;
    totalsize = size = de_calc_iov_size(iovp);
    if (size < ETH_MIN_PACK_SIZE || size > ETH_MAX_PACK_SIZE)
      panic(str_SizeErrMsg, size);

    dep->bytes_tx += size;
    dep->de_stat.ets_packetT++;

    ix=0;
    do {
      bytes = iovp->iod_iovec[ix].iov_size;
      if (bytes >= size) 
	bytes = size;		

      r= sys_safecopyfrom(iovp->iod_proc_nr, iovp->iod_iovec[ix].iov_grant,
			  0, (vir_bytes)buffer, bytes);
      if (r != OK)
	panic(str_CopyErrMsg, r);
      buffer += bytes;

      if (++ix >= IOVEC_NR) {	
	de_next_iov(iovp);
	ix = 0;
      }
    } while ((size -= bytes) > 0);

    descr->descr->des[DES1] = (descr->descr->des[DES1]&DES1_ER) | 
      DES1_FS | DES1_LS | DES1_IC | totalsize;
    descr->descr->des[DES0] = DES0_OWN;

    dep->cur_descr[DESCR_TRAN]++;
    if(dep->cur_descr[DESCR_TRAN] >= DE_NB_SEND_DESCR)
      dep->cur_descr[DESCR_TRAN] = 0;

    io_outl(CSR_ADDR(dep, CSR1), 0xFFFFFFFF);
  }
  
  dep->de_flags |= DEF_ACK_SEND;
  if(from_int){
    dep->de_flags &= NOT(DEF_SENDING);
    return;
  }
  do_reply(dep);
  return;

 suspend:
  if(from_int)
    panic("should not happen: %d", 0);

  dep->de_stat.ets_transDef++;
  dep->de_flags |= DEF_SENDING;
  dep->de_stat.ets_transDef++;
  dep->tx_return_msg = *mp;
  do_reply(dep);
}

static void warning(const char *type, int err){
  printf("Warning: %s sys_%s failed (%d)\n", str_DevName, type, err);
  return;
}

static u32_t io_inl(u16_t port){
  u32_t value;
  int rc;
  if ((rc = sys_inl(port, &value)) != OK) warning("inl", rc);
  return value;
}

static void io_outl(u16_t port, u32_t value){
  int rc;
  if ((rc = sys_outl(port, value)) != OK) warning("outl", rc);
  return;
}
