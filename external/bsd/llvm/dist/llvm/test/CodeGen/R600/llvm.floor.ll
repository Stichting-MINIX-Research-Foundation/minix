; RUN: llc < %s -march=r600 -mcpu=redwood | FileCheck %s --check-prefix=R600-CHECK
; RUN: llc < %s -march=amdgcn -mcpu=SI -verify-machineinstrs | FileCheck %s --check-prefix=SI-CHECK

; R600-CHECK: {{^}}f32:
; R600-CHECK: FLOOR
; SI-CHECK: {{^}}f32:
; SI-CHECK: v_floor_f32_e32
define void @f32(float addrspace(1)* %out, float %in) {
entry:
  %0 = call float @llvm.floor.f32(float %in)
  store float %0, float addrspace(1)* %out
  ret void
}

; R600-CHECK: {{^}}v2f32:
; R600-CHECK: FLOOR
; R600-CHECK: FLOOR
; SI-CHECK: {{^}}v2f32:
; SI-CHECK: v_floor_f32_e32
; SI-CHECK: v_floor_f32_e32
define void @v2f32(<2 x float> addrspace(1)* %out, <2 x float> %in) {
entry:
  %0 = call <2 x float> @llvm.floor.v2f32(<2 x float> %in)
  store <2 x float> %0, <2 x float> addrspace(1)* %out
  ret void
}

; R600-CHECK: {{^}}v4f32:
; R600-CHECK: FLOOR
; R600-CHECK: FLOOR
; R600-CHECK: FLOOR
; R600-CHECK: FLOOR
; SI-CHECK: {{^}}v4f32:
; SI-CHECK: v_floor_f32_e32
; SI-CHECK: v_floor_f32_e32
; SI-CHECK: v_floor_f32_e32
; SI-CHECK: v_floor_f32_e32
define void @v4f32(<4 x float> addrspace(1)* %out, <4 x float> %in) {
entry:
  %0 = call <4 x float> @llvm.floor.v4f32(<4 x float> %in)
  store <4 x float> %0, <4 x float> addrspace(1)* %out
  ret void
}

; Function Attrs: nounwind readonly
declare float @llvm.floor.f32(float) #0

; Function Attrs: nounwind readonly
declare <2 x float> @llvm.floor.v2f32(<2 x float>) #0

; Function Attrs: nounwind readonly
declare <4 x float> @llvm.floor.v4f32(<4 x float>) #0

attributes #0 = { nounwind readonly }
