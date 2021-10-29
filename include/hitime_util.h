/*******************************************************************************
 * Copyright (c) 2021 Craig Jacobson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ******************************************************************************/
/**
 * @file hitime_util.h
 * @author Craig Jacobson
 * @brief Lower level utilities for simple tasks.
 */
#ifndef HITIME_UTIL_H_
#define HITIME_UTIL_H_
#ifdef __cplusplus
extern "C" {
#endif


#include <string.h>


/// @cond DOXYGEN_IGNORE

#define hitime_rawalloc _hitime_rawalloc_impl
#define hitime_rawrealloc _hitime_rawrealloc_impl
#define hitime_rawfree _hitime_rawfree_impl
#define hitime_memzero(p, s) memset((p), 0, (s))

#ifndef LIKELY
#   ifdef __GNUC__
#       define LIKELY(x)   __builtin_expect(!!(x), 1)
#   else
#       define LIKELY(x) (x)
#   endif
#endif

#ifndef UNLIKELY
#   ifdef __GNUC__
#       define UNLIKELY(x) __builtin_expect(!!(x), 0)
#   else
#       define UNLIKELY(x) (x)
#   endif
#endif

#ifndef INLINE
#   ifdef __GNUC__
#       define INLINE __attribute__((always_inline)) inline
#   else
#       define INLINE
#   endif
#endif

#ifndef NOINLINE
#   ifdef __GNUC__
#       define NOINLINE __attribute__((noinline))
#   else
#       define NOINLINE
#   endif
#endif

#ifndef UNUSED
#   define UNUSED
#endif

/// @endcond

#ifdef __cplusplus
}
#endif
#endif /* HITIME_UTIL_H_ */


