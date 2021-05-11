//===- ARMKagePrivilegePromotion - Move function to privileged code section ==//
//
//         Protecting Control Flow of Real-time OS applications
//
// This file was written by at the University of Rochester.
// All Rights Reserved.
//
//===----------------------------------------------------------------------===//
//
// This pass moves all functions that do not belong to a specific section to a
// privileged code section.
//
//===----------------------------------------------------------------------===//
//

#include "ARMSilhouetteInstrumentor.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

namespace llvm {

  struct ARMKagePrivilegePromotion : public MachineFunctionPass {
    // pass identifier variable
    static char ID;

    const StringRef PrivilegedSectionName = "privileged_functions";

    ARMKagePrivilegePromotion();

    virtual StringRef getPassName() const override;

    virtual bool runOnMachineFunction(MachineFunction & MF) override;
  };

  FunctionPass * createARMKagePrivilegePromotion(void);
}
