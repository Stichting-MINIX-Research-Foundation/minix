; RUN: llc -march=amdgcn -mcpu=SI -verify-machineinstrs < %s | FileCheck -check-prefix=SI %s
; RUN: llc -march=amdgcn -mcpu=tonga -verify-machineinstrs < %s | FileCheck -check-prefix=SI %s

declare double @llvm.maxnum.f64(double, double) nounwind readnone

; SI-LABEL: {{^}}test_fmax3_f64:
; SI-DAG: buffer_load_dwordx2 [[REGA:v\[[0-9]+:[0-9]+\]]], s[{{[0-9]+:[0-9]+}}], 0{{$}}
; SI-DAG: buffer_load_dwordx2 [[REGB:v\[[0-9]+:[0-9]+\]]], s[{{[0-9]+:[0-9]+}}], 0 offset:8
; SI-DAG: buffer_load_dwordx2 [[REGC:v\[[0-9]+:[0-9]+\]]], s[{{[0-9]+:[0-9]+}}], 0 offset:16
; SI: v_max_f64 [[REGA]], [[REGA]], [[REGB]]
; SI: v_max_f64 [[RESULT:v\[[0-9]+:[0-9]+\]]], [[REGA]], [[REGC]]
; SI: buffer_store_dwordx2 [[RESULT]],
; SI: s_endpgm
define void @test_fmax3_f64(double addrspace(1)* %out, double addrspace(1)* %aptr) nounwind {
  %bptr = getelementptr double addrspace(1)* %aptr, i32 1
  %cptr = getelementptr double addrspace(1)* %aptr, i32 2
  %a = load double addrspace(1)* %aptr, align 8
  %b = load double addrspace(1)* %bptr, align 8
  %c = load double addrspace(1)* %cptr, align 8
  %f0 = call double @llvm.maxnum.f64(double %a, double %b) nounwind readnone
  %f1 = call double @llvm.maxnum.f64(double %f0, double %c) nounwind readnone
  store double %f1, double addrspace(1)* %out, align 8
  ret void
}
