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

#include "ARMSilhouetteInstrumentor.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

namespace llvm {

  struct ARMKageCodeScanner
      : public MachineFunctionPass, ARMSilhouetteInstrumentor {
    // pass identifier variable
    static char ID;

    ARMKageCodeScanner();

    virtual StringRef getPassName() const override;

    virtual bool runOnMachineFunction(MachineFunction & MF) override;
  };

  FunctionPass * createARMKageCodeScanner(void);
}
