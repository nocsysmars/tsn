#ifndef COMPILER_H
#define COMPILER_H 1

#include <stddef.h>
#include <stdbool.h>

#ifndef __has_feature
#define __has_feature(x) 0
#endif
#ifndef __has_extension
#define __has_extension(x) 0
#endif

#if __GNUC__ && !__CHECKER__
#define UNUSED __attribute__((__unused__))
#define PRINTF_FORMAT(FMT, ARG1) __attribute__((__format__(printf, FMT, ARG1)))
#define SCANF_FORMAT(FMT, ARG1) __attribute__((__format__(scanf, FMT, ARG1)))
#define WARN_UNUSED_RESULT __attribute__((__warn_unused_result__))
#define LIKELY(CONDITION) __builtin_expect(!!(CONDITION), 1)
#define UNLIKELY(CONDITION) __builtin_expect(!!(CONDITION), 0)
#else
#define UNUSED
#define PRINTF_FORMAT(FMT, ARG1)
#define SCANF_FORMAT(FMT, ARG1)
#define WARN_UNUSED_RESULT
#define LIKELY(CONDITION) (!!(CONDITION))
#define UNLIKELY(CONDITION) (!!(CONDITION))
#endif

/* Build assertions.
 *
 * Use BUILD_ASSERT_DECL as a declaration or a statement, or BUILD_ASSERT as
 * part of an expression. */
#ifdef __CHECKER__
#define BUILD_ASSERT(EXPR) ((void) 0)
#define BUILD_ASSERT_DECL(EXPR) extern int (*build_assert(void))[1]
#elif defined(__cplusplus) && __cplusplus >= 201103L
#define BUILD_ASSERT(EXPR) static_assert(EXPR, "assertion failed")
#define BUILD_ASSERT_DECL(EXPR) static_assert(EXPR, "assertion failed")
#elif defined(__cplusplus) && __cplusplus < 201103L
#include <boost/static_assert.hpp>
#define BUILD_ASSERT BOOST_STATIC_ASSERT
#define BUILD_ASSERT_DECL BOOST_STATIC_ASSERT
#elif (__GNUC__ * 256 + __GNUC_MINOR__ >= 0x403 \
       || __has_extension(c_static_assert))
#define BUILD_ASSERT_DECL(EXPR) _Static_assert(EXPR, #EXPR)
#define BUILD_ASSERT(EXPR) (void) ({ _Static_assert(EXPR, #EXPR); })
#else
#define BUILD_ASSERT__(EXPR) \
        sizeof(struct { unsigned int build_assert_failed : (EXPR) ? 1 : -1; })
#define BUILD_ASSERT(EXPR) (void) BUILD_ASSERT__(EXPR)
#define BUILD_ASSERT_DECL(EXPR) \
        extern int (*build_assert(void))[BUILD_ASSERT__(EXPR)]
#endif

#ifdef __GNUC__
#define BUILD_ASSERT_GCCONLY(EXPR) BUILD_ASSERT(EXPR)
#define BUILD_ASSERT_DECL_GCCONLY(EXPR) BUILD_ASSERT_DECL(EXPR)
#else
#define BUILD_ASSERT_GCCONLY(EXPR) ((void) 0)
#define BUILD_ASSERT_DECL_GCCONLY(EXPR) ((void) 0)
#endif

#endif
