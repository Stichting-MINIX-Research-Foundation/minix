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

#include <machine/pci.h>
#include <assert.h>

#include "dec21140A.h"

static u32_t io_inl(u16_t);
static void io_outl(u16_t, u32_t);

static int do_init(unsigned int, netdriver_addr_t *, uint32_t *,
	unsigned int *);
static void do_stop(void);
static int do_send(struct netdriver_data *, size_t);
static ssize_t do_recv(struct netdriver_data *, size_t);
static void do_intr(unsigned int);

static int de_probe(dpeth_t *, unsigned int skip);
static void de_conf_addr(dpeth_t *, netdriver_addr_t *);
static void de_init_buf(dpeth_t *);
static void de_reset(const dpeth_t *);
static void de_hw_conf(const dpeth_t *);
static void de_start(const dpeth_t *);
static void de_setup_frame(const dpeth_t *, const netdriver_addr_t *);
static u16_t de_read_rom(const dpeth_t *, u8_t, u8_t);

static dpeth_t de_state;

static const struct netdriver de_table = {
	.ndr_name	= "dec",
	.ndr_init	= do_init,
	.ndr_stop	= do_stop,
	.ndr_recv	= do_recv,
	.ndr_send	= do_send,
	.ndr_intr	= do_intr
};

int main(int argc, char *argv[])
{
  env_setargs(argc, argv);

  netdriver_task(&de_table);

  return 0;
}

static void de_init_hw(dpeth_t *dep, netdriver_addr_t *addr)
{
  de_reset(dep);
  de_conf_addr(dep, addr);
  de_init_buf(dep);

  /* Set the interrupt handler policy. Request interrupts not to be reenabled
   * automatically. Return the IRQ line number when an interrupt occurs.
   */
  dep->de_hook = dep->de_irq;
  sys_irqsetpolicy(dep->de_irq, 0, &dep->de_hook);
  sys_irqenable(&dep->de_hook);

  de_reset(dep);
  de_hw_conf(dep);
  de_setup_frame(dep, addr);
  de_start(dep);
}

static int do_init(unsigned int instance, netdriver_addr_t *addr,
	uint32_t *caps, unsigned int *ticks)
{
/* Initialize the DEC 21140A driver. */
  dpeth_t *dep;

  dep = &de_state;
  memset(dep, 0, sizeof(*dep));

  if (!de_probe(dep, instance))
    return ENXIO;

  de_init_hw(dep, addr);

  *caps = NDEV_CAP_MCAST | NDEV_CAP_BCAST;
  return OK;
}

static int de_probe(dpeth_t *dep, unsigned int skip)
{
  int r, devind;
  u16_t vid, did;

  DEBUG(printf("PROBING..."));

  pci_init();

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
	       netdriver_name(), (unsigned long)dep->de_base_port,
	       dep->de_irq));

  dep->de_type = pci_attr_r8(devind, PCI_REV);

  /* device validation. We support only the DEC21140A */
  if(dep->de_type != DEC_21140A){
    printf("%s: unsupported card type %x\n", netdriver_name(),
      dep->de_type);
    return FALSE;
  }

  return TRUE;
}

static u16_t de_read_rom(const dpeth_t *dep, u8_t addr, u8_t nbAddrBits)
{
  u16_t retVal = 0;
  int i;
  u32_t csr = 0;
  u32_t csr2 = 0; /* csr2 is used to hold constant values that are
		     setup in the init phase, it makes this a little
		     more readable, the following macro is also just
		     to clear up the code a little.*/

#define EMIT \
  do { \
    io_outl(CSR_ADDR(dep, CSR9), csr | csr2); \
    io_outl(CSR_ADDR(dep, CSR1), 0); \
  } while(0)

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

static ssize_t do_recv(struct netdriver_data *data, size_t max)
{
  u32_t size;
  dpeth_t *dep;
  de_loc_descr_t *descr;

  dep = &de_state;

  descr = &dep->descr[DESCR_RECV][dep->cur_descr[DESCR_RECV]];

  /* check if packet is in the current descr and only there */
  if ((descr->descr->des[DES0] & DES0_OWN) ||
      !(descr->descr->des[DES0] & DES0_FS) ||
      !(descr->descr->des[DES0] & DES0_LS))
    return SUSPEND;

  /*TODO: multi-descr msgs...*/
  /* We only support packets contained in a single descriptor.
     Setting the descriptor buffer size to less then
     ETH_MAX_PACK_SIZE will result in multi-descriptor
     packets that we won't be able to handle
  */

  /* Check for abnormal messages. We assert here
     because this driver is for a virtualized
     envrionment where we will not get bad packets
  */
  assert(!(descr->descr->des[DES0]&DES0_ES));
  assert(!(descr->descr->des[DES0]&DES0_RE));

  /* Copy buffer to user area and clear ownage */
  size = (descr->descr->des[DES0]&DES0_FL)>>DES0_FL_SHIFT;

  /* HACK: VPC2007 sends short-sized packets, pad to minimum ethernet length */
  if(size<60){
    memset(&descr->buf1[size], 0, 60-size);
    size=60;
  }

  /* Truncate large packets */
  if (size > max)
    size = max;

  netdriver_copyout(data, 0, descr->buf1, size);

  descr->descr->des[DES0]=DES0_OWN;
  dep->cur_descr[DESCR_RECV]++;
  if(dep->cur_descr[DESCR_RECV] >= DE_NB_RECV_DESCR)
    dep->cur_descr[DESCR_RECV] = 0;

  DEBUG(printf("Read returned size = %d\n", size));

  return size;
}

static void de_conf_addr(dpeth_t *dep, netdriver_addr_t *addr)
{
  u16_t temp16;
  int i;

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
    addr->na_addr[i] = dep->srom[i+DE_SROM_EA_OFFSET];
    DEBUG(printf("%02X%c", addr->na_addr[i],i!=5?'-':'\n'));
  }
  DEBUG(printf("probe success\n"));
}

static void de_init_buf(dpeth_t *dep)
{
  int i,j,r;
  vir_bytes descr_vir = (vir_bytes)dep->sendrecv_descr_buf;
  vir_bytes buffer_vir = (vir_bytes)dep->sendrecv_buf;
  de_loc_descr_t *loc_descr;
  phys_bytes temp;

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
		   (phys_bytes *) &(loc_descr->descr->des[DES_BUF1]));
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
  if(r != OK) panic("sys_umap failed: %d", r);

  r = sys_umap(SELF, VM_D, (vir_bytes)dep->descr[DESCR_TRAN][0].descr,
	       sizeof(de_descr_t), &dep->sendrecv_descr_phys_addr[DESCR_TRAN]);
  if(r != OK) panic("sys_umap failed: %d", r);

  DEBUG(printf("Descr: head tran=[%08X] head recv=[%08X]\n",
	       dep->sendrecv_descr_phys_addr[DESCR_TRAN],
	       dep->sendrecv_descr_phys_addr[DESCR_RECV]));

  /* check alignment just to be extra safe */
  for(i=0;i<2;i++){
    loc_descr = &dep->descr[i][0];
    for(j=0;j< (i==DESCR_RECV?DE_NB_RECV_DESCR:DE_NB_SEND_DESCR);j++){
      r = sys_umap(SELF, VM_D, (vir_bytes)&(loc_descr->descr),
			sizeof(de_descr_t), &temp);
      if(r != OK) panic("sys_umap failed: %d", r);

      if( ((loc_descr->descr->des[DES_BUF1] & 0x3) != 0) ||
	  ((loc_descr->descr->des[DES_BUF2] & 0x3) != 0) ||
	  ((temp&0x3)!=0) )
	panic("alignment error: 0x%lx", temp);

      loc_descr++;
    }
  }

  /* Init default values */
  dep->cur_descr[DESCR_TRAN]=1;
  dep->cur_descr[DESCR_RECV]=0;
}

static void do_intr(unsigned int __unused mask)
{
  dpeth_t *dep;
  u32_t val;

  dep = &de_state;

  val = io_inl(CSR_ADDR(dep, CSR5));

  if(val & CSR5_AIS){
    panic("Abnormal Int CSR5=: %d", val);
  }

  if (val & CSR5_RI)
    netdriver_recv();

  if (val & CSR5_TI)
    netdriver_send();

  /* ack and reset interrupts */
  io_outl(CSR_ADDR(dep, CSR5), 0xFFFFFFFF);

  sys_irqenable(&dep->de_hook);
}

static void de_reset(const dpeth_t *dep)
{
  io_outl(CSR_ADDR(dep, CSR0), CSR0_SWR);
}

static void do_stop(void)
{
  de_reset(&de_state);
}

static void de_hw_conf(const dpeth_t *dep)
{
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

static void de_start(const dpeth_t *dep)
{
  u32_t val;
  val = io_inl(CSR_ADDR(dep, CSR6)) | CSR6_ST | CSR6_SR;
  io_outl(CSR_ADDR(dep, CSR6), val);
}

static void de_setup_frame(const dpeth_t *dep, const netdriver_addr_t *addr)
{
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
    dep->descr[DESCR_TRAN][0].buf1[12*i+0] = addr->na_addr[0];
    dep->descr[DESCR_TRAN][0].buf1[12*i+1] = addr->na_addr[1];
    dep->descr[DESCR_TRAN][0].buf1[12*i+4] = addr->na_addr[2];
    dep->descr[DESCR_TRAN][0].buf1[12*i+5] = addr->na_addr[3];
    dep->descr[DESCR_TRAN][0].buf1[12*i+8] = addr->na_addr[4];
    dep->descr[DESCR_TRAN][0].buf1[12*i+9] = addr->na_addr[5];
  }

  dep->descr[DESCR_TRAN][0].descr->des[DES0] = DES0_OWN;
  dep->descr[DESCR_TRAN][0].descr->des[DES1] = DES1_SET |
    DE_SETUP_FRAME_SIZE | DES1_IC;

  /* start transmit process to process setup frame */
  val = io_inl(CSR_ADDR(dep, CSR6)) | CSR6_ST;
  io_outl(CSR_ADDR(dep, CSR6), val);
  io_outl(CSR_ADDR(dep, CSR1), 0xFFFFFFFF);
}

static int do_send(struct netdriver_data *data, size_t size)
{
  static int setup_done = 0;
  dpeth_t *dep;
  de_loc_descr_t *descr = NULL;

  dep = &de_state;

  descr = &dep->descr[DESCR_TRAN][dep->cur_descr[DESCR_TRAN]];

  if(( descr->descr->des[DES0] & DES0_OWN)!=0)
    return SUSPEND;

  if(!setup_done && (dep->cur_descr[DESCR_TRAN] == 0) ){
    dep->descr[DESCR_TRAN][0].descr->des[DES0] = 0;
    setup_done=1;
  }

  netdriver_copyin(data, 0, descr->buf1, size);

  descr->descr->des[DES1] = (descr->descr->des[DES1]&DES1_ER) |
    DES1_FS | DES1_LS | DES1_IC | size;
  descr->descr->des[DES0] = DES0_OWN;

  dep->cur_descr[DESCR_TRAN]++;
  if(dep->cur_descr[DESCR_TRAN] >= DE_NB_SEND_DESCR)
    dep->cur_descr[DESCR_TRAN] = 0;

  io_outl(CSR_ADDR(dep, CSR1), 0xFFFFFFFF);

  return OK;
}

static u32_t io_inl(u16_t port)
{
  u32_t value;
  int rc;
  if ((rc = sys_inl(port, &value)) != OK)
    panic("sys_inl failed: %d", rc);
  return value;
}

static void io_outl(u16_t port, u32_t value)
{
  int rc;
  if ((rc = sys_outl(port, value)) != OK)
    panic("sys_outl failed: %d", rc);
}
