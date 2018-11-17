Building Heimdal for Windows
===================

1. Introduction
---------------

Heimdal can be built and run on Windows XP or later.  Older OSs may
work, but have not been tested.

2. Prerequisites
----------------

* __Microsoft Visual C++ Compiler__: Heimdal has been tested with
  Microsoft Visual C/C++ compiler version 15.x.  This corresponds to
  Microsoft Visual Studio version 2008.  The compiler and tools that
  are included with Microsoft Windows SDK versions 6.1 and later can
  also be used for building Heimdal.  If you have a recent Windows
  SDK, then you already have a compatible compiler.

* __Microsoft Windows SDK__: Heimdal has been tested with Microsoft
  Windows SDK version 6.1 and 7.0.

* __Microsoft HTML Help Compiler__: Needed for building documentation.

* __Perl__: A recent version of Perl.  Tested with ActiveState
  ActivePerl.

* __Python__: Tested with Python 2.5 and 2.6.

* __WiX__: The Windows [Installer XML toolkit (WiX)][1] Version 3.x is
  used to build the installers.

* __Cygwin__: The Heimdal build system requires a number of additional
  tools: `awk`, `yacc`, `lex`, `cmp`, `sed`, `makeinfo`, `sh`
  (Required for running tests).  These can be found in the Cygwin
  distribution.  MinGW or GnuWin32 may also be used instead of Cygwin.
  However, a recent build of `makeinfo` is required for building the
  documentation. Cygwin makeinfo 4.7 is known to work.

* __Certificate for code-signing__: The Heimdal build produces a
  number of Assemblies that should be signed if they are to be
  installed via Windows Installer.  In addition, all executable
  binaries produced by the build including installers can be signed
  and timestamped if a code-signing certificate is available.
  As of 1 January 2016 Windows 7 and above require the use of sha256
  signatures.  The signtool.exe provided with Windows SDK 8.1 or
  later must be used.

[1]: http://wix.sourceforge.net/

3. Setting up the build environment
-----------------------------------

* Start with a Windows SDK or Visual Studio build environment.  The
  target platform, OS and build type (debug / release) is determined
  by the build environment.

  E.g.: If you are using the Windows SDK, you can use the `SetEnv.Cmd`
  script to set up a build environment targetting 64-bit Windows XP or
  later with:

      SetEnv.Cmd /xp /x64 /Debug

  The build will produce debug binaries.  If you specify

      SetEnv.Cmd /xp /x64 /Release

  the build will produce release binaries.

* Add any directories to `PATH` as necessary for tools required by
  the build to be found.  The build scripts will check for build
  tools at the start of the build and will indicate which ones are
  missing.  In general, adding Perl, Python, WiX, HTML Help Compiler and
  Cygwin binary directories to the path should be sufficient.

* Set up environment variables for code signing.  This can be done in
  one of two ways.  By specifying options for `signtool` or by
  specifying the code-signing command directly.  To use `signtool`,
  define `SIGNTOOL_C` and optionally, `SIGNTOOL_O` and `SIGNTOOL_T`.

  - `SIGNTOOL_C`: Certificate selection and private key selection
    options for `signtool`.

    E.g.:

        set SIGNTOOL_C=/f c:\mycerts\codesign.pfx

	set SIGNTOOL_C=/n "Certificate Subject Name" /a

  - `SIGNTOOL_O`: Signing parameter options for `signtool`. Optional.

    E.g.:

        set SIGNTOOL_O=/du http://example.com/myheimdal

  - `SIGNTOOL_T`: SHA1 Timestamp URL for `signtool`.  If not specified,
    defaults to `http://timestamp.verisign.com/scripts/timstamp.dll`.

  - `SIGNTOOL_T_SHA256`: SHA256 Timestamp URL for `signtool`.  If not
    specified, defaults to `http://timestamp.geotrust.com/tsa`.

  - `CODESIGN`: SHA1 Code signer command.  This environment variable, if
    defined, overrides the `SIGNTOOL_*` variables.  It should be
    defined to be a command that takes one parameter: the binary to be
    signed.

  - `CODESIGN_SHA256`: SHA256 Code signer command.  This environment variable, if
    defined, applies a second SHA256 signature to the parameter.  It should be
    defined to be a command that takes one parameter: the binary to be
    signed.

    E.g.:

        set CODESIGN=c:\scripts\mycodesigner.cmd
	set CODESIGN_SHA256=c:\scripts\mycodesigner256.cmd

* Define the code sign public key token.  This is contained in the
  environment variable `CODESIGN_PKT` and is needed to build the
  Heimdal assemblies.  If you are not using a code-sign certificate,
  set this to `0000000000000000`.

  You can use the `pktextract` tool to determine the public key token
  corresponding to your code signing certificate as follows (assuming
  your code signing certificate is in `c:\mycerts\codesign.cer`:

      pktextract c:\mycerts\codesign.cer

  The above command will output the certificate name, key size and the
  public key token.  Set the `CODESIGN_PKT` variable to the
  `publicKeyToken` value (excluding quotes).

  E.g.:

      set CODESIGN_PKT=abcdef0123456789

4. Running the build
--------------------

Change the current directory to the root of the Heimdal source tree
and run:

    nmake /f NTMakefile

This should build the binaries, assemblies and the installers.

The build can also be invoked from any subdirectory that contains an
`NTMakefile` using the same command.  Keep in mind that there are
inter-dependencies between directories and therefore it is recommended
that a full build be invoked from the root of the source tree.

Tests can be invoked, after a full build, by executing:

    nmake /f NTMakefile test

The build tree can be cleaned with:

    nmake /f NTMakefile clean

It is recommended that both AMD64 and X86 builds take place on the
same machine.  This permits a multi-platform installer package to
be built.  First build for X86 and then build AMD64

    nmake /f NTMakefile MULTIPLATFORM_INSTALLER=1

The build must be executed under cmd.exe.
