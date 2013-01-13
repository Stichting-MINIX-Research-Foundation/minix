#include "inc.h"
#include "../../kernel/arch/i386/include/archconst.h"

#ifndef CONFIG_MAX_CPUS
#define CONFIG_MAX_CPUS	1
#endif

static const char * x86_flag[] = {
	"fpu",
	"vme",
	"de",
	"pse",
	"tsc",
	"msr",
	"pae",
	"mce",
	"cx8",
	"apic",
	"",
	"sep",
	"mtrr",
	"pge",
	"mca",
	"cmov",
	"pat",
	"pse36",
	"psn",
	"clfsh",
	"",
	"dts",
	"acpi",
	"mmx",
	"fxsr",
	"sse",
	"sse2",
	"ss",
	"ht",
	"tm",
	"",
	"pbe",
	"pni",
	"",
	"",
	"monitor",
	"ds_cpl",
	"vmx",
	"smx",
	"est",
	"tm2",
	"ssse3",
	"cid",
	"",
	"",
	"cx16",
	"xtpr",
	"pdcm",
	"",
	"",
	"dca",
	"sse4_1",
	"sse4_2",
	"x2apic",
	"movbe",
	"popcnt",
	"",
	"",
	"xsave",
	"osxsave",
	"",
	"",
	"",
	"",
};

static void print_cpu_flags(u32_t * flags)
{
	int i, j;

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 32; j++) {
			if (flags[i] & (1 << j) &&
					x86_flag[i * 32 + j][0])
				buf_printf("%s ", x86_flag[i * 32 + j]);
		}
	}
	buf_printf("\n");
}

static void print_cpu(struct cpu_info * cpu_info, unsigned id)
{
	buf_printf("%-16s: %d\n", "processor", id);

#if defined(__i386__)
	switch (cpu_info->vendor) {
		case CPU_VENDOR_INTEL:
			buf_printf("%-16s: %s\n", "vendor_id", "GenuineIntel");
			buf_printf("%-16s: %s\n", "model name", "Intel");
			break;
		case CPU_VENDOR_AMD:
			buf_printf("%-16s: %s\n", "vendor_id", "AuthenticAMD");
			buf_printf("%-16s: %s\n", "model name", "AMD");
			break;
		default:
			buf_printf("%-16: %s\n", "vendor_id", "unknown");
	}

	buf_printf("%-16s: %d\n", "cpu family", cpu_info->family);
	buf_printf("%-16s: %d\n", "model", cpu_info->model);
	buf_printf("%-16s: %d\n", "stepping", cpu_info->stepping);
	buf_printf("%-16s: %d\n", "cpu MHz", cpu_info->freq);
	buf_printf("%-16s: ", "flags");
	print_cpu_flags(cpu_info->flags);
	buf_printf("\n");
#endif
}

void root_cpuinfo(void)
{
	struct cpu_info cpu_info[CONFIG_MAX_CPUS];
	struct machine machine;
	unsigned c;

	if (sys_getmachine(&machine)) {
		printf("PROCFS: cannot get machine\n");
		return;
	}
	if (sys_getcpuinfo(&cpu_info)) {
		printf("PROCFS: cannot get cpu info\n");
		return;
	}

	for (c = 0; c < machine.processors_count; c++)
		print_cpu(&cpu_info[c], c);
}
