/*	$NetBSD: tcc.c,v 1.1.1.2 2018/08/16 18:17:47 jmcneill Exp $	*/

/*
 * Test if our calling convention gymnastics actually work
 */

#include <efi.h>
#include <efilib.h>

#if 0
extern void dump_stack(void);
asm(	".globl	dump_stack\n"
	"dump_stack:\n"
	"	movq %rsp, %rdi\n"
	"	jmp *dump_stack_helper@GOTPCREL(%rip)\n"
	".size	dump_stack, .-dump_stack");

void dump_stack_helper(uint64_t rsp_val)
{
	uint64_t *rsp = (uint64_t *)rsp_val;
	int x;

	Print(L"%%rsp: 0x%08x%08x stack:\r\n",
					(rsp_val & 0xffffffff00000000) >>32,
					 rsp_val & 0xffffffff);
	for (x = 0; x < 8; x++) {
		Print(L"%08x: ", ((uint64_t)rsp) & 0xffffffff);
		Print(L"%016x ", *rsp++);
		Print(L"%016x ", *rsp++);
		Print(L"%016x ", *rsp++);
		Print(L"%016x\r\n", *rsp++);
	}
}
#endif

EFI_STATUS EFI_FUNCTION test_failure_callback(void)
{
	return EFI_UNSUPPORTED;
}

EFI_STATUS test_failure(void)
{
	return uefi_call_wrapper(test_failure_callback, 0);
}

EFI_STATUS EFI_FUNCTION test_call0_callback(void)
{
	return EFI_SUCCESS;
}

EFI_STATUS test_call0(void)
{
	return uefi_call_wrapper(test_call0_callback, 0);
}

EFI_STATUS EFI_FUNCTION test_call1_callback(UINT32 a)
{
	if (a != 0x12345678) {
		return EFI_LOAD_ERROR;
	}
	return EFI_SUCCESS;
}

EFI_STATUS test_call1(void)
{
	return uefi_call_wrapper(test_call1_callback, 1,0x12345678);
}

EFI_STATUS EFI_FUNCTION test_call2_callback(UINT32 a, UINT32 b)
{
	if (a != 0x12345678) {
		return EFI_LOAD_ERROR;
	}
	if (b != 0x23456789) {
		return EFI_INVALID_PARAMETER;
	}
	return EFI_SUCCESS;
}

EFI_STATUS test_call2(void)
{
	return uefi_call_wrapper(test_call2_callback, 2,
		0x12345678, 0x23456789);
}

EFI_STATUS EFI_FUNCTION test_call3_callback(UINT32 a, UINT32 b,
	UINT32 c)
{
	if (a != 0x12345678)
		return EFI_LOAD_ERROR;
	if (b != 0x23456789)
		return EFI_INVALID_PARAMETER;
	if (c != 0x3456789a)
		return EFI_UNSUPPORTED;
	return EFI_SUCCESS;
}

EFI_STATUS test_call3(void)
{
	return uefi_call_wrapper(test_call3_callback, 3,
		0x12345678, 0x23456789, 0x3456789a);
}

EFI_STATUS EFI_FUNCTION test_call4_callback(UINT32 a, UINT32 b,
	UINT32 c, UINT32 d)
{
	if (a != 0x12345678)
		return EFI_LOAD_ERROR;
	if (b != 0x23456789)
		return EFI_INVALID_PARAMETER;
	if (c != 0x3456789a)
		return EFI_UNSUPPORTED;
	if (d != 0x456789ab)
		return EFI_BAD_BUFFER_SIZE;

	return EFI_SUCCESS;
}

EFI_STATUS test_call4(void)
{
	return uefi_call_wrapper(test_call4_callback, 4,
		0x12345678, 0x23456789, 0x3456789a, 0x456789ab);
}

EFI_STATUS EFI_FUNCTION test_call5_callback(UINT32 a, UINT32 b,
	UINT32 c, UINT32 d, UINT32 e)
{
	if (a != 0x12345678)
		return EFI_LOAD_ERROR;
	if (b != 0x23456789)
		return EFI_INVALID_PARAMETER;
	if (c != 0x3456789a)
		return EFI_UNSUPPORTED;
	if (d != 0x456789ab)
		return EFI_BAD_BUFFER_SIZE;
	if (e != 0x56789abc)
		return EFI_BUFFER_TOO_SMALL;

	return EFI_SUCCESS;
}

EFI_STATUS test_call5(void)
{
	return uefi_call_wrapper(test_call5_callback, 5,
		0x12345678, 0x23456789, 0x3456789a, 0x456789ab, 0x56789abc);
}

EFI_STATUS EFI_FUNCTION test_call6_callback(UINT32 a, UINT32 b,
	UINT32 c, UINT32 d, UINT32 e, UINT32 f)
{
	if (a != 0x12345678)
		return EFI_LOAD_ERROR;
	if (b != 0x23456789)
		return EFI_INVALID_PARAMETER;
	if (c != 0x3456789a)
		return EFI_UNSUPPORTED;
	if (d != 0x456789ab)
		return EFI_BAD_BUFFER_SIZE;
	if (e != 0x56789abc)
		return EFI_BUFFER_TOO_SMALL;
	if (f != 0x6789abcd)
		return EFI_NOT_READY;

	return EFI_SUCCESS;
}

EFI_STATUS test_call6(void)
{
	return uefi_call_wrapper(test_call6_callback, 6,
		0x12345678, 0x23456789, 0x3456789a, 0x456789ab, 0x56789abc,
		0x6789abcd);
}

EFI_STATUS EFI_FUNCTION test_call7_callback(UINT32 a, UINT32 b,
	UINT32 c, UINT32 d, UINT32 e, UINT32 f, UINT32 g)
{
	if (a != 0x12345678)
		return EFI_LOAD_ERROR;
	if (b != 0x23456789)
		return EFI_INVALID_PARAMETER;
	if (c != 0x3456789a)
		return EFI_UNSUPPORTED;
	if (d != 0x456789ab)
		return EFI_BAD_BUFFER_SIZE;
	if (e != 0x56789abc)
		return EFI_BUFFER_TOO_SMALL;
	if (f != 0x6789abcd)
		return EFI_NOT_READY;
	if (g != 0x789abcde)
		return EFI_DEVICE_ERROR;

	return EFI_SUCCESS;
}

EFI_STATUS test_call7(void)
{
	return uefi_call_wrapper(test_call7_callback, 7,
		0x12345678, 0x23456789, 0x3456789a, 0x456789ab,
		0x56789abc, 0x6789abcd, 0x789abcde);
}

EFI_STATUS EFI_FUNCTION test_call8_callback(UINT32 a, UINT32 b,
	UINT32 c, UINT32 d, UINT32 e, UINT32 f, UINT32 g, UINT32 h)
{
	if (a != 0x12345678)
		return EFI_LOAD_ERROR;
	if (b != 0x23456789)
		return EFI_INVALID_PARAMETER;
	if (c != 0x3456789a)
		return EFI_UNSUPPORTED;
	if (d != 0x456789ab)
		return EFI_BAD_BUFFER_SIZE;
	if (e != 0x56789abc)
		return EFI_BUFFER_TOO_SMALL;
	if (f != 0x6789abcd)
		return EFI_NOT_READY;
	if (g != 0x789abcde)
		return EFI_DEVICE_ERROR;
	if (h != 0x89abcdef)
		return EFI_WRITE_PROTECTED;

	return EFI_SUCCESS;
}

EFI_STATUS test_call8(void)
{
	return uefi_call_wrapper(test_call8_callback, 8,
		0x12345678,
		0x23456789,
		0x3456789a,
		0x456789ab,
		0x56789abc,
		0x6789abcd,
		0x789abcde,
		0x89abcdef);
}

EFI_STATUS EFI_FUNCTION test_call9_callback(UINT32 a, UINT32 b,
	UINT32 c, UINT32 d, UINT32 e, UINT32 f, UINT32 g, UINT32 h, UINT32 i)
{
	if (a != 0x12345678)
		return EFI_LOAD_ERROR;
	if (b != 0x23456789)
		return EFI_INVALID_PARAMETER;
	if (c != 0x3456789a)
		return EFI_UNSUPPORTED;
	if (d != 0x456789ab)
		return EFI_BAD_BUFFER_SIZE;
	if (e != 0x56789abc)
		return EFI_BUFFER_TOO_SMALL;
	if (f != 0x6789abcd)
		return EFI_NOT_READY;
	if (g != 0x789abcde)
		return EFI_DEVICE_ERROR;
	if (h != 0x89abcdef)
		return EFI_WRITE_PROTECTED;
	if (i != 0x9abcdef0)
		return EFI_OUT_OF_RESOURCES;

	return EFI_SUCCESS;
}

EFI_STATUS test_call9(void)
{
	return uefi_call_wrapper(test_call9_callback, 9,
		0x12345678,
		0x23456789,
		0x3456789a,
		0x456789ab,
		0x56789abc,
		0x6789abcd,
		0x789abcde,
		0x89abcdef,
		0x9abcdef0);
}

extern EFI_STATUS test_call10(void);
EFI_STATUS EFI_FUNCTION test_call10_callback(UINT32 a, UINT32 b,
	UINT32 c, UINT32 d, UINT32 e, UINT32 f, UINT32 g, UINT32 h, UINT32 i,
	UINT32 j)
{
	if (a != 0x12345678)
		return EFI_LOAD_ERROR;
	if (b != 0x23456789)
		return EFI_INVALID_PARAMETER;
	if (c != 0x3456789a)
		return EFI_UNSUPPORTED;
	if (d != 0x456789ab)
		return EFI_BAD_BUFFER_SIZE;
	if (e != 0x56789abc)
		return EFI_BUFFER_TOO_SMALL;
	if (f != 0x6789abcd)
		return EFI_NOT_READY;
	if (g != 0x789abcde)
		return EFI_DEVICE_ERROR;
	if (h != 0x89abcdef)
		return EFI_WRITE_PROTECTED;
	if (i != 0x9abcdef0)
		return EFI_OUT_OF_RESOURCES;
	if (j != 0xabcdef01)
		return EFI_VOLUME_CORRUPTED;

	return EFI_SUCCESS;
}

EFI_STATUS test_call10(void)
{
	return uefi_call_wrapper(test_call10_callback, 10,
		0x12345678,
		0x23456789,
		0x3456789a,
		0x456789ab,
		0x56789abc,
		0x6789abcd,
		0x789abcde,
		0x89abcdef,
		0x9abcdef0,
		0xabcdef01);
}

EFI_STATUS
efi_main (EFI_HANDLE *image, EFI_SYSTEM_TABLE *systab)
{
	EFI_STATUS rc = EFI_SUCCESS;

	InitializeLib(image, systab);
	PoolAllocationType = 2; /* klooj */

#ifdef __x86_64__
	__asm__ volatile("out %0,%1" : : "a" ((uint8_t)0x14), "dN" (0x80));
#endif

	Print(L"Hello\r\n");
	rc = test_failure();
	if (EFI_ERROR(rc)) {
		Print(L"Returning Failure works\n");
	} else {
		Print(L"Returning failure doesn't work.\r\n");
		Print(L"%%rax was 0x%016x, should have been 0x%016x\n",
			rc, EFI_UNSUPPORTED);
		return EFI_INVALID_PARAMETER;
	}

	rc = test_call0();
	if (!EFI_ERROR(rc)) {
		Print(L"0 args works just fine here.\r\n");
	} else {
		Print(L"0 args failed: 0x%016x\n", rc);
		return rc;
	}

	rc = test_call1();
	if (!EFI_ERROR(rc)) {
		Print(L"1 arg works just fine here.\r\n");
	} else {
		Print(L"1 arg failed: 0x%016x\n", rc);
		return rc;
	}

	rc = test_call2();
	if (!EFI_ERROR(rc)) {
		Print(L"2 args works just fine here.\r\n");
	} else {
		Print(L"2 args failed: 0x%016x\n", rc);
		return rc;
	}

	rc = test_call3();
	if (!EFI_ERROR(rc)) {
		Print(L"3 args works just fine here.\r\n");
	} else {
		Print(L"3 args failed: 0x%016x\n", rc);
		return rc;
	}

	rc = test_call4();
	if (!EFI_ERROR(rc)) {
		Print(L"4 args works just fine here.\r\n");
	} else {
		Print(L"4 args failed: 0x%016x\n", rc);
		return rc;
	}

	rc = test_call5();
	if (!EFI_ERROR(rc)) {
		Print(L"5 args works just fine here.\r\n");
	} else {
		Print(L"5 args failed: 0x%016x\n", rc);
		return rc;
	}

	rc = test_call6();
	if (!EFI_ERROR(rc)) {
		Print(L"6 args works just fine here.\r\n");
	} else {
		Print(L"6 args failed: 0x%016x\n", rc);
		return rc;
	}

	rc = test_call7();
	if (!EFI_ERROR(rc)) {
		Print(L"7 args works just fine here.\r\n");
	} else {
		Print(L"7 args failed: 0x%016x\n", rc);
		return rc;
	}

	rc = test_call8();
	if (!EFI_ERROR(rc)) {
		Print(L"8 args works just fine here.\r\n");
	} else {
		Print(L"8 args failed: 0x%016x\n", rc);
		return rc;
	}

	rc = test_call9();
	if (!EFI_ERROR(rc)) {
		Print(L"9 args works just fine here.\r\n");
	} else {
		Print(L"9 args failed: 0x%016x\n", rc);
		return rc;
	}

	rc = test_call10();
	if (!EFI_ERROR(rc)) {
		Print(L"10 args works just fine here.\r\n");
	} else {
		Print(L"10 args failed: 0x%016x\n", rc);
		return rc;
	}

	return rc;
}
