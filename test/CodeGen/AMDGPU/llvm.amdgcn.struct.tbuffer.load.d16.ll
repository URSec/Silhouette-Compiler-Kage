; RUN: llc < %s -march=amdgcn -mcpu=tonga -verify-machineinstrs -show-mc-encoding | FileCheck -enable-var-scope -check-prefix=GCN -check-prefix=UNPACKED %s
; RUN: llc < %s -march=amdgcn -mcpu=gfx810 -verify-machineinstrs | FileCheck -enable-var-scope -check-prefix=GCN -check-prefix=PACKED %s
; RUN: llc < %s -march=amdgcn -mcpu=gfx900 -verify-machineinstrs | FileCheck -enable-var-scope -check-prefix=GCN -check-prefix=PACKED %s

; GCN-LABEL: {{^}}tbuffer_load_d16_x:
; GCN: v_mov_b32_e32 [[ZEROREG:v[0-9]+]], 0
; GCN: tbuffer_load_format_d16_x v{{[0-9]+}}, [[ZEROREG]], s[{{[0-9]+:[0-9]+}}],  dfmt:6,  nfmt:1, 0 idxen
define amdgpu_ps half @tbuffer_load_d16_x(<4 x i32> inreg %rsrc) {
main_body:
  %data = call half @llvm.amdgcn.struct.tbuffer.load.f16(<4 x i32> %rsrc, i32 0, i32 0, i32 0, i32 22, i32 0)
  ret half %data
}

; GCN-LABEL: {{^}}tbuffer_load_d16_xy:
; GCN: v_mov_b32_e32 [[ZEROREG:v[0-9]+]], 0
; UNPACKED: tbuffer_load_format_d16_xy v{{\[}}{{[0-9]+}}:[[HI:[0-9]+]]{{\]}}, [[ZEROREG]], s[{{[0-9]+:[0-9]+}}],  dfmt:6,  nfmt:1, 0 idxen
; UNPACKED: v_mov_b32_e32 v{{[0-9]+}}, v[[HI]]

; PACKED: tbuffer_load_format_d16_xy v[[FULL:[0-9]+]], [[ZEROREG]], s[{{[0-9]+:[0-9]+}}],  dfmt:6,  nfmt:1, 0 idxen
; PACKED: v_lshrrev_b32_e32 v{{[0-9]+}}, 16, v[[FULL]]
define amdgpu_ps half @tbuffer_load_d16_xy(<4 x i32> inreg %rsrc) {
main_body:
  %data = call <2 x half> @llvm.amdgcn.struct.tbuffer.load.v2f16(<4 x i32> %rsrc, i32 0, i32 0, i32 0, i32 22, i32 0)
  %elt = extractelement <2 x half> %data, i32 1
  ret half %elt
}

; GCN-LABEL: {{^}}tbuffer_load_d16_xyzw:
; GCN: v_mov_b32_e32 [[ZEROREG:v[0-9]+]], 0
; UNPACKED: tbuffer_load_format_d16_xyzw v{{\[}}{{[0-9]+}}:[[HI:[0-9]+]]{{\]}}, [[ZEROREG]], s[{{[0-9]+:[0-9]+}}],  dfmt:6,  nfmt:1, 0 idxen
; UNPACKED: v_mov_b32_e32 v{{[0-9]+}}, v[[HI]]

; PACKED: tbuffer_load_format_d16_xyzw v{{\[}}{{[0-9]+}}:[[HI:[0-9]+]]{{\]}}, [[ZEROREG]], s[{{[0-9]+:[0-9]+}}],  dfmt:6,  nfmt:1, 0 idxen
; PACKED: v_lshrrev_b32_e32 v{{[0-9]+}}, 16, v[[HI]]
define amdgpu_ps half @tbuffer_load_d16_xyzw(<4 x i32> inreg %rsrc) {
main_body:
  %data = call <4 x half> @llvm.amdgcn.struct.tbuffer.load.v4f16(<4 x i32> %rsrc, i32 0, i32 0, i32 0, i32 22, i32 0)
  %elt = extractelement <4 x half> %data, i32 3
  ret half %elt
}

declare half @llvm.amdgcn.struct.tbuffer.load.f16(<4 x i32>, i32, i32, i32, i32, i32)
declare <2 x half> @llvm.amdgcn.struct.tbuffer.load.v2f16(<4 x i32>, i32, i32, i32, i32, i32)
declare <4 x half> @llvm.amdgcn.struct.tbuffer.load.v4f16(<4 x i32>, i32, i32, i32, i32, i32)
