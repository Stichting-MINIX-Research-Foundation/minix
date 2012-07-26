
    idn wrapper - Client Side IDN Conversion Software for Windows

    Copyright (c) 2000,2001,2002 Japan Network Information Center.
                All rights reserved.

    *** NOTICE ******************************************************
    If you have installed mDN Wrapper (former version of idn wrapper)
    on your system, you should unwrap all the programs before
    installing idn wrapper.
    *****************************************************************


1. Introduction

    For supporting internationalized domain names, each client
    application should convert domain names (their encodings) to that
    DNS server accepts.  This requires applications to handle
    internationalized domain names in its core, and it is the vendor's
    responsibility to make their programs IDN-compatible.

    Although there are ongoing efforts in IETF to standardize IDN
    framework (architecture, encoding etc.) and several RFCs are
    expected to be published soon as the result, not many applications
    support IDN to this date.

    So, there are needs for some helper application which makes legacy
    applications IDN-aware.  `runidn' in idnkit is one of such
    solutions for Unix-like operating systems, and this software, `idn
    wrapper' is the one for Windows.

    On windows, name resolving request is passed to WINSOCK DLL.  idn
    wrapper replaces WINSOCK DLL with the one that can handle IDN,
    which makes legacy windows applications compatible with IDN.

2. Architecture

2.1. Wrapper DLL

    Wrapper DLL resides between application and original DLL.  It
    intercept application's calls to original DLL, and preforms some
    additional processing on those calls.

    +------------+  Call  +------------+  Call  +------------+
    |            |------->|            |------->|            |
    |Application |        |Wrapper DLL |        |Original DLL|
    |            |<-------|            |<-------|            |
    +------------+ Return +------------+ Return +------------+
                           additional
			   processing
			   here

    DLL call from apllication is passed to wrapper DLL.  Wrapper DLL
    then performs some additional processing on that call, and then
    calls original DLL.  Also, result from original DLL will once passed
    to wrapper DLL and wrapper does additional process on that result,
    and finally result will passed to the application.

    idn wrapper provides wrapper DLLs for WINSOCK,
    
        WSOCK32.DLL     WINSOCK V1.1
	WS2_32.DLL      WINSOCK V2.0

    to resolve multi-lingual domain names.

2.2. Wrapping APIs

    idn wrapper performs additional processing on name resolving APIs in
    WINSOCK, listed below.

    both WINSOCK 1.1, WINSOCK 2.0
    
        gethostbyaddr
	gethostbyname
	WSAAsyncGetHostByAddr
	WSAAsyncGetHostByName
	
    only in WINSOCK 2.0
    
	getaddrinfo
	freeaddrinfo
	getnameinfo
        WSALookupServiceBeginA
	WSALookupServiceNextA
	WSALookupServiceEnd

    Some applications do not use these APIs to resolve domain names. 
    `nslookup' is one of those programs. `nslookup' builds and parse DNS
    messages internally and does not use WINSOCK's name resolver APIs.
    idn wrapper cannot make those programs IDN-aware.
    
    NOTE:
      WINSOCK 2.0 also contains WIDE-CHARACTER based name resolution
      APIs,

          WSALookupServiceBeginW
          WSALookupServiceNextW

      idn wrapper does not wrap these APIs.  These APIs are used in
      Microsoft's own internationalization framework.  It is dangerous
      to convert to another internationalization framework.
    
2.3. Other APIs in WINSOCK

    For other APIs in WINSOCK, idn wrapper does nothing, only calls
    original DLL's entries.

    idn wrapper copies original WINSOCK DLLs with renaming
    as below, and forward requests to them.

        wsock32.dll     ->  wsock32o.dll
	ws2_32.dll      ->  ws2_32o.dll

    Wrappper DLL will be installed with original DLL names. So after
    installation of idn wrapper, WINSOCK DLLs should be

        wsock32.dll         idn wrapper for WINSOCK V1.1
	ws2_32.dll          idn wrapper for WINSOCK V2.0
	wsock32o.dll        Original WINSOCK V1.1 DLL
	ws2_32o.dll         Original WINSOCK V2.0 DLL 

2.4. Asynchronous API

    Domain name conversion take place on
    
        request to DNS

            convert from local encoding to DNS compatible encoding

        response from DNS

            convert from DNS encoding to local encoding

    For synchronous APIs, local to DNS conversion is done before calling
    original API, and after return from original API, name should be
    converted from DNS encoding to local encoding.

    But WINSOCK having some asynchronous APIs, such as

	WSAAsyncGetHostByAddr
	WSAAsyncGetHostByName

    In these APIs, completion is notified with windows message.  To
    perform DNS to local conversion, wrapper should hook target window
    procedure to capture those completion messages.
    
    So, if asynchronous API was called, idn wrapper set hook to target
    window procedure (passed with API parameter).  If hook found
    notify message (also given with API parameter), then convert
    resulting name (in DNS encoding) to local encoding.
    
2.5. Installing Wrapper DLLs

    WINSOCK DLLs are placed at Windows's system directory.  To wrap
    WINSOCK DLLs, one could do following sequence at system directory.

        + Rename Original WINSOCK DLLs

	    ren wsock32.dll wsock32o.dll
	    ren ws2_32.dll  ws2_32o.dll

        + Install (copy in) Wrapper DLLs

	    copy somewhere\wsock32.dll wsock32.dll
	    copy somewhere\ws2_32.dll  ws2_32.dll
	    copy another DLLs also

    However, replacing DLLs in Window's system directory is very
    dangerous:

    a)  If you re-install idn wrapper again, original WINSOCK DLLs
        may be lost.

    b)  Some application or service pack will replace WINSOCK DLLs.  It
        may corrupt WINSOCK environment.

    If these happen, at least networking does not work, and worse,
    Windows never startup again.

    So, idn wrapper usually does not wrap in the system directory, but wrap in
    each indivisual application's directory.

    In Windows, DLL will be searched in the following places:
    
        Application's Load Directory
	%SystemRoot%\System32
	%SystemRoot%
	Directories in PATH

    and loaded & linked first found one.  So if installed wrapper DLLs is
    found on application's load directory, the application's call to
    WINSOCK will wrapped.

    But some applications or DLLs are binded to specific DLL, they do
    not rely on above DLL's search path.  For those applcaitons or DLLs,
    idn wrapper (in standard installation) cannot wrap them.

    NOTE:   Netscape is one of those program.  It cannot be wrapped if
            installed to applications directory.  Also WINSOCK DLLs are
            also binded to related DLLs in system directory.  On the
            other hand, Internet Explore or Window Media Player relys on
            standard DLL search path, and well wrapped with idn wrapper.

2.6. At which point conversion applied

    If windows supporting WINSOCK 2.0, there are DLLs one for 1.1 and
    another for 2.0, and call to WINSOCK 1.1 will redirected to 2.0 DLL.

        +------------+  Call  +------------+  Call  +------------+
        |            |------->|            |------->|            |
        |Application |        |WINSOCK 1.1 |        |WINSOCK 2.0 |
        |            |<-------|            |<-------|            |
        +------------+ Return +------------+ Return +------------+

    In this case, calls to 1.1 and 2.0 are both passed to 2.0 DLL.  So
    conversion will done in WINSOCK 2.0 DLL side.

    If windows only supports WINSOCK 1.1, there's 1.1 DLL only.

        +------------+  Call  +------------+
        |            |------->|            |
        |Application |        |WINSOCK 1.1 |
        |            |<-------|            |
        +------------+ Return +------------+

    In this case, conversion must done in 1.1 DLL.

    If idn wrapper was installed on system directory, DLLs will work as
    described above.  But if wrapper was installed on application's
    directory, call/return sequence changes.  Original WINSOCK 1.1 DLL
    in windows seems binded to specific WINSOCK 2.0 DLL, placed at
    window's system diretory.  So call from WINSOCK 1.1 to WINSOCK 2.0
    will passed to original DLL (in system directory) and never passed
    to wrapper DLL in application's directory.  So in this case, both
    1.1 and 2.0 DLLs should coonvert domain name encodings.
    
    These DLL binding is not documented.  It may be change on OS
    versions or DLL versions.  So, mDn wrapper determines place of
    conversion on registry value.  With this registry value, idn
    wrappper absolb OS/DLL variations.
    
    Registry values for idn wrapper will placed under

        HKEY_LOCAL_MACHINE\SOFTWARE\JPNIC\IDN
	HKEY_CURRENT_USER\SOFTWARE\JPNIC\IDN

    Place of conversion is determined with registry value "Where",
    
        Registry Value "Where"   REG_DWORD
	    
	    0       both on WINSOCK 1.1 and WINSOCK 2.0
	    1       if WINSOCK 2.0 exist, only in WINSOCK 2.0
	            otherwise, convert on WINSOCK 1.1
            2       only in WINSOCK 1.1
	    3       only in WINSOCK 2.0

    If you install idn wrapper into application's directory, use "0".
    If you install idn wrapper into system directory, use "1".  If there
    are no "Where" value, idn wrapper uses "0" as default, it is suited
    to installation into application's directory (default installation).

2.7. Converting From/To

    Wrapper DLL convert resolving domain name encoded with local code to
    DNS server's encoding.  Also, wrapper DLL convert resulting name (
    encoded with DNS's encoding) back to local encoding.
    
    There are several proposals for DNS encodings to handle multi-lingual
    domain names.  Wrapper DLL should be configured to convert to one of
    those encodings.  This DNS side encoding will specified with
    registry.  When installing idn wrapper, this registry will set to
    some (yet undefined) DNS encoding.
    
    Registry values for idn wrapper will placed under

        HKEY_LOCAL_MACHINE\SOFTWARE\JPNIC\IDN
	HKEY_CURRENT_USER\SOFTWARE\JPNIC\IDN

    DNS encoding name will given with registry value (REG_SZ) of "Encoding",
    this name must be one of encoding names which 'libmdn' recognize.

        Registry Value "Encoding"   REG_SZ
	
	    Encoding name of DNS server accepts.
    
    Local encodings (Windows Apllication Encodings) is generally
    acquired from process's code page.  'iconv' library, used for idn
    wrapper, generally accepts MS's codepage names.

    Some windows apllication encode domain name with some specific multi-
    lingual encoding. For example, if you configured IE to use UTF-8,
    then domain names are encoded with UTF-8. UTF-8 is one of proposed
    DNS encoding, but DNS server may expect another encoding.
    
    For those cases, idn wrapper accept program specific encoding as
    local encoding.  These program specific local encoding should be
    marked in registry.
    
    Program specific registry setting will placed under

        HKEY_LOCAL_MACHINE\SOFTWARE\JPNIC\IDN\PerProg
	HKEY_CURRENT_USER\SOFTWARE\JPNIC\IDN\PerProg
    
    using program name (executable file name) as key.  For example,
    setting specific to Internet Explore, it executable name is 
    "IEXPLORE", will plcaed at

        HKEY_LOCAL_MACHINE\SOFTWARE\JPNIC\IDN\PerProg\IEXPLORE

    Local encoding name will specified with registry value (REG_SZ) of 
    "Encoding".  This name must be one of encoding names which '
    recognize.libmdn'

        Registry Value "Encoding"   REG_SZ
	
	    Encoding name of application program encodes, if it is not
            system's default encoding.

3. Setup and Configuration

    idn wrapper wraps WINSOCK DLL by placing wrapper (fake) DLLs in
    the application's directory.  For the installation, idn wrapper
    comes with a setup program and a configuration program.

    NOTE:   You can also install idn wrapper DLLs in the Windows
            system directory.  But this installation is very dangerous
	    and may cause severe problems in your system.
	    You should try it at your own risk.

3.1. Setup Program

    To install idn wrapper, run "setup.exe".  Setup program will do:
    
    Installing Files
    
        Copy idn wrapper files (DLL, Program EXE, etc) into diretory
	
	    "\Program Files\JPNIC\idn wrapper"

        This directory may be changed on setup sequence.

    Setting registry entries

        Setup program will create keys and values under registry:
	
	    "HKEY_LOCAL_MACHINES\Software\JPNIC\IDN"

	InstallDir	REG_SZ	"<installation directory>"
	    Pathname of the idn wrapper's installation directory.
	    The installer makes copies of the original WINSOCK DLLs
	    in that directory, which is referenced by the idn wrapper's
	    fake DLLs.
    
        ConfFile        REG_SZ  "<installation directory>\idn.conf"
	    Name of the idnkit's configuration file, which defines
	    various parameter regarding multilingual domain name
	    handling.  See the contents of the file for details.
            This value can be changed with the Configuration Program
	    or the registry editor.

	LogFile		REG_SZ	"<installation directory>\idn_wrapper.log"
	    Name of the idn wrapper's log file.
            This value can be changed with the Configuration Program
	    or the registry editor.

	LogLevel	DWORD	-1
	    Logging level.  Default is -1, which indicates no logging
	    is made.  This value can be changed with the Configuration
	    Program or the registry editor.

        PerProg         KEY
	
	    Under this key, idn wrapper set program specific values. idn
            wrapper uses program's executable name as key, and put
            values under that key.
	    
	    PerProg\<progname>\Where    REG_DWORD Encoding Position
	    PerProg\>progname>\Encoding REG_SZ    Local Encoding Name

            Configuration program set local encpoding name.  "Where"
            value is usually not required in standard installation.  If
            you installed idn wrapper in system directory, chanage
            "Where" values to fit your environment.

    Creating ICON
    
        Setup program will create program icon for idn wrapper's
        configuration program, and put it into "Start Menu".  You can
        start configuration program with it.
	   
3.2. Configuration Program

    Configuration program is a tool for wrap specific program, or unwrap
    programs.  If you start "Configuration Program", you'll get window
    like this.

    +---+-------------------------------------------------+---+---+---+
    |   | idn wrapper - Configuration                     | _ | O | X |
    +---+-------------------------------------------------+---+---+---+
    |          idn wrapper Configuration Program version X.X          |
    +-----------------------------------------------------------------+
    |                  Wrapped Program                    +---------+ |
    | +---------------------------------------------+---+ | Wrap..  | |
    | |                                             | A | +---------+ |
    | |                                             +---+ +---------+ |
    | |                                             |   | | Unwrap..| |
    | |                                             |   | +---------+ |
    | |                                             |   | +---------+ |
    | |                                             |   | |UnwrapAll| |
    | |                                             |   | +---------+ |
    | |                                             |   | +---------+ |
    | |                                             |   | |RewrapAll| |
    | |                                             |   | +---------+ |
    | |                                             |   | +---------+ |
    | |                                             |   | |  Log..  | |
    | |                                             |   | +---------+ |
    | |                                             |   | +---------+ |
    | |                                             +---+ |Advanced.| |
    | |                                             | V | +---------+ |
    | +---+-------------------------------------+---+---+ +---------+ |
    | | < |                                     | > |     |  Exit   | |
    | +---+-------------------------------------+---+     +---------+ |
    +-----------------------------------------------------------------+

    Listbox contains list of current wrapped programs.  Initially it is
    empty.  
    
    To wrap a program, press button "wrap".  You'll get following dialog.
    
    +---+-------------------------------------------------+---+---+---+
    |   | idn wrapper - Wrap Executable                   | _ | O | X |
    +---+-------------------------------------------------+---+---+---+
    |           +----------------------------------------+ +--------+ |
    |  Program: |                                        | |Browse..| |
    |           +----------------------------------------+ +--------+ |
    |           +----------+                                          |
    | Encoding: |          |  o Default  o UTF-8                      |
    |           +----------+                                          |
    |           [] Force local DLL reference                          |
    +-----------------------------------------------------------------+
    |                                           +--------+ +--------+ |
    |                                           |  Wrap  | | Cancel | |
    |                                           +--------+ +--------+ |
    +-----------------------------------------------------------------+

    First, enter program (executable name with full path) or browse
    wrapping exectable from file browser. Then set local encoding of
    that program.  Usually use "Default" as local encoding. If target
    program uses internationalized encoding, then specify "UFT-8". 

    The "Force local DLL reference" button controls the DLL search
    order of the program to be wrapped (Windows95 does not have this
    capability, hence this button does not appear).  If it is checked,
    DLLs in the local directory (the directory which the executable
    file is in) are always preferred, even if the executable specifies
    otherwise.  If you have problem with wrapping, checking this
    button may solve the problem, but it is also possible that it
    causes other problem.

    Finally, put "wrap" button to wrap specified program with given
    encoding. Wrapped program will be listed in listbox of the first
    window.

    When you install a new version of idn wrapper, you have to re-wrap
    your programs in order to update DLLs used for wrapping.  "Rewrap
    all" button is provided for this purpose.  Just press the button,
    and all the currently wrapped programs will be re-wrapped.

    To unwrap a program, press button "unwrap".  You'll get following 
    confirmating dialog.
    
    +---+-------------------------------------------------+---+---+---+
    |   | idn wrapper - Unwrap Executable                 | _ | O | X |
    +---+-------------------------------------------------+---+---+---+
    |           +---------------------------------------------------+ |
    | Program:  |                                                   | |
    |           +---------------------------------------------------+ |
    +-----------------------------------------------------------------+
    |                                           +--------+ +--------+ |
    |                                           | Unwrap | | Cancel | |
    |                                           +--------+ +--------+ |
    +-----------------------------------------------------------------+

    If you unwrap a program, the program will be vanished from listbox
    of the first window.

    Also "Unwrap all" button is provided to unwrap all the programs
    that are currently wrapped.

    To configure logging, press button "log".  You'll get the following
    dialog.

    +---+-------------------------------------------------+---+---+---+
    |   | idn wrapper - Log Configuration                 | _ | O | X |
    +---+-------------------------------------------------+---+---+---+
    |    Log Level: o None o Fatal o Error o Warning o Info o Trace   |
    |                                                                 |
    |              +------------------------------------+ +---------+ |
    |     Log File:|                                    | | Browse..| |
    |              +------------------------------------+ +---------+ |
    |               +------+ +--------+                               |
    |Log Operation: | View | | Delete |                               |
    |               +------+ +--------+                               |
    +-----------------------------------------------------------------+
    |                                           +--------+ +--------+ |
    |                                           |   OK   | | Cancel | |
    |                                           +--------+ +--------+ |
    +-----------------------------------------------------------------+

    Logging level can be selected from the followings.
	None	no logging at all
	Fatal   only records fatal errors
	Error	also records non-fatal errors
	Warning	also records warning mssages
	Info	also records informational messages
	Trace	also records trace information
    Note that these levels are for log output from IDN library (idnkit.dll).
    idn wrapper itself supports only off (None) and on (the rest).

    Pathname of the log file can also be specified with this dialog.

    You can view the current log file contents by pressing "View" button,
    or delete it by "Delete" button.

    Note that log level and log file configuration doesn't affect already
    running processes.

    Press "advanced" button to invoke the advanced configuration dialog.
    This dialog is for advanced users and enables customization for
    some basic parameters which normal users need not change, since
    appropriate defaults are provided.

    +---+-------------------------------------------------+---+---+---+
    |   | idn wrapper - Advanced Configuration            | _ | O | X |
    +---+-------------------------------------------------+---+---+---+
    |                    IDN Wrapping Mode                            |
    |  o Wrap both WINSOCK 1.1 and WINSOCK 2.0                        |
    |  o Wrap only WINSOCK 1.1                                        |
    |  o Wrap only WINSOCK 2.0                                        |
    |  o Wrap only WINSOCK 2.0 if it exists.                          |
    |    Otherwise wrap only WINSOCK 1.1                              |
    +-----------------------------------------------------------------+
    |                       IDN Configuration                         |
    |               +--------------------------------+ +----------+   |
    |  Config File: |                                | | Browse.. |   |
    |               +--------------------------------+ +----------+   |
    |               +------+                                          |
    |               | Edit |                                          |
    |               +------+                                          |
    +-----------------------------------------------------------------+
    |                                           +--------+ +--------+ |
    |                                           |   OK   | | Cancel | |
    |                                           +--------+ +--------+ |
    +-----------------------------------------------------------------+

    With the dialog users can do the following configuration.

    Wrapping Mode
	Customize wrapping mode.  Normally the default item should be
	appropriate.  Changing it to other item may help when you
	have problems.

    IDN Configuration
	Set the configuration file for multilingual domain name handling.
	By pressing "Edit" button, you can edit then contents of the file.

4. Limitations

4.1. DLL Versions

    Wrapper DLL is tightly coupled with specific DLL version, because
    it must export all the entries including un-documented ones.
    If WINSOCK DLL version changed, idn wrapper may not work correctly.

    Current idn wrapper is tested on
    
        Win2000         (WINSOCK 1.1 + 2.0)
        WinME           (WINSOCK 1.1 + 2.0)

    But there are no assuarance for future versions of Windows.

4.2. DNS, WINS, LMHOSTS

    There are three name resolving methods in windows, DNS, WINS and
    LMHOSTS. Using idn wrapper, domain name conversion will performed 
    on all of thoses methods.  It may cause some trouble if windows 
    using WINS or LMHOSTS.  We recommend use DNS oly if you want to use
    idn wrapper.

4.3. Converting Names other than Domain Name

    In WINSOCK 2.0, there are generic name resolution APIs are
    introduced.
    
        WSALookupServiceBeginA
	WSALookupServiceNextA
	WSALookupServiceEnd

    They are use mainly domain name conversion now, but not limited to
    resolving domain name.  idn wrapper hooks this API and convert
    given name anyway.  This causes some trouble if conversion name is
    not domain name.

4.4. Applications don't use these APIa

    Some applications don't use these APIs to resolving domain names.
    For example, 'nslookup' issue DNS request locally.  For these
    applications, idn wrapper does not work.

4.5. Applications bound to specific WINSOCK DLL

    Some applications are bound to specific DLL, not relying on
    standard DLL search path. Netscape Communicator seems to be one of
    such programs.  idn wrapper in standard installation cannot wrap
    such programs.
    
    If you want to wrap those programs, you may use installation into
    system directory.  But this installation is very dangerous, for
    it is possible that your system cannot boot again.

5. Registry Setting - Summary

5.1. Priority of Setting

    Settings of idn wrapper is placed on registry 
    
        Software\JPNIC\IDN
	
    under HKEY_LOCAL_MACHINE or HKEY_CURRENT_USER.  idn wrapper first
    read HKEY_LOCAL_MACHINE, and if HKEY_CURRENT_USER exist, overwrite
    with this one.  Usually set HKEY_LOCAL_MACHINE only.  But if you
    need per user setting, then set HKEY_CURRENT_USER.

    Note that the configuration program reads/writes only
    HKEY_LOCAL_MACHINE.

5.2. Registry Key

    There's common settings and per program settings.
    
_Common Settings

	Software\JPNIC\IDN\InstallDir	 Installation directory
        Software\JPNIC\IDN\Where         Where to convert encoding
	                    0: both WINSOCK 1.1 and WINSOCK 2.0
                            1: if WINSOCK 2.0 exist, convert at 2.0 DLL
                               if WINSOCK 1.1 only, convert at 1.1 DLL
			    2: only in WINSOCK1.1
			    3: only in WINSOCK2.0
        Software\JPNIC\IDN\ConfFile	 idnkit Configuration File
        Software\JPNIC\IDN\LogFile       Log File
        Software\JPNIC\IDN\LogLevel      Log Level

_Per Program Settings

    Converting position and program's local encoding may be set per
    program bases.

        Software\JPNIC\IDN\PerProg\<name>\Where
        Software\JPNIC\IDN\PerProg\<name>\Encoding

    If not specified, the following values are assumed.
    
        Where       0 (both 1.1 DLL and 2.0 DLL)
	Encoding    [process's code page]
