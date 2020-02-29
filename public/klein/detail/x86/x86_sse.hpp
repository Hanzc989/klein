// File: x86_sse.hpp
// Purpose: Provide convenience macros and utilities for invoking x86 SSE
// intrinsics

// SSE4.2 header
#include <smmintrin.h>

// Little-endian XMM register swizzle
//
// KLN_SWIZZLE(reg, 3, 2, 1, 0) is the identity.
//
// This is undef-ed at the bottom of klein.hpp so as not to
// pollute the macro namespace
#ifndef KLN_SWIZZLE
#    define KLN_SWIZZLE(reg, x, y, z, w) \
        _mm_shuffle_ps((reg), (reg), _MM_SHUFFLE(x, y, z, w))
#endif

#ifndef KLN_VEC_CALL
#    ifdef _MSC_VER
// Microsoft Compiler
#        define KLN_VEC_CALL __vectorcall
#    else
// GCC or Clang compiler (sse register passing is on by default)
#        define KLN_VEC_CALL
#    endif
#endif

#ifndef KLN_INLINE
#    ifdef _MSC_VER
#        define KLN_INLINE __forceinline
#    else
#        define KLN_INLINE inline __attribute__((always_inline))
#    endif
#endif