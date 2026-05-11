#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Tools/Plugins/PassPlugin.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"

using namespace mlir;

namespace {
class CallCountPass
    : public PassWrapper<CallCountPass, OperationPass<ModuleOp>> {
public:
  StringRef getArgument() const final { return "call-count"; }
  StringRef getDescription() const final {
    return "Counts how many times each func.func is called by other functions "
           "and attaches it as an attribute";
  }

  void runOnOperation() override {
    ModuleOp moduleOp = getOperation();

    llvm::DenseMap<StringRef, int64_t> callCounts;

    moduleOp.walk(
        [&](func::FuncOp funcOp) { callCounts[funcOp.getSymName()] = 0; });

    moduleOp.walk([&](func::CallOp callOp) {
      StringRef callee = callOp.getCallee();
      auto it = callCounts.find(callee);
      if (it != callCounts.end()) {
        it->second++;
      }
    });

    moduleOp.walk([&](func::FuncOp funcOp) {
      StringRef name = funcOp.getSymName();
      int64_t count = callCounts.lookup(name);
      funcOp->setAttr(
          "call_count",
          IntegerAttr::get(IntegerType::get(&getContext(), 64), count));
    });
  }
};
} // namespace

MLIR_DECLARE_EXPLICIT_TYPE_ID(CallCountPass)
MLIR_DEFINE_EXPLICIT_TYPE_ID(CallCountPass)

mlir::PassPluginLibraryInfo getCallCountPassPluginInfo() {
  return {MLIR_PLUGIN_API_VERSION, "CallCountPass", "1.0",
          []() { mlir::PassRegistration<CallCountPass>(); }};
}

extern "C" LLVM_ATTRIBUTE_WEAK mlir::PassPluginLibraryInfo
mlirGetPassPluginInfo() {
  return getCallCountPassPluginInfo();
}