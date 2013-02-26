// Copyright 2012 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/// \file run.h
/// Structures and functions to run a program in an isolated manner.
///
/// Pretty much all the functions of this module (ab)use global variables to
/// store their state.  This means that they are not reentrant, but this is by
/// design: the whole point of implementing standalone testers is to simplify
/// the writing of this code by being able to make such assumptions.

#if !defined(KYUA_RUN_H)
#define KYUA_RUN_H

#include <stdbool.h>
#include <unistd.h>

#include "defs.h"
#include "error_fwd.h"
#include "run_fwd.h"


extern const char* kyua_run_tmpdir;


/// Parameters that indicate how to isolate a subprocess.
///
/// In the name of simplicity, the fields of this structure can be considered
/// public.
struct kyua_run_params {
    /// Execution deadline for the subprocess.
    unsigned long timeout_seconds;

    /// Work directory for the subprocess to use.
    const char* work_directory;

    /// UID of the user to switch to before running the subprocess.
    uid_t unprivileged_user;

    /// GID of the group to switch to before running the subprocess.
    gid_t unprivileged_group;
};


void kyua_run_params_init(kyua_run_params_t*);


void kyua_run_exec(const char*, const char* const*) KYUA_DEFS_NORETURN;


kyua_error_t kyua_run_fork(const kyua_run_params_t*, pid_t* const);
kyua_error_t kyua_run_wait(const pid_t, int*, bool*);


kyua_error_t kyua_run_work_directory_enter(const char*, const uid_t,
                                           const gid_t, char**);
kyua_error_t kyua_run_work_directory_leave(char**);


#endif  // !defined(KYUA_RUN_H)
