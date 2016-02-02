/*===- InstrProfilingFile.c - Write instrumentation to a file -------------===*\
|*
|*                     The LLVM Compiler Infrastructure
|*
|* This file is distributed under the University of Illinois Open Source
|* License. See LICENSE.TXT for details.
|*
\*===----------------------------------------------------------------------===*/

#include "InstrProfiling.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNCONST(ptr) ((void *)(uintptr_t)(ptr))

static int writeFile(FILE *File) {
  /* Match logic in __llvm_profile_write_buffer(). */
  const __llvm_profile_data *DataBegin = __llvm_profile_data_begin();
  const __llvm_profile_data *DataEnd = __llvm_profile_data_end();
  const uint64_t *CountersBegin = __llvm_profile_counters_begin();
  const uint64_t *CountersEnd   = __llvm_profile_counters_end();
  const char *NamesBegin = __llvm_profile_names_begin();
  const char *NamesEnd   = __llvm_profile_names_end();

  /* Calculate size of sections. */
  const uint64_t DataSize = DataEnd - DataBegin;
  const uint64_t CountersSize = CountersEnd - CountersBegin;
  const uint64_t NamesSize = NamesEnd - NamesBegin;
  const uint64_t Padding = sizeof(uint64_t) - NamesSize % sizeof(uint64_t);

  /* Enough zeroes for padding. */
  const char Zeroes[sizeof(uint64_t)] = {0};

  /* Create the header. */
  uint64_t Header[PROFILE_HEADER_SIZE];
  Header[0] = __llvm_profile_get_magic();
  Header[1] = __llvm_profile_get_version();
  Header[2] = DataSize;
  Header[3] = CountersSize;
  Header[4] = NamesSize;
  Header[5] = (uintptr_t)CountersBegin;
  Header[6] = (uintptr_t)NamesBegin;

  /* Write the data. */
#define CHECK_fwrite(Data, Size, Length, File) \
  do { if (fwrite(Data, Size, Length, File) != Length) return -1; } while (0)
  CHECK_fwrite(Header,        sizeof(uint64_t), PROFILE_HEADER_SIZE, File);
  CHECK_fwrite(DataBegin,     sizeof(__llvm_profile_data), DataSize, File);
  CHECK_fwrite(CountersBegin, sizeof(uint64_t), CountersSize, File);
  CHECK_fwrite(NamesBegin,    sizeof(char), NamesSize, File);
  CHECK_fwrite(Zeroes,        sizeof(char), Padding, File);
#undef CHECK_fwrite

  return 0;
}

static int writeFileWithName(const char *OutputName) {
  int RetVal;
  FILE *OutputFile;
  if (!OutputName || !OutputName[0])
    return -1;

  /* Append to the file to support profiling multiple shared objects. */
  OutputFile = fopen(OutputName, "a");
  if (!OutputFile)
    return -1;

  RetVal = writeFile(OutputFile);

  fclose(OutputFile);
  return RetVal;
}

__attribute__((weak)) int __llvm_profile_OwnsFilename = 0;
__attribute__((weak)) const char *__llvm_profile_CurrentFilename = NULL;

static void setFilename(const char *Filename, int OwnsFilename) {
  if (__llvm_profile_OwnsFilename)
    free(UNCONST(__llvm_profile_CurrentFilename));

  __llvm_profile_CurrentFilename = Filename;
  __llvm_profile_OwnsFilename = OwnsFilename;
}

static void truncateCurrentFile(void) {
  const char *Filename = __llvm_profile_CurrentFilename;
  if (!Filename || !Filename[0])
    return;

  /* Truncate the file.  Later we'll reopen and append. */
  FILE *File = fopen(Filename, "w");
  if (!File)
    return;
  fclose(File);
}

static void setDefaultFilename(void) { setFilename("default.profraw", 0); }

int getpid(void);
static int setFilenameFromEnvironment(void) {
  const char *Filename = getenv("LLVM_PROFILE_FILE");
  if (!Filename || !Filename[0])
    return -1;

  /* Check the filename for "%p", which indicates a pid-substitution. */
#define MAX_PID_SIZE 16
  char PidChars[MAX_PID_SIZE] = {0};
  int NumPids = 0;
  int PidLength = 0;
  int I;
  for (I = 0; Filename[I]; ++I)
    if (Filename[I] == '%' && Filename[++I] == 'p')
      if (!NumPids++) {
        PidLength = snprintf(PidChars, MAX_PID_SIZE, "%d", getpid());
        if (PidLength <= 0)
          return -1;
      }
  if (!NumPids) {
    setFilename(Filename, 0);
    return 0;
  }

  /* Allocate enough space for the substituted filename. */
  char *Allocated = (char*)malloc(I + NumPids*(PidLength - 2) + 1);
  if (!Allocated)
    return -1;

  /* Construct the new filename. */
  int J;
  for (I = 0, J = 0; Filename[I]; ++I)
    if (Filename[I] == '%') {
      if (Filename[++I] == 'p') {
        memcpy(Allocated + J, PidChars, PidLength);
        J += PidLength;
      }
      /* Drop any unknown substitutions. */
    } else
      Allocated[J++] = Filename[I];
  Allocated[J] = 0;

  /* Use the computed name. */
  setFilename(Allocated, 1);
  return 0;
}

static void setFilenameAutomatically(void) {
  if (!setFilenameFromEnvironment())
    return;

  setDefaultFilename();
}

__attribute__((visibility("hidden")))
void __llvm_profile_initialize_file(void) {
  /* Check if the filename has been initialized. */
  if (__llvm_profile_CurrentFilename)
    return;

  /* Detect the filename and truncate. */
  setFilenameAutomatically();
  truncateCurrentFile();
}

__attribute__((visibility("hidden")))
void __llvm_profile_set_filename(const char *Filename) {
  setFilename(Filename, 0);
  truncateCurrentFile();
}

__attribute__((visibility("hidden")))
int __llvm_profile_write_file(void) {
  /* Check the filename. */
  if (!__llvm_profile_CurrentFilename)
    return -1;

  /* Write the file. */
  return writeFileWithName(__llvm_profile_CurrentFilename);
}

static void writeFileWithoutReturn(void) {
  __llvm_profile_write_file();
}

__attribute__((visibility("hidden")))
int __llvm_profile_register_write_file_atexit(void) {
  static int HasBeenRegistered = 0;

  if (HasBeenRegistered)
    return 0;

  HasBeenRegistered = 1;
  return atexit(writeFileWithoutReturn);
}
