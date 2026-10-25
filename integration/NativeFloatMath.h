#pragma once
#include <cmath>
#include <emmintrin.h>
namespace mwel_math {
#ifdef MWEL_NATIVE_MATH_TEST_SWITCH
inline bool legacy=false;
#endif
// Every finite float's square and the sum of two squares fit in double,
// including subnormal input. SSE2 avoids the x86 CRT's repeated x87 control-
// word changes. Keep libm semantics for non-finite and subnormal operands.
inline float Hypot(float x,float y){
#ifdef MWEL_NATIVE_MATH_TEST_SWITCH
 if(legacy)return std::hypot(x,y);
#endif
 if(!std::isfinite(x)||!std::isfinite(y)||
    (x!=0&&std::abs(x)<1.17549435e-38f)||(y!=0&&std::abs(y)<1.17549435e-38f))return std::hypot(x,y);
 const auto a=_mm_cvtss_sd(_mm_setzero_pd(),_mm_set_ss(x));
 const auto b=_mm_cvtss_sd(_mm_setzero_pd(),_mm_set_ss(y));
 const auto dot=_mm_add_sd(_mm_mul_sd(a,a),_mm_mul_sd(b,b));
 const auto norm=_mm_sqrt_sd(_mm_setzero_pd(),dot);
 return _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(),norm));
}
}
