//===---- X86CondBrFolding.cpp - optimize conditional branches ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This file defines a pass that optimizes condition branches on x86 by taking
// advantage of the three-way conditional code generated by compare
// instructions.
// Currently, it tries to hoisting EQ and NE conditional branch to a dominant
// conditional branch condition where the same EQ/NE conditional code is
// computed. An example:
//   bb_0:
//     cmp %0, 19
//     jg bb_1
//     jmp bb_2
//   bb_1:
//     cmp %0, 40
//     jg bb_3
//     jmp bb_4
//   bb_4:
//     cmp %0, 20
//     je bb_5
//     jmp bb_6
// Here we could combine the two compares in bb_0 and bb_4 and have the
// following code:
//   bb_0:
//     cmp %0, 20
//     jg bb_1
//     jl bb_2
//     jmp bb_5
//   bb_1:
//     cmp %0, 40
//     jg bb_3
//     jmp bb_6
// For the case of %0 == 20 (bb_5), we eliminate two jumps, and the control
// height for bb_6 is also reduced. bb_4 is gone after the optimization.
//
// There are plenty of this code patterns, especially from the switch case
// lowing where we generate compare of "pivot-1" for the inner nodes in the
// binary search tree.
//===----------------------------------------------------------------------===//

#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineBranchProbabilityInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/BranchProbability.h"

using namespace llvm;

#define DEBUG_TYPE "x86-condbr-folding"

STATISTIC(NumFixedCondBrs, "Number of x86 condbr folded");

namespace {
class X86CondBrFoldingPass : public MachineFunctionPass {
public:
  X86CondBrFoldingPass() : MachineFunctionPass(ID) {
    initializeX86CondBrFoldingPassPass(*PassRegistry::getPassRegistry());
  }
  StringRef getPassName() const override { return "X86 CondBr Folding"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
    AU.addRequired<MachineBranchProbabilityInfo>();
  }

public:
  static char ID;
};
} // namespace

char X86CondBrFoldingPass::ID = 0;
INITIALIZE_PASS(X86CondBrFoldingPass, "X86CondBrFolding", "X86CondBrFolding", false, false)

FunctionPass *llvm::createX86CondBrFolding() {
  return new X86CondBrFoldingPass();
}

namespace {
// A class the stores the auxiliary information for each MBB.
struct TargetMBBInfo {
  MachineBasicBlock *TBB;
  MachineBasicBlock *FBB;
  MachineInstr *BrInstr;
  MachineInstr *CmpInstr;
  X86::CondCode BranchCode;
  unsigned SrcReg;
  int CmpValue;
  bool Modified;
  bool CmpBrOnly;
};

// A class that optimizes the conditional branch by hoisting and merge CondCode.
class X86CondBrFolding {
public:
  X86CondBrFolding(const X86InstrInfo *TII,
                   const MachineBranchProbabilityInfo *MBPI,
                   MachineFunction &MF)
      : TII(TII), MBPI(MBPI), MF(MF) {}
  bool optimize();

private:
  const X86InstrInfo *TII;
  const MachineBranchProbabilityInfo *MBPI;
  MachineFunction &MF;
  std::vector<std::unique_ptr<TargetMBBInfo>> MBBInfos;
  SmallVector<MachineBasicBlock *, 4> RemoveList;

  void optimizeCondBr(MachineBasicBlock &MBB,
                      SmallVectorImpl<MachineBasicBlock *> &BranchPath);
  void fixBranchProb(MachineBasicBlock *NextMBB, MachineBasicBlock *RootMBB,
                     SmallVectorImpl<MachineBasicBlock *> &BranchPath);
  void replaceBrDest(MachineBasicBlock *MBB, MachineBasicBlock *OrigDest,
                     MachineBasicBlock *NewDest);
  void fixupModifiedCond(MachineBasicBlock *MBB);
  std::unique_ptr<TargetMBBInfo> analyzeMBB(MachineBasicBlock &MBB);
  static bool analyzeCompare(const MachineInstr &MI, unsigned &SrcReg,
                             int &CmpValue);
  bool findPath(MachineBasicBlock *MBB,
                SmallVectorImpl<MachineBasicBlock *> &BranchPath);
  TargetMBBInfo *getMBBInfo(MachineBasicBlock *MBB) const {
    return MBBInfos[MBB->getNumber()].get();
  }
};
} // namespace

// Find a valid path that we can reuse the CondCode.
// The resulted path (if return true) is stored in BranchPath.
// Return value:
//  false: is no valid path is found.
//  true: a valid path is found and the targetBB can be reached.
bool X86CondBrFolding::findPath(
    MachineBasicBlock *MBB, SmallVectorImpl<MachineBasicBlock *> &BranchPath) {
  TargetMBBInfo *MBBInfo = getMBBInfo(MBB);
  assert(MBBInfo && "Expecting a candidate MBB");
  int CmpValue = MBBInfo->CmpValue;

  MachineBasicBlock *PredMBB = *MBB->pred_begin();
  MachineBasicBlock *SaveMBB = MBB;
  while (PredMBB) {
    TargetMBBInfo *PredMBBInfo = getMBBInfo(PredMBB);
    if (!PredMBBInfo || PredMBBInfo->SrcReg != MBBInfo->SrcReg)
      return false;

    assert(SaveMBB == PredMBBInfo->TBB || SaveMBB == PredMBBInfo->FBB);
    bool IsFalseBranch = (SaveMBB == PredMBBInfo->FBB);

    X86::CondCode CC = PredMBBInfo->BranchCode;
    assert(CC == X86::COND_L || CC == X86::COND_G || CC == X86::COND_E);
    int PredCmpValue = PredMBBInfo->CmpValue;
    bool ValueCmpTrue = ((CmpValue < PredCmpValue && CC == X86::COND_L) ||
                         (CmpValue > PredCmpValue && CC == X86::COND_G) ||
                         (CmpValue == PredCmpValue && CC == X86::COND_E));
    // Check if both the result of value compare and the branch target match.
    if (!(ValueCmpTrue ^ IsFalseBranch)) {
      LLVM_DEBUG(dbgs() << "Dead BB detected!\n");
      return false;
    }

    BranchPath.push_back(PredMBB);
    // These are the conditions on which we could combine the compares.
    if ((CmpValue == PredCmpValue) ||
        (CmpValue == PredCmpValue - 1 && CC == X86::COND_L) ||
        (CmpValue == PredCmpValue + 1 && CC == X86::COND_G))
      return true;

    // If PredMBB has more than on preds, or not a pure cmp and br, we bailout.
    if (PredMBB->pred_size() != 1 || !PredMBBInfo->CmpBrOnly)
      return false;

    SaveMBB = PredMBB;
    PredMBB = *PredMBB->pred_begin();
  }
  return false;
}

// Fix up any PHI node in the successor of MBB.
static void fixPHIsInSucc(MachineBasicBlock *MBB, MachineBasicBlock *OldMBB,
                          MachineBasicBlock *NewMBB) {
  if (NewMBB == OldMBB)
    return;
  for (auto MI = MBB->instr_begin(), ME = MBB->instr_end();
       MI != ME && MI->isPHI(); ++MI)
    for (unsigned i = 2, e = MI->getNumOperands() + 1; i != e; i += 2) {
      MachineOperand &MO = MI->getOperand(i);
      if (MO.getMBB() == OldMBB)
        MO.setMBB(NewMBB);
    }
}

// Utility function to set branch probability for edge MBB->SuccMBB.
static inline bool setBranchProb(MachineBasicBlock *MBB,
                                 MachineBasicBlock *SuccMBB,
                                 BranchProbability Prob) {
  auto MBBI = std::find(MBB->succ_begin(), MBB->succ_end(), SuccMBB);
  if (MBBI == MBB->succ_end())
    return false;
  MBB->setSuccProbability(MBBI, Prob);
  return true;
}

// Utility function to find the unconditional br instruction in MBB.
static inline MachineBasicBlock::iterator
findUncondBrI(MachineBasicBlock *MBB) {
  return std::find_if(MBB->begin(), MBB->end(), [](MachineInstr &MI) -> bool {
    return MI.getOpcode() == X86::JMP_1;
  });
}

// Replace MBB's original successor, OrigDest, with NewDest.
// Also update the MBBInfo for MBB.
void X86CondBrFolding::replaceBrDest(MachineBasicBlock *MBB,
                                     MachineBasicBlock *OrigDest,
                                     MachineBasicBlock *NewDest) {
  TargetMBBInfo *MBBInfo = getMBBInfo(MBB);
  MachineInstr *BrMI;
  if (MBBInfo->TBB == OrigDest) {
    BrMI = MBBInfo->BrInstr;
    unsigned JNCC = GetCondBranchFromCond(MBBInfo->BranchCode);
    MachineInstrBuilder MIB =
        BuildMI(*MBB, BrMI, MBB->findDebugLoc(BrMI), TII->get(JNCC))
            .addMBB(NewDest);
    MBBInfo->TBB = NewDest;
    MBBInfo->BrInstr = MIB.getInstr();
  } else { // Should be the unconditional jump stmt.
    MachineBasicBlock::iterator UncondBrI = findUncondBrI(MBB);
    BuildMI(*MBB, UncondBrI, MBB->findDebugLoc(UncondBrI), TII->get(X86::JMP_1))
        .addMBB(NewDest);
    MBBInfo->FBB = NewDest;
    BrMI = &*UncondBrI;
  }
  fixPHIsInSucc(NewDest, OrigDest, MBB);
  BrMI->eraseFromParent();
  MBB->addSuccessor(NewDest);
  setBranchProb(MBB, NewDest, MBPI->getEdgeProbability(MBB, OrigDest));
  MBB->removeSuccessor(OrigDest);
}

// Change the CondCode and BrInstr according to MBBInfo.
void X86CondBrFolding::fixupModifiedCond(MachineBasicBlock *MBB) {
  TargetMBBInfo *MBBInfo = getMBBInfo(MBB);
  if (!MBBInfo->Modified)
    return;

  MachineInstr *BrMI = MBBInfo->BrInstr;
  X86::CondCode CC = MBBInfo->BranchCode;
  MachineInstrBuilder MIB = BuildMI(*MBB, BrMI, MBB->findDebugLoc(BrMI),
                                    TII->get(GetCondBranchFromCond(CC)))
                                .addMBB(MBBInfo->TBB);
  BrMI->eraseFromParent();
  MBBInfo->BrInstr = MIB.getInstr();

  MachineBasicBlock::iterator UncondBrI = findUncondBrI(MBB);
  BuildMI(*MBB, UncondBrI, MBB->findDebugLoc(UncondBrI), TII->get(X86::JMP_1))
      .addMBB(MBBInfo->FBB);
  MBB->erase(UncondBrI);
  MBBInfo->Modified = false;
}

//
// Apply the transformation:
//  RootMBB -1-> ... PredMBB -3-> MBB -5-> TargetMBB
//     \-2->           \-4->       \-6-> FalseMBB
// ==>
//             RootMBB -1-> ... PredMBB -7-> FalseMBB
// TargetMBB <-8-/ \-2->           \-4->
//
// Note that PredMBB and RootMBB could be the same.
// And in the case of dead TargetMBB, we will not have TargetMBB and edge 8.
//
// There are some special handling where the RootMBB is COND_E in which case
// we directly short-cycle the brinstr.
//
void X86CondBrFolding::optimizeCondBr(
    MachineBasicBlock &MBB, SmallVectorImpl<MachineBasicBlock *> &BranchPath) {

  X86::CondCode CC;
  TargetMBBInfo *MBBInfo = getMBBInfo(&MBB);
  assert(MBBInfo && "Expecting a candidate MBB");
  MachineBasicBlock *TargetMBB = MBBInfo->TBB;
  BranchProbability TargetProb = MBPI->getEdgeProbability(&MBB, MBBInfo->TBB);

  // Forward the jump from MBB's predecessor to MBB's false target.
  MachineBasicBlock *PredMBB = BranchPath.front();
  TargetMBBInfo *PredMBBInfo = getMBBInfo(PredMBB);
  assert(PredMBBInfo && "Expecting a candidate MBB");
  if (PredMBBInfo->Modified)
    fixupModifiedCond(PredMBB);
  CC = PredMBBInfo->BranchCode;
  // Don't do this if depth of BranchPath is 1 and PredMBB is of COND_E.
  // We will short-cycle directly for this case.
  if (!(CC == X86::COND_E && BranchPath.size() == 1))
    replaceBrDest(PredMBB, &MBB, MBBInfo->FBB);

  MachineBasicBlock *RootMBB = BranchPath.back();
  TargetMBBInfo *RootMBBInfo = getMBBInfo(RootMBB);
  assert(RootMBBInfo && "Expecting a candidate MBB");
  if (RootMBBInfo->Modified)
    fixupModifiedCond(RootMBB);
  CC = RootMBBInfo->BranchCode;

  if (CC != X86::COND_E) {
    MachineBasicBlock::iterator UncondBrI = findUncondBrI(RootMBB);
    // RootMBB: Cond jump to the original not-taken MBB.
    X86::CondCode NewCC;
    switch (CC) {
    case X86::COND_L:
      NewCC = X86::COND_G;
      break;
    case X86::COND_G:
      NewCC = X86::COND_L;
      break;
    default:
      llvm_unreachable("unexpected condtional code.");
    }
    BuildMI(*RootMBB, UncondBrI, RootMBB->findDebugLoc(UncondBrI),
            TII->get(GetCondBranchFromCond(NewCC)))
        .addMBB(RootMBBInfo->FBB);

    // RootMBB: Jump to TargetMBB
    BuildMI(*RootMBB, UncondBrI, RootMBB->findDebugLoc(UncondBrI),
            TII->get(X86::JMP_1))
        .addMBB(TargetMBB);
    RootMBB->addSuccessor(TargetMBB);
    fixPHIsInSucc(TargetMBB, &MBB, RootMBB);
    RootMBB->erase(UncondBrI);
  } else {
    replaceBrDest(RootMBB, RootMBBInfo->TBB, TargetMBB);
  }

  // Fix RootMBB's CmpValue to MBB's CmpValue to TargetMBB. Don't set Imm
  // directly. Move MBB's stmt to here as the opcode might be different.
  if (RootMBBInfo->CmpValue != MBBInfo->CmpValue) {
    MachineInstr *NewCmp = MBBInfo->CmpInstr;
    NewCmp->removeFromParent();
    RootMBB->insert(RootMBBInfo->CmpInstr, NewCmp);
    RootMBBInfo->CmpInstr->eraseFromParent();
  }

  // Fix branch Probabilities.
  auto fixBranchProb = [&](MachineBasicBlock *NextMBB) {
    BranchProbability Prob;
    for (auto &I : BranchPath) {
      MachineBasicBlock *ThisMBB = I;
      if (!ThisMBB->hasSuccessorProbabilities() ||
          !ThisMBB->isSuccessor(NextMBB))
        break;
      Prob = MBPI->getEdgeProbability(ThisMBB, NextMBB);
      if (Prob.isUnknown())
        break;
      TargetProb = Prob * TargetProb;
      Prob = Prob - TargetProb;
      setBranchProb(ThisMBB, NextMBB, Prob);
      if (ThisMBB == RootMBB) {
        setBranchProb(ThisMBB, TargetMBB, TargetProb);
      }
      ThisMBB->normalizeSuccProbs();
      if (ThisMBB == RootMBB)
        break;
      NextMBB = ThisMBB;
    }
    return true;
  };
  if (CC != X86::COND_E && !TargetProb.isUnknown())
    fixBranchProb(MBBInfo->FBB);

  if (CC != X86::COND_E)
    RemoveList.push_back(&MBB);

  // Invalidate MBBInfo just in case.
  MBBInfos[MBB.getNumber()] = nullptr;
  MBBInfos[RootMBB->getNumber()] = nullptr;

  LLVM_DEBUG(dbgs() << "After optimization:\nRootMBB is: " << *RootMBB << "\n");
  if (BranchPath.size() > 1)
    LLVM_DEBUG(dbgs() << "PredMBB is: " << *(BranchPath[0]) << "\n");
}

// Driver function for optimization: find the valid candidate and apply
// the transformation.
bool X86CondBrFolding::optimize() {
  bool Changed = false;
  LLVM_DEBUG(dbgs() << "***** X86CondBr Folding on Function: " << MF.getName()
                    << " *****\n");
  // Setup data structures.
  MBBInfos.resize(MF.getNumBlockIDs());
  for (auto &MBB : MF)
    MBBInfos[MBB.getNumber()] = analyzeMBB(MBB);

  for (auto &MBB : MF) {
    TargetMBBInfo *MBBInfo = getMBBInfo(&MBB);
    if (!MBBInfo || !MBBInfo->CmpBrOnly)
      continue;
    if (MBB.pred_size() != 1)
      continue;
    LLVM_DEBUG(dbgs() << "Work on MBB." << MBB.getNumber()
                      << " CmpValue: " << MBBInfo->CmpValue << "\n");
    SmallVector<MachineBasicBlock *, 4> BranchPath;
    if (!findPath(&MBB, BranchPath))
      continue;

#ifndef NDEBUG
    LLVM_DEBUG(dbgs() << "Found one path (len=" << BranchPath.size() << "):\n");
    int Index = 1;
    LLVM_DEBUG(dbgs() << "Target MBB is: " << MBB << "\n");
    for (auto I = BranchPath.rbegin(); I != BranchPath.rend(); ++I, ++Index) {
      MachineBasicBlock *PMBB = *I;
      TargetMBBInfo *PMBBInfo = getMBBInfo(PMBB);
      LLVM_DEBUG(dbgs() << "Path MBB (" << Index << " of " << BranchPath.size()
                        << ") is " << *PMBB);
      LLVM_DEBUG(dbgs() << "CC=" << PMBBInfo->BranchCode
                        << "  Val=" << PMBBInfo->CmpValue
                        << "  CmpBrOnly=" << PMBBInfo->CmpBrOnly << "\n\n");
    }
#endif
    optimizeCondBr(MBB, BranchPath);
    Changed = true;
  }
  NumFixedCondBrs += RemoveList.size();
  for (auto MBBI : RemoveList) {
    while (!MBBI->succ_empty())
      MBBI->removeSuccessor(MBBI->succ_end() - 1);

    MBBI->eraseFromParent();
  }

  return Changed;
}

// Analyze instructions that generate CondCode and extract information.
bool X86CondBrFolding::analyzeCompare(const MachineInstr &MI, unsigned &SrcReg,
                                      int &CmpValue) {
  unsigned SrcRegIndex = 0;
  unsigned ValueIndex = 0;
  switch (MI.getOpcode()) {
  // TODO: handle test instructions.
  default:
    return false;
  case X86::CMP64ri32:
  case X86::CMP64ri8:
  case X86::CMP32ri:
  case X86::CMP32ri8:
  case X86::CMP16ri:
  case X86::CMP16ri8:
  case X86::CMP8ri:
    SrcRegIndex = 0;
    ValueIndex = 1;
    break;
  case X86::SUB64ri32:
  case X86::SUB64ri8:
  case X86::SUB32ri:
  case X86::SUB32ri8:
  case X86::SUB16ri:
  case X86::SUB16ri8:
  case X86::SUB8ri:
    SrcRegIndex = 1;
    ValueIndex = 2;
    break;
  }
  SrcReg = MI.getOperand(SrcRegIndex).getReg();
  if (!MI.getOperand(ValueIndex).isImm())
    return false;
  CmpValue = MI.getOperand(ValueIndex).getImm();
  return true;
}

// Analyze a candidate MBB and set the extract all the information needed.
// The valid candidate will have two successors.
// It also should have a sequence of
//  Branch_instr,
//  CondBr,
//  UnCondBr.
// Return TargetMBBInfo if MBB is a valid candidate and nullptr otherwise.
std::unique_ptr<TargetMBBInfo>
X86CondBrFolding::analyzeMBB(MachineBasicBlock &MBB) {
  MachineBasicBlock *TBB;
  MachineBasicBlock *FBB;
  MachineInstr *BrInstr;
  MachineInstr *CmpInstr;
  X86::CondCode CC;
  unsigned SrcReg;
  int CmpValue;
  bool Modified;
  bool CmpBrOnly;

  if (MBB.succ_size() != 2)
    return nullptr;

  CmpBrOnly = true;
  FBB = TBB = nullptr;
  CmpInstr = nullptr;
  MachineBasicBlock::iterator I = MBB.end();
  while (I != MBB.begin()) {
    --I;
    if (I->isDebugValue())
      continue;
    if (I->getOpcode() == X86::JMP_1) {
      if (FBB)
        return nullptr;
      FBB = I->getOperand(0).getMBB();
      continue;
    }
    if (I->isBranch()) {
      if (TBB)
        return nullptr;
      CC = X86::getCondFromBranchOpc(I->getOpcode());
      switch (CC) {
      default:
        return nullptr;
      case X86::COND_E:
      case X86::COND_L:
      case X86::COND_G:
      case X86::COND_NE:
      case X86::COND_LE:
      case X86::COND_GE:
        break;
      }
      TBB = I->getOperand(0).getMBB();
      BrInstr = &*I;
      continue;
    }
    if (analyzeCompare(*I, SrcReg, CmpValue)) {
      if (CmpInstr)
        return nullptr;
      CmpInstr = &*I;
      continue;
    }
    CmpBrOnly = false;
    break;
  }

  if (!TBB || !FBB || !CmpInstr)
    return nullptr;

  // Simplify CondCode. Note this is only to simplify the findPath logic
  // and will not change the instruction here.
  switch (CC) {
  case X86::COND_NE:
    CC = X86::COND_E;
    std::swap(TBB, FBB);
    Modified = true;
    break;
  case X86::COND_LE:
    if (CmpValue == INT_MAX)
      return nullptr;
    CC = X86::COND_L;
    CmpValue += 1;
    Modified = true;
    break;
  case X86::COND_GE:
    if (CmpValue == INT_MIN)
      return nullptr;
    CC = X86::COND_G;
    CmpValue -= 1;
    Modified = true;
    break;
  default:
    Modified = false;
    break;
  }
  return llvm::make_unique<TargetMBBInfo>(TargetMBBInfo{
      TBB, FBB, BrInstr, CmpInstr, CC, SrcReg, CmpValue, Modified, CmpBrOnly});
}

bool X86CondBrFoldingPass::runOnMachineFunction(MachineFunction &MF) {
  const X86Subtarget &ST = MF.getSubtarget<X86Subtarget>();
  if (!ST.threewayBranchProfitable())
    return false;
  const X86InstrInfo *TII = ST.getInstrInfo();
  const MachineBranchProbabilityInfo *MBPI =
      &getAnalysis<MachineBranchProbabilityInfo>();

  X86CondBrFolding CondBr(TII, MBPI, MF);
  return CondBr.optimize();
}