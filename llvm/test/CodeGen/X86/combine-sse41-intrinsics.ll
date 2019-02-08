; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=x86_64-unknown-unknown -mattr=sse4.1 | FileCheck %s


define <2 x double> @test_x86_sse41_blend_pd(<2 x double> %a0, <2 x double> %a1) {
; CHECK-LABEL: test_x86_sse41_blend_pd:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %1 = call <2 x double> @llvm.x86.sse41.blendpd(<2 x double> %a0, <2 x double> %a1, i32 0)
  ret <2 x double> %1
}

define <4 x float> @test_x86_sse41_blend_ps(<4 x float> %a0, <4 x float> %a1) {
; CHECK-LABEL: test_x86_sse41_blend_ps:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %1 = call <4 x float> @llvm.x86.sse41.blendps(<4 x float> %a0, <4 x float> %a1, i32 0)
  ret <4 x float> %1
}

define <8 x i16> @test_x86_sse41_pblend_w(<8 x i16> %a0, <8 x i16> %a1) {
; CHECK-LABEL: test_x86_sse41_pblend_w:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %1 = call <8 x i16> @llvm.x86.sse41.pblendw(<8 x i16> %a0, <8 x i16> %a1, i32 0)
  ret <8 x i16> %1
}

define <2 x double> @test2_x86_sse41_blend_pd(<2 x double> %a0, <2 x double> %a1) {
; CHECK-LABEL: test2_x86_sse41_blend_pd:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movaps %xmm1, %xmm0
; CHECK-NEXT:    retq
  %1 = call <2 x double> @llvm.x86.sse41.blendpd(<2 x double> %a0, <2 x double> %a1, i32 -1)
  ret <2 x double> %1
}

define <4 x float> @test2_x86_sse41_blend_ps(<4 x float> %a0, <4 x float> %a1) {
; CHECK-LABEL: test2_x86_sse41_blend_ps:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movaps %xmm1, %xmm0
; CHECK-NEXT:    retq
  %1 = call <4 x float> @llvm.x86.sse41.blendps(<4 x float> %a0, <4 x float> %a1, i32 -1)
  ret <4 x float> %1
}

define <8 x i16> @test2_x86_sse41_pblend_w(<8 x i16> %a0, <8 x i16> %a1) {
; CHECK-LABEL: test2_x86_sse41_pblend_w:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movaps %xmm1, %xmm0
; CHECK-NEXT:    retq
  %1 = call <8 x i16> @llvm.x86.sse41.pblendw(<8 x i16> %a0, <8 x i16> %a1, i32 -1)
  ret <8 x i16> %1
}

define <2 x double> @test3_x86_sse41_blend_pd(<2 x double> %a0) {
; CHECK-LABEL: test3_x86_sse41_blend_pd:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %1 = call <2 x double> @llvm.x86.sse41.blendpd(<2 x double> %a0, <2 x double> %a0, i32 7)
  ret <2 x double> %1
}

define <4 x float> @test3_x86_sse41_blend_ps(<4 x float> %a0) {
; CHECK-LABEL: test3_x86_sse41_blend_ps:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %1 = call <4 x float> @llvm.x86.sse41.blendps(<4 x float> %a0, <4 x float> %a0, i32 7)
  ret <4 x float> %1
}

define <8 x i16> @test3_x86_sse41_pblend_w(<8 x i16> %a0) {
; CHECK-LABEL: test3_x86_sse41_pblend_w:
; CHECK:       # %bb.0:
; CHECK-NEXT:    retq
  %1 = call <8 x i16> @llvm.x86.sse41.pblendw(<8 x i16> %a0, <8 x i16> %a0, i32 7)
  ret <8 x i16> %1
}

define double @demandedelts_blendvpd(<2 x double> %a0, <2 x double> %a1, <2 x double> %a2) {
; CHECK-LABEL: demandedelts_blendvpd:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movapd %xmm0, %xmm3
; CHECK-NEXT:    movaps %xmm2, %xmm0
; CHECK-NEXT:    blendvpd %xmm0, %xmm1, %xmm3
; CHECK-NEXT:    movapd %xmm3, %xmm0
; CHECK-NEXT:    retq
  %1 = shufflevector <2 x double> %a0, <2 x double> undef, <2 x i32> zeroinitializer
  %2 = shufflevector <2 x double> %a1, <2 x double> undef, <2 x i32> zeroinitializer
  %3 = shufflevector <2 x double> %a2, <2 x double> undef, <2 x i32> zeroinitializer
  %4 = tail call <2 x double> @llvm.x86.sse41.blendvpd(<2 x double> %1, <2 x double> %2, <2 x double> %3)
  %5 = extractelement <2 x double> %4, i32 0
  ret double %5
}

define float @demandedelts_blendvps(<4 x float> %a0, <4 x float> %a1, <4 x float> %a2) {
; CHECK-LABEL: demandedelts_blendvps:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movaps %xmm0, %xmm3
; CHECK-NEXT:    movaps %xmm2, %xmm0
; CHECK-NEXT:    blendvps %xmm0, %xmm1, %xmm3
; CHECK-NEXT:    movaps %xmm3, %xmm0
; CHECK-NEXT:    retq
  %1 = shufflevector <4 x float> %a0, <4 x float> undef, <4 x i32> zeroinitializer
  %2 = shufflevector <4 x float> %a1, <4 x float> undef, <4 x i32> zeroinitializer
  %3 = shufflevector <4 x float> %a2, <4 x float> undef, <4 x i32> zeroinitializer
  %4 = tail call <4 x float> @llvm.x86.sse41.blendvps(<4 x float> %1, <4 x float> %2, <4 x float> %3)
  %5 = extractelement <4 x float> %4, i32 0
  ret float %5
}

define <16 x i8> @demandedelts_pblendvb(<16 x i8> %a0, <16 x i8> %a1, <16 x i8> %a2) {
; CHECK-LABEL: demandedelts_pblendvb:
; CHECK:       # %bb.0:
; CHECK-NEXT:    movdqa %xmm0, %xmm3
; CHECK-NEXT:    movdqa %xmm2, %xmm0
; CHECK-NEXT:    pblendvb %xmm0, %xmm1, %xmm3
; CHECK-NEXT:    pxor %xmm0, %xmm0
; CHECK-NEXT:    pshufb %xmm0, %xmm3
; CHECK-NEXT:    movdqa %xmm3, %xmm0
; CHECK-NEXT:    retq
  %1 = shufflevector <16 x i8> %a0, <16 x i8> undef, <16 x i32> zeroinitializer
  %2 = shufflevector <16 x i8> %a1, <16 x i8> undef, <16 x i32> zeroinitializer
  %3 = shufflevector <16 x i8> %a2, <16 x i8> undef, <16 x i32> zeroinitializer
  %4 = tail call <16 x i8> @llvm.x86.sse41.pblendvb(<16 x i8> %1, <16 x i8> %2, <16 x i8> %3)
  %5 = shufflevector <16 x i8> %4, <16 x i8> undef, <16 x i32> zeroinitializer
  ret <16 x i8> %5
}

declare <2 x double> @llvm.x86.sse41.blendpd(<2 x double>, <2 x double>, i32)
declare <4 x float> @llvm.x86.sse41.blendps(<4 x float>, <4 x float>, i32)
declare <8 x i16> @llvm.x86.sse41.pblendw(<8 x i16>, <8 x i16>, i32)

declare <2 x double> @llvm.x86.sse41.blendvpd(<2 x double>, <2 x double>, <2 x double>)
declare <4 x float> @llvm.x86.sse41.blendvps(<4 x float>, <4 x float>, <4 x float>)
declare <16 x i8> @llvm.x86.sse41.pblendvb(<16 x i8>, <16 x i8>, <16 x i8>)