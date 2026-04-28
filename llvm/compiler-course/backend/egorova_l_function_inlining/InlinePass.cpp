#include "X86.h"
#include "X86InstrInfo.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "egorova-inline"

namespace {

class EgorovaInlineFunctionPass : public MachineFunctionPass {
public:
  static char ID;
  EgorovaInlineFunctionPass() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  static constexpr unsigned MaxInlineInstrs = 15;
  static constexpr unsigned MaxRecDepth = 3;

  DenseMap<const Function *, unsigned> RecDepth;

  bool tryInline(MachineFunction &Caller, MachineBasicBlock &MBB,
                 MachineInstr &MI);
  bool isInlineCandidate(MachineFunction &MF);
};

char EgorovaInlineFunctionPass::ID = 0;

bool EgorovaInlineFunctionPass::isInlineCandidate(MachineFunction &MF) {
  if (MF.size() != 1)
    return false;

  unsigned Cnt = 0;
  for (auto &BB : MF) {
    for (auto &MI : BB) {
      if (!MI.isDebugInstr())
        ++Cnt;
    }
  }

  return Cnt <= MaxInlineInstrs;
}

bool EgorovaInlineFunctionPass::tryInline(MachineFunction &Caller,
                                          MachineBasicBlock &MBB,
                                          MachineInstr &MI) {
  if (MI.getOpcode() != X86::CALL64pcrel32)
    return false;

  if (MI.getNumOperands() == 0)
    return false;

  MachineOperand &Op = MI.getOperand(0);
  if (!Op.isGlobal())
    return false;

  const Function *CalleeF = dyn_cast<Function>(Op.getGlobal());
  if (!CalleeF)
    return false;

  if (RecDepth[CalleeF] >= MaxRecDepth)
    return false;

  MachineFunction *CalleeMF = nullptr;

  if (CalleeF == &Caller.getFunction()) {
    CalleeMF = &Caller;
  } else {
    auto &MMI = getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
    CalleeMF = MMI.getMachineFunction(*CalleeF);
    if (!CalleeMF)
      return false;
  }

  if (!isInlineCandidate(*CalleeMF))
    return false;

  ++RecDepth[CalleeF];

  MachineRegisterInfo &MRI = Caller.getRegInfo();
  MachineBasicBlock &CalleeBB = CalleeMF->front();

  DenseMap<Register, Register> VRegMap;
  SmallVector<MachineInstr *, 16> ToClone;

  for (auto &I : CalleeBB) {
    if (!I.isReturn())
      ToClone.push_back(&I);
  }

  for (MachineInstr *Src : ToClone) {
    MachineInstr *NewMI = Caller.CloneMachineInstr(Src);

    for (MachineOperand &MO : NewMI->operands()) {
      if (!MO.isReg())
        continue;

      Register R = MO.getReg();
      if (!R.isVirtual())
        continue;

      auto It = VRegMap.find(R);
      if (It == VRegMap.end()) {
        const TargetRegisterClass *RC = MRI.getRegClass(R);
        Register NewR = MRI.createVirtualRegister(RC);
        It = VRegMap.insert({R, NewR}).first;
      }

      MO.setReg(It->second);
    }

    MBB.insert(MI.getIterator(), NewMI);
  }

  MI.eraseFromParent();

  --RecDepth[CalleeF];
  return true;
}

bool EgorovaInlineFunctionPass::runOnMachineFunction(MachineFunction &MF) {
  bool Changed = false;
  bool LocalChanged = true;

  while (LocalChanged) {
    LocalChanged = false;

    for (auto &MBB : MF) {
      for (auto It = MBB.begin(); It != MBB.end();) {
        MachineInstr &MI = *It++;
        if (tryInline(MF, MBB, MI)) {
          LocalChanged = true;
          Changed = true;
        }
      }
    }
  }

  return Changed;
}

} // namespace

static RegisterPass<EgorovaInlineFunctionPass>
    X("egorova-x86-inline", "Machine IR Function Inlining", false, false);