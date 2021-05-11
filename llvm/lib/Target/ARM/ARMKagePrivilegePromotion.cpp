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

#include "ARMKagePrivilegePromotion.h"

using namespace llvm;

char ARMKagePrivilegePromotion::ID = 0;

ARMKagePrivilegePromotion::ARMKagePrivilegePromotion()
    : MachineFunctionPass(ID) {
  return;
}

StringRef
ARMKagePrivilegePromotion::getPassName() const {
  return "ARM Kage Privilege Promotion Pass";
}

//
// Method: runOnMachineFunction()
//
// Description:
//   This method is called when the PassManager wants this pass to transform
//   the specified MachineFunction.  This method moves a function to a
//   privileged code section unless the function already has a section
//   specified.
//
// Inputs:
//   MF - A reference to the MachineFunction to transform.
//
// Outputs:
//   MF - The transformed MachineFunction.
//
// Return value:
//   true  - The MachineFunction was transformed.
//   false - The MachineFunction was not transformed.
//
bool
ARMKagePrivilegePromotion::runOnMachineFunction(MachineFunction & MF) {
  Function & F = const_cast<Function &>(MF.getFunction());

  if (MF.size() > 0 && !F.hasSection()) {
    F.setSection(PrivilegedSectionName);
  }

  return false;
}

//
// Create a new pass.
//
namespace llvm {
  FunctionPass * createARMKagePrivilegePromotion(void) {
    return new ARMKagePrivilegePromotion();
  }
}
