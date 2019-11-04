//===- ARMSilhouetteShadowStack - Modify Prologue/Epilogue for Shadow Stack ==//
//
//         Protecting Control Flow of Real-time OS applications
//
// This file was written by at the University of Rochester.
// All Rights Reserved.
//
//===----------------------------------------------------------------------===//
//
// This pass instruments the function prologue/epilogue to save/load the return
// address from a parallel shadow stack.
//
//===----------------------------------------------------------------------===//
//

#include "ARM.h"
#include "ARMBaseInstrInfo.h"
#include "ARMSilhouetteConvertFuncList.h"
#include "ARMSilhouetteShadowStack.h"
#include "ARMTargetMachine.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"

#include <deque>

using namespace llvm;

extern bool SilhouetteInvert;

char ARMSilhouetteShadowStack::ID = 0;

static DebugLoc DL;

static cl::opt<int>
ShadowStackOffset("arm-silhouette-shadowstack-offset",
                  cl::desc("Silhouette shadow stack offset"),
                  cl::init(4092), cl::Hidden);

ARMSilhouetteShadowStack::ARMSilhouetteShadowStack()
    : MachineFunctionPass(ID) {
  return;
}

StringRef
ARMSilhouetteShadowStack::getPassName() const {
  return "ARM Silhouette Shadow Stack Pass";
}

//
// Function: findTailJmp()
//
// Description:
//   This function finds a TAILJMP instruction after a given instruction MI in
//   the same basic block.
//
// Input:
//   MI - A reference to the instruction after which to find TAILJMP.
//
// Return value:
//   A pointer to TAILJMP if found, nullptr otherwise.
//
static MachineInstr *
findTailJmp(MachineInstr & MI) {
  MachineInstr * I = MI.getNextNode();
  while (I != nullptr) {
    switch (I->getOpcode()) {
    case ARM::tTAILJMPr:
    case ARM::tTAILJMPd:
    case ARM::tTAILJMPdND:
    case ARM::tBX_RET:  // This is also the case!
      return I;

    default:
      I = I->getNextNode();
      break;
    }
  }

  return nullptr;
}

//
// Method: setupShadowStack()
//
// Description:
//   This method inserts instructions that store the return address onto the
//   shadow stack.
//
// Input:
//   MI - A reference to a PUSH instruction before which to insert instructions.
//
void
ARMSilhouetteShadowStack::setupShadowStack(MachineInstr & MI) {
  MachineFunction & MF = *MI.getMF();
  const TargetInstrInfo * TII = MF.getSubtarget().getInstrInfo();

  int offset = ShadowStackOffset;
  int offsetNonNeg = offset >= 0 ? offset : -offset;
  int offsetToGo = offsetNonNeg;
  unsigned addOpc = offset >= 0 ? ARM::t2ADDri12 : ARM::t2SUBri12;
  unsigned subOpc = offset >= 0 ? ARM::t2SUBri12 : ARM::t2ADDri12;
  unsigned strOpc = SilhouetteInvert ? ARM::t2STRT : ARM::t2STRi12;

  unsigned PredReg;
  ARMCC::CondCodes Pred = getInstrPredicate(MI, PredReg);

  std::deque<MachineInstr *> NewMIs;

  // Adjust SP properly
  while (offsetToGo > 4092) {
    NewMIs.push_back(BuildMI(MF, DL, TII->get(addOpc), ARM::SP)
                     .addReg(ARM::SP)
                     .addImm(4092)
                     .add(predOps(Pred, PredReg))
                     .setMIFlag(MachineInstr::ShadowStack));
    offsetToGo -= 4092;
  }
  if (offset < 0 || (SilhouetteInvert && offsetToGo > 255)) {
    NewMIs.push_back(BuildMI(MF, DL, TII->get(addOpc), ARM::SP)
                     .addReg(ARM::SP)
                     .addImm(offsetToGo)
                     .add(predOps(Pred, PredReg))
                     .setMIFlag(MachineInstr::ShadowStack));
    offsetToGo = 0;
  }

  // Generate an STR to the shadow stack
  auto MIB = BuildMI(MF, DL, TII->get(strOpc), ARM::LR)
             .addReg(ARM::SP)
             .addImm(offsetToGo);
  if (strOpc == ARM::t2STRi12) {
    MIB.add(predOps(Pred, PredReg));
  }
  NewMIs.push_back(MIB.setMIFlag(MachineInstr::ShadowStack));

  // Restore SP; subtract back the same amount we added to SP
  offsetToGo = offsetNonNeg - offsetToGo;
  while (offsetToGo > 4092) {
    NewMIs.push_back(BuildMI(MF, DL, TII->get(subOpc), ARM::SP)
                     .addReg(ARM::SP)
                     .addImm(4092)
                     .add(predOps(Pred, PredReg))
                     .setMIFlag(MachineInstr::ShadowStack));
    offsetToGo -= 4092;
  }
  if (offsetToGo != 0) {
    NewMIs.push_back(BuildMI(MF, DL, TII->get(subOpc), ARM::SP)
                     .addReg(ARM::SP)
                     .addImm(offsetToGo)
                     .add(predOps(Pred, PredReg))
                     .setMIFlag(MachineInstr::ShadowStack));
    offsetToGo = 0;
  }

  // Now insert these new instructions into the basic block
  insertInstsBefore(MI, NewMIs);
}

//
// Method: popFromShadowStack()
//
// Description:
//   This method modifies a POP instruction to not write to PC/LR and inserts
//   new instructions that load the return address from the shadow stack into
//   PC/LR.
//
// Input:
//   MI   - A reference to a POP instruction after which to insert instructions.
//   PCLR - A reference to the PC or LR operand of the POP.
//
void
ARMSilhouetteShadowStack::popFromShadowStack(MachineInstr & MI,
                                             MachineOperand & PCLR) {
  MachineFunction & MF = *MI.getMF();
  const TargetInstrInfo * TII = MF.getSubtarget().getInstrInfo();

  int offset = ShadowStackOffset;

  unsigned PredReg;
  ARMCC::CondCodes Pred = getInstrPredicate(MI, PredReg);

  std::deque<MachineInstr *> NewMIs;

  // Shortcut for small positive offset so that we don't need to restore SP
  if (offset > 0 && offset < 4096) {
    // Adjust SP to skip PC/LR on the stack
    NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::tADDspi), ARM::SP)
                     .addReg(ARM::SP)
                     .addImm(1)
                     .add(predOps(Pred, PredReg))
                     .setMIFlag(MachineInstr::ShadowStack));
    // Generate an LDR from the shadow stack
    NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::t2LDRi12), PCLR.getReg())
                     .addReg(ARM::SP)
                     .addImm(offset)
                     .add(predOps(Pred, PredReg))
                     .setMIFlag(MachineInstr::ShadowStack));
  } else {
    offset += 4;  // Skip PC/LR on the stack
    int offsetNonNeg = offset >= 0 ? offset : -offset;
    int offsetToGo = offsetNonNeg;
    unsigned addOpc = offset >= 0 ? ARM::t2ADDri12 : ARM::t2SUBri12;
    unsigned subOpc = offset >= 0 ? ARM::t2SUBri12 : ARM::t2ADDri12;

    // Adjust SP properly
    while (offsetToGo > 4092) {
      NewMIs.push_back(BuildMI(MF, DL, TII->get(addOpc), ARM::SP)
                       .addReg(ARM::SP)
                       .addImm(4092)
                       .add(predOps(Pred, PredReg))
                       .setMIFlag(MachineInstr::ShadowStack));
      offsetToGo -= 4092;
    }
    if (offset < 0) {
      NewMIs.push_back(BuildMI(MF, DL, TII->get(addOpc), ARM::SP)
                       .addReg(ARM::SP)
                       .addImm(offsetToGo)
                       .add(predOps(Pred, PredReg))
                       .setMIFlag(MachineInstr::ShadowStack));
      offsetToGo = 0;
    }

    // Generate an LDR from the shadow stack to LR
    NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::t2LDRi12), ARM::LR)
                     .addReg(ARM::SP)
                     .addImm(offsetToGo)
                     .add(predOps(Pred, PredReg))
                     .setMIFlag(MachineInstr::ShadowStack));

    // Restore SP; subtract it by the amount we added minus 4
    offset -= 4;
    offsetNonNeg = offset >= 0 ? offset : -offset;
    offsetToGo = offsetNonNeg - offsetToGo;
    while (offsetToGo > 4092) {
      NewMIs.push_back(BuildMI(MF, DL, TII->get(subOpc), ARM::SP)
                       .addReg(ARM::SP)
                       .addImm(4092)
                       .add(predOps(Pred, PredReg))
                       .setMIFlag(MachineInstr::ShadowStack));
      offsetToGo -= 4092;
    }
    if (offsetToGo != 0) {
      NewMIs.push_back(BuildMI(MF, DL, TII->get(subOpc), ARM::SP)
                       .addReg(ARM::SP)
                       .addImm(offsetToGo)
                       .add(predOps(Pred, PredReg))
                       .setMIFlag(MachineInstr::ShadowStack));
      offsetToGo = 0;
    }

    // Generate a BX_RET when necessary
    if (PCLR.getReg() == ARM::PC) {
      NewMIs.push_back(BuildMI(MF, DL, TII->get(ARM::tBX_RET))
                       .add(predOps(Pred, PredReg)));
    }
  }

  // Now insert these new instructions into the basic block
  insertInstsAfter(MI, NewMIs);

  // At last, replace the old POP with a new one that doesn't write to PC/LR
  switch (MI.getOpcode()) {
  case ARM::t2LDMIA_RET:
    MI.setDesc(TII->get(ARM::t2LDMIA_UPD));
    break;

  case ARM::tPOP_RET:
    MI.setDesc(TII->get(ARM::tPOP));
    break;

  default:
    break;
  }
  MI.RemoveOperand(MI.getOperandNo(&PCLR));
}

//
// Method: runOnMachineFunction()
//
// Description:
//   This method is called when the PassManager wants this pass to transform
//   the specified MachineFunction.  This method instruments the
//   prologue/epilogue of a MachineFunction so that the return address is saved
//   into/loaded from the shadow stack.
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
ARMSilhouetteShadowStack::runOnMachineFunction(MachineFunction & MF) {
#if 1
  // Skip certain functions
  if (funcBlacklist.find(MF.getName()) != funcBlacklist.end()) {
    return false;
  }
#endif

#if 1
  errs() << "[SS] Stack frame size: " << MF.getName() << ": "
         << MF.getFrameInfo().getStackSize() << "\n";
#endif

  unsigned long OldCodeSize = getFunctionCodeSize(MF);

  for (MachineBasicBlock & MBB : MF) {
    for (MachineInstr & MI : MBB) {
      switch (MI.getOpcode()) {
      // Frame setup instructions in function prologue
      case ARM::t2STMDB_UPD:
        // STMDB_UPD writing to SP! is treated same as PUSH
        if (MI.getOperand(0).getReg() != ARM::SP) {
          break;
        }
        LLVM_FALLTHROUGH;
      case ARM::tPUSH:
        // LR can appear as a GPR not in prologue, in which case we don't care
        if (MI.getFlag(MachineInstr::FrameSetup)) {
          for (MachineOperand & MO : MI.operands()) {
            if (MO.isReg() && MO.getReg() == ARM::LR) {
              setupShadowStack(MI);
              break;
            }
          }
        }
        break;

      // Frame destroy instructions in function epilogue
      case ARM::t2LDMIA_UPD:
      case ARM::t2LDMIA_RET:
        // LDMIA_UPD writing to SP! is treated same as POP
        if (MI.getOperand(0).getReg() != ARM::SP) {
          break;
        }
        LLVM_FALLTHROUGH;
      case ARM::tPOP:
      case ARM::tPOP_RET:
        // Handle 2 cases:
        // (1) POP writing to LR followed by TAILJMP.
        // (2) POP writing to PC
        for (MachineOperand & MO : MI.operands()) {
          if (MO.isReg()) {
            if ((MO.getReg() == ARM::LR && findTailJmp(MI) != nullptr) ||
                MO.getReg() == ARM::PC) {
              popFromShadowStack(MI, MO);
              // Bail out as POP cannot write to both LR and PC
              break;
            }
          }
        }
        break;

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
  MemStat << MF.getName() << ":" << OldCodeSize << ":" << NewCodeSize << "\n";

  return true;
}

//
// Create a new pass.
//
namespace llvm {
  FunctionPass * createARMSilhouetteShadowStack(void) {
    return new ARMSilhouetteShadowStack();
  }
}
