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

#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

namespace llvm {

  struct ARMKagePrivilegePromotion : public ModulePass {
    // pass identifier variable
    static char ID;

    static constexpr char const * PrivilegedCodeSectionName = "privileged_functions";
    static constexpr char const * PrivilegedDataSectionName = "privileged_data";

    ARMKagePrivilegePromotion();

    virtual StringRef getPassName() const override;

    virtual bool runOnModule(Module & M) override;
  };

  ModulePass * createARMKagePrivilegePromotion(void);
}
