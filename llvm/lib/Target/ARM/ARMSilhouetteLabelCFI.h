//===-- ARMSilhouetteLabelCFI - Label-Based Forward Control-Flow Integrity ===//
//
//         Protecting Control Flow of Real-time OS applications
//              Copyright (c) 2019-2020, University of Rochester
//
// Part of the Silhouette Project, under the Apache License v2.0 with
// LLVM Exceptions.
// See LICENSE.txt in the top-level directory for license information.
//
//===----------------------------------------------------------------------===//
//
// This pass implements the label-based single-label control-flow integrity for
// forward indirect control-flow transfer instructions on ARM.
//
//===----------------------------------------------------------------------===//
//

#ifndef ARM_SILHOUETTE_LABEL_CFI
#define ARM_SILHOUETTE_LABEL_CFI

#include "ARMSilhouetteInstrumentor.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

namespace llvm {
  struct ARMSilhouetteLabelCFI
      : public MachineFunctionPass, ARMSilhouetteInstrumentor {
    // pass identifier variable
    static char ID;

    // Number of bits of the constant CFI label
    static const uint32_t CFI_LABEL_WIDTH = 32;

    // The constant CFI label for indirect calls (an undefined encoding)
    static const uint32_t CFI_LABEL = 0xf870f871;

    ARMSilhouetteLabelCFI();

    virtual StringRef getPassName() const override;

    virtual bool runOnMachineFunction(MachineFunction & MF) override;

  private:
    void insertCFILabel(MachineFunction & MF);
    void insertCFICheck(MachineInstr & MI, unsigned Reg);
  };

  FunctionPass * createARMSilhouetteLabelCFI(void);
}

#endif
