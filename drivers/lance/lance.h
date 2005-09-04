/*#include "kernel.h"*/
#include <net/gen/ether.h>
#include <net/gen/eth_io.h>

/* PCI STUFF */
#define PCI_BASE_ADDRESS_0		0x10
#define PCI_BASE_ADDRESS_1		0x14
#define PCI_BASE_ADDRESS_2		0x18
#define PCI_BASE_ADDRESS_3		0x1c
#define PCI_BASE_ADDRESS_4		0x20
#define PCI_BASE_ADDRESS_5		0x24

#define PCI_BASE_ADDRESS_IO_MASK	(~0x03)
#define PCI_BASE_ADDRESS_SPACE_IO	0x01
#define PCI_INTERRUPT_LINE		0x3c
#define PCI_INTERRUPT_PIN		0x3d

#define PCI_COMMAND_MASTER		0x4

#define PCI_VENDOR_ID_AMD	0x1022
#define PCI_DEVICE_ID_AMD_LANCE	0x2000


/* supported max number of ether cards */
#define EC_PORT_NR_MAX 2

/* macros for 'mode' */
#define EC_DISABLED    0x0
#define EC_SINK        0x1
#define EC_ENABLED     0x2

/* macros for 'flags' */
#define ECF_EMPTY       0x000
#define ECF_PACK_SEND   0x001
#define ECF_PACK_RECV   0x002
#define ECF_SEND_AVAIL  0x004
#define ECF_READING     0x010
#define ECF_PROMISC     0x040
#define ECF_MULTI       0x080
#define ECF_BROAD       0x100
#define ECF_ENABLED     0x200
#define ECF_STOPPED     0x400

/* === macros for ether cards (our generalized version) === */
#define EC_ISR_RINT     0x0001
#define EC_ISR_WINT     0x0002
#define EC_ISR_RERR     0x0010
#define EC_ISR_WERR     0x0020
#define EC_ISR_ERR      0x0040
#define EC_ISR_RST      0x0100

/* IOVEC */
#define IOVEC_NR        16
typedef struct iovec_dat
{
  iovec_t iod_iovec[IOVEC_NR];
  int iod_iovec_s;
  int iod_proc_nr;
  vir_bytes iod_iovec_addr;
} iovec_dat_t;

#define ETH0_SELECTOR  0x61
#define ETH1_SELECTOR  0x69

/* ====== ethernet card info. ====== */
typedef struct ether_card
{
  /* ####### MINIX style ####### */
  char port_name[sizeof("eth_card#n")];
  int flags;
  int mode;
  int transfer_mode;
  eth_stat_t eth_stat;
  iovec_dat_t read_iovec;
  iovec_dat_t write_iovec;
  iovec_dat_t tmp_iovec;
  vir_bytes write_s;
  vir_bytes read_s;
  int client;
  message sendmsg;

  /* ######## device info. ####### */
  port_t ec_port;
  phys_bytes ec_linmem;
  int ec_irq;
  int ec_int_pending;
  int ec_hook;

  int ec_ramsize;
  /* PCI */
  u8_t ec_pcibus;	
  u8_t ec_pcidev;	
  u8_t ec_pcifunc;	
 
  /* Addrassing */
  u16_t ec_memseg;
  vir_bytes ec_memoff;
  
  ether_addr_t mac_address;
} ether_card_t;

#define DEI_DEFAULT    0x8000

