// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CWISSTABLE_BASE_H_
#define CWISSTABLE_BASE_H_

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/// Feature detection and basic helper macros.

/// C++11 compatibility macros.
///
/// Atomic support, due to incompatibilities between C++ and C11 atomic syntax.
/// - `CWISS_ATOMIC_T(Type)` names an atomic version of `Type`. We must use this
///   instead of `_Atomic(Type)` to name an atomic type.
/// - `CWISS_ATOMIC_INC(value)` will atomically increment `value` without
///   performing synchronization. This is used as a weak entropy source
///   elsewhere.
///
/// `extern "C"` support via `CWISS_BEGIN_EXTERN_` and `CWISS_END_EXTERN_`,
/// which open and close an `extern "C"` block in C++ mode.
#ifdef __cplusplus
  #include <atomic>
  #define CWISS_ATOMIC_T(Type_) std::atomic<Type_>
  #define CWISS_ATOMIC_INC(val_) (val_).fetch_add(1, std::memory_order_relaxed)

  #define CWISS_BEGIN_EXTERN_ extern "C" {
  #define CWISS_END_EXTERN_ }
#else
  #include <stdatomic.h>
  #define CWISS_ATOMIC_T(Type_) _Atomic(Type_)
  #define CWISS_ATOMIC_INC(val_) \
    atomic_fetch_add_explicit(&(val_), 1, memory_order_relaxed)

  #define CWISS_BEGIN_EXTERN_
  #define CWISS_END_EXTERN_
#endif

/// Compiler detection macros.
///
/// The following macros are defined:
/// - `CWISS_IS_CLANG` detects Clang.
/// - `CWISS_IS_GCC` detects GCC (and *not* Clang pretending to be GCC).
/// - `CWISS_IS_MSVC` detects MSCV (and *not* clang-cl).
/// - `CWISS_IS_GCCISH` detects GCC and GCC-mode Clang.
/// - `CWISS_IS_MSVCISH` detects MSVC and clang-cl.
#ifdef __clang__
  #define CWISS_IS_CLANG 1
#else
  #define CWISS_IS_CLANG 0
#endif
#if defined(__GNUC__)
  #define CWISS_IS_GCCISH 1
#else
  #define CWISS_IS_GCCISH 0
#endif
#if defined(_MSC_VER)
  #define CWISS_IS_MSVCISH 1
#else
  #define CWISS_IS_MSVCISH 0
#endif
#define CWISS_IS_GCC (CWISS_IS_GCCISH && !CWISS_IS_CLANG)
#define CWISS_IS_MSVC (CWISS_IS_MSVCISH && !CWISS_IS_CLANG)

/// Warning control around `CWISS` symbol definitions. These macros will
/// disable certain false-positive warnings that `CWISS` definitions tend to
/// emit.
#if CWISS_IS_GCCISH
  #define CWISS_BEGIN_             \
    _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wunused-function\"")
  #define CWISS_END_ _Pragma("GCC diagnostic pop")
#else
  #define CWISS_BEGIN_
  #define CWISS_END_
#endif

/// `CWISS_HAVE_SSE2` is nonzero if we have SSE2 support.
///
/// `-DCWISS_HAVE_SSE2` can be used to override it; it is otherwise detected
/// via the usual non-portable feature-detection macros.
#ifndef CWISS_HAVE_SSE2
  #if defined(__SSE2__) || \
      (CWISS_IS_MSVCISH && \
       (defined(_M_X64) || (defined(_M_IX86) && _M_IX86_FP >= 2)))
    #define CWISS_HAVE_SSE2 1
  #else
    #define CWISS_HAVE_SSE2 0
  #endif
#endif

/// `CWISS_HAVE_SSSE2` is nonzero if we have SSSE2 support.
///
/// `-DCWISS_HAVE_SSSE2` can be used to override it; it is otherwise detected
/// via the usual non-portable feature-detection macros.
#ifndef CWISS_HAVE_SSSE3
  #ifdef __SSSE3__
    #define CWISS_HAVE_SSSE3 1
  #else
    #define CWISS_HAVE_SSSE3 0
  #endif
#endif

#if CWISS_HAVE_SSE2
  #include <emmintrin.h>
#endif

#if CWISS_HAVE_SSSE3
  #if !CWISS_HAVE_SSE2
    #error "Bad configuration: SSSE3 implies SSE2!"
  #endif
  #include <tmmintrin.h>
#endif

/// `CWISS_HAVE_BUILTIN` will, in Clang, detect whether a Clang language
/// extension is enabled.
///
/// See https://clang.llvm.org/docs/LanguageExtensions.html.
#ifdef __has_builtin
  #define CWISS_HAVE_CLANG_BUILTIN(x) __has_builtin(x)
#else
  #define CWISS_HAVE_CLANG_BUILTIN(x) 0
#endif

/// `CWISS_HAVE_GCC_ATTRIBUTE` detects if a particular `__attribute__(())` is
/// present.
///
/// GCC: https://gcc.gnu.org/gcc-5/changes.html
/// Clang: https://clang.llvm.org/docs/LanguageExtensions.html
#ifdef __has_attribute
  #define CWISS_HAVE_GCC_ATTRIBUTE(x) __has_attribute(x)
#else
  #define CWISS_HAVE_GCC_ATTRIBUTE(x) 0
#endif

/// `CWISS_THREAD_LOCAL` expands to the appropriate TLS annotation, if one is
/// available.
#if CWISS_IS_GCCISH
  #define CWISS_THREAD_LOCAL __thread
#endif

/// `CWISS_CHECK` will evaluate `cond_` and, if false, print an error and crash
/// the program.
///
/// This is like `assert()` but unconditional.
#define CWISS_CHECK(cond_, ...)                                           \
  do {                                                                    \
    if (cond_) break;                                                     \
    fprintf(stderr, "CWISS_CHECK failed at %s:%d\n", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);                                         \
    fprintf(stderr, "\n");                                                \
    fflush(stderr);                                                       \
    abort();                                                              \
  } while (false)

/// `CWISS_DCHECK` is like `CWISS_CHECK` but is disabled by `NDEBUG`.
#ifdef NDEBUG
  #define CWISS_DCHECK(cond_, ...) ((void)0)
#else
  #define CWISS_DCHECK CWISS_CHECK
#endif

/// `CWISS_LIKELY` and `CWISS_UNLIKELY` provide a prediction hint to the
/// compiler to encourage branches to be scheduled in a particular way.
#if CWISS_HAVE_CLANG_BUILTIN(__builtin_expect) || CWISS_IS_GCC
  #define CWISS_LIKELY(cond_) (__builtin_expect(false || (cond_), true))
  #define CWISS_UNLIKELY(cond_) (__builtin_expect(false || (cond_), false))
#else
  #define CWISS_LIKELY(cond_) (cond_)
  #define CWISS_UNLIKELY(cond_) (cond_)
#endif

/// `CWISS_ALWAYS_INLINE` informs the compiler that it should try really hard
/// to inline a function.
#if CWISS_HAVE_GCC_ATTRIBUTE(always_inline)
  #define CWISS_ALWAYS_INLINE __attribute__((always_inline))
#else
  #define CWISS_ALWAYS_INLINE
#endif

/// `CWISS_NOINLINE` informs the compiler that it should avoid inlining a
/// function.
#if CWISS_HAVE_GCC_ATTRIBUTE(noinline)
  #define CWISS_NOINLINE __attribute__((noinline))
#else
  #define CWISS_NOINLINE
#endif

/// `CWISS_PREFETCH` informs the compiler that it should issue prefetch
/// instructions.
#if CWISS_HAVE_CLANG_BUILTIN(__builtin_prefetch) || CWISS_IS_GCC
  #define CWISS_HAVE_PREFETCH 1
  #define CWISS_PREFETCH(addr_, locality_) \
    __builtin_prefetch(addr_, 0, locality_);
#else
  #define CWISS_HAVE_PREFETCH 0
  #define CWISS_PREFETCH(addr_, locality_) ((void)0)
#endif

#endif  // CWISSTABLE_BASE_H_