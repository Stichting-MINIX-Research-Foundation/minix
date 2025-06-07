# meson.build - Complete kernel build configuration for MINIX
project('minix-kernel', 'c',
    version: '3.4.0',
    license: 'BSD', # Assuming BSD, adjust if different
    default_options: [
        'c_std=c11',
        'warning_level=2', # Level 3 can be very noisy initially
        'b_staticpic=false', # Position Independent Code for static libs, not usually for kernel
        'b_pie=false',       # Position Independent Executable, no for kernel
        'b_lto=false',       # Link Time Optimization, disable for kernel dev
        # 'buildtype=debugoptimized' # Common for development
    ]
)

# Compiler instance
cc = meson.get_compiler('c')

# Detect architecture
arch = host_machine.cpu_family()
if arch == 'x86_64'
    arch_subdir = 'x86_64'
    arch_defines = ['-D__x86_64__', '-DARCH_X86_64']
    # Flags for x86_64 kernel
    arch_flags = ['-m64', '-mcmodel=kernel', '-mno-red-zone',
                  '-mno-sse', '-mno-sse2', '-mno-mmx', '-mno-3dnow', # Minimal features
                  '-fno-omit-frame-pointer', '-fno-optimize-sibling-calls']
elif arch == 'x86'
    arch_subdir = 'i386'
    arch_defines = ['-D__i386__', '-DARCH_I386']
    # Flags for i386 kernel
    arch_flags = ['-m32', '-fno-omit-frame-pointer', '-fno-optimize-sibling-calls']
else
    error('Unsupported architecture: ' + arch + '. Supported: x86, x86_64')
endif

# Kernel-specific compiler flags (common)
kernel_common_flags = [
    '-ffreestanding',      # No standard library
    '-fno-builtin',        # No built-in functions (e.g. printf, memcpy)
    '-fno-stack-protector',# No stack canary
    '-nostdinc',           # No standard include dirs
    '-nostdlib',           # No standard startup files or libraries
    '-Wall',
    '-Wextra',
    '-Wno-unused-parameter',
    '-Wno-missing-field-initializers',
    # '-g', # Debug symbols
]

# Combine common kernel flags with arch-specific flags and defines
kernel_c_args = kernel_common_flags + arch_flags + arch_defines
kernel_link_args = arch_flags # Linker also needs basic arch flags

# Include directories (relative to this meson.build file, i.e., minix/kernel/)
kernel_includes = include_directories(
    'include',                     # For <minix/kernel_types.h> etc.
    'include/arch/' + arch_subdir, # For arch-specific headers under include/arch/
    'klib/include',                # For our klib headers (kstring.h, etc.)
    '.'                            # Current directory, for top-level kernel headers
)

# Kernel library (klib)
klib_sources = files(
    'klib/kstring.c',
    'klib/kmemory.c',
    'klib/kprintf_stub.c',
    'klib/kpanic.c'
    # Add other klib source files here if created (e.g. katom.c)
)

klib = static_library('klib',
    klib_sources,
    c_args: kernel_c_args,
    include_directories: kernel_includes,
    install: false
)

# Main kernel sources (core)
# Note: main.c, utility.c were refactored.
# system.c is the top-level one, not the directory.
kernel_core_sources = files(
    'clock.c',
    'cpulocals.c',
    'debug.c',
    'interrupt.c',
    'main.c',
    'proc.c',
    'profile.c',
    'smp.c',
    'system.c', # Top-level system.c
    'table.c',
    'usermapped_data.c',
    'utility.c',
    'watchdog.c'
)

# System call handler sources (from minix/kernel/system/)
# This assumes all .c files in 'system/' are syscall handlers.
# A more explicit list might be safer if there are non-handler .c files.
syscall_sources = []
syscall_files_find_result = run_command('find', 'system', '-maxdepth', '1', '-name', '*.c', '-print', check: true)
syscall_files_stdout = syscall_files_find_result.stdout().strip()
if syscall_files_stdout != ''
    syscall_files = syscall_files_stdout.split('\n')
    foreach f : syscall_files
        if f != ''
            syscall_sources += files(f)
        endif
    endforeach
endif

# Architecture-specific sources
# This needs to be robust if arch_subdir is empty or files are missing.
arch_sources_list = []
# Example for i386, must be adapted if file structure differs or for x86_64
if arch_subdir == 'i386'
  arch_sources_list = files(
    'arch/i386/acpi.c',
    'arch/i386/apic.c',
    # 'arch/i386/apic_asm.S', # Assembler files need separate handling
    'arch/i386/arch_clock.c',
    'arch/i386/arch_do_vmctl.c',
    'arch/i386/arch_reset.c',
    'arch/i386/arch_smp.c',
    'arch/i386/arch_system.c',
    'arch/i386/arch_watchdog.c',
    'arch/i386/breakpoints.c',
    # 'arch/i386/debugreg.S',
    'arch/i386/direct_tty_utils.c',
    'arch/i386/do_iopenable.c',
    'arch/i386/do_readbios.c',
    'arch/i386/do_sdevio.c',
    'arch/i386/exception.c',
    # 'arch/i386/head.S',
    'arch/i386/i8259.c',
    # 'arch/i386/io_inb.S', ... other .S files
    # 'arch/i386/klib.S',
    'arch/i386/memory.c',
    # 'arch/i386/mpx.S',
    'arch/i386/oxpcie.c',
    'arch/i386/pg_utils.c',
    'arch/i386/pre_init.c',
    'arch/i386/protect.c',
    # 'arch/i386/trampoline.S',
    'arch/i386/usermapped_data_arch.c'
    # 'arch/i386/usermapped_glo_ipc.S'
  )
elif arch_subdir == 'x86_64'
  # Add x86_64 specific C sources here when available
  # For now, it might be an empty list or share some i386 code if compatible
  # and if separate files don't exist.
  # The issue report's old meson.build listed many x86_64 files that were
  # copies of i386 paths. This needs to be based on actual existing files.
  # If arch/x86_64 is empty of C files, this list will be empty.
  noop_x86_64 = [] # Placeholder
endif

# Define architecture-specific assembly sources
kernel_asm_sources = []
if arch == 'i386'
    kernel_asm_sources = files(
        'arch/i386/mpx.S',
        'arch/i386/head.S',
        'arch/i386/klib.S',
        'arch/i386/apic_asm.S'
        # Add other essential .S files here if identified later e.g.
        # 'arch/i386/debugreg.S',
        # 'arch/i386/io_inb.S', 'arch/i386/io_inl.S', 'arch/i386/io_intr.S', 'arch/i386/io_inw.S',
        # 'arch/i386/io_outb.S', 'arch/i386/io_outl.S', 'arch/i386/io_outw.S',
        # 'arch/i386/trampoline.S',
        # 'arch/i386/usermapped_glo_ipc.S'
        # For now, using the minimal list from feedback.
    )
elif arch == 'x86_64'
    # Assuming these files do not exist yet based on previous 'ls' output.
    # If they are added, they can be listed here.
    # kernel_asm_sources = files(
    #     'arch/x86_64/mpx.S',
    #     'arch/x86_64/head.S',
    #     'arch/x86_64/klib.S'
    # )
    kernel_asm_sources = [] # Empty list for x86_64 if files are not present
endif

# Need to handle Assembly files separately and link them.
# For now, focusing on C files. This will be a FIXME for the build.

all_c_sources = kernel_core_sources + syscall_sources + arch_sources_list

# Linker script
linker_script = meson.current_source_dir() / 'arch' / arch_subdir / 'kernel.lds'
if not run_command('test', '-f', linker_script, check: false).returncode() == 0
    error('Linker script not found: ' + linker_script +
          '. Please ensure it exists for architecture ' + arch_subdir)
endif

kernel_link_args += ['-T', linker_script, '-Wl,--build-id=none']

# Build kernel executable
kernel = executable('kernel',
    all_c_sources + kernel_asm_sources,
    link_with: [klib],
    link_args: kernel_link_args,
    c_args: kernel_c_args,
    include_directories: kernel_includes,
    install: true,
    install_dir: 'boot' # Or appropriate install path
)

# Output some information
message('Building MINIX kernel for ' + arch)
message('Kernel C_ARGS: ' + ' '.join(kernel_c_args))
message('Kernel LINK_ARGS: ' + ' '.join(kernel_link_args))
message('Linker script: ' + linker_script)

# TODO: Add handling for assembly files (.S)
# TODO: Add custom targets for things like procoffsets.cf if needed
# TODO: Define what to do if x86_64 sources are missing (e.g. error or allow empty build)
