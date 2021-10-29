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
 * @file hitime_extra.c
 * @author Craig Jacobson
 * @brief Extra functions for convenience.
 */

#include "hitime_util.h"
#include "hitime_extra.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>


/*******************************************************************************
 * ALLOC FUNCTIONS
*******************************************************************************/

static void
_hitime_abort(bool is_malloc, size_t size)
{
    const char msg[] = "hitime %salloc(%zu) failure";

    /* The 2 is for the prefix "re".
     * Note that 2**64 is actually 20 max chars, good to have space.
     */
    char buf[sizeof(msg) + 2 + 32];

    snprintf(buf, sizeof(buf), msg, is_malloc ? "m" : "re", size);
    perror(buf);
    abort();
}

INLINE static void *
_hitime_rawalloc_impl(size_t size)
{
    void *mem = malloc(size);
    if (UNLIKELY(!mem))
    {
        _hitime_abort(true, size);
    }
    return mem;
}

/*
 * Don't check for NULL since hitime_free does operations
 * that would cause segfault anyways.
 */
INLINE static void
_hitime_rawfree_impl(void *mem)
{
    free(mem);
}


/*******************************************************************************
 * TIMEOUT FUNCTIONS
*******************************************************************************/

hitimeout_t *
hitimeout_new(void)
{
    hitimeout_t *t = hitime_rawalloc(sizeof(hitimeout_t));
    hitimeout_init(t);
    return t;
}

void
hitimeout_free(hitimeout_t **t)
{
    hitime_rawfree(*t);
    *t = NULL;
}


/*******************************************************************************
 * HIGHTIME FUNCTIONS
*******************************************************************************/

/**
 * @return Heap allocated hitime_t struct pointer.
 */
hitime_t *
hitime_new(void)
{
    hitime_t *h = hitime_rawalloc(sizeof(hitime_t));
    hitime_init(h);
    return h;
}

/**
 * @brief Free allocated hitime_t struct pointer.
 * @param h
 */
void
hitime_free(hitime_t **h)
{
    hitime_destroy(*h);
    hitime_rawfree(*h);
    *h = NULL;
}

/**
 * @return Time in milliseconds.
 */
uint64_t
hitime_now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts))
    {
        return 0;
    }

    return (((uint64_t)ts.tv_sec) * 1000) + ((uint64_t)ts.tv_nsec/1000000);
}

/**
 * @return Time in seconds.
 */
uint64_t
hitime_now(void)
{
    return (uint64_t)time(0);
}

