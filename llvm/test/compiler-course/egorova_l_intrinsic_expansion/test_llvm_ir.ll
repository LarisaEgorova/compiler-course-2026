; RUN: opt -load-pass-plugin %llvmshlibdir/egorova_l_intrinsic_expansion_LLVM_IR%pluginext \
; RUN: -passes=powi-decompose -S %s | FileCheck %s

; Объявляем интринсики powi
declare float @llvm.powi.f32.i32(float, i32)
declare double @llvm.powi.f64.i32(double, i32)
declare <4 x float> @llvm.powi.v4f32.i32(<4 x float>, i32)

; --- 1. Степень 0 ---
; CHECK-LABEL: define float @test_powi_0(
define float @test_powi_0(float %a) {
; CHECK-NEXT:    ret float 1.000000e+00
  %res = call float @llvm.powi.f32.i32(float %a, i32 0)
  ret float %res
}

; --- 2. Степень 1 ---
; CHECK-LABEL: define float @test_powi_1(
define float @test_powi_1(float %a) {
; CHECK-NEXT:    ret float %a
  %res = call float @llvm.powi.f32.i32(float %a, i32 1)
  ret float %res
}

; --- 3. Степень 2 ---
; CHECK-LABEL: define float @test_powi_2(
define float @test_powi_2(float %a) {
; CHECK-NEXT:    [[POWI_2:%.*]] = fmul float %a, %a
; CHECK-NEXT:    ret float [[POWI_2]]
  %res = call float @llvm.powi.f32.i32(float %a, i32 2)
  ret float %res
}

; --- 4. Степень 3 (на double) ---
; CHECK-LABEL: define double @test_powi_3(
define double @test_powi_3(double %a) {
; CHECK-NEXT:    [[POWI_2:%.*]] = fmul double %a, %a
; CHECK-NEXT:    [[POWI_3:%.*]] = fmul double [[POWI_2]], %a
; CHECK-NEXT:    ret double [[POWI_3]]
  %res = call double @llvm.powi.f64.i32(double %a, i32 3)
  ret double %res
}

; --- 5. Степень 4 (с флагами fast-math) ---
; CHECK-LABEL: define float @test_powi_4_fast(
define float @test_powi_4_fast(float %a) {
; CHECK-NEXT:    [[POWI_2:%.*]] = fmul fast float %a, %a
; CHECK-NEXT:    [[POWI_4:%.*]] = fmul fast float [[POWI_2]], [[POWI_2]]
; CHECK-NEXT:    ret float [[POWI_4]]
  %res = call fast float @llvm.powi.f32.i32(float %a, i32 4)
  ret float %res
}

; --- 6. Степень 4 (с векторами) ---
; CHECK-LABEL: define <4 x float> @test_powi_4_vector(
define <4 x float> @test_powi_4_vector(<4 x float> %a) {
; CHECK-NEXT:    [[POWI_2:%.*]] = fmul <4 x float> %a, %a
; CHECK-NEXT:    [[POWI_4:%.*]] = fmul <4 x float> [[POWI_2]], [[POWI_2]]
; CHECK-NEXT:    ret <4 x float> [[POWI_4]]
  %res = call <4 x float> @llvm.powi.v4f32.i32(<4 x float> %a, i32 4)
  ret <4 x float> %res
}
