
void oxpcie_set_vaddr(unsigned char *vaddr);
void oxpcie_putc(char c);
int oxpcie_in(void);

#include "serial.h"

/* OXPCIe952 info */
#define UART1BASE_550   0x1000
#define UART1BASE_650   0x1090
#define UART1BASE_950
#define BASELINEICR     (UART1BASE_550 + 0xC0)
#define         OXPCIE_THR      oxpcie_vaddr[UART1BASE_550 + THRREG]
#define         OXPCIE_RBR      oxpcie_vaddr[UART1BASE_550 + RBRREG]
#define         OXPCIE_LSR      oxpcie_vaddr[UART1BASE_550 + LSRREG]
#define         OXPCIE_LCR      oxpcie_vaddr[UART1BASE_550 + LCRREG]
#define         OXPCIE_DLL      oxpcie_vaddr[UART1BASE_550 + 0x00]
#define         OXPCIE_DLM      oxpcie_vaddr[UART1BASE_550 + 0x01]
#define         OXPCIE_FICR     oxpcie_vaddr[UART1BASE_550 + FICRREG]
#define         OXPCIE_SPR      oxpcie_vaddr[UART1BASE_550 + SPRREG]
#define         OXPCIE_EFR      oxpcie_vaddr[UART1BASE_650 + 0x10]
#define         OXPCIE_ICR      oxpcie_vaddr[UART1BASE_950 + 0x05]

#define         OXPCIE_CPR      oxpcie_vaddr[BASELINEICR + 0x01]
#define         OXPCIE_TCR      oxpcie_vaddr[BASELINEICR + 0x02]
#define         OXPCIE_CPR2     oxpcie_vaddr[BASELINEICR + 0x03]
#define         OXPCIE_CSR      oxpcie_vaddr[BASELINEICR + 0x0C]
#define         OXPCIE_PIDX     oxpcie_vaddr[BASELINEICR + 0x12]

#define         LCR_CONFIG      0x03 /* bits 6:0 -= 0x03 => 8N1, no break. */

