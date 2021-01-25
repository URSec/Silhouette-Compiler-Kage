//===-- ARMKageCodeScanner - Check for violations of Kage's requirements --===//
//
//         Protecting Control Flow of Real-time OS applications
//
// All Rights Reserved.
//
//===----------------------------------------------------------------------===//
//
// This pass checks whether the untrusted code contains MSR instruction or
// calls internal trusted functions.
//
//===----------------------------------------------------------------------===//
//

#include "ARM.h"
#include "ARMBaseInstrInfo.h"
#include "ARMSilhouetteConvertFuncList.h"
#include "ARMKageCodeScanner.h"
#include "ARMTargetMachine.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"

#include <deque>

using namespace llvm;

char ARMKageCodeScanner::ID = 0;

static DebugLoc DL;

ARMKageCodeScanner::ARMKageCodeScanner()
    : MachineFunctionPass(ID) {
  return;
}

StringRef
ARMKageCodeScanner::getPassName() const {
  return "ARM Kage Code Scanner Pass";
}

//
// Method: runOnMachineFunction()
//
// Description:
//   This method is called when the PassManager wants this pass to transform
//   the specified MachineFunction.  This method checks whether the code violates
//   any of Kage's requirements. 
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
ARMKageCodeScanner::runOnMachineFunction(MachineFunction & MF) {
#if 1
  // Skip certain functions
  if (funcBlacklist.find(MF.getName()) != funcBlacklist.end()) {
    return false;
  }
  // Skip privileged functions in FreeRTOS
  if (MF.getFunction().getSection().equals("privileged_functions")){
    return false;
  }

  // Skip all HAL Library functions in FreeRTOS
  if (MF.getFunction().getEntryBlock().getModule()->getSourceFileName().find("stm32l475_discovery") != std::string::npos){
    if (MF.getFunction().getEntryBlock().getModule()->getSourceFileName().find("mcu_vendor") != std::string::npos){
      return false;
    }
  }

#endif

  unsigned long OldCodeSize = getFunctionCodeSize(MF);

  std::string target;

  for (MachineBasicBlock & MBB : MF) {
    for (MachineInstr & MI : MBB) {
      switch (MI.getOpcode()) {
        // Throw warning if an MSR instruction is present in untrusted code
        case ARM::t2MSR_M:
        // Also add MSR instructions of other ARM architectures here just in case
        case ARM::t2MSR_AR:
        case ARM::t2MSRbanked:
            errs() << "[Kage] Warning: Illegal privileged instruction found in untrusted function " << MF.getName() << " \n";
            break;
        
        // Branch and Link: Check if the target is an internal trusted function
        case ARM::tBL:
            // Get the target function name
            target = MI.getOperand(0).getSymbolName();
            if (kageInternalPrivFunc.find(target) != kageInternalPrivFunc.end()) {
                errs() << "[Kage] Warning: Calling internal trusted function " << target << " from untrusted function" << MF.getName() << "\n";
            }

        default:
            break;
      }
    }
  }

  unsigned long NewCodeSize = getFunctionCodeSize(MF);

  // Output code size information
  std::error_code EC;
  raw_fd_ostream MemStat("./code_size_ss.stat", EC,
                         sys::fs::OpenFlags::F_Append);
  MemStat << MF.getFunction().getEntryBlock().getModule()->getSourceFileName() << ":" << MF.getName() << ":" << OldCodeSize << ":" << NewCodeSize << "\n";

  return true;
}

//
// Create a new pass.
//
namespace llvm {
  FunctionPass * createARMKageCodeScanner(void) {
    return new ARMKageCodeScanner();
  }
}
