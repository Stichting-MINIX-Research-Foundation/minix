/*
 * lance.c
 *
 * This file contains a ethernet device driver for AMD LANCE based ethernet
 * cards.
 *
 * The valid messages and their parameters are:
 *
 *   m_type       DL_PORT    DL_PROC   DL_COUNT   DL_MODE   DL_ADDR
 * |------------+----------+---------+----------+---------+---------|
 * | HARDINT    |          |         |          |         |         |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_WRITE   | port nr  | proc nr | count    | mode    | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_WRITEV  | port nr  | proc nr | count    | mode    | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_READ    | port nr  | proc nr | count    |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_READV   | port nr  | proc nr | count    |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_INIT    | port nr  | proc nr | mode     |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_GETSTAT | port nr  | proc nr |          |         | address |
 * |------------|----------|---------|----------|---------|---------|
 * | DL_STOP    | port_nr  |         |          |         |         |
 * |------------|----------|---------|----------|---------|---------|
 *
 * The messages sent are:
 *
 *   m-type       DL_POR T   DL_PROC   DL_COUNT   DL_STAT   DL_CLCK
 * |------------|----------|---------|----------|---------|---------|
 * |DL_TASK_REPL| port nr  | proc nr | rd-count | err|stat| clock   |
 * |------------|----------|---------|----------|---------|---------|
 *
 *   m_type       m3_i1     m3_i2       m3_ca1
 * |------------+---------+-----------+---------------|
 * |DL_INIT_REPL| port nr | last port | ethernet addr |
 * |------------|---------|-----------|---------------|
 *
 * Created: Jul 27, 2002 by Kazuya Kodama <kazuya@nii.ac.jp>
 * Adapted for Minix 3: Sep 05, 2005 by Joren l'Ami <jwlami@cs.vu.nl>
 */

#define VERBOSE 0

#include "../drivers.h"

#include <minix/keymap.h>
#include <net/hton.h>
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>
#include <assert.h>

#include <minix/syslib.h>

#include "lance.h"
#include "../libpci/pci.h"
/*#include "proc.h"*/

#include <sys/ioc_memory.h>

/* new I/O functions in Minix 3 */
#define out_byte( x, y ) sys_outb( x, y )
#define out_word( x, y ) sys_outw( x, y )

static U8_t in_byte(U16_t port)
{
	U8_t value;
	int s;
	if ((s=sys_inb(port, &value)) != OK)
		printf( "lance: warning, sys_inb failed: %d\n", s );
	return value;
}

static U16_t in_word( U16_t port)
{
	U16_t value;
	int s;
	if ((s=sys_inw(port, &value)) != OK)
		printf( "lance: warning, sys_inw failed: %d\n", s );
	return value;
}
/*
#define in_byte( x ) inb( x )
#define in_word( x ) inw( x )
*/

static ether_card_t ec_table[EC_PORT_NR_MAX];
static int eth_tasknr= ANY;
static u16_t eth_ign_proto;

/* Configuration */
typedef struct ec_conf
{
  port_t ec_port;
  int ec_irq;
  phys_bytes ec_mem;
  char *ec_envvar;
} ec_conf_t;

/* We hardly use these. Just "LANCE0=on/off" "LANCE1=on/off" mean. */
ec_conf_t ec_conf[]=    /* Card addresses */
{
        /* I/O port, IRQ,  Buffer address,  Env. var,   Buf selector. */
        {  0x1000,     9,    0x00000,        "LANCE0" },
        {  0xD000,    15,    0x00000,        "LANCE1" },
};

/* Actually, we use PCI-BIOS info. */
PRIVATE struct pcitab
{
	u16_t vid;
	u16_t did;
	int checkclass;
} pcitab[]=
{
	{ PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_LANCE, 0 },		/* AMD LANCE */

	{ 0x0000, 0x0000, 0 }
};
/*
struct pci_device pci_dev_list[] = {
  { PCI_VENDOR_ID_AMD,          PCI_DEVICE_ID_AMD_LANCE,
    "AMD Lance/PCI",            0, 0, 0, 0, 0, 0}, 
  { PCI_VENDOR_ID_AMD,          PCI_DEVICE_ID_AMD_LANCE,
    "AMD Lance/PCI",            0, 0, 0, 0, 0, 0}, 
  {0, 0, NULL, 0, 0, 0, 0, 0, 0}
};
*/

/* General */
_PROTOTYPE( static void do_init, (message *mp)                          );
_PROTOTYPE( static void ec_init, (ether_card_t *ec)                     );
_PROTOTYPE( static void ec_confaddr, (ether_card_t *ec)                 );
_PROTOTYPE( static void ec_reinit, (ether_card_t *ec)                   );
_PROTOTYPE( static void ec_check_ints, (ether_card_t *ec)               );
_PROTOTYPE( static void conf_hw, (ether_card_t *ec)                     );
/*_PROTOTYPE( static int ec_handler, (irq_hook_t *hook)                   );*/
_PROTOTYPE( static void update_conf, (ether_card_t *ec, ec_conf_t *ecp) );
_PROTOTYPE( static void mess_reply, (message *req, message *reply)      );
_PROTOTYPE( static void do_int, (ether_card_t *ec)                      );
_PROTOTYPE( static void reply, 
	    (ether_card_t *ec, int err, int may_block)                  );
_PROTOTYPE( static void ec_reset, (ether_card_t *ec)                    );
_PROTOTYPE( static void ec_send, (ether_card_t *ec)                     );
_PROTOTYPE( static void ec_recv, (ether_card_t *ec)                     );
_PROTOTYPE( static void do_vwrite, 
	    (message *mp, int from_int, int vectored)                   );
_PROTOTYPE( static void do_vread, (message *mp, int vectored)           );
_PROTOTYPE( static void get_userdata, 
	    (int user_proc, vir_bytes user_addr, 
	     vir_bytes count, void *loc_addr)                           );
_PROTOTYPE( static void ec_user2nic, 
	    (ether_card_t *dep, iovec_dat_t *iovp, 
	     vir_bytes offset, int nic_addr, 
	     vir_bytes count)                                           );
_PROTOTYPE( static void ec_nic2user, 
	    (ether_card_t *ec, int nic_addr, 
	     iovec_dat_t *iovp, vir_bytes offset, 
	     vir_bytes count)                                           );
_PROTOTYPE( static int calc_iovec_size, (iovec_dat_t *iovp)             );
_PROTOTYPE( static void ec_next_iovec, (iovec_dat_t *iovp)              );
_PROTOTYPE( static void do_getstat, (message *mp)                       );
_PROTOTYPE( static void put_userdata, 
	    (int user_proc,
	     vir_bytes user_addr, vir_bytes count, 
	     void *loc_addr)                                            );
_PROTOTYPE( static void do_stop, (message *mp)                          );

_PROTOTYPE( static void lance_dump, (void)				);
_PROTOTYPE( static void lance_stop, (void)				);
_PROTOTYPE( static void getAddressing, (int devind, ether_card_t *ec)	);

/* probe+init LANCE cards */
_PROTOTYPE( static int lance_probe, (ether_card_t *ec)                  );
_PROTOTYPE( static void lance_init_card, (ether_card_t *ec)             );

/* --- LANCE --- */
/* General */
#define Address                 unsigned long


/* Minix 3 */
#define virt_to_bus(x)          (vir2phys((unsigned long)x))
unsigned long vir2phys( unsigned long x )
{
	int r;
	unsigned long value;
	
	if ( (r=sys_umap( SELF, D, x, 4, &value )) != OK )
		panic( "lance", "sys_umap failed", r );
	
	return value;
}

/* DMA limitations */
#define DMA_ADDR_MASK  0xFFFFFF	/* mask to verify DMA address is 24-bit */

#define CORRECT_DMA_MEM() ( (virt_to_bus(lance + sizeof(lance)) & ~DMA_ADDR_MASK) == 0 )

#define ETH_FRAME_LEN           1518

#define LANCE_MUST_PAD          0x00000001
#define LANCE_ENABLE_AUTOSELECT 0x00000002
#define LANCE_SELECT_PHONELINE  0x00000004
#define LANCE_MUST_UNRESET      0x00000008

static const struct lance_chip_type
{
  int        id_number;
  const char *name;
  int        flags;
} chip_table[] = {
  {0x0000, "LANCE 7990",           /* Ancient lance chip.  */
   LANCE_MUST_PAD + LANCE_MUST_UNRESET},
  {0x0003, "PCnet/ISA 79C960",     /* 79C960 PCnet/ISA.  */
   LANCE_ENABLE_AUTOSELECT},
  {0x2260, "PCnet/ISA+ 79C961",    /* 79C961 PCnet/ISA+, Plug-n-Play.  */
   LANCE_ENABLE_AUTOSELECT},
  {0x2420, "PCnet/PCI 79C970",     /* 79C970 or 79C974 PCnet-SCSI, PCI. */
   LANCE_ENABLE_AUTOSELECT},
  {0x2430, "PCnet32",              /* 79C965 PCnet for VL bus. */
   LANCE_ENABLE_AUTOSELECT},
  {0x2621, "PCnet/PCI-II 79C970A", /* 79C970A PCInetPCI II. */
   LANCE_ENABLE_AUTOSELECT},
  {0x2625, "PCnet-FAST III 79C973",/* 79C973 PCInet-FAST III. */
   LANCE_ENABLE_AUTOSELECT},
  {0x2626, "PCnet/HomePNA 79C978",        
   LANCE_ENABLE_AUTOSELECT|LANCE_SELECT_PHONELINE},
  {0x0, "PCnet (unknown)",
   LANCE_ENABLE_AUTOSELECT},
};

/* ############## for LANCE device ############## */
#define LANCE_ETH_ADDR          0x0
#define LANCE_DATA              0x10
#define LANCE_ADDR              0x12
#define LANCE_RESET             0x14
#define LANCE_BUS_IF            0x16
#define LANCE_TOTAL_SIZE        0x18

/* Use 2^4=16 {Rx,Tx} buffers */
#define LANCE_LOG_RX_BUFFERS    4
#define RX_RING_SIZE            (1 << (LANCE_LOG_RX_BUFFERS))
#define RX_RING_MOD_MASK        (RX_RING_SIZE - 1)
#define RX_RING_LEN_BITS        ((LANCE_LOG_RX_BUFFERS) << 29)

#define LANCE_LOG_TX_BUFFERS    4
#define TX_RING_SIZE            (1 << (LANCE_LOG_TX_BUFFERS))
#define TX_RING_MOD_MASK        (TX_RING_SIZE - 1)
#define TX_RING_LEN_BITS        ((LANCE_LOG_TX_BUFFERS) << 29)

/* for lance_interface */
struct lance_init_block
{
  unsigned short  mode;
  unsigned char   phys_addr[6];
  unsigned long   filter[2];
  Address         rx_ring;
  Address         tx_ring;
};

struct lance_rx_head
{
  union {
    Address         base;
    unsigned char   addr[4];
  } u;
  short           buf_length;     /* 2s complement */
  short           msg_length;
};

struct lance_tx_head
{
  union {
    Address         base;
    unsigned char   addr[4];
  } u;
  short           buf_length;     /* 2s complement */
  short           misc;
};

struct lance_interface
{
  struct lance_init_block init_block;
  struct lance_rx_head    rx_ring[RX_RING_SIZE];
  struct lance_tx_head    tx_ring[TX_RING_SIZE];
  unsigned char           rbuf[RX_RING_SIZE][ETH_FRAME_LEN];
  unsigned char           tbuf[TX_RING_SIZE][ETH_FRAME_LEN];
};

/* =============== global variables =============== */
static struct lance_interface  *lp;
static char lance[sizeof(struct lance_interface)+8];
static int rx_slot_nr = 0;          /* Rx-slot number */
static int tx_slot_nr = 0;          /* Tx-slot number */
static int cur_tx_slot_nr = 0;      /* Tx-slot number */
static char isstored[TX_RING_SIZE]; /* Tx-slot in-use */


/*===========================================================================*
 *                            lance_task                                     *
 *===========================================================================*/
void main( int argc, char **argv )
{
  message m;
  int i,irq,r;
  ether_card_t *ec;
  long v;
	int fkeys, sfkeys;

	env_setargs( argc, argv );
	
	fkeys = sfkeys = 0; bit_set( sfkeys, 7 );
 
	if ( (r = fkey_map(&fkeys, &sfkeys)) != OK )
		printf( "Error registering key\n" );
	
	if((eth_tasknr=getprocnr()) < 0)
		panic("lance","couldn't get own proc nr", i);

  v= 0;
  (void) env_parse("ETH_IGN_PROTO", "x", 0, &v, 0x0000L, 0xFFFFL);
  eth_ign_proto= htons((u16_t) v);

  while (TRUE)
    {
      for (i=0;i<EC_PORT_NR_MAX;++i)
	{
	  ec= &ec_table[i];
	  if (ec->ec_irq != 0)
	    sys_irqenable(&ec->ec_hook);
	}
	
      if ((r= receive(ANY, &m)) != OK)
        panic( "lance", "receive failed", r);
        
      for (i=0;i<EC_PORT_NR_MAX;++i)
	{
	  ec= &ec_table[i];
	  if (ec->ec_irq != 0)
	    sys_irqdisable(&ec->ec_hook);
	}

/*printf( "." );*/

      switch (m.m_type){
      case DL_WRITE:   do_vwrite(&m, FALSE, FALSE);    break;
      case DL_WRITEV:  do_vwrite(&m, FALSE, TRUE);     break;
      case DL_READ:    do_vread(&m, FALSE);            break;
      case DL_READV:   do_vread(&m, TRUE);             break;
      case DL_INIT:    do_init(&m);                    break;
      case DL_GETSTAT: do_getstat(&m);                 break;
      case DL_STOP:    do_stop(&m);                    break;
      case FKEY_PRESSED: lance_dump();                 break;
      /*case HARD_STOP:  lance_stop();                   break;*/
      case SYS_SIG:
      {
      	sigset_t set = m.NOTIFY_ARG;
      	if ( sigismember( &set, SIGKSTOP ) )
      		lance_stop();
      }
      	break;
      case HARD_INT:
        for (i=0;i<EC_PORT_NR_MAX;++i)
          {
            ec= &ec_table[i];
            if (ec->mode != EC_ENABLED)
              continue;
            
            /*
            printf( "#.\n" );
            */
            
            irq=ec->ec_irq;
            /*if (ec->ec_int_pending)*/
              {
                ec->ec_int_pending = 0;
                ec_check_ints(ec);
                do_int(ec);
              }
          }
        break;
      default:
        panic( "lance", "illegal message", m.m_type);
      }
    }
}

/*===========================================================================*
 *                            lance_dump                                     *
 *===========================================================================*/
static void lance_dump()
{
  ether_card_t *ec;
  int i, isr;
  unsigned short ioaddr;
  
  printf("\n");
  for (i= 0, ec = &ec_table[0]; i<EC_PORT_NR_MAX; i++, ec++)
    {
      if (ec->mode == EC_DISABLED)
	printf("lance port %d is disabled\n", i);
      else if (ec->mode == EC_SINK)
	printf("lance port %d is in sink mode\n", i);
      
      if (ec->mode != EC_ENABLED)
	continue;
      
      printf("lance statistics of port %d:\n", i);
      
      printf("recvErr    :%8ld\t", ec->eth_stat.ets_recvErr);
      printf("sendErr    :%8ld\t", ec->eth_stat.ets_sendErr);
      printf("OVW        :%8ld\n", ec->eth_stat.ets_OVW);
      
      printf("CRCerr     :%8ld\t", ec->eth_stat.ets_CRCerr);
      printf("frameAll   :%8ld\t", ec->eth_stat.ets_frameAll);
      printf("missedP    :%8ld\n", ec->eth_stat.ets_missedP);
      
      printf("packetR    :%8ld\t", ec->eth_stat.ets_packetR);
      printf("packetT    :%8ld\t", ec->eth_stat.ets_packetT);
      printf("transDef   :%8ld\n", ec->eth_stat.ets_transDef);
      
      printf("collision  :%8ld\t", ec->eth_stat.ets_collision);
      printf("transAb    :%8ld\t", ec->eth_stat.ets_transAb);
      printf("carrSense  :%8ld\n", ec->eth_stat.ets_carrSense);
      
      printf("fifoUnder  :%8ld\t", ec->eth_stat.ets_fifoUnder);
      printf("fifoOver   :%8ld\t", ec->eth_stat.ets_fifoOver);
      printf("CDheartbeat:%8ld\n", ec->eth_stat.ets_CDheartbeat);
      
      printf("OWC        :%8ld\t", ec->eth_stat.ets_OWC);
      
      ioaddr = ec->ec_port;
      out_word(ioaddr+LANCE_ADDR, 0x00);
      isr=in_word(ioaddr+LANCE_DATA);
      printf("isr = 0x%x + 0x%x, flags = 0x%x\n", isr,
	     in_word(ioaddr+LANCE_DATA), ec->flags);
      
      printf("irq = %d\tioadr = %d\n", ec->ec_irq, ec->ec_port);
    }
}

/*===========================================================================*
 *                               lance_stop                                  *
 *===========================================================================*/
static void lance_stop()
{
  message mess;
  int i;

  for (i= 0; i<EC_PORT_NR_MAX; i++)
    {
      if (ec_table[i].mode != EC_ENABLED)
	continue;
      mess.m_type= DL_STOP;
      mess.DL_PORT= i;
      do_stop(&mess);
    }
    
  	/*printf("LANCE driver stopped.\n");*/
  	
  	sys_exit( 0 );
}


/*===========================================================================*
 *                              do_init                                      *
 *===========================================================================*/
static void do_init(mp)
message *mp;
{
  int port;
  ether_card_t *ec;
  message reply_mess;

pci_init();

  port = mp->DL_PORT;
  if (port < 0 || port >= EC_PORT_NR_MAX)
    {
      reply_mess.m_type= DL_INIT_REPLY;
      reply_mess.m3_i1= ENXIO;
      mess_reply(mp, &reply_mess);
      return;
    }
  ec= &ec_table[port];
  strcpy(ec->port_name, "eth_card#0");
  ec->port_name[9] += port;
  if (ec->mode == EC_DISABLED)
    {
      /* This is the default, try to (re)locate the device. */
      /* only try to enable if memory is correct for DMA */
	  if ( CORRECT_DMA_MEM() )
	  {
		conf_hw(ec);
	  }
	  else
	  {
	  	report( "LANCE", "DMA denied because address out of range", NO_NUM );
	  }
	  
      if (ec->mode == EC_DISABLED)
	{
	  /* Probe failed, or the device is configured off. */
	  reply_mess.m_type= DL_INIT_REPLY;
	  reply_mess.m3_i1= ENXIO;
	  mess_reply(mp, &reply_mess);
	  return;
	}
      if (ec->mode == EC_ENABLED)
	ec_init(ec);
    }

  if (ec->mode == EC_SINK)
    {
      ec->mac_address.ea_addr[0] = 
	ec->mac_address.ea_addr[1] = 
	ec->mac_address.ea_addr[2] = 
	ec->mac_address.ea_addr[3] = 
	ec->mac_address.ea_addr[4] = 
	ec->mac_address.ea_addr[5] = 0;
      ec_confaddr(ec);
      reply_mess.m_type = DL_INIT_REPLY;
      reply_mess.m3_i1 = mp->DL_PORT;
      reply_mess.m3_i2 = EC_PORT_NR_MAX;
      *(ether_addr_t *) reply_mess.m3_ca1 = ec->mac_address;
      mess_reply(mp, &reply_mess);
      return;
    }
  assert(ec->mode == EC_ENABLED);
  assert(ec->flags & ECF_ENABLED);

  ec->flags &= ~(ECF_PROMISC | ECF_MULTI | ECF_BROAD);

  if (mp->DL_MODE & DL_PROMISC_REQ)
    ec->flags |= ECF_PROMISC | ECF_MULTI | ECF_BROAD;
  if (mp->DL_MODE & DL_MULTI_REQ)
    ec->flags |= ECF_MULTI;
  if (mp->DL_MODE & DL_BROAD_REQ)
    ec->flags |= ECF_BROAD;

  ec->client = mp->m_source;
  ec_reinit(ec);

  reply_mess.m_type = DL_INIT_REPLY;
  reply_mess.m3_i1 = mp->DL_PORT;
  reply_mess.m3_i2 = EC_PORT_NR_MAX;
  *(ether_addr_t *) reply_mess.m3_ca1 = ec->mac_address;

  mess_reply(mp, &reply_mess);
}


/*===========================================================================*
 *                              do_int                                       *
 *===========================================================================*/
static void do_int(ec)
ether_card_t *ec;
{
  if (ec->flags & (ECF_PACK_SEND | ECF_PACK_RECV))
    reply(ec, OK, TRUE);
}

#if 0
/*===========================================================================*
 *                              ec_handler                                   *
 *===========================================================================*/
static int ec_handler(hook)
irq_hook_t *hook;
{
  /* LANCE interrupt, send message and reenable interrupts. */
#if 0
  printf(">> ec_handler(): \n");
#endif

  structof(ether_card_t, ec_hook, hook)->ec_int_pending= 1;

  notify(eth_tasknr);

  return 0;
}
#endif

/*===========================================================================*
 *                              conf_hw                                      *
 *===========================================================================*/
static void conf_hw(ec)
ether_card_t *ec;
{
  static eth_stat_t empty_stat = {0, 0, 0, 0, 0, 0        /* ,... */ };

  int ifnr;
  ec_conf_t *ecp;

  ec->mode= EC_DISABLED;     /* Superfluous */
  ifnr= ec-ec_table;

  ecp= &ec_conf[ifnr];
  update_conf(ec, ecp);
  if (ec->mode != EC_ENABLED)
    return;

  if (!lance_probe(ec))
    {
      printf("%s: No ethernet card found on PCI-BIOS info.\n", 
	     ec->port_name);
      ec->mode= EC_DISABLED;
      return;
    }

  /* Allocate a memory segment, programmed I/O should set the
   * memory segment (linmem) to zero.
   */
  if (ec->ec_linmem != 0)
    {
    	assert( 0 );
      	/*phys2seg(&ec->ec_memseg, &ec->ec_memoff, ec->ec_linmem);*/
    }

/* XXX */ if (ec->ec_linmem == 0) ec->ec_linmem= 0xFFFF0000;

  ec->flags = ECF_EMPTY;
  ec->eth_stat = empty_stat;
}


/*===========================================================================*
 *                              update_conf                                  *
 *===========================================================================*/
static void update_conf(ec, ecp)
ether_card_t *ec;
ec_conf_t *ecp;
{
  long v;
  static char ec_fmt[] = "x:d:x:x";

  /* Get the default settings and modify them from the environment. */
  ec->mode= EC_SINK;
  v= ecp->ec_port;
  switch (env_parse(ecp->ec_envvar, ec_fmt, 0, &v, 0x0000L, 0xFFFFL)) {
  case EP_OFF:
    ec->mode= EC_DISABLED;
    break;
  case EP_ON:
  case EP_SET:
    ec->mode= EC_ENABLED;      /* Might become disabled if 
				* all probes fail */
    break;
  }
  
  ec->ec_port= v;

  v= ecp->ec_irq | DEI_DEFAULT;
  (void) env_parse(ecp->ec_envvar, ec_fmt, 1, &v, 0L,
		   (long) NR_IRQ_VECTORS - 1);
  ec->ec_irq= v;
  
  v= ecp->ec_mem;
  (void) env_parse(ecp->ec_envvar, ec_fmt, 2, &v, 0L, 0xFFFFFL);
  ec->ec_linmem= v;
  
  v= 0;
  (void) env_parse(ecp->ec_envvar, ec_fmt, 3, &v, 0x2000L, 0x8000L);
  ec->ec_ramsize= v;
}


/*===========================================================================*
 *                              ec_init                                      *
 *===========================================================================*/
static void ec_init(ec)
ether_card_t *ec;
{
  int i, r;

  /* General initialization */
  ec->flags = ECF_EMPTY;
  /*disable_irq(ec->ec_irq);*/
  lance_init_card(ec); /* Get mac_address, etc ...*/

  ec_confaddr(ec);

#if VERBOSE
  printf("%s: Ethernet address ", ec->port_name);
  for (i= 0; i < 6; i++)
    printf("%x%c", ec->mac_address.ea_addr[i],
	   i < 5 ? ':' : '\n');
#endif

  /* Finish the initialization */
  ec->flags |= ECF_ENABLED;

  /* Set the interrupt handler */
  /*put_irq_handler(&ec->ec_hook, ec->ec_irq, ec_handler);*/
  ec->ec_hook = ec->ec_irq;
  	if ((r=sys_irqsetpolicy(ec->ec_irq, 0, &ec->ec_hook)) != OK)
		printf("lance: error, couldn't set IRQ policy: %d\n", r);

/*  enable_irq(ec->ec_irq); */

/*  enter_kdebug(">> ec_init():"); */

  return;
}


/*===========================================================================*
 *                              reply                                        *
 *===========================================================================*/
static void reply(ec, err, may_block)
ether_card_t *ec;
int err;
int may_block;
{
  message reply;
  int status,r;
  clock_t now;

  status = 0;
  if (ec->flags & ECF_PACK_SEND)
    status |= DL_PACK_SEND;
  if (ec->flags & ECF_PACK_RECV)
    status |= DL_PACK_RECV;

  reply.m_type   = DL_TASK_REPLY;
  reply.DL_PORT  = ec - ec_table;
  reply.DL_PROC  = ec->client;
  reply.DL_STAT  = status | ((u32_t) err << 16);
  reply.DL_COUNT = ec->read_s;
#if 1
  if ((r=getuptime(&now)) != OK)
	panic("lance", "getuptime() failed:", r);
  reply.DL_CLCK = now; 
#else
  reply.DL_CLCK = 0;
#endif

  r = send(ec->client, &reply);
#if 1
  if (r == ELOCKED && may_block)
    {
/*     enter_kdebug(">> lance_task: ELOCKED!"); */
      return;
    }
#endif
  if (r < 0)
    panic( "lance", "send failed:", r);

  ec->read_s = 0;
  ec->flags &= ~(ECF_PACK_SEND | ECF_PACK_RECV);
}


/*===========================================================================*
 *                              mess_reply                                   *
 *===========================================================================*/
static void mess_reply(req, reply_mess)
message *req;
message *reply_mess;
{
  if (send(req->m_source, reply_mess) != OK)
    panic( "lance", "unable to mess_reply", NO_NUM);
}


/*===========================================================================*
 *                              ec_confaddr                                  *
 *===========================================================================*/
static void ec_confaddr(ec)
ether_card_t *ec;
{
  int i;
  char eakey[16];
  static char eafmt[]= "x:x:x:x:x:x";
  long v;

  /* User defined ethernet address? */
  strcpy(eakey, ec_conf[ec-ec_table].ec_envvar);
  strcat(eakey, "_EA");

  for (i= 0; i < 6; i++)
    {
      v= ec->mac_address.ea_addr[i];
      if (env_parse(eakey, eafmt, i, &v, 0x00L, 0xFFL) != EP_SET)
	break;
      ec->mac_address.ea_addr[i]= v;
    }
  
  if (i != 0 && i != 6)
    {
      /* It's all or nothing; force a panic. */
      (void) env_parse(eakey, "?", 0, &v, 0L, 0L);
    }
}


/*===========================================================================*
 *                              ec_reinit                                    *
 *===========================================================================*/
static void ec_reinit(ec)
ether_card_t *ec;
{
  int i;
  unsigned short ioaddr = ec->ec_port;

  out_word(ioaddr+LANCE_ADDR, 0x0);
  (void)in_word(ioaddr+LANCE_ADDR);
  out_word(ioaddr+LANCE_DATA, 0x4);           /* stop */

  /* purge Tx-ring */
  tx_slot_nr = cur_tx_slot_nr = 0;
  for (i=0; i<TX_RING_SIZE; i++) {
    lp->tx_ring[i].u.base = 0;
    isstored[i]=0;
  }

  /* re-init Rx-ring */
  rx_slot_nr = 0;
  for (i=0; i<RX_RING_SIZE; i++) 
    {
      lp->rx_ring[i].buf_length = -ETH_FRAME_LEN;
      lp->rx_ring[i].u.addr[3] |= 0x80;
    }

  /* Set 'Receive Mode' */
  if (ec->flags & ECF_PROMISC)
    {
      out_word(ioaddr+LANCE_ADDR, 0xf);
      out_word(ioaddr+LANCE_DATA, 0x8000);
    }
  else
    {
      if (ec->flags & (ECF_BROAD | ECF_MULTI))
        {
          out_word(ioaddr+LANCE_ADDR, 0xf);
          out_word(ioaddr+LANCE_DATA, 0x0000);
        }
      else
        {
          out_word(ioaddr+LANCE_ADDR, 0xf);
          out_word(ioaddr+LANCE_DATA, 0x4000);
        }
    }

  out_word(ioaddr+LANCE_ADDR, 0x0);
  (void)in_word(ioaddr+LANCE_ADDR);
  out_word(ioaddr+LANCE_DATA, 0x142);   /* start && enable interrupt */

  return;
}

/*===========================================================================*
 *                              ec_check_ints                                *
 *===========================================================================*/
static void ec_check_ints(ec)
ether_card_t *ec;
{
  int must_restart=0;
  int check,status;
  int isr=0x0000;
  unsigned short ioaddr = ec->ec_port;

  if (!(ec->flags & ECF_ENABLED))
    panic( "lance", "got premature interrupt", NO_NUM);

    for (;;)
      {
#if 0
	printf("ETH: Reading ISR...");
#endif
	out_word(ioaddr+LANCE_ADDR, 0x00);
	isr=in_word(ioaddr+LANCE_DATA);
	if (isr & 0x8600)
	  out_word( ioaddr+LANCE_DATA, isr & ~0x004f);
	out_word(ioaddr+LANCE_DATA, 0x7940);
#if 0
	printf("ISR=0x%x...",in_word(ioaddr+LANCE_DATA));
#endif
#define ISR_WINT 0x0200
#define ISR_RINT 0x0400
#define ISR_RERR 0x1000
#define ISR_WERR 0x4000
#define ISR_ERR  0x8000
#define ISR_RST  0x0000

        if ((isr & (ISR_WINT|ISR_RINT|ISR_RERR|ISR_WERR|ISR_ERR)) == 0x0000)
          {
#if 0
	    printf("OK\n");
#endif
            break;
          }

        if (isr & ISR_RERR)
          {
#if 0
	    printf("RERR\n");
#endif
            ec->eth_stat.ets_recvErr++;
          }
        if ((isr & ISR_WERR) || (isr & ISR_WINT))
          {
            if (isr & ISR_WERR)
              {
#if 0
		printf("WERR\n");
#endif
                ec->eth_stat.ets_sendErr++;
              }
            if (isr & ISR_WINT)
              {
#if 0
		printf("WINT\n");
#endif
		/* status check: restart if needed. */
		status = lp->tx_ring[cur_tx_slot_nr].u.base;

		/* ??? */
		if (status & 0x40000000)
		  {
		    status = lp->tx_ring[cur_tx_slot_nr].misc;
		    ec->eth_stat.ets_sendErr++;
		    if (status & 0x0400)
		      ec->eth_stat.ets_transAb++;
		    if (status & 0x0800)
		      ec->eth_stat.ets_carrSense++;
		    if (status & 0x1000)
		      ec->eth_stat.ets_OWC++;
		    if (status & 0x4000)
		      {
			ec->eth_stat.ets_fifoUnder++;
			must_restart=1;
		      }
		  }
		else
		  {
		    if (status & 0x18000000)
		      ec->eth_stat.ets_collision++;
		    ec->eth_stat.ets_packetT++;
		  }
              }
            /* transmit a packet on the next slot if it exists. */
	    check = 0;
	    if (isstored[cur_tx_slot_nr]==1)
	      {
		/* free the tx-slot just transmitted */
		isstored[cur_tx_slot_nr]=0;
		cur_tx_slot_nr = (++cur_tx_slot_nr) & TX_RING_MOD_MASK;
		
		/* next tx-slot is ready? */
		if (isstored[cur_tx_slot_nr]==1)
		  check=1;
		else
		  check=0;
	      }
	    else
	      {
		panic( "lance", "got premature WINT...", NO_NUM);
	      }
	    if (check==1)
	      {
		lp->tx_ring[cur_tx_slot_nr].u.addr[3] = 0x83;
		out_word(ioaddr+LANCE_ADDR, 0x0000);
		out_word(ioaddr+LANCE_DATA, 0x0048);
	      }
	    else
	      if (check==-1)
		continue;
	    /* we set a buffered message in the slot if it exists. */
	    /* and transmit it, if needed. */
	    if (ec->flags & ECF_SEND_AVAIL)
	      ec_send(ec);
          }
        if (isr & ISR_RINT)
          {
#if 0
	    printf("RINT\n");
#endif
            ec_recv(ec);
          }

        if (isr & ISR_RST)
          {
            ec->flags = ECF_STOPPED;
            break;
          }

        /* ??? cf. lance driver on linux */
        if (must_restart == 1)
          {
#if 0
	    printf("ETH: restarting...\n");
#endif
	    out_word(ioaddr+LANCE_ADDR, 0x0);
	    (void)in_word(ioaddr+LANCE_ADDR);
	    out_word(ioaddr+LANCE_DATA, 0x4);  /* stop */
	    out_word(ioaddr+LANCE_DATA, 0x2);  /* start */
          }
      }
    
    if ((ec->flags & (ECF_READING|ECF_STOPPED)) == (ECF_READING|ECF_STOPPED))
      {
#if 0
	printf("ETH: resetting...\n");
#endif
        ec_reset(ec);
      }
}

/*===========================================================================*
 *                              ec_reset                                     *
 *===========================================================================*/
static void ec_reset(ec)
ether_card_t *ec;
{
  /* Stop/start the chip, and clear all RX,TX-slots */
  unsigned short ioaddr = ec->ec_port;
  int i;
  
  out_word(ioaddr+LANCE_ADDR, 0x0);
  (void)in_word(ioaddr+LANCE_ADDR);
  out_word(ioaddr+LANCE_DATA, 0x4);                      /* stop */
  out_word(ioaddr+LANCE_DATA, 0x2);                      /* start */
  
  /* purge Tx-ring */
  tx_slot_nr = cur_tx_slot_nr = 0;
  for (i=0; i<TX_RING_SIZE; i++) {
    lp->tx_ring[i].u.base = 0;
    isstored[i]=0;
  }

  /* re-init Rx-ring */
  rx_slot_nr = 0;
  for (i=0; i<RX_RING_SIZE; i++) 
    {
      lp->rx_ring[i].buf_length = -ETH_FRAME_LEN;
      lp->rx_ring[i].u.addr[3] |= 0x80;
    }

  /* store a buffered message on the slot if exists */
  ec_send(ec);
  ec->flags &= ~ECF_STOPPED;
}

/*===========================================================================*
 *                              ec_send                                      *
 *===========================================================================*/
static void ec_send(ec)
ether_card_t *ec;
{
  /* from ec_check_ints() or ec_reset(). */
  /* this function proccesses the buffered message. (slot/transmit) */
  if (!(ec->flags & ECF_SEND_AVAIL))
    return;
  
  ec->flags &= ~ECF_SEND_AVAIL;
  switch(ec->sendmsg.m_type)
    {
    case DL_WRITE:  do_vwrite(&ec->sendmsg, TRUE, FALSE);       break;
    case DL_WRITEV: do_vwrite(&ec->sendmsg, TRUE, TRUE);        break;
    default:
      panic( "lance", "wrong type:", ec->sendmsg.m_type);
      break;
    }
}

/*===========================================================================*
 *                              do_vread                                     *
 *===========================================================================*/
static void do_vread(mp, vectored)
message *mp;
int vectored;
{
  int port, count, size;
  ether_card_t *ec;

  port = mp->DL_PORT;
  count = mp->DL_COUNT;
  ec= &ec_table[port];
  ec->client= mp->DL_PROC;

  if (vectored)
    {
      get_userdata(mp->DL_PROC, (vir_bytes) mp->DL_ADDR,
                   (count > IOVEC_NR ? IOVEC_NR : count) *
                   sizeof(iovec_t), ec->read_iovec.iod_iovec);
      ec->read_iovec.iod_iovec_s    = count;
      ec->read_iovec.iod_proc_nr    = mp->DL_PROC;
      ec->read_iovec.iod_iovec_addr = (vir_bytes) mp->DL_ADDR;
      
      ec->tmp_iovec = ec->read_iovec;
      size= calc_iovec_size(&ec->tmp_iovec);
    }
  else
    {
      ec->read_iovec.iod_iovec[0].iov_addr = (vir_bytes) mp->DL_ADDR;
      ec->read_iovec.iod_iovec[0].iov_size = mp->DL_COUNT;
      ec->read_iovec.iod_iovec_s           = 1;
      ec->read_iovec.iod_proc_nr           = mp->DL_PROC;
      ec->read_iovec.iod_iovec_addr        = 0;

      size= count;
    }
  ec->flags |= ECF_READING;

  ec_recv(ec);

  if ((ec->flags & (ECF_READING|ECF_STOPPED)) == (ECF_READING|ECF_STOPPED))
    ec_reset(ec);
  reply(ec, OK, FALSE);
}

/*===========================================================================*
 *                              ec_recv                                      *
 *===========================================================================*/
static void ec_recv(ec)
ether_card_t *ec;
{
  vir_bytes length;
  int packet_processed;
  int status;
  unsigned short ioaddr = ec->ec_port;

  if ((ec->flags & ECF_READING)==0)
    return;
  if (!(ec->flags & ECF_ENABLED))
    return;

  /* we check all the received slots until find a properly received packet */
  packet_processed = FALSE;
  while (!packet_processed)
    {
      status = lp->rx_ring[rx_slot_nr].u.base >> 24;
      if ( (status & 0x80) == 0x00 )
        {
	  status = lp->rx_ring[rx_slot_nr].u.base >> 24;

	  /* ??? */
	  if (status != 0x03)
	    {
	      if (status & 0x01)
		ec->eth_stat.ets_recvErr++;
	      if (status & 0x04)
		ec->eth_stat.ets_fifoOver++;
	      if (status & 0x08)
		ec->eth_stat.ets_CRCerr++;
	      if (status & 0x10)
		ec->eth_stat.ets_OVW++;
	      if (status & 0x20)
		ec->eth_stat.ets_frameAll++;
	      length = 0;
	    }
	  else
	    {
	      ec->eth_stat.ets_packetR++;
	      length = lp->rx_ring[rx_slot_nr].msg_length;
	    }
          if (length > 0)
            {
	      ec_nic2user(ec, (int)(lp->rbuf[rx_slot_nr]),
			  &ec->read_iovec, 0, length);
              
              ec->read_s = length;
              ec->flags |= ECF_PACK_RECV;
              ec->flags &= ~ECF_READING;
              packet_processed = TRUE;
            }
          /* set up this slot again, and we move to the next slot */
	  lp->rx_ring[rx_slot_nr].buf_length = -ETH_FRAME_LEN;
	  lp->rx_ring[rx_slot_nr].u.addr[3] |= 0x80;

	  out_word(ioaddr+LANCE_ADDR, 0x00);
	  out_word(ioaddr+LANCE_DATA, 0x7940);

	  rx_slot_nr = (++rx_slot_nr) & RX_RING_MOD_MASK;
        }
      else
        break;
    }
}

/*===========================================================================*
 *                              do_vwrite                                    *
 *===========================================================================*/
static void do_vwrite(mp, from_int, vectored)
message *mp;
int from_int;
int vectored;
{
  int port, count, check;
  ether_card_t *ec;
  unsigned short ioaddr;

  port = mp->DL_PORT;
  count = mp->DL_COUNT;
  ec = &ec_table[port];
  ec->client= mp->DL_PROC;

  if (isstored[tx_slot_nr]==1)
    {
      /* all slots are used, so this message is buffered */
      ec->sendmsg= *mp;
      ec->flags |= ECF_SEND_AVAIL;
      reply(ec, OK, FALSE);
      return;
    }

  /* convert the message to write_iovec */
  if (vectored)
    {
      get_userdata(mp->DL_PROC, (vir_bytes) mp->DL_ADDR,
                   (count > IOVEC_NR ? IOVEC_NR : count) *
                   sizeof(iovec_t), ec->write_iovec.iod_iovec);

      ec->write_iovec.iod_iovec_s    = count;
      ec->write_iovec.iod_proc_nr    = mp->DL_PROC;
      ec->write_iovec.iod_iovec_addr = (vir_bytes) mp->DL_ADDR;

      ec->tmp_iovec = ec->write_iovec;
      ec->write_s = calc_iovec_size(&ec->tmp_iovec);
    }
  else
    {  
      ec->write_iovec.iod_iovec[0].iov_addr = (vir_bytes) mp->DL_ADDR;
      ec->write_iovec.iod_iovec[0].iov_size = mp->DL_COUNT;

      ec->write_iovec.iod_iovec_s    = 1;
      ec->write_iovec.iod_proc_nr    = mp->DL_PROC;
      ec->write_iovec.iod_iovec_addr = 0;

      ec->write_s = mp->DL_COUNT;
    }

  /* copy write_iovec to the slot on DMA address */
  ec_user2nic(ec, &ec->write_iovec, 0,
              (int)(lp->tbuf[tx_slot_nr]), ec->write_s);
  /* set-up for transmitting, and transmit it if needed. */
  lp->tx_ring[tx_slot_nr].buf_length = -ec->write_s;
  lp->tx_ring[tx_slot_nr].misc = 0x0;
  lp->tx_ring[tx_slot_nr].u.base 
    = virt_to_bus(lp->tbuf[tx_slot_nr]) & 0xffffff;
  isstored[tx_slot_nr]=1;
  if (cur_tx_slot_nr == tx_slot_nr)
    check=1;
  else
    check=0;
  tx_slot_nr = (++tx_slot_nr) & TX_RING_MOD_MASK;

  if (check == 1)
    {
      ioaddr = ec->ec_port;
      lp->tx_ring[cur_tx_slot_nr].u.addr[3] = 0x83;
      out_word(ioaddr+LANCE_ADDR, 0x0000);
      out_word(ioaddr+LANCE_DATA, 0x0048);
    }
        
  ec->flags |= ECF_PACK_SEND;

  /* reply by calling do_int() if this function is called from interrupt. */
  if (from_int)
    return;
  reply(ec, OK, FALSE);
}


/*===========================================================================*
 *                              get_userdata                                 *
 *===========================================================================*/
static void get_userdata(user_proc, user_addr, count, loc_addr)
int user_proc;
vir_bytes user_addr;
vir_bytes count;
void *loc_addr;
{
	/*
  phys_bytes src;

  src = numap_local(user_proc, user_addr, count);
  if (!src)
    panic( "lance", "umap failed", NO_NUM);

  phys_copy(src, vir2phys(loc_addr), (phys_bytes) count);
  */
	int cps;
	cps = sys_datacopy(user_proc, user_addr, SELF, (vir_bytes) loc_addr, count);
	if (cps != OK) printf("lance: warning, scopy failed: %d\n", cps);
}

/*===========================================================================*
 *                              ec_user2nic                                  *
 *===========================================================================*/
static void ec_user2nic(ec, iovp, offset, nic_addr, count)
ether_card_t *ec;
iovec_dat_t *iovp;
vir_bytes offset;
int nic_addr;
vir_bytes count;
{
  /*phys_bytes phys_hw, phys_user;*/
  int bytes, i, r;

  /*
  phys_hw = vir2phys(nic_addr);
  */
  i= 0;
  while (count > 0)
    {
      if (i >= IOVEC_NR)
        {
          ec_next_iovec(iovp);
          i= 0;
          continue;
        }
      if (offset >= iovp->iod_iovec[i].iov_size)
        {
          offset -= iovp->iod_iovec[i].iov_size;
          i++;
          continue;
        }
      bytes = iovp->iod_iovec[i].iov_size - offset;
      if (bytes > count)
        bytes = count;
      
      /*
      phys_user = numap_local(iovp->iod_proc_nr,
                        iovp->iod_iovec[i].iov_addr + offset, bytes);
      
      phys_copy(phys_user, phys_hw, (phys_bytes) bytes);
      */
      if ( (r=sys_datacopy(iovp->iod_proc_nr, iovp->iod_iovec[i].iov_addr + offset,
      	SELF, nic_addr, count )) != OK )
      	panic( "lance", "sys_datacopy failed", r );
      	
      count -= bytes;
      nic_addr += bytes;
      offset += bytes;
    }
}

/*===========================================================================*
 *                              ec_nic2user                                  *
 *===========================================================================*/
static void ec_nic2user(ec, nic_addr, iovp, offset, count)
ether_card_t *ec;
int nic_addr;
iovec_dat_t *iovp;
vir_bytes offset;
vir_bytes count;
{
  /*phys_bytes phys_hw, phys_user;*/
  int bytes, i, r;

  /*phys_hw = vir2phys(nic_addr);*/

  i= 0;
  while (count > 0)
    {
      if (i >= IOVEC_NR)
        {
          ec_next_iovec(iovp);
          i= 0;
          continue;
        }
      if (offset >= iovp->iod_iovec[i].iov_size)
        {
          offset -= iovp->iod_iovec[i].iov_size;
          i++;
          continue;
        }
      bytes = iovp->iod_iovec[i].iov_size - offset;
      if (bytes > count)
        bytes = count;
      /*
      phys_user = numap_local(iovp->iod_proc_nr,
                        iovp->iod_iovec[i].iov_addr + offset, bytes);

      phys_copy(phys_hw, phys_user, (phys_bytes) bytes);
      */
      if ( (r=sys_datacopy( SELF, nic_addr, iovp->iod_proc_nr, iovp->iod_iovec[i].iov_addr + offset, bytes )) != OK )
      	panic( "lance", "sys_datacopy failed: ", r );
      
      count -= bytes;
      nic_addr += bytes;
      offset += bytes;
    }
}


/*===========================================================================*
 *                              calc_iovec_size                              *
 *===========================================================================*/
static int calc_iovec_size(iovp)
iovec_dat_t *iovp;
{
  int size,i;

  size = i = 0;
        
  while (i < iovp->iod_iovec_s)
    {
      if (i >= IOVEC_NR)
        {
          ec_next_iovec(iovp);
          i= 0;
          continue;
        }
      size += iovp->iod_iovec[i].iov_size;
      i++;
    }

  return size;
}

/*===========================================================================*
 *                           ec_next_iovec                                   *
 *===========================================================================*/
static void ec_next_iovec(iovp)
iovec_dat_t *iovp;
{
  iovp->iod_iovec_s -= IOVEC_NR;
  iovp->iod_iovec_addr += IOVEC_NR * sizeof(iovec_t);

  get_userdata(iovp->iod_proc_nr, iovp->iod_iovec_addr, 
               (iovp->iod_iovec_s > IOVEC_NR ? 
                IOVEC_NR : iovp->iod_iovec_s) * sizeof(iovec_t), 
               iovp->iod_iovec); 
}


/*===========================================================================*
 *                              do_getstat                                   *
 *===========================================================================*/
static void do_getstat(mp)
message *mp;
{
  int port;
  ether_card_t *ec;

  port = mp->DL_PORT;
  if (port < 0 || port >= EC_PORT_NR_MAX)
    panic( "lance", "illegal port", port);

  ec= &ec_table[port];
  ec->client= mp->DL_PROC;

  put_userdata(mp->DL_PROC, (vir_bytes) mp->DL_ADDR,
               (vir_bytes) sizeof(ec->eth_stat), &ec->eth_stat);
  reply(ec, OK, FALSE);
}

/*===========================================================================*
 *                              put_userdata                                 *
 *===========================================================================*/
static void put_userdata(user_proc, user_addr, count, loc_addr)
int user_proc;
vir_bytes user_addr;
vir_bytes count;
void *loc_addr;
{
  /*phys_bytes dst;

  dst = numap_local(user_proc, user_addr, count);
  if (!dst)
    panic( "lance", "umap failed", NO_NUM);

  phys_copy(vir2phys(loc_addr), dst, (phys_bytes) count);
  */
	int cps;
	cps = sys_datacopy(SELF, (vir_bytes) loc_addr, user_proc, user_addr, count);
	if (cps != OK) printf("lance: warning, scopy failed: %d\n", cps);
}

/*===========================================================================*
 *                              do_stop                                      *
 *===========================================================================*/
static void do_stop(mp)
message *mp;
{
  int port;
  ether_card_t *ec;
  unsigned short ioaddr;

  port = mp->DL_PORT;
  if (port < 0 || port >= EC_PORT_NR_MAX)
    panic( "lance", "illegal port", port);
  ec = &ec_table[port];

  if (!(ec->flags & ECF_ENABLED))
    return;

  ioaddr = ec->ec_port;
  
  out_word(ioaddr+LANCE_ADDR, 0x0);
  (void)in_word(ioaddr+LANCE_ADDR);
  out_word(ioaddr+LANCE_DATA, 0x4);                          /* stop */
  out_word(ioaddr+LANCE_RESET, in_word(ioaddr+LANCE_RESET)); /* reset */

  ec->flags = ECF_EMPTY;
}

static void getAddressing(devind, ec)
int devind;
ether_card_t *ec;
{
	unsigned int      membase, ioaddr;
	int reg, irq;

	for (reg = PCI_BASE_ADDRESS_0; reg <= PCI_BASE_ADDRESS_5; reg += 4)
	{
          ioaddr = pci_attr_r32(devind, reg);

          if ((ioaddr & PCI_BASE_ADDRESS_IO_MASK) == 0 
              || (ioaddr & PCI_BASE_ADDRESS_SPACE_IO) == 0)
            continue;
          /* Strip the I/O address out of the returned value */
          ioaddr &= PCI_BASE_ADDRESS_IO_MASK;
          /* Get the memory base address */
          membase = pci_attr_r32(devind, PCI_BASE_ADDRESS_1);
          /* KK: Get the IRQ number */
          irq = pci_attr_r8(devind, PCI_INTERRUPT_PIN);
          if (irq)
            irq = pci_attr_r8(devind, PCI_INTERRUPT_LINE);
          /* Get the ROM base address */
          /*pci_attr_r32(devind, PCI_ROM_ADDRESS, &romaddr);
          romaddr >>= 10;*/
          /* Take the first one or the one that matches in boot ROM address */
          /*if (pci_ioaddr == 0 
              || romaddr == ((unsigned long) rom.rom_segment << 4)) {*/
            ec->ec_linmem = membase;
            ec->ec_port = ioaddr;
            ec->ec_irq = irq;
          /*}*/
	}
}

/*===========================================================================*
 *                            lance_probe                                    *
 *===========================================================================*/
static int lance_probe(ec)
ether_card_t *ec;
{
  unsigned short    pci_cmd, attached = 0;
  unsigned short    ioaddr;
  int               lance_version, chip_version;
	int devind, just_one, i, r;
	
	u16_t vid, did;
	u32_t bar;
	u8_t ilr;
	char *dname;

	if ((ec->ec_pcibus | ec->ec_pcidev | ec->ec_pcifunc) != 0)
	{
		/* Look for specific PCI device */
		r= pci_find_dev(ec->ec_pcibus, ec->ec_pcidev,
			ec->ec_pcifunc, &devind);
		if (r == 0)
		{
			printf("%s: no PCI found at %d.%d.%d\n",
				ec->port_name, ec->ec_pcibus,
				ec->ec_pcidev, ec->ec_pcifunc);
			return 0;
		}
		pci_ids(devind, &vid, &did);
		just_one= TRUE;
	}
	else
	{
		r= pci_first_dev(&devind, &vid, &did);
		if (r == 0)
			return 0;
		just_one= FALSE;
	}

	for(;;)
	{
		for (i= 0; pcitab[i].vid != 0; i++)
		{
			if (pcitab[i].vid != vid)
				continue;
			if (pcitab[i].did != did)
				continue;
			if (pcitab[i].checkclass)
			{
			  panic("lance",
			    "class check not implemented", NO_NUM);
			}
			break;
		}
		if (pcitab[i].vid != 0)
			break;

		if (just_one)
		{
			printf(
		"%s: wrong PCI device (%04x/%04x) found at %d.%d.%d\n",
				ec->port_name, vid, did,
				ec->ec_pcibus,
				ec->ec_pcidev, ec->ec_pcifunc);
			return 0;
		}

		r= pci_next_dev(&devind, &vid, &did);
		if (!r)
			return 0;
	}

	dname= pci_dev_name(vid, did);
	if (!dname)
		dname= "unknown device";
	
	/*
	printf("%s: ", ec->port_name);
	printf("%s ", dname);
	printf("(%x/", vid);
	printf("%x) ", did);
	printf("at %s\n", pci_slot_name(devind));
	*/
	pci_reserve(devind);	
	
/*  for (i = 0; pci_dev_list[i].vendor != 0; i++) {
    if (pci_dev_list[i].suffix == 1)
      {
	ec->ec_port   = pci_dev_list[i].ioaddr;
	ec->ec_irq    = pci_dev_list[i].irq;
	ec->ec_linmem = pci_dev_list[i].membase;
	ec->ec_bus    = pci_dev_list[i].bus;
	ec->ec_dev    = pci_dev_list[i].devfn;
	ec->ec_fnc    =
	pci_dev_list[i].suffix = -1;
	attached = 1;
	break;
      }
  }
  if (attached == 0)
    return 0; 
*/
	getAddressing(devind, ec);

  /* ===== Bus Master ? ===== */
  /*pcibios_read_config_word(ec->ec_bus, ec->ec_devfn, PCI_COMMAND, &pci_cmd);*/
  pci_cmd = pci_attr_r32(devind, PCI_CR);
  if (!(pci_cmd & PCI_COMMAND_MASTER)) {
    pci_cmd |= PCI_COMMAND_MASTER;
    /*pcibios_write_config_word(ec->ec_bus, ec->ec_devfn, PCI_COMMAND, pci_cmd);*/
    pci_attr_w32(devind, PCI_CR, pci_cmd);
  }

  /* ===== Probe Details ===== */
  ioaddr = ec->ec_port;

  out_word(ioaddr+LANCE_RESET, in_word(ioaddr+LANCE_RESET)); /* Reset */
  out_word(ioaddr+LANCE_ADDR, 0x0);                          /* Sw to win 0 */
  if (in_word(ioaddr+LANCE_DATA) != 0x4)
    {
      ec->mode=EC_DISABLED;
    }
  /* Probe Chip Version */
  out_word(ioaddr+LANCE_ADDR, 88);     /* Get the version of the chip */
  if (in_word(ioaddr+LANCE_ADDR) != 88)
    lance_version = 0;
  else
    {
      chip_version = in_word(ioaddr+LANCE_DATA);
      out_word(ioaddr+LANCE_ADDR, 89);
      chip_version |= in_word(ioaddr+LANCE_DATA) << 16;
      if ((chip_version & 0xfff) != 0x3)
        {
          ec->mode=EC_DISABLED;
        }
      chip_version = (chip_version >> 12) & 0xffff;
      for (lance_version = 1; chip_table[lance_version].id_number != 0;
           ++lance_version)
        if (chip_table[lance_version].id_number == chip_version)
          break;
    }

#if 0
  printf("%s: %s at %X:%d\n",
	 ec->port_name, chip_table[lance_version].name,
	 ec->ec_port, ec->ec_irq);
#endif

  return lance_version;
}


/*===========================================================================*
 *                            lance_init_card                                *
 *===========================================================================*/
static void lance_init_card(ec)
ether_card_t *ec;
{
  int i;
  Address l;
  unsigned short ioaddr = ec->ec_port;

  /* ============= setup init_block(cf. lance_probe1) ================ */
  /* make sure data structure is 8-byte aligned */
  l = ((Address)lance + 7) & ~7;
  lp = (struct lance_interface *)l;
  lp->init_block.mode = 0x3;      /* disable Rx and Tx */
  lp->init_block.filter[0] = lp->init_block.filter[1] = 0x0;
  /* using multiple Rx/Tx buffer */
  lp->init_block.rx_ring 
    = (virt_to_bus(&lp->rx_ring) & 0xffffff) | RX_RING_LEN_BITS;
  lp->init_block.tx_ring 
    = (virt_to_bus(&lp->tx_ring) & 0xffffff) | TX_RING_LEN_BITS;

  l = virt_to_bus(&lp->init_block);
  out_word(ioaddr+LANCE_ADDR, 0x1); 
  (void)in_word(ioaddr+LANCE_ADDR);
  out_word(ioaddr+LANCE_DATA, (unsigned short)l);
  out_word(ioaddr+LANCE_ADDR, 0x2);
  (void)in_word(ioaddr+LANCE_ADDR);
  out_word(ioaddr+LANCE_DATA, (unsigned short)(l >> 16));
  out_word(ioaddr+LANCE_ADDR, 0x4);
  (void)in_word(ioaddr+LANCE_ADDR);
  out_word(ioaddr+LANCE_DATA, 0x915);
  out_word(ioaddr+LANCE_ADDR, 0x0);
  (void)in_word(ioaddr+LANCE_ADDR);

  /* ============= Get MAC address (cf. lance_probe1) ================ */
  for (i = 0; i < 6; ++i)
    ec->mac_address.ea_addr[i]=in_byte(ioaddr+LANCE_ETH_ADDR+i);

  /* ============ (re)start init_block(cf. lance_reset) =============== */
  /* Reset the LANCE */
  (void)in_word(ioaddr+LANCE_RESET);

  /* ----- Re-initialize the LANCE ----- */
  /* Set station address */
  for (i = 0; i < 6; ++i)
    lp->init_block.phys_addr[i] = ec->mac_address.ea_addr[i];
  /* Preset the receive ring headers */
  for (i=0; i<RX_RING_SIZE; i++) {
    lp->rx_ring[i].buf_length = -ETH_FRAME_LEN;
    /* OWN */
    lp->rx_ring[i].u.base = virt_to_bus(lp->rbuf[i]) & 0xffffff;
    /* we set the top byte as the very last thing */
    lp->rx_ring[i].u.addr[3] = 0x80;
  }
  /* Preset the transmitting ring headers */
  for (i=0; i<TX_RING_SIZE; i++) {
    lp->tx_ring[i].u.base = 0;
    isstored[i] = 0;
  }
  lp->init_block.mode = 0x0;      /* enable Rx and Tx */

  l = (Address)virt_to_bus(&lp->init_block);
  out_word(ioaddr+LANCE_ADDR, 0x1);
  (void)in_word(ioaddr+LANCE_ADDR);
  out_word(ioaddr+LANCE_DATA, (short)l);
  out_word(ioaddr+LANCE_ADDR, 0x2);
  (void)in_word(ioaddr+LANCE_ADDR);
  out_word(ioaddr+LANCE_DATA, (short)(l >> 16));
  out_word(ioaddr+LANCE_ADDR, 0x4);
  (void)in_word(ioaddr+LANCE_ADDR);
  out_word(ioaddr+LANCE_DATA, 0x915);
  out_word(ioaddr+LANCE_ADDR, 0x0);
  (void)in_word(ioaddr+LANCE_ADDR);

  /* ----- start when init done. ----- */
  out_word(ioaddr+LANCE_DATA, 0x4);           /* stop */
  out_word(ioaddr+LANCE_DATA, 0x1);           /* init */
  for (i = 10000; i > 0; --i)
    if (in_word(ioaddr+LANCE_DATA) & 0x100)
      break;

  /* Set 'Multicast Table' */
  for (i=0;i<4;++i)
    {
      out_word(ioaddr+LANCE_ADDR, 0x8 + i);
      out_word(ioaddr+LANCE_DATA, 0xffff);
    }

  /* Set 'Receive Mode' */
  if (ec->flags & ECF_PROMISC)
    {
      out_word(ioaddr+LANCE_ADDR, 0xf);
      out_word(ioaddr+LANCE_DATA, 0x8000);
    }
  else
    {
      if (ec->flags & (ECF_BROAD | ECF_MULTI))
        {
          out_word(ioaddr+LANCE_ADDR, 0xf);
          out_word(ioaddr+LANCE_DATA, 0x0000);
        }
      else
        {
          out_word(ioaddr+LANCE_ADDR, 0xf);
          out_word(ioaddr+LANCE_DATA, 0x4000);
        }
    }
  
  out_word(ioaddr+LANCE_ADDR, 0x0);
  (void)in_word(ioaddr+LANCE_ADDR);
  out_word(ioaddr+LANCE_DATA, 0x142);   /* start && enable interrupt */

  return;
}
