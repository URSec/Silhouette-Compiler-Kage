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

ARMKagePrivilegePromotion::ARMKagePrivilegePromotion() : FunctionPass(ID) {
  return;
}

StringRef
ARMKagePrivilegePromotion::getPassName() const {
  return "ARM Kage Privilege Promotion Pass";
}

//
// Method: runOnFunction()
//
// Description:
//   This method is called when the PassManager wants this pass to transform
//   the specified Function.  This method moves a function to a
//   privileged code section unless the function already has a section
//   specified.
//
// Inputs:
//   F - A reference to the Function to transform.
//
// Outputs:
//   F - The transformed Function.
//
// Return value:
//   true - The Function was transformed.
//
bool
ARMKagePrivilegePromotion::runOnFunction(Function & F) {
  if (F.size() > 0 && (!F.hasSection() || F.getSection().startswith(".text"))) {
    F.setSection(PrivilegedSectionName);
  }

  return true;
}

//
// Create a new pass.
//
namespace llvm {
  FunctionPass * createARMKagePrivilegePromotion(void) {
    return new ARMKagePrivilegePromotion();
  }
}
