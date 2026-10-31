/*	$NetBSD: drv0.h,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

#ifndef _GNU_EFI_APPS_DRV0_H_
#define _GNU_EFI_APPS_DRV0_H_

#ifdef __cplusplus
extern "C" {
#endif

/* UEFI naming conventions */
#define GNU_EFI_APPS_DRV0_PROTOCOL_GUID \
{ 0xe4dcafd0, 0x586c, 0x4b3d, {0x86, 0xe7, 0x28, 0xde, 0x7f, 0xcc, 0x04, 0xb9} }

INTERFACE_DECL(_GNU_EFI_APPS_DRV0_PROTOCOL);

typedef
EFI_STATUS
(EFIAPI *GNU_EFI_APPS_DRV0_SAY_HELLO) (
    IN const CHAR16 *HelloWho
    );

typedef
EFI_STATUS
(EFIAPI *GNU_EFI_APPS_DRV0_GET_NUMBER_OF_HELLO) (
    OUT UINTN *NumberOfHello
    );

typedef struct _GNU_EFI_APPS_DRV0_PROTOCOL {
  GNU_EFI_APPS_DRV0_SAY_HELLO           SayHello;
  GNU_EFI_APPS_DRV0_GET_NUMBER_OF_HELLO GetNumberOfHello;
} GNU_EFI_APPS_DRV0_PROTOCOL;

#ifdef __cplusplus
}
#endif

#endif
