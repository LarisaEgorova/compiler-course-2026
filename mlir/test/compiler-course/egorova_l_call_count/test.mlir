// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/call-count%shlibext --pass-pipeline="builtin.module(call-count)" %s | FileCheck %s

// CHECK-LABEL: func.func @foo
// CHECK-SAME: call_count = 0
// CHECK-LABEL: func.func @bar
// CHECK-SAME: call_count = 2
// CHECK-LABEL: func.func @baz
// CHECK-SAME: call_count = 1

module {
  func.func @foo() {
    func.call @bar() : () -> ()
    func.call @bar() : () -> ()
    func.call @baz() : () -> ()
    func.return
  }

  func.func @bar() {
    func.return
  }

  func.func @baz() {
    func.return
  }
}
