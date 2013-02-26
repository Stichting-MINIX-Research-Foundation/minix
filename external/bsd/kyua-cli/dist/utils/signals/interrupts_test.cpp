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

#include "utils/signals/interrupts.hpp"

extern "C" {
#include <signal.h>
#include <unistd.h>
}

#include <cstdlib>

#include <atf-c++.hpp>

#include "utils/format/macros.hpp"
#include "utils/process/child.ipp"
#include "utils/process/status.hpp"
#include "utils/signals/exceptions.hpp"
#include "utils/signals/programmer.hpp"

namespace process = utils::process;
namespace signals = utils::signals;


namespace {


/// Set to the signal that fired; -1 if none.
static volatile int fired_signal = -1;


/// Test handler for signals.
///
/// \post fired_signal is set to the signal that triggered the handler.
///
/// \param signo The signal that triggered the handler.
static void
signal_handler(const int signo)
{
    fired_signal = signo;
}


/// Child process that pauses waiting to be killed.
static void
pause_child(void)
{
    sigset_t mask;
    sigemptyset(&mask);
    if (sigsuspend(&mask) == -1)
        ::exit(EXIT_FAILURE);
    else {
        // If this happens, it is because we received a non-deadly signal and
        // the execution resumed.  This is not what we expect, so exit with an
        // arbitrary code.
        ::exit(45);
    }
}


/// Checks that interrupts_handler() handles a particular signal.
///
/// This indirectly checks the check_interrupt() function, which is not part of
/// the class but is tightly related.
///
/// \param signo The signal to check.
static void
check_interrupts_handler(const int signo)
{
    signals::programmer test_handler(signo, signal_handler);

    {
        signals::interrupts_handler interrupts;

        signals::check_interrupt();
        ::kill(getpid(), signo);
        ATF_REQUIRE_THROW_RE(signals::interrupted_error,
                             F("Interrupted by signal %s") % signo,
                             signals::check_interrupt());
    }

    ATF_REQUIRE_EQ(-1, fired_signal);
    ::kill(getpid(), signo);
    ATF_REQUIRE_EQ(signo, fired_signal);

    test_handler.unprogram();
}


/// Checks that interrupts_inhibiter() handles a particular signal.
///
/// \param signo The signal to check.
static void
check_interrupts_inhibiter(const int signo)
{
    signals::programmer test_handler(signo, signal_handler);

    {
        signals::interrupts_inhibiter inhibiter;
        ::kill(::getpid(), signo);
        ATF_REQUIRE_EQ(-1, fired_signal);
    }
    ATF_REQUIRE_EQ(signo, fired_signal);

    test_handler.unprogram();
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(interrupts_handler__sighup);
ATF_TEST_CASE_BODY(interrupts_handler__sighup)
{
    check_interrupts_handler(SIGHUP);
}


ATF_TEST_CASE_WITHOUT_HEAD(interrupts_handler__sigint);
ATF_TEST_CASE_BODY(interrupts_handler__sigint)
{
    check_interrupts_handler(SIGINT);
}


ATF_TEST_CASE_WITHOUT_HEAD(interrupts_handler__sigterm);
ATF_TEST_CASE_BODY(interrupts_handler__sigterm)
{
    check_interrupts_handler(SIGTERM);
}


ATF_TEST_CASE_WITHOUT_HEAD(interrupts_handler__kill_children);
ATF_TEST_CASE_BODY(interrupts_handler__kill_children)
{
    std::auto_ptr< process::child > child1(process::child::fork_capture(
         pause_child));
    std::auto_ptr< process::child > child2(process::child::fork_capture(
         pause_child));

    signals::interrupts_handler interrupts;

    // Our children pause until the reception of a signal.  Interrupting
    // ourselves will cause the signal to be re-delivered to our children due to
    // the interrupts_handler semantics.  If this does not happen, the wait
    // calls below would block indefinitely and cause our test to time out.
    ::kill(::getpid(), SIGHUP);

    const process::status status1 = child1->wait();
    ATF_REQUIRE(status1.signaled());
    ATF_REQUIRE_EQ(SIGHUP, status1.termsig());
    const process::status status2 = child2->wait();
    ATF_REQUIRE(status2.signaled());
    ATF_REQUIRE_EQ(SIGHUP, status2.termsig());
}


ATF_TEST_CASE_WITHOUT_HEAD(interrupts_inhibiter__sighup);
ATF_TEST_CASE_BODY(interrupts_inhibiter__sighup)
{
    check_interrupts_inhibiter(SIGHUP);
}


ATF_TEST_CASE_WITHOUT_HEAD(interrupts_inhibiter__sigint);
ATF_TEST_CASE_BODY(interrupts_inhibiter__sigint)
{
    check_interrupts_inhibiter(SIGINT);
}


ATF_TEST_CASE_WITHOUT_HEAD(interrupts_inhibiter__sigterm);
ATF_TEST_CASE_BODY(interrupts_inhibiter__sigterm)
{
    check_interrupts_inhibiter(SIGTERM);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, interrupts_handler__sighup);
    ATF_ADD_TEST_CASE(tcs, interrupts_handler__sigint);
    ATF_ADD_TEST_CASE(tcs, interrupts_handler__sigterm);
    ATF_ADD_TEST_CASE(tcs, interrupts_handler__kill_children);

    ATF_ADD_TEST_CASE(tcs, interrupts_inhibiter__sighup);
    ATF_ADD_TEST_CASE(tcs, interrupts_inhibiter__sigint);
    ATF_ADD_TEST_CASE(tcs, interrupts_inhibiter__sigterm);
}
