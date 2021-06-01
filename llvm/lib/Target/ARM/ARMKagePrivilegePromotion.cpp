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

extern bool KagePrivilegePromotion;

char ARMKagePrivilegePromotion::ID = 0;

ARMKagePrivilegePromotion::ARMKagePrivilegePromotion() : ModulePass(ID) {
  return;
}

StringRef
ARMKagePrivilegePromotion::getPassName() const {
  return "ARM Kage Privilege Promotion Pass";
}

//
// Method: runOnModule()
//
// Description:
//   This method is called when the PassManager wants this pass to transform
//   the specified Module.  This method moves each function to a
//   privileged code section unless the function already has a section
//   specified.
//
// Inputs:
//   M - A reference to the Module to transform.
//
// Outputs:
//   M - The transformed Module.
//
// Return value:
//   true  - The Module was transformed.
//   false - The Module was not transformed.
//
bool
ARMKagePrivilegePromotion::runOnModule(Module & M) {
  bool changed = false;

  // Move all the functions to the privileged code section
  for (Function & F : M) {
    if (KagePrivilegePromotion) {
      if (!F.hasSection() || F.getSection().startswith(".text")) {
        F.setSection(PrivilegedCodeSectionName);
        changed |= true;
      }
    }
  }

  // To work with --gc-sections option, append the object name to the
  // privileged section name
  for (Function & F : M) {
    if (F.hasSection() && F.getSection() == PrivilegedCodeSectionName) {
      Twine T(PrivilegedCodeSectionName);
      F.setSection(T.concat(".").concat(F.getName()).str());
      changed |= true;
    }
  }
  for (GlobalVariable & GV : M.globals()) {
    if (GV.hasSection() && GV.getSection() == PrivilegedDataSectionName) {
      Twine T(PrivilegedDataSectionName);
      GV.setSection(T.concat(".").concat(GV.getName()).str());
      changed |= true;
    }
  }

  return changed;
}

//
// Create a new pass.
//
namespace llvm {
  ModulePass * createARMKagePrivilegePromotion(void) {
    return new ARMKagePrivilegePromotion();
  }
}
