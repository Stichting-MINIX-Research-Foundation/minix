/*	$NetBSD: cmdline.c,v 1.1.1.1 2018/08/16 18:17:47 jmcneill Exp $	*/

#include "lib.h"

#include "efiprot.h"
#include "efishellintf.h"
#include "efishellparm.h"

#ifndef MAX_ARGV_CONTENTS_SIZE
# define MAX_CMDLINE_SIZE 1024
#endif
#ifndef MAX_ARGC
# define MAX_CMDLINE_ARGC 32
#endif

/*
  Parse LoadedImage options area, called only in case the regular
  shell protos are not available.

  Format of LoadedImage->LoadOptions appears to be a
  single-space-separated list of args (looks like the shell already
  pre-parses the input, it apparently folds several consecutive spaces
  into one):
    argv[0] space argv[1] (etc.) argv[N] space \0 cwd \0 other data
  For safety, we support the trailing \0 without a space before, as
  well as several consecutive spaces (-> several args).
*/
static
INTN
GetShellArgcArgvFromLoadedImage(
    EFI_HANDLE ImageHandle,
    CHAR16 **ResultArgv[]
    )
{
  EFI_STATUS Status;
  void *LoadedImage = NULL;
  static CHAR16 ArgvContents[MAX_CMDLINE_SIZE];
  static CHAR16 *Argv[MAX_CMDLINE_ARGC], *ArgStart, *c;
  UINTN Argc = 0, BufLen;

  Status = uefi_call_wrapper(BS->OpenProtocol, 6,
                             ImageHandle,
                             &LoadedImageProtocol,
                             &LoadedImage,
                             ImageHandle,
                             NULL,
                             EFI_OPEN_PROTOCOL_GET_PROTOCOL
                             );
  if (EFI_ERROR(Status))
    return -1;

  BufLen = ((EFI_LOADED_IMAGE *)LoadedImage)->LoadOptionsSize;
  if (BufLen < 2)  /* We are expecting at least a \0 */
    return -1;
  else if (BufLen > sizeof(ArgvContents))
    BufLen = sizeof(ArgvContents);

  CopyMem(ArgvContents, ((EFI_LOADED_IMAGE *)LoadedImage)->LoadOptions, BufLen);
  ArgvContents[MAX_CMDLINE_SIZE - 1] = L'\0';

  for (c = ArgStart = ArgvContents ; *c != L'\0' ; ++c) {
    if (*c == L' ') {
      *c = L'\0';
      if (Argc < MAX_CMDLINE_ARGC) Argv[Argc++] = ArgStart;
      ArgStart = c + 1;
    }
  }

  if ((*ArgStart != L'\0') && (Argc < MAX_CMDLINE_ARGC))
    Argv[Argc++] = ArgStart;

  // Print(L"Got argc/argv from loaded image proto\n");
  *ResultArgv = Argv;
  return Argc;
}

INTN GetShellArgcArgv(EFI_HANDLE ImageHandle, CHAR16 **Argv[])
{
  // Code inspired from EDK2's
  // ShellPkg/Library/UefiShellCEntryLib/UefiShellCEntryLib.c (BSD)
  EFI_STATUS Status;
  static const EFI_GUID EfiShellParametersProtocolGuid
      = EFI_SHELL_PARAMETERS_PROTOCOL_GUID;
  static const EFI_GUID ShellInterfaceProtocolGuid
      = SHELL_INTERFACE_PROTOCOL_GUID;
  EFI_SHELL_PARAMETERS_PROTOCOL *EfiShellParametersProtocol = NULL;
  EFI_SHELL_INTERFACE *EfiShellInterfaceProtocol = NULL;

  Status = uefi_call_wrapper(BS->OpenProtocol, 6,
                             ImageHandle,
                             (EFI_GUID*)&EfiShellParametersProtocolGuid,
                             (VOID **)&EfiShellParametersProtocol,
                             ImageHandle,
                             NULL,
                             EFI_OPEN_PROTOCOL_GET_PROTOCOL
                             );
  if (!EFI_ERROR(Status))
  {
    // use shell 2.0 interface
    // Print(L"Got argc/argv from shell intf proto\n");
    *Argv = EfiShellParametersProtocol->Argv;
    return EfiShellParametersProtocol->Argc;
  }

  // try to get shell 1.0 interface instead.
  Status = uefi_call_wrapper(BS->OpenProtocol, 6,
                             ImageHandle,
                             (EFI_GUID*)&ShellInterfaceProtocolGuid,
                             (VOID **)&EfiShellInterfaceProtocol,
                             ImageHandle,
                             NULL,
                             EFI_OPEN_PROTOCOL_GET_PROTOCOL
                             );
  if (!EFI_ERROR(Status))
  {
    // Print(L"Got argc/argv from shell params proto\n");
    *Argv = EfiShellInterfaceProtocol->Argv;
    return EfiShellInterfaceProtocol->Argc;
  }

  // shell 1.0 and 2.0 interfaces failed
  return GetShellArgcArgvFromLoadedImage(ImageHandle, Argv);
}
