#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

namespace {

struct PowiDecomposePass : llvm::PassInfoMixin<PowiDecomposePass> {
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &) {
    bool Changed = false;
    llvm::SmallVector<llvm::CallInst *, 4> CallsToReplace;

    // Ищем вызовы интринсика powi, где степень — это константа от 0 до 4
    for (auto &BB : F) {
      for (auto &I : BB) {
        if (auto *Call = llvm::dyn_cast<llvm::CallInst>(&I)) {
          if (Call->getIntrinsicID() == llvm::Intrinsic::powi) {
            // Второй аргумент (индекс 1) — это степень
            if (auto *ExpInt = llvm::dyn_cast<llvm::ConstantInt>(Call->getArgOperand(1))) {
              int64_t Exp = ExpInt->getSExtValue();
              if (Exp >= 0 && Exp <= 4) {
                CallsToReplace.push_back(Call);
              }
            }
          }
        }
      }
    }

    // Выполняем замену
    for (auto *Call : CallsToReplace) {
      llvm::IRBuilder<> Builder(Call);
      Builder.setFastMathFlags(Call->getFastMathFlags());

      llvm::Value *Base = Call->getArgOperand(0);
      llvm::Type *Ty = Base->getType();
      llvm::Value *Result = nullptr;

      int64_t Exp = llvm::cast<llvm::ConstantInt>(Call->getArgOperand(1))->getSExtValue();

      switch (Exp) {
      case 0:
        // x^0 = 1.0 (ConstantFP::get автоматически поддерживает и скаляры, и векторы)
        Result = llvm::ConstantFP::get(Ty, 1.0);
        break;
      case 1:
        // x^1 = x
        Result = Base;
        break;
      case 2:
        // x^2 = x * x
        Result = Builder.CreateFMul(Base, Base, "powi.2");
        break;
      case 3: {
        // x^3 = (x * x) * x
        llvm::Value *Mul2 = Builder.CreateFMul(Base, Base, "powi.2");
        Result = Builder.CreateFMul(Mul2, Base, "powi.3");
        break;
      }
      case 4: {
        // x^4 = (x * x) * (x * x) — экономим одно умножение!
        llvm::Value *Mul2 = Builder.CreateFMul(Base, Base, "powi.2");
        Result = Builder.CreateFMul(Mul2, Mul2, "powi.4");
        break;
      }
      }

      if (Result) {
        Call->replaceAllUsesWith(Result);
        Call->eraseFromParent();
        Changed = true;
      }
    }

    return Changed ? llvm::PreservedAnalyses::none()
                   : llvm::PreservedAnalyses::all();
  }

  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "PowiDecomposePass", "0.1",
          [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) -> bool {
                  if (name == "powi-decompose") {
                    FPM.addPass(PowiDecomposePass{});
                    return true;
                  }
                  return false;
                });
          }};
}