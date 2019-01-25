//===-- BPFSubtarget.h - Define Subtarget for the BPF -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the BPF specific subclass of TargetSubtargetInfo.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_BPF_BPFSUBTARGET_H
#define LLVM_LIB_TARGET_BPF_BPFSUBTARGET_H

#include "BPFFrameLowering.h"
#include "BPFISelLowering.h"
#include "BPFInstrInfo.h"
#include "BPFSelectionDAGInfo.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"

#define GET_SUBTARGETINFO_HEADER
#include "BPFGenSubtargetInfo.inc"

namespace llvm {
class StringRef;

class BPFSubtarget : public BPFGenSubtargetInfo {
  virtual void anchor();
  BPFInstrInfo InstrInfo;
  BPFFrameLowering FrameLowering;
  BPFTargetLowering TLInfo;
  BPFSelectionDAGInfo TSInfo;

private:
  void initializeEnvironment();
  void initSubtargetFeatures(StringRef CPU, StringRef FS);
  bool probeJmpExt();

protected:
  // unused
  bool isDummyMode;

  // whether the cpu supports jmp ext
  bool HasJmpExt;

  // whether the cpu supports alu32 instructions.
  bool HasAlu32;

  // whether we should enable MCAsmInfo DwarfUsesRelocationsAcrossSections
  bool UseDwarfRIS;

public:
  // This constructor initializes the data members to match that
  // of the specified triple.
  BPFSubtarget(const Triple &TT, const std::string &CPU, const std::string &FS,
               const TargetMachine &TM);

  BPFSubtarget &initializeSubtargetDependencies(StringRef CPU, StringRef FS);

  // ParseSubtargetFeatures - Parses features string setting specified
  // subtarget options.  Definition of function is auto generated by tblgen.
  void ParseSubtargetFeatures(StringRef CPU, StringRef FS);
  bool getHasJmpExt() const { return HasJmpExt; }
  bool getHasAlu32() const { return HasAlu32; }
  bool getUseDwarfRIS() const { return UseDwarfRIS; }

  const BPFInstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const BPFFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  const BPFTargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const BPFSelectionDAGInfo *getSelectionDAGInfo() const override {
    return &TSInfo;
  }
  const TargetRegisterInfo *getRegisterInfo() const override {
    return &InstrInfo.getRegisterInfo();
  }
};
} // End llvm namespace

#endif