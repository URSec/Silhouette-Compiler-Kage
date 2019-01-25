; RUN: llc %s -o - | FileCheck %s

; This test case was generated by running
;
;   clang++ -O2 -g PR37234.cpp -S -save-temps
;   opt -O2 -S PR37234.bc -o PR37234.ll
;
; using the following PR37234.cpp program
;
;    int main() {
;            const char* buffer = "aaa";
;            unsigned aa = 0;
;            while (char c = *buffer++) {
;                    if (c == 'a')
;                            ++aa;  // DexWatch('aa')
;            }
;            return aa;
;    }


; CHECK-LABEL: # %bb.{{.*}}:
; CHECK:        #DEBUG_VALUE: main:aa <- 0
; CHECK: 	#DEBUG_VALUE: main:aa <- $[[REG:[0-9a-z]+]]
; CHECK: 	jmp	.LBB0_1
; CHECK: .LBB0_2:
; CHECK:        #DEBUG_VALUE: main:aa <- $[[REG]]
; CHECK:        jne     .LBB0_1
; CHECK: # %bb.{{.*}}:
; CHECK:        #DEBUG_VALUE: main:aa <- $[[REG]]
; CHECK:        incl    %[[REG]]
; CHECK:        #DEBUG_VALUE: main:aa <- $[[REG]]
; CHECK: .LBB0_1:
; CHECK: 	#DEBUG_VALUE: main:aa <- $[[REG]]
; CHECK:        jne     .LBB0_2
; CHECK: # %bb.{{.*}}:
; CHECK: 	#DEBUG_VALUE: main:aa <- $[[REG]]
; CHECK: 	retq

source_filename = "PR37234.cpp"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [4 x i8] c"aaa\00", align 1

; Function Attrs: norecurse nounwind readonly uwtable
define dso_local i32 @main() local_unnamed_addr #0 !dbg !7 {
entry:
  call void @llvm.dbg.value(metadata i32 0, metadata !16, metadata !DIExpression()), !dbg !19
  br label %while.cond.outer, !dbg !20

while.cond.outer:                                 ; preds = %if.then, %entry
  %buffer.0.ph = phi i8* [ %incdec.ptr, %if.then ], [ getelementptr inbounds ([4 x i8], [4 x i8]* @.str, i64 0, i64 0), %entry ]
  %aa.0.ph = phi i32 [ %inc, %if.then ], [ 0, %entry ]
  br label %while.cond, !dbg !21

while.cond:                                       ; preds = %while.cond.outer, %while.cond
  %buffer.0 = phi i8* [ %incdec.ptr, %while.cond ], [ %buffer.0.ph, %while.cond.outer ], !dbg !22
  call void @llvm.dbg.value(metadata i32 %aa.0.ph, metadata !16, metadata !DIExpression()), !dbg !19
  call void @llvm.dbg.value(metadata i8* %buffer.0, metadata !12, metadata !DIExpression()), !dbg !23
  %incdec.ptr = getelementptr inbounds i8, i8* %buffer.0, i64 1, !dbg !21
  call void @llvm.dbg.value(metadata i8* %incdec.ptr, metadata !12, metadata !DIExpression()), !dbg !23
  %0 = load i8, i8* %buffer.0, align 1, !dbg !24, !tbaa !25
  call void @llvm.dbg.value(metadata i8 %0, metadata !18, metadata !DIExpression()), !dbg !28
  switch i8 %0, label %while.cond [
    i8 0, label %while.end
    i8 97, label %if.then
  ], !dbg !20

if.then:                                          ; preds = %while.cond
  %inc = add i32 %aa.0.ph, 1, !dbg !29
  call void @llvm.dbg.value(metadata i32 %inc, metadata !16, metadata !DIExpression()), !dbg !19
  br label %while.cond.outer, !dbg !29

while.end:                                        ; preds = %while.cond
  call void @llvm.dbg.value(metadata i32 %aa.0.ph, metadata !16, metadata !DIExpression()), !dbg !19
  call void @llvm.dbg.value(metadata i32 %aa.0.ph, metadata !16, metadata !DIExpression()), !dbg !19
  call void @llvm.dbg.value(metadata i32 %aa.0.ph, metadata !16, metadata !DIExpression()), !dbg !19
  call void @llvm.dbg.value(metadata i32 %aa.0.ph, metadata !16, metadata !DIExpression()), !dbg !19
  ret i32 %aa.0.ph, !dbg !32
}

; Function Attrs: nounwind readnone speculatable
declare void @llvm.dbg.value(metadata, metadata, metadata) #1

attributes #0 = { norecurse nounwind readonly uwtable }
attributes #1 = { nounwind readnone speculatable }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus, file: !1, producer: "clang version 7.0.0 (trunk 330946) (llvm/trunk 330952)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2)
!1 = !DIFile(filename: "PR37234.cpp", directory: "")
!2 = !{}
!3 = !{i32 2, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 7.0.0 (trunk 330946) (llvm/trunk 330952)"}
!7 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 1, type: !8, isLocal: false, isDefinition: true, scopeLine: 1, flags: DIFlagPrototyped, isOptimized: true, unit: !0, retainedNodes: !11)
!8 = !DISubroutineType(types: !9)
!9 = !{!10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !{!12, !16, !18}
!12 = !DILocalVariable(name: "buffer", scope: !7, file: !1, line: 2, type: !13)
!13 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !14, size: 64)
!14 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !15)
!15 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!16 = !DILocalVariable(name: "aa", scope: !7, file: !1, line: 3, type: !17)
!17 = !DIBasicType(name: "unsigned int", size: 32, encoding: DW_ATE_unsigned)
!18 = !DILocalVariable(name: "c", scope: !7, file: !1, line: 4, type: !15)
!19 = !DILocation(line: 3, column: 18, scope: !7)
!20 = !DILocation(line: 4, column: 9, scope: !7)
!21 = !DILocation(line: 4, column: 32, scope: !7)
!22 = !DILocation(line: 0, scope: !7)
!23 = !DILocation(line: 2, column: 21, scope: !7)
!24 = !DILocation(line: 4, column: 25, scope: !7)
!25 = !{!26, !26, i64 0}
!26 = !{!"omnipotent char", !27, i64 0}
!27 = !{!"Simple C++ TBAA"}
!28 = !DILocation(line: 4, column: 21, scope: !7)
!29 = !DILocation(line: 6, column: 25, scope: !30)
!30 = distinct !DILexicalBlock(scope: !31, file: !1, line: 5, column: 21)
!31 = distinct !DILexicalBlock(scope: !7, file: !1, line: 4, column: 36)
!32 = !DILocation(line: 9, column: 1, scope: !7)