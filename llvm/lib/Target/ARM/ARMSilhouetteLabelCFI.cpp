//===-- ARMSilhouetteLabelCFI - Label-Based Forward Control-Flow Integrity ===//
//
//         Protecting Control Flow of Real-time OS applications
//
// This file was written by at the University of Rochester.
// All Rights Reserved.
//
//===----------------------------------------------------------------------===//
//
// This pass implements the label-based single-label control-flow integrity for
// forward indirect control-flow transfer instructions on ARM.
//
//===----------------------------------------------------------------------===//
//

#include "ARM.h"
#include "ARMSilhouetteConvertFuncList.h"
#include "ARMSilhouetteLabelCFI.h"
#include "ARMTargetMachine.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"

#include <vector>

using namespace llvm;

extern bool SilhouetteInvert;
extern bool SilhouetteStr2Strt;

static DebugLoc DL;

char ARMSilhouetteLabelCFI::ID = 0;

ARMSilhouetteLabelCFI::ARMSilhouetteLabelCFI()
    : MachineFunctionPass(ID) {
}

StringRef
ARMSilhouetteLabelCFI::getPassName() const {
  return "ARM Silhouette Label-Based Forward CFI Pass";
}

//
// Function: BackupReisters()
//
// Description:
//   This function inserts instructions that store the content of two LO
//   registers (R0 -- R7) onto the stack.
//
// Inputs:
//   MI   - A reference to the instruction before which to insert instructions.
//   Reg  - The first register to spill.
//   Reg2 - The second register to spill.
//
static void
BackupRegisters(MachineInstr & MI, unsigned Reg, unsigned Reg2) {
  MachineBasicBlock & MBB = *MI.getParent();
  const TargetInstrInfo * TII = MBB.getParent()->getSubtarget().getInstrInfo();

  if (SilhouetteInvert || !SilhouetteStr2Strt) {
    // Build a PUSH
    BuildMI(MBB, &MI, DL, TII->get(ARM::tPUSH))
    .add(predOps(ARMCC::AL))
    .addReg(Reg)
    .addReg(Reg2);
  } else {
    //
    // Build the following instruction sequence:
    //
    // sub  sp, #8
    // strt reg, [sp, #0]
    // strt reg2, [sp, #4]
    //
    BuildMI(MBB, &MI, DL, TII->get(ARM::tSUBspi), ARM::SP)
    .addReg(ARM::SP)
    .addImm(2)
    .add(predOps(ARMCC::AL));
    BuildMI(MBB, &MI, DL, TII->get(ARM::t2STRT))
    .addReg(Reg)
    .addReg(ARM::SP)
    .addImm(0)
    .add(predOps(ARMCC::AL));
    BuildMI(MBB, &MI, DL, TII->get(ARM::t2STRT))
    .addReg(Reg2)
    .addReg(ARM::SP)
    .addImm(4)
    .add(predOps(ARMCC::AL));
  }
}

//
// Function: RestoreRegisters()
//
// Description:
//   This function inserts instructions that load the content of two LO
//   registers (R0 -- R7) from the stack.
//
// Inputs:
//   MI   - A reference to the instruction before which to insert instructions.
//   Reg  - The first register to restore.
//   Reg2 - The second register to restore.
//
static void
RestoreRegisters(MachineInstr & MI, unsigned Reg, unsigned Reg2) {
  MachineBasicBlock & MBB = *MI.getParent();
  const TargetInstrInfo * TII = MBB.getParent()->getSubtarget().getInstrInfo();

  // Generate a POP that pops out the register content from stack
  BuildMI(MBB, &MI, DL, TII->get(ARM::tPOP))
  .add(predOps(ARMCC::AL))
  .addReg(Reg)
  .addReg(Reg2);
}

//
// Method: insertCFILabel()
//
// Description:
//   This method inserts the CFI label for call before a machine function.
//
// Input:
//   MF - A reference to the machine function.
//
void
ARMSilhouetteLabelCFI::insertCFILabel(MachineFunction & MF) {
  Function & F = const_cast<Function &>(MF.getFunction());

  if (F.hasPrefixData()) {
    errs() << "[CFI] Override existing prefix data of @" << F.getName() << "\n";
  }
  F.setPrefixData(ConstantInt::get(IntegerType::get(F.getContext(),
                                                    CFI_LABEL_WIDTH),
                                   CFI_LABEL));

  // Make sure the function is at least label-width aligned to avoid unaligned
  // access when loading the label
  MF.ensureAlignment(CFI_LABEL_WIDTH / 8);
}

//
// Method: insertCFICheck()
//
// Description:
//   This method inserts a CFI check before a specified indirect forward
//   control-flow transfer instruction that jumps to a target in a register.
//
// Inputs:
//   MI    - A reference to the indirect forward control-flow transfer
//           instruction.
//   Reg   - The register used by @MI.
//
void
ARMSilhouetteLabelCFI::insertCFICheck(MachineInstr & MI, unsigned Reg) {
  MachineBasicBlock & MBB = *MI.getParent();
  const TargetInstrInfo * TII = MBB.getParent()->getSubtarget().getInstrInfo();

  //
  // Try to find a free register first.  If we are unlucky, spill and (later)
  // restore R4.
  //
  unsigned ScratchReg;
  unsigned ScratchReg2;
  std::deque<unsigned> FreeRegs = findFreeRegisters(MI);
  if (FreeRegs.size() >= 2) {
    ScratchReg = FreeRegs[0];
    ScratchReg2 = FreeRegs[1];
  } else {
    errs() << "[CFI] Unable to find free registers for " << MI;
    ScratchReg = Reg == ARM::R4 ? ARM::R5 : ARM::R4;
    ScratchReg2 = Reg == ARM::R6 ? ARM::R7 : ARM::R6;
    BackupRegisters(MI, ScratchReg, ScratchReg2);
  }

  //
  // Build the following instruction sequence:
  //
  // ldrh  scratch, [reg, #-4 or #-5]
  // movw  scratch2, #CFL_LABEL:lo16
  // movt  scratch2, #CFL_LABEL:hi16
  // cmp   scratch, scratch2
  // it    ne
  // orrne reg, reg, #0xffffffff
  //

  // Load the target CFI label to @ScratchReg
  BuildMI(MBB, &MI, DL, TII->get(ARM::t2LDRHi8), ScratchReg)
  .addReg(Reg)
  .addImm(MI.getOpcode() == ARM::tBRIND ? -4 : -5)
  .add(predOps(ARMCC::AL));
  // Load the correct CFI label to @ScratchReg2
  BuildMI(MBB, &MI, DL, TII->get(ARM::t2MOVi16), ScratchReg)
  .addImm(CFI_LABEL & 0xffff)
  .add(predOps(ARMCC::AL));
  BuildMI(MBB, &MI, DL, TII->get(ARM::t2MOVTi16), ScratchReg)
  .addReg(ScratchReg)
  .addImm(CFI_LABEL >> 16)
  .add(predOps(ARMCC::AL));
  // Compare the target label with the correct label
  BuildMI(MBB, &MI, DL, TII->get(ARM::t2CMPrr))
  .addReg(ScratchReg)
  .addReg(ScratchReg2)
  .add(predOps(ARMCC::AL));
  // Set all the bits of @Reg if two labels are not equal (a CFI violation)
  BuildMI(MBB, &MI, DL, TII->get(ARM::t2IT))
  .addImm(ARMCC::NE)
  .addImm(0x8);
  BuildMI(MBB, &MI, DL, TII->get(ARM::t2ORRri), Reg)
  .addReg(Reg)
  .addImm(0xffffffff)
  .add(predOps(ARMCC::NE, ARM::CPSR))
  .add(condCodeOp());

  // Restore the scratch register if we spilled it
  if (FreeRegs.size() < 2) {
    RestoreRegisters(MI, ScratchReg, ScratchReg2);
  }
}

//
// Method: runOnMachineFunction()
//
// Description:
//   This method is called when the PassManager wants this pass to transform
//   the specified MachineFunction.  This method .
//
// Input:
//   MF - A reference to the MachineFunction to transform.
//
// Output:
//   MF - The transformed MachineFunction.
//
// Return value:
//   true  - The MachineFunction was transformed.
//   false - The MachineFunction was not transformed.
//
bool
ARMSilhouetteLabelCFI::runOnMachineFunction(MachineFunction & MF) {
#if 1
  // Skip certain functions
  if (funcBlacklist.find(MF.getName()) != funcBlacklist.end()) {
    return false;
  }
  // Skip privileged functions in FreeRTOS
  if (MF.getFunction().getSection().equals("privileged_functions")){
    errs() << "Privileged function: " << MF.getName() << "\n";
    return false;
  }

  // Skip all HAL Library functions in FreeRTOS
  if (MF.getFunction().getEntryBlock().getModule()->getSourceFileName().find("stm32l475_discovery") != std::string::npos){
    if (MF.getFunction().getEntryBlock().getModule()->getSourceFileName().find("mcu_vendor") != std::string::npos){
      errs() << "HAL Library Function: " << MF.getName() << " From " << MF.getFunction().getEntryBlock().getModule()->getSourceFileName() << " \n";
      return false;
    }
  }
  errs() << "Transforming " << MF.getName() << " From " << MF.getFunction().getEntryBlock().getModule()->getSourceFileName() << "\r\n";
#endif

  unsigned long OldCodeSize = getFunctionCodeSize(MF);

  //
  // Iterate through all the instructions within the function to locate
  // indirect branches and calls.
  //
  std::vector<MachineInstr *> IndirectBranches;
  std::vector<MachineInstr *> JTJs;
  for (MachineBasicBlock & MBB : MF) {
    for (MachineInstr & MI : MBB) {
      switch (MI.getOpcode()) {
      // Indirect branch
      case ARM::tBRIND:     // 0: GPR, 1: predCC, 2: predReg
      case ARM::tBX:        // 0: GPR, 1: predCC, 2: predReg
      case ARM::tBXNS:      // 0: GPR, 1: predCC, 2: predReg
        break;

      // Indirect call
      case ARM::tBLXr:      // 0: predCC, 1: predReg, 2: GPR
      case ARM::tBLXNSr:    // 0: predCC, 1: predReg, 2: GPRnopc
      case ARM::tBX_CALL:   // 0: tGPR
      case ARM::tTAILJMPr:  // 0: tcGPR
        IndirectBranches.push_back(&MI);
        break;

      // Jump table jump is complicated and not dealt with for now
      case ARM::tBR_JTr:    // 0: tGPR, 1: i32imm
      case ARM::tTBB_JT:    // 0: tGPR, 1: tGPR, 2: i32imm, 3: i32imm
      case ARM::tTBH_JT:    // 0: tGPR, 1: tGPR, 2: i32imm, 3: i32imm
      case ARM::t2BR_JT:    // 0: GPR, 1: GPR, 2: i32imm
      case ARM::t2TBB_JT:   // 0: GPR, 1: GPR, 2: i32imm, 3: i32imm
      case ARM::t2TBH_JT:   // 0: GPR, 1: GPR, 2: i32imm, 3: i32imm
        JTJs.push_back(&MI);
        break;

      //
      // Also list direct {function, system, hyper} calls here to make the
      // default branch be able to use MI.isCall().
      //
      case ARM::tBL:
      case ARM::tBLXi:
      case ARM::tTAILJMPd:
      case ARM::tTAILJMPdND:
      case ARM::tSVC:
      case ARM::t2SMC:
      case ARM::t2HVC:
        break;

      default:
        if (MI.isIndirectBranch() || MI.isCall()) {
          errs() << "[CFI]: unidentified branch/call: " << MI;
        }
        break;
      }
    }
  }

#if 1
  //
  // Insert a CFI label before the function if it is visible to other
  // compilation units or has its address taken.
  //
  const Function & F = MF.getFunction();
  if ((!F.hasInternalLinkage() && !F.hasPrivateLinkage()) ||
      F.hasAddressTaken()) {
    if (MF.begin() != MF.end()) {
      insertCFILabel(MF);
    }
  }
#else
  // Insert a CFI label before the function
  if (MF.begin() != MF.end()) {
    insertCFILabel(MF);
  }
#endif

  //
  // Insert a CFI check before each indirect branch and call, and insert a CFI
  // label before every successor MBB of each indirect branch.
  //
  for (MachineInstr * MI : IndirectBranches) {
    switch (MI->getOpcode()) {
    case ARM::tBLXr:      // 0: predCC, 1: predReg, 2: GPR
    case ARM::tBLXNSr:    // 0: predCC, 1: predReg, 2: GPRnopc
      insertCFICheck(*MI, MI->getOperand(2).getReg());
      break;

    case ARM::tBX_CALL:   // 0: tGPR
    case ARM::tTAILJMPr:  // 0: tcGPR
      insertCFICheck(*MI, MI->getOperand(0).getReg());
      break;

    default:
      llvm_unreachable("Unexpected opcode");
    }
  }

  unsigned long NewCodeSize = getFunctionCodeSize(MF);

  // Output code size information
  std::error_code EC;
  raw_fd_ostream MemStat("./code_size_cfi.stat", EC,
                         sys::fs::OpenFlags::F_Append);
  MemStat << MF.getFunction().getEntryBlock().getModule()->getSourceFileName() << ":" << MF.getName() << ":" << OldCodeSize << ":" << NewCodeSize << "\n";

  // Output jump table jump information
  raw_fd_ostream JTJStat("./jump_table_jump.stat", EC,
                         sys::fs::OpenFlags::F_Append);
  for (MachineInstr * MI : JTJs) {
    JTJStat << MI->getMF()->getName() << "\n";
  }

  return true;
}

namespace llvm {
  FunctionPass * createARMSilhouetteLabelCFI(void) {
    return new ARMSilhouetteLabelCFI();
  }
}
