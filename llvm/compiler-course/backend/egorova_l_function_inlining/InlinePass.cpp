#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "egorova-inline"

namespace {

class EgorovaInlineFunctionPass : public ModulePass {
public:
  static char ID;
  EgorovaInlineFunctionPass() : ModulePass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    ModulePass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override {
    return "Egorova Machine IR Function Inlining";
  }

  bool runOnModule(Module &M) override;

private:
  static constexpr unsigned MaxInlineInstrs = 15;
  static constexpr unsigned MaxRecDepth = 3;
  DenseMap<const Function *, unsigned> RecDepth;
  MachineModuleInfo *MMI = nullptr;

  bool isInlineCandidate(MachineFunction &MF);
  bool tryInline(MachineFunction &Caller, MachineBasicBlock &MBB,
                 MachineInstr &MI);
  bool inlineAll(MachineFunction &MF);
};

char EgorovaInlineFunctionPass::ID = 0;

bool EgorovaInlineFunctionPass::isInlineCandidate(MachineFunction &MF) {
  if (MF.size() != 1)
    return false;
  unsigned Cnt = 0;
  for (auto &BB : MF)
    for (auto &MI : BB)
      if (!MI.isDebugInstr())
        ++Cnt;
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
    CalleeMF = MMI->getMachineFunction(*CalleeF);
    if (!CalleeMF)
      return false;
  }

  if (!isInlineCandidate(*CalleeMF))
    return false;

  ++RecDepth[CalleeF];

  MachineRegisterInfo &CallerMRI = Caller.getRegInfo();
  MachineRegisterInfo &CalleeMRI = CalleeMF->getRegInfo();

  MachineBasicBlock &CalleeBB = CalleeMF->front();
  DenseMap<Register, Register> VRegMap;

  SmallVector<MachineInstr *, 16> ToClone;
  for (auto &I : CalleeBB)
    if (!I.isReturn())
      ToClone.push_back(&I);

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
        const TargetRegisterClass *RC = CalleeMRI.getRegClass(R);
        Register NewR = CallerMRI.createVirtualRegister(RC);
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

bool EgorovaInlineFunctionPass::inlineAll(MachineFunction &MF) {
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

bool EgorovaInlineFunctionPass::runOnModule(Module &M) {
  MMI = &getAnalysis<MachineModuleInfoWrapperPass>().getMMI();
  bool Changed = false;
  for (Function &F : M) {
    if (F.isDeclaration())
      continue;
    MachineFunction *MF = MMI->getMachineFunction(F);
    if (!MF)
      continue;
    RecDepth.clear();
    Changed |= inlineAll(*MF);
  }
  return Changed;
}

} // namespace

static RegisterPass<EgorovaInlineFunctionPass>
    X("egorova-x86-inline", "Machine IR Function Inlining", false, false);
